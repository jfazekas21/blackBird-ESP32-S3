/*
 * SYSTEM_Actor.c
 *
 *  Created on: 18-Mar-2024
 *      Author: Priyanka Patil
 */

#include "SYSTEM_Actor.h"
#include "ETH_Actor.h"
#include "BLE_Actor.h"
#include "HTTP_Actor.h"
#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "math.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <stdio.h>
#include "driver/uart.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "esp_http_client.h"
#include <time.h>
#include "esp_system.h"
#include "soc/rtc_cntl_reg.h"
#include <math.h>

//----------------------------- Actor Tags ---------------------------------------//
static const char * THIS_ACTOR = "SYSTEM";
static const char 			THIS_ACTOR_ID 	= 	SYSTEM;  // assign src id

//------------------------- SYSTEM Actor Resources -----------------------------//
static uint8_t credential_status_u8 = 0;
#define OBJ_QUE_COUNT                	100	//2
static int NET_status = -1;
static char ping_dest_id = 0;
char BLE_CS_Prev_state = 255, BLE_CM_Prev_state = 255;
static uint8_t DB_Sync_Status_Post =0;
static char Server_Conn_Flag = Enable;
static uint64_t Credential_status_time_u64 = 0;
static uint64_t DeviceAnnoune_status_time_u64 = 0;
static char deviceAnnounceStatus[50] = {0};

static char localtime_str[50] = {0};

static void httpDeviceAnnounceInit(AMessage_st* s_Message_Rx);
static void SendAcktoServer(char* SessionId);   //Send acknowledgment for device announce method
static void Fetch_Record_Log (AMessage_st* s_Message_Rx);
static void Send_RecordLogData_Server(void *pvParameters __attribute__((unused)));
static void Record_sync(AMessage_st* s_Message_Rx);
static void Get_Record_Sync_Status(void *pvParameters __attribute__((unused)));
static void DB_Sync_Check(void *pvParameters __attribute__((unused)));
static void Server_Connect(void *pvParameters __attribute__((unused)));
static void initiate_ServerConnect(AMessage_st* s_Message_Rx);
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
uint8_t create_and_sendJson(char * SSID,char * da_message, char* ping_dns_data, char* ping_gw_data);
static void Reset_Stack(AMessage_st* s_Message_Rx, char *payLoadData_New);
static void InitMonSrvConn(void);
static void MonSrvConn(void *pvParameters __attribute__((unused)));
static void Set_Server_Connection_Flag(AMessage_st* s_Message_Rx);
static void Get_System_states(void *pvParameters __attribute__((unused)));
static void Get_System_timers(void *pvParameters __attribute__((unused)));
static void Read_System_Properties_From_SPIFFS (AMessage_st* s_Message_Rx);
static void Read_SPIFFS(void *pvParameters __attribute__((unused)));
static void Init_Post_File_Ack (AMessage_st* s_Message_Rx);
static void Post_File_Ack_Task(void *pvParameters __attribute__((unused)));
static void sendLedState(const char *stateName, int duration, char *category);
static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason);
static void Reset_Credentials(AMessage_st* s_Message_Rx);
//-------------------------- Common Actor Resources ------------------------------//

