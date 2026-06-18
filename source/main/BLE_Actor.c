/*
 * BLE_Actor.c
 *
 *  Created on: 13-Oct-2023
 *  Author: Amruta and Priyanka
 */

#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "driver/uart.h"
#include "esp_bt.h"
#include "base64/base64.h"
#include "BLE_Actor.h"
#include "esp_log.h"
#include "string.h"
#include "stdint.h"
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include "esp_mac.h"
#include "freertos/FreeRTOSConfig.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_gatt.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "esp_app_format.h"
#include "esp_ota_ops.h"
#include "device_info/device_info.h"
#include "gatt_service.h"
#include "host/ble_hs.h"

#define RX_QUE_COUNT  10
#define MAX_BUFFER_SIZE 512

extern bool fg_BLE_Data_Rcvd;
extern uint8_t gatt_svr_thrpt_static_write[];
extern uint8_t gatt_svr_thrpt_static_Read[];

typedef struct {
    char actor[50];
    char buffer[50];
    char payload[MAX_BUFFER_SIZE];
} ParsedData;
PSRAM_ATTR_BSS ParsedData parsedData;
static const char * THIS_ACTOR = "BLE";
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char BLETXBuf[520];
PSRAM_ATTR_BSS static StackType_t xTaskStack [BLE_TASK_STACK_DEPTH], xTaskStack2[BLE_SUBTASK_STACK_DEPTH], xReadSys_TaskStack[BLEA_RD_SYS_PROP_TASK_STACK_DEPTH];
static char BleAppRespFg =0;
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_event[1024];
static bool ble_stack_initialized = false;
static bool ble_advertise_requested = false;

#define ROUND_FLT_TO_UINT16(x)	(uint16_t)((x) + 0.5)
#define LOW_BYTE(x)				(uint8_t)(x)
#define HIGH_BYTE(x)			(uint8_t)((x >> 8) & 0xFF)
#define NOTIFY_THROUGHPUT_PAYLOAD 500
#define MIN_REQUIRED_MBUF         2 /* Assuming payload of 500Bytes and each mbuf can take 292Bytes.  */
#define PREFERRED_MTU_VALUE       512
#define LL_PACKET_TIME            2120
#define LL_PACKET_LENGTH          251
#define MTU_DEF                   512
#define ADV_DATA_SIZE 225

#define DEVICE_DISCONNECTED 				0
#define DEVICE_CONNECTED 					1
#define BLE_DEVICE_NON_CONNECTABLE_MODE 	0
#define BLE_DEVICE_CONNECTABLE_MODE 		1

bool volatile timer_cb_done;
uint8_t ble_sma_msg_received;
uint8_t ble_sma_continuous;
ble_addr_t ble_mac_addr;

typedef struct {
           /**
            * The status of the connection attempt;
            *     o 0: the connection was successfully established.
            *     o BLE host error code: the connection attempt failed for
            *       the specified reason.
            */
           int status;

           /** The handle of the relevant connection. */
           uint16_t handle;
       } struct_ble_connect;
struct_ble_connect ble_conn_info = {-1,0};
BleStatesEnum CurrBleState = BLE_IDLE;

SemaphoreHandle_t mutex;

static bool 		FirstEntry_bool 	= false;
static bool 		FirstEntry_bool_ble 	= false;
uint8_t instance = 0;				// BLE execution loop
static TaskHandle_t  	Task_Handle		= NULL, read_sys_prop_Handle = NULL; //ble_wifiScanList_Handle = NULL;
TaskHandle_t BLE_Read_Write_Task_Handle = NULL; 	// BLE Task Handler
static QueueHandle_t 	msg_Rx_Queue	= NULL,ble_GetSysPropQueue	= NULL,ble_wifiScanListQueue	= NULL; 	// BLE Rx Queue
static StaticTask_t xBLETaskBuffer, xBLEReadwriteTaskBuffer, xReadSysTaskBuffer; //  ;  //// Declare a static task control block
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [RX_QUE_COUNT * sizeof(AMessage_st)], ble_GetSysPropQueStorage[BLEA_RD_SYS_PROP_TASK_QUEUE_LENGTH * BLEA_RD_SYS_PROP_TASK_QUEUE_ITEMSIZE];
static StaticQueue_t Monitor_pxQueueBuffer, ble_GetSysPropQueBuffer; //*ble_GetSysPropQueBuffer=NULL;