BaseType_t ACTOR_SYSTEMMonitor, POST_Method ,Debug_Log, Record_sync_Method,Auto_Exe_cmd, Record_sync_new_Method, Sync_Status_Method,DB_Sync_Check_task;//,ServerConnect_Method;
QueueHandle_t ACTOR_SYSTEM_Rx_Queue,Debug_Que = NULL, Auto_Exe_Que = NULL, Record_Sync_Que = NULL, Record_Sync_new_Que = NULL, Sync_Status_Que = NULL,ServerConnect_Que = NULL,MonSrvConn_Que = NULL, SPIFFS_Que = NULL, STATE_Que = NULL, Post_File_Ack_Que = NULL, TIMER_Que = NULL;
TaskHandle_t ACTOR_SYSTEMHandle ,KeyHandle = NULL, DebugHandle = NULL, Record_Handle = NULL, Record_new_Handle = NULL, Auto_Exe_Handle = NULL, Sync_Status_Handle = NULL, DB_Sync_Check_Handle = NULL,ServerConnect_Handle = NULL,MonSrvConn_Handle = NULL, SPIFFS_Handle = NULL, STATE_Handle = NULL, TIMER_Handle = NULL, Post_File_Ack_Handle = NULL; // HTTP Task Handler
static StaticTask_t xSYSTEMTaskBuffer,xGetRcrdSyncTaskBuffer,xRcrdSyncNewTaskBuffer, xSPIFFSTaskBuffer, xSrvConnTaskBuffer,xMonSrvConnTaskBuffer,xStateTaskBuffer, xTimerTaskBuffer, xPOST_File_ACKTaskBuffer;  //// xAutoExecuteTaskBuffer, Declare a static task control block
PSRAM_ATTR_BSS static StackType_t xTaskStack[SYSTEM_TASK_STACK_DEPTH], xGetRcrdSyncTaskStack[GET_RECORD_SYNCSTATUS_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xRcrdSyncNewTaskStack[SEND_RECORDLOG_SERVER_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xSPIFFSTaskStack [READ_PROPERTIES_SPIFFS_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xServ_conn_TaskStack[SRV_CONN_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xServ_mon_TaskStack [MON_SERVER_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xstate_TaskStack [SYSTEM_STATE_TASK_STACK_DEPTH], xstate_TaskStack_1 [SYSTEM_TIMER_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xPost_File_AckTaskStack [POST_FILE_ACK_TASK_STACK_DEPTH];  //*Auto_Execute_xTaskStack = NULL,
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage[OBJ_QUE_COUNT * sizeof(AMessage_st)], SPIFFS_pucQueueStorage [SPIFFS_QUE_COUNT * SPIFFS_QUE_ITEMSIZE], Serv_conn_pucQueueStorage[5 * 2000], GetRcrdSync_pucQueueStorage[GET_RECORD_SYNCSTATUS_QUEUE_LENGTH * GET_RECORD_SYNCSTATUS_QUEUE_ITEMSIZE];
PSRAM_ATTR_BSS static uint8_t RcrdSyncNewQ_pucQueueStorage[Record_Sync_New_Que_COUNT * Record_Sync_New_Que_COUNT_ITEMSIZE],Serv_mon_pucQueueStorage[5 * 2000],  State_pucQueueStorage[5 * 1000], Timer_pucQueueStorage[5 * 1000], Post_File_Ack_pucQueueStorage[Post_File_Ack_QUE_COUNT * Post_File_Ack_QUEUE_ITEMSIZE];  //*Auto_exe_pucQueueStorage = NULL,
static StaticQueue_t Monitor_pxQueueBuffer, SPIFFS_pxQueueBuffer , Serv_conn_pxQueueBuffer,GetRcrdSync_pxQueueBuffer ,RcrdSyncNewQ_pxQueueBuffer , Serv_mon_pxQueueBuffer,  State_pxQueueBuffer, Timer_pxQueueBuffer, Post_File_Ack_pxQueueBuffer;  //*Auto_exe_pxQueueBuffer = NULL,

//static AMessage_st s_Message_Tx;
static int FirstACTOR_SYSTEMEntry = 0;
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void Get_Property_Parameters(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void epoch_to_date_time_m(date_time_t* date_time,uint64_t epoch);
static void set_gmt_dst(AMessage_st* s_Message_Rx);
PSRAM_ATTR_BSS static uint16_t InternetDisconnectCount;

static void periodic_timer_callback(void* arg);

static const esp_timer_create_args_t periodic_timer_args_new = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"
};
static esp_timer_handle_t periodic_timer = NULL;


static void periodic_timer_callback_5_Minute(void* arg);

static const esp_timer_create_args_t periodic_timer_args_new_5 = {
		.callback = &periodic_timer_callback_5_Minute,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic_5"
};
static esp_timer_handle_t periodic_timer_5 = NULL;

// Define the states
typedef enum {
	STATE_CHECK_CREDENTIALS,
	STATE_CHECK_BLE_START_RESP,
	STATE_CHECK_WIFI_ENABLED,
	STATE_CHECK_WIFI_AVAILABLE,
    STATE_NETWORK_SCAN,
	STATE_WIFI_CONNECT_SSID,
    STATE_ETH_CONNECT,
	STATE_NTP_SYNC_CHECK,
    STATE_DEVICE_ANNOUNCE,
    STATE_PING_DNS,
    STATE_PING_GATEWAY,
	STATE_FAILED_PING_GATEWAY,
	STATE_WAITING_FOR_IHUB_RESPONSE,
	STATE_CONNECTED,
    STATE_FAILED,
	STATE_FAILED_1,
	STATE_FAILED_2,
	STATE_FAILED_3,
	STATE_FAILED_4,
	STATE_FAILED_5,
	STATE_FAILED_6,
	STATE_FAILED_10,
	STATE_FAILED_11,
	STATE_FAILED_12,
	STATE_FAILED_13
}State;

#define SER_CONN_BLE_MAX_DELAY 300000 // 5 min
#define SER_CONN_NORMAL_DELAY 3000 // 1 min
#define MON_CONN_MAX_DELAY 60000 // 1 min
#define WAIT_FOR_30_SECONDS 30000
#define WAIT_FOR_10_SECONDS 10000
#define WAIT_FOR_1_MINUTE 60000
#define WAIT_FOR_10_MINUTE 60000
#define WAIT_FOR_3_SECONDS 3000
#define WAIT_FOR_1_SECOND 1000

// Define constants for better readability and maintainability
#define BUFFER_SIZE 4096
#define SSID_SIZE 50
#define MESSAGE_SIZE 100
#define WAIT_ONE_MINUTE 60000 / portTICK_PERIOD_MS
#define WAIT_THIRTY_SECONDS 30000 / portTICK_PERIOD_MS
#define WAIT_TEN_MINUTES 600000 / portTICK_PERIOD_MS
uint8_t BLE_INIT_Flag = 0;
uint8_t BLE_INIT_Flag_2 = 0;
uint8_t BLE_INIT_Flag_3 = 0;

uint8_t Device_Announce_F_delay_Flag_1 = 0;

#define ETHERNET_TIMEOUT_SECONDS 3
#define WIFI_CONNECT_TIMEOUT_SECONDS 60
#define DEVICE_ANNOUNCE_TIMEOUT_SECONDS 51
#define PING_DNS_TIMEOUT_SECONDS 40
#define PING_GATEWAY_TIMEOUT_SECONDS 40
#define SAVE_SRV_CONN_TIMEOUT_SECONDS 3
#define SAVE_SRV_CONN_TIMEOUT_SECONDS 3
#define WIFI_ENABLED_TIMEOUT_SECONDS 3
#define WIFI_ETH_ENABLED_TIMEOUT_SECONDS 3
#define WIFI_APINFO_TIMEOUT_SECONDS 3
#define RESCUE_SSID_TIMEOUT_SECONDS 3
#define NETWORK_SCAN_TIMEOUT_SECONDS 3
#define IHUB_RESP_TIMEOUT_SECONDS 60
#define STATE_RESP_TIMEOUT_SECONDS 30

static time_t network_scan_timeout_start = 0;
static time_t rescue_ssid_timeout_start = 0;
static time_t wifi_apinfo_timeout_start = 0;
static time_t wifi_eth_enabled_timeout_start = 0;
static time_t wifi_enabled_timeout_start = 0;
static time_t device_announce_timeout_start = 0;
static time_t ping_dns_timeout_start = 0;
static time_t ping_gateway_timeout_start = 0;
static time_t eth_timeout_start = 0;
static time_t wifi_connect_timeout_start = 0;
static time_t ihub_connect_timeout_start = 0;
static time_t timeout_start = 0;
static time_t state_timeout_start = 0;

// Function prototypes
static void handle_check_credentials(State *state, uint8_t *timeout_count_credential_u8, uint8_t *BLE_INIT_Flag, uint32_t *que_rx_delay, AMessage_st *s_Message_Rx,cJSON *In_JSON, uint8_t *blink_state_val);
static void handle_ble_start_resp(State *state, cJSON *In_JSON, uint8_t *timeout_count_credential_u8, uint8_t *timeout_count_credential_2_u8, uint8_t *timeout_count_credential_3_u8, AMessage_st *s_Message_Rx);
static void handle_wifi_enabled(State *state, cJSON *In_JSON, uint8_t *retryCount2, char *SSID, char *Pass, uint8_t *wifi_enabled, uint8_t *sent_flag, AMessage_st *s_Message_Rx);
static void handle_network_scan(State *state, uint8_t *timeout_count_credential_2_u8, uint8_t *BLE_INIT_Flag_2, uint32_t *que_rx_delay, cJSON *In_JSON, char *SSID, char *Pass, char *eth_keyVal, uint8_t *retryCount, AMessage_st *s_Message_Rx, uint8_t *blink_state_val);
static void handle_wifi_connect_ssid(State *state, uint8_t *timeout_count_credential_3_u8, uint8_t *BLE_INIT_Flag_3, cJSON *In_JSON, char *SSID, char *ConnectStatus, uint8_t *retryCount,uint32_t *que_rx_delay, AMessage_st *s_Message_Rx ,uint8_t *blink_state_val);
static void handle_eth_connect(State *state, cJSON *In_JSON, char *SSID, char *ping_dest_actor, char ping_dest_id, uint8_t *wifi_enabled,uint32_t *que_rx_delay, AMessage_st *s_Message_Rx, uint8_t *blink_state_val);
static void handle_device_announce(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dest_actor, char ping_dest_id, uint8_t *blink_state_val, AMessage_st *s_Message_Rx);
static void handle_ping_dns(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, char ping_dest_id, AMessage_st *s_Message_Rx, uint8_t *blink_state_val);
static void handle_ping_gateway(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, AMessage_st *s_Message_Rx,uint8_t *blink_state_val);
static void handle_waiting_for_ihub_response(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, uint8_t *blink_state_val, AMessage_st *s_Message_Rx);
static void handle_failed_state(State *state, AMessage_st* s_Message_Rx);
static void Mon_reset_stack(char *buffer, AMessage_st *s_Message_Rx, char *message);
static void handle_idle_state(MonSrvConnState *CurrentMonSrvState, AMessage_st *s_Message_Rx);
static void handle_wifi_eth_enabled_status(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, uint8_t *wifi_enabled, uint8_t *l_wifi_status,uint8_t *l_eth_status, AMessage_st *s_Message_Rx, uint8_t *blink_state_val);
static void handle_ihub_status(char* buffer, MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, uint8_t *wifi_enabled, AMessage_st *s_Message_Rx, uint8_t *blink_state_val);
static void handle_wifi_apinfo(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, AMessage_st *s_Message_Rx, uint8_t *wifi_enabled);
static void handle_rescue_ssid(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID,AMessage_st *s_Message_Rx,uint8_t *l_eth_status,uint8_t *wifi_enabled);
static void mon_handle_network_scan(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, AMessage_st *s_Message_Rx,uint8_t *l_wifi_status,uint8_t *l_eth_status,uint8_t *wifi_enabled);
//-------------------------- Actor Parameters ------------------------------//

PSRAM_ATTR_BSS static struct ACTOR_SYSTEM_parameter
{
	uint8_t 	Manufacturer_date_a8 [BLE_INFO_LEN ];
	uint8_t   	Manufacturer_name_a8 [BLE_INFO_LEN ];
	uint8_t   	Model_name_a8 [BLE_INFO_LEN ];
	uint8_t     FirmwareVer[BLE_BTNAME_LEN ];
	uint8_t     BootFirmwareVer[BLE_BTNAME_LEN ];
	uint8_t     HardwareVer[BLE_BTNAME_LEN ];
	uint8_t 	autoEXEC;
	uint8_t 	DeviceId[32];
	uint8_t 	DeviceAnnounce_URL[256];
	uint8_t 	DeviceAnnounce_ACK_URL[256];		//
	uint8_t 	debugLog_URL[256];
	uint8_t  	HTTP_endpoint_u8[150];				//
	uint8_t 	APIkey[100];						//
	uint32_t 	deviceAnnouncePeriod;				//		//HTTP_retry_interval_u32;
    uint8_t     Debug_Log_Flag_u8;
    uint16_t    DB_sync_status_scan_delay_u16;
    uint8_t 	DB_Sync_Status_URL_u8[256];
    uint8_t 	File_Fetch_Ack_URL[256];
    uint32_t    Device_Announce_F_delay_u32;
    char 		automation_url[256];
} ACTOR_SYSTEM_Para;

PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Record[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Re_Sync_St[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_state[4 * 1024];
PSRAM_ATTR_BSS static char payLoadData_timer[1 * 1024];
PSRAM_ATTR_BSS static char payLoadData_File_Ack[2 * 1024];
PSRAM_ATTR_BSS static char payLoadData_Server[MAX_JSON_PAYLOAD_BYTES/2];  //4 * 1024
PSRAM_ATTR_BSS static char payLoadData_Monitor[2 * 1024]; 
PSRAM_ATTR_BSS static char da_message[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char bufferRecordLog[2000];
PSRAM_ATTR_BSS static char bufferRecord_Sync[2000];
PSRAM_ATTR_BSS static char line[2048];
PSRAM_ATTR_BSS static char data_bufferSystem_states[1024];
PSRAM_ATTR_BSS static char bufferSystem_states[1024];
PSRAM_ATTR_BSS static char data_bufferSystem_timers[1024];
PSRAM_ATTR_BSS static char bufferSystem_timers[1024];
PSRAM_ATTR_BSS static char serverconnectbuffer[BUFFER_SIZE];
PSRAM_ATTR_BSS static char serverconnect_data_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char Monitor_data_buffer[4096];
PSRAM_ATTR_BSS static char Monitor_buffer [BUFFER_SIZE];
PSRAM_ATTR_BSS static char Read_SPIFFS_buffer[SPIFFS_QUE_ITEMSIZE];
PSRAM_ATTR_BSS static  char DB_Sync_buffer[MAX_JSON_PAYLOAD_BYTES/2];
static int16_t	gmt_val;
static uint8_t	dst_val;

static int16_t	device_ann_delay_count;
static int64_t	remaining_time_device_ann;

static const unsigned short days[4][12] =
{
   {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
   { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
   { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
   {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};


PSRAM_ATTR static struct property prop[] = // Actor Property
{
	{ &ACTOR_SYSTEM_Para.Manufacturer_date_a8,     "MANUFACTURER_DATE",          STRING, "R", "Manufacturer date of the device" },
    { &ACTOR_SYSTEM_Para.Manufacturer_name_a8,     "MANUFACTURER_NAME",          STRING, "R", "Manufacturer name of the device" },
    { &ACTOR_SYSTEM_Para.Model_name_a8,            "MODEL_NAME",                 STRING, "R", "Model name of the device" },
    { &ACTOR_SYSTEM_Para.FirmwareVer,              "FIRMWARE",                   STRING, "R", "Firmware version of the device" },
	{ &ACTOR_SYSTEM_Para.BootFirmwareVer,          "BOOTLOADER_VERSION",         STRING, "R", "Secondary Bootloader Firmware version of the device" },
	{ &ACTOR_SYSTEM_Para.HardwareVer,              "HARDWARE",                   STRING, "R", "Hardware version of the device" },
    { &ACTOR_SYSTEM_Para.autoEXEC,                 "AUTO_EXEC",                  U_INT8, "RW", "Auto execution flag" },
    { &ACTOR_SYSTEM_Para.DeviceId,                 "DEVICEID",                   STRING, "R", "Device ID" },
    { &ACTOR_SYSTEM_Para.DeviceAnnounce_URL,       "DEVICE_ANNOUNCE_URL",        STRING, "RW", "Device announcement URL" },
    { &ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL,   "DEVICE_ANNOUNCE_ACK_URL",    STRING, "RW", "Device announcement ACK URL" },
    { &ACTOR_SYSTEM_Para.debugLog_URL,             "DEBUGLOG_URL",               STRING, "RW", "Debug log URL" },
    { &ACTOR_SYSTEM_Para.Debug_Log_Flag_u8,        "POST_DEBUGLOG",              U_INT8, "RW", "Flag to enable post debug log" },
    { &ACTOR_SYSTEM_Para.HTTP_endpoint_u8,         "RECORDSYNC_URL",             STRING, "RW", "HTTP endpoint for record synchronization" },
    { &ACTOR_SYSTEM_Para.APIkey,                   "API_KEY",                    STRING, "RW", "API key for device authentication" },
    { &ACTOR_SYSTEM_Para.deviceAnnouncePeriod,     "DEVICE_ANNOUNCE_PERIOD",     U_INT32, "RW", "Device announcement period" },
	{ &ACTOR_SYSTEM_Para.DB_sync_status_scan_delay_u16,     "DB_SCAN_DELAY",     U_INT16, "RW", "Scan delay of posting database sync status" },
    { &ACTOR_SYSTEM_Para.DB_Sync_Status_URL_u8,     "RECORD_SYNC_STATUS_URL",     STRING, "RW", "Record sync status URL" },
	{ &ACTOR_SYSTEM_Para.File_Fetch_Ack_URL,    	 "FILE_FETCH_ACK",     		  STRING, "RW", "File fetch ACK URL" },
	{ &ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32, "DEVICE_ANNOUNCE_F_DELAY",  U_INT32, "RW", "Scan delay in seconds of posting database sync status" },
	{ &ACTOR_SYSTEM_Para.automation_url,              "AUTOMATION_URL",           STRING,  "RW", "Server Automation URL, e.g. https://api.example.com" }
};

//-------------------------- Common Actor Methods ------------------------------//

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(property, prop[i].str_name)) {
			if (!strcmp(prop[i].access, "RW")) {

			parameter_found = 1; // Set flag to indicate actor is found
			switch (prop[i].type) {

			case U_INT8:
				*(uint8_t*) prop[i].name = atoi(value);
				break;

			case U_INT16:
				*(uint16_t*) prop[i].name = atoi(value);
				break;

			case U_INT32:
				*(uint32_t*) prop[i].name = atoi(value);
				break;

			case INT:
				*(int*) prop[i].name = atoi(value);
				break;
			case FLOAT:
				*(float*) prop[i].name = atof(value);
				break;

			case STRING:
				strcpy((char*) prop[i].name, value);

				break;

			default:
				break;
			}
			if((!(strcmp(prop[i].str_name, "API_KEY"))) || (!(strcmp(prop[i].str_name, "DEVICE_ANNOUNCE_URL"))))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				Credential_status_time_u64  = current_epos_sec;
			}
		}
		else
		{
			return 2;
		}
	  }
	}
	if(parameter_found)
		return 1;
	else
		return 0;
}//	set

static void get(char *str_prop, char *val_a8)
{
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(prop[0]);
	for (int i = 0; i < no_of_elements; i++)
	{
		if (!strcmp(str_prop, prop[i].str_name))
		{
			switch (prop[i].type)
			{
				case U_INT8:
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case INT:
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case FLOAT:
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(val_a8, prop[i].name);
					break;

				default:
					break;
			}
		}
	}
}//	get


static void init(void *a, void *b)
{
	uint8_t mac[6];
	if (FirstACTOR_SYSTEMEntry == 0)
	{
		ACTOR_SYSTEM_Rx_Queue = xQueueCreateStatic(OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
		ACTOR_SYSTEMHandle = xTaskCreateStaticPinnedToCore(
						monitor,                 // Task function
						"System Monitor",            // Task name
						SYSTEM_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						SYSTEM_TASK_PRIORITY,                       // Task priority
						xTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xSYSTEMTaskBuffer,             // Pointer to task control block
						0
	   );

	if (ACTOR_SYSTEMHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
			// Handle error
		}

	if(periodic_timer == NULL)
	{
		esp_err_t err = esp_timer_create(&periodic_timer_args_new, &periodic_timer);
		if(err != ESP_OK)
		{
			printf("\n\n Error! failed to create period timer in System actor err- %d\n\n", err);
			goto Exit12;
		}
		err = esp_timer_start_periodic(periodic_timer, (10800000000)); // start the timer with the period of 3x60x60x1000000 sec

		if(err != ESP_OK)
			printf("\n\n Error in start periodic timer. err = %d\n\n", err);
	}
Exit12:

	if(periodic_timer_5 == NULL)
	{
		esp_err_t err1 = esp_timer_create(&periodic_timer_args_new_5, &periodic_timer_5);
		if(err1 != ESP_OK)
		{
			printf("\n\n Error! failed to create period timer 5 in System actor err- %d\n\n", err1);
			goto Exit125;
		}
		err1 = esp_timer_start_periodic(periodic_timer_5, (300000000)); // start the timer with the period of 5x60x1000000 sec

		if(err1 != ESP_OK)
			printf("\n\n Error in start periodic timer 5. err = %d\n\n", err1);
	}
Exit125:


	    memset((char*)&ACTOR_SYSTEM_Para.Manufacturer_name_a8, 0, sizeof(ACTOR_SYSTEM_Para.Manufacturer_name_a8));
	    memset((char*)&ACTOR_SYSTEM_Para.Manufacturer_date_a8, 0, sizeof(ACTOR_SYSTEM_Para.Manufacturer_date_a8));
	    memset((char*)&ACTOR_SYSTEM_Para.Model_name_a8, 0, sizeof(ACTOR_SYSTEM_Para.Model_name_a8));
	    memset((char*)&ACTOR_SYSTEM_Para.FirmwareVer, 0, sizeof(ACTOR_SYSTEM_Para.FirmwareVer));
	    memset((char*)&ACTOR_SYSTEM_Para.BootFirmwareVer, 0, sizeof(ACTOR_SYSTEM_Para.BootFirmwareVer));
	    memset((char*)&ACTOR_SYSTEM_Para.HardwareVer, 0, sizeof(ACTOR_SYSTEM_Para.HardwareVer));
	    memset((char*)&ACTOR_SYSTEM_Para.APIkey, 0, sizeof(ACTOR_SYSTEM_Para.APIkey));
	    memset((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL, 0, sizeof(ACTOR_SYSTEM_Para.DeviceAnnounce_URL));
	    memset((char*)&ACTOR_SYSTEM_Para.File_Fetch_Ack_URL, 0, sizeof(ACTOR_SYSTEM_Para.File_Fetch_Ack_URL));
	    ACTOR_SYSTEM_Para.autoEXEC = 1;
        ACTOR_SYSTEM_Para.Debug_Log_Flag_u8 = 1;
        ACTOR_SYSTEM_Para.DB_sync_status_scan_delay_u16 = 60;
        ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32 = 0;		//0 Second
        device_ann_delay_count = 0;
        remaining_time_device_ann = 0;

        snprintf(ACTOR_SYSTEM_Para.automation_url, sizeof(ACTOR_SYSTEM_Para.automation_url),
        		"%s", "https://hls-dev-ca-d0f63af2.gentleground-3662f123.eastus.azurecontainerapps.io/api/Device/triggerAutomationLink");

        memset((char*)&ACTOR_SYSTEM_Para.debugLog_URL, 0, sizeof(ACTOR_SYSTEM_Para.debugLog_URL));
        memset((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL, 0, sizeof(ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL));
        memset((char*)&ACTOR_SYSTEM_Para.HTTP_endpoint_u8, 0, sizeof(ACTOR_SYSTEM_Para.HTTP_endpoint_u8));
        memset((char*)&ACTOR_SYSTEM_Para.DB_Sync_Status_URL_u8, 0, sizeof(ACTOR_SYSTEM_Para.DB_Sync_Status_URL_u8));
		strcpy(deviceAnnounceStatus,"Connection Yet To Start");
		struct timeval currentTime;
		_gettimeofday_r(NULL, &currentTime, NULL);
		uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		DeviceAnnoune_status_time_u64  = current_epos_sec;
		esp_read_mac(mac, ESP_MAC_WIFI_STA);
	    sprintf((char*)&ACTOR_SYSTEM_Para.DeviceId, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		ACTOR_SYSTEM_Para.deviceAnnouncePeriod = 1000;  //600

		Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", "\0", 0, "GET_RTC_TIME");

		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/SYSTEM.json");
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);	
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		cJSON_Delete(responseObject);
		FirstACTOR_SYSTEMEntry = 1;
	}
}

static void monitor(void *pvParameters __attribute__((unused)))
{
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;
    char stack_reset_buf[100] = {0};
	while (1)
	{
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(ACTOR_SYSTEM_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
//			printf("SYSTEM msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("SYSTEM DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;

			/*
			 * Match and acall functions
			 */
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties
				if(FirstACTOR_SYSTEMEntry==1)
				{
					Add_Response_msg("SYSTEM Actor initialization is done.", s_Message_Rx, payLoadData);
					if((strlen((char*)&ACTOR_SYSTEM_Para.FirmwareVer) == 0) || (strlen((char*)&ACTOR_SYSTEM_Para.HardwareVer) == 0) || (strlen((char*)&ACTOR_SYSTEM_Para.DeviceId) == 0) || (strlen((char*)&ACTOR_SYSTEM_Para.Manufacturer_name_a8) == 0) || (strlen((char*)&ACTOR_SYSTEM_Para.Manufacturer_date_a8) == 0) || (strlen((char*)&ACTOR_SYSTEM_Para.Model_name_a8) == 0))
					{
						Read_System_Properties_From_SPIFFS(s_Message_Rx);   // Read parameters from SPIFFS
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				if (!strcmp((char*)s_Message_Rx->src_Actor_a8, "BLE"))
				{
					Get_Property_Parameters(s_Message_Rx);
				}
				else
				{
					Get_Property(s_Message_Rx);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				u8Result =0;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL)
				{
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				}
				else{
				cJSON *root_JSON = NULL;
				head_JSON = name_JSON->child;
			   // Loop through each key-value pair
				do {
					if ((strcmp(head_JSON->string, "DEVICE_ANNOUNCE_URL") == 0) || (strcmp(head_JSON->string, "API_KEY") == 0) )
					{
						if(root_JSON == NULL)
						{
							root_JSON  = cJSON_CreateObject();
							if(root_JSON != NULL)
							{
								cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/SYSTEM.json");
							}
						}
						cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
					}

					// Check if the value string is not NULL
					if (cJSON_IsString(head_JSON))  //&& (strlen(head_JSON->valuestring)!= 0)
					{
						// Set the key-value pair
						u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
						if(u8Result==2){
						sprintf(str,"'%s' is a read only property", head_JSON->string);
						Add_Response_msg(str, s_Message_Rx, payLoadData);
						}
						else if(u8Result==0){
							sprintf(str,"'%s' is a invalid Key", head_JSON->string);
							Add_Response_msg(str,s_Message_Rx, payLoadData);
						}
					} else {
						// Handle the case where value string is NULL (e.g., log an error or take appropriate action)
						sprintf(str, "Invalid parameter '%s'", head_JSON->string);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
						// Handle the error as per your application's requirements
					}
					head_JSON = head_JSON->next;
				} while (head_JSON != 0);

				if((u8Result==1) && (root_JSON != NULL))
				{
					//  save parameters to JFS
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
					Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
					cJSON_Delete(root_JSON);
					console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
				// Free the parsed JSON
				cJSON_Delete(name_JSON);
			 }
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DEVICE_ANN"))
			{
				httpDeviceAnnounceInit(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RECORD_SYNC"))
			{
				DB_Sync_Status_Post = 1;
				if(DB_Sync_Check_Handle == NULL)
				{
					DB_Sync_Check_task = xTaskCreatePinnedToCore(DB_Sync_Check, (const char*) "DB_Sync Status Check", DB_SYNC_CHECK_TASK_STACK_DEPTH, s_Message_Rx, DB_SYNC_CHECK_TASK_PRIORITY, &DB_Sync_Check_Handle, 1);
				}
				if(Record_Handle == NULL)
				Record_sync(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
//			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "POST_DEBUG_LOG"))
//			{
//				if(Debug_Que == NULL)
//						Debug_Que = xQueueCreate(POST_DEBUG_LOG_SERVER_QUEUE_LENGTH, POST_DEBUG_LOG_SERVER_QUEUE_ITEMSIZE);
//
//				if(DebugHandle == NULL)
//				{
//					Debug_Log = xTaskCreatePinnedToCore(Post_Debug_Log, (const char*) "Post_Debug_Log", POST_DEBUG_LOG_SERVER_STACK_DEPTH, NULL, POST_DEBUG_LOG_SERVER_PRIORITY, &DebugHandle, 0);
////					if(Debug_Que == NULL)
////						Debug_Que = xQueueCreate(3, 2500);
////					if (Debug_Que == NULL)
////						printf("ERROR(Debug_Que is not created)\n");
//
//				}
//				else
//				{
//					vTaskDelete(DebugHandle);
//					DebugHandle = NULL;
//					// Inside your cleanup or exit function
//					vQueueDelete(Debug_Que);
//					Debug_Log = xTaskCreatePinnedToCore(Post_Debug_Log, (const char*) "Post_Debug_Log", POST_DEBUG_LOG_SERVER_STACK_DEPTH, NULL, POST_DEBUG_LOG_SERVER_PRIORITY, &DebugHandle, 0);
//				}
//			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			{
				get_actor_properties(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "FETCH_RECORD"))
			{
				if(Record_new_Handle == NULL)
				{
					Fetch_Record_Log(s_Message_Rx);
				}
				else
					Add_Response_msg("FETCH_RECORD task is already created", s_Message_Rx, payLoadData);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DBSYNC_STATUS"))
			{
				if(Sync_Status_Que == NULL)
					Sync_Status_Que = xQueueCreateStatic(GET_RECORD_SYNCSTATUS_QUEUE_LENGTH, GET_RECORD_SYNCSTATUS_QUEUE_ITEMSIZE, GetRcrdSync_pucQueueStorage, &GetRcrdSync_pxQueueBuffer);
				if (Sync_Status_Que == NULL)
					 Add_Response_msg("Error! Sync_Status_Que is not created", s_Message_Rx, payLoadData);

				if(Sync_Status_Handle == NULL)
				{
					AMessage_st s_Message_Rx_data;
					memset(DB_Sync_buffer,0,sizeof(DB_Sync_buffer));
					memcpy(&s_Message_Rx_data, s_Message_Rx, sizeof(AMessage_st));
					if((char*)s_Message_Rx_data.payload_p8 != NULL)
					{
						strncpy(DB_Sync_buffer, (char*)s_Message_Rx_data.payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
					}
					s_Message_Rx_data.payload_p8 = (uint8_t*)DB_Sync_buffer;

//					Sync_Status_Method = xTaskCreate(Get_Record_Sync_Status, (const char*) "RECORDS SYNC STATUS", GET_RECORD_SYNCSTATUS_STACK_DEPTH, s_Message_Rx, GET_RECORD_SYNCSTATUS_PRIORITY, &Sync_Status_Handle);
//					size_t stack_size = GET_RECORD_SYNCSTATUS_STACK_DEPTH * sizeof(StackType_t);
//					if (xGetRcrdSyncTaskStack!=NULL) {
//						free(xGetRcrdSyncTaskStack);
//						xGetRcrdSyncTaskStack = NULL;  // Set pointer to NULL after freeing
//					}
//					 xGetRcrdSyncTaskStack = (StackType_t *)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//					if (xGetRcrdSyncTaskStack == NULL) {
//						// Memory allocation failed
//	#ifdef ENABLE_PRINT_MSG
//						printf( "Failed to allocate memory for the task stack");
//	#endif
//						Add_Response_msg("Error! Failed to allocate memory for the task stack.",s_Message_Rx);
//						// Handle error
//						continue;
//					}
//					xGetRcrdSyncTaskStack = (StackType_t *)memory_xGetRcrdSyncTaskStack;
//					memset(memory_xGetRcrdSyncTaskStack, 0, sizeof(memory_xGetRcrdSyncTaskStack));

					Sync_Status_Handle = xTaskCreateStatic(
									Get_Record_Sync_Status,                 // Task function
									"RECORDS SYNC STATUS",            // Task name
									GET_RECORD_SYNCSTATUS_STACK_DEPTH,        // Stack size in words
									&s_Message_Rx_data,                    // Task parameters (not used here)
									GET_RECORD_SYNCSTATUS_PRIORITY,                       // Task priority
									xGetRcrdSyncTaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xGetRcrdSyncTaskBuffer             // Pointer to task control block
				   );

			if (Sync_Status_Handle == NULL) {
	#ifdef ENABLE_PRINT_MSG
					printf("Failed to create task\n");
	#endif
					// Handle error
				}
				}
				else
					Add_Response_msg("Task for get record sync is already created", s_Message_Rx, payLoadData);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SERVER_CONNECT"))
			{
				if(Server_Conn_Flag == Enable)
				{
					printf("<%s.%s()\n", s_Message_Rx->dest_Actor_a8,s_Message_Rx->cmdFun_a8);
					initiate_ServerConnect(s_Message_Rx);
				}
				else{

					Add_Response_msg("Call SERVER_CONNECT Method after Enabling Server Connect Flag", s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "STACK_RESET"))
			{
				printf("<%s.%s()\n",s_Message_Rx->dest_Actor_a8,s_Message_Rx->cmdFun_a8);

				strcpy(stack_reset_buf, (char*) s_Message_Rx->payload_p8);
				Add_Response_msg("STACK_RESET method received", s_Message_Rx, payLoadData);

				vTaskDelay(10000 / portTICK_PERIOD_MS);

				strcpy((char*) s_Message_Rx->payload_p8, stack_reset_buf);

				Reset_Stack(s_Message_Rx, payLoadData);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SETSERVCONNFLAG"))
			{
				Set_Server_Connection_Flag(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "STATE"))
			{
				if(STATE_Que == NULL)
					STATE_Que = xQueueCreateStatic(5, 1000, State_pucQueueStorage, &State_pxQueueBuffer);

				if (STATE_Que == NULL)
				{
					printf("Unable to create SYSTEM.STATE method's task queue \n");
					continue;
				}
				if(STATE_Handle == NULL)
				{
					AMessage_st s_Message_Rx_data_New;
					memset(data_bufferSystem_states,0,sizeof(data_bufferSystem_states));
					s_Message_Rx_data_New.payload_p8 = (uint8_t*)data_bufferSystem_states;
					strcpy((char*)s_Message_Rx_data_New.dest_Actor_a8, "SYSTEM");
					strcpy((char*)s_Message_Rx_data_New.src_Actor_a8, (char*)s_Message_Rx->src_Actor_a8);
					strcpy((char*)s_Message_Rx_data_New.cmdFun_a8, (char*)s_Message_Rx->cmdFun_a8);

					STATE_Handle = xTaskCreateStaticPinnedToCore(
									Get_System_states,                 // Task function
									"STATE",            // Task name
									SYSTEM_STATE_TASK_STACK_DEPTH,        // Stack size in words
									&s_Message_Rx_data_New,                    // Task parameters (not used here)
									SYSTEM_STATE_TASK_PRIORITY,                       // Task priority
									xstate_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xStateTaskBuffer,             // Pointer to task control block
									1
					);
					if (STATE_Handle == NULL)
					{
						printf("Failed to create task for SYSTEM.STATE method.\n");
						continue;
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETTIMERS"))
			{
				if(TIMER_Que == NULL)
					TIMER_Que = xQueueCreateStatic(5, 1000, Timer_pucQueueStorage, &Timer_pxQueueBuffer);

				if (TIMER_Que == NULL)
				{
					printf("Error! Unable to create SYSTEM.GETTIMERS method's task queue \n");
					continue;
				}
				if(TIMER_Handle == NULL)
				{
					AMessage_st s_Message_Rx_data_New;
					memset(data_bufferSystem_timers,0,sizeof(data_bufferSystem_timers));
					s_Message_Rx_data_New.payload_p8 = (uint8_t*)data_bufferSystem_timers;
					strcpy((char*)s_Message_Rx_data_New.dest_Actor_a8, "SYSTEM");
					strcpy((char*)s_Message_Rx_data_New.src_Actor_a8, (char*)s_Message_Rx->src_Actor_a8);
					strcpy((char*)s_Message_Rx_data_New.cmdFun_a8, (char*)s_Message_Rx->cmdFun_a8);
					TIMER_Handle = xTaskCreateStaticPinnedToCore(
									Get_System_timers,                 // Task function
									"TIMERS",            // Task name
									SYSTEM_TIMER_TASK_STACK_DEPTH,        // Stack size in words
									&s_Message_Rx_data_New,                    // Task parameters (not used here)
									SYSTEM_TIMER_TASK_PRIORITY,                       // Task priority
									xstate_TaskStack_1,              // Pointer to task stack (allocated in PSRAM)
									&xTimerTaskBuffer,             // Pointer to task control block
									1
					);
					if (TIMER_Handle == NULL)
					{
						printf("Failed to create task for SYSTEM.GETTIMERS method.\n");
						continue;
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESET_CREDENTIALS"))
			{
				Reset_Credentials(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "POST_FILE_ACK"))
			{
				if(Post_File_Ack_Handle == NULL)
				{
					Init_Post_File_Ack(s_Message_Rx);
				}
				if(Post_File_Ack_Que != NULL)
				{
					xQueueSend(Post_File_Ack_Que, (char*) s_Message_Rx->payload_p8, QUE_DELAY);
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESETINTERNETCOUNT"))
			{
				InternetDisconnectCount = 0;
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "GMT_DST_SYSTEM"))
			 {
				set_gmt_dst(s_Message_Rx);
			 }
			else
			{
				//ACTOR_SYSTEM error message: invalid method
				  Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}

void SYSTEM_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstACTOR_SYSTEMEntry == 0)
		init(0,0);
	uint8_t state = xQueueSend(ACTOR_SYSTEM_Rx_Queue, s_Message, QUE_DELAY);
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<SYSTEM.ERROR(SYSTEM RX Queue is full)\n");
		}
		else
		{
			printf("<SYSTEM.ERROR(SYSTEM RX Queue send unsuccessful)\n");
		}
	}
}//	ACTOR_SYSTEM_ConsolWriteToActor

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[256] = {0};
	char typeString[20] = {0};
	int no_of_elements = sizeof(prop) / sizeof(struct property);

	    // Create JSON arrays
	    cJSON *jsonArrayName = cJSON_CreateArray();
	    cJSON *jsonArrayType = cJSON_CreateArray();
	    cJSON *jsonArrayValue = cJSON_CreateArray();
	    cJSON *jsonArrayAccess = cJSON_CreateArray();
	    cJSON *jsonArrayHelpString = cJSON_CreateArray();

	    for (int i = 0; i < no_of_elements; i++) {
			cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(prop[i].str_name));
			// Convert DataType enum to string representation for property type
			// Add value based on data type for property name
			switch (prop[i].type) {
				case U_INT8:
					strcpy(typeString, "U_INT8");
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					strcpy(typeString, "U_INT16");
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					strcpy(typeString, "U_INT32");
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case INT:
					strcpy(typeString, "INT");
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case FLOAT:
					strcpy(typeString, "FLOAT");
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(typeString, "STRING");
					strcpy(val_a8, prop[i].name);
					break;

				default:
					break;
				}
			cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
			cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(val_a8));
			cJSON_AddItemToArray(jsonArrayAccess, cJSON_CreateString(prop[i].access));
			cJSON_AddItemToArray(jsonArrayHelpString, cJSON_CreateString(prop[i].HelpString));
		}
		// Create a JSON object and add the array to it
		cJSON *jsonObject = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonObject, "Name", jsonArrayName);
		cJSON_AddItemToObject(jsonObject, "Type", jsonArrayType);
		cJSON_AddItemToObject(jsonObject, "Value", jsonArrayValue);
		cJSON_AddItemToObject(jsonObject, "Access", jsonArrayAccess);
		cJSON_AddItemToObject(jsonObject, "Help String", jsonArrayHelpString);
		
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the  ACTOR_SYSTEM actor.");
	cJSON_AddStringToObject(responseObject, "SET(U8 AUTO_EXEC)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table. Pass the parameter in prop and its value is return in val_a8");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "DEVICE_ANN()", "Announce the device.");
	cJSON_AddStringToObject(responseObject, "RECORD_SYNC(string DB_TYPE, string DBNAME, string RECORD_DATA)", "Record Sync: Save record in the database and post it.");
	cJSON_AddStringToObject(responseObject, "POST_DEBUG_LOG()", "Post debug log.");
	cJSON_AddStringToObject(responseObject, "DBSYNC_STATUS()", "Obtain record sync status.");
	cJSON_AddStringToObject(responseObject, "SERVER_CONNECT()", "Server Connection flow");
	cJSON_AddStringToObject(responseObject, "STACK_RESET(int RESET_TIME)", "Deinit HTTP, WIFI, ETH, and IHUB actors and reinit them. RESET_TIME in minute");
	cJSON_AddStringToObject(responseObject, "STATE()", "Display various system states.");
	cJSON_AddStringToObject(responseObject, "GETTIMERS()", "Display various timer values.");
	cJSON_AddStringToObject(responseObject, "RESET_CREDENTIALS()", "Reset the device credentials");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);

}//	help

static void Get_Property_Parameters(AMessage_st* s_Message_Rx)
{
		cJSON *in_JSON   = NULL;
		cJSON *out_JSON  = NULL;
		cJSON *head_JSON = NULL;
		char str[100] = {0};
		char val_p8[256] = {0};

		in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		out_JSON 	= cJSON_CreateObject();
		head_JSON 	= in_JSON->child;

		do {
			memset(val_p8, 0, sizeof (val_p8));
			get(head_JSON->string, val_p8);
			cJSON_AddStringToObject(out_JSON, head_JSON->string, (char*) val_p8);
			head_JSON = head_JSON->next;
		} while (head_JSON != NULL);

		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(out_JSON);
		cJSON_Delete(head_JSON);
		cJSON_Delete(in_JSON);
		console_send_responce_to_console_xface(s_Message_Rx);
}

static void Get_Property(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	char val_p8[256] = {0};
	int Array_size = 0;

	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	out_JSON 	= cJSON_CreateObject();
	head_JSON = cJSON_GetObjectItem(in_JSON, "Property_Names");
	Array_size = cJSON_GetArraySize(head_JSON);
	if(Array_size > 0)
	{
		for(int i=0; i<Array_size; i++)
		{
			cJSON *element = cJSON_GetArrayItem(head_JSON, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL))
			{
				if(strlen(element->valuestring) == 0)
					continue;
				memset(val_p8, 0, sizeof (val_p8));
				get(element->valuestring, val_p8);
				cJSON_AddStringToObject(out_JSON, element->valuestring, (char*) val_p8);
			}
		}
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
	{
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx, payLoadData);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  		= (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	strcpy((char*)newpointer, response);
	s_Message_Tx_new.payload_p8 	=  newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char keyValue[100] = {0};
	char str[200] = {0}, str1[30] = {0}, str2[30] = {0}, str3[30] = {0};
	uint8_t temp[10];
    

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL)
	{
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		 printf("\n system s_Message_Rx->payload_p8 = %s",s_Message_Rx->payload_p8);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	else
	{
			// Obtain the COMMAND and RESPONSE keys
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

			if (commandKey != NULL && responseKey != NULL)
			{
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"HTTP")==0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				  //  return 1;
				return;
			}
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"POST")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL && cJSON_IsString(responseKey->child))
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "SESSIONID");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						char sessionid[200] = {0};
						strcpy((char*)sessionid,name_JSON->valuestring);
						SendAcktoServer(sessionid);
					}
				}
				if (responseKey != NULL && cJSON_IsNumber(responseKey->child))
				{
					cJSON *name_JSONMsg 		= cJSON_GetObjectItem(responseKey, "MESSAGE");
					if((name_JSONMsg != NULL) && (cJSON_IsObject(name_JSONMsg)))  // send device announce's successful response to server connect
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);				
						if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
							xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					}
					cJSON *http_status 		= cJSON_GetObjectItem(responseKey, "HTTP STATUS");
					cJSON *http_error 		= cJSON_GetObjectItem(responseKey, "ERROR");
					if((http_status != NULL) && (cJSON_IsNumber(http_status)) && ((http_error != NULL)))  // send device announce's failed response to server connect
					{
						if(http_status->valueint != 200)
						{
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
							if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
								xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
						}
					}
				}
			}

			if (cJSON_IsString(commandKey) &&(strcmp(commandKey->valuestring,"GET")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL && cJSON_IsString(responseKey->child))
				{
					cJSON *name_JSON1 		= cJSON_GetObjectItem(responseKey, "HTTP_DL_TIME");
					if((name_JSON1 != NULL) && (cJSON_IsString(name_JSON1)))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if((TIMER_Que!=NULL) && (TIMER_Handle != NULL))
						{
							xQueueSend(TIMER_Que, payLoadData, QUE_DELAY);
						}
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			 printf("\n 3 s_Message_Rx->payload_p8 = %s",s_Message_Rx->payload_p8);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if ((root != NULL)  && (cJSON_IsObject(root)))
			{
				// Iterate over the keys
				cJSON *currentItem = root->child;
				if(currentItem->valuestring != NULL)
				{
					if(!strcasecmp(currentItem->valuestring, "System/SYSTEM.json"))
					{
						currentItem = currentItem->next;
						while (currentItem != NULL)
						{
							if (cJSON_IsString(currentItem))   // Check the type of the value
							{
								set(currentItem->string, currentItem->valuestring,s_Message_Rx);
							}
							else if (cJSON_IsNumber(currentItem))
							{
								sprintf(keyValue, "%d", currentItem->valueint);
								set(currentItem->string, keyValue,s_Message_Rx);
							}
							currentItem = currentItem->next;    // Move to the next key-value pair
						}
					}
				}
			}
			if((ServerConnect_Handle == NULL) )  //&& (initiate_server_connect_flag >= 2)
			{
				initiate_ServerConnect(s_Message_Rx);
			}
		}
		else if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SMTP_READ")==0))
	    {
		   cJSON *jmsg_object = cJSON_GetObjectItem(in_JSON, "RESPONSE");
		   if ((jmsg_object != NULL)  && (cJSON_IsObject(jmsg_object)))
			{
			   cJSON* file_data_value = cJSON_GetObjectItemCaseSensitive(jmsg_object, "RESP");
				// Check if "File Data" value exists
				if ((file_data_value != NULL) && (file_data_value->valuestring != NULL))  // && (smtp_Response == 0)
				{
					memset(payLoadData,0,sizeof(payLoadData));
					strcpy(payLoadData, file_data_value->valuestring); // = cJSON_GetStringValue(file_data_value);  // after using this we get some garbage data at the end
					xQueueSend(Debug_Que, payLoadData, QUE_DELAY);
				}
			}
	    }
		else if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_FILE_LIST")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsArray(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "files");
				if((name_JSON != NULL) && (cJSON_IsArray(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Sync_Status_Que != NULL)
						xQueueSend(Sync_Status_Que, payLoadData, QUE_DELAY);

				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SQL")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ_DB_RECORDS")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL)
			{
				cJSON *DB_NAME = cJSON_GetObjectItem(responseKey, "DB_NAME");
				if(DB_NAME != NULL)
				{					
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Record_Sync_new_Que != NULL)
					{
						uint8_t state = xQueueSend(Record_Sync_new_Que, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
							if (state == errQUEUE_FULL){
#ifdef ENABLE_PRINT_MSG
								printf("<SYSTEM.ERROR(Record_Sync_new_Que is full)<CR>\n");
#endif
							}
								else{
#ifdef ENABLE_PRINT_MSG
								printf("<SYSTEM.ERROR(Record_Sync_new_Que send unsuccessful)<CR>");
#endif
						}
						}
					}
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_SYNC_STATUS")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL)
			{
				cJSON *SYNC_STATUS = cJSON_GetObjectItem(responseKey, "SYNC_STATUS");
				if(SYNC_STATUS != NULL)
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Sync_Status_Que != NULL)
					{
						uint8_t state = xQueueSend(Sync_Status_Que, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
							if (state == errQUEUE_FULL){
#ifdef ENABLE_PRINT_MSG
								printf("<SYSTEM.ERROR(Sync_Status_Que is full)<CR>\n");
#endif
							}					else{
#ifdef ENABLE_PRINT_MSG
								printf("<SYSTEM.ERROR(Sync_Status_Que send unsuccessful)<CR>");
#endif
							}
						}
					}
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SAVE_SRV_CONN")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{;
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
				}
			}

		}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			  //  return 1;
			return;
		}
		cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) &&(strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{

				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "ETH_STATUS");
				cJSON *name_JSON1 		= cJSON_GetObjectItem(responseKey, "ETH_UPDATE_TIME");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)) && (name_JSON1 == NULL))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);					
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
				if((name_JSON1 != NULL) && (cJSON_IsString(name_JSON1)))
				{					
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);						
					if(STATE_Que!=NULL)
						xQueueSend(STATE_Que, payLoadData, QUE_DELAY);  //
				}

			}
		}
		if (responseKey != NULL && cJSON_IsString(responseKey->child))
		{
			cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
			{
				strcpy((char*)temp,name_JSON->valuestring);
				NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
				if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
				}
			}
			cJSON *ENABLE_WIFI_name_JSON 		= cJSON_GetObjectItem(responseKey, "ENABLE_WIFI");
			cJSON *name_JSON1 		= cJSON_GetObjectItem(responseKey, "ETH_UPDATE_TIME");
			if((ENABLE_WIFI_name_JSON != NULL) && (cJSON_IsString(ENABLE_WIFI_name_JSON)) && (name_JSON1 == NULL))
			{
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
					xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);

				if((TIMER_Que!=NULL) && (TIMER_Handle != NULL))
					xQueueSend(TIMER_Que, payLoadData, QUE_DELAY);
			}

		}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"WIFI")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"NETWORK_SCAN")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *ssid_name_JSON 		= cJSON_GetObjectItem(responseKey, "SSID");
				cJSON *nw_name_JSON 		= cJSON_GetObjectItem(responseKey, "NETWORK");
				cJSON *eth_name_JSON 		= cJSON_GetObjectItem(responseKey, "ETHERNET");

				if(((ssid_name_JSON != NULL) && (cJSON_IsString(ssid_name_JSON))) ||
						((nw_name_JSON != NULL) && (cJSON_IsString(nw_name_JSON))) ||
						((eth_name_JSON != NULL) && (cJSON_IsString(eth_name_JSON))))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"EVENT")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "WIFI_CONNECTION_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						 xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"PING")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"AP_INFO")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "SSID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}				

				cJSON *name_err 		= cJSON_GetObjectItem(responseKey, "ERROR");
				if((name_err != NULL) && (cJSON_IsString(name_err)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESCUE_SSID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
				cJSON *name_JSON_new 		= cJSON_GetObjectItem(responseKey, "WIFI_DISCON_REASON");
				if((name_JSON_new != NULL) && (cJSON_IsString(name_JSON_new)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(STATE_Que!=NULL)
						xQueueSend(STATE_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SCAN")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"RESCUE_SSID")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}

		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"ETH")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"PING")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"EVENT")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "ETH_CONNECTION_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
					if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"IHUB")==0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

				if (responseKey != NULL && cJSON_IsString(responseKey->child))
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "STATUS");
					cJSON *name_JSON1 		= cJSON_GetObjectItem(responseKey, "IHUB_UPDATE_TIME");
					cJSON *name_JSON2 		= cJSON_GetObjectItem(responseKey, "IHUB_DISCON_REASON");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)) && (name_JSON1 == NULL))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
							xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);

						if((TIMER_Que!=NULL) && (TIMER_Handle != NULL))
						{
							xQueueSend(TIMER_Que, payLoadData, QUE_DELAY);
						}

					}

					if((name_JSON1 != NULL) && (cJSON_IsString(name_JSON1)))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if((TIMER_Que!=NULL) && (TIMER_Handle != NULL)  && (name_JSON2 == NULL))
						{
							xQueueSend(TIMER_Que, payLoadData, QUE_DELAY);
						}
						if((STATE_Que!=NULL) && (STATE_Handle != NULL)  && (name_JSON2 != NULL))
							xQueueSend(STATE_Que, payLoadData, QUE_DELAY);
					}
				}
			}

			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"EVENT")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL)  //&& cJSON_IsString(responseKey->child)
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "IHUB_CONNECTION_STATUS");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						if((!strcmp(name_JSON->valuestring, "DISCONNECTED")))
						{
							//test code start
//							printf("test code \n");
		                    // Send ping command to check server connectivity
		                    cJSON *responseObject = cJSON_CreateObject();
//		                    cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "8.8.8.8");
		                    cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");

		                    cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
							cJSON_Delete(responseObject);
		                    if (ping_dest_id == WIFI) {
		                        Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData, strlen(payLoadData), "PING");
		                    } else {
		                        Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData, strlen(payLoadData), "PING");
		                    }
		                    //test code end
						}

						if((MonSrvConn_Handle==NULL))// && (!strcmp(name_JSON->valuestring, "CONNECTED")))
						{
							if((!strcmp(name_JSON->valuestring, "CONNECTED")))
							{


								if(esp_timer_is_active(periodic_timer) == false)  // start the timer if it is not yet started
								{

									esp_err_t err = esp_timer_start_periodic(periodic_timer, (10800000000)); // start the timer with the period of 3x60x60x1000000 sec
									if(err != ESP_OK)
										printf("\n\n set Error in start periodic timer. err = %d\n\n", err);
								}

								if(ServerConnect_Handle != NULL)
								{
									cJSON *root_JSON = cJSON_CreateObject();
									cJSON_AddStringToObject(root_JSON, "IHUBRESP", "CONNECTED");
									memset(payLoadData,0,sizeof(payLoadData));
									cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
									cJSON_Delete(root_JSON);
									if((ServerConnect_Que!=NULL) && (strlen(payLoadData) != 0) && (ServerConnect_Handle != NULL))
									{
										xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
									}
								}
								InitMonSrvConn();
							}
							else if((!strcmp(name_JSON->valuestring, "DISCONNECTED")))
							{

								if(esp_timer_is_active(periodic_timer) == true)
								{

									esp_err_t err = esp_timer_stop(periodic_timer); // stop the timer
									if(err != ESP_OK)
										printf("\n\n set Error in stopping the periodic timer. err = %d\n\n", err);
								}

								uint16_t error_val_ihub = 0;
								if(ServerConnect_Handle != NULL)
								{
									cJSON *ERROR_NUM_item 		= cJSON_GetObjectItem(responseKey, "ERROR_NUM");
									if (cJSON_IsNumber(ERROR_NUM_item) && (ERROR_NUM_item != NULL))
									{
										error_val_ihub = ERROR_NUM_item->valueint;
									}
									cJSON *root_JSON = cJSON_CreateObject();
									cJSON_AddStringToObject(root_JSON, "IHUBRESP", "DISCONNECTED");
									cJSON_AddNumberToObject(root_JSON, "ERROR_NUM_IHUB",error_val_ihub);
									memset(payLoadData,0,sizeof(payLoadData));
									cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
									cJSON_Delete(root_JSON);
									if((ServerConnect_Que!=NULL) && (strlen(payLoadData) != 0) && (ServerConnect_Handle != NULL))
									{
										xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
									}
								}
								else
								{
									if(Server_Conn_Flag == Enable)
									{
										printf("<%s.STACK_RESET()\n", s_Message_Rx->dest_Actor_a8);
										Reset_Stack(s_Message_Rx, payLoadData);
									}
								}
							}
						}

						else if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						{
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
							xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
						}
					}
					cJSON *Twin_JSON 		= cJSON_GetObjectItem(responseKey, "TWIN_MESSAGE");
					if((Twin_JSON != NULL) && (cJSON_IsObject(Twin_JSON)))
					{
						cJSON *root_JSON = cJSON_CreateObject();
						cJSON_AddStringToObject(root_JSON, "TWIN_MESSAGE", "RECEIVED");
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
						cJSON_Delete(root_JSON);
						if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
						{
							xQueueSend(MonSrvConn_Que, payLoadData, QUE_DELAY);
						}
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"BLE")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

		if (cJSON_IsString(commandKey) &&
						(strcmp(commandKey->valuestring,"EVENT")==0))
		{cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
		if (responseKey != NULL && cJSON_IsString(responseKey->child))
		{
			cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "CONNECTION_STATUS");
			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
			{
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
					xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
			}
			cJSON *name_RECEIVED_DATA 		= cJSON_GetObjectItem(responseKey, "RECEIVED_DATA");
			if((name_RECEIVED_DATA != NULL) && (cJSON_IsString(name_RECEIVED_DATA)))
			{
//				memset(payLoadData,0,sizeof(payLoadData));
//				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
//				if(ServerConnect_Que!=NULL)
//					xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
//				cJSON_free(filedata);
			}
		  }
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"ADVERT_START")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
						xQueueSend(ServerConnect_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "ADVERT_MODE");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(STATE_Que != NULL)
						xQueueSend(STATE_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SPIFFS")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ_PARA")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "FIRMWARE_VERSION");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(SPIFFS_Que!=NULL)
					{
						uint8_t state = xQueueSend(SPIFFS_Que, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
							if (state == errQUEUE_FULL)
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<SYSTEM.ERROR(SPIFFS_Que is full)<CR>\n");
								#endif
							}
							else
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<SYSTEM.ERROR(SPIFFS_Que send unsuccessful)<CR>");
								#endif
							}
						}
					}
				}
			}
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "SPIFFS_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(STATE_Que!=NULL)
					{
						uint8_t state = xQueueSend(STATE_Que, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
							if (state == errQUEUE_FULL)
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<SYSTEM.ERROR(STATE_Que is full)<CR>\n");
								#endif
							}
							else
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<SYSTEM.ERROR(STATE_Que send unsuccessful)<CR>");
								#endif
							}
						}
					}
				}
			  }
			}
		cJSON_Delete(in_JSON);
		return;
	}
	else if(strcmp((char*)s_Message_Rx->src_Actor_a8,"EVENT_ACTOR")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"EVENT")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL )	//&& cJSON_IsString(responseKey->child)
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "Local TIME");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					strcpy(str1, name_JSON->valuestring);
				}

				cJSON *name_JSON_2 		= cJSON_GetObjectItem(responseKey, "SUNRISE Time");
				if((name_JSON_2 != NULL) && (cJSON_IsString(name_JSON_2)))
				{
					strcpy(str2, name_JSON_2->valuestring);
				}

				cJSON *name_JSON_3 		= cJSON_GetObjectItem(responseKey, "SUNSET Time");
				if((name_JSON_3 != NULL) && (cJSON_IsString(name_JSON_3)))
				{
					strcpy(str3, name_JSON_3->valuestring);
				}

				if((name_JSON != NULL) && (name_JSON_2 != NULL)  && (name_JSON_3 != NULL))
				{
					strcpy(localtime_str, str1+8);
					strcat(localtime_str, "|");

					strcat(localtime_str, str2);
					strcat(localtime_str, "|");

					strcat(localtime_str, str3);
				}
			}
		}

		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_RTC_TIME")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL )	//&& cJSON_IsString(responseKey->child)
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "Local TIME");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					strcpy(str1, name_JSON->valuestring);
				}

				cJSON *name_JSON_2 		= cJSON_GetObjectItem(responseKey, "SUNRISE Time");
				if((name_JSON_2 != NULL) && (cJSON_IsString(name_JSON_2)))
				{
					strcpy(str2, name_JSON_2->valuestring);
				}

				cJSON *name_JSON_3 		= cJSON_GetObjectItem(responseKey, "SUNSET Time");
				if((name_JSON_3 != NULL) && (cJSON_IsString(name_JSON_3)))
				{
					strcpy(str3, name_JSON_3->valuestring);
				}

				if((name_JSON != NULL) && (name_JSON_2 != NULL)  && (name_JSON_3 != NULL))
				{
					strcpy(localtime_str, str1+8);
					strcat(localtime_str, "|");

					strcat(localtime_str, str2);
					strcat(localtime_str, "|");

					strcat(localtime_str, str3);
				}
			}
		}

		cJSON_Delete(in_JSON);
		return;
	}
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
//	memset(payLoadData_new,0,MAX_JSON_PAYLOAD_BYTES/2);//\0';
	payLoadData_new[0]='\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