PSRAM_ATTR static uint8_t Device_Fw_Ver[30]; // DeviceID[30];; //,DeviceID[30];
static void init		(void *a, void *b); 							// Init actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					// Set or Change a parameter
static void get			(char *prop, char *val_a8);						// Get or Read a Parameter
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor		(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void BLE_Read_Write(void *pvParameters __attribute__((unused)));
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void ble_Server_Init(void);  		   // Init Bluetooth Server
void ble_Server_Deinit(void);
static void send_data_to_app(const uint8_t* payload,AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void read_sys_prop (void *pvParameters __attribute__((unused)));
static void initiate_read_sys_prop();
static void blehr_advertise(void);
static void sendLedState(const char *stateName, int duration);
static void Stop_BLE_Advertising(AMessage_st* s_Message_Rx);
static int blehr_gap_event(struct ble_gap_event *event, void *arg);
//-------------------------- Actor Parameters ------------------------------//

PSRAM_ATTR_BSS static struct actor_parameter {
	uint16_t    packet_size_u16;
	uint16_t	connectStatus_u16;
	uint8_t   	device_name_a8 	[BLE_BTNAME_LEN ];   	 // BLE_DEVICE_NAME
	uint8_t		UUID_a8[BLE_UUID_LEN];
	uint8_t     Advertising_mode_u8;
	uint8_t		connectionMode;
	uint64_t	BLE_Conn_status_time_u64;
	uint64_t	BLE_advert_status_time_u64;
	uint64_t	BLE_Conn_mode_time_u64;
	uint8_t 	device_online_status_u8;
	uint8_t     credential_status_u8;
} s_Para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &s_Para.packet_size_u16   			,   "PKT_SIZE"      		,   U_INT16, 	"R", 	"Packet size in bytes" },
	{ &s_Para.connectStatus_u16  			,   "CONN_STATUS"   		,   U_INT16, 	"R",  	"Connection status" },
	{ &s_Para.device_name_a8    			,   "DEVICE_NAME"   		,   STRING,  	"R",  	"Name of the device" },
    { &s_Para.UUID_a8           			,   "UUID"         			,   STRING,  	"RW", 	"UUID of the device" },
    { &s_Para.Advertising_mode_u8   		,   "ADVERT_MODE"   		,   U_INT8,  	"R",  	"Advertising mode" },
    { &s_Para.connectionMode        		,   "CONN_MODE"     		,   U_INT8, 	"RW", 	"Connection mode of the device" },
	{ &s_Para.BLE_Conn_status_time_u64   	, 	"BLE_CS_UPDATE_TIME"    , 	U_INT64, 	"R", 	"BLE connection status last updated time" },
	{ &s_Para.BLE_advert_status_time_u64   	, 	"BLE_AS_UPDATE_TIME"    , 	U_INT64, 	"R", 	"BLE advertising mode status last updated time" },
	{ &s_Para.BLE_Conn_mode_time_u64   		, 	"BLE_CM_UPDATE_TIME"    , 	U_INT64,	"R", 	"BLE connection mode last updated time" },
	{ &s_Para.device_online_status_u8		,   "ONLINE_STATUS" 		,   U_INT8,  	"RW",  	"device online status" },
	{ &s_Para.credential_status_u8  		,   "CRED_STATUS"   		,   U_INT8,  	"RW", 	"credential status" }
};

//------------------------- BLE Actor Resources -----------------------------//
#define DEBUG_ON  0

#if DEBUG_ON
#define EXAMPLE_DEBUG ESP_LOGI
#endif


#define PROFILE_NUM                 1
#define PROFILE_APP_IDX             0
#define ESP_APP_ID                  0x55
#define SVC_INST_ID                 0

static bool notify_state;
static uint16_t conn_handle;
static  char device_name[32] ;
static uint8_t blehr_addr_type;

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
			if(!(strcmp(prop[i].str_name, "CONN_MODE")))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				s_Para.BLE_Conn_mode_time_u64  = current_epos_sec;
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

static void get(char *str_prop, char *val_a8) {
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(str_prop, prop[i].str_name)) {
			switch (prop[i].type) {

			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
				break;

			case U_INT64:
				sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
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

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[40] = {0};
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

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
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
			if(!(strcmp(prop[i].str_name, "CONN_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "BLE NOT CONNECTED");
					break;

				case 1:
					strcpy(val_a8, "BLE CONNECTED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ADVERT_MODE")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "BLE NOT ADVERTISING");
					break;

				case 1:
					strcpy(val_a8, "BLE ADVERTISING STARTED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "CONN_MODE")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "BLE NON CONNECTABLE");
					break;

				case 1:
					strcpy(val_a8, "BLE CONNECTABLE");
					break;

				default:
					break;
				}
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
		cJSON_Delete(jsonObject);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the BLE actor.");
	cJSON_AddStringToObject(responseObject, "SET(string DEVICE_NAME)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "SEND_DATA(string DATA)", "Send data to BLE App.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "ADVERT_START()", "Start advertising the BLE device.");
	cJSON_AddStringToObject(responseObject, "ADVERT_STOP()", "Stop advertising the BLE device.");
	cJSON_AddStringToObject(responseObject, "CONN_DISABLE()", "Terminate BLE connection to the client device");
	cJSON_AddStringToObject(responseObject, "DEINIT()", "Deinit the actor. It delete the monitor task and its queue.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(responseObject);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void init(void *a, void *b)
{
	if(FirstEntry_bool)
	{
		return;
	}
	s_Para.Advertising_mode_u8 = 0;
	s_Para.device_online_status_u8 = 0;
	s_Para.credential_status_u8 = 0;

	if (msg_Rx_Queue == NULL) {
		msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
		if (msg_Rx_Queue == NULL) {
	#ifdef ENABLE_PRINT_MSG
			printf("BLE RX Queue is not created.\n ");
	#endif
		}
	}
	Task_Handle = xTaskCreateStaticPinnedToCore(
					monitor,                 // Task function
					"BLE Monitor",            // Task name
					BLE_TASK_STACK_DEPTH,        // Stack size in words
					NULL,                    // Task parameters (not used here)
					BLE_TASK_PRIORITY,                       // Task priority
					xTaskStack,              // Pointer to task stack (allocated in PSRAM)
					&xBLETaskBuffer,             // Pointer to task control block
					1
   );

	if (Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
		    printf("Failed to create task\n");
#endif
		    // Handle error
		}
				BLE_Read_Write_Task_Handle = xTaskCreateStaticPinnedToCore(
				BLE_Read_Write,                 // Task function
			    "BLE_Read_Write",            // Task name
				BLE_SUBTASK_STACK_DEPTH,        // Stack size in words
			    NULL,                    // Task parameters (not used here)
				BLE_SUBTASK_PRIORITY,                       // Task priority
			    xTaskStack2,              // Pointer to task stack (allocated in PSRAM)
			    &xBLEReadwriteTaskBuffer,             // Pointer to task control block
				1
			);

		if (BLE_Read_Write_Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			    printf("Failed to create task\n");
#endif
			}
	FirstEntry_bool = true;
    uint8_t mac[6];

    mutex = xQueueCreateMutex( ( ( uint8_t ) 1U ) );
   // Get the local BLE address
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    memset(s_Para.UUID_a8, 0, sizeof(s_Para.UUID_a8));
	const esp_app_desc_t *new_app_info = esp_app_get_description();
	for(int i= 0; i<strlen(new_app_info->version); i++)
	{
	   if(new_app_info->version[i]=='_')
		   Device_Fw_Ver[i] = '.';
	   else
		   Device_Fw_Ver[i] = new_app_info->version[i];
	}
    s_Para.connectStatus_u16 = DEVICE_DISCONNECTED;
	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
	s_Para.BLE_Conn_status_time_u64  = current_epos_sec;

    s_Para.packet_size_u16 = CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU;
	cJSON *responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/BLE.json");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
	initiate_read_sys_prop();
}// init

void parseInput(const char *input, ParsedData *parsedData) {
    // Find the actor name
    sscanf(input, "%[^.].", parsedData->actor);

    // Find the buffer data
    sscanf(input, "%*[^.].%[^(]", parsedData->buffer);

    char *start = strchr(input, '{');  // Find the first occurrence of '{'
       char *end = strrchr(input, '}');   // Find the last occurrence of '}'

       if (start && end && end > start) {
           int length = end - start + 1;
           strncpy(parsedData->payload, start, length);  // Copy the substring
       }
}
int Bledataparsing(char* val_p8)
{
	if(val_p8 == NULL)
		return -1;

	int ret = -1;
	esp_err_t err = esp_console_run_Custom(val_p8, &ret, THIS_ACTOR);
	if (err == ESP_ERR_NOT_FOUND) {
		printf("BLE: Unrecognized command\n");
	} else if (err == ESP_ERR_INVALID_ARG) {
		// command was empty
	} else if (err == ESP_OK && ret != ESP_OK) {
		printf("BLE: Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));

	} else if (err != ESP_OK) {
		printf("BLE: Internal error: %s\n", esp_err_to_name(err));
	}
	for(int i = 0; i< strlen(val_p8); i++)
	{
		if(val_p8[i] == '<')
			BleAppRespFg++;
	}
	return 0;
}

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*)(s_Message_Rx),portMAX_DELAY)){
//			printf("BLE msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("BLE DT = %s\n",s_Message_Rx->payload_p8);
//			}
						strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
			if (FirstEntry_bool == 0){
			init(0,0);

			}
			else if(FirstEntry_bool==1){
			Add_Response_msg("BLE initialization is done.", s_Message_Rx, payLoadData);
			}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) { // Get Actor Properties
				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SET")) // Set Actor Properties
			{
				u8Result =0;
				cJSON *root_JSON = NULL;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				    }
				else{
					head_JSON = name_JSON->child;
					if ((strcmp(head_JSON->string, "ONLINE_STATUS")) && (strcmp(head_JSON->string, "CRED_STATUS")) && (strcmp(head_JSON->string, "DEVICE_NAME")))
					{
						root_JSON  = cJSON_CreateObject();
						cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/BLE.json");
					}

				   // Loop through each key-value pair
				    do {
				        // Check if the value string is not NULL
				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
				    	{
							// Set the key-value pair
							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
							if(u8Result==1)
							{
								if(root_JSON != NULL)
								cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
							}
							else if(u8Result==2){
								sprintf(str,"'%s' is a read only property", head_JSON->string);
								 Add_Response_msg(str, s_Message_Rx, payLoadData);
							}
							else{
								if(root_JSON != NULL)
									cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
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
						cJSON_Delete(root_JSON);
						Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
				    }

				    // Free the parsed JSON
				    cJSON_Delete(name_JSON);
				    console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SEND_DATA")) {

				send_data_to_app(s_Message_Rx->payload_p8,s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "ADVERT_START"))
			 {
			 	ble_advertise_requested = true;
			 	ble_Server_Init();
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				s_Para.BLE_advert_status_time_u64  = current_epos_sec;
			 	if(s_Para.Advertising_mode_u8 ==0)
				{
					blehr_advertise();
					s_Para.Advertising_mode_u8 = 1;
				}
				Add_Response_msg("Server is started..",s_Message_Rx, payLoadData);
				sendLedState("BLE Mode",-1);
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "ADVERT_STOP"))
			 {
				if((s_Para.connectStatus_u16 ==1) || (s_Para.Advertising_mode_u8 ==1))
				{
					Stop_BLE_Advertising(s_Message_Rx);
				}
				else
				{
					ble_advertise_requested = false;
					Add_Response_msg("Server is already stopped..", s_Message_Rx, payLoadData);
				}
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "CONN_DISABLE"))
			 {
				if(ble_conn_info.status==0)
					ble_gap_terminate(ble_conn_info.handle,BLE_ERR_REM_USER_CONN_TERM);
				else
					Add_Response_msg("BLE not connected", s_Message_Rx, payLoadData);
			 }
			else
			{
				//BLE error message: invalid method
				Add_Response_msg("Invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}  //	monitor

void ble_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;

	if (FirstEntry_bool == false)
	{
		memset(&xBLETaskBuffer, 0, sizeof(xBLETaskBuffer));
		init(0,0);
	}
	
	uint8_t state = xQueueSend(msg_Rx_Queue, s_Message, QUE_DELAY);
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<BLE.ERROR(BLE RX Queue is full)\n");
		}
		else
		{
			printf("<BLE.ERROR(BLE RX Queue send unsuccessful)\n");
		}
	}
}//	ble_ConsoleWriteToActor

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  = NULL;
	if(response!=NULL)
	{
		newpointer  		= (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
		if (newpointer == NULL)
		{
			printf("Memory allocation failed\n");
			return;
		}
		strcpy((char*)newpointer, response);
		s_Message_Tx_new.payload_p8 	=  newpointer;
	}
	else
		s_Message_Tx_new.payload_p8 = NULL;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
} //	COP_add_response_to_COP_Tx_Queue

static char get_deviceStatus(void) {
    if (s_Para.device_online_status_u8 == 1) {
        return 'o';  // Device is online
    } else if (s_Para.device_online_status_u8 == 0 && s_Para.credential_status_u8 == 1) {
        return 'c';  // Device has credentials but is offline
    } else {
        return 'x';  // Device has empty credentials
    }
}
static void blehr_advertise(void) {
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields, scan_rsp;
    int rc;
    static uint8_t mfg_data[26] = {0}; // Max 26 byte
    char deviceStatus = get_deviceStatus();

    if (!ble_stack_initialized || !ble_hs_is_enabled() || ble_gap_adv_active()) {
        return;
    }

    // Format the firmware version string with the device status
    strcpy((char *)mfg_data, (char *)Device_Fw_Ver);
    sprintf((char *)mfg_data + strlen((char *)mfg_data), "(%c)", deviceStatus);

    //------------ Adv. data Logic End -------------------------------------//

    // Clear the advertisement fields structure
    memset(&fields, 0, sizeof(fields));

    // Set advertisement flags (General discoverable and BR/EDR unsupported)
    //    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    // Set advertisement flags (shortened)
    fields.flags = BLE_HS_ADV_F_DISC_GEN;
#ifdef ENABLE_PRINT_MSG
    printf("Flags set: 0x%02X\n", fields.flags);
#endif

    // Set manufacturer data (8 bytes)
    fields.mfg_data = mfg_data;
    fields.mfg_data_len = strlen((char *)mfg_data);	//sizeof(mfg_data);

    // We will not include the device name in the advertisement packet to save space
    fields.name = NULL;
    fields.name_len = 0;
    fields.name_is_complete = 0;

    // Set advertisement fields
    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        printf("Error setting advertisement fields, rc = %d\n", rc);
        return;
    }

    // Initialize advertisement parameters
    memset(&adv_params, 0, sizeof(adv_params));
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;  // Connectable undirected mode
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;  // General discoverable mode

    // Clear scan response fields
    memset(&scan_rsp, 0, sizeof(scan_rsp));
    // Set the full device name in the scan response
    scan_rsp.name = (uint8_t *)&s_Para.device_name_a8;
    scan_rsp.name_len = strlen((char*)&s_Para.device_name_a8);
    scan_rsp.name_is_complete = 1;
    // Set scan response fields
    rc = ble_gap_adv_rsp_set_fields(&scan_rsp);
    if (rc != 0) {
        return;
    }

    // Start advertising with the scan response
    rc = ble_gap_adv_start(blehr_addr_type, NULL, BLE_HS_FOREVER, &adv_params, blehr_gap_event, NULL);
    if (rc != 0) {
        return;
    }
}

//------------------------- Enter in function BLE Actor Methods -----------------------------//

static void
blehr_on_sync(void)
{
    int rc;
#if CONFIG_EXAMPLE_RANDOM_ADDR
    /* Generate a non-resolvable private address. */
    ble_app_set_addr();
#endif

    /* Make sure we have proper identity address set (public preferred) */
#if CONFIG_EXAMPLE_RANDOM_ADDR
    rc = ble_hs_util_ensure_addr(1);
#else
    rc = ble_hs_util_ensure_addr(0);
#endif

    rc = ble_hs_id_infer_auto(0, &blehr_addr_type);
    assert(rc == 0);

    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(blehr_addr_type, addr_val, NULL);

    /* Begin advertising only when explicitly requested */
    if (ble_advertise_requested) {
        blehr_advertise();
        s_Para.Advertising_mode_u8 = 1;
    }
}

static void blehr_on_reset(int reason)
{
   // MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
    printf("blehr_on_reset Resetting state; reason=%d\n", reason);
}

void blehr_host_task(void *param)
{
    /* This function will return only when nimble_port_stop() is executed */
    nimble_port_run();
    printf(">BLE.EVENT(Error! nimble_port_run() returned unexpectedly!)\n");

    nimble_port_freertos_deinit();
    // Optionally, restart the task or take appropriate action
    vTaskDelete(NULL);  // Safely delete the task to avoid further issues
}

static void ble_Server_Init(void)
{
    int rc;
    /* Initialize NVS — it is used to store PHY calibration data */
    esp_err_t ret; //= nvs_flash_init();
    if (ble_stack_initialized && ble_hs_is_enabled()) {
        return;
    }
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
#ifdef ENABLE_PRINT_MSG
        printf("\n Failed to init nimble %d \n",ret);
#endif
        return;
    }

    /* Initialize the NimBLE host configuration */
    ble_hs_cfg.sync_cb = blehr_on_sync;
    ble_hs_cfg.reset_cb = blehr_on_reset;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb,
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;

    rc = gatts_svr_init();
    if (rc != 0) {
#ifdef ENABLE_PRINT_MSG
            printf("error to gatts_svr_init\n");
#endif
            return;
        }

    /* Set the default device name */
    rc = ble_svc_gap_device_name_set(device_name);
    if (rc != 0) {
#ifdef ENABLE_PRINT_MSG
             printf("error to ble_svc_gap_device_name_set\n");
#endif
             return;
         }
    /* Start the task */
    nimble_port_freertos_init(blehr_host_task);
    FirstEntry_bool_ble=1;
    ble_stack_initialized = true;
}	//	ble_Init_Server

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char keyValue[200] = {0};
	char str[200]={0};
	//char BleRespData[2048] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n BLE s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  //  return 1;
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
				if (BleAppRespFg > 0) //BleAppRespFg ==1
				{
					if((s_Para.connectStatus_u16 == (uint16_t)DEVICE_CONNECTED) && (strlen(payLoadData) < CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU))  //strcmp(commandKey->valuestring,"STATE")!=0
					{
						memset(gatt_svr_thrpt_static_Read,0,sizeof((char*)gatt_svr_thrpt_static_Read));
						strcpy((char*)gatt_svr_thrpt_static_Read,(char*)payLoadData);  //BleRespData
						BleAppRespFg--;
						cJSON *responseObject = cJSON_CreateObject();
						cJSON_AddStringToObject(responseObject, "BLE_SEND_DATA", (char*)gatt_svr_thrpt_static_Read);

						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
						cJSON_Delete(responseObject);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
						console_send_responce_to_console_xface(s_Message_Rx);
						return;
					}
					else
					{
						BleAppRespFg--;
					}
				 }				
			}
			cJSON_Delete(in_JSON);
		}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				return;
		    }
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
			{
				cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (root != NULL)
				{
					// Iterate over the keys
					cJSON *currentItem = root->child;
					if(currentItem->valuestring != NULL)
					{
						if(!strcasecmp(currentItem->valuestring, "System/BLE.json"))
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
			}
			cJSON_Delete(in_JSON);
			return;
		}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL && cJSON_IsString(responseKey->child))
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICE_ID");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(ble_GetSysPropQueue != NULL)
							xQueueSend(ble_GetSysPropQueue, payLoadData, QUE_DELAY);
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"WIFI")==0)
			{
				in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
				cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
				if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_SCANLIST_RAM")==0))
				{
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (responseKey != NULL && cJSON_IsArray(responseKey->child))
					{
						cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "SSID");
						if((name_JSON != NULL) && (cJSON_IsArray(name_JSON)))
						{
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
							if(ble_wifiScanListQueue != NULL)
								xQueueSend(ble_wifiScanListQueue, payLoadData, QUE_DELAY);
						}
					}
				}
				cJSON_Delete(in_JSON);
				return;
			}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SPIFFS")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ_PARA")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICE_ID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(ble_GetSysPropQueue!=NULL)
					{
						uint8_t state = xQueueSend(ble_GetSysPropQueue, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
							if (state == errQUEUE_FULL)
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<BLE.ERROR(SPIFFS_Que is full)<CR>\n");
								#endif
							}
							else
							{
								#ifdef ENABLE_PRINT_MSG
									printf("<BLE.ERROR(SPIFFS_Que send unsuccessful)<CR>");
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
	return;
}
static void send_data_to_app(const uint8_t* payload,AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char str[100]={0};
	memset(BLETXBuf,0,sizeof(BLETXBuf));
	in_JSON 		= cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	 if(strcmp(in_JSON->child->string,"DATA")==0)
	{
		name_JSON 		= cJSON_GetObjectItem(in_JSON, "DATA");
		memset(gatt_svr_thrpt_static_Read,0,sizeof((char*)gatt_svr_thrpt_static_Read));
		strcpy((char*)gatt_svr_thrpt_static_Read,name_JSON->valuestring);
		Add_Response_msg((char*) gatt_svr_thrpt_static_Read,s_Message_Rx, payLoadData);
	}
	cJSON_Delete(in_JSON);
	return;
}

static int blehr_gap_event(struct ble_gap_event *event, void *arg)
{
	int rc;
	char str[100] = {0}, data_buffer[512] = {0};  //(char*) heap_caps_calloc(100, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	AMessage_st s_Message_Tx_new;
	s_Message_Tx_new.Dest_ID_a8 = BLE;
	s_Message_Tx_new.Src_ID_u8 =SYSTEM;
	strcpy((char*)s_Message_Tx_new.dest_Actor_a8,"BLE");
	strcpy((char*)s_Message_Tx_new.src_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Tx_new.cmdFun_a8,"EVENT");
	s_Message_Tx_new.payload_p8 = (uint8_t*) data_buffer;

   switch (event->type)
   {
    case BLE_GAP_EVENT_CONNECT:
        /* A new connection was established or a connection attempt failed */
		if(event->connect.status == 0){
			s_Para.Advertising_mode_u8 = 0;
			struct timeval currentTime;
			_gettimeofday_r(NULL, &currentTime, NULL);
			uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
			s_Para.BLE_advert_status_time_u64  = current_epos_sec;
			cJSON *root = cJSON_CreateObject();
			cJSON_AddStringToObject(root, "CONNECTION_STATUS","CONNECTED");
			
			memset(payLoadData_event,0,sizeof(payLoadData_event));
			cJSON_PrintPreallocated(root, payLoadData_event, sizeof(payLoadData_event), false);
			cJSON_Delete(root);
			strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_event);
			console_send_responce_to_console_xface(&s_Message_Tx_new);
			
			ble_conn_info.handle = event->connect.conn_handle;
			ble_conn_info.status = event->connect.status;
			s_Para.connectStatus_u16 = DEVICE_CONNECTED;
			struct timeval currentTime1;
			_gettimeofday_r(NULL, &currentTime1, NULL);
			uint64_t current_epos_sec1 = (uint64_t) (currentTime1.tv_sec * 1000L) + (uint64_t) (currentTime1.tv_usec / 1000L);
			s_Para.BLE_Conn_status_time_u64  = current_epos_sec1;
			Send_CMD_To_Other_Actor(LED, "LED", "0", 0, "BLE_CONNECTED");
		}
		else{
			cJSON *root = cJSON_CreateObject();
			cJSON_AddStringToObject(root, "CONNECTION_STATUS","DISCONNECTED..");
			memset(payLoadData_event,0,sizeof(payLoadData_event));
			cJSON_PrintPreallocated(root, payLoadData_event, sizeof(payLoadData_event), false);
			cJSON_Delete(root);
			strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_event);
			console_send_responce_to_console_xface(&s_Message_Tx_new);

			ble_conn_info.handle = event->connect.conn_handle;
			ble_conn_info.status = event->connect.status;
			if(s_Para.connectStatus_u16 == DEVICE_CONNECTED)
			{
				Stop_BLE_Advertising(&s_Message_Tx_new);
			}
			s_Para.connectStatus_u16 = DEVICE_DISCONNECTED;
			struct timeval currentTime;
			_gettimeofday_r(NULL, &currentTime, NULL);
			uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
			s_Para.BLE_Conn_status_time_u64  = current_epos_sec;
			Send_CMD_To_Other_Actor(LED, "LED", "0", 0, "STOP_BLE_LED");  //BLE_DISCONNECTED
		}


        rc = ble_att_set_preferred_mtu(CONFIG_BT_NIMBLE_ATT_PREFERRED_MTU);   //PREFERRED_MTU_VALUE

        if (event->connect.status != 0 && ble_advertise_requested) {
            /* Connection failed; resume advertising */
        	blehr_advertise();
        	s_Para.Advertising_mode_u8 = 1;
        }

        rc = ble_hs_hci_util_set_data_len(event->connect.conn_handle,
                                          LL_PACKET_LENGTH,
                                          LL_PACKET_TIME);
        if (rc != 0) {
        	 Add_Response_msg("Set packet length failed", &s_Message_Tx_new, payLoadData_event);
        }

        conn_handle = event->connect.conn_handle;
        break;

    case BLE_GAP_EVENT_DISCONNECT:
		cJSON *root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "CONNECTION_STATUS","DISCONNECTED");
		memset(payLoadData_event,0,sizeof(payLoadData_event));
		cJSON_PrintPreallocated(root, payLoadData_event, sizeof(payLoadData_event), false);
		cJSON_Delete(root);
		strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_event);
		console_send_responce_to_console_xface(&s_Message_Tx_new);

		ble_conn_info.handle = event->connect.conn_handle;
		ble_conn_info.status = event->connect.status;
		Send_CMD_To_Other_Actor(LED, "LED", "0", 0, "STOP_BLE_LED");  //BLE_DISCONNECTED
		if(s_Para.connectStatus_u16 == DEVICE_CONNECTED)
		{
			Stop_BLE_Advertising(&s_Message_Tx_new);
		}
		s_Para.connectStatus_u16 = DEVICE_DISCONNECTED;//not
		struct timeval currentTime;
		_gettimeofday_r(NULL, &currentTime, NULL);
		uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		s_Para.Advertising_mode_u8 = 0;
		s_Para.BLE_advert_status_time_u64  = current_epos_sec;
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
    	if (ble_advertise_requested) {
    		blehr_advertise();
    		s_Para.Advertising_mode_u8 = 1;
    	}
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
#ifdef ENABLE_PRINT_MSG
        printf("subscribe event; cur_notify=%d\n value handle; "
                "val_handle=%d\n",
                event->subscribe.cur_notify, hrs_hrm_handle);