//------------------------- System actor Functions -----------------------------//

static void format_mac_colon_upper(const uint8_t mac[6], char *out, size_t out_sz)
{
	if (out == NULL || out_sz < 18) {
		return;
	}
	snprintf(out, out_sz, "%02X:%02X:%02X:%02X:%02X:%02X",
	         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

/** Build "network" object for device announce JSON (Wi-Fi vs Ethernet per active link). */
static cJSON *create_device_announce_network_object(void)
{
	cJSON *network = cJSON_CreateObject();
	if (network == NULL) {
		return NULL;
	}

	char ip_str[20] = "0.0.0.0";
	esp_netif_ip_info_t ip_info;
	memset(&ip_info, 0, sizeof(ip_info));

	if (ping_dest_id == (char)ETH) {
		cJSON_AddStringToObject(network, "connection_type", "ethernet");
		/* Use ETH actor netif pointer; ifkey lookup often fails for esp_netif_new() SPI eth. */
		esp_netif_t *ni = get_ethernet_netif_handle();
		if (ni != NULL && esp_netif_get_ip_info(ni, &ip_info) == ESP_OK) {
			snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
		}
		/* Fallback: IP cached in ETH actor on IP_EVENT_ETH_GOT_IP (also if announce runs before netif read updates). */
		if (strcmp(ip_str, "0.0.0.0") == 0) {
			const char *eth_ip = get_ethernet_device_ip();
			if (eth_ip != NULL && eth_ip[0] != '\0' && strcmp(eth_ip, "0.0.0.0") != 0) {
				strncpy(ip_str, eth_ip, sizeof(ip_str) - 1);
				ip_str[sizeof(ip_str) - 1] = '\0';
			}
		}
		cJSON_AddStringToObject(network, "ip_address", ip_str);

		cJSON *eth = cJSON_CreateObject();
		char mac_str[20] = "00:00:00:00:00:00";
		uint8_t emac[6] = {0};
		if (esp_read_mac(emac, ESP_MAC_ETH) == ESP_OK) {
			format_mac_colon_upper(emac, mac_str, sizeof(mac_str));
		}
		cJSON_AddStringToObject(eth, "eth_mac", mac_str);
		const char *eth_gw_mac = get_ethernet_gateway_mac();
		cJSON_AddStringToObject(eth, "eth_gateway_mac",
				(eth_gw_mac != NULL && eth_gw_mac[0] != '\0') ? eth_gw_mac : "00:00:00:00:00:00");
		cJSON_AddItemToObject(network, "ethernet", eth);
	} else {
		/* Wi-Fi (or unknown ping_dest_id: treat as STA best-effort) */
		cJSON_AddStringToObject(network, "connection_type", "wifi");
		esp_netif_t *ni = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
		if (ni != NULL && esp_netif_get_ip_info(ni, &ip_info) == ESP_OK) {
			snprintf(ip_str, sizeof(ip_str), IPSTR, IP2STR(&ip_info.ip));
		}
		cJSON_AddStringToObject(network, "ip_address", ip_str);

		cJSON *wifi = cJSON_CreateObject();
		wifi_ap_record_t ap = {0};
		char mac_str[20] = "00:00:00:00:00:00";
		char ssid_str[33] = {0};
		esp_err_t ap_ok = esp_wifi_sta_get_ap_info(&ap);

		if (ap_ok == ESP_OK) {
			size_t L = strnlen((char *)ap.ssid, sizeof(ap.ssid));
			if (L >= sizeof(ssid_str)) {
				L = sizeof(ssid_str) - 1;
			}
			memcpy(ssid_str, ap.ssid, L);
			ssid_str[L] = '\0';
			format_mac_colon_upper(ap.bssid, mac_str, sizeof(mac_str));
		}
		cJSON_AddStringToObject(wifi, "ssid", ssid_str);
		cJSON_AddStringToObject(wifi, "ap_mac", mac_str);

		uint8_t sta_mac[6] = {0};
		if (esp_wifi_get_mac(WIFI_IF_STA, sta_mac) == ESP_OK) {
			format_mac_colon_upper(sta_mac, mac_str, sizeof(mac_str));
		} else {
			mac_str[0] = '\0';
		}
		cJSON_AddStringToObject(wifi, "wifi_mac", mac_str);

		int rssi = 0;
		if (ap_ok == ESP_OK) {
			rssi = ap.rssi;
		}
		cJSON_AddNumberToObject(wifi, "rssi_dbm", rssi);
		cJSON_AddItemToObject(network, "wifi", wifi);
	}
	return network;
}

static void httpDeviceAnnounceInit(AMessage_st* s_Message_Rx)
{
	char CPU0_REST_REA[50] = {0};
	char CPU1_REST_REA[50] = {0};
	Get_ESP_Reset_Reason(CPU0_REST_REA, CPU1_REST_REA);
	uint64_t mills =0;
	struct timeval currentTime;
	cJSON *name_JSON 	= NULL;
	cJSON *root 	= NULL;
	gettimeofday(&currentTime, NULL); // Get current time
	mills = (uint64_t)(currentTime.tv_sec * 1000L) + (uint64_t)(currentTime.tv_usec / 1000L); // Convert to milliseconds
	
	name_JSON = cJSON_CreateObject();
	root = cJSON_CreateObject();
	cJSON_AddStringToObject(name_JSON, "DEVICEID",(char*)&ACTOR_SYSTEM_Para.DeviceId);// "225_SAT1");  // source info"SAT_TESTDEV1");  // source info
	cJSON_AddStringToObject(name_JSON, "UniqueProductName", (char*)&ACTOR_SYSTEM_Para.Model_name_a8);
	cJSON *array = cJSON_CreateArray();
	cJSON_AddItemToArray(array, cJSON_CreateString((char*)&ACTOR_SYSTEM_Para.FirmwareVer));
	cJSON_AddItemToObject(name_JSON, "FirmwareVersions", array);
	cJSON *array2 = cJSON_CreateArray();
	cJSON_AddItemToArray(array2, cJSON_CreateString((char*)&ACTOR_SYSTEM_Para.HardwareVer));
	cJSON_AddItemToObject(name_JSON, "HardwareVersions", array2);
	cJSON_AddNumberToObject(name_JSON, "UtcDate",mills);//mills);  //682355204);

	cJSON_AddStringToObject(name_JSON, "localTimeInfo",localtime_str);//mills);  //682355204);
	cJSON_AddStringToObject(name_JSON, "CPU0ResetReason",CPU0_REST_REA);//mills);  //682355204);
	cJSON_AddStringToObject(name_JSON, "CPU1ResetReason",CPU1_REST_REA);//mills);  //682355204);
	{
		cJSON *network = create_device_announce_network_object();
		if (network != NULL) {
			cJSON_AddItemToObject(name_JSON, "network", network);
		}
	}
	cJSON_AddStringToObject(root, "URL", (char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL);
	cJSON_AddItemToObject(root, "HTTP_PAYLOAD", name_JSON);
	cJSON_AddStringToObject(root, "API_KEY", (char*)&ACTOR_SYSTEM_Para.APIkey);
	cJSON_AddStringToObject(root, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);
	cJSON_AddNumberToObject(root, "RETRY_PERIOD", ACTOR_SYSTEM_Para.deviceAnnouncePeriod);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(root);  // It will free the memory allocated for out_JSON also
    Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData, strlen(payLoadData), "POST");
}

static void SendAcktoServer(char* SessionId)
{
	strcat((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL,"/");
	strcat((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL,SessionId);
	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "URL", (char*) &ACTOR_SYSTEM_Para.DeviceAnnounce_ACK_URL);
	cJSON_AddStringToObject(root, "HTTP_PAYLOAD", "\0");
	cJSON_AddStringToObject(root, "API_KEY", (char*)&ACTOR_SYSTEM_Para.APIkey);
	cJSON_AddStringToObject(root, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);
	cJSON_AddNumberToObject(root, "RETRY_PERIOD", ACTOR_SYSTEM_Para.deviceAnnouncePeriod);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);	
    Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData, strlen(payLoadData), "POST");
	cJSON_Delete(root);  // It will free the memory allocated for out_JSON also
}

static void Fetch_Record_Log (AMessage_st* s_Message_Rx)   //READ_RECORD
{
	int Start_ID = 0, End_ID = 0;
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	cJSON *root 	= NULL;
	char *dot_position = NULL;
	char extension[20] = {0};
	char Database_name [100] = {0}, Database_table_name [100] = {0}, str[150]={0};
	char *token = NULL;
	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);  //It is deleted in http_stream_reader() function
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "DB_Name");
	if(name_JSON != NULL)
	{
		strcpy(Database_name,name_JSON->valuestring);
	}
	dot_position = strchr(Database_name, '.');

	    if (dot_position != NULL) {
	        // Extract the substring after the dot
	        strcpy(extension, dot_position + 1);
	        // Print the extracted extension
	        if(strcmp(extension, "json") == 0)
	        {
	        	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", (char*)s_Message_Rx->payload_p8, strlen((char*)s_Message_Rx->payload_p8), "READ_RECORDS");
	        	cJSON_Delete(in_JSON);
	        	return;
	        }
	    }
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "ID_START");
	if(name_JSON != NULL)
	{
		Start_ID = name_JSON->valueint;
	}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "ID_END");
	if(name_JSON != NULL)
	{
		End_ID = Start_ID + name_JSON->valueint - 1;
	}
	cJSON_Delete(in_JSON);

	token = strtok(Database_name, ".");  // Obtain table name
	strcat(Database_table_name, token);
	strcat(Database_table_name, "_table");
	strcat(Database_name, ".db");
	sprintf(str, "SELECT * FROM %s WHERE id BETWEEN %d AND %d;", Database_table_name, Start_ID, End_ID);

	root = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
	cJSON_AddStringToObject(root, "FILE_NAME", Database_name);
	cJSON_AddStringToObject(root, "QUERY", str);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);	
	Send_CMD_To_Other_Actor(SQL,"SQL", payLoadData, strlen(payLoadData), "READ_DB_RECORDS");
	cJSON_Delete(root);

	// copy the structure
	AMessage_st s_Message_Rx_data;
	char Data_buffer[512];
	memset(Data_buffer,0,sizeof(Data_buffer));
	strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
	strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
	strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);
	if(strlen((char*)s_Message_Rx->payload_p8) != 0)
	{
		strncpy(Data_buffer, (char*)s_Message_Rx->payload_p8,511);
	}
	s_Message_Rx_data.payload_p8 = (uint8_t*)Data_buffer;

	if(Record_Sync_new_Que == NULL)
		Record_Sync_new_Que = xQueueCreateStatic(Record_Sync_New_Que_COUNT, Record_Sync_New_Que_COUNT_ITEMSIZE, RcrdSyncNewQ_pucQueueStorage, &RcrdSyncNewQ_pxQueueBuffer);

	if (Record_Sync_new_Que == NULL){
#ifdef ENABLE_PRINT_MSG
		printf("ERROR(Record_Sync_new_Que is not created)\n");
#endif
	}
	if(Record_new_Handle == NULL)
	{
		Record_new_Handle = xTaskCreateStatic(
						Send_RecordLogData_Server,                 // Task function
						"RECORDS SYNC NEW",            // Task name
						SEND_RECORDLOG_SERVER_STACK_DEPTH,        // Stack size in words
						&s_Message_Rx_data,                    // Task parameters (not used here)
						SEND_RECORDLOG_SERVER_PRIORITY,                       // Task priority
						xRcrdSyncNewTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xRcrdSyncNewTaskBuffer             // Pointer to task control block
	   );

	if (Record_new_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
		}
	}
}

static void Send_RecordLogData_Server(void *pvParameters __attribute__((unused)))
{
		cJSON *out_JSON 	= NULL;
		cJSON *name_JSON 	= NULL;
		cJSON *in_JSON 	= NULL;
		cJSON *root 	= NULL;
		struct timeval currentTime;
		AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
		char str[100] = {0}, Database_name[50] = {0}, Record_sync_DBname_u8[30]={0};
		int Log_ID_End = 0, Log_ID_MIN = 0;

		if (pdTRUE == xQueueReceive(Record_Sync_new_Que, (void*)bufferRecordLog, portMAX_DELAY))
			{
			out_JSON = cJSON_Parse((char*)bufferRecordLog);
			if (out_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_Record);
				goto exit;
			}

			root = cJSON_CreateObject();
			name_JSON = cJSON_CreateObject();

			in_JSON 		= cJSON_GetObjectItem(out_JSON, "DB_NAME");
			if((in_JSON != NULL) && (cJSON_IsString(in_JSON)))
			{
				strcpy(Database_name,in_JSON->valuestring);
			}
			cJSON_AddStringToObject(name_JSON, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);  // source info
			gettimeofday(&currentTime, NULL); // Get current time
			uint64_t mills = (uint64_t)(currentTime.tv_sec * 1000L) + (uint64_t)(currentTime.tv_usec / 1000L); // Convert to milliseconds
			cJSON_AddNumberToObject(name_JSON, "DeviceTime", mills);
			in_JSON 		= cJSON_GetObjectItem(out_JSON, "DB_RECORDS");
			if((in_JSON != NULL) && (cJSON_IsArray(in_JSON)))
			{
				cJSON_AddItemToObject(name_JSON, "RECORDS", in_JSON);
			}
			in_JSON 	  = cJSON_GetObjectItem(out_JSON, "MAX_ID");  // for SQL
			if((in_JSON != NULL)&&(cJSON_IsNumber(in_JSON)))
				Log_ID_End = in_JSON->valuedouble;

			in_JSON 	  = cJSON_GetObjectItem(out_JSON, "MIN_ID");  // for SQL
			if((in_JSON != NULL)&&(cJSON_IsNumber(in_JSON)))
				Log_ID_MIN = in_JSON->valuedouble;

			// Find the position of the '.' character
			char *dot_position = strchr(Database_name, '.');

			// If '.' is found, copy the substring before it
			if (dot_position != NULL) {
				strncpy(Record_sync_DBname_u8, Database_name, dot_position - Database_name);
				Record_sync_DBname_u8[dot_position - Database_name] = '\0'; // Null-terminate the string
			} else {
				// If '.' is not found, copy the entire string
				strcpy(Record_sync_DBname_u8, Database_name);
			}
			strcat((char*)Record_sync_DBname_u8, ".db");
			cJSON_AddStringToObject(name_JSON, "DBNAME", Database_name);
			cJSON_AddBoolToObject(name_JSON, "ISLIVE", false);
			cJSON_AddNumberToObject(name_JSON, "MinRecord", Log_ID_MIN);
			cJSON_AddNumberToObject(name_JSON, "MaxRecord", Log_ID_End);
			cJSON_AddStringToObject(root, "URL", (char*) &ACTOR_SYSTEM_Para.HTTP_endpoint_u8);
			cJSON_AddItemToObject(root, "HTTP_PAYLOAD", name_JSON);
			cJSON_AddStringToObject(root, "API_KEY", (char*)&ACTOR_SYSTEM_Para.APIkey);
			cJSON_AddStringToObject(root, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);
			cJSON_AddNumberToObject(root, "RETRY_PERIOD", ACTOR_SYSTEM_Para.deviceAnnouncePeriod);
		
			memset(payLoadData_Record,0,sizeof(payLoadData_Record));
			cJSON_PrintPreallocated(root, payLoadData_Record, sizeof(payLoadData_Record), false);

			if(strlen((char*)&ACTOR_SYSTEM_Para.HTTP_endpoint_u8) != 0)
				Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_Record, strlen(payLoadData_Record), "POST");
			else
				Add_Response_msg(" Error! 'RECORDSYNC_URL' is not set. Hence, cannot post the DB records.",s_Message_Rx, payLoadData_Record);
			cJSON_Delete(root);  // It will free the memory allocated for out_JSON also
		}
exit:
		Record_new_Handle = NULL;
		vTaskDelete(Record_new_Handle);  // Delete the task
}

static void Record_sync(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	cJSON *out_JSON 	= NULL;
	char str[100] = {0}, DB_Type[10] = {0};
	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);  //It is deleted in http_stream_reader() function
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}

	out_JSON = cJSON_CreateObject();
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "DB_TYPE");
	if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
	{
		strcpy(DB_Type, name_JSON->valuestring);
	}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "DBNAME");
	if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
	{
		cJSON_AddStringToObject(out_JSON, "DBNAME", name_JSON->valuestring);
		//strcpy((char*)Record_sync_DBname_u8, name_JSON->valuestring);
	}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "RECORD_DATA");
	if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
	{
		cJSON_AddStringToObject(out_JSON, "RECORD_DATA", name_JSON->valuestring);
	}

	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);				

	if(strcmp(DB_Type, "SQL") == 0)
		Send_CMD_To_Other_Actor(SQL,"SQL", payLoadData, strlen(payLoadData), "SAVE_DB_RECORD");
	else if(strcmp(DB_Type, "FILE") == 0)
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "LOG_DATA");
	else
	{
		cJSON_Delete(out_JSON);
		Add_Response_msg("Kindly enter database type as 'SQL' or 'FILE'.",s_Message_Rx, payLoadData);
		return;
	}
	cJSON_Delete(out_JSON);
}

static void Get_Record_Sync_Status(void *pvParameters __attribute__((unused)))
{
	cJSON *in_JSON 		= NULL;
	cJSON *my_JSON 		= NULL;
	char str[100] = {0};//  bufferRecord_Sync[2000] = {0};  //Sync_URL[100] = {0},
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "DIR_NAME", "A:/Database"); 
	memset(payLoadData_Re_Sync_St,0,sizeof(payLoadData_Re_Sync_St));
	cJSON_PrintPreallocated(my_JSON, payLoadData_Re_Sync_St, sizeof(payLoadData_Re_Sync_St), false);			
	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Re_Sync_St, strlen(payLoadData_Re_Sync_St), "GET_FILE_LIST");
	cJSON_Delete(my_JSON);

	if (pdTRUE == xQueueReceive(Sync_Status_Que, (void*)bufferRecord_Sync, 5000))
	{
		in_JSON 		= cJSON_Parse(bufferRecord_Sync);  //It is deleted in http_stream_reader() function
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input of database listat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData_Re_Sync_St);
			goto exit;
		}
		cJSON *name_JSON 		= cJSON_GetObjectItem(in_JSON, "files");
		if((name_JSON != NULL) && (cJSON_IsArray(name_JSON)))
		{
			my_JSON  	= cJSON_CreateObject();
			cJSON_AddItemToObject(my_JSON, "DB_NAME", name_JSON);
			memset(payLoadData_Re_Sync_St,0,sizeof(payLoadData_Re_Sync_St));
			cJSON_PrintPreallocated(my_JSON, payLoadData_Re_Sync_St, sizeof(payLoadData_Re_Sync_St), false);
			Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Re_Sync_St, strlen(payLoadData_Re_Sync_St), "GET_SYNC_STATUS");
		}
		cJSON_Delete(in_JSON);

		if (pdTRUE == xQueueReceive(Sync_Status_Que, (void*)bufferRecord_Sync, 5000))
		{
			in_JSON = cJSON_Parse((char*)bufferRecord_Sync);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_Re_Sync_St);
				goto exit;
			}
			cJSON *root = cJSON_CreateObject();
			name_JSON = cJSON_CreateObject();
			cJSON_AddStringToObject(name_JSON, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);  // source info
			my_JSON 		= cJSON_GetObjectItem(in_JSON, "SYNC_STATUS");
			if((my_JSON != NULL) && (cJSON_IsArray(my_JSON)))
			{
				cJSON_AddItemToObject(name_JSON, "SyncStatus", my_JSON);
			}
			cJSON_AddStringToObject(root, "URL", (char*)&ACTOR_SYSTEM_Para.DB_Sync_Status_URL_u8);
			cJSON_AddItemToObject(root, "HTTP_PAYLOAD", name_JSON);
			cJSON_AddStringToObject(root, "API_KEY", (char*)&ACTOR_SYSTEM_Para.APIkey);
			cJSON_AddStringToObject(root, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);
			cJSON_AddNumberToObject(root, "RETRY_PERIOD", ACTOR_SYSTEM_Para.deviceAnnouncePeriod);
			memset(payLoadData_Re_Sync_St,0,sizeof(payLoadData_Re_Sync_St));
			cJSON_PrintPreallocated(root, payLoadData_Re_Sync_St, sizeof(payLoadData_Re_Sync_St), false);
			if(strlen((char*)&ACTOR_SYSTEM_Para.DB_Sync_Status_URL_u8) != 0)
				Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_Re_Sync_St, strlen(payLoadData_Re_Sync_St), "POST");
			else
				Add_Response_msg(" Error! 'RECORD_SYNC_STATUS_URL' is not set. Hence, cannot post the DBSYNC status.",s_Message_Rx, payLoadData_Re_Sync_St);
			cJSON_Delete(root);  // It will free the memory allocated for out_JSON also
		}
	}
exit:
#ifdef ENABLE_PRINT_MSG
		printf("\n exit the sync status task");
#endif
		Sync_Status_Handle = NULL;
		vTaskDelete(Sync_Status_Handle);  // Delete the task
}

static void DB_Sync_Check(void *pvParameters __attribute__((unused)))
{
	time_t now;
	struct tm timeinfo;
	while(1)
	{
		if(DB_Sync_Status_Post == 1)
		{
			do
			{
				time(&now);
				localtime_r(&now, &timeinfo);
			}while(!(((timeinfo.tm_sec) > 25) && ((timeinfo.tm_sec) < 35)));
			Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", "\0", 0 , "DBSYNC_STATUS");
		}
		DB_Sync_Status_Post = 0;
		vTaskDelay((ACTOR_SYSTEM_Para.DB_sync_status_scan_delay_u16*1000)/portTICK_PERIOD_MS);
	}
}