#endif
        if (event->subscribe.attr_handle == hrs_hrm_handle) {
            notify_state = event->subscribe.cur_notify;
        } else if (event->subscribe.attr_handle != hrs_hrm_handle) {
            notify_state = event->subscribe.cur_notify;
        }
        ESP_LOGI("BLE_GAP_SUBSCRIBE_EVENT", "conn_handle from subscribe=%d", conn_handle);
        break;

    case BLE_GAP_EVENT_MTU:
    	sprintf(str,"mtu update event, MTU value = %d ",event->mtu.value);
    	Add_Response_msg(str,&s_Message_Tx_new,payLoadData_event);
    	 rc = ble_att_set_preferred_mtu(event->mtu.value);   //PREFERRED_MTU_VALUE
		if (rc != 0)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Failed to set preferred MTU; rc = %d\n", rc);
#endif
		}
        break;
    default: break;
    }
    return 0;
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
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
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
				if(!(strcmp(element->valuestring, "CONN_STATUS")))
				{
					switch (val_p8[0])
					{
					case '0':
						strcpy(val_p8, "BLE NOT CONNECTED");
						break;

					case '1':
						strcpy(val_p8, "BLE CONNECTED");
						break;

					default:
						break;
					}
				}
				if(!(strcmp(element->valuestring, "ADVERT_MODE")))
				{
					switch (val_p8[0])
					{
					case '0':
						strcpy(val_p8, "BLE NOT ADVERTISING");
						break;

					case '1':
						strcpy(val_p8, "BLE ADVERTISING STARTED");
						break;

					default:
						break;
					}
				}
				if(!(strcmp(element->valuestring, "CONN_MODE")))
				{
					switch (val_p8[0])
					{
					case '0':
						strcpy(val_p8, "BLE NON CONNECTABLE");
						break;

					case '1':
						strcpy(val_p8, "BLE CONNECTABLE");
						break;

					default:
						break;
					}
				}
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

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void BLE_Read_Write(void *pvParameters __attribute__((unused)))
{
	AMessage_st s_Message_Rx;
	uint8_t val_p8[512]  = {0}, data_buffer[600] = {0};
	char Rx_buffer[600] = {0};
	s_Message_Rx.Dest_ID_a8 = BLE;
	s_Message_Rx.Src_ID_u8 =SYSTEM;
	strcpy((char*)s_Message_Rx.dest_Actor_a8,"BLE");
	strcpy((char*)s_Message_Rx.src_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx.cmdFun_a8,"EVENT");
	s_Message_Rx.payload_p8 = (uint8_t*) data_buffer;
	while(1)
	{
		memset(val_p8,0,sizeof(val_p8));
		ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
		if (fg_BLE_Data_Rcvd == true)
		{
			fg_BLE_Data_Rcvd=false;
			memset(val_p8,0,sizeof(val_p8));
			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "RECEIVED_DATA", (char*)gatt_svr_thrpt_static_write);

			memset(Rx_buffer,0,sizeof(Rx_buffer));
			cJSON_PrintPreallocated(responseObject, Rx_buffer, sizeof(Rx_buffer), false);
			strcpy((char*)s_Message_Rx.payload_p8, Rx_buffer);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(&s_Message_Rx);

			sprintf((char*)val_p8,"\"%s\"",gatt_svr_thrpt_static_write);
			if(strncmp((char*)val_p8, "\"<",2)==0)  // entered BLE data is command and send it to console for execution
			{
				Bledataparsing((char*)val_p8+1); ////val[0] = " - neglect it
			}
		}
	}
}