static void Server_Connect(void *pvParameters __attribute__((unused)))
{
	AMessage_st s_Message_Rx_data; // = heap_caps_calloc(sizeof(AMessage_st),1,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	AMessage_st *s_Message_Rx = &s_Message_Rx_data;
    char ping_dest_actor[10] = { "\0" };
	if (s_Message_Rx == NULL)
	{
		printf("Memory allocation failed\n");
		goto exit;
	}

	s_Message_Rx->Dest_ID_a8 = SYSTEM;
	s_Message_Rx->Src_ID_u8 = CONSOLE;
	s_Message_Rx->payload_p8 = (uint8_t*) serverconnect_data_buffer;
	s_Message_Rx->payload_size = 0;
	strcpy((char*)s_Message_Rx->cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx->dest_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx->src_Actor_a8,"CONSOLE");
	printf("<%s.SERVER_CONNECT()\n", s_Message_Rx->dest_Actor_a8);
	cJSON *In_JSON = NULL;
	cJSON *root = NULL;
	cJSON *temp = NULL;
	int8_t ifreceived = pdFALSE;
    char SSID[SSID_SIZE] = { "\0" }, Pass[PASS_SIZE] = { "\0" };
    char eth_keyVal[KEY_VAL_SIZE] = { "\0" };
    char ConnectStatus[CONNECT_STATUS_SIZE] = {0};
    char ping_dns_data[CONNECT_STATUS_SIZE] = { "\0" };
    char ping_gw_data[CONNECT_STATUS_SIZE] = { "\0" };
    char Current_State[100] = {0};
    State state = STATE_CHECK_CREDENTIALS;
    uint8_t retryCount = 0;
    uint8_t retryCount2 = 0;
    uint8_t sent_flag = 0;
    uint8_t blink_state_val = 0;
    char str[MESSAGE_SIZE] = { 0 };
    uint8_t wifi_enabled = 0xFF;
    uint8_t timeout_count_credential_u8 = 0;
    uint8_t timeout_count_credential_2_u8 = 0;
    uint8_t timeout_count_credential_3_u8 = 0;
    uint32_t que_rx_delay = WAIT_FOR_3_SECONDS;
    while (1) {
        if (Server_Conn_Flag == Disable) {
            goto exit;
        }
        ifreceived = pdFALSE;
        memset(serverconnectbuffer, 0, BUFFER_SIZE);
        ifreceived = xQueueReceive(ServerConnect_Que, (void*)serverconnectbuffer, (que_rx_delay / portTICK_PERIOD_MS));
        {
        	switch(state)
        	{
				case 	STATE_CHECK_CREDENTIALS		: strcpy(Current_State, "CHECK CREDENTIALS"); 			break;
				case	STATE_CHECK_BLE_START_RESP	: strcpy(Current_State, "CHECK BLE START RESPONSE"); 	break;
				case	STATE_CHECK_WIFI_ENABLED	: strcpy(Current_State, "CHECK WIFI ENABLED"); 			break;
				case	STATE_CHECK_WIFI_AVAILABLE	: strcpy(Current_State, "CHECK WIFI AVAILABLE"); 		break;
				case	STATE_NETWORK_SCAN			: strcpy(Current_State, "NETWORK SCAN"); 				break;
				case	STATE_WIFI_CONNECT_SSID		: strcpy(Current_State, "WIFI CONNECT SSID"); 			break;
				case	STATE_ETH_CONNECT			: strcpy(Current_State, "ETH CONNECT"); 				break;
				case	STATE_NTP_SYNC_CHECK		: strcpy(Current_State, "NTP CONNECT"); 				break;
				case	STATE_DEVICE_ANNOUNCE		: strcpy(Current_State, "DEVICE ANNOUNCE"); 			break;
				case	STATE_PING_DNS				: strcpy(Current_State, "PING DNS"); 					break;
				case	STATE_PING_GATEWAY			: strcpy(Current_State, "PING GATEWAY"); 				break;
				case	STATE_WAITING_FOR_IHUB_RESPONSE:
													  strcpy(Current_State, "STATE_WAITING_FOR_IHUB_RESPONSE");	break;
				case	STATE_CONNECTED				: strcpy(Current_State, "CONNECTED"); 					break;
				case	STATE_FAILED 				: 					                                    break; //strcpy(Current_State, "FAILED");
				case 	STATE_FAILED_1  			: strcpy(Current_State, "FAILED 1: Network not Available"); 				break;
				case 	STATE_FAILED_2  			: strcpy(Current_State, "FAILED 2: Invalid Json input"); 					break;
				case 	STATE_FAILED_3  			: strcpy(Current_State, "FAILED 3: WIFI connection failed"); 				break;
				case 	STATE_FAILED_4  			: strcpy(Current_State, "FAILED 4: Ethernet connection failed"); 			break;
				case 	STATE_FAILED_5  			: strcpy(Current_State, "FAILED 5: Response from Ethernet actor is not received.");break;
				case 	STATE_FAILED_6  			: strcpy(Current_State, "FAILED 6: DNS ping successful, but device announce failed."); 					break;
				case 	STATE_FAILED_10  			: strcpy(Current_State, "FAILED 10: Ping DNS and gateway successful, but device announce failed.");   break;
				case 	STATE_FAILED_11  			: strcpy(Current_State, "FAILED 11: IHUB Connection FAILED"); break;
				case 	STATE_FAILED_12  			: strcpy(Current_State, "FAILED 12: No IHUB Response"); break;
				default								: strcpy(Current_State, "DEFAULT");						break;
        	}
        	root = cJSON_CreateObject();
        	cJSON_AddStringToObject(root, "SERVER CONNECT STATE", Current_State);
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
        	cJSON_Delete(root);
        	root = NULL;
        	console_send_responce_to_console_xface(s_Message_Rx);

            In_JSON = cJSON_Parse((char*)serverconnectbuffer);
            if( (In_JSON == NULL)&&(ifreceived ==pdTRUE )){//&&(state != STATE_CHECK_CREDENTIALS)){
            	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
                Add_Response_msg(str,s_Message_Rx, payLoadData_Server);
                state = STATE_FAILED_2;
            } else
            {
				if((In_JSON != NULL) && (cJSON_IsObject(In_JSON)))
				{
					temp = cJSON_GetObjectItem(In_JSON, "REINIT");
					if((temp != NULL)&& (cJSON_IsString(temp)))
					{
						if((strcmp(temp->valuestring,"1")==0))
							state = STATE_CHECK_CREDENTIALS;
					}
					else if(state == STATE_CHECK_CREDENTIALS)   // This is added to avoid unwanted que receive in between
					{
						cJSON_Delete(In_JSON);
						In_JSON = NULL;
						continue;
					}
				}
                // Process JSON based on current state
                switch (state) {
                case STATE_CHECK_CREDENTIALS:
               // case STATE_CHECK_BLE_START_RESP:
                		que_rx_delay = WAIT_FOR_30_SECONDS;
                		handle_check_credentials(&state, &timeout_count_credential_u8, &BLE_INIT_Flag, &que_rx_delay, s_Message_Rx,In_JSON, &blink_state_val);
                		break;
                    case STATE_CHECK_BLE_START_RESP:
                        handle_ble_start_resp(&state, In_JSON, &timeout_count_credential_u8, &timeout_count_credential_2_u8, &timeout_count_credential_3_u8, s_Message_Rx);
                        __attribute__((fallthrough));

                    case STATE_CHECK_WIFI_ENABLED:
						handle_wifi_enabled(&state, In_JSON, &retryCount2, SSID, Pass, &wifi_enabled, &sent_flag, s_Message_Rx);
						break;

					case STATE_NETWORK_SCAN:
						que_rx_delay = WAIT_FOR_30_SECONDS;
						handle_network_scan(&state, &timeout_count_credential_2_u8, &BLE_INIT_Flag_2, &que_rx_delay, In_JSON, SSID, Pass, eth_keyVal, &retryCount, s_Message_Rx, &blink_state_val);
						break;

					case STATE_WIFI_CONNECT_SSID:
						que_rx_delay = WAIT_FOR_30_SECONDS;
						handle_wifi_connect_ssid(&state, &timeout_count_credential_3_u8, &BLE_INIT_Flag_3, In_JSON, SSID, ConnectStatus, &retryCount,&que_rx_delay, s_Message_Rx , &blink_state_val);
						break;

					case STATE_ETH_CONNECT:
						handle_eth_connect(&state, In_JSON, SSID, ping_dest_actor, ping_dest_id, &wifi_enabled,&que_rx_delay, s_Message_Rx, &blink_state_val);
						break;
					case STATE_NTP_SYNC_CHECK:
						uint64_t temp_d = (ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32);

						if(Device_Announce_F_delay_Flag_1 == 1)
						{
							device_ann_delay_count++;

							sprintf(str, "Device annouce delay in second is %lld, disconnect count is %d", temp_d, device_ann_delay_count);
							Add_Response_msg(str, s_Message_Rx, payLoadData_Server);

							if(temp_d != 0)
							{
								if(temp_d > 300)	//5minute = 60x5 second
								{
									remaining_time_device_ann = temp_d;

									if(esp_timer_is_active(periodic_timer_5) == false)  // start the timer if it is not yet started
									{

										esp_err_t err2 = esp_timer_start_periodic(periodic_timer_5, (300000000)); // start the timer with the period of 5x60x1000000 sec
										if(err2 != ESP_OK)
											printf("\n\n set Error in start periodic timer. err = %d\n\n", err2);
									}

								}

								vTaskDelay((temp_d*1000)/portTICK_PERIOD_MS);
							}
							else
							{
								temp_d = 5;	//Second
							}
						}
						else
						{
							Device_Announce_F_delay_Flag_1 = 1;
						}

						if(2*temp_d > 100000) // More than one day (86400 second)
						{
							ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32 = 10;		//10 Second
						}
						else
						{
							ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32 = 2*temp_d;
						}

						Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", "\0", 0, "DEVICE_ANN");
						que_rx_delay = WAIT_FOR_1_MINUTE; //portMAX_DELAY;
						state = STATE_DEVICE_ANNOUNCE;
						break;
					case STATE_DEVICE_ANNOUNCE:
						que_rx_delay = WAIT_FOR_1_MINUTE; //WAIT_FOR_30_SECONDS;
						handle_device_announce(&state, In_JSON, SSID, da_message, ping_dest_actor, ping_dest_id,  &blink_state_val, s_Message_Rx);
						que_rx_delay = WAIT_FOR_10_SECONDS;
						break;

					case STATE_PING_DNS:
						handle_ping_dns(&state, In_JSON, SSID, da_message, ping_dns_data, ping_gw_data, ping_dest_id, s_Message_Rx, &blink_state_val);
						break;

					case STATE_PING_GATEWAY:
						handle_ping_gateway(&state, In_JSON, SSID, da_message, ping_dns_data, ping_gw_data, s_Message_Rx,&blink_state_val);
						break;

					case STATE_FAILED:
					case STATE_FAILED_1:
					case STATE_FAILED_2:
					case STATE_FAILED_3:
					case STATE_FAILED_4:
					case STATE_FAILED_5:
					case STATE_FAILED_6:
					case STATE_FAILED_10:
					case STATE_FAILED_11:
					case STATE_FAILED_12:
					case STATE_FAILED_13:
					case STATE_FAILED_PING_GATEWAY:
						handle_failed_state(&state, s_Message_Rx);
						state = STATE_CHECK_CREDENTIALS;
						que_rx_delay = WAIT_FOR_1_MINUTE;
						break;
					case STATE_WAITING_FOR_IHUB_RESPONSE:
						handle_waiting_for_ihub_response(&state, In_JSON, SSID, da_message, ping_dns_data, ping_gw_data, &blink_state_val, s_Message_Rx);
						que_rx_delay = WAIT_FOR_10_SECONDS; //WAIT_FOR_3_SECONDS;
						break;
					case STATE_CONNECTED:
						que_rx_delay = WAIT_FOR_1_MINUTE;
			            Add_Response_msg("Device is already connected", s_Message_Rx, payLoadData_Server);
			            break;
                    default:
                        break;
                }
                cJSON_Delete(In_JSON);
                In_JSON = NULL;
            }
        }
        // Finalize Task
        if (state == STATE_CONNECTED) {
           goto exit;
        }

    }
exit:
	Add_Response_msg("Exit server connect task", s_Message_Rx, payLoadData_Server);
	ServerConnect_Handle = NULL;
	vTaskDelete(ServerConnect_Handle);  // Delete the task
}

// Handle STATE_CHECK_CREDENTIALS and STATE_CHECK_BLE_START_RESP
static void handle_check_credentials(State *state, uint8_t *timeout_count_credential_u8, uint8_t *BLE_INIT_Flag, uint32_t *que_rx_delay, AMessage_st *s_Message_Rx,cJSON *In_JSON, uint8_t *blink_state_val) {
    if (strlen((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL) == 0 || strlen((char*)&ACTOR_SYSTEM_Para.APIkey) == 0) {

    	// Send credential status to BLE actor for advertising data :SK
    	credential_status_u8 = 0;
    	set_to_other_actor("BLE", U_INT8, "CRED_STATUS", &credential_status_u8);

    	 uint8_t Online_status_u8 = 0;
    	 set_to_other_actor("BLE", U_INT8, "ONLINE_STATUS", &Online_status_u8);

    	if (*timeout_count_credential_u8 == 0 && *BLE_INIT_Flag == 0) {
            *BLE_INIT_Flag = 1;
            *timeout_count_credential_u8 = 1;
            *blink_state_val = LED_BLINK_STATE1;
            sendLedState("1-blink", -1, "none");
            Add_Response_msg("Set API key & Device announce credential using BLE.", s_Message_Rx, payLoadData_Server);
            Add_Response_msg("Stack will reset in 5 minutes if no credentials received", s_Message_Rx, payLoadData_Server);
            *que_rx_delay = SER_CONN_BLE_MAX_DELAY;
            char BLE_connect = 1;
            set_to_other_actor("BLE", U_INT8, "CONN_MODE", &BLE_connect);
            Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_START");
            *state = STATE_CHECK_BLE_START_RESP;
        } else {
        	 *que_rx_delay = SER_CONN_BLE_MAX_DELAY;
        	 *state = STATE_CHECK_CREDENTIALS;
        	 *timeout_count_credential_u8 = 0;
        }
        cJSON *root_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(root_JSON, "connectionMessage", "Credentials Missing");
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
        cJSON_Delete(root_JSON);
        console_send_responce_to_console_xface(s_Message_Rx);

        cJSON *rootNew_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR CREDENTIALS");
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
        cJSON_Delete(rootNew_JSON);
        console_send_responce_to_console_xface(s_Message_Rx);
    } else {
    	// Send credential status to BLE actor for advertising data :SK
    	credential_status_u8 = 1;
    	set_to_other_actor("BLE", U_INT8, "CRED_STATUS", &credential_status_u8);

		 uint8_t Online_status_u8 = 0;
		 set_to_other_actor("BLE", U_INT8, "ONLINE_STATUS", &Online_status_u8);

        cJSON *root_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(root_JSON, "connectionMessage", "Credentials Present");
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
        cJSON_Delete(root_JSON);
        console_send_responce_to_console_xface(s_Message_Rx);
        *que_rx_delay = SER_CONN_NORMAL_DELAY;

		cJSON *my_JSON1  	= cJSON_CreateArray();
		if (my_JSON1 == NULL) {
			    printf("\n Error! Failed to create an array. \n");
		        return;
		    }
		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ENABLE_WIFI"));
		cJSON *jsonObject = cJSON_CreateObject();
		if (jsonObject == NULL) {
			    printf("\n Error! Failed to create json object. \n");
		        return;
		    }
		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(jsonObject, payLoadData_Server, sizeof(payLoadData_Server), false);
        cJSON_Delete(jsonObject);
        Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData_Server, strlen(payLoadData_Server), "GET");
        *state = STATE_CHECK_WIFI_ENABLED;
    }
}

// Handle BLE start response
static void handle_ble_start_resp(State *state, cJSON *In_JSON, uint8_t *timeout_count_credential_u8, uint8_t *timeout_count_credential_2_u8, uint8_t *timeout_count_credential_3_u8, AMessage_st *s_Message_Rx) {
    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "RESP");
    if (cJSON_IsString(JSON_Message) && (JSON_Message->valuestring != NULL)) {
        char ble_data_str[50] = {"\0"};
        strcpy(ble_data_str, JSON_Message->valuestring);

        if (strcmp(ble_data_str, "Server is started..") == 0) {
            Add_Response_msg("BLE Server started. 5 minutes timeout reloaded.", s_Message_Rx, payLoadData_Server);
        }
    }
    if(*timeout_count_credential_u8 == 1)
    {
        *timeout_count_credential_u8 = 0;
        *state = STATE_CHECK_CREDENTIALS;
    }
    else if(*timeout_count_credential_2_u8 == 1)
    {
        *timeout_count_credential_2_u8 = 0;
        *state = STATE_FAILED_1;
    }
    else if(*timeout_count_credential_3_u8 == 1)
    {
        *timeout_count_credential_3_u8 = 0;
        *state = STATE_FAILED_3;
    }
}

static void handle_wifi_enabled(State *state, cJSON *In_JSON, uint8_t *retryCount2, char *SSID, char *Pass, uint8_t *wifi_enabled, uint8_t *sent_flag, AMessage_st *s_Message_Rx) {
    cJSON *JSON_WifiENabled = cJSON_GetObjectItemCaseSensitive(In_JSON, "ENABLE_WIFI");
    if (cJSON_IsString(JSON_WifiENabled) && (JSON_WifiENabled->valuestring != NULL)) {
        char WifiENStatus = atoi(JSON_WifiENabled->valuestring);
        *wifi_enabled = WifiENStatus;
        if (*wifi_enabled == 1) {
            cJSON *root_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(root_JSON, "connectionMessage", "Wifi Enabled");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);

            cJSON_Delete(root_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);

            Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "NETWORK_SCAN");
            *sent_flag = 1;
            *state = STATE_NETWORK_SCAN;

            // Reset timeout start since Wi-Fi was enabled successfully
            wifi_enabled_timeout_start = 0;
        } else {
            cJSON *root_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(root_JSON, "connectionMessage", "Wifi Disabled");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(root_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);

    		cJSON *my_JSON1  	= cJSON_CreateArray();
    		if (my_JSON1 == NULL) {
    		        return;
    		    }
    		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
    		cJSON *jsonObject = cJSON_CreateObject();
    		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(jsonObject, payLoadData_Server, sizeof(payLoadData_Server), false);
            cJSON_Delete(jsonObject);
            Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData_Server, strlen(payLoadData_Server), "GET");
            *state = STATE_ETH_CONNECT;
            *wifi_enabled = 0xFF;
            // Reset timeout start since transitioning to Ethernet connect state
            wifi_enabled_timeout_start = 0;
        }
    } else {
               // Start or check timeout for Wi-Fi enabled check failure
        if (wifi_enabled_timeout_start == 0) {
            wifi_enabled_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), wifi_enabled_timeout_start) > WIFI_ENABLED_TIMEOUT_SECONDS) {
                // Timeout of 3 seconds reached, handle as failure
                Add_Response_msg("Timeout waiting for Wi-Fi enabled status is reached.", s_Message_Rx, payLoadData_Server);
                *state = STATE_CHECK_CREDENTIALS;
                wifi_enabled_timeout_start = 0; // Reset timeout start
            }
        }
    }
}


// Handle network scan state
static void handle_network_scan(State *state, uint8_t *timeout_count_credential_2_u8, uint8_t *BLE_INIT_Flag_2, uint32_t *que_rx_delay, cJSON *In_JSON, char *SSID, char *Pass, char *eth_keyVal, uint8_t *retryCount, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
    char ping_dest_actor[10] = {"\0"};
    memset(SSID,0,SSID_SIZE);
    memset(Pass,0,PASS_SIZE);

    // Extract SSID from input JSON
    cJSON *JSON_SSID = cJSON_GetObjectItemCaseSensitive(In_JSON, "SSID");
    if (cJSON_IsString(JSON_SSID) && (JSON_SSID->valuestring != NULL)) {
        strcpy(SSID, JSON_SSID->valuestring);
    }

    // Extract PASSWORD from input JSON
    cJSON *JSON_PASS = cJSON_GetObjectItemCaseSensitive(In_JSON, "PASSWORD");
    if (cJSON_IsString(JSON_PASS) && (JSON_PASS->valuestring != NULL)) {
        strcpy(Pass, JSON_PASS->valuestring);
    }

    // If SSID is available, attempt Wi-Fi connection
    if (strlen(SSID) != 0) {
        // Prepare JSON for Wi-Fi connection
        cJSON *wifi_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(wifi_JSON, "SSID", SSID);
        cJSON_AddStringToObject(wifi_JSON, "PASSWORD", Pass);       
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(wifi_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
       // Send command to connect to Wi-Fi
        Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Server, strlen(payLoadData_Server), "CONNECT_SSID");
        *state = STATE_WIFI_CONNECT_SSID;
        strcpy(ping_dest_actor, "WIFI");
        ping_dest_id = WIFI;
        *retryCount = 0;
        cJSON_Delete(wifi_JSON);
    } else {
        // Clear SSID and PASSWORD
        memset(SSID, 0, strlen(SSID));
        memset(Pass, 0, strlen(Pass));

        // Extract ETHERNET status from input JSON
        cJSON *JSON_ETH = cJSON_GetObjectItemCaseSensitive(In_JSON, "ETHERNET");
        if (cJSON_IsString(JSON_ETH) && (JSON_ETH->valuestring != NULL)) {
            strcpy(eth_keyVal, JSON_ETH->valuestring);
        }

        // If Ethernet is available, attempt Ethernet connection
        if (strcmp(eth_keyVal, "AVAILABLE") == 0) {
        	   Send_CMD_To_Other_Actor(ETH, "ETH", "", strlen(""), "CONNECT");
            // Prepare JSON for Ethernet connection
       		cJSON *my_JSON1  	= cJSON_CreateArray();
       		if (my_JSON1 == NULL) {
       		        return;
       		    }
       		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
       		cJSON *jsonObject = cJSON_CreateObject();
       		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
   			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
   			cJSON_PrintPreallocated(jsonObject, payLoadData_Server, sizeof(payLoadData_Server), false);
            cJSON_Delete(jsonObject);
            Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData_Server, strlen(payLoadData_Server), "GET");

            *state = STATE_ETH_CONNECT;
            ping_dest_id = ETH;
            strcpy(ping_dest_actor, "ETH");
        }
        // Check for no network
        cJSON *JSON_Network = cJSON_GetObjectItemCaseSensitive(In_JSON, "NETWORK");
        if (cJSON_IsString(JSON_Network) && (JSON_Network->valuestring != NULL))
        {
            if(strcmp(JSON_Network->valuestring, "NOT AVAILABLE") == 0)
            {
        		if (*timeout_count_credential_2_u8 == 0 )
        		{
					*BLE_INIT_Flag_2 = 1;
					*timeout_count_credential_2_u8 = 1;
					*blink_state_val = LED_BLINK_STATE2;
					sendLedState("2-blink", -1, "none");
					Add_Response_msg("Set network credential using BLE.", s_Message_Rx, payLoadData_Server);
					Add_Response_msg("Stack will reset in 5 minutes if no credentials received", s_Message_Rx, payLoadData_Server);
					*que_rx_delay = SER_CONN_BLE_MAX_DELAY;
					char BLE_connect = 1;
					set_to_other_actor("BLE", U_INT8, "CONN_MODE", &BLE_connect);
					Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_START");
					*state = STATE_CHECK_BLE_START_RESP;
        		}
            }
        }
        else {
                // Default case: Handle timeout or invalid message
                if (timeout_start == 0) {
                    timeout_start = time(NULL); // Start the timeout
                } else {
                    if (difftime(time(NULL), timeout_start) > 3) {
                        // Timeout of 3 seconds reached, reset state machine
                        // Network not available, update response and LED state
						//Add_Response_msg("NETWORK NOT AVAILABLE", s_Message_Rx);
                    	if (*timeout_count_credential_2_u8 == 0 ) {
							*BLE_INIT_Flag_2 = 1;
							*timeout_count_credential_2_u8 = 1;
							*blink_state_val = LED_BLINK_STATE2;
							sendLedState("2-blink", -1, "none");
							Add_Response_msg("Set network credential using BLE/ETH/TCP/UDP/WIFIAP/Webserver", s_Message_Rx, payLoadData_Server);
							Add_Response_msg("Stack will reset in 5 minutes if no credentials received", s_Message_Rx, payLoadData_Server);
							*que_rx_delay = SER_CONN_BLE_MAX_DELAY;
							char BLE_connect = 1;
							set_to_other_actor("BLE", U_INT8, "CONN_MODE", &BLE_connect);
							Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_START");
							*state = STATE_CHECK_BLE_START_RESP;
						}
						// Prepare JSON response for waiting state
						cJSON *rootNew_JSON = cJSON_CreateObject();
						cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR WIFI");
						memset(payLoadData_Server,0,sizeof(payLoadData_Server));
						cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
						cJSON_Delete(rootNew_JSON);
						console_send_responce_to_console_xface(s_Message_Rx);
						timeout_start = 0; // Reset timeout start
                    }
                }
            }
        }
    }


// Handle WiFi connect SSID state
static void handle_wifi_connect_ssid(State *state, uint8_t *timeout_count_credential_3_u8, uint8_t *BLE_INIT_Flag_3, cJSON *In_JSON, char *SSID, char *ConnectStatus, uint8_t *retryCount,uint32_t *que_rx_delay, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
	memset(ConnectStatus, 0, CONNECT_STATUS_SIZE);
	cJSON *JSON_CONNECTION_STATUS = cJSON_GetObjectItem(In_JSON, "WIFI_CONNECTION_STATUS");
    if (cJSON_IsString(JSON_CONNECTION_STATUS) && (JSON_CONNECTION_STATUS->valuestring != NULL)) {
        strcpy(ConnectStatus, JSON_CONNECTION_STATUS->valuestring);
    }
    if (strcmp(ConnectStatus, "CONNECTED") == 0) {
        cJSON *root_JSON = cJSON_CreateObject();
        cJSON_AddStringToObject(root_JSON, "connectionMessage", "WIFI Connected");
		memset(payLoadData_Server,0,sizeof(payLoadData_Server));
		cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
        cJSON_Delete(root_JSON);
        console_send_responce_to_console_xface(s_Message_Rx);
        Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "CONNECT");
        *state = STATE_NTP_SYNC_CHECK;
        sendLedState("NET CONNECTED",-1, "none");
        wifi_connect_timeout_start = 0;
        *que_rx_delay = WAIT_FOR_1_SECOND; //SER_CONN_NORMAL_DELAY; //;   //Speed-up

    } else if (strcmp(ConnectStatus, "DISCONNECTED") == 0){
        {
            Add_Response_msg("WIFI connection failed", s_Message_Rx, payLoadData_Server);
            if (*timeout_count_credential_3_u8 == 0 ) {
			*BLE_INIT_Flag_3 = 1;
			*timeout_count_credential_3_u8 = 1;
			*blink_state_val = LED_BLINK_STATE3;
			sendLedState("3-blink", -1, "none");
			Add_Response_msg("Change WIFI credential using BLE.", s_Message_Rx, payLoadData_Server);
			Add_Response_msg("Stack will reset in 5 minutes if no credentials received", s_Message_Rx, payLoadData_Server);
			*que_rx_delay = SER_CONN_BLE_MAX_DELAY;
			char BLE_connect = 1;
			set_to_other_actor("BLE", U_INT8, "CONN_MODE", &BLE_connect);
			Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_START");
			*state = STATE_CHECK_BLE_START_RESP;
			}
            cJSON *rootNew_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "BAD WIFI PASSWORD");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(rootNew_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);
            wifi_connect_timeout_start = 0;
        }
    }
    else {

		// Check if timeout has started
		if (wifi_connect_timeout_start == 0) {
			wifi_connect_timeout_start = time(NULL); // Start the timeout
		} else {
			// Check if the timeout duration has been exceeded
			if (difftime(time(NULL), wifi_connect_timeout_start) > WIFI_CONNECT_TIMEOUT_SECONDS) {
				// Timeout of 3 seconds reached, handle as failure
				wifi_connect_timeout_start = 0; // Reset timeout start
				// JSON object is not a string or is NULL
						Add_Response_msg("Response from WIFI actor is not received.", s_Message_Rx, payLoadData_Server);
						*state = STATE_FAILED_3;
			}
		}
        }
}

// Handle Ethernet connect state
static void handle_eth_connect(State *state, cJSON *In_JSON, char *SSID, char *ping_dest_actor, char ping_dest_id, uint8_t *wifi_enabled, uint32_t *que_rx_delay, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
    // Extract Ethernet status from input JSON
    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "ETH_STATUS");
    if (cJSON_IsString(JSON_Message) && (JSON_Message != NULL)) {
        if (strcmp(JSON_Message->valuestring, "1") == 0) {
            // Ethernet connected successfully
            cJSON *root_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(root_JSON, "connectionMessage", "ETH Connected");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(root_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);
            Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "CONNECT");
            *state = STATE_NTP_SYNC_CHECK;
            sendLedState("NET CONNECTED",-1, "none");
            *que_rx_delay = WAIT_FOR_1_SECOND;  //Speed-up
            // Reset timeout start since connection was successful
            eth_timeout_start = 0;
        } else {
            // Ethernet connection failed
            *blink_state_val = LED_BLINK_STATE5;
            Add_Response_msg("Ethernet connection failed", s_Message_Rx, payLoadData_Server);

            // Prepare JSON response indicating waiting for Ethernet connection
            cJSON *rootNew_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR ETH");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(rootNew_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);

            // Check if timeout has started
            if (eth_timeout_start == 0) {
                eth_timeout_start = time(NULL); // Start the timeout
            } else {
                // Check if the timeout duration has been exceeded
                if (difftime(time(NULL), eth_timeout_start) > ETHERNET_TIMEOUT_SECONDS) {
                    // Timeout of 3 seconds reached, handle as failure
                    eth_timeout_start = 0; // Reset timeout start
                    *state = STATE_FAILED_4;

                    // Additional action if Wi-Fi is not enabled
                    if (*wifi_enabled == 0) {
                        Add_Response_msg("Wait 10 Minutes for Ethernet connection", s_Message_Rx, payLoadData_Server);
                        vTaskDelay(WAIT_10_MINUTES);
                    }
                }
            }
        }
    } else {
        // Check if timeout has started
                    if (eth_timeout_start == 0) {
                        eth_timeout_start = time(NULL); // Start the timeout
                    } else {
                        // Check if the timeout duration has been exceeded
                        if (difftime(time(NULL), eth_timeout_start) > ETHERNET_TIMEOUT_SECONDS) {
                            // Timeout of 3 seconds reached, handle as failure
                            eth_timeout_start = 0; // Reset timeout start
                            // JSON object is not a string or is NULL
                                    Add_Response_msg("Response from Ethernet actor is not received.", s_Message_Rx, payLoadData_Server);
                                    *state = STATE_FAILED_5;
                        }
                    }
			}
}

// Handle device announce state
static void handle_device_announce(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dest_actor, char ping_dest_id, uint8_t *blink_state_val, AMessage_st *s_Message_Rx) {
    char IhubCreflag = 0;

    // Extract HTTP status from input JSON
    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "HTTP STATUS");
    if (cJSON_IsNumber(JSON_Message) && (JSON_Message != NULL)) { // HTTP STATUS key is present
        if (JSON_Message->valueint != 200) {

#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	if(JSON_Message->valueint == 408)
        	{
        		sendLedState("5-blink", -1, "Server");
        	}
        	else if((JSON_Message->valueint == 401) || (JSON_Message->valueint == 405))
        	{
        		sendLedState("6-blink", -1, "Server");
        	}
        	else
        	{
        		sendLedState("7-blink", -1, "Server");
        	}
#endif
					switch(JSON_Message->valueint)
					{
						case 400: strcpy(deviceAnnounceStatus, "Connection Refused (Bad Request)");  break;
						case 401: strcpy(deviceAnnounceStatus, "Connection Refused (Wrong API key)");  break;
						case 404: strcpy(deviceAnnounceStatus, "Connection Refused (Server not found)");  break;
						case 405: strcpy(deviceAnnounceStatus, "Connection Refused (Invalid URL.)");  break;
						case 408: strcpy(deviceAnnounceStatus, "Server timeout");  break;
						default:  sprintf(deviceAnnounceStatus, "Connection Refused (Value: %d)", JSON_Message->valueint);	break;
					}
                    struct timeval currentTime;
                    _gettimeofday_r(NULL, &currentTime, NULL);
                    uint64_t current_epos_sec = (uint64_t)(currentTime.tv_sec * 1000L) + (uint64_t)(currentTime.tv_usec / 1000L);
                    DeviceAnnoune_status_time_u64 = current_epos_sec;

                    // Update response and LED state
                    Add_Response_msg("Device Announce FAILED", s_Message_Rx, payLoadData_Server);
                    *blink_state_val = LED_BLINK_STATE6;
#if defined(B480) || defined(B553)
                    sendLedState("6-blink",-1, "Server");
#endif

                    // Prepare JSON response for waiting state
                    cJSON *rootNew_JSON = cJSON_CreateObject();
                    cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR SERVER");
					memset(payLoadData_Server,0,sizeof(payLoadData_Server));
					cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);

                    cJSON_Delete(rootNew_JSON);
                    console_send_responce_to_console_xface(s_Message_Rx);

                    if (JSON_Message->valueint == 401) {
                        cJSON *rootNew_JSON1 = cJSON_CreateObject();
                        cJSON_AddStringToObject(rootNew_JSON1, "connectionMessage", "deviceAnnounce FAILED: bad API key");
						memset(payLoadData_Server,0,sizeof(payLoadData_Server));
						cJSON_PrintPreallocated(rootNew_JSON1, payLoadData_Server, sizeof(payLoadData_Server), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);

                        cJSON_Delete(rootNew_JSON1);
                        console_send_responce_to_console_xface(s_Message_Rx);
                    }

                    // Send ping command to check server connectivity
                    cJSON *responseObject = cJSON_CreateObject();
                    cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");
                    cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);
					memset(payLoadData_Server,0,sizeof(payLoadData_Server));
					cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);
					cJSON_Delete(responseObject);
                    if (ping_dest_id == WIFI) {
                        Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Server, strlen(payLoadData_Server), "PING");
                    } else {
                        Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData_Server, strlen(payLoadData_Server), "PING");
                    }
                    sprintf(da_message, "Device Announce Failed. Error code is %d",JSON_Message->valueint);
                    *state = STATE_PING_DNS;
                    // Reset timeout start
                    device_announce_timeout_start = 0;
        } else {
            // Device announce successful
            strcpy(deviceAnnounceStatus, "Connected");

            // Prepare and send success response
            cJSON *rootNew_JSON1 = cJSON_CreateObject();
            cJSON_AddStringToObject(rootNew_JSON1, "connectionMessage", "deviceAnnounce OK");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(rootNew_JSON1, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);

            cJSON_Delete(rootNew_JSON1);
            console_send_responce_to_console_xface(s_Message_Rx);

            // Record the current time for success status
            struct timeval currentTime;
            _gettimeofday_r(NULL, &currentTime, NULL);
            uint64_t current_epos_sec = (uint64_t)(currentTime.tv_sec * 1000L) + (uint64_t)(currentTime.tv_usec / 1000L);
            DeviceAnnoune_status_time_u64 = current_epos_sec;

            // Process device announce response
			memset(da_message,0, MAX_JSON_PAYLOAD_BYTES/2);
			cJSON_PrintPreallocated(In_JSON,  da_message, MAX_JSON_PAYLOAD_BYTES/2, false);
            if (strlen(da_message) == 0) {
                Add_Response_msg("Device Announce response is NULL", s_Message_Rx, payLoadData_Server);
            } else
            {
                cJSON *Message = cJSON_GetObjectItem(In_JSON, "MESSAGE");
                if (Message == NULL) {
                    Add_Response_msg("No response received from server", s_Message_Rx, payLoadData_Server);
                } else {
                    cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(Message, "methods");
                    if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray))) {
                        int arraySize = cJSON_GetArraySize(methodsArray);
                        *blink_state_val = LED_BLINK_STATE7;
                        // Loop through each method in the array and process it
                        for (int i = 0; i < arraySize; i++) {
                            cJSON *element = cJSON_GetArrayItem(methodsArray, i);
                            if (cJSON_IsString(element) && (element->valuestring != NULL)) {
//                                char line[2048] = {0};
                                strcpy(line, element->valuestring);
                                if (strncmp(line, "<IHUB.SET({\"HOSTNAME\":", 22) == 0) {
                                    if (strlen(line) > 30) {
                                        IhubCreflag++;
                                    }
                                }
                                if (strncmp(line, "<IHUB.SET({\"PRIMARY_KEY\":", 25) == 0) {
                                    if (strlen(line) > 30) {
                                        IhubCreflag++;
                                    }
                                }
                            }
                        }

                        // Prepare JSON response based on IHUB credentials status
                        cJSON *root_JSON = cJSON_CreateObject();
                        if (IhubCreflag == 2) {
                            cJSON_AddStringToObject(root_JSON, "connectionMessage", "IHUB Credentials Populated");
                        } else if ((IhubCreflag == 1) || (IhubCreflag == 0)) {
                            cJSON_AddStringToObject(root_JSON, "connectionMessage", "IHUB Credentials Missing/Partial");
                            sendLedState("5-blink", -1, "iHUB");
    						*state = STATE_FAILED_13;
                            device_announce_timeout_start = 0;
                        }
                        IhubCreflag = 0;

                        // Send IHUB credentials status response
						memset(payLoadData_Server,0,sizeof(payLoadData_Server));
						cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
                        cJSON_Delete(root_JSON);
                        console_send_responce_to_console_xface(s_Message_Rx);
                        if(*state == STATE_FAILED_13)
                        {
                        	return;
                        }

                        // Prepare JSON response for waiting state
                        cJSON *rootNew_JSON = cJSON_CreateObject();
                        cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR IHUB");
						memset(payLoadData_Server,0,sizeof(payLoadData_Server));
						cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
                        cJSON_Delete(rootNew_JSON);
                        console_send_responce_to_console_xface(s_Message_Rx);
                        // Reset timeout start since connection was successful
						*state = STATE_WAITING_FOR_IHUB_RESPONSE;
                        device_announce_timeout_start = 0;
                    }
                }
            }
        }
    } else { // HTTP STATUS key is not present
        // Start or check timeout for device announce failure
        if (device_announce_timeout_start == 0) {
            device_announce_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), device_announce_timeout_start) > DEVICE_ANNOUNCE_TIMEOUT_SECONDS) {
                // Timeout reached, handle as failure
                strcpy(da_message, "Device Announce Failed. Unknown error.");
                Add_Response_msg("Device Announce FAILED", s_Message_Rx, payLoadData_Server);
                *blink_state_val = LED_BLINK_STATE6;
                // Prepare JSON response for failed device announce
                cJSON *rootNew_JSON1 = cJSON_CreateObject();
                cJSON_AddStringToObject(rootNew_JSON1, "connectionMessage", "deviceAnnounce FAILED:URL No Response");
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(rootNew_JSON1, payLoadData_Server, sizeof(payLoadData_Server), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
                cJSON_Delete(rootNew_JSON1);
                console_send_responce_to_console_xface(s_Message_Rx);
#if defined(B480) || defined(B553)
                sendLedState("6-blink",-1, "none");
#else
                sendLedState("7-blink", -1, "Server");
#endif
                cJSON *rootNew_JSON = cJSON_CreateObject();
                cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR SERVER");
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
                cJSON_Delete(rootNew_JSON);
                console_send_responce_to_console_xface(s_Message_Rx);

                // Send ping command to check server connectivity
                cJSON *responseObject = cJSON_CreateObject();
                cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");
                cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);
				cJSON_Delete(responseObject);
                if (ping_dest_id == WIFI) {
                    Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Server, strlen(payLoadData_Server), "PING");
                } else {
                    Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData_Server, strlen(payLoadData_Server), "PING");
                }
                *state = STATE_PING_DNS;
                // Reset timeout start
                device_announce_timeout_start = 0;
            }
        }
    }
}


// Handle ping DNS state
static void handle_ping_dns(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, char ping_dest_id, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "RESP");
    if (cJSON_IsString(JSON_Message) && (JSON_Message->valuestring != NULL)) {
        strcpy(ping_dns_data, JSON_Message->valuestring);
        if (strcmp(ping_dns_data, "Ping to the server is successful.") == 0) {
            // Ping DNS successful
            cJSON *root_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping DNS Successful");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(root_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);
			*state = STATE_FAILED_6; //device announce failed        //STATE_WAITING_FOR_IHUB_RESPONSE;

            // Reset timeout start since connection was successful
            ping_dns_timeout_start = 0;
        } else if(strcmp(ping_dns_data, "Ping to the server is failed.") == 0) {
			// Timeout reached, handle as failure
			*blink_state_val = LED_BLINK_STATE4;
			sendLedState("4-blink", -1, "none");
			cJSON *root_JSON = cJSON_CreateObject();
			cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping DNS Failed");

			InternetDisconnectCount++;
			char str2[100] = {0};
			sprintf(str2,"INTERNET DISCONNECT COUNT = %d", InternetDisconnectCount);
			Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str2, strlen(str2), "SAVE_WIFI_AUDIT_LOG");

			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
			cJSON_Delete(root_JSON);
			console_send_responce_to_console_xface(s_Message_Rx);

			cJSON *rootNew_JSON = cJSON_CreateObject();
			cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR INTERNET");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
			cJSON_Delete(rootNew_JSON);
			console_send_responce_to_console_xface(s_Message_Rx);

			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");
			cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);
			cJSON_Delete(responseObject);
			if (ping_dest_id == WIFI) {
				Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Server, strlen(payLoadData_Server), "PING");
			} else {
				Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData_Server, strlen(payLoadData_Server), "PING");
			}
			*state = STATE_PING_GATEWAY;
			ping_dns_timeout_start = 0; // Reset timeout start
        }
    } else {
        // Start or check timeout for ping DNS failure
        if (ping_dns_timeout_start == 0) {
            ping_dns_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), ping_dns_timeout_start) > PING_DNS_TIMEOUT_SECONDS) {
                // Timeout reached, handle as failure
				Add_Response_msg("PING DNS response is not received.", s_Message_Rx, payLoadData_Server);
				cJSON *responseObject = cJSON_CreateObject();
				   // Timeout reached, handle as failure
				*blink_state_val = LED_BLINK_STATE4;
				sendLedState("4-blink", -1, "none");
				cJSON *root_JSON = cJSON_CreateObject();
				cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping DNS Failed");
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
				cJSON_Delete(root_JSON);
				console_send_responce_to_console_xface(s_Message_Rx);

				cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");
				cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);
				if (ping_dest_id == WIFI) {
					Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Server, strlen(payLoadData_Server), "PING");
				} else {
					Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData_Server, strlen(payLoadData_Server), "PING");
				}
				*state = STATE_PING_GATEWAY;
				cJSON_Delete(responseObject);
                ping_dns_timeout_start = 0; // Reset timeout start
            }
        }
    }
}

// Handle ping gateway state
static void handle_ping_gateway(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, AMessage_st *s_Message_Rx,uint8_t *blink_state_val) {
    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "RESP");
    if (cJSON_IsString(JSON_Message) && (JSON_Message->valuestring != NULL)) {
        strcpy(ping_gw_data, JSON_Message->valuestring);
        if (strcmp(ping_gw_data, "Ping to the server is successful.") == 0) {
            // Ping gateway successful

            cJSON *root_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping Gateway Successful");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
            cJSON_Delete(root_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);
            *state = STATE_FAILED_10;   // DA failed

            // Reset timeout start since connection was successful
            ping_gateway_timeout_start = 0;
        } else if(strcmp(ping_dns_data, "Ping to the server is failed.") == 0) {
			// Timeout reached, handle as failure
			  // Ping gateway failed
			cJSON *root_JSON = cJSON_CreateObject();
			cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping Gateway Failed");
			*blink_state_val = LED_BLINK_STATE4;
			sendLedState("4-blink", -1, "none");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));
			cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
			cJSON_Delete(root_JSON);
			console_send_responce_to_console_xface(s_Message_Rx);
			Add_Response_msg("Ping Gateway to the server is failed.", s_Message_Rx, payLoadData_Server);
			*state = STATE_FAILED_PING_GATEWAY;
			ping_gateway_timeout_start = 0; // Reset timeout start
        }
    } else {
        // Start or check timeout for ping gateway failure
        if (ping_gateway_timeout_start == 0) {
            ping_gateway_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), ping_gateway_timeout_start) > PING_GATEWAY_TIMEOUT_SECONDS) {
                // Ping gateway failed
				cJSON *root_JSON = cJSON_CreateObject();
				cJSON_AddStringToObject(root_JSON, "connectionMessage", "Ping Gateway Failed");
				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(root_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Server);
				cJSON_Delete(root_JSON);
				console_send_responce_to_console_xface(s_Message_Rx);
				Add_Response_msg("PING gateway response is not received. Ping Gateway to the server is failed.", s_Message_Rx, payLoadData_Server);
                ping_gateway_timeout_start = 0; // Reset timeout start
                *state = STATE_FAILED_PING_GATEWAY;
            }
        }
    }
}

static void handle_failed_state(State *state, AMessage_st* s_Message_Rx)
{
	Add_Response_msg("Server Connect flow failed. Stack will reset after 30 seconds ", s_Message_Rx, payLoadData_Server);
	vTaskDelay(WAIT_FOR_30_SECONDS / portTICK_PERIOD_MS);  //30 seconds delay
	Reset_Stack(s_Message_Rx, payLoadData_Server);
}

static void initiate_ServerConnect(AMessage_st* s_Message_Rx)
{
	if(ServerConnect_Que == NULL)
		ServerConnect_Que = xQueueCreateStatic(5, 2000, Serv_conn_pucQueueStorage, &Serv_conn_pxQueueBuffer);
	if((ServerConnect_Handle == NULL)&&(MonSrvConn_Handle == NULL))
	{
		ServerConnect_Handle = xTaskCreateStaticPinnedToCore(
						Server_Connect,                 // Task function
						"Server_Connect",            // Task name
						SRV_CONN_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						SRV_CONN_TASK_PRIORITY,                       // Task priority
						xServ_conn_TaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xSrvConnTaskBuffer,             // Pointer to task control block
						0
	   );
		if (ServerConnect_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create SRV CONNECT task\n");
#endif
			}
	}
	else
	{
		if(MonSrvConn_Handle != NULL)
		{
			for(int i = 0; i<5; i++)
			{
				if(MonSrvConn_Handle == NULL)
				{
					break;
				}
				else
				{
					vTaskDelay(1000/ portTICK_PERIOD_MS);
				}
			}
			if((MonSrvConn_Que!=NULL) && (MonSrvConn_Handle != NULL))
			{
				Add_Response_msg("Server connect method is received. Deleting Server monitor task", s_Message_Rx, payLoadData_Server);
				cJSON *responseObject = cJSON_CreateObject();
				cJSON_AddStringToObject(responseObject, "DELETE_MON","1");

				memset(payLoadData_Server,0,sizeof(payLoadData_Server));
				cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);
				xQueueSend(MonSrvConn_Que, payLoadData_Server, QUE_DELAY);
				cJSON_Delete(responseObject);
			}
			else if(ServerConnect_Handle == NULL)//case: failed to send DELETE_MON command. monitor exit code may take time to delete the task
				//In this case, ServerConnect_Handle = NULL, MonSrvConn_Que = NULL, MonSrvConn_Handle != NULL, and server connect command is received due to stack reset.
			{
				ServerConnect_Handle = xTaskCreateStaticPinnedToCore(
						Server_Connect,                 // Task function
								"Server_Connect",            // Task name
								SRV_CONN_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								SRV_CONN_TASK_PRIORITY,                       // Task priority
								xServ_conn_TaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xSrvConnTaskBuffer,             // Pointer to task control block
								0
			   );
				if (ServerConnect_Handle == NULL) {
		#ifdef ENABLE_PRINT_MSG
					printf("Failed to create SRV CONNECT task\n");
		#endif
			}

		}			
		}
		else if(ServerConnect_Handle != NULL)
		{
			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "REINIT","1");
			memset(payLoadData_Server,0,sizeof(payLoadData_Server));					
			cJSON_PrintPreallocated(responseObject, payLoadData_Server, sizeof(payLoadData_Server), false);	
			if((ServerConnect_Que != NULL) && (ServerConnect_Handle != NULL))
				xQueueSend(ServerConnect_Que, payLoadData_Server, QUE_DELAY);
			cJSON_Delete(responseObject);
			Add_Response_msg("Server Connect re-create start", s_Message_Rx, payLoadData_Server);
		}
	}
}

static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {

	AMessage_st s_Message_Tx;
	uint8_t out_val[384]  	=  {0}; //(uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char payload[512] = {0};
	cJSON   *my_JSON  	= cJSON_CreateObject();
	switch(data_type)
	{
		case U_INT8  :	sprintf((char*)out_val,	"%d",	*(uint8_t *) 	value);	break;
		case U_INT16 :	sprintf((char*)out_val,	"%d",	*(uint16_t *) 	value);	break;
		case INT	 :	sprintf((char*)out_val,	"%d", 	*(int *) 		value);	break;
//		case FLOAT   :  sprintf((char*)out_val,	"%f", 	(float) 	value); break;
		case STRING  :	sprintf((char*)out_val,	"%s",	(char*) 	value);	break;
		default 	 : 	break;
	}
	cJSON_AddStringToObject(my_JSON, parameter, (char*)out_val);
	//free(out_val);
	
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(my_JSON, payload, sizeof(payload), false);
	cJSON_Delete(my_JSON);
	uint8_t *newpointer = (uint8_t*) heap_caps_calloc((strlen((char*) payload) + 1), sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payload);
	s_Message_Tx.payload_size = strlen((char*)payload);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
	s_Message_Tx.Src_ID_u8 = THIS_ACTOR_ID;
	console_ActorWriteToConsole_xface( &s_Message_Tx);
}//	set_to_other_actor

uint8_t create_and_sendJson(char * SSID,char * da_message, char* ping_dns_data, char* ping_gw_data)
{
	uint8_t ret_val = 1;
	cJSON *dbname = cJSON_CreateObject();
	cJSON *srv_rec_upsert = cJSON_CreateObject();
	cJSON_AddStringToObject(dbname, "DBNAME","CONNECTION_RECORD");
	struct timeval TImeStamp;
	uint64_t TImeStampmills =0;
	 char timestamp_str[50];
	gettimeofday(&TImeStamp, NULL); // Get current time
	TImeStampmills = (uint64_t)(TImeStamp.tv_sec * 1000L) + (uint64_t)(TImeStamp.tv_usec / 1000L); // Convert to milliseconds
	snprintf(timestamp_str, sizeof(timestamp_str), "%llu", TImeStampmills);
	cJSON_AddStringToObject(srv_rec_upsert, "TIME_STAMP", timestamp_str);
	cJSON_AddStringToObject(srv_rec_upsert, "SSID_NAME", SSID);
	cJSON_AddStringToObject(srv_rec_upsert, "NETWORK_AP_MAC", (char*)&ACTOR_SYSTEM_Para.DeviceId);
	cJSON_AddStringToObject(srv_rec_upsert, "HTTP_RESPONSE", da_message);
	cJSON_AddStringToObject(srv_rec_upsert, "PING_DNS_RESPONSE", ping_dns_data);
	cJSON_AddStringToObject(srv_rec_upsert, "PING_GATEWAY_RESPONSE", ping_gw_data);
	 cJSON_AddItemToObject(dbname, "SRV_REC_UPSERT", srv_rec_upsert);
	memset(payLoadData_Server,0,sizeof(payLoadData_Server));
	cJSON_PrintPreallocated(dbname, payLoadData_Server, sizeof(payLoadData_Server), false);

	 if (strlen(payLoadData_Server) == 0){
		 ret_val = 1;
	 }
	 else{
		 Send_CMD_To_Other_Actor(SQL,"SQL", payLoadData_Server, strlen(payLoadData_Server), "SAVE_SRV_CONN");
		 ret_val = 0;
	 }
	 cJSON_Delete(dbname);
	 return ret_val;
}

static void Reset_Stack(AMessage_st* s_Message_Rx, char *payLoadData_New)
{
	uint32_t reset_time_1 = 0;
	cJSON *in_JSON 		= NULL;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL)
	{
		reset_time_1 = 0;
	}
	else
	{
		// Obtain the COMMAND and RESPONSE keys
		cJSON *resetTime = cJSON_GetObjectItem(in_JSON, "RESET_TIME");

		if((resetTime != NULL) && (cJSON_IsNumber(resetTime)))
		{
			reset_time_1 = resetTime->valueint;		// In minute
			if(reset_time_1 <= 60)
			{
				reset_time_1 = reset_time_1*60*1000;	// In milisecond
			}
			else
			{
				reset_time_1 = 60*60*1000;
			}
		}
		else
		{
			reset_time_1 = 0;
		}


	}
	cJSON_Delete(in_JSON);

	  sendLedState("OFF",-1, "none");
	  Send_CMD_To_Other_Actor(HTTP,"HTTP", "\0", 0, "DEINIT");
      Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "DISCONNECT");  // Disconnect WIFI at first
      vTaskDelay(1000 / portTICK_PERIOD_MS);
	  Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "DEINIT");
	  Send_CMD_To_Other_Actor(IHUB,"IHUB", "\0", 0, "DEINIT");
	  Add_Response_msg("HTTP, WIFI, AND IHUB actors are de-initialized.", s_Message_Rx, payLoadData_New);

	  if(reset_time_1 == 0)
	  {
		  vTaskDelay(1000 / portTICK_PERIOD_MS);
	  }
	  else
	  {
		  vTaskDelay(reset_time_1 / portTICK_PERIOD_MS);
		  Restart_ESP_Xface(1);
	  }

      Send_CMD_To_Other_Actor(HTTP,"HTTP", "\0", 0, "INIT");
      vTaskDelay(500 / portTICK_PERIOD_MS);
	  Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "INIT");
	  vTaskDelay(500 / portTICK_PERIOD_MS);
	  Send_CMD_To_Other_Actor(IHUB,"IHUB", "\0", 0, "INIT");
	  Add_Response_msg("HTTP, WIFI, AND IHUB actors are re-initialized.", s_Message_Rx, payLoadData_New);
	  vTaskDelay(2000 / portTICK_PERIOD_MS);
	  strcpy(deviceAnnounceStatus,"Connection Yet To Start");
	  Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", "\0", 0, "SERVER_CONNECT");
}

static void InitMonSrvConn(void)
{

		if(MonSrvConn_Que == NULL)
			MonSrvConn_Que = xQueueCreateStatic(5, 512 , Serv_mon_pucQueueStorage, &Serv_mon_pxQueueBuffer);  //4*1024

		if (MonSrvConn_Que == NULL){
#ifdef ENABLE_PRINT_MSG
			printf("Unable to create server connect task queue \n");
#endif
		}
		if(MonSrvConn_Handle == NULL)
		{
			MonSrvConn_Handle = xTaskCreateStaticPinnedToCore(
							MonSrvConn,                 // Task function
							"MonSrvConn",            // Task name
							MON_SERVER_TASK_STACK_DEPTH,        // Stack size in words
							NULL,                    // Task parameters (not used here)
							MON_SERVER_TASK_PRIORITY,                       // Task priority
							xServ_mon_TaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xMonSrvConnTaskBuffer,             // Pointer to task control block
							0
		   );
			if (MonSrvConn_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create Monitor SRV CONNECT task\n");
#endif
				}
		}
}