static void initiate_read_sys_prop()
{
	if(ble_GetSysPropQueue == NULL)
	{
		ble_GetSysPropQueue = xQueueCreateStatic(BLEA_RD_SYS_PROP_TASK_QUEUE_LENGTH, BLEA_RD_SYS_PROP_TASK_QUEUE_ITEMSIZE, ble_GetSysPropQueStorage, &ble_GetSysPropQueBuffer);
	}
	if (ble_GetSysPropQueue == NULL)
	{
#ifdef ENABLE_PRINT_MSG
		printf("\nUnable to create BLE SYS PROP Queue \n");
#endif
	}

	if(read_sys_prop_Handle == NULL){
		read_sys_prop_Handle = xTaskCreateStaticPinnedToCore(
						read_sys_prop,                 // Task function
						"read_sys_prop",            // Task name
						BLEA_RD_SYS_PROP_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						BLEA_RD_SYS_PROP_TASK_PRIORITY,                       // Task priority
						xReadSys_TaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xReadSysTaskBuffer,             // Pointer to task control block
						1
		);
		if (read_sys_prop_Handle == NULL)
		{
			#ifdef ENABLE_PRINT_MSG
				printf("Failed to create task fot SYSTEM.STATE method.\n");
			#endif
		}
	}
}

static void read_sys_prop(void *pvParameters __attribute__((unused)))
{
		char buffer[200] = {"\0"};//,str[255] = {0};
		char DeviceID[50] = {0}, ManufacturerName[20] = {0}, ModelName[20] = {0};
		static char l_device_name[60] = {0}, Prop_payload[100] = {0}; // 29 bytes max
		uint8_t mac[6] = {0};
		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME",(char*)Device_File);
		cJSON_AddStringToObject(responseObject, "DEVICE_ID","");
		cJSON_AddStringToObject(responseObject, "MANUFACTURER_NAME","");
		cJSON_AddStringToObject(responseObject, "MODEL_NAME","");
		memset(Prop_payload,0,sizeof(Prop_payload));
		cJSON_PrintPreallocated(responseObject, Prop_payload, sizeof(Prop_payload), false);
		Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS", Prop_payload, strlen(Prop_payload), "READ_PARA");
		cJSON_Delete(responseObject);

		if (pdTRUE == xQueueReceive(ble_GetSysPropQueue, (void*)buffer, portMAX_DELAY))
		{
	        cJSON *in_JSON 	= cJSON_Parse(buffer);
	       	if (in_JSON == NULL)
	       	{
#ifdef ENABLE_PRINT_MSG
	       		printf("\n\n Invalid Json input\n\n");
#endif
	       		goto exit;
	       	}
	       	else
	       	{
				cJSON *Device_id = cJSON_GetObjectItem(in_JSON, "DEVICE_ID");
				if((Device_id != NULL) && (cJSON_IsString(Device_id)))
				{
					strcpy((char*)DeviceID, Device_id->valuestring);

					cJSON *Manufacture_name = cJSON_GetObjectItem(in_JSON, "MANUFACTURER_NAME");
					if((Manufacture_name != NULL) && (cJSON_IsString(Manufacture_name)))
					{
						strcpy((char*)ManufacturerName, Manufacture_name->valuestring);
					}
					cJSON *Model_name = cJSON_GetObjectItem(in_JSON, "MODEL_NAME");
					if((Model_name != NULL) && (cJSON_IsString(Model_name)))
					{
						strcpy((char*)ModelName, Model_name->valuestring);
					}

				    // Extract the last 4 characters of DeviceID
				    char last4Chars[5]; // 4 characters + null terminator
				    strncpy(last4Chars, (char*)DeviceID + strlen((char*)DeviceID) - 4, 4);
				    last4Chars[4] = '\0'; // Null-terminate the string

				    // Convert last4Chars to integer if needed
				    int last4Value = (int)strtol(last4Chars, NULL, 16);

				    // Assuming you want to use this value to replace mac[4] and mac[5]
				    mac[4] = (last4Value >> 8) & 0xFF; // Higher 8 bits
				    mac[5] = last4Value & 0xFF;        // Lower 8 bits

				    // Format the device name using the last 4 bytes
				    sprintf(l_device_name, "%s %s:%02X%02X", ManufacturerName, ModelName, mac[4], mac[5]);
				    strcpy((char*)&s_Para.device_name_a8, l_device_name);
				    strcpy(device_name,(char*)&s_Para.device_name_a8);
				}
	       	}
	       	cJSON_Delete(in_JSON);
		}

exit:
		read_sys_prop_Handle = NULL;
		vTaskDelete(read_sys_prop_Handle);  // Delete the task
}


void ble_Server_Deinit(void) {
    ble_advertise_requested = false;

    if (!ble_stack_initialized) {
        return;
    }

    // Deinitialize the NimBLE host stack
    ble_hs_deinit();

    // Stop the NimBLE port
    nimble_port_stop();

    // Deinitialize the NimBLE port
    nimble_port_deinit();
    ble_stack_initialized = false;
}

static void sendLedState(const char *stateName, int duration) {
	char payLoad[100];
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    // Print the JSON object as a string
	memset(payLoad,0,sizeof(payLoad));
	cJSON_PrintPreallocated(responseObject, payLoad, sizeof(payLoad), false);

    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoad, strlen(payLoad), "SETSTATE");

    // Free the allocated memory
    cJSON_Delete(responseObject);
}


static void Stop_BLE_Advertising(AMessage_st* s_Message_Rx)
{
	char payLoad[100] = {0}, str[150] = {0};
	int rc = 0;

	ble_advertise_requested = false;
	s_Para.connectStatus_u16 = 0;
	s_Para.Advertising_mode_u8 = 0;

	if (!ble_stack_initialized || !ble_hs_is_enabled()) {
		Add_Response_msg("Server is already stopped..", s_Message_Rx, payLoad);
		return;
	}

	if (ble_conn_info.status == 0) {
		rc = ble_gap_terminate(ble_conn_info.handle, BLE_ERR_REM_USER_CONN_TERM);
		if ((rc != 0) && (rc != BLE_HS_EALREADY)) {
			sprintf(str, "Error! failed to disconnect BLE.. Error code=%d", rc);
			Add_Response_msg(str, s_Message_Rx, payLoad);
			s_Para.connectStatus_u16 = 1;
			return;
		}
	} else if (ble_gap_adv_active()) {
		rc = ble_gap_adv_stop();
		if ((rc != 0) && (rc != BLE_HS_EALREADY)) {
			sprintf(str, "Error! failed to stop BLE advertising.. Error code=%d", rc);
			Add_Response_msg(str, s_Message_Rx, payLoad);
			s_Para.Advertising_mode_u8 = 1;
			return;
		}
	}

	Add_Response_msg("Server is stopped..",s_Message_Rx, payLoad);
	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
	s_Para.BLE_Conn_status_time_u64  = current_epos_sec;
	Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "STOP_BLE_LED");
}
//--------------------------- BLE Actor Methods Ends ------------------------------//