// Main monitoring server connection task
static void MonSrvConn(void *pvParameters __attribute__((unused))) {
    // Allocate memory for message reception
	AMessage_st s_Message_Rx_data; // = heap_caps_calloc(sizeof(AMessage_st),1,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	AMessage_st *s_Message_Rx = &s_Message_Rx_data;
	s_Message_Rx->Dest_ID_a8 = SYSTEM;
	s_Message_Rx->Src_ID_u8 = CONSOLE;
	s_Message_Rx->payload_p8 = (uint8_t*) Monitor_data_buffer;
	s_Message_Rx->payload_size = 0;
	strcpy((char*)s_Message_Rx->cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx->dest_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx->src_Actor_a8,"CONSOLE");

    char str[MESSAGE_SIZE] = {0};  // Initialize message buffer
    char SSID[SSID_SIZE] = {"\0"};  // Initialize SSID string
    char Rescue_SSID[SSID_SIZE] = {"\0"};  // Initialize rescue SSID string
    char Current_State[100] = {0};
    uint8_t blink_state_val=0, wifi_enabled = 0xFF, l_wifi_status = E_wifi_ZERO_STATE, l_eth_status =E_ETH_ZERO_STATE;  // Initialize status variables

    MonSrvConnState CurrentMonSrvState = MONSRV_STATE_IDLE;  // Set initial state

    while (1) {
        // Check if server connection flag is disabled
        if (Server_Conn_Flag == Disable) {
            // Log and handle the disable state
            CurrentMonSrvState = MONSRV_STATE_IDLE;
            wifi_enabled = 0xFF;
            memset(SSID, 0, sizeof(SSID));  // Clear SSID
            memset(Rescue_SSID, 0, sizeof(Rescue_SSID));  // Clear rescue SSID
            MonSrvConn_Handle = NULL;
            vTaskDelete(MonSrvConn_Handle);  // Delete the task
        }
        // Handle incoming messages from the queue
        memset(Monitor_buffer, 0, BUFFER_SIZE);
        xQueueReceive(MonSrvConn_Que, (void*)Monitor_buffer, MON_CONN_MAX_DELAY / portTICK_PERIOD_MS);
		{
			 if (Server_Conn_Flag == Disable)
			 {
				   MonSrvConn_Handle = NULL;
				   vTaskDelete(MonSrvConn_Handle);  // Delete the task
			  }
        	switch(CurrentMonSrvState)
        	{
				case 	MONSRV_STATE_IDLE							: strcpy(Current_State, "IDLE"); 					break;
				case	MONSRV_STATE_CHECK_IHUB_STATUS				: strcpy(Current_State, "CHECK IHUB STATUS"); 		break;
				case	MONSRV_STATE_CHECK_WIFI_ETH_ENABLED			: strcpy(Current_State, "GET WIFI ETH STATUS"); 	break;
				case	MONSRV_STATE_CHECK_WIFI_ETH_ENABLED_STATUS	: strcpy(Current_State, "CHECK WIFI ETH STATUS"); 	break;
				case	MONSRV_STATE_CHECK_WIFI_APINFO				: strcpy(Current_State, "CHECK WIFI APINFO"); 		break;
				case	MONSRV_STATE_CHECK_RESCUE_SSID				: strcpy(Current_State, "CHECK RESCUE SSID"); 		break;
				case	MONSRV_STATE_DISCONNECT_WIFI				: strcpy(Current_State, "DISCONNECT WIFI"); 		break;
				case	MONSRV_STATE_NETWORK_SCAN					: strcpy(Current_State, "NETWORK SCAN"); 			break;
				default												: strcpy(Current_State, "DEFAULT");					break;
        	}
        	cJSON *root = cJSON_CreateObject();
        	cJSON_AddStringToObject(root, "MONITOR STATE", Current_State);
			memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
			cJSON_PrintPreallocated(root, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);
        	cJSON_Delete(root);
        	console_send_responce_to_console_xface(s_Message_Rx);

            cJSON *In_JSON = cJSON_Parse((char*)Monitor_buffer);  // Parse JSON input
            if ((In_JSON == NULL)&&(CurrentMonSrvState != MONSRV_STATE_IDLE)){
                // Handle invalid JSON input
            	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
                Add_Response_msg(str, s_Message_Rx, payLoadData_Monitor);
                 continue;
            } else
			{
            	// WIFI/ETH/IHUB events are checked at first because they are not state dependent
            	cJSON *JSON_WifiStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "WIFI_CONNECTION_STATUS");
				if (cJSON_IsString(JSON_WifiStatus) && (JSON_WifiStatus != NULL)) {
					if(strcmp(JSON_WifiStatus->valuestring,"DISCONNECTED")==0)
					{

						if(esp_timer_is_active(periodic_timer) == true)
						{
							esp_err_t err = esp_timer_stop(periodic_timer); // stop the timer
							if(err != ESP_OK)
								printf("\n\n set Error in stopping the periodic timer. err = %d\n\n", err);
						}

						blink_state_val= LED_BLINK_STATE3;
						sendLedState("3-blink", -1, "none");
						Mon_reset_stack(NULL, s_Message_Rx, "WIFI is disconnected. Resetting the stack...");  // Reset stack on IHUB disconnect
					}
				}
				cJSON *JSON_IhubStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "IHUB_CONNECTION_STATUS");
				if (cJSON_IsString(JSON_IhubStatus) && (JSON_IhubStatus != NULL)) {
					if(strcmp(JSON_IhubStatus->valuestring,"DISCONNECTED")==0)
					{

						if(esp_timer_is_active(periodic_timer) == true)
						{
							esp_err_t err = esp_timer_stop(periodic_timer); // stop the timer
							if(err != ESP_OK)
								printf("\n\n set Error in stopping the periodic timer. err = %d\n\n", err);
						}

						blink_state_val= LED_BLINK_STATE8;
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
						cJSON *ERROR_NUM_item = cJSON_GetObjectItem(In_JSON, "ERROR_NUM");
					    if (cJSON_IsNumber(ERROR_NUM_item) && (ERROR_NUM_item != NULL))
					    {
					       // if (ERROR_NUM_item->valueint != 0)
					        {

					        	if(ERROR_NUM_item->valueint == 1)
					        	{
					        		sendLedState("6-blink", -1, "iHUB");
					        	}
					        	else if((ERROR_NUM_item->valueint == 6) || (ERROR_NUM_item->valueint == 12))
					        	{
					        		sendLedState("5-blink", -1, "iHUB");
					        	}
					        	else
					        	{
									sendLedState("7-blink", -1, "iHUB");
					        	}
					        }
					    }
					    else
						{
							sendLedState("7-blink", -1, "iHUB");

							printf("7-blink iHUB 3 \n");
						}
#else
						sendLedState("7-blink",-1, "iHUB");
#endif
						Mon_reset_stack(NULL, s_Message_Rx, "IHUB status disconnected. Resetting the stack...");  // Reset stack on IHUB disconnect
					//	CurrentMonSrvState = MONSRV_STATE_IDLE;

					}
					else if(strcmp(JSON_IhubStatus->valuestring,"CONNECTED")==0)
					{
						if(esp_timer_is_active(periodic_timer) == false)  // start the timer if it is not yet started
						{
							esp_err_t err = esp_timer_start_periodic(periodic_timer, (10800000000)); // start the timer with the period of 3x60x60x1000000 sec
							if(err != ESP_OK)
								printf("\n\n set Error in start periodic timer. err = %d\n\n", err);
						}


						if(blink_state_val != LED_BLINK_STATE_FULLON){
							vTaskDelay(50);
							blink_state_val= LED_BLINK_STATE_FULLON;
							sendLedState("SOLID",-1, "none");
							Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_STOP");
						}
					}

				}
				cJSON *JSON_EthStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "ETH_CONNECTION_STATUS");
				if (cJSON_IsString(JSON_EthStatus) && (JSON_EthStatus != NULL)) {
					if((strcmp(JSON_EthStatus->valuestring,"DISCONNECTED")==0) && (l_wifi_status != E_wifi_CONNECTED_LOW))
					{
						blink_state_val= LED_BLINK_STATE5;
						Mon_reset_stack(NULL, s_Message_Rx, "ETH is disconnected. Resetting the stack...");  // Reset stack on IHUB disconnect
					}
				}
				cJSON *JSON_Twin = cJSON_GetObjectItemCaseSensitive(In_JSON, "TWIN_MESSAGE");
				if (JSON_Twin != NULL)
				{
				    cJSON *root_JSON = cJSON_CreateObject();
					cJSON_AddStringToObject(root_JSON, "connectionMessage", "IHUB Twin Received");
					memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
					cJSON_PrintPreallocated(root_JSON, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);

					cJSON_Delete(root_JSON);
					console_send_responce_to_console_xface(s_Message_Rx);

			        cJSON *rootNew_JSON = cJSON_CreateObject();
			        cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "CONNECTED");
					memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
					cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);

			        cJSON_Delete(rootNew_JSON);
			        console_send_responce_to_console_xface(s_Message_Rx);

					if(blink_state_val != LED_BLINK_STATE_FULLON){
						vTaskDelay(50);
						blink_state_val= LED_BLINK_STATE_FULLON;
						sendLedState("SOLID",-1, "none");
						Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_STOP");
					}
				}

				cJSON *JSON_Delete = cJSON_GetObjectItemCaseSensitive(In_JSON, "DELETE_MON");
					if (cJSON_IsString(JSON_Delete) && (JSON_Delete != NULL)) {
						if((strcmp(JSON_Delete->valuestring,"1")==0)){
							Mon_reset_stack(Monitor_buffer, s_Message_Rx, "Delete Server monitor task");  // Reset stack on IHUB disconnect
						}
					}
                // Process the JSON based on the current state
                switch (CurrentMonSrvState) {
					case MONSRV_STATE_IDLE:
						handle_idle_state(&CurrentMonSrvState, s_Message_Rx);
						break;

                    case MONSRV_STATE_CHECK_WIFI_ETH_ENABLED_STATUS:
                    //	handle_wifi_eth_enabled_status(&CurrentMonSrvState, &Mon_que_rx_delay, &timeout_count_credential_3_3_u8, In_JSON, &wifi_enabled, &l_wifi_status, &l_eth_status, s_Message_Rx, &blink_state_val);
                        handle_wifi_eth_enabled_status(&CurrentMonSrvState, In_JSON, &wifi_enabled, &l_wifi_status, &l_eth_status, s_Message_Rx, &blink_state_val);
                        break;
                    case MONSRV_STATE_CHECK_IHUB_STATUS:
                        handle_ihub_status(Monitor_buffer, &CurrentMonSrvState, In_JSON, SSID, Rescue_SSID, &wifi_enabled, s_Message_Rx, &blink_state_val);
                        __attribute__((fallthrough));
					case MONSRV_STATE_CHECK_WIFI_ETH_ENABLED:
						// Create JSON command to check WiFi enabled status
			       		cJSON *my_JSON1  	= cJSON_CreateArray();
			       		if (my_JSON1 == NULL) {
			       		        return;
			       		    }
			       		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ENABLE_WIFI"));
			       		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_STATUS"));
			       		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
			       		cJSON *jsonObject = cJSON_CreateObject();
			       		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
						memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
						cJSON_PrintPreallocated(jsonObject, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
						cJSON_Delete(jsonObject);
						Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData_Monitor, strlen(payLoadData_Monitor), "GET");
						wifi_enabled = 0xFF;  // Reset WiFi enabled status
						l_wifi_status = E_wifi_ZERO_STATE;  // Reset local WiFi status
						CurrentMonSrvState = MONSRV_STATE_CHECK_WIFI_ETH_ENABLED_STATUS;
					    break;
//                    case MONSRV_STATE_ETH_STATUS:
//                        handle_eth_status(&CurrentMonSrvState, In_JSON, SSID, Rescue_SSID, &wifi_enabled, s_Message_Rx);
//                        break;
                    case MONSRV_STATE_CHECK_WIFI_APINFO:
                        handle_wifi_apinfo(&CurrentMonSrvState, In_JSON, SSID,Rescue_SSID,s_Message_Rx,&wifi_enabled);
                        break;
                    case MONSRV_STATE_CHECK_RESCUE_SSID:
                        handle_rescue_ssid(&CurrentMonSrvState, In_JSON, SSID, Rescue_SSID,s_Message_Rx,&l_eth_status,&wifi_enabled);
                        break;
                    case MONSRV_STATE_NETWORK_SCAN:
                    	mon_handle_network_scan(&CurrentMonSrvState, In_JSON, SSID, Rescue_SSID, s_Message_Rx, &l_wifi_status, &l_eth_status, &wifi_enabled);
                        break;
                    default:
                        break;
                }
                cJSON_Delete(In_JSON);  // Free the parsed JSON object
            }
        }
    }
}

// Reset stack and free resources
static void Mon_reset_stack(char *buffer, AMessage_st *s_Message_Rx, char *message) {
    Add_Response_msg(message, s_Message_Rx, payLoadData_Monitor);  // Log the reset reason
    Reset_Stack(s_Message_Rx, payLoadData_Monitor);
    Add_Response_msg("Exit monitoring task", s_Message_Rx, payLoadData_Monitor);
    xQueueReset(MonSrvConn_Que);   // reset the MonSrvConn_Que
    MonSrvConn_Handle = NULL;
    vTaskDelete(MonSrvConn_Handle);  // Delete the task
}

// Handle the idle state
static void handle_idle_state(MonSrvConnState *CurrentMonSrvState, AMessage_st *s_Message_Rx) {
	cJSON *my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
			return;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
	cJSON_Delete(jsonObject);
    Send_CMD_To_Other_Actor(IHUB, "IHUB", payLoadData_Monitor, strlen(payLoadData_Monitor), "GET");
    *CurrentMonSrvState = MONSRV_STATE_CHECK_IHUB_STATUS;  // Transition to next state
}

//static void handle_wifi_eth_enabled_status(MonSrvConnState *CurrentMonSrvState, uint32_t *Mon_que_rx_delay, uint8_t *timeout_count_credential_3_3_u8, cJSON *In_JSON, uint8_t *wifi_enabled, uint8_t *l_wifi_status, uint8_t *l_eth_status, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
static void handle_wifi_eth_enabled_status(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, uint8_t *wifi_enabled, uint8_t *l_wifi_status, uint8_t *l_eth_status, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) { 
    uint8_t u8prevethstatus = 0, u8prevwifistatus = 0;
    bool all_keys_present = true;

    cJSON *JSON_WifiENabled = cJSON_GetObjectItemCaseSensitive(In_JSON, "ENABLE_WIFI");
    if (cJSON_IsString(JSON_WifiENabled) && (JSON_WifiENabled->valuestring != NULL)) {
        char WifiENStatus = atoi(JSON_WifiENabled->valuestring);
        *wifi_enabled = WifiENStatus;  // Update WiFi enabled status
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "ENABLE_WIFI") == NULL) {
        all_keys_present = false;
    }

    cJSON *JSON_WifiStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "WIFI_STATUS");
    if (cJSON_IsString(JSON_WifiStatus) && (JSON_WifiStatus->valuestring != NULL)) {
        *l_wifi_status = atoi(JSON_WifiStatus->valuestring);  // Update local WiFi status
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "WIFI_STATUS") == NULL) {
        all_keys_present = false;
    }

    cJSON *JSON_ETHStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "ETH_STATUS");
    if (cJSON_IsString(JSON_ETHStatus) && (JSON_ETHStatus->valuestring != NULL)) {
        *l_eth_status = atoi(JSON_ETHStatus->valuestring);  // Update local ETH status
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "ETH_STATUS") == NULL) {
        all_keys_present = false;
    }

    if (all_keys_present) {
        wifi_eth_enabled_timeout_start = 0;  // Reset timeout since all keys are present

        if ((*l_eth_status == E_ETH_CONNECTED) && (*l_wifi_status != E_wifi_CONNECTED_LOW)) {
            u8prevethstatus = 1;
        	cJSON *my_JSON1  	= cJSON_CreateArray();
        	if (my_JSON1 == NULL) {
        			return;
        		}
        	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("RESCUE_SSID"));
        	cJSON *jsonObject = cJSON_CreateObject();
        	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
        	memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
        	cJSON_PrintPreallocated(jsonObject, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
        	cJSON_Delete(jsonObject);
            Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Monitor, strlen(payLoadData_Monitor), "GET");
            *CurrentMonSrvState = MONSRV_STATE_CHECK_RESCUE_SSID;  // Proceed to check rescue SSID
        } else if ((*l_eth_status != E_ETH_CONNECTED) && (*l_wifi_status == E_wifi_CONNECTED_LOW)) {
            u8prevwifistatus = 1;
            Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "AP_INFO");
            *CurrentMonSrvState = MONSRV_STATE_CHECK_WIFI_APINFO;
        } else if ((*l_eth_status == E_ETH_CONNECTED) && (*l_wifi_status == E_wifi_CONNECTED_LOW)) {
            u8prevwifistatus = 1;
            Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "AP_INFO");
            *CurrentMonSrvState = MONSRV_STATE_CHECK_WIFI_APINFO;
        } else if ((u8prevwifistatus == 1) && (*l_eth_status != E_ETH_CONNECTED) && (*l_wifi_status != E_wifi_CONNECTED_LOW)) {
            u8prevwifistatus = 0;
            *blink_state_val = LED_BLINK_STATE3;
            sendLedState("3-blink", -1, "none");
            cJSON *rootNew_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "BAD WIFI PASSWORD");
			memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
			cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);

            cJSON_Delete(rootNew_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);

        } else if ((u8prevethstatus == 1) && (*l_eth_status != E_ETH_CONNECTED) && (*l_wifi_status != E_wifi_CONNECTED_LOW)) {
            u8prevethstatus = 0;
            *blink_state_val = LED_BLINK_STATE5;
            cJSON *rootNew_JSON = cJSON_CreateObject();
            cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "WAITING FOR ETH");
			memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
			cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);
            cJSON_Delete(rootNew_JSON);
            console_send_responce_to_console_xface(s_Message_Rx);
        }
    } else {
        // Start or check timeout for WiFi/ETH status check failure
        if (wifi_eth_enabled_timeout_start == 0) {
            wifi_eth_enabled_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), wifi_eth_enabled_timeout_start) > WIFI_ETH_ENABLED_TIMEOUT_SECONDS) {
                // Timeout of 3 seconds reached, handle as failure
                Add_Response_msg("Timeout waiting for WiFi/ETH status", s_Message_Rx, payLoadData_Monitor);
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                // Handle timeout by resetting state or taking appropriate action
                // Reset timeout start
                wifi_eth_enabled_timeout_start = 0;
            }
        }
    }
}

// Handle checking IHUB status
static void handle_ihub_status(char* buffer, MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, uint8_t *wifi_enabled, AMessage_st *s_Message_Rx, uint8_t *blink_state_val) {
    cJSON *JSON_IhubStatus = cJSON_GetObjectItemCaseSensitive(In_JSON, "STATUS");
    if (cJSON_IsString(JSON_IhubStatus) && (JSON_IhubStatus != NULL)) {
        if (strcmp(JSON_IhubStatus->valuestring, "IHUB CONNECTED") != 0) {
            *blink_state_val= LED_BLINK_STATE8;
            sendLedState("7-blink", -1, "iHUB");
            Mon_reset_stack(buffer, s_Message_Rx, "IHUB status disconnected. Resetting the stack.");  // Reset stack on IHUB disconnect
        } else {
            *CurrentMonSrvState = MONSRV_STATE_CHECK_WIFI_ETH_ENABLED;//MONSRV_STATE_CHECK_WIFI_ETH_ENABLED;  // Proceed to check WiFi status
        }
    }
}

// Handle checking Ethernet status
//static void handle_eth_status(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, uint8_t *wifi_enabled, AMessage_st *s_Message_Rx) {
//    cJSON *JSON_Message = cJSON_GetObjectItem(In_JSON, "ETH_STATUS");
//    if (cJSON_IsString(JSON_Message) && (JSON_Message != NULL)) {
//        if (strcmp(JSON_Message->valuestring, "1") == 0) {
//            *CurrentMonSrvState = MONSRV_STATE_IDLE;
//            *wifi_enabled = 0xFF;
//            memset(SSID, 0, strlen(SSID));  // Clear SSID
//            memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
//          //  Add_Response_msg("Waiting for 1 min interval", s_Message_Rx);
//            //vTaskDelay(WAIT_ONE_MINUTE);  // Delay for 1 minute
//        } else {
//            Add_Response_msg("Ethernet connection failed", s_Message_Rx);
//            *CurrentMonSrvState = MONSRV_STATE_IDLE;
//            *wifi_enabled = 0xFF;
//            memset(SSID, 0, strlen(SSID));  // Clear SSID
//            memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
//            //Add_Response_msg("Waiting for 1 min interval", s_Message_Rx);
//          //  vTaskDelay(WAIT_ONE_MINUTE);  // Delay for 1 minute
//        }
//    } else {
//        Add_Response_msg("JSON object is not a string or is NULL", s_Message_Rx);
//    }
//}

// Handle checking WiFi AP information
static void handle_wifi_apinfo(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, AMessage_st *s_Message_Rx, uint8_t *wifi_enabled) {
    bool ssid_key_present = true;

    // Check for SSID key
    cJSON *JSON_SSID = cJSON_GetObjectItemCaseSensitive(In_JSON, "SSID");
    if (cJSON_IsString(JSON_SSID) && (JSON_SSID->valuestring != NULL)) {
        strcpy(SSID, JSON_SSID->valuestring);  // Copy SSID from JSON
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "SSID") == NULL) {
        ssid_key_present = false;
    }

    if (ssid_key_present) {
        wifi_apinfo_timeout_start = 0;  // Reset timeout since SSID key is present

        if (strlen(SSID) != 0) {
        	cJSON *my_JSON1  	= cJSON_CreateArray();
        	if (my_JSON1 == NULL) {
        			return;
        		}
        	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("RESCUE_SSID"));
        	cJSON *jsonObject = cJSON_CreateObject();
        	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
        	memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
        	cJSON_PrintPreallocated(jsonObject, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
        	cJSON_Delete(jsonObject);
            Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData_Monitor, strlen(payLoadData_Monitor), "GET");
            *CurrentMonSrvState = MONSRV_STATE_CHECK_RESCUE_SSID;  // Proceed to check rescue SSID
        } else {
            *CurrentMonSrvState = MONSRV_STATE_IDLE;
            *wifi_enabled = 0xFF;
            memset(SSID, 0, strlen(SSID));  // Clear SSID
            memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
        }
    } else {
        // Start or check timeout for WiFi AP info check failure
        if (wifi_apinfo_timeout_start == 0) {
            wifi_apinfo_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), wifi_apinfo_timeout_start) > WIFI_APINFO_TIMEOUT_SECONDS) {
                // Timeout of 3 seconds reached, handle as failure
                Add_Response_msg("Timeout waiting for SSID", s_Message_Rx, payLoadData_Monitor);
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID

                // Prepare JSON response for timeout
                cJSON *rootNew_JSON = cJSON_CreateObject();
                cJSON_AddStringToObject(rootNew_JSON, "connectionSTATE", "TIMEOUT WAITING FOR SSID");
				memset(payLoadData_Monitor,0,sizeof(payLoadData_Monitor));
				cJSON_PrintPreallocated(rootNew_JSON, payLoadData_Monitor, sizeof(payLoadData_Monitor), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Monitor);

                cJSON_Delete(rootNew_JSON);
                console_send_responce_to_console_xface(s_Message_Rx);

                // Reset timeout start
                wifi_apinfo_timeout_start = 0;
            }
        }
    }
}
// Handle checking rescue SSID
static void handle_rescue_ssid(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, AMessage_st *s_Message_Rx, uint8_t *l_eth_status, uint8_t *wifi_enabled) {
    bool rescue_ssid_key_present = true;

    // Check for RESCUE_SSID key
    cJSON *JSON_Rescue_SSID = cJSON_GetObjectItemCaseSensitive(In_JSON, "RESCUE_SSID");
    if (cJSON_IsString(JSON_Rescue_SSID) && (JSON_Rescue_SSID->valuestring != NULL)) {
        strcpy(Rescue_SSID, JSON_Rescue_SSID->valuestring);  // Copy rescue SSID from JSON
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "RESCUE_SSID") == NULL) {
        rescue_ssid_key_present = false;
    }

    if (rescue_ssid_key_present) {
        rescue_ssid_timeout_start = 0;  // Reset timeout since RESCUE_SSID key is present

        if (strlen(Rescue_SSID) != 0) {
            if (strcmp(Rescue_SSID, SSID) == 0) {
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
            } else {
                Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "NETWORK_SCAN");
                *CurrentMonSrvState = MONSRV_STATE_NETWORK_SCAN;  // Proceed to network scan
            }
        } else {
            if ((strlen(SSID) != 0) && (*l_eth_status == 1)) {
                Add_Response_msg("Disconnect WIFI and re-connect to server using Ethernet", s_Message_Rx, payLoadData_Monitor);
                Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "DISCONNECT");
            } else {
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
            }
        }
    } else {
        // Start or check timeout for rescue SSID check failure
        if (rescue_ssid_timeout_start == 0) {
            rescue_ssid_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), rescue_ssid_timeout_start) > RESCUE_SSID_TIMEOUT_SECONDS) {
                // Timeout of 3 seconds reached, handle as failure
                Add_Response_msg("Timeout waiting for RESCUE_SSID", s_Message_Rx, payLoadData_Monitor);
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
                // Reset timeout start
                rescue_ssid_timeout_start = 0;
            }
        }
    }
}

// Handle network scan results
static void mon_handle_network_scan(MonSrvConnState *CurrentMonSrvState, cJSON *In_JSON, char *SSID, char *Rescue_SSID, AMessage_st *s_Message_Rx, uint8_t *l_wifi_status, uint8_t *l_eth_status,uint8_t *wifi_enabled) {
    char Nw_Scan_SSID[SSID_SIZE] = {"\0"};  // Initialize network scan SSID string
    char Nw_Scan_PWD[SSID_SIZE] = {"\0"};  // Initialize network scan password string
    uint8_t u8EthAvailable = 0;

    bool ssid_key_present = true;
    bool password_key_present = true;
    bool ethernet_key_present = true;

    // Extract SSID from input JSON
    cJSON *JSON_SSID = cJSON_GetObjectItemCaseSensitive(In_JSON, "SSID");
    if (cJSON_IsString(JSON_SSID) && (JSON_SSID->valuestring != NULL)) {
        strcpy(Nw_Scan_SSID, JSON_SSID->valuestring);  // Copy scanned SSID from JSON
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "SSID") == NULL) {
        ssid_key_present = false;
    }

    // Extract PASSWORD from input JSON
    cJSON *JSON_PASS = cJSON_GetObjectItemCaseSensitive(In_JSON, "PASSWORD");
    if (cJSON_IsString(JSON_PASS) && (JSON_PASS->valuestring != NULL)) {
        strcpy(Nw_Scan_PWD, JSON_PASS->valuestring);  // Copy scanned password from JSON
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "PASSWORD") == NULL) {
        password_key_present = false;
    }

    // Extract ETHERNET status from input JSON
    cJSON *JSON_ETH = cJSON_GetObjectItemCaseSensitive(In_JSON, "ETHERNET");
    if (cJSON_IsString(JSON_ETH) && (JSON_ETH->valuestring != NULL)) {
        if (strcmp(JSON_ETH->valuestring, "AVAILABLE") == 0) {
            u8EthAvailable = 1;
        }
    } else if (cJSON_GetObjectItemCaseSensitive(In_JSON, "ETHERNET") == NULL) {
        ethernet_key_present = false;
    }

    // Check if all required keys are present
    if ((ssid_key_present && password_key_present) || ethernet_key_present) {
        network_scan_timeout_start = 0;  // Reset timeout since all required keys are present

        if ((strlen(Nw_Scan_SSID) != 0))  // && (strlen(Nw_Scan_PWD) != 0)
        {
            if (strcmp(Rescue_SSID, Nw_Scan_SSID) == 0) {
                if (*l_wifi_status == E_wifi_CONNECTED_LOW) {
                  //  Add_Response_msg("Disconnect WIFI and re-connect to server", s_Message_Rx);
                    Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "DISCONNECT");
                } else if ((*l_eth_status == E_ETH_CONNECTED) && (*l_wifi_status != E_wifi_CONNECTED_LOW)) {
                    Mon_reset_stack(NULL, s_Message_Rx, "Reconnect to server with Rescue SSID");  // Reset stack to reconnect
                }
            } else {
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
            }
        } else {
            if ((u8EthAvailable == 1) && (*l_wifi_status == E_wifi_CONNECTED_LOW)) {
                Add_Response_msg("Disconnect WIFI and re-connect to server using Ethernet", s_Message_Rx, payLoadData_Monitor);
                Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "DISCONNECT");
            } else if ((u8EthAvailable == 1) && (*l_wifi_status != E_wifi_CONNECTED_LOW)) {
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
            }
        }
    } else {
        // Start or check timeout for network scan failure
        if (network_scan_timeout_start == 0) {
            network_scan_timeout_start = time(NULL); // Start the timeout
        } else {
            if (difftime(time(NULL), network_scan_timeout_start) > NETWORK_SCAN_TIMEOUT_SECONDS) {
                // Timeout of 3 seconds reached, handle as failure
                Add_Response_msg("Timeout waiting for SSID, PASSWORD, or ETHERNET", s_Message_Rx, payLoadData_Monitor);
                *CurrentMonSrvState = MONSRV_STATE_IDLE;
                *wifi_enabled = 0xFF;
                memset(SSID, 0, strlen(SSID));  // Clear SSID
                memset(Rescue_SSID, 0, strlen(Rescue_SSID));  // Clear rescue SSID
                // Reset timeout start
                network_scan_timeout_start = 0;
            }
        }
    }
}


static void Set_Server_Connection_Flag(AMessage_st* s_Message_Rx)
{
	cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);

	cJSON *enableItem = cJSON_GetObjectItem(New_JSON, "SER_TASK_DIS_FLAG");
	if (cJSON_IsNumber(enableItem))
	{
		uint8_t u8ser_task_dis_fg = enableItem->valueint;
		if(u8ser_task_dis_fg)
		{
			Add_Response_msg("Server_Connection_Flag Enabled", s_Message_Rx, payLoadData);
			Server_Conn_Flag = Enable;
		}
		else
		{
			Add_Response_msg("Server_Connection_Flag Disabled", s_Message_Rx, payLoadData);
			Server_Conn_Flag = Disable;
		}
	}
}

static void Get_System_timers(void *pvParameters __attribute__((unused)))
{
	uint8_t var_IHUB_state = 0, var_WIFI_state = 0;
	uint64_t var_IHUB_UPDATE_TIME = 0, var_WIFI_UPDATE_TIME = 0, var_HTTP_DL_TIME = 0;
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *temp_JSON = NULL;
	char str[200] = {0};
	char  found = 0;
	cJSON *root = NULL;

    cJSON *my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("STATUS"));  // get IHUB status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("IHUB_UPDATE_TIME"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_timer,0,sizeof(payLoadData_timer));
	cJSON_PrintPreallocated(jsonObject, payLoadData_timer, sizeof(payLoadData_timer), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(IHUB,"IHUB", payLoadData_timer, strlen(payLoadData_timer), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ENABLE_WIFI"));  // get wifi and eth status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_STATUS"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_UPDATE_TIME"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_timer,0,sizeof(payLoadData_timer));
	cJSON_PrintPreallocated(jsonObject, payLoadData_timer, sizeof(payLoadData_timer), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_timer, strlen(payLoadData_timer), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("HTTP_DL_TIME"));  // get wifi and eth status
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_timer,0,sizeof(payLoadData_timer));
	cJSON_PrintPreallocated(jsonObject, payLoadData_timer, sizeof(payLoadData_timer), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_timer, strlen(payLoadData_timer), "GET");

	root = cJSON_CreateObject();
	if (root == NULL) {
		Add_Response_msg("Failed to create root object",s_Message_Rx, payLoadData_state);
		goto exit;
	}

	while(1)
	{
		if (pdTRUE == xQueueReceive(TIMER_Que, (void*) bufferSystem_timers, 50))
		{
			cJSON *in_JSON 		= cJSON_Parse(bufferSystem_timers);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_timer);
				goto exit;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				if(strcmp(temp_JSON->valuestring, "2") == 0)
					var_WIFI_state = 1;
				else
					var_WIFI_state = 0;

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					var_WIFI_UPDATE_TIME = strtoull(temp_JSON->valuestring, NULL, 10);  // base 10
				}
				found++;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "HTTP_DL_TIME");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					var_HTTP_DL_TIME = strtoull(temp_JSON->valuestring, NULL, 10);  // base 10
				}
				found++;
			}
			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				if(strcmp(temp_JSON->valuestring, "IHUB CONNECTED") == 0)
				{
					var_IHUB_state = 1;
				}
				else
				{
					var_IHUB_state = 0;
				}

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "IHUB_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					var_IHUB_UPDATE_TIME = strtoull(temp_JSON->valuestring, NULL, 10);  // base 10
				}
				found++;
			}
			cJSON_Delete(in_JSON);
		}
		if(found == 3)
			break;
	}  // end of while (1)

	 int64_t time_since_boot_us = esp_timer_get_time();

	// Convert the time to seconds
	int64_t time_since_boot_sec = time_since_boot_us / 1000000;

	// Extract hours, minutes, and seconds from the total time in seconds
	int hours = time_since_boot_sec / 3600;
	int minutes = (time_since_boot_sec % 3600) / 60;
	sprintf(str,"%04d:%02d", hours, minutes);

    // Add string fields
    cJSON_AddStringToObject(root, "POWER_UPTIME",  str );

	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	uint64_t var_Current_time = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
	int64_t WIFI_UPDATE_TIME_sec = (var_Current_time - var_WIFI_UPDATE_TIME) / 1000;
	// Extract hours, minutes, and seconds from the total time in seconds
	int WIFI_UPDATE_TIME_hours = WIFI_UPDATE_TIME_sec / 3600;
	int WIFI_UPDATE_TIME_minutes = (WIFI_UPDATE_TIME_sec % 3600) / 60;

	sprintf(str,"%04d:%02d", WIFI_UPDATE_TIME_hours, WIFI_UPDATE_TIME_minutes);

	if(var_WIFI_state == 1)
	{
	    cJSON_AddStringToObject(root, "WIFI_CONN_TIME",  str);
	    cJSON_AddNullToObject(root,   "WIFI_DISC_TIME");
	}
	else
	{
		cJSON_AddNullToObject(root,   "WIFI_CONN_TIME");
	    cJSON_AddStringToObject(root, "WIFI_DISC_TIME",  str);
	}

	int64_t DeviceAnnoune_sec = (var_Current_time - DeviceAnnoune_status_time_u64 ) / 1000;

	// Extract hours, minutes, and seconds from the total time in seconds
	int DeviceAnnoune_hours = DeviceAnnoune_sec / 3600;
	int DeviceAnnoune_minutes = (DeviceAnnoune_sec % 3600) / 60;

	sprintf(str,"%04d:%02d", DeviceAnnoune_hours, DeviceAnnoune_minutes);

    cJSON_AddStringToObject(root, "HTTP_ANNOUNCE",   str);

	int64_t IHUB_UPDATE_TIME_sec = (var_Current_time - var_IHUB_UPDATE_TIME) / 1000;

	// Extract hours, minutes, and seconds from the total time in seconds
	int IHUB_UPDATE_TIME_hours = IHUB_UPDATE_TIME_sec / 3600;
	int IHUB_UPDATE_TIME_minutes = (IHUB_UPDATE_TIME_sec % 3600) / 60;
	sprintf(str,"%04d:%02d", IHUB_UPDATE_TIME_hours, IHUB_UPDATE_TIME_minutes);

	if(var_IHUB_state == 1)
	{
		cJSON_AddStringToObject(root, "IHUB_CONN_TIME",  str);
		cJSON_AddNullToObject(root,   "IHUB_DISC_TIME");
	}
	else
	{
		cJSON_AddNullToObject(root,   "IHUB_CONN_TIME");
		cJSON_AddStringToObject(root, "IHUB_DISC_TIME",  str);
	}

	if(var_HTTP_DL_TIME == 0)
	{
		cJSON_AddNullToObject(root,   "HTTP_DL_TIME");
	}
	else
	{
		int64_t HTTP_DL_TIME_sec = (var_Current_time - var_HTTP_DL_TIME) / 1000;

		// Extract hours, minutes, and seconds from the total time in seconds
		int HTTP_DL_TIME_hours = HTTP_DL_TIME_sec / 3600;
		int HTTP_DL_TIME_minutes = (HTTP_DL_TIME_sec % 3600) / 60;

		sprintf(str,"%04d:%02d", HTTP_DL_TIME_hours, HTTP_DL_TIME_minutes);
	    cJSON_AddStringToObject(root, "HTTP_DL_TIME",  str);
	}

    if(root != NULL)
    {
		memset(payLoadData_timer,0,sizeof(payLoadData_timer));
		cJSON_PrintPreallocated(root, payLoadData_timer, sizeof(payLoadData_timer), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_timer);
		cJSON_Delete(root);
		console_send_responce_to_console_xface(s_Message_Rx);
    }

    exit:
	TIMER_Handle = NULL;
    vTaskDelete(TIMER_Handle);  // Delete the task
}

static void Get_System_states(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *temp_JSON = NULL;
	char str[200] = {0}, BLE_str_cnt = 0;
	char  ETH_Status = 0, found = 0, WIFI_enable_status = 0, WIFI_Connection_status = 0;  //WIFI_Status =0,
	char BLE_CS_UPDATE_TIME[50] = {0},  BLE_CM_UPDATE_TIME[50] = {0};
	char BLE_str_CONN_STATUS[20] = {0}, BLE_str_CONN_MODE [20] = {0};
	char WIFI_Disconnect_Reason[100] = {0}, WIFI_update_time [50] = {0}, WIFI_En_update_time [50] = {0},IHUB_Disconnect_Reason[100] = {0};

	date_time_t  sdate_tim;
	uint64_t current_epos_msec;//mills;
	uint64_t Temp_msec_1;
	struct timeval currentTime_1;
	// Extract hours, minutes, and seconds from the total time in seconds
	int hours = 0;
	int minutes = 0;
	int seconds = 0;
	int64_t Time_diff = 0;
	uint64_t temp_val_64 = 0;

	cJSON *state_spiffs = NULL;
    cJSON *state_IHUB = NULL;
    cJSON *state_BLE_CM = NULL;
    cJSON *state_BLE_AM = NULL;
    cJSON *state_ETH = NULL;
    cJSON *state_WIFI = NULL;
    cJSON *root = NULL;

	cJSON *my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_DISCON_REASON"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_state,0,sizeof(payLoadData_state));
	cJSON_PrintPreallocated(jsonObject, payLoadData_state, sizeof(payLoadData_state), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(WIFI,"WIFI", payLoadData_state, strlen(payLoadData_state), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SPIFFS_STATUS"));  // get SPIFFS status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SPIFFS_UPDATE_TIME"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_state,0,sizeof(payLoadData_state));
	cJSON_PrintPreallocated(jsonObject, payLoadData_state, sizeof(payLoadData_state), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS", payLoadData_state, strlen(payLoadData_state), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ADVERT_MODE"));  // get BLE status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("CONN_STATUS"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("CONN_MODE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("BLE_CS_UPDATE_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("BLE_AS_UPDATE_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("BLE_CM_UPDATE_TIME"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_state,0,sizeof(payLoadData_state));
	cJSON_PrintPreallocated(jsonObject, payLoadData_state, sizeof(payLoadData_state), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(BLE,"BLE", payLoadData_state, strlen(payLoadData_state), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("STATUS"));  // get IHUB status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("IHUB_UPDATE_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("IHUB_DISCON_REASON"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_state,0,sizeof(payLoadData_state));
	cJSON_PrintPreallocated(jsonObject, payLoadData_state, sizeof(payLoadData_state), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(IHUB,"IHUB", payLoadData_state, strlen(payLoadData_state), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
		   goto exit;
		}
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ENABLE_WIFI"));  // get wifi and eth status
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ENABLE_ETH"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_STATUS"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_UPDATE_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_UPDATE_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_EN_UPDATE_TIME"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_state,0,sizeof(payLoadData_state));
	cJSON_PrintPreallocated(jsonObject, payLoadData_state, sizeof(payLoadData_state), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_state, strlen(payLoadData_state), "GET");

	root = cJSON_CreateObject();
	if (root == NULL) {
		Add_Response_msg("Failed to create root object",s_Message_Rx, payLoadData_state);
		goto exit;
	}
	cJSON *states = cJSON_CreateArray();
	if (states == NULL) {
		Add_Response_msg("Failed to create States array",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}
	cJSON_AddItemToObject(root, "States", states);

	while(1)
	{
		if (pdTRUE == xQueueReceive(STATE_Que, (void*) bufferSystem_states, 50))
		{
			cJSON *in_JSON 		= cJSON_Parse(bufferSystem_states);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_state);
				goto exit;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_DISCON_REASON");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				strcpy(WIFI_Disconnect_Reason, temp_JSON->valuestring);
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "ENABLE_ETH");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				state_ETH = cJSON_CreateObject();
				if (state_ETH == NULL) {
					Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
					cJSON_Delete(root);
					root = NULL;
					goto exit;
				}
				cJSON_AddStringToObject(state_ETH, "NAME", "Ethernet");
				if(atoi(temp_JSON->valuestring) == 1)  // if ethernet is enabled
				{
					temp_JSON 		= cJSON_GetObjectItem(in_JSON, "ETH_STATUS");
					if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
					{
						ETH_Status = atoi(temp_JSON->valuestring);
						switch (ETH_Status)
						{
						case E_ETH_ZERO_STATE:
							strcpy(str, "Disconnected");
							break;

						case E_ETH_NOT_CONNECTED:
							strcpy(str, "Disconnected");
							break;

						case E_ETH_CONNECTED:
							strcpy(str, "Connected");
							break;

						case E_ETH_LINK_UP:
							strcpy(str, "Disconnected");
							break;

						case E_ETH_STARTED:
							strcpy(str, "Disconnected");
							break;

						case E_ETH_STOPPED:
							strcpy(str, "Disconnected");
							break;

						default:
							break;
						}
					}  // end of ETH_STATUS
				} //end of if(temp_JSON->valuestring == '1')
				else
				{
					strcpy(str, "Disabled");
				}
				cJSON_AddStringToObject(state_ETH, "VALUE", str);

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "ETH_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(temp_JSON->valuestring, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);
						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_ETH, "LAST CHANGED", str);
					}
					else
					{
						cJSON_AddStringToObject(state_ETH, "LAST CHANGED", temp_JSON->valuestring);
					}
				}
				found++;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "ENABLE_WIFI");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				WIFI_enable_status = atoi(temp_JSON->valuestring);
				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_EN_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
					strcpy(WIFI_En_update_time, temp_JSON->valuestring);
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				if(strcmp(temp_JSON->valuestring, "2") == 0)
					WIFI_Connection_status = 1;
				else
					WIFI_Connection_status = 0;

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "WIFI_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
					strcpy(WIFI_update_time, temp_JSON->valuestring);

				found++;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "ADVERT_MODE");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				state_BLE_AM = cJSON_CreateObject();
				if (state_BLE_AM == NULL) {
					Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
					cJSON_Delete(root);
					root = NULL;
					goto exit;
				}
				cJSON_AddStringToObject(state_BLE_AM, "NAME", "BLE Advertising");
				if(strcmp(temp_JSON->valuestring, "BLE ADVERTISING STARTED") == 0)
					cJSON_AddStringToObject(state_BLE_AM, "VALUE", "Enabled");
				else
					cJSON_AddStringToObject(state_BLE_AM, "VALUE", "Disabled");

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "BLE_AS_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(temp_JSON->valuestring, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_BLE_AM, "LAST CHANGED", str);

	//					cJSON_AddStringToObject(state_BLE_AM, "LAST CHANGED", temp_JSON->valuestring);
					}
					else
					{
						cJSON_AddStringToObject(state_BLE_AM, "LAST CHANGED", temp_JSON->valuestring);
					}
				}

				found++;
			}
			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "CONN_STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				strcpy(BLE_str_CONN_STATUS, temp_JSON->valuestring);
				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "BLE_CS_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					strcpy(BLE_CS_UPDATE_TIME, temp_JSON->valuestring);
				}
				BLE_str_cnt++;
				found++;
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "CONN_MODE");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				strcpy(BLE_str_CONN_MODE, temp_JSON->valuestring);
				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "BLE_CM_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					strcpy(BLE_CM_UPDATE_TIME, temp_JSON->valuestring);
				}
				BLE_str_cnt++;
				found++;
			}
			if(BLE_str_cnt >= 2)
			{
				BLE_str_cnt = 0;
				state_BLE_CM = cJSON_CreateObject();
				if (state_BLE_CM == NULL) {
					Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
					cJSON_Delete(root);
					root = NULL;
					goto exit;
				}

				cJSON_AddStringToObject(state_BLE_CM, "NAME", "BLE Connection");
				if(strcmp(BLE_str_CONN_MODE, "BLE NON CONNECTABLE") == 0)
				{
					cJSON_AddStringToObject(state_BLE_CM, "VALUE", "Disabled");
					BLE_CM_Prev_state = 0;
					BLE_CS_Prev_state = 255;

					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(BLE_CM_UPDATE_TIME, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", str);

	//					cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CM_UPDATE_TIME);
					}
					else
					{
						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CM_UPDATE_TIME);
					}
				}
				else if((strcmp(BLE_str_CONN_STATUS, "BLE CONNECTED") == 0) && (strcmp(BLE_str_CONN_MODE, "BLE CONNECTABLE") == 0))
				{
					cJSON_AddStringToObject(state_BLE_CM, "VALUE", "Connected");
					BLE_CS_Prev_state = 1;
					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(BLE_CS_UPDATE_TIME, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", str);

	//					cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CS_UPDATE_TIME);
					}
					else
					{
						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CS_UPDATE_TIME);
					}
				}
				else if((strcmp(BLE_str_CONN_STATUS, "BLE NOT CONNECTED") == 0) && (strcmp(BLE_str_CONN_MODE, "BLE CONNECTABLE") == 0))
				{
					BLE_CM_Prev_state = 1;

					cJSON_AddStringToObject(state_BLE_CM, "VALUE", "Enabled");
					if(BLE_CS_Prev_state == 255)   //Initial condition
					{
						if(Hardware_RevH_flag == true)
						{
							temp_val_64 = strtoll(BLE_CM_UPDATE_TIME, NULL, 10);

							_gettimeofday_r(NULL, &currentTime_1, NULL);
							current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

							if(temp_val_64 == 0)
							{
								hours = 0;
								minutes = 0;
								seconds = 0;
								sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
							}
							else if(temp_val_64 > 1000000000000000)
							{
								sprintf(str,"%016lld", temp_val_64);
							}
							else
							{
								if(current_epos_msec > temp_val_64)
								{
									Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
								}
								else
								{
									Time_diff = 0;	// Convert time from mSec to Sec
								}

								hours = Time_diff / 3600;
								minutes = (Time_diff % 3600) / 60;
								seconds = Time_diff % 60;
								sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
							}
							cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", str);

	//						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CM_UPDATE_TIME);
						}
						else
						{
							cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CM_UPDATE_TIME);
						}
					}
					else
					{
						BLE_CS_Prev_state  = 0;

						if(Hardware_RevH_flag == true)
						{
							temp_val_64 = strtoll(BLE_CS_UPDATE_TIME, NULL, 10);

							_gettimeofday_r(NULL, &currentTime_1, NULL);
							current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

							if(temp_val_64 == 0)
							{
								hours = 0;
								minutes = 0;
								seconds = 0;
								sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
							}
							else if(temp_val_64 > 1000000000000000)
							{
								sprintf(str,"%016lld", temp_val_64);
							}
							else
							{
								if(current_epos_msec > temp_val_64)
								{
									Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
								}
								else
								{
									Time_diff = 0;	// Convert time from mSec to Sec
								}

								hours = Time_diff / 3600;
								minutes = (Time_diff % 3600) / 60;
								seconds = Time_diff % 60;
								sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
							}
							cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", str);

	//						cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CS_UPDATE_TIME);
						}
						else
						{
							cJSON_AddStringToObject(state_BLE_CM, "LAST CHANGED", BLE_CS_UPDATE_TIME);
						}
					}
				}
			}

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "IHUB_DISCON_REASON");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
				strcpy(IHUB_Disconnect_Reason, temp_JSON->valuestring);

			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				state_IHUB = cJSON_CreateObject();
				if (state_IHUB == NULL) {
					Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
					cJSON_Delete(root);
					goto exit;
				}

//				cJSON_AddStringToObject(state_IHUB, "NAME", "Server Access");
				cJSON_AddStringToObject(state_IHUB, "NAME", "IHUB Connection");

				if(strcmp(temp_JSON->valuestring, "IHUB CONNECTED") == 0)
					cJSON_AddStringToObject(state_IHUB, "VALUE", "Connected");
				else
				{
					if ((!strcmp(IHUB_Disconnect_Reason, "")) || (strlen(IHUB_Disconnect_Reason) ==0))
					{
						strcpy(str, "Connection Yet To Start");
					}
					else
					{
						sprintf(str, "Connection Refused(%s)", IHUB_Disconnect_Reason);
					}
					cJSON_AddStringToObject(state_IHUB, "VALUE", str);
				}

				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "IHUB_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(temp_JSON->valuestring, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_IHUB, "LAST CHANGED", str);

	//					cJSON_AddStringToObject(state_IHUB, "LAST CHANGED", temp_JSON->valuestring);
					}
					else
					{
						cJSON_AddStringToObject(state_IHUB, "LAST CHANGED", temp_JSON->valuestring);
					}
				}
				found++;
			}
			temp_JSON 		= cJSON_GetObjectItem(in_JSON, "SPIFFS_STATUS");
			if((temp_JSON != NULL) && (temp_JSON->valuestring!=NULL))
			{
				state_spiffs = cJSON_CreateObject();
				if (state_spiffs == NULL) {
					Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
					cJSON_Delete(root);
					root = NULL;
					goto exit;
				}
				cJSON_AddStringToObject(state_spiffs, "NAME", "Boot Info");
				if(strcmp(temp_JSON->valuestring, "SPIFFS_POLULATED") == 0)
					cJSON_AddStringToObject(state_spiffs, "VALUE", "Populated");
				else if(strcmp(temp_JSON->valuestring, "SPIFFS_FILE_EMPTY") == 0)
					cJSON_AddStringToObject(state_spiffs, "VALUE", "Empty");
				else
					cJSON_AddStringToObject(state_spiffs, "VALUE", "Partial");
				temp_JSON 		= cJSON_GetObjectItem(in_JSON, "SPIFFS_UPDATE_TIME");
				if((temp_JSON != NULL) && (cJSON_IsString(temp_JSON)))
				{
					if(Hardware_RevH_flag == true)
					{
						temp_val_64 = strtoll(temp_JSON->valuestring, NULL, 10);

						_gettimeofday_r(NULL, &currentTime_1, NULL);
						current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

						if(temp_val_64 == 0)
						{
							hours = 0;
							minutes = 0;
							seconds = 0;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						else if(temp_val_64 > 1000000000000000)
						{
							sprintf(str,"%016lld", temp_val_64);
						}
						else
						{
							if(current_epos_msec > temp_val_64)
							{
								Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
							}
							else
							{
								Time_diff = 0;	// Convert time from mSec to Sec
							}

							hours = Time_diff / 3600;
							minutes = (Time_diff % 3600) / 60;
							seconds = Time_diff % 60;
							sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
						}
						cJSON_AddStringToObject(state_spiffs, "LAST CHANGED", str);

	//					cJSON_AddStringToObject(state_spiffs, "LAST CHANGED", temp_JSON->valuestring);
					}
					else
					{
						cJSON_AddStringToObject(state_spiffs, "LAST CHANGED", temp_JSON->valuestring);
					}
				}
				found++;
			}
			cJSON_Delete(in_JSON);
		}
		if(found == 7)
		{
			state_timeout_start = 0;
			break;
		}
		else
		{
	        // Start or check timeout for rescue SSID check failure
	        if (state_timeout_start == 0) {
	        	state_timeout_start = time(NULL); // Start the timeout
	        } else {
	            if (difftime(time(NULL), state_timeout_start) > STATE_RESP_TIMEOUT_SECONDS) {
	                // Timeout of 10 seconds reached, handle as failure
	                Add_Response_msg("Timeout waiting for SYSTEM.STATE method's response is reached.", s_Message_Rx, payLoadData_Monitor);
	                state_timeout_start = 0;
	        		cJSON_Delete(root);
	        		root = NULL;
	        		goto exit;
	            }
	        }

		}
	}  // end of while (1)

	// Obtain status of wifi
	state_WIFI = cJSON_CreateObject();
	if (state_WIFI == NULL) {
		Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}
	cJSON_AddStringToObject(state_WIFI, "NAME", "WiFi");
	if(WIFI_enable_status == 0)
	{
		cJSON_AddStringToObject(state_WIFI, "VALUE", "Disabled");
		if(Hardware_RevH_flag == true)
		{
			temp_val_64 = strtoll(WIFI_En_update_time, NULL, 10);

			_gettimeofday_r(NULL, &currentTime_1, NULL);
			current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

			if(temp_val_64 == 0)
			{
				hours = 0;
				minutes = 0;
				seconds = 0;
				sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
			}
			else if(temp_val_64 > 1000000000000000)
			{
				sprintf(str,"%016lld", temp_val_64);
			}
			else
			{
				if(current_epos_msec > temp_val_64)
				{
					Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
				}
				else
				{
					Time_diff = 0;	// Convert time from mSec to Sec
				}

				hours = Time_diff / 3600;
				minutes = (Time_diff % 3600) / 60;
				seconds = Time_diff % 60;
				sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
			}
			cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", str);

	//		cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", WIFI_En_update_time);
		}
		else
		{
			cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", WIFI_En_update_time);
		}
	}
	else
	{
		if(WIFI_Connection_status == 1)
			cJSON_AddStringToObject(state_WIFI, "VALUE", "Connected");
		else
		{
			if (!strcmp(WIFI_Disconnect_Reason, "")|| (strlen(WIFI_Disconnect_Reason) == 0))
			{
				strcpy(str, "Connection Yet To Start");
			}
			else
			{
				sprintf(str, "Connection Refused(%s)", WIFI_Disconnect_Reason);
			}

			cJSON_AddStringToObject(state_WIFI, "VALUE", str);
		}

		if(Hardware_RevH_flag == true)
		{
			temp_val_64 = strtoll(WIFI_update_time, NULL, 10);

			_gettimeofday_r(NULL, &currentTime_1, NULL);
			current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

			if(temp_val_64 == 0)
			{
				hours = 0;
				minutes = 0;
				seconds = 0;
				sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
			}
			else if(temp_val_64 > 1000000000000000)
			{
				sprintf(str,"%016lld", temp_val_64);
			}
			else
			{
				if(current_epos_msec > temp_val_64)
				{
					Time_diff = (current_epos_msec - temp_val_64)/1000;	// Convert time from mSec to Sec
				}
				else
				{
					Time_diff = 0;	// Convert time from mSec to Sec
				}

				hours = Time_diff / 3600;
				minutes = (Time_diff % 3600) / 60;
				seconds = Time_diff % 60;
				sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
			}
			cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", str);

	//		cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", WIFI_update_time);
		}
		else
		{
			cJSON_AddStringToObject(state_WIFI, "LAST CHANGED", WIFI_update_time);
		}
	}


	// Obtain status of credentials
	cJSON *state_Cred = cJSON_CreateObject();
	if (state_Cred == NULL) {
		Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}
	cJSON_AddStringToObject(state_Cred, "NAME", "Credentials");
	if (strlen((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL) == 0 && strlen((char*)&ACTOR_SYSTEM_Para.APIkey) == 0)
		cJSON_AddStringToObject(state_Cred, "VALUE", "Empty");
	else if (strlen((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL) == 0 || strlen((char*)&ACTOR_SYSTEM_Para.APIkey) == 0)
		cJSON_AddStringToObject(state_Cred, "VALUE", "Partial");
	else if (strlen((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL) != 0 && strlen((char*)&ACTOR_SYSTEM_Para.APIkey) != 0)
		cJSON_AddStringToObject(state_Cred, "VALUE", "Ready");
		
	if(Hardware_RevH_flag == true)
	{
		_gettimeofday_r(NULL, &currentTime_1, NULL);
		current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

		if(Credential_status_time_u64 == 0)
		{
			hours = 0;
			minutes = 0;
			seconds = 0;
			sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
		}
		else if(Credential_status_time_u64 > 1000000000000000)
		{
			sprintf(str,"%016lld", Credential_status_time_u64);
		}
		else
		{
			if(current_epos_msec > Credential_status_time_u64)
			{
				Time_diff = (current_epos_msec - Credential_status_time_u64)/1000;	// Convert time from mSec to Sec
			}
			else
			{
				Time_diff = 0;	// Convert time from mSec to Sec
			}

			hours = Time_diff / 3600;
			minutes = (Time_diff % 3600) / 60;
			seconds = Time_diff % 60;
			sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
		}
		cJSON_AddStringToObject(state_Cred, "LAST CHANGED", str);
	}
	else
	{
		sprintf(str,"%016lld", Credential_status_time_u64);
		cJSON_AddStringToObject(state_Cred, "LAST CHANGED", str);
	}

	// Obtain status of device announce
	cJSON *state_Internet = cJSON_CreateObject();
	if (state_Internet == NULL) {
		Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}

	cJSON_AddStringToObject(state_Internet, "NAME", "Last Device Announce");
	cJSON_AddStringToObject(state_Internet, "VALUE", deviceAnnounceStatus);

	if(Hardware_RevH_flag == true)
	{
		_gettimeofday_r(NULL, &currentTime_1, NULL);
		current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);

		if(DeviceAnnoune_status_time_u64 == 0)
		{
			hours = 0;
			minutes = 0;
			seconds = 0;
			sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
		}
		else if(DeviceAnnoune_status_time_u64 > 1000000000000000)
		{
			sprintf(str,"%016lld", DeviceAnnoune_status_time_u64);
		}
		else
		{
			if(current_epos_msec > DeviceAnnoune_status_time_u64)
			{
				Time_diff = (current_epos_msec - DeviceAnnoune_status_time_u64)/1000;	// Convert time from mSec to Sec
			}
			else
			{
				Time_diff = 0;	// Convert time from mSec to Sec
			}

			hours = Time_diff / 3600;
			minutes = (Time_diff % 3600) / 60;
			seconds = Time_diff % 60;
			sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
		}

		cJSON_AddStringToObject(state_Internet, "LAST CHANGED", str);
	}
	else
	{
		sprintf(str,"%016lld", DeviceAnnoune_status_time_u64);
		cJSON_AddStringToObject(state_Internet, "LAST CHANGED", str);
	}

	cJSON *state_PowerOnTime = cJSON_CreateObject();
	if (state_PowerOnTime == NULL) {
		Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}
	cJSON_AddStringToObject(state_PowerOnTime, "NAME", "Power On Time");
	cJSON_AddStringToObject(state_PowerOnTime, "VALUE", "");

	 int64_t time_since_boot_us = esp_timer_get_time();

	// Convert the time to seconds
	int64_t time_since_boot_sec = time_since_boot_us / 1000000;

	// Extract hours, minutes, and seconds from the total time in seconds
	hours = time_since_boot_sec / 3600;
	minutes = (time_since_boot_sec % 3600) / 60;
	seconds = time_since_boot_sec % 60;
	sprintf(str,"%04d:%02d:%02d", hours, minutes, seconds);
	cJSON_AddStringToObject(state_PowerOnTime, "LAST CHANGED", str);

//Local Time

    _gettimeofday_r(NULL, &currentTime_1, NULL);
	current_epos_msec = (uint64_t) (currentTime_1.tv_sec * 1000L) + (uint64_t) (currentTime_1.tv_usec / 1000L);
#ifdef ENABLE_PRINT_MSG
	printf("Current_epos_sec: %lld\n", current_epos_msec);
#endif
	Temp_msec_1 = EPOSCH_TO_30_YEAR;
	Temp_msec_1 = Temp_msec_1*1000;
	current_epos_msec = current_epos_msec - Temp_msec_1;
	if(current_epos_msec > Temp_msec_1)
	{
		current_epos_msec = (current_epos_msec - Temp_msec_1);
	}
	else
	{
								Time_diff = 0;	// Convert time from mSec to Sec
	}

	current_epos_msec = current_epos_msec + (gmt_val * 60*1000);//Reverse of set_rtc
	if(dst_val == 1)
	{
		current_epos_msec = current_epos_msec + (3600 * 1000); //advance clock by 1 hour/3600 seconds
	}
	epoch_to_date_time_m(&sdate_tim,current_epos_msec);
	UserDateTimeL_1.hour = sdate_tim.hour;
	UserDateTimeL_1.minute = sdate_tim.minute;
	UserDateTimeL_1.second = sdate_tim.second;
	UserDateTimeL_1.milSec = sdate_tim.milSec;
	UserDateTimeL_1.year = sdate_tim.year;
	UserDateTimeL_1.month = sdate_tim.month;
	UserDateTimeL_1.date = sdate_tim.date;

#ifdef ENABLE_PRINT_MSG
	printf("Local TIME: %02d/%02d/%02d %02d:%02d:%02d.%03d\n",UserDateTimeL_1.date, UserDateTimeL_1.month,UserDateTimeL_1.year,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec); // Years since 1900
#endif

	cJSON *state_LocalTime = cJSON_CreateObject();
	if (state_LocalTime == NULL) {
		Add_Response_msg("Failed to create State object",s_Message_Rx, payLoadData_state);
		cJSON_Delete(root);
		root = NULL;
		goto exit;
	}
	cJSON_AddStringToObject(state_LocalTime, "NAME", "Current Local Date/Time");
	cJSON_AddStringToObject(state_LocalTime, "VALUE", "");
//	uint64_t u64CurrentTime = get_current_time_ms();

	sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d.%03d", UserDateTimeL_1.date, UserDateTimeL_1.month,UserDateTimeL_1.year,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec);

	cJSON_AddStringToObject(state_LocalTime, "LAST CHANGED", str);


	cJSON_AddItemToArray(states, state_spiffs);
	cJSON_AddItemToArray(states, state_BLE_AM);
	cJSON_AddItemToArray(states, state_BLE_CM);
	cJSON_AddItemToArray(states, state_Cred);
	cJSON_AddItemToArray(states, state_ETH);
	cJSON_AddItemToArray(states, state_WIFI);
	cJSON_AddItemToArray(states, state_Internet);
	cJSON_AddItemToArray(states, state_IHUB);
	cJSON_AddItemToArray(states, state_PowerOnTime);
	cJSON_AddItemToArray(states, state_LocalTime);

    if(root != NULL)
    {
		memset(payLoadData_state,0,sizeof(payLoadData_state));
		cJSON_PrintPreallocated(root, payLoadData_state, sizeof(payLoadData_state), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_state);
		cJSON_Delete(root);
		console_send_responce_to_console_xface(s_Message_Rx);
    }

exit:
   STATE_Handle = NULL;
   vTaskDelete(STATE_Handle);  // Delete the task
}

static void Read_System_Properties_From_SPIFFS (AMessage_st* s_Message_Rx)
{
	// Create queue
	if(SPIFFS_Que == NULL)
	{
		SPIFFS_Que = xQueueCreateStatic(SPIFFS_QUE_COUNT, SPIFFS_QUE_ITEMSIZE, SPIFFS_pucQueueStorage, &SPIFFS_pxQueueBuffer);
	}
		if (SPIFFS_Que == NULL) {
		printf("Error in creating SPIFFS_Que\n ");
		return;
	}
	if(SPIFFS_Handle == NULL)
	{
		SPIFFS_Handle = xTaskCreateStaticPinnedToCore(
						Read_SPIFFS,                 // Task function
						"Read SPIFFS",            // Task name
						READ_PROPERTIES_SPIFFS_TASK_STACK_DEPTH,        // Stack size in words
						s_Message_Rx,                    // Task parameters (not used here)
						SPIFFS_READ_TASK_PRIORITY,                       // Task priority
						xSPIFFSTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xSPIFFSTaskBuffer,             // Pointer to task control block
						0
	   );
		if (SPIFFS_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create SPIFFS read task\n");
#endif
			}
	}
}

static void Read_SPIFFS(void *pvParameters __attribute__((unused)))
{
	char str[200] = {0}, string[512] = {0};
	AMessage_st s_Message_Rx_data;
	char data_buffer[512];
	memset(data_buffer,0,sizeof(data_buffer));
	memcpy(&s_Message_Rx_data,(AMessage_st*)pvParameters,sizeof(AMessage_st));
	if((char*)s_Message_Rx_data.payload_p8 != NULL)
	{
		strncpy(data_buffer, (char*)s_Message_Rx_data.payload_p8,511);
	}
	s_Message_Rx_data.payload_p8 = (uint8_t*) data_buffer;

	cJSON *responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "FILE_NAME",(char*)Device_File);
	cJSON_AddStringToObject(responseObject, "FIRMWARE_VERSION","");
	cJSON_AddStringToObject(responseObject, "BOOTLOADER_VERSION","");
	cJSON_AddStringToObject(responseObject, "HARDWARE_VERSION","");
	cJSON_AddStringToObject(responseObject, "DEVICE_ID","");
	cJSON_AddStringToObject(responseObject, "MANUFACTURER_NAME","");
	cJSON_AddStringToObject(responseObject, "MODEL_NAME","");
	cJSON_AddStringToObject(responseObject, "MANUFACTURER_DATE","");

	memset(string,0,sizeof(string));
	cJSON_PrintPreallocated(responseObject, string, sizeof(string), false);
	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS", string, strlen(string), "READ_PARA");
	cJSON_Delete(responseObject);

	memset(Read_SPIFFS_buffer,0,SPIFFS_QUE_ITEMSIZE);

	if (pdTRUE == xQueueReceive(SPIFFS_Que, (void*)Read_SPIFFS_buffer, 5000))
	{
		cJSON *in_JSON 	= cJSON_Parse(Read_SPIFFS_buffer);
		if (in_JSON == NULL)
		{
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,&s_Message_Rx_data, string);
			goto exit;
		}
		cJSON *FW_ver = cJSON_GetObjectItem(in_JSON, "FIRMWARE_VERSION");
		if((FW_ver != NULL) && (cJSON_IsString(FW_ver)))
			strcpy((char*)&ACTOR_SYSTEM_Para.FirmwareVer, FW_ver->valuestring);
		cJSON *Boot_ver = cJSON_GetObjectItem(in_JSON, "BOOTLOADER_VERSION");
		if((Boot_ver != NULL) && (cJSON_IsString(Boot_ver)))
			strcpy((char*)&ACTOR_SYSTEM_Para.BootFirmwareVer, Boot_ver->valuestring);
		cJSON *HW_ver = cJSON_GetObjectItem(in_JSON, "HARDWARE_VERSION");
		if((HW_ver != NULL) && (cJSON_IsString(HW_ver)))
			strcpy((char*)&ACTOR_SYSTEM_Para.HardwareVer, HW_ver->valuestring);
		cJSON *Device_id = cJSON_GetObjectItem(in_JSON, "DEVICE_ID");
			if((Device_id != NULL) && (cJSON_IsString(Device_id)))
			strcpy((char*)&ACTOR_SYSTEM_Para.DeviceId, Device_id->valuestring);
		cJSON *Manufac_name = cJSON_GetObjectItem(in_JSON, "MANUFACTURER_NAME");
			if((Manufac_name != NULL) && (cJSON_IsString(Manufac_name)))
			strcpy((char*)&ACTOR_SYSTEM_Para.Manufacturer_name_a8, Manufac_name->valuestring);
		cJSON *Manufac_date = cJSON_GetObjectItem(in_JSON, "MANUFACTURER_DATE");
				if((Manufac_name != NULL) && (cJSON_IsString(Manufac_date)))
				strcpy((char*)&ACTOR_SYSTEM_Para.Manufacturer_date_a8, Manufac_date->valuestring);
		cJSON *Model = cJSON_GetObjectItem(in_JSON, "MODEL_NAME");
			if((Model != NULL) && (cJSON_IsString(Model)))
			strcpy((char*)&ACTOR_SYSTEM_Para.Model_name_a8, Model->valuestring);
	}
	else
		Add_Response_msg("No response received from SPIFFS actor. Deleting the task...", &s_Message_Rx_data, string);
exit:
	SPIFFS_Handle = NULL;
	vTaskDelete(SPIFFS_Handle);  // Delete the task
}

static void Init_Post_File_Ack (AMessage_st* s_Message_Rx)
{
	// Create queue
	if(Post_File_Ack_Que == NULL)
	{
		Post_File_Ack_Que = xQueueCreateStatic(Post_File_Ack_QUE_COUNT, Post_File_Ack_QUEUE_ITEMSIZE, Post_File_Ack_pucQueueStorage, &Post_File_Ack_pxQueueBuffer);
		if (Post_File_Ack_Que == NULL) {
		#ifdef ENABLE_PRINT_MSG
			printf("Error in creating Post_File_Ack_Que\n ");
		#endif
		return;
		}
	}
		if(Post_File_Ack_Handle == NULL)
		{

			AMessage_st s_Message_Rx_data;
			char Rx_buffer_File_Ack[512];
			memset(Rx_buffer_File_Ack,0,sizeof(Rx_buffer_File_Ack));
			memcpy(&s_Message_Rx_data, s_Message_Rx, sizeof(AMessage_st));
			if((char*)s_Message_Rx_data.payload_p8 != NULL)
			{
				strncpy(Rx_buffer_File_Ack, (char*)s_Message_Rx_data.payload_p8,511);
			}
			s_Message_Rx_data.payload_p8 = (uint8_t*)Rx_buffer_File_Ack;
			strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
			strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
			strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);

			//	 Creating Monitor Task
			Post_File_Ack_Handle = xTaskCreateStaticPinnedToCore(
							Post_File_Ack_Task,                 // Task function
							"Post_File_Ack_Task",            // Task name
							POST_FILE_ACK_TASK_STACK_DEPTH,        // Stack size in words
							&s_Message_Rx_data,                    // Task parameters (not used here)
							POST_FILE_ACK_TASK_PRIORITY,                       // Task priority
							xPost_File_AckTaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xPOST_File_ACKTaskBuffer,             // Pointer to task control block
							0
		   );
			if (Post_File_Ack_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create SPIFFS read task\n");
#endif
		}
	}
}

static void Post_File_Ack_Task(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	char buffer[400] = {0}, str [400]= {0};
	while(1)
	{
		if (pdTRUE == xQueueReceive(Post_File_Ack_Que, (void*)buffer, portMAX_DELAY))
		{
			cJSON *in_JSON 	= cJSON_Parse(buffer);
			if (in_JSON == NULL)
			{
				printf("\n\n Post_File_Ack_Task buffer = %s \n\n", buffer);
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_File_Ack);
				goto exit;
			}
			cJSON *payload = cJSON_CreateObject();
			cJSON_AddStringToObject(payload, "deviceId", (char*)&ACTOR_SYSTEM_Para.DeviceId);

			cJSON *File_url = cJSON_GetObjectItem(in_JSON, "file_url");
			if((File_url != NULL) && (cJSON_IsString(File_url)))
				cJSON_AddStringToObject(payload, "file_url", File_url->valuestring);

			cJSON *File_path = cJSON_GetObjectItem(in_JSON, "file_path");
			if((File_path != NULL) && (cJSON_IsString(File_path)))
				cJSON_AddStringToObject(payload, "file_path", File_path->valuestring);

			cJSON *CRC = cJSON_GetObjectItem(in_JSON, "File_crc");
			if((CRC != NULL) && (cJSON_IsString(CRC)))
				cJSON_AddStringToObject(payload, "File_crc", CRC->valuestring);

			cJSON *File_size = cJSON_GetObjectItem(in_JSON, "file_size");
			if((File_size != NULL) && (cJSON_IsNumber(File_size)))
				cJSON_AddNumberToObject(payload, "file_size", File_size->valueint);

			cJSON *Message = cJSON_GetObjectItem(in_JSON, "Message");
			if((Message != NULL) && (cJSON_IsString(Message)))
				cJSON_AddStringToObject(payload, "Message", Message->valuestring);
			cJSON_Delete(in_JSON);
			cJSON *root = cJSON_CreateObject();
			cJSON_AddStringToObject(root, "URL", (char*) &ACTOR_SYSTEM_Para.File_Fetch_Ack_URL);
			cJSON_AddItemToObject(root, "HTTP_PAYLOAD", payload);
			cJSON_AddStringToObject(root, "API_KEY", (char*)&ACTOR_SYSTEM_Para.APIkey);
			cJSON_AddStringToObject(root, "DEVICEID", (char*)&ACTOR_SYSTEM_Para.DeviceId);
			cJSON_AddNumberToObject(root, "RETRY_PERIOD", ACTOR_SYSTEM_Para.deviceAnnouncePeriod); 
			memset(payLoadData_File_Ack,0,sizeof(payLoadData_File_Ack));
			cJSON_PrintPreallocated(root, payLoadData_File_Ack, sizeof(payLoadData_File_Ack), false);

			if(strlen((char*)&ACTOR_SYSTEM_Para.File_Fetch_Ack_URL) != 0)
				Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_File_Ack, strlen(payLoadData_File_Ack), "POST");
			else
				Add_Response_msg(" Error! 'FILE_FETCH_ACK' is not set. Hence, cannot post the file acknowledgement.",s_Message_Rx, payLoadData_Record);
			cJSON_Delete(root);  // It will free the memory allocated for out_JSON also
		}
	}

exit:
	Post_File_Ack_Handle = NULL;
	vTaskDelete(Post_File_Ack_Handle);  // Delete the task


}

static void sendLedState(const char *stateName, int duration, char *category) {
	char payLoad[100];
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

	cJSON_AddStringToObject(responseObject, "category", category);
    // Print the JSON object as a string
	memset(payLoad,0,sizeof(payLoad));
	cJSON_PrintPreallocated(responseObject, payLoad, sizeof(payLoad), false);
			
    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoad, strlen(payLoad), "SETSTATE");

    // Free the allocated memory
    cJSON_Delete(responseObject);
}

static void handle_waiting_for_ihub_response(State *state, cJSON *In_JSON, char *SSID, char *da_message, char *ping_dns_data, char *ping_gw_data, uint8_t *blink_state_val, AMessage_st *s_Message_Rx) {

	char IhubResponse[30] = {0};
	char IhubResponse_present = 0;

	memset(IhubResponse, 0, sizeof(IhubResponse));
	if(In_JSON != NULL)
	{
		cJSON *JSON_IhubResponse = cJSON_GetObjectItem(In_JSON, "IHUBRESP");
		if (cJSON_IsString(JSON_IhubResponse) && (JSON_IhubResponse->valuestring != NULL)) {
			if(strlen (JSON_IhubResponse->valuestring) != 0)
			{
				strcpy(IhubResponse, JSON_IhubResponse->valuestring);
				IhubResponse_present = 1;
			}
		}
	}

     if(IhubResponse_present == 1)
     {
		if (strcmp(IhubResponse, "CONNECTED") == 0)
		{
			*state = STATE_CONNECTED;
		}

		else if (strcmp(IhubResponse, "DISCONNECTED") == 0)
		{
			if(MonSrvConn_Handle == NULL)
			{
				*state = STATE_FAILED_11;
//				sendLedState("8-blink",-1);
#if defined(B480) || defined(B553)
				sendLedState("7-blink",-1, "iHUB");
#else
				cJSON *ERROR_NUM_item = cJSON_GetObjectItem(In_JSON, "ERROR_NUM_IHUB");
				if (cJSON_IsNumber(ERROR_NUM_item) && (ERROR_NUM_item != NULL))
				{
					if(ERROR_NUM_item->valueint == 1)
					{
						sendLedState("6-blink", -1, "iHUB");
					}
					else if((ERROR_NUM_item->valueint == 6) || (ERROR_NUM_item->valueint == 12))
					{
						sendLedState("5-blink", -1, "iHUB");
					}
					else
					{
						sendLedState("7-blink", -1, "iHUB");

					}
				}
				else
				{
					sendLedState("7-blink", -1, "iHUB");

				}
#endif

			}
			else
			{
				*state = STATE_CONNECTED;
			}
		}
	}
	else
	{
		if (ihub_connect_timeout_start == 0) {
			ihub_connect_timeout_start = time(NULL); // Start the timeout
			} else {
				// Check if the timeout duration has been exceeded
				if (difftime(time(NULL), ihub_connect_timeout_start) > IHUB_RESP_TIMEOUT_SECONDS) {
					// Timeout of 3 seconds reached, handle as failure
					if(MonSrvConn_Handle == NULL)
					{
						*state = STATE_FAILED_12;
//						sendLedState("8-blink",-1);
						sendLedState("7-blink", -1, "iHUB");
					}
					else
						*state = STATE_CONNECTED;
					ihub_connect_timeout_start = 0; // Reset timeout start
					Add_Response_msg("Response from IHUB actor is not received.", s_Message_Rx, payLoadData_Server);

				}
			}
	}
    if(MonSrvConn_Handle != NULL)
    {
    	*state = STATE_CONNECTED;
    	ihub_connect_timeout_start = 0; // Reset timeout start
    }
}

static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason)
{
	 // Read the raw register that stores the reset state
	    uint32_t reset_reason_raw = REG_READ(RTC_CNTL_RESET_STATE_REG);
	    unsigned int CPU0_Reset_Reason = 0;
	    unsigned int CPU1_Reset_Reason = 0;
	    CPU0_Reset_Reason = (unsigned int)(reset_reason_raw & 0x0000003F);
	    CPU1_Reset_Reason = (unsigned int)(reset_reason_raw & 0x00000FC0)>>6;
	    switch (CPU0_Reset_Reason) {
	        case ESP_RST_UNKNOWN:
	            strcpy(CPU0_RST_Reason,"Unknown");
	            break;
	        case ESP_RST_POWERON:
	        	strcpy(CPU0_RST_Reason,"Power-on");
	            break;
	        case ESP_RST_EXT:
	        	strcpy(CPU0_RST_Reason,"External reset");
	            break;
	        case ESP_RST_SW:
	        	strcpy(CPU0_RST_Reason,"Software reset");
	            break;
	        case ESP_RST_PANIC:
	        	strcpy(CPU0_RST_Reason,"Panic (exception)");
	            break;
	        case ESP_RST_INT_WDT:
	        	strcpy(CPU0_RST_Reason,"Interrupt Watchdog Timeout");
	            break;
	        case ESP_RST_TASK_WDT:
	        	strcpy(CPU0_RST_Reason,"Task Watchdog Timeout");
	            break;
	        case ESP_RST_WDT:
	        	strcpy(CPU0_RST_Reason,"Forced Watchdog");
	            break;
	        case ESP_RST_DEEPSLEEP:
	        	strcpy(CPU0_RST_Reason,"Wake from Deep Sleep");
	            break;
	        case ESP_RST_BROWNOUT:
	        	strcpy(CPU0_RST_Reason,"Brownout");
	            break;
	        case ESP_RST_SDIO:
	        	strcpy(CPU0_RST_Reason,"SDIO reset");
	            break;
	        default:
	        	strcpy(CPU0_RST_Reason,"Unhandled reason");
	            break;
	    }

	    switch (CPU1_Reset_Reason) {
	        case ESP_RST_UNKNOWN:
	        	strcpy(CPU1_RST_Reason,"Unknown");
	            break;
	        case ESP_RST_POWERON:
	        	strcpy(CPU1_RST_Reason,"Power-on");
	            break;
	        case ESP_RST_EXT:
	        	strcpy(CPU1_RST_Reason,"External reset");
	            break;
	        case ESP_RST_SW:
	        	strcpy(CPU1_RST_Reason,"Software reset");
	            break;
	        case ESP_RST_PANIC:
	        	strcpy(CPU1_RST_Reason,"Panic (exception)");
	            break;
	        case ESP_RST_INT_WDT:
	        	strcpy(CPU1_RST_Reason,"Interrupt Watchdog Timeout");
	            break;
	        case ESP_RST_TASK_WDT:
	        	strcpy(CPU1_RST_Reason,"Task Watchdog Timeout");
	            break;
	        case ESP_RST_WDT:
	        	strcpy(CPU1_RST_Reason,"Forced Watchdog");
	            break;
	        case ESP_RST_DEEPSLEEP:
	        	strcpy(CPU1_RST_Reason,"Wake from Deep Sleep");
	            break;
	        case ESP_RST_BROWNOUT:
	        	strcpy(CPU1_RST_Reason,"Brownout");
	            break;
	        case ESP_RST_SDIO:
	        	strcpy(CPU1_RST_Reason,"SDIO reset");
	            break;
	        default:
	        	strcpy(CPU1_RST_Reason,"Unhandled reason");
	            break;
	    }
}

static void Reset_Credentials(AMessage_st* s_Message_Rx)
{
	 cJSON *root = NULL;
	 // reset API key and device announce URL
	 memset((char*)&ACTOR_SYSTEM_Para.DeviceAnnounce_URL, 0, sizeof(ACTOR_SYSTEM_Para.DeviceAnnounce_URL));
	 memset((char*)&ACTOR_SYSTEM_Para.APIkey, 0, sizeof(ACTOR_SYSTEM_Para.APIkey));

	 // reset IHUB credentials
	 root = cJSON_CreateObject();
	 cJSON_AddStringToObject(root, "FILE_NAME", "A:/System/SYSTEM.json");
	 cJSON_AddStringToObject(root, "DEVICE_ANNOUNCE_URL","");
	 cJSON_AddStringToObject(root, "API_KEY","");
	 memset(payLoadData,0,sizeof(payLoadData));
	 cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	 cJSON_Delete(root);
	 Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");

	 // reset IHUB credentials
	 root = cJSON_CreateObject();
	 cJSON_AddStringToObject(root, "HOSTNAME","");
	 cJSON_AddStringToObject(root, "PRIMARY_KEY","");
	 memset(payLoadData,0,sizeof(payLoadData));
	 cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	 cJSON_Delete(root);
	 Send_CMD_To_Other_Actor(IHUB,"IHUB", payLoadData, strlen(payLoadData), "SET");

	 //reset WIFI credentials
	 root = cJSON_CreateObject();
	 cJSON_AddStringToObject(root, "SSID1","");
	 cJSON_AddStringToObject(root, "SSID2","");
	 cJSON_AddStringToObject(root, "SSID3","");
	 cJSON_AddStringToObject(root, "PASS1","");
	 cJSON_AddStringToObject(root, "PASS2","");
	 cJSON_AddStringToObject(root, "PASS3","");
	 memset(payLoadData,0,sizeof(payLoadData));
	 cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	 cJSON_Delete(root);
	 Send_CMD_To_Other_Actor(WIFI,"WIFI", payLoadData, strlen(payLoadData), "SET");
	 Add_Response_msg("Reseted the device credentials. Restarting the ESP..", s_Message_Rx, payLoadData);
	 vTaskDelay(1000 / portTICK_PERIOD_MS);
	 Restart_ESP_Xface(1);
}

static void epoch_to_date_time_m(date_time_t* date_time, uint64_t epoch)
{
   date_time->milSec = epoch%1000; epoch /= 1000;
   date_time->second = epoch%60; epoch /= 60;
   date_time->minute = epoch%60; epoch /= 60;
   date_time->hour   = epoch%24; epoch /= 24;
   unsigned long years = epoch/(365*4+1)*4; epoch %= 365*4+1;
   unsigned long year;
//   const unsigned short days[4][12] =
//   {
//      {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
//      { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
//      { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
//      {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
//   };
   for (year=3; year>0; year--)
   {
       if (epoch >= days[year][0])
           break;
   }
   unsigned long month;
   for (month=11; month>0; month--)
   {
       if (epoch >= days[year][month])
           break;
   }
   date_time->year  = years+year;
   date_time->month = month+1;
   date_time->date   = epoch-days[year][month]+1;
}


static void set_gmt_dst(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;

	char str[100] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
	}
	else
	{
		name_JSON = cJSON_GetObjectItem(in_JSON, "GMT_VAL");
		if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
		{
			gmt_val = name_JSON->valueint;
		}
		name_JSON = cJSON_GetObjectItem(in_JSON, "DST_VAL");
		if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
		{
			dst_val = name_JSON->valueint;
		}
		cJSON_Delete(in_JSON);
	}
}

static void periodic_timer_callback(void* arg)
{
		ACTOR_SYSTEM_Para.Device_Announce_F_delay_u32 = 0;		//0 Second
		device_ann_delay_count = 0;
		remaining_time_device_ann = 0;

		esp_timer_stop(periodic_timer);

}

static void periodic_timer_callback_5_Minute(void* arg)
{
	if(remaining_time_device_ann == 0)
	{
		esp_timer_stop(periodic_timer_5);
		return;
	}

	AMessage_st s_Message_Rx_data; // = heap_caps_calloc(sizeof(AMessage_st),1,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char payLoadData_rem_10[100]= {0};

	s_Message_Rx_data.Dest_ID_a8 = SYSTEM;
	s_Message_Rx_data.Src_ID_u8 = CONSOLE;
	s_Message_Rx_data.payload_p8 = (uint8_t*) payLoadData_rem_10;
	s_Message_Rx_data.payload_size = 0;
	strcpy((char*)s_Message_Rx_data.cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx_data.dest_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx_data.src_Actor_a8,"CONSOLE");

	char str[MESSAGE_SIZE] = { 0 };
	char payLoadData_rem_5[100];

	remaining_time_device_ann = remaining_time_device_ann - 300;	//5minute = 60x5 second
	sprintf(str, "Remaining Device announce delay in second is %lld, disconnect count is %d", remaining_time_device_ann, device_ann_delay_count);

	memset(payLoadData_rem_5,0,sizeof(payLoadData_rem_5));

	Add_Response_msg(str, &s_Message_Rx_data, payLoadData_rem_5);

	if(remaining_time_device_ann < 300)
	{
		esp_timer_stop(periodic_timer_5);
	}

}
