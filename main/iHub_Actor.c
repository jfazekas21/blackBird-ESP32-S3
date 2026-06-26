/*
 *iHub_Actor.c
 *
 *  Created on: Sep 13, 2023
 *  Author: Priyanka
 */

/**
 * IoT Hub Configuration File
 *
 * This file contains configuration settings for the IoT Hub, which serves as the central
 * communication and data processing hub for our Internet of Things (IoT) ecosystem.
 *
 * Contents:
 * - Azure IoT Hub connection string
 * - Device credentials and authentication keys
 * - IoT Hub end point URLs
 * - Routing rules for data ingestion and processing
 *
 * proper IoT device connectivity and data flow within the IoT system.
 *
 */


#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "cJSON.h"
#include "iHub_Actor.h"

//////////////////////////////////////////
#include "multi_heap.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include <stdarg.h>
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "demo_config.h"
#include "wifi_Actor.h"
#include <stdbool.h>

/* Azure Provisioning/IoT Hub library includes */
#include "azure_iot_hub_client.h"
#include "azure_iot_hub_client_properties.h"
#include "azure_iot_provisioning_client.h"

/* Azure JSON includes */
#include "azure_iot_json_reader.h"
#include "azure_iot_json_writer.h"
#include "azure_iot_message.h"

/* Exponential backoff retry include. */
#include "backoff_algorithm.h"

/* Transport interface implementation include header for TLS. */
#include "transport_tls_socket.h"

/* Crypto helper header. */
#include "azure_sample_crypto.h"

/* Demo Specific configs. */
#include "demo_config.h"

/* Demo Specific Interface Functions. */
#include "azure_sample_connection.h"

/* Data Interface Definition */
#include "sample_azure_iot_pnp_data_if.h"

#include "azure_iot_mqtt.h"
#include "esp_task_wdt.h"
#include <math.h>
#include <sys/socket.h>
//////////////////////////////////////////
//#define TASK_STACK_DEPTH1 (12*1024)

#define NR_OF_IP_ADDRESSES_TO_WAIT_FOR     1

#define SNTP_SERVER_FQDN                                "pool.ntp.org"
/**
 * @brief Timeout for receiving CONNACK packet in milliseconds.
 */
#define sampleazureiotCONNACK_RECV_TIMEOUT_MS                 ( 10 * 1000U )
/**
 * @brief Wait timeout for subscribe to finish.
 */
#define CONNECTIVITY_BOOT_GRACE_US      (8 * 60 * 1000000LL)
#define CONNECTIVITY_DISCONNECT_TIMEOUT_US (20 * 60 * 1000000LL)
#define CONNECTIVITY_SUPERVISOR_INTERVAL_US (10 * 1000000LL)
#define CONNECTIVITY_ACTIVITY_TIMEOUT_US (5 * 60 * 1000000LL)

/*-----------------------Newly added------------------------------------*/
/* Compile time error for undefined configs. */
#if !defined( democonfigHOSTNAME ) && !defined( democonfigENABLE_DPS_SAMPLE )
    #error "Define the config democonfigHOSTNAME by following the instructions in file demo_config.h."
#endif

#if !defined( democonfigENDPOINT ) && defined( democonfigENABLE_DPS_SAMPLE )
    #error "Define the config dps endpoint by following the instructions in file demo_config.h."
#endif

#ifndef democonfigROOT_CA_PEM
    #error "Please define Root CA certificate of the IoT Hub(democonfigROOT_CA_PEM) in demo_config.h."
#endif

#if defined( democonfigDEVICE_SYMMETRIC_KEY ) && defined( democonfigCLIENT_CERTIFICATE_PEM )
    #error "Please define only one auth democonfigDEVICE_SYMMETRIC_KEY or democonfigCLIENT_CERTIFICATE_PEM in demo_config.h."
#endif

#if !defined( democonfigDEVICE_SYMMETRIC_KEY ) && !defined( democonfigCLIENT_CERTIFICATE_PEM )
    #error "Please define one auth democonfigDEVICE_SYMMETRIC_KEY or democonfigCLIENT_CERTIFICATE_PEM in demo_config.h."
#endif
/*-----------------------------------------------------------*/

/**
 * @brief The maximum number of retries for network operation with server.
 */
#define sampleazureiotRETRY_MAX_ATTEMPTS                      ( 10U )

/**
 * @brief The maximum back-off delay (in milliseconds) for retrying failed operation
 *  with server.
 */
#define sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS              ( 30000U )

/**
 * @brief The base back-off delay (in milliseconds) to use for network operation retry
 * attempts.
 */
#define sampleazureiotRETRY_BACKOFF_BASE_MS                   ( 500U )

/**
 * @brief Phase 1: maximum number of in-task IHUB reconnect cycles attempted on a
 * ProcessLoop / stale-connection failure before falling back to the full teardown
 * (which lets the monitor escalate to a stack reset).
 *
 * Each cycle calls Connect_IHUB(), which itself performs up to
 * sampleazureiotRETRY_MAX_ATTEMPTS TLS back-off retries, so a small value here
 * keeps total recovery time bounded while still being robust to transient drops.
 */
#define sampleazureiotINTASK_RECONNECT_MAX_ATTEMPTS          ( 3U )

///**
// * @brief Timeout for receiving CONNACK packet in milliseconds.
// */
//#define sampleazureiotCONNACK_RECV_TIMEOUT_MS                 ( 20 * 1000U )

/**
 * @brief Date-time to use for the model id
 */
#define sampleazureiotDATE_TIME_FORMAT                        "%Y-%m-%dT%H:%M:%S.000Z"

/**
 * @brief Time in ticks to wait between each cycle of the demo implemented
 * by prvMQTTDemoTask().
 */
#define sampleazureiotDELAY_BETWEEN_DEMO_ITERATIONS_TICKS     ( pdMS_TO_TICKS( 5000U ) )

/**
 * @brief Timeout for MQTT_ProcessLoop in milliseconds.
 */
#if defined(B394)
/* B394: shorter timeout so D2C (e.g. digital input events) is sent within ~2?3 s when connected */
#define sampleazureiotPROCESS_LOOP_TIMEOUT_MS                 ( 2000U )
#else
#define sampleazureiotPROCESS_LOOP_TIMEOUT_MS                 ( 60000U )
#endif

/**
 * @brief Delay (in ticks) between consecutive cycles of MQTT publish operations in a
 * demo iteration.
 *
 * Note that the process loop also has a timeout, so the total time between
 * publishes is the sum of the two delays.
 */
#define sampleazureiotDELAY_BETWEEN_PUBLISHES_TICKS          ( pdMS_TO_TICKS( 500U ) ) // 200  ( pdMS_TO_TICKS( 2000U ) )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define sampleazureiotTRANSPORT_SEND_RECV_TIMEOUT_MS          ( 5000U )

/**
 * @brief Transport timeout in milliseconds for transport send and receive.
 */
#define sampleazureiotProvisioning_Registration_TIMEOUT_MS    ( 3 * 1000U )

/**
 * @brief Wait timeout for subscribe to finish.
 */
#define sampleazureiotSUBSCRIBE_TIMEOUT                       ( 10 * 1000U )

#define PING_INTERVAL_MICROSECONDS (60 * 1000000) // 60 seconds in microseconds
#define D2C_INTERVAL_SECONDS (10* 60) // 10 minute in seconds
#define ucCommandResponsePayloadBufferSize  4096
struct NetworkContext
{
    void * pParams;
};

AzureIoTHubClient_t xAzureIoTHubClient;
NetworkContext_t xNetworkContext;
NetworkCredentials_t xNetworkCredentials;
AzureIoTTransportInterface_t xTransport;
TlsTransportParams_t xTlsTransportParams;
AzureIoTHubClientOptions_t xHubOptions;
char 	DeviceId[64]= {0};
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadData_IHUB[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Twin_msg_payload[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char D2CPayload[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Reported_Properties[2048];
PSRAM_ATTR_BSS static char CONNECTIVITY_INFO_DATA[512];
PSRAM_ATTR_BSS static char MethodPayload[MAX_JSON_PAYLOAD_BYTES/2];  //1000
PSRAM_ATTR_BSS static char command_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char line[COMMAND_LEN];
PSRAM_ATTR_BSS static char g_jsonBuffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char IHUB_buffer[1024];
/* Telemetry buffers */
PSRAM_ATTR_BSS static uint8_t ucScratchBuffer[MAX_JSON_PAYLOAD_BYTES];  //2 * 1024
/* Command buffers */

/* Reported Properties buffers */
PSRAM_ATTR_BSS static uint8_t ucReportedPropertiesUpdate[ 1024 ];//[380]
static uint32_t ulReportedPropertiesUpdateLength;
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Task_data_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char IHUB_str[1024]; //Delete_task_flag = 0;
PSRAM_ATTR_BSS static char IHUB_str1[512];
PSRAM_ATTR_BSS static char ucCommandResponsePayloadBuffer[ucCommandResponsePayloadBufferSize];
static int64_t ihubTimeActive = 0;
static uint8_t deinit_Flag = 0;

/**
 * @brief Connect to endpoint with reconnection retries.
 *
 * If connection fails, retry is attempted after a timeout.
 * Timeout value will exponentially increase until maximum
 * timeout value is reached or the number of attempts are exhausted.
 *
 * @param pcHostName Hostname of the endpoint to connect to.
 * @param ulPort Endpoint port.
 * @param pxNetworkCredentials Pointer to Network credentials.
 * @param pxNetworkContext Point to Network context created.
 * @return uint32_t The status of the final connection attempt.
 */
static uint32_t prvConnectToServerWithBackoffRetries( const char * pcHostName,
                                                      uint32_t ulPort,
                                                      NetworkCredentials_t * pxNetworkCredentials,
                                                      NetworkContext_t * pxNetworkContext );
/*-----------------------------------------------------------*/

/**
 * @brief Static buffer used to hold MQTT messages being sent and received.
 */
PSRAM_ATTR static uint8_t ucMQTTMessageBuffer[ 5* democonfigNETWORK_BUFFER_SIZE  ];  //democonfigNETWORK_BUFFER_SIZE

/*-----------------------------------------------------------*/

uint8_t u8ledstatusflag;
char  Delete_task_flag = 0;  //DeviceID[30]= {0},
static bool s_is_connected_to_internet = false;
static StaticQueue_t Monitor_pxQueueBuffer , Twin_pxQueueBuffer , IHUB_pxQueueBuffer, Twin_Command_pxQueueBuffer ;

static bool ihub_connected_now = false;
static int64_t boot_time_us = 0;
static int64_t disconnect_start_us = 0;
static int64_t last_successful_activity_us = 0;

static uint64_t get_current_time_ms();
static void periodic_timer_callback(void* arg);
static void twin_additional_data(AMessage_st* s_Message_Rx);
static void merge_connectivity_info_response(cJSON *responseKey);
static time_t convert_to_epoch(int year, int month, int day, int hour, int minute, int second);
static int parse_datetime(const char* datetime, int* year, int* month, int* day, int* hour, int* minute);
void Azure_RefreshToken_IfExpiring(AMessage_st* s_Message_Rx_new);
uint16_t u16Reset_Device_Count = 0;
static const esp_timer_create_args_t periodic_timer_args_new = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"
};
static esp_timer_handle_t periodic_timer = NULL;

static void connectivity_supervisor_init(void);
static void connectivity_supervisor_mark_connected(void);
static void connectivity_supervisor_mark_disconnected(void);
static bool connectivity_supervisor_is_armed(int64_t now);

typedef struct iHub_registers {
	uint8_t conn_status;
	uint8_t Enable_WDT;
	char DeviceID[DeviceID_LEN];  				// Device ID, Made using WiFi MAC
	char HostName[HostName_LEN];                //// Host Name
	char PrimaryKey[ConnectionStr_LEN];  	// Azure IoT Hub Connection String
	uint64_t IHUB_status_time_u64;
	char IHUB_disconnect_reason[100];
	uint32_t D2C_interval_second_u32;
	uint8_t ever_connected_since_boot;
}iHub_registers;

void vStartDemoTask(AMessage_st* s_Message_Rx);
static void prvAzureDemoTask( void * pvParameters);
static void TwinCommandProcessTask(void *pvParameters __attribute__((unused)));
uint64_t ullGetUnixTime( void );
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void ErrortoStringConverter(AzureIoTResult_t xResult, char* Error_str);
static void Process_Post_Method_Response(AMessage_st* s_Message_Rx, char* directmethodpayload);
static void process_Twin_data(const char *json_data);
static void updateReportedPropertiesWithConnectivityInfo();
static void sendLedState(const char *stateName, int duration, char *category);
/*-----------------------------------------------------------*/
static const char * THIS_ACTOR 	= "IHUB";
static int FirstiHubEntry = false;
static void init(void *a, void *b);
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);					//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Device_Info(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void Deinit_Actor(AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void iHub_Connect(AMessage_st* s_Message_Rx);
static uint8_t prvAttemptInTaskReconnect(AMessage_st *s_Message_Rx_new, AzureIoTResult_t xLoopErr, int64_t *plastPingTime);
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void update_device_online_status(uint8_t status);
PSRAM_ATTR_BSS struct iHub_registers iHub_Para;
static char u8D2Cmessage =0,IHUBServerRespFg =0;  // Flag use for sending device-to-cloud messages

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &iHub_Para.conn_status,   "STATUS",  U_INT8, 		"R",  "Connection status" },
	{ &iHub_Para.Enable_WDT,   "ENABLE_WDT",  U_INT8, 	"RW",  "Enable WatchDog to check IHUB disconnected state" },
    { &iHub_Para.HostName,      "HOSTNAME",     STRING, "RW",  "Host name of the device" },
	{ &iHub_Para.IHUB_status_time_u64   , "IHUB_UPDATE_TIME"     , U_INT64, "R", "IHUB status last update time" },
    { &iHub_Para.PrimaryKey,    "PRIMARY_KEY",  STRING, "RW",  "Primary key for device identification" },
    { &iHub_Para.IHUB_disconnect_reason,    "IHUB_DISCON_REASON",  STRING, "R",  "Primary key for device identification" },
	{ &iHub_Para.D2C_interval_second_u32   , "D2C_INTERVAL_SECOND"     , U_INT32, "RW", "D2C interval in second for heartbeat" },
	{ &iHub_Para.ever_connected_since_boot,  "EVER_CONNECTED_SINCE_BOOT", U_INT8, "RW",  "ever_connected_since_boot" }
};

PSRAM_ATTR_BSS static char payLoadData_Server[4 * 1024];
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage[IHUB_OBJ_QUE_COUNT * sizeof(AMessage_st)], Twin_pucQueueStorage [PROCESS_TWIN_QUEUE_LENGTH * PROCESS_TWIN_QUEUE_ITEMSIZE], Twin_Command_pucQueueStorage [PROCESS_TWIN_COMMAND_QUEUE_LENGTH * PROCESS_TWIN_COMMAND_QUEUE_ITEMSIZE],IHUB_pucQueueStorage [IHUB_CONNECT_QUEUE_LENGTH * IHUB_CONNECT_QUEUE_ITEMSIZE];
PSRAM_ATTR_BSS static StackType_t xTaskStack [IHUB_TASK_STACK_DEPTH], xDevice_Info_TaskStack [IHUB_GET_DEVICE_INFO_TASK_STACK_DEPTH], xTaskStack2 [democonfigDEMO_STACKSIZE], TwinCommandProcessxTaskStack [TWIN_COMMAND_PROCESS_STACK_DEPTH]; // xWDTTaskStack [IHUB_WDT_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static char payLoadData_TwinCmmand[MAX_JSON_PAYLOAD_BYTES];

static AMessage_st s_Message_Tx;

static TaskHandle_t  iHubHandle = NULL, vStartDemoTask_handle = NULL, TwinCommandTask_handle = NULL, DeviceInfo_Handle = NULL; // iHubWDTHandle = NULL;
static QueueHandle_t msg_Rx_Queue, IHUB_Connect_Queue = NULL, Process_Twin_Queue = NULL, Process_Twin_Commands_Queue = NULL;   //msg_Tx_Queue
static StaticTask_t xIHUBTaskBuffer, xIHUBDeviceInfoTaskBuffer, xIHUBSubTaskBuffer, xProcess_Twin_CommandTaskBuffer; // xIHUBWDTTaskBuffer;  //// Declare a static task control block; //// Declare a static task control block

static uint64_t get_current_time_ms() {
	return get_current_rtc_time_ms(0);
}

static void connectivity_supervisor_init(void)
{
    const int64_t now = esp_timer_get_time();
    boot_time_us = now;
 
    ihub_connected_now = false;
    disconnect_start_us = 0;
    last_successful_activity_us = now;
}

static void connectivity_supervisor_mark_connected(void)
{
	char payLoadData_11[100];

    const int64_t now = esp_timer_get_time();
    ihub_connected_now = true;

    iHub_Para.ever_connected_since_boot = 1;
    disconnect_start_us = 0;
    last_successful_activity_us = now;

	cJSON *out_JSON  = cJSON_CreateObject();
	cJSON_AddStringToObject(out_JSON, "FILE_NAME", "A:/System/IHUB.json");

	char ever_conn_str[16];   // big enough for int
	sprintf(ever_conn_str, "%d", iHub_Para.ever_connected_since_boot);

	cJSON_AddStringToObject(out_JSON, "EVER_CONNECTED_SINCE_BOOT", ever_conn_str);

	memset(payLoadData_11,0,sizeof(payLoadData_11));//\0';
	cJSON_PrintPreallocated(out_JSON, payLoadData_11, sizeof(payLoadData_11), false);

	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData_11,strlen(payLoadData_11),"WRITE_VAR");
}

static void connectivity_supervisor_mark_disconnected(void)
{
    ihub_connected_now = false;
}

static bool connectivity_supervisor_is_armed(int64_t now)
{
    if ((boot_time_us == 0) || ((now - boot_time_us) < CONNECTIVITY_BOOT_GRACE_US)) {
    	last_successful_activity_us = now;
        return false;
    }

    uint8_t connect_once = iHub_Para.ever_connected_since_boot;

    return connect_once;
}

static void iHub_conn_status_set(uint8_t status)
{
	iHub_Para.conn_status = status;

	if (status == IHUB_CONNECTED) {
		connectivity_supervisor_mark_connected();
	} else {
		connectivity_supervisor_mark_disconnected();
	}
}
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
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				iHub_Para.IHUB_status_time_u64  = current_epos_sec;
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_WDT")))
			{
				esp_err_t err;
				if(iHub_Para.Enable_WDT == 1)  // enable wdt
				{
					if(esp_timer_is_active(periodic_timer) == false)  // start the timer if it is not yet started
					{
						err = esp_timer_start_periodic(periodic_timer, CONNECTIVITY_SUPERVISOR_INTERVAL_US);

						if(err != ESP_OK)
							printf("\n\n set Error in start periodic timer. err = %d\n\n", err);
					}
				}
				if(iHub_Para.Enable_WDT == 0)
				{
					if(esp_timer_is_active(periodic_timer) == true)
					{
						//printf("\n set timer esp_timer_stop \n");
						err = esp_timer_stop(periodic_timer); // stop the timer
						if(err != ESP_OK)
							printf("\n\n set Error in stopping the periodic timer. err = %d\n\n", err);
					}
				}
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
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case IHUB_NOT_INITIALISED:
					strcpy(val_a8, "IHUB NOT INITIALISED");
					break;

				case IHUB_INITIALISED:
					strcpy(val_a8, "IHUB INITIALISED");
					break;

				case IHUB_DISCONNECTED:
					strcpy(val_a8, "IHUB DISCONNECTED");
					break;

				case IHUB_CONNECTED:
					strcpy(val_a8, "IHUB CONNECTED");
					break;

				case IHUB_NOT_CONNECTED:
					strcpy(val_a8, "IHUB NOT CONNECTED");
					break;

				default:
					break;
				}
			}
		}
	}
}//	get

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[200] = {0};
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
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				//printf("\n prop[i].str_name = %s, prop[i].name= %d",prop[i].str_name,*(char*)prop[i].name);
				switch (*(char*)prop[i].name)
				{
				case IHUB_NOT_INITIALISED:
					strcpy(val_a8, "IHUB NOT INITIALISED");
					break;

				case IHUB_INITIALISED:
					strcpy(val_a8, "IHUB INITIALISED");
					break;

				case IHUB_DISCONNECTED:
					strcpy(val_a8, "IHUB DISCONNECTED");
					break;

				case IHUB_CONNECTED:
					strcpy(val_a8, "IHUB CONNECTED");
					break;

				case IHUB_NOT_CONNECTED:
					strcpy(val_a8, "IHUB NOT CONNECTED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_WDT")))
			{
				//printf("\n prop[i].str_name = %s, prop[i].name= %d",prop[i].str_name,*(char*)prop[i].name);
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "IHUB WDT DISABLED");
					break;

				case 1:
					strcpy(val_a8, "IHUB WDT ENABLED");
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
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		

		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "SET(string PRIMARY_KEY)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "CONNECT()", "Connect to the IOT Hub.");
	cJSON_AddStringToObject(responseObject, "D2C_MESSAGE(string PAYLOAD)", "Send D2C message to IoT server.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "DEINIT()", "Deinit the actor. It delete the monitor task and its queue.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help


static void init(void *a, void *b)
{
	if (FirstiHubEntry == false){
		//Create Rx_Queue
		if(msg_Rx_Queue == NULL)
		{
			msg_Rx_Queue = xQueueCreateStatic(IHUB_OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
			if (msg_Rx_Queue == NULL) {
						printf(">IHUB.Error(IHUB RX Queue is not created.) \n");
			}
		}

		iHubHandle = xTaskCreateStaticPinnedToCore(
			monitor,                 // Task function
			"iHub_Monitor",            // Task name
			IHUB_TASK_STACK_DEPTH,        // Stack size in words
			NULL,                    // Task parameters (not used here)
			IHUB_TASK_PRIORITY,                       // Task priority
			xTaskStack,              // Pointer to task stack (allocated in PSRAM)
			&xIHUBTaskBuffer,             // Pointer to task control block
			1
		);
		if (iHubHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
		return;
		}

		FirstiHubEntry = true;
		iHub_Para.ever_connected_since_boot = 0;

		if(boot_time_us == 0)
		{
			connectivity_supervisor_init();
		}

		iHub_conn_status_set(IHUB_INITIALISED);

		struct timeval currentTime;
		_gettimeofday_r(NULL, &currentTime, NULL);
		uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		if(deinit_Flag == 0)
		{
			iHub_Para.IHUB_status_time_u64  = current_epos_sec;
			deinit_Flag = 1;
		}

		iHub_Para.D2C_interval_second_u32  = D2C_INTERVAL_SECONDS;

		 memset((char*)&iHub_Para.HostName, 0, sizeof(iHub_Para.HostName));
		 memset((char*)&iHub_Para.PrimaryKey, 0, sizeof(iHub_Para.PrimaryKey));
		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/IHUB.json");
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		cJSON_Delete(responseObject);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		if (Process_Twin_Queue == NULL) {
			Process_Twin_Queue = xQueueCreateStatic(PROCESS_TWIN_QUEUE_LENGTH, PROCESS_TWIN_QUEUE_ITEMSIZE, Twin_pucQueueStorage, &Twin_pxQueueBuffer);
			if (Process_Twin_Queue == NULL) {
					// Handle the error, such as logging or entering a safe state
					printf("Free heap Now Failed to create Process_Twin_Queue\n");
					// Add additional error handling as needed
			}
		}
		iHub_Para.Enable_WDT = 1; // enable ihub wdt
		if(periodic_timer == NULL)
		{
			esp_err_t err = esp_timer_create(&periodic_timer_args_new, &periodic_timer);
			if(err != ESP_OK)
			{
				printf("\n\n Error! failed to create period timer in IHUB actor err- %d\n\n", err);
				return;
			}
			err = esp_timer_start_periodic(periodic_timer, CONNECTIVITY_SUPERVISOR_INTERVAL_US);

			if(err != ESP_OK)
				printf("\n\n Error in start periodic timer. err = %d\n\n", err);
		}
	}
}// init

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	uint8_t u8Result =0;
	while (1){
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
//			printf("IHUB msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("IHUB DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT")) {   // INIT Properties
				if (FirstiHubEntry == false) {
					init(0, 0);
				}
				else if(FirstiHubEntry==true){
					Add_Response_msg("IHUB initialization is done.", s_Message_Rx, payLoadData);
				}
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			{
//				getAll((char*)prop, (char*) val, s_Message_Rx);
//			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				Get_Property(s_Message_Rx);

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET")) // Set Actor Properties
			{
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				    }
				else{
					head_JSON = name_JSON->child;
					if((strcmp(head_JSON->string, "ENABLE_WDT") == 0) || (strcmp(head_JSON->string, "EVER_CONNECTED_SINCE_BOOT") == 0))
					{
						cJSON *root_JSON  = cJSON_CreateObject();
						cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/IHUB.json");
						cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
						Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
						cJSON_Delete(root_JSON);
					}

				   // Loop through each key-value pair
				    do {
				        // Check if the value string is not NULL
				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
				        {
				            // Set the key-value pair
				        	u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
				        	if(u8Result==2){
				        		sprintf(str,"'%s' is a read only property", head_JSON->string);
				        		 Add_Response_msg(str, s_Message_Rx, payLoadData);
				        	}
				        } else {
				            // Handle the case where value string is NULL (e.g., log an error or take appropriate action)
				            sprintf(str, "Invalid parameter '%s'", head_JSON->string);
				            Add_Response_msg(str,s_Message_Rx, payLoadData);
				            // Handle the error as per your application's requirements
				        }
				        head_JSON = head_JSON->next;
				    } while (head_JSON != 0);

				    if(u8Result==1){
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
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "CONNECT"))
			{
				if(iHub_Para.conn_status != IHUB_CONNECTION_IN_PROGRESS)
				{
					if(iHub_Para.conn_status==IHUB_CONNECTED)
						Add_Response_msg("Device already connected to IHUB. ", s_Message_Rx, payLoadData);
					else
					{
						iHub_conn_status_set(IHUB_NOT_CONNECTED);

						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						iHub_Para.IHUB_status_time_u64  = current_epos_sec;
						iHub_Connect(s_Message_Rx);
					}
				}
				else
					Add_Response_msg("IHUB_CONNECTION_IN_PROGRESS. Ihub connection will be restarted if failed", s_Message_Rx, payLoadData);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "D2C_MESSAGE"))
			{
				name_JSON  = cJSON_Parse((char*) s_Message_Rx->payload_p8);
					if (name_JSON == NULL) {
						sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
					}
					else{
						head_JSON = cJSON_GetObjectItem(name_JSON, "PAYLOAD");
						if((head_JSON != NULL) && (cJSON_IsObject(head_JSON)))
						{		
							memset(D2CPayload,0,sizeof(D2CPayload));
							cJSON_PrintPreallocated(head_JSON, D2CPayload, sizeof(D2CPayload), false);
							u8D2Cmessage =1;
						}
						else if((head_JSON != NULL) && (cJSON_IsString(head_JSON)))
						{
							strcpy(D2CPayload,(char*) head_JSON->valuestring);
							u8D2Cmessage =1;
						}
						cJSON_Delete(name_JSON);
						printf("<%s.D2C_MESSAGE(%s)\n", s_Message_Rx->dest_Actor_a8,s_Message_Rx->payload_p8);

					}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UPDATE_TWIN"))
			 {

				name_JSON  = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {

					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				}
				else{

					cJSON *updateTwin = cJSON_CreateObject();
					cJSON_AddItemToObject(updateTwin, "TWIN_UPDATE", name_JSON);				
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(updateTwin, payLoadData, sizeof(payLoadData), false);
					xQueueSend(Process_Twin_Queue, payLoadData, QUE_DELAY);
					cJSON_Delete(updateTwin);
				}
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DEINIT"))
			{
				Add_Response_msg("IHUB DEINIT method is received", s_Message_Rx, payLoadData);
				Deinit_Actor(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DISCONNECT"))
			{
				Add_Response_msg("Disconnecting IHUB...", s_Message_Rx, payLoadData);
				Delete_task_flag = 1;
			}
			else
			{
				//IHUB error message: invalid method
				 Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}//	monitor

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


static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char keyValue[100]= {0};
	char str[200]= {0};
//	char IhubRespData[ucCommandResponsePayloadBufferSize] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n IHUB s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
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
				 if (IHUBServerRespFg ==1)
				 {
 //					strncpy(IhubRespData, payLoadData, sizeof(IhubRespData));
					memset(ucCommandResponsePayloadBuffer,0,ucCommandResponsePayloadBufferSize);
					strncpy((char*)ucCommandResponsePayloadBuffer,(char*)payLoadData, (ucCommandResponsePayloadBufferSize -1));
					IHUBServerRespFg =0;
					// Free the parsed JSON
					cJSON_Delete(in_JSON);
 //					 xSemaphoreGive(Direct_Method_Semaphore);
					return;
				 }
			}
			// Free the parsed JSON
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
						if(!strcmp(currentItem->valuestring, "System/IHUB.json"))
						{
							currentItem = currentItem->next;
							while (currentItem != NULL)
							{
								if (cJSON_IsString(currentItem))
								{
									set(currentItem->string, currentItem->valuestring,s_Message_Rx);
								}
								else if (cJSON_IsNumber(currentItem))
								{
									sprintf(keyValue, "%d", currentItem->valueint);
									set(currentItem->string, keyValue,s_Message_Rx);
								}
								// Move to the next key-value pair
								currentItem = currentItem->next;
							}
						}
					}
				 }
			cJSON_Delete(in_JSON);
			return;
		 }
		}
		 if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
		 {
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				return;
			}
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					if(strcmp(name_JSON->valuestring, "2") == 0)
					 {
						s_is_connected_to_internet=1;
					 }
				}

				name_JSON 		= cJSON_GetObjectItem(responseKey, "ETH_STATUS");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						if(strcmp(name_JSON->valuestring, "1") == 0)
						 {
							Send_CMD_To_Other_Actor(ETH,"ETH", "/0", 0, "GET_CONN_INFO");
							if(DeviceInfo_Handle == NULL)  // create task to obtain device info from CONSOLE
							{
								DeviceInfo_Handle = xTaskCreateStaticPinnedToCore(
									Get_Device_Info,                 // Task function
									"Get_Device_Info",            // Task name
									IHUB_GET_DEVICE_INFO_TASK_STACK_DEPTH,        // Stack size in words
									NULL,                    // Task parameters (not used here)
									IHUB_TASK_PRIORITY,                       // Task priority
									xDevice_Info_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xIHUBDeviceInfoTaskBuffer,             // Pointer to task control block
									1
								);
							}
						 }
					}
				name_JSON 		= cJSON_GetObjectItem(responseKey, "WIFI_STATUS");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						if(strcmp(name_JSON->valuestring, "2") == 0)
						 {
							Send_CMD_To_Other_Actor(WIFI,"WIFI", "/0", 0, "GET_CONN_INFO");
							if(DeviceInfo_Handle == NULL)  // create task to obtain device info from CONSOLE
							{
								DeviceInfo_Handle = xTaskCreateStaticPinnedToCore(
									Get_Device_Info,                 // Task function
									"Get_Device_Info",            // Task name
									IHUB_GET_DEVICE_INFO_TASK_STACK_DEPTH,        // Stack size in words
									NULL,                    // Task parameters (not used here)
									IHUB_TASK_PRIORITY,                       // Task priority
									xDevice_Info_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xIHUBDeviceInfoTaskBuffer,             // Pointer to task control block
									1
								);
							}
						 }
					}
			}
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"WHO_AM_I")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL && cJSON_IsObject(responseKey))
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DeviceID");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						twin_additional_data(s_Message_Rx);
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		 }
		 if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
		 	{
		 		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		 		if (in_JSON == NULL)
		 		{
		 			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		 			Add_Response_msg(str,s_Message_Rx, payLoadData);
		 			return;
		 		}
		 		cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
		 		if (responseKey != NULL && cJSON_IsString(responseKey->child))
		 		{
		 			name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICEID");
		 			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
		 			{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(IHUB_Connect_Queue!=NULL){
							xQueueSend(IHUB_Connect_Queue, payLoadData, QUE_DELAY);
						}
		 			}
		 		}
		 		cJSON_Delete(in_JSON);
		 		return;
		 	}

			if((strcmp((char*)s_Message_Rx->src_Actor_a8,"WIFI")==0)||strcmp((char*)s_Message_Rx->src_Actor_a8,"ETH")==0)
			{
				in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
				cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
				if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_CONN_INFO")==0))
				{
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (responseKey != NULL && cJSON_IsObject(responseKey->child))
					{
						cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "CONNECTIVITY_INFO");
						if((name_JSON != NULL) && (cJSON_IsObject(name_JSON)))
						{
							merge_connectivity_info_response(responseKey);
							if ((vStartDemoTask_handle != NULL) && (Process_Twin_Queue != NULL))
							{
								xQueueSend(Process_Twin_Queue, CONNECTIVITY_INFO_DATA, QUE_DELAY);
							}
						}
					}
				}
				cJSON_Delete(in_JSON);
				return;
			}

		 return;
}

static void twin_additional_data(AMessage_st* s_Message_Rx)
{
	cJSON *device_info = NULL;
	cJSON_bool ret = false;
	char ask_twin_data_flag = 0;
	int internet_avl_flg = 0;
	char str[100] = {0};

	cJSON *responseKey = cJSON_Parse(payLoadData);
	if (responseKey == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}

	cJSON *WIFI_STATUS_item = cJSON_GetObjectItem(responseKey, "WIFI_STATUS");
	if((WIFI_STATUS_item != NULL) && (cJSON_IsString(WIFI_STATUS_item)))
	{
		if (!strcmp( WIFI_STATUS_item->valuestring, "Connected"))
		{
			internet_avl_flg = 1;
		}
	}
	cJSON *ETH_STATUS_item = cJSON_GetObjectItem(responseKey, "ETH_STATUS");
	if((ETH_STATUS_item != NULL) && (cJSON_IsString(ETH_STATUS_item)))
	{
		if (!strcmp( ETH_STATUS_item->valuestring, "Connected"))
		{
			internet_avl_flg = 1;
		}
	}
	if(internet_avl_flg == 0)
	{
		cJSON_Delete(responseKey);
		return;
	}
	device_info = cJSON_CreateObject();
	cJSON *DeviceID = cJSON_GetObjectItem(responseKey, "DeviceID");
	if((DeviceID != NULL) && (cJSON_IsString(DeviceID)))
	{
		cJSON_AddStringToObject(device_info, "DEVICE_ID", DeviceID->valuestring);
	}
	cJSON *Firmware_Version = cJSON_GetObjectItem(responseKey, "Firmware_Version");
	if((Firmware_Version != NULL) && (cJSON_IsString(Firmware_Version)))
	{
		cJSON_AddStringToObject(device_info, "FIRMWARE_VERSION", Firmware_Version->valuestring);
	}
	cJSON *Bootloader_Version = cJSON_GetObjectItem(responseKey, "Bootloader_Version");
	if((Bootloader_Version != NULL) && (cJSON_IsString(Bootloader_Version)))
	{
		cJSON_AddStringToObject(device_info, "BOOTLOADER_VERSION", Bootloader_Version->valuestring);
	}
	cJSON *Hardware_Version = cJSON_GetObjectItem(responseKey, "Hardware_Version");
	if((Hardware_Version != NULL) && (cJSON_IsString(Hardware_Version)))
	{
		cJSON_AddStringToObject(device_info, "HARDWARE_VERSION", Hardware_Version->valuestring);
	}
	cJSON *Model_Name = cJSON_GetObjectItem(responseKey, "Model_Name");
	if((Model_Name != NULL) && (cJSON_IsString(Model_Name)))
	{
		cJSON_AddStringToObject(device_info, "MODEL_NAME", Model_Name->valuestring);
	}
	cJSON *Manufacturer_Name = cJSON_GetObjectItem(responseKey, "Manufacturer_Name");
	if((Manufacturer_Name != NULL) && (cJSON_IsString(Manufacturer_Name)))
	{
		cJSON_AddStringToObject(device_info, "MANUFACTURER_NAME", Manufacturer_Name->valuestring);
	}
	cJSON *Manufacturer_Date = cJSON_GetObjectItem(responseKey, "Manufacturer_Date");
	if((Manufacturer_Date != NULL) && (cJSON_IsString(Manufacturer_Date)))
	{
		cJSON_AddStringToObject(device_info, "MANUFACTURER_DATE", Manufacturer_Date->valuestring);
	}

	cJSON *Location_info = cJSON_CreateObject();
	cJSON *DST = cJSON_GetObjectItem(responseKey, "DST");
	if((DST != NULL) && (cJSON_IsString(DST)))
	{
		cJSON_AddStringToObject(Location_info, "DST", DST->valuestring);
	}
	cJSON *GMT = cJSON_GetObjectItem(responseKey, "GMT");
	if((GMT != NULL) && (cJSON_IsString(GMT)))
	{
		cJSON_AddStringToObject(Location_info, "GMT", GMT->valuestring);
	}
	cJSON *SR_TIME = cJSON_GetObjectItem(responseKey, "SR_TIME");
	if((SR_TIME != NULL) && (cJSON_IsString(SR_TIME)))
	{
		cJSON_AddStringToObject(Location_info, "SR_TIME", SR_TIME->valuestring);
	}
	cJSON *SS_TIME = cJSON_GetObjectItem(responseKey, "SS_TIME");
	if((SS_TIME != NULL) && (cJSON_IsString(SS_TIME)))
	{
		cJSON_AddStringToObject(Location_info, "SS_TIME", SS_TIME->valuestring);
	}
	cJSON *LATITUDE = cJSON_GetObjectItem(responseKey, "LATITUDE");
	if((LATITUDE != NULL) && (cJSON_IsString(LATITUDE)))
	{
		cJSON_AddStringToObject(Location_info, "LATITUDE", LATITUDE->valuestring);
	}
	cJSON *LONGITUDE = cJSON_GetObjectItem(responseKey, "LONGITUDE");
	if((LONGITUDE != NULL) && (cJSON_IsString(LONGITUDE)))
	{
		cJSON_AddStringToObject(Location_info, "LONGITUDE", LONGITUDE->valuestring);
	}
	cJSON *REBOOT_FLAG = cJSON_GetObjectItem(responseKey, "REBOOT_FLAG");
	if((REBOOT_FLAG != NULL) && (cJSON_IsBool(REBOOT_FLAG)))
	{
		cJSON_AddBoolToObject(Location_info, "REBOOT_FLAG", cJSON_IsTrue(REBOOT_FLAG));
	}
	cJSON *REBOOT_HH = cJSON_GetObjectItem(responseKey, "REBOOT_HH");
	if((REBOOT_HH != NULL) && (cJSON_IsNumber(REBOOT_HH)))
	{
		cJSON_AddNumberToObject(Location_info, "REBOOT_HH", REBOOT_HH->valueint);
	}
	cJSON *REBOOT_MM = cJSON_GetObjectItem(responseKey, "REBOOT_MM");
	if((REBOOT_MM != NULL) && (cJSON_IsNumber(REBOOT_MM)))
	{
		cJSON_AddNumberToObject(Location_info, "REBOOT_MM", REBOOT_MM->valueint);
	}
	cJSON *root = cJSON_CreateObject();
	cJSON_AddItemToObject(root, "DEVICE_INFO", device_info);
	cJSON_AddItemToObject(root, "LOCATION_INFO", Location_info);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
	if(vStartDemoTask_handle == NULL)   // return if IHUB is disconnected
	{
		cJSON_Delete(responseKey);
		return;
	}
	if(Process_Twin_Queue != NULL)
	{
		xQueueSend(Process_Twin_Queue, payLoadData, QUE_DELAY);
	}
	cJSON_Delete(responseKey);
	vTaskDelay(1000/portTICK_PERIOD_MS); // wait to send the properties to IHUB

	// Compare new and old reported properties. If they are different then get  updated twin data
	cJSON *Rep_Prop = cJSON_Parse(Reported_Properties);
	// Check CONNECTIVITY_INFO
	cJSON *Conn_Info_Old = cJSON_GetObjectItem(Rep_Prop, "CONNECTIVITY_INFO");
	cJSON *Conn_Info = cJSON_Parse(CONNECTIVITY_INFO_DATA);
	if((Conn_Info == NULL)  || (Conn_Info_Old == NULL))
	{
		return;
	}
	cJSON *Conn_Info_New = cJSON_GetObjectItem(Conn_Info, "CONNECTIVITY_INFO");

	cJSON *SSID_Old = cJSON_GetObjectItem(Conn_Info_Old, "SSID");
	cJSON *SSID_New = cJSON_GetObjectItem(Conn_Info_New, "SSID");
	if((SSID_New != NULL) && (SSID_Old != NULL))
	{
		if(strcmp(SSID_Old->valuestring, SSID_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}

	cJSON *PASSWORD_Old = cJSON_GetObjectItem(Conn_Info_Old, "PASSWORD");
	cJSON *PASSWORD_New = cJSON_GetObjectItem(Conn_Info_New, "PASSWORD");
	if((PASSWORD_Old != NULL) && (PASSWORD_New != NULL))
	{
		if(strcmp(PASSWORD_Old->valuestring, PASSWORD_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *WIFI_GATEWAY_MAC_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "WIFI_GATEWAY_MAC_ADDRESS");
	cJSON *WIFI_GATEWAY_MAC_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "WIFI_GATEWAY_MAC_ADDRESS");
	if((WIFI_GATEWAY_MAC_ADDRESS_Old != NULL) && (WIFI_GATEWAY_MAC_ADDRESS_New != NULL))
	{
		if(strcmp(WIFI_GATEWAY_MAC_ADDRESS_Old->valuestring, WIFI_GATEWAY_MAC_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *GATEWAY_IP_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "GATEWAY_IP_ADDRESS");
	cJSON *GATEWAY_IP_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "GATEWAY_IP_ADDRESS");
	if((GATEWAY_IP_ADDRESS_Old != NULL) && (GATEWAY_IP_ADDRESS_New != NULL))
	{
		if(strcmp(GATEWAY_IP_ADDRESS_Old->valuestring, GATEWAY_IP_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *DEVICE_IP_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "DEVICE_IP_ADDRESS");
	cJSON *DEVICE_IP_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "DEVICE_IP_ADDRESS");
	if((DEVICE_IP_ADDRESS_Old != NULL) && (DEVICE_IP_ADDRESS_New != NULL))
	{
		if(strcmp(DEVICE_IP_ADDRESS_Old->valuestring, DEVICE_IP_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *IP_ADDRESS_UPDATED_DATETIME_Old = cJSON_GetObjectItem(Conn_Info_Old, "IP_ADDRESS_UPDATED_DATETIME");
	cJSON *IP_ADDRESS_UPDATED_DATETIME_New = cJSON_GetObjectItem(Conn_Info_New, "IP_ADDRESS_UPDATED_DATETIME");
	if((IP_ADDRESS_UPDATED_DATETIME_Old != NULL) && (IP_ADDRESS_UPDATED_DATETIME_New != NULL))
	{
		if(strcmp(IP_ADDRESS_UPDATED_DATETIME_Old->valuestring, IP_ADDRESS_UPDATED_DATETIME_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *ETH_GATEWAY_MAC_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "ETH_GATEWAY_MAC_ADDRESS");
	cJSON *ETH_GATEWAY_MAC_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "ETH_GATEWAY_MAC_ADDRESS");
	if((ETH_GATEWAY_MAC_ADDRESS_Old != NULL) && (ETH_GATEWAY_MAC_ADDRESS_New != NULL))
	{
		if(strcmp(ETH_GATEWAY_MAC_ADDRESS_Old->valuestring, ETH_GATEWAY_MAC_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *WIFI_MAC_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "WIFI_MAC_ADDRESS");
	cJSON *WIFI_MAC_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "WIFI_MAC_ADDRESS");
	if((WIFI_MAC_ADDRESS_Old != NULL) && (WIFI_MAC_ADDRESS_New != NULL))
	{
		if(strcmp(WIFI_MAC_ADDRESS_Old->valuestring, WIFI_MAC_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON *ETH_MAC_ADDRESS_Old = cJSON_GetObjectItem(Conn_Info_Old, "ETH_MAC_ADDRESS");
	cJSON *ETH_MAC_ADDRESS_New = cJSON_GetObjectItem(Conn_Info_New, "ETH_MAC_ADDRESS");
	if((ETH_MAC_ADDRESS_Old != NULL) && (ETH_MAC_ADDRESS_New != NULL))
	{
		if(strcmp(ETH_MAC_ADDRESS_Old->valuestring, ETH_MAC_ADDRESS_New->valuestring) != 0)
		{
			ask_twin_data_flag++;
		}
	}
	cJSON_Delete(Conn_Info);

	// Check DEVICE_INFO
	cJSON *Device_Info_Old = cJSON_GetObjectItem(Rep_Prop, "DEVICE_INFO");
	ret = cJSON_Compare(Device_Info_Old, device_info, 1);  // compare two JSON objects with case sensitive
	if(ret == false)
	{
		ask_twin_data_flag++;
	}

	// Check LOCATION_INFO
	cJSON *Location_Info_Old = cJSON_GetObjectItem(Rep_Prop, "LOCATION_INFO");
	ret = cJSON_Compare(Location_Info_Old, Location_info, 1);  // compare two JSON objects with case sensitive
	if(ret == false)
	{
		ask_twin_data_flag++;
	}
	cJSON_Delete(Rep_Prop);
	cJSON_Delete(root);

	if(ask_twin_data_flag != 0)  // get updated twin message
	{
		vTaskDelay(1000/portTICK_PERIOD_MS);  // give some delay to update the properties and then read twin message
		if(vStartDemoTask_handle == NULL)   // return if IHUB is disconnected
		{
			return;
		}
		AzureIoTHubClient_RequestPropertiesAsync( &xAzureIoTHubClient );  //AzureIoTResult_t xResult = AzureIoTHubClient_RequestPropertiesAsync( &xAzureIoTHubClient );
	}
}

static void merge_connectivity_info_response(cJSON *responseKey)
{
	cJSON *new_conn_info = cJSON_GetObjectItem(responseKey, "CONNECTIVITY_INFO");
	if ((new_conn_info == NULL) || !cJSON_IsObject(new_conn_info))
	{
		return;
	}

	cJSON *merged_response = cJSON_CreateObject();
	if (merged_response == NULL)
	{
		return;
	}

	cJSON *merged_conn_info = cJSON_CreateObject();
	if (merged_conn_info == NULL)
	{
		cJSON_Delete(merged_response);
		return;
	}
	cJSON_AddItemToObject(merged_response, "CONNECTIVITY_INFO", merged_conn_info);

	cJSON_AddStringToObject(merged_conn_info, "SSID", "");
	cJSON_AddStringToObject(merged_conn_info, "PASSWORD", "");
	cJSON_AddNumberToObject(merged_conn_info, "RSSI", 0);
	cJSON_AddStringToObject(merged_conn_info, "WIFI_MAC_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "ETH_MAC_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "WIFI_GATEWAY_MAC_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "ETH_GATEWAY_MAC_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "GATEWAY_IP_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "DEVICE_IP_ADDRESS", "");
	cJSON_AddStringToObject(merged_conn_info, "IP_ADDRESS_UPDATED_DATETIME", "");

	for (cJSON *item = new_conn_info->child; item != NULL; item = item->next)
	{
		if ((item->string == NULL) || (merged_conn_info == NULL))
		{
			continue;
		}

		cJSON *new_item = cJSON_Duplicate(item, true);
		if (new_item == NULL)
		{
			continue;
		}

		if (cJSON_GetObjectItemCaseSensitive(merged_conn_info, item->string) != NULL)
		{
			cJSON_ReplaceItemInObjectCaseSensitive(merged_conn_info, item->string, new_item);
		}
		else
		{
			cJSON_AddItemToObject(merged_conn_info, item->string, new_item);
		}
	}

	memset(CONNECTIVITY_INFO_DATA, 0, sizeof(CONNECTIVITY_INFO_DATA));
	cJSON_PrintPreallocated(merged_response, CONNECTIVITY_INFO_DATA, sizeof(CONNECTIVITY_INFO_DATA), false);
	cJSON_Delete(merged_response);
}

//========================= Azure IOT Functions ============================================//
bool xAzureSample_IsConnectedToInternet()
{
    return s_is_connected_to_internet;
}

uint64_t ullGetUnixTime( void )
{
    time_t now = time( NULL );

    if( now == ( time_t ) ( -1 ) )
    {
      //  ESP_LOGE( TAG, "Failed obtaining current time.\r\n" );
    }

    return now;
}
/*-----------------------------------------------------------
 * @remark This function is required for the interface with samples to work properly.
 */
static void prvHandleCommand( AzureIoTHubClientCommandRequest_t * pxMessage, void * pvContext)
{
    AzureIoTHubClient_t * pxHandle = ( AzureIoTHubClient_t * ) pvContext;
    uint32_t ulResponseStatus = 0;
    AzureIoTResult_t xResult;
    char CommandName[100]={0};  //, command_buffer[2048] = {0}
    AMessage_st s_Message_Tx_new;
	ihubTimeActive = get_current_time_ms();
    sendLedState("SOLID Message",1, "none");

    s_Message_Tx_new.Dest_ID_a8 = IHUB;
	s_Message_Tx_new.Src_ID_u8 =CONSOLE;
	strcpy((char*)s_Message_Tx_new.cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Tx_new.dest_Actor_a8,"IHUB");
	strcpy((char*)s_Message_Tx_new.src_Actor_a8,"SYSTEM");
	s_Message_Tx_new.payload_p8 = (uint8_t*) command_buffer;

    if(pxMessage->ulPayloadLength > (COMMAND_LEN-50))
    {
    	Add_Response_msg("Error! Command size is larger than maximum command size limit. Hence cannot process the command.", &s_Message_Tx_new, payLoadData_IHUB);
		return;
    }
    size_t payload_copy_len = pxMessage->ulPayloadLength;
    if (payload_copy_len >= sizeof(MethodPayload)) {
        payload_copy_len = sizeof(MethodPayload) - 1U;
    }
    memcpy(MethodPayload, pxMessage->pvMessagePayload, payload_copy_len);
    MethodPayload[payload_copy_len] = '\0';

    size_t name_copy_len = pxMessage->usCommandNameLength;
    if (name_copy_len >= sizeof(CommandName)) {
        name_copy_len = sizeof(CommandName) - 1U;
    }
    memcpy(CommandName, pxMessage->pucCommandName, name_copy_len);
    CommandName[name_copy_len] = '\0';

	cJSON *NewObject = cJSON_Parse(MethodPayload);  

    if(NewObject != NULL)
    {
		cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
		if(responseObject != NULL)
		{
			cJSON_AddStringToObject(responseObject, "METHOD_NAME", CommandName);  //(char*)pxMessage->pucCommandName
			cJSON_AddItemToObject(responseObject, "METHOD_PAYLOAD", NewObject);	
			memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
			cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
			strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_IHUB);				
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(&s_Message_Tx_new);
		}
	}

    uint32_t ulCommandResponsePayloadLength = ulHandleCommand( pxMessage,
                                                               &ulResponseStatus,
															   (uint8_t*)ucCommandResponsePayloadBuffer,
															   ucCommandResponsePayloadBufferSize );

    if(strcmp(CommandName,"ExecuteCommand")==0){
       strcpy(ucCommandResponsePayloadBuffer,"Data not yet populated.");
       Process_Post_Method_Response(&s_Message_Tx_new, MethodPayload);
       IHUBServerRespFg =1;
    }
     vTaskDelay(1000/portTICK_PERIOD_MS);
     if(strlen((char*)ucCommandResponsePayloadBuffer)!=0)
     ulCommandResponsePayloadLength =strlen((char*)ucCommandResponsePayloadBuffer);

    if( ( xResult = AzureIoTHubClient_SendCommandResponse( pxHandle, pxMessage, ulResponseStatus,
    														(uint8_t*)ucCommandResponsePayloadBuffer,
                                                           ulCommandResponsePayloadLength ) ) != eAzureIoTSuccess )
    {
#ifdef ENABLE_PRINT_MSG
    	printf( "Error sending command response: result 0x%08x", ( uint16_t ) xResult);
#endif
    }
}
/*-----------------------------------------------------------*/
static void prvHandleCtoDCommand( AzureIoTHubClientCloudToDeviceMessageRequest_t * pxMessage,
                                  void * pvContext)
{
    ( void ) pvContext;
	AMessage_st s_Message_Tx_new;
    char C2DMessge_Payload[200]={0};
    uint32_t ulPayloadCopyLength = pxMessage->ulPayloadLength;
    if( ulPayloadCopyLength > ( sizeof( C2DMessge_Payload ) - 1U ) )
    {
        ulPayloadCopyLength = sizeof( C2DMessge_Payload ) - 1U;
    }
    memcpy( C2DMessge_Payload, ( const char * ) pxMessage->pvMessagePayload, ulPayloadCopyLength );
    C2DMessge_Payload[ ulPayloadCopyLength ] = '\0';
	s_Message_Tx_new.Dest_ID_a8 = IHUB;
	s_Message_Tx_new.Src_ID_u8 =CONSOLE;
	strcpy((char*)s_Message_Tx_new.cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Tx_new.dest_Actor_a8,"IHUB");
	strcpy((char*)s_Message_Tx_new.src_Actor_a8,"CONSOLE");
	s_Message_Tx_new.payload_p8 = (uint8_t*) command_buffer;
	s_Message_Tx_new.payload_size = strlen((char*)C2DMessge_Payload);  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "C2D_MESSAGE", C2DMessge_Payload);  //(char*)pxMessage->pvMessagePayload
	memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
	cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
	strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_IHUB);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(&s_Message_Tx_new);
}
/*-----------------------------------------------------------*/

static void prvDispatchPropertiesUpdate( AzureIoTHubClientPropertiesResponse_t * pxMessage)
{
   //Process twin data
    if(Process_Twin_Commands_Queue != NULL)
    {
    	xQueueReset(Process_Twin_Commands_Queue);
    	xQueueSend(Process_Twin_Commands_Queue, (const char*)pxMessage->pvMessagePayload, QUE_DELAY);
    }
}

static void process_Twin_data(const char *json_data) {
    cJSON *TwinMessage = cJSON_Parse((char*)json_data);
    if (TwinMessage == NULL) {
        return;
    }
    cJSON *datetime_on_item = NULL;
    cJSON *datetime_off_item = NULL;
    cJSON *file_sync_array   = NULL;
    time_t OnEpoch = 0, OffEpoch = 0;
    cJSON *desired = cJSON_GetObjectItemCaseSensitive(TwinMessage, "desired");

    if (desired == NULL)
    {
	    // Extract "defer_ON_Events_until"
    	datetime_on_item = cJSON_GetObjectItem(TwinMessage, "defer_ON_Events_until");
    	// Extract "defer_OFF_Events_until"
		datetime_off_item = cJSON_GetObjectItem(TwinMessage, "defer_OFF_Events_until");
        file_sync_array = cJSON_GetObjectItemCaseSensitive(TwinMessage, "file_sync");
//        actors = cJSON_GetObjectItem(TwinMessage, "actors");
    }
    else
    {
 // Extract "defer_ON_Events_until"
    	datetime_on_item = cJSON_GetObjectItem(desired, "defer_ON_Events_until");
		// Extract "defer_OFF_Events_until"
		datetime_off_item = cJSON_GetObjectItem(desired, "defer_OFF_Events_until");
		file_sync_array = cJSON_GetObjectItemCaseSensitive(desired, "file_sync");
    }

	if (datetime_on_item && cJSON_IsString(datetime_on_item))
	{
		// Parse the date and time
		int year = 0, month = 0, day = 0, hour = 0, minute = 0;
		if (parse_datetime(datetime_on_item->valuestring, &year, &month, &day, &hour, &minute) == 0) {
		} else {
			printf("Failed to parse date-time.\n");
		}
		OnEpoch = convert_to_epoch(year, month, day, hour, minute, 0);
	}

	if (datetime_off_item && cJSON_IsString(datetime_off_item))
	{
		// Parse the date and time
		int year, month, day, hour, minute;
		if (parse_datetime(datetime_off_item->valuestring, &year, &month, &day, &hour, &minute) == 0) {
		} else {
			printf("Failed to parse date-time.\n");
		}
		OffEpoch = convert_to_epoch(year, month, day, hour, minute, 0);
	}

	if((OffEpoch != 0) || (OnEpoch != 0))
	{
		cJSON* root = cJSON_CreateObject();
		if (root != NULL)
		{
//			cJSON_AddNumberToObject(root, "DEFER_ON", OnEpoch);
//			cJSON_AddNumberToObject(root, "DEFER_OFF", OffEpoch);
		    char defer_on_str[21]; // Enough to hold a 64-bit integer as a string, including null terminator
		    snprintf(defer_on_str, sizeof(defer_on_str), "%llu", OnEpoch);
		    cJSON_AddStringToObject(root, "DEFER_ON", defer_on_str);

		    char defer_off_str[21]; // Enough to hold a 64-bit integer as a string, including null terminator
		    snprintf(defer_off_str, sizeof(defer_off_str), "%llu", OffEpoch);
		    cJSON_AddStringToObject(root, "DEFER_OFF", defer_off_str);

			memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
			cJSON_PrintPreallocated(root, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
			cJSON_Delete(root);
			Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", payLoadData_IHUB, strlen(payLoadData_IHUB), "SET");
		}
	}

	if (file_sync_array != NULL && (cJSON_IsArray(file_sync_array)))
	{
			int arraySize = cJSON_GetArraySize(file_sync_array);
			for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(file_sync_array, i);
			if (cJSON_IsObject(element)) {
				cJSON *url = cJSON_GetObjectItemCaseSensitive(element, "Url");
				cJSON *filename = cJSON_GetObjectItemCaseSensitive(element, "FileName");
				cJSON *crc = cJSON_GetObjectItemCaseSensitive(element, "CRC");
				cJSON *FileAck = cJSON_GetObjectItemCaseSensitive(element, "AckIfFileExists");
				bool extracted_bool = cJSON_IsTrue(FileAck);
				if (cJSON_IsString(filename) && cJSON_IsString(url) && cJSON_IsString(crc)) {

				if((strlen(filename->valuestring)!= 0) && (strlen(url->valuestring)!= 0) && (strlen(crc->valuestring)!= 0))
				{
					const char *Filename = NULL, *Url = NULL, *Crc = NULL;
					Filename = filename->valuestring;
					Url = url->valuestring;
					Crc = crc->valuestring;
					cJSON *responseObject1 = cJSON_CreateObject();
					if(responseObject1 != NULL)
					{
						cJSON_AddStringToObject(responseObject1, "FILE_NAME", Filename);
						cJSON_AddStringToObject(responseObject1, "URL", Url);
						cJSON_AddStringToObject(responseObject1, "MODE", "w");
						cJSON_AddStringToObject(responseObject1, "CRC", Crc);
						cJSON_AddBoolToObject(responseObject1, "AckIfFileExists", extracted_bool);
						if(i==0)
						cJSON_AddStringToObject(responseObject1, "PURGE", "Y");
						memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
						cJSON_PrintPreallocated(responseObject1, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
						cJSON_Delete(responseObject1);
						Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_IHUB, strlen(payLoadData_IHUB), "FETCH_FILE");
			 		}
				  }
				}
			}
		}
	}

	if(TwinMessage != NULL)
	{
		cJSON_Delete(TwinMessage);
	}
}

/*-----------------------------------------------------------*/
/**
 * @brief Private property message callback handler.
 *        This handler dispatches the calls to the functions defined in
 *        sample_azure_iot_pnp_data_if.h
 */
static void prvHandleProperties( AzureIoTHubClientPropertiesResponse_t * pxMessage,
                                 void * pvContext)
{
    ( void ) pvContext;

    LogDebug( ( "Property document payload : %.*s \r\n",
                ( int16_t ) pxMessage->ulPayloadLength,
                ( const char * ) pxMessage->pvMessagePayload ) );

    switch( pxMessage->xMessageType )
    {
        case eAzureIoTHubPropertiesRequestedMessage:
        	//printf("\n\n Device property document GET received \n\n");
            LogDebug( ( "Device property document GET received" ) );
            prvDispatchPropertiesUpdate( pxMessage);
            break;

        case eAzureIoTHubPropertiesWritablePropertyMessage:
        	//printf("\n\n Device writeable property received \n\n");
            LogDebug( ( "Device writeable property received" ) );
            prvDispatchPropertiesUpdate( pxMessage);
            break;

        case eAzureIoTHubPropertiesReportedResponseMessage:
        	//printf("\n\n Device reported property response received \n\n");
            LogDebug( ( "Device reported property response received" ) );
            break;

        default:
            LogError( ( "Unknown property message: 0x%08x", pxMessage->xMessageType ) );
//            configASSERT( false );
    }
}
/*-----------------------------------------------------------*/
/**
 * @brief Setup transport credentials.
 */
static uint32_t prvSetupNetworkCredentials( NetworkCredentials_t * pxNetworkCredentials )
{
    pxNetworkCredentials->xDisableSni = pdFALSE;
    /* Set the credentials for establishing a TLS connection. */
    pxNetworkCredentials->pucRootCa = ( const unsigned char * ) democonfigROOT_CA_PEM;
    pxNetworkCredentials->xRootCaSize = sizeof( democonfigROOT_CA_PEM );
    #ifdef democonfigCLIENT_CERTIFICATE_PEM
        pxNetworkCredentials->pucClientCert = ( const unsigned char * ) democonfigCLIENT_CERTIFICATE_PEM;
        pxNetworkCredentials->xClientCertSize = sizeof( democonfigCLIENT_CERTIFICATE_PEM );
        pxNetworkCredentials->pucPrivateKey = ( const unsigned char * ) democonfigCLIENT_PRIVATE_KEY_PEM;
        pxNetworkCredentials->xPrivateKeySize = sizeof( democonfigCLIENT_PRIVATE_KEY_PEM );
    #endif

    return 0;
}
/*-----------------------------------------------------------*/
static uint32_t prvConnectToServerWithBackoffRetries( const char * pcHostName,
                                                      uint32_t port,
                                                      NetworkCredentials_t * pxNetworkCredentials,
                                                      NetworkContext_t * pxNetworkContext )
{
    TlsTransportStatus_t xNetworkStatus;
    BackoffAlgorithmStatus_t xBackoffAlgStatus = BackoffAlgorithmSuccess;
    BackoffAlgorithmContext_t xReconnectParams;
    uint16_t usNextRetryBackOff = 0U;  //500U; //

    /* Initialize reconnect attempts and interval. */
    BackoffAlgorithm_InitializeParams( &xReconnectParams,
                                       sampleazureiotRETRY_BACKOFF_BASE_MS,
                                       sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS,
                                       sampleazureiotRETRY_MAX_ATTEMPTS );

    /* Attempt to connect to IoT Hub. If connection fails, retry after
     * a timeout. Timeout value will exponentially increase till maximum
     * attempts are reached.
     */
    do
    {
        LogInfo( ( "Creating a TLS connection to %s:%u.\r\n", pcHostName, ( uint16_t ) port ) );
        /* Attempt to create a mutually authenticated TLS connection. */
        xNetworkStatus = TLS_Socket_Connect( pxNetworkContext,
                                             pcHostName, port,
                                             pxNetworkCredentials,
                                             sampleazureiotTRANSPORT_SEND_RECV_TIMEOUT_MS,
                                             sampleazureiotTRANSPORT_SEND_RECV_TIMEOUT_MS );

        if( xNetworkStatus != eTLSTransportSuccess )
        {
            /* Generate a random number and calculate backoff value (in milliseconds) for
             * the next connection retry.
             * Note: It is recommended to seed the random number generator with a device-specific
             * entropy source so that possibility of multiple devices retrying failed network operations
             * at similar intervals can be avoided. */
            xBackoffAlgStatus = BackoffAlgorithm_GetNextBackoff( &xReconnectParams, configRAND32(), &usNextRetryBackOff );

            if( xBackoffAlgStatus == BackoffAlgorithmRetriesExhausted )
            {
//            	LogInfo( ( "Connection to the IoT Hub failed, all attempts exhausted." ) );//LogError
            	//printf("Error! Connection to the IoT Hub failed, all attempts exhausted. \n");//LogError
            }
            else if( xBackoffAlgStatus == BackoffAlgorithmSuccess )
            {
//            	LogInfo( ( "Connection to the IoT Hub failed [%d]. "
//                           "Retrying connection with backoff and jitter [%d]ms.",
//                           xNetworkStatus, usNextRetryBackOff ) );//LogWarn
//                vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );

//            	printf("Error! Connection to the IoT Hub failed [%d]. "
//            	                           "Retrying connection with backoff and jitter [%d]ms. \n",
//            	                           xNetworkStatus, usNextRetryBackOff);//LogWarn
				vTaskDelay( pdMS_TO_TICKS( usNextRetryBackOff ) );
            }
        }
    } while( ( xNetworkStatus != eTLSTransportSuccess ) && ( xBackoffAlgStatus == BackoffAlgorithmSuccess ) );

    return xNetworkStatus == eTLSTransportSuccess ? 0 : 1;
}
/**
 * @brief Azure IoT demo task that gets started in the platform specific project.
 *  In this demo task, middleware API's are used to connect to Azure IoT Hub and
 *  function to adhere to the Plug and Play device convention.
 */

bool Azure_MQTTContextIsStale(MQTTContext_t *ctx)
{
	uint32_t timeSinceLastActivity = 0;
	uint32_t timeSincePing = 0;
    if (ctx == NULL || ctx->getTime == NULL)
        return true;  // invalid context = treat as disconnected

    uint32_t now = ctx->getTime();


    // If a ping was sent and we've waited too long for a response
    if (ctx->waitingForPingResp)
    {
        timeSincePing = now - ctx->pingReqSendTimeMs;
        if (timeSincePing > (ctx->keepAliveIntervalSec * 1000))
        {
            return true;
        }
    }

    // Also: if nothing at all has happened in a long time
   timeSinceLastActivity = now - ctx->lastPacketTime;
    if (timeSinceLastActivity > (ctx->keepAliveIntervalSec * 3000))  // 3x keepalive
    {
        return true;
    }
    return false;
}
static void prvAzureDemoTask(void *pvParameters __attribute__((unused)))
{
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
	AMessage_st s_Message_Rx_new;
	s_Message_Rx_new.Dest_ID_a8 = IHUB;
	s_Message_Rx_new.Src_ID_u8 =SYSTEM;
	strcpy((char*)s_Message_Rx_new.dest_Actor_a8,"IHUB");
	strcpy((char*)s_Message_Rx_new.src_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Rx_new.cmdFun_a8,"EVENT");
	Task_data_buffer[0]= '\0';
	s_Message_Rx_new.payload_p8 = (uint8_t*)Task_data_buffer;

    uint32_t ulScratchBufferLength = 0U;
	char count =0, display_msg = 0; //, display_msg1 = 0;
	memset(&xNetworkCredentials, 0, sizeof(xNetworkCredentials));
    memset(&xTransport,0,sizeof(xTransport));
    memset(&xTlsTransportParams,0,sizeof(xTlsTransportParams));
    memset(&xHubOptions,0,sizeof(xHubOptions));

    AzureIoTResult_t xResult = 255;
    uint32_t ulStatus = 255;  // give high initial value
    char Error[120] = {0};
    uint32_t pulIothubHostnameLength =0;
    uint32_t pulIothubDeviceIdLength =0;
    uint32_t IothubConnectionStrLength =0;
    bool xSessionPresent;
    memset(&xAzureIoTHubClient, 0, sizeof(AzureIoTHubClient_t));
    int64_t lastPingTime = get_current_time_ms();

    iHub_conn_status_set(IHUB_CONNECTION_IN_PROGRESS);

    memset(&xNetworkContext,0,sizeof(xNetworkContext));
    xNetworkContext.pParams = NULL;

	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
	cJSON_PrintPreallocated(jsonObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "GET");

	while(1)
	{
	  if (pdTRUE == xQueueReceive(IHUB_Connect_Queue, (void*) (IHUB_str), portMAX_DELAY))
	  {
		  root_new 	  = cJSON_Parse((char*)IHUB_str);
		  if (root_new == NULL)
		  {
			sprintf(IHUB_str1,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(IHUB_str1,&s_Message_Rx_new, payLoadData_IHUB);
			goto exit;
		  }
		  name_new_JSON  = cJSON_GetObjectItem(root_new, "DEVICEID");  // for SQL
		  if((name_new_JSON != NULL)&&(name_new_JSON->valuestring != NULL)){
		  strcpy(DeviceId,(char*)name_new_JSON->valuestring);
		  }

		  cJSON_Delete(root_new);
		  uint8_t * pucIotHubHostname = ( uint8_t * ) &iHub_Para.HostName;
		  uint8_t * pucIotHubDeviceId = ( uint8_t * ) DeviceId;
		  uint8_t * IotHubConnectionStr = ( uint8_t * ) &iHub_Para.PrimaryKey;
		  pulIothubHostnameLength = strlen((char*) &iHub_Para.HostName );
		  pulIothubDeviceIdLength = strlen((char*)DeviceId );
		  IothubConnectionStrLength = strlen((char*)&iHub_Para.PrimaryKey );

		Add_Response_msg("Connecting to IoT Server...", &s_Message_Rx_new, payLoadData_IHUB);
		/* Initialize Azure IoT Middleware.  */
		if( AzureIoT_Init() != eAzureIoTSuccess )
		{
			Add_Response_msg("AzureIoT Init failed", &s_Message_Rx_new, payLoadData_IHUB);
			Delete_task_flag = 1;
			goto exit;
		}
		ulStatus = prvSetupNetworkCredentials( &xNetworkCredentials );
		if( ulStatus != 0 )
		{
			Add_Response_msg("Setup network credentials failed", &s_Message_Rx_new, payLoadData_IHUB);
			Delete_task_flag = 1;
			goto exit;
		}
		xNetworkContext.pParams = &xTlsTransportParams;
		for( ; ; )
		{
			if( xAzureSample_IsConnectedToInternet() )
			{
				ulStatus = prvConnectToServerWithBackoffRetries( ( const char * ) pucIotHubHostname,
																 democonfigIOTHUB_PORT,
																 &xNetworkCredentials, &xNetworkContext );
				if( ulStatus != 0 )
				{
					update_device_online_status(0);
					sprintf(IHUB_str1, "Connect To server with back off retries failed, NTP_Sync_Status = %d", sntp_get_sync_status());
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
					cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
					memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
					cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
					strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);

			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");

					cJSON_Delete(responseObject);
					Delete_task_flag = 1;
					console_send_responce_to_console_xface(&s_Message_Rx_new);
					goto exit;
				}

				/* Fill in Transport Interface send and receive function pointers. */
				xTransport.pxNetworkContext = &xNetworkContext;
				xTransport.xSend = TLS_Socket_Send;
				xTransport.xRecv = TLS_Socket_Recv;
				/* Init IoT Hub option */
				xResult = AzureIoTHubClient_OptionsInit( &xHubOptions );
				if( xResult != eAzureIoTSuccess)
				{
					update_device_online_status(0);
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client options Init failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");

					Delete_task_flag = 1;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					iHub_Para.IHUB_status_time_u64  = current_epos_sec;
					cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
					cJSON_AddNumberToObject(responseObject, "ERROR_NUM",xResult);
					memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
					cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
					strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);

					cJSON_Delete(responseObject);
					console_send_responce_to_console_xface(&s_Message_Rx_new);
					goto exit;
				}
				xHubOptions.pucModuleID = ( const uint8_t * ) democonfigMODULE_ID;
				xHubOptions.ulModuleIDLength = sizeof( democonfigMODULE_ID ) - 1;
				xHubOptions.pucModelID = ( const uint8_t * ) sampleazureiotMODEL_ID;
				xHubOptions.ulModelIDLength = sizeof( sampleazureiotMODEL_ID ) - 1;

				xResult = AzureIoTHubClient_Init( &xAzureIoTHubClient,
						pucIotHubHostname, pulIothubHostnameLength,
						pucIotHubDeviceId, pulIothubDeviceIdLength,
												  &xHubOptions,
												  ucMQTTMessageBuffer, sizeof( ucMQTTMessageBuffer ),
												  ullGetUnixTime,
												  &xTransport );
				if( xResult != eAzureIoTSuccess)
				{
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client Init failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}
					xResult = AzureIoTHubClient_SetSymmetricKey( &xAzureIoTHubClient,
																 ( const uint8_t * ) IotHubConnectionStr,
																 IothubConnectionStrLength,
																 Crypto_HMAC );
					if( xResult != eAzureIoTSuccess)
					{
						ErrortoStringConverter(xResult , Error);
						sprintf(IHUB_str1,"Azure IoT Hub client set symmetric key failed, Error is %s", Error);
						Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
						Delete_task_flag = 1;
						goto exit;
					}

					sprintf(IHUB_str1, "Creating an MQTT connection to %s.", pucIotHubHostname );
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);

				xResult = AzureIoTHubClient_Connect( &xAzureIoTHubClient,
													 true, &xSessionPresent,
													 sampleazureiotCONNACK_RECV_TIMEOUT_MS );
			 	if(xSessionPresent == true)
				{
		            Add_Response_msg("Error! CONNACK session present bit set. Previous session is in progress.", &s_Message_Rx_new, payLoadData_IHUB);
				}
				if(xResult == eAzureIoTSuccess)
				{
					update_device_online_status(1);
					deinit_Flag = 0;

					iHub_conn_status_set(IHUB_CONNECTED);

					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					iHub_Para.IHUB_status_time_u64  = current_epos_sec;
					cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "CONNECTED");  //(char*)pxMessage->pucCommandName
					cJSON_AddNumberToObject(responseObject, "ERROR_NUM",xResult);
					memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
					cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
					strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");

					cJSON_Delete(responseObject);
					console_send_responce_to_console_xface(&s_Message_Rx_new);

					cJSON *my_JSON1  	= cJSON_CreateArray();
					cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("WIFI_STATUS"));
					cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
					cJSON *jsonObject = cJSON_CreateObject();
					cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
					memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
					cJSON_PrintPreallocated(jsonObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
					cJSON_Delete(jsonObject);
					Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData_IHUB, strlen(payLoadData_IHUB), "GET");

					esp_err_t err;
					if(esp_timer_is_active(periodic_timer) == true)
					{
						//printf("\n timer esp_timer_restart \n");
						err = esp_timer_restart(periodic_timer, CONNECTIVITY_SUPERVISOR_INTERVAL_US);

						if(err != ESP_OK)
							printf("\n\n Error in restarting periodic timer. err = %d\n\n", err);
					}
				}
				else
				{
					ErrortoStringConverter(xResult , Error);
					iHub_conn_status_set(IHUB_NOT_CONNECTED);

					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					iHub_Para.IHUB_status_time_u64  = current_epos_sec;
					sprintf(IHUB_str1,"AzureIoTHubClient_Connect failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}
				xResult = AzureIoTHubClient_SubscribeCommand( &xAzureIoTHubClient, prvHandleCommand,
															  &xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT );
				if( xResult != eAzureIoTSuccess)
				{
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client subscribe command failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}

				xResult = AzureIoTHubClient_SubscribeProperties( &xAzureIoTHubClient, prvHandleProperties,
																 &xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT );

				if( xResult != eAzureIoTSuccess)
				{
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client subscribe properties failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}

				/* Get property document after initial connection */
				xResult = AzureIoTHubClient_RequestPropertiesAsync( &xAzureIoTHubClient );
				if( xResult != eAzureIoTSuccess)
				{
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client request properties async failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}
				xResult = AzureIoTHubClient_SubscribeCloudToDeviceMessage( &xAzureIoTHubClient, prvHandleCtoDCommand,
						&xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT);
				if( xResult != eAzureIoTSuccess)
				{
					ErrortoStringConverter(xResult , Error);
					sprintf(IHUB_str1,"Azure IoT Hub client subscribe CloudToDevice message failed, Error is %s", Error);
					Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
			        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
					Delete_task_flag = 1;
					goto exit;
				}

				/* Publish messages with QoS1, send and process Keep alive messages. */
				for( ; xAzureSample_IsConnectedToInternet(); )   // After IHUB connection, this loop is continuously executed
				{
					uint64_t currentTime1 = get_current_time_ms();
					if(ihubTimeActive == 0)
					{
						ihubTimeActive = currentTime1;
					}

					// Check if it's time to send a ping

					if (((currentTime1 - ihubTimeActive)/1000) >= (iHub_Para.D2C_interval_second_u32))
					{
						ihubTimeActive = currentTime1;
						memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
						Send_CMD_To_Other_Actor(WIFI,"WIFI", payLoadData_IHUB, strlen(payLoadData_IHUB), "HEARTBEAT");
					}

					/* Hook for sending Telemetry */
					if( ( ulCreateTelemetry( ucScratchBuffer, sizeof( ucScratchBuffer ), &ulScratchBufferLength ) == 0 ) &&
						( ulScratchBufferLength > 0 ) )
					{
						if(u8D2Cmessage ==1){  // Flag use for sending device-to-cloud messages
						strcpy((char*)ucScratchBuffer,D2CPayload);
						ulScratchBufferLength = strlen((char*)ucScratchBuffer);
						u8D2Cmessage =0;
#if defined(B394)
						/* Set up message properties for D2C message */
						uint8_t ucPropertyBuffer[128];
						AzureIoTMessageProperties_t xMessageProperties;
						AzureIoTResult_t xPropertiesResult;


						xPropertiesResult = AzureIoTMessage_PropertiesInit(&xMessageProperties, ucPropertyBuffer, 0, sizeof(ucPropertyBuffer));
						if (xPropertiesResult == eAzureIoTSuccess) {
							xPropertiesResult = AzureIoTMessage_PropertiesAppend(&xMessageProperties,
																	 (const uint8_t*)"messageType", 11,
																	 (const uint8_t*)"automationLink", 14);


							xResult = AzureIoTHubClient_SendTelemetry( &xAzureIoTHubClient,
															   ucScratchBuffer, ulScratchBufferLength,
															   (xPropertiesResult == eAzureIoTSuccess) ? &xMessageProperties : NULL,
															   eAzureIoTHubMessageQoS1, NULL);
						}
#else
						xResult = AzureIoTHubClient_SendTelemetry( &xAzureIoTHubClient,
																   ucScratchBuffer, ulScratchBufferLength,
																   NULL, eAzureIoTHubMessageQoS1, NULL);

						if(xResult == eAzureIoTSuccess)
					    {
							Add_Response_msg("D2C message is sent.", &s_Message_Rx_new, payLoadData_IHUB);

					    }
#endif
						else
						{
							ErrortoStringConverter(xResult , Error);
							sprintf(IHUB_str1, "D2C message is not sent, Error = %s", Error);
							Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
						}
					  }
					}

					if (pdTRUE == xQueueReceive(Process_Twin_Queue, (void*)(IHUB_buffer),sampleazureiotDELAY_BETWEEN_PUBLISHES_TICKS ))//portMAX_DELAY
					 {
		                updateReportedPropertiesWithConnectivityInfo(IHUB_buffer);
					 }
					Azure_RefreshToken_IfExpiring(&s_Message_Rx_new);  // SAS token refresh logic
					if(Delete_task_flag == 1)
					{
						goto exit;
					}
					LogInfo( ( "Attempt to receive publish message from IoT Hub.\r\n" ) );
					xResult = AzureIoTHubClient_ProcessLoop( &xAzureIoTHubClient,
															 sampleazureiotPROCESS_LOOP_TIMEOUT_MS );
					if( xResult != eAzureIoTSuccess )
					{
						update_device_online_status(0);
						ErrortoStringConverter(xResult , Error);
						sprintf(IHUB_str1, "Azure IoTHub client process loop failed, Error = %s. Attempting in-task recovery...", Error);
						Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");

						/* ---- Phase 1: try to recover the MQTT/TLS session in-task before
						 * broadcasting DISCONNECTED. A broadcast would make the monitor run a
						 * full Reset_Stack (tearing down WiFi/HTTP too), turning a short MQTT
						 * blip into a long outage. Only if recovery is exhausted do we fall
						 * through to the original teardown so the monitor can escalate. ---- */
						if( prvAttemptInTaskReconnect(&s_Message_Rx_new, xResult, &lastPingTime) == 1 )
						{
							continue;   /* session restored; resume the connected loop */
						}

						Delete_task_flag = 1;
						iHub_conn_status_set(IHUB_DISCONNECTED);

						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						iHub_Para.IHUB_status_time_u64  = current_epos_sec;
						cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
						cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
						cJSON_AddNumberToObject(responseObject, "ERROR_NUM",xResult);
						memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
						cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
						strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);
						cJSON_Delete(responseObject);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");
						console_send_responce_to_console_xface(&s_Message_Rx_new);
						xResult = AzureIoTHubClient_UnsubscribeProperties( &xAzureIoTHubClient );
						if( xResult != eAzureIoTSuccess )
						{
							ErrortoStringConverter(xResult , Error);
							sprintf(IHUB_str1, "Azure IoTHub client unsubscribe properties failed, Error = %s", Error);
					        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
							Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
						}

						xResult = AzureIoTHubClient_UnsubscribeCommand( &xAzureIoTHubClient );
						if( xResult != eAzureIoTSuccess )
						{
							ErrortoStringConverter(xResult , Error);
							sprintf(IHUB_str1, "Azure IoTHub client unsubscribe command failed, Error = %s", Error);
					        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
							Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
						}

						xResult = AzureIoTHubClient_UnsubscribeCloudToDeviceMessage(&xAzureIoTHubClient);
						if(xResult != eAzureIoTSuccess)
						{
							Add_Response_msg("AzureIoTHubClient_UnsubscribeCloudToDeviceMessage failed",&s_Message_Rx_new, payLoadData_IHUB);					
					        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
						}

						/* Send an MQTT Disconnect packet over the already connected TLS over
						 * TCP connection. There is no corresponding response for the disconnect
						 * packet. After sending disconnect, client must close the network
						 * connection. */
						xResult = AzureIoTHubClient_Disconnect( &xAzureIoTHubClient );
						if(xResult == eAzureIoTSuccess)
						{
							update_device_online_status(0);
							Delete_task_flag = 1;
							iHub_conn_status_set(IHUB_DISCONNECTED);
					     }

						/* Close the network connection.  */
						 TLS_Socket_Disconnect( &xNetworkContext );

						 // Force a fresh SAS token on next Connect
						 if( xResult == eAzureIoTErrorMQTTTimeout)
						 {
					            uint32_t ulPasswordLength = 0;
					            AzureIoTMQTTConnectInfo_t xConnectInfo = { 0 };
					            /* Use working buffer for username/password */
					            xConnectInfo.pcUserName = xAzureIoTHubClient._internal.pucWorkingBuffer;
					            xConnectInfo.pcPassword = xConnectInfo.pcUserName + azureiotconfigUSERNAME_MAX;

					            if( ( xAzureIoTHubClient._internal.pxTokenRefresh ) &&
										 ( xAzureIoTHubClient._internal.pxTokenRefresh(&xAzureIoTHubClient,
										  xAzureIoTHubClient._internal.xTimeFunction() +
										  azureiotconfigDEFAULT_TOKEN_TIMEOUT_IN_SEC,
										  xAzureIoTHubClient._internal.pucSymmetricKey,
										  xAzureIoTHubClient._internal.ulSymmetricKeyLength,
										  ( uint8_t * ) xConnectInfo.pcPassword, azureiotconfigPASSWORD_MAX,
										  &ulPasswordLength ) ) )
					                    {
					                       printf("\n Error! Failed to generate new SAS token \n");
					                        xResult = eAzureIoTErrorFailed;
					                    }
						 }
						vStartDemoTask_handle = NULL;  // delete the task
						vTaskDelete(vStartDemoTask_handle);
					}
					//send mqtt ping request
					if( xAzureSample_IsConnectedToInternet() )
					{
						int64_t currentTime = get_current_time_ms();
						// Check if it's time to send a ping
						if (currentTime - lastPingTime >= (PING_INTERVAL_MICROSECONDS/1000))
						{
							if (Azure_MQTTContextIsStale(&xAzureIoTHubClient._internal.xMQTTContext))
							{
							    Add_Response_msg("Error! Azure connection stale. Attempting in-task recovery....", &s_Message_Rx_new, payLoadData_IHUB);
							    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", "Azure connection stale. Attempting in-task recovery", strlen("Azure connection stale. Attempting in-task recovery"), "SAVE_WIFI_AUDIT_LOG");

							    /* Phase 1: a stale context means keepalive was lost; treat like an
							     * MQTT timeout (forces a fresh SAS token) and try to recover in-task
							     * before broadcasting DISCONNECTED. */
							    if (prvAttemptInTaskReconnect(&s_Message_Rx_new, eAzureIoTErrorMQTTTimeout, &lastPingTime) != 1)
							    {
								    cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
									cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
									memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
									cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
									strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);
									cJSON_Delete(responseObject);
							        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");
									console_send_responce_to_console_xface(&s_Message_Rx_new);
								    Delete_task_flag = 1;
							    }
							}
							lastPingTime = currentTime;  // Update the last ping time
						}
					}

					if(Delete_task_flag == 1)
					{
						goto exit;
					}
					vTaskDelay( sampleazureiotDELAY_BETWEEN_PUBLISHES_TICKS );
				}
			}

			else
			{
				count++;
				if((count > 100) && (display_msg == 0))
				{
					display_msg = 1;
					Add_Response_msg("Kindly connect to internet at first.", &s_Message_Rx_new, payLoadData_IHUB);
					vStartDemoTask_handle = NULL;
					vTaskDelete(vStartDemoTask_handle);
				}
			}
	exit:
			if(Delete_task_flag == 1)
			{
				if(( xAzureSample_IsConnectedToInternet() ) && (iHub_Para.conn_status == IHUB_NOT_CONNECTED))
				{
					/* Send an MQTT Disconnect packet over the already connected TLS over
					 * TCP connection. There is no corresponding response for the disconnect
					 * packet. After sending disconnect, client must close the network
					 * connection. */
					xResult = AzureIoTHubClient_Disconnect( &xAzureIoTHubClient );
					if(xResult == eAzureIoTSuccess)
					{
						update_device_online_status(0);
						iHub_conn_status_set(IHUB_DISCONNECTED);

						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						iHub_Para.IHUB_status_time_u64  = current_epos_sec;

						cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
						cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
						cJSON_AddNumberToObject(responseObject, "ERROR_NUM",xResult);
						memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
						cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
						strcpy((char*)s_Message_Rx_new.payload_p8, payLoadData_IHUB);
						cJSON_Delete(responseObject);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");
						console_send_responce_to_console_xface(&s_Message_Rx_new);
					}
					else
					{
						printf("\n\n error in IHUB disconnect \n\n");
					}
				}
				else if(( xAzureSample_IsConnectedToInternet() ) && (iHub_Para.conn_status == IHUB_CONNECTED))
				{
					xResult = AzureIoTHubClient_UnsubscribeProperties( &xAzureIoTHubClient );
					if( xResult != eAzureIoTSuccess )
					{
						ErrortoStringConverter(xResult , Error);
						sprintf(IHUB_str1, "Azure IoTHub client unsubscribe properties failed, Error = %s", Error);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
						Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
					}

					xResult = AzureIoTHubClient_UnsubscribeCommand( &xAzureIoTHubClient );
					if( xResult != eAzureIoTSuccess )
					{
						ErrortoStringConverter(xResult , Error);
						sprintf(IHUB_str1, "Azure IoTHub client unsubscribe command failed, Error = %s", Error);
				        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
						Add_Response_msg(IHUB_str1, &s_Message_Rx_new, payLoadData_IHUB);
					}

					xResult = AzureIoTHubClient_UnsubscribeCloudToDeviceMessage(&xAzureIoTHubClient);
					if(xResult != eAzureIoTSuccess)
					{
						Add_Response_msg("AzureIoTHubClient_UnsubscribeCloudToDeviceMessage failed",&s_Message_Rx_new, payLoadData_IHUB);
					}
					/* Send an MQTT Disconnect packet over the already connected TLS over
					 * TCP connection. There is no corresponding response for the disconnect
					 * packet. After sending disconnect, client must close the network
					 * connection. */
					xResult = AzureIoTHubClient_Disconnect( &xAzureIoTHubClient );
					if(xResult == eAzureIoTSuccess)
					{
						update_device_online_status(0);
						iHub_conn_status_set(IHUB_DISCONNECTED);

						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						iHub_Para.IHUB_status_time_u64  = current_epos_sec;
					}
				}
				/* Close the network connection.  */
				 TLS_Socket_Disconnect( &xNetworkContext );
				 iHub_conn_status_set(IHUB_DISCONNECTED);

				vStartDemoTask_handle = NULL;
				vTaskDelete(vStartDemoTask_handle);
			}
			vTaskDelay(10 / portTICK_PERIOD_MS);
		}
	  }// end of if que receive

	} // end of while(1)
}
/*-----------------------------------------------------------*/
/*
 * @brief Create the task that demonstrates the AzureIoTHub demo
 */
void vStartDemoTask(AMessage_st* s_Message_Rx)
{
	if(vStartDemoTask_handle == NULL)
	{
		vStartDemoTask_handle = xTaskCreateStaticPinnedToCore(
						prvAzureDemoTask,                 // Task function
						"AzureDemoTask",            // Task name
						democonfigDEMO_STACKSIZE,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						democonfigDEMO_PRIORITY,                       // Task priority
						xTaskStack2,              // Pointer to task stack (allocated in PSRAM)
						&xIHUBSubTaskBuffer,             // Pointer to task control block
						1
					);

	   if (vStartDemoTask_handle == NULL) {
				printf("Failed to create IHUB task. \n");
			}
	}
	// Create queue to process twin message commands
	if (Process_Twin_Commands_Queue == NULL)
	{
		Process_Twin_Commands_Queue = xQueueCreateStatic(PROCESS_TWIN_COMMAND_QUEUE_LENGTH, PROCESS_TWIN_COMMAND_QUEUE_ITEMSIZE, Twin_Command_pucQueueStorage, &Twin_Command_pxQueueBuffer);
		if (Process_Twin_Commands_Queue == NULL)
		{
			printf("Free heap Now Failed to create Process_Twin_Commands_Queue\n");
		}
	}
	//Create task to process twin message commands
	if(TwinCommandTask_handle == NULL)
	{
		TwinCommandTask_handle = xTaskCreateStaticPinnedToCore(
						TwinCommandProcessTask,                 // Task function
						"TwinCommandProcessTask",            // Task name
						TWIN_COMMAND_PROCESS_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						TWIN_COMMAND_PROCESS_PRIORITY,                       // Task priority
						TwinCommandProcessxTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xProcess_Twin_CommandTaskBuffer,             // Pointer to task control block
						1
					);

	   if (TwinCommandTask_handle == NULL) {
				printf("Failed to create Twin process Command task. \n");
			}
	}
}
/*-----------------------------------------------------------*/

/*-----------------------------------------------------------*/

static void iHub_Connect(AMessage_st* s_Message_Rx)
{
	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("NET_STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData, strlen(payLoadData), "GET");

	if(IHUB_Connect_Queue == NULL)
	{
		IHUB_Connect_Queue = xQueueCreateStatic(IHUB_CONNECT_QUEUE_LENGTH, IHUB_CONNECT_QUEUE_ITEMSIZE, IHUB_pucQueueStorage, &IHUB_pxQueueBuffer);
//		IHUB_Connect_Queue = xQueueCreateInPSRAM(IHUB_CONNECT_QUEUE_LENGTH, IHUB_CONNECT_QUEUE_ITEMSIZE, &IHUB_pucQueueStorage, &IHUB_pxQueueBuffer);
		if (IHUB_Connect_Queue == NULL) {
			Add_Response_msg("Error! IHUB_Connect_Queue is not created.",s_Message_Rx, payLoadData);
			return;
		}
	}
	Delete_task_flag = 0;
	vStartDemoTask(s_Message_Rx);
}


void IHUB_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
		if (FirstiHubEntry == false)
		{
			if(strcmp((char*)s_Message->cmdFun_a8, "DEINIT") == 0)
				return;
			memset(&xIHUBTaskBuffer, 0, sizeof(xIHUBTaskBuffer));
			init(0,0);
		}

		uint8_t state = xQueueSend(msg_Rx_Queue, msg, QUE_DELAY); //100

		if (state != pdTRUE)
		{
			if(s_Message->payload_p8 != NULL){
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
			}
			if (state == errQUEUE_FULL)
			{
				printf("<IHUB.ERROR(IHUB RX Queue is full)\n");
			}
			else
			{
				printf("<IHUB.ERROR(IHUB RX Queue send unsuccessful)\n");
			}
		}
}//	IHUB_ConsolWriteToActor


static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	memset(payLoadData_new,0,MAX_JSON_PAYLOAD_BYTES);//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
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
} //	COP_add_response_to_COP_Tx_Queue

static void ErrortoStringConverter(AzureIoTResult_t xResult, char* Error_str)
{
	switch (xResult)
	{
		case eAzureIoTErrorFailed: strcpy(Error_str, "Failed to establish MQTT connection. Kindly check the IHUB parameters and retry.");  break;                /**< There was a failure. */
		case eAzureIoTErrorInvalidArgument: strcpy(Error_str, "Input argument does not comply with the expected range of values.");  break;
		case eAzureIoTErrorPending: strcpy(Error_str, "The status of the operation is pending.");  break;
		case eAzureIoTErrorOutOfMemory: strcpy(Error_str, "The system is out of memory.");  break;
		case eAzureIoTErrorInitFailed: strcpy(Error_str, "The initialization failed.");  break;
		case eAzureIoTErrorSubackWaitTimeout: strcpy(Error_str, "There was timeout while waiting for SUBACK.");  break;
		case eAzureIoTErrorTopicNotSubscribed: strcpy(Error_str, "Topic not subscribed.");  break;
		case eAzureIoTErrorPublishFailed: strcpy(Error_str, "Failed to publish.");  break;
		case eAzureIoTErrorSubscribeFailed: strcpy(Error_str, "Failed to subscribe.");  break;
		case eAzureIoTErrorUnsubscribeFailed: strcpy(Error_str, "Failed to unsubscribe.");  break;
		case eAzureIoTErrorServerError: strcpy(Error_str, "There was a server error in registration.");  break;
		case eAzureIoTErrorItemNotFound: strcpy(Error_str, "The item was not found.");  break;
		case eAzureIoTErrorTopicNoMatch: strcpy(Error_str, "The received message was not for the currently processed feature.");  break;
		case eAzureIoTErrorTokenGenerationFailed: strcpy(Error_str, "There was a failure.");  break;
		case eAzureIoTErrorEndOfProperties: strcpy(Error_str, "End of properties when iterating with AzureIoTHubClientProperties_GetNextComponentProperty().");  break;
		case eAzureIoTErrorInvalidResponse: strcpy(Error_str, "Invalid response from server.");  break;
		case eAzureIoTErrorUnexpectedChar: strcpy(Error_str, "Input can't be successfully parsed.");  break;
		case eAzureIoTErrorJSONInvalidState: strcpy(Error_str, "JSON invalid state.");  break;
		case eAzureIoTErrorJSONNestingOverflow: strcpy(Error_str, "The JSON depth is too large.");  break;
		case eAzureIoTErrorJSONReaderDone: strcpy(Error_str, "No more JSON text left to process.");  break;
		case eAzureIoTErrorMQTTServerRefused: strcpy(Error_str, "MQTT server refused the request (Wrong primary key/disabled from IoT server)");  break;
		case eAzureIoTErrorMQTTBadParameter: strcpy(Error_str, "Invalid host name/ Bad MQTT parameter");  break;
		case eAzureIoTErrorMQTTTimeout: strcpy(Error_str, "MQTT Timeout");  break;
		default : strcpy(Error_str, "There was a failure.");  break;
	}
	strcpy((char*)&iHub_Para.IHUB_disconnect_reason,Error_str);
}
////------------------------- iHub Functions Ends ------------------------------//

static void Process_Post_Method_Response(AMessage_st* s_Message_Rx, char* directmethodpayload)
{
//	cJSON *in_JSON = NULL;
//	cJSON *name_JSON = NULL;
//	cJSON *head_JSON = NULL;
	cJSON *root = NULL;
	char str[100] = {0};
	//POST_RESPONSE_FLag = 0;
	root = cJSON_Parse((char*) directmethodpayload);
	if (root == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_IHUB);
		return;
	}

    cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(root, "methods");
    if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
    {
    	int arraySize = cJSON_GetArraySize(methodsArray);

		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(methodsArray, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL)) {
				/* execute the command */
				size_t cmd_len = strlen(element->valuestring);
				if (cmd_len >= sizeof(line)) {
#ifdef ENABLE_PRINT_MSG
					printf("ExecuteCommand skipped: method length %u exceeds limit %u\n",
						(unsigned)cmd_len, (unsigned)(sizeof(line) - 1U));
#endif
					continue;
				}
				memcpy(line, element->valuestring, cmd_len + 1U);
				int ret;
				esp_err_t err = esp_console_run_Custom(line, &ret, THIS_ACTOR);
				if (err == ESP_ERR_NOT_FOUND) {
#ifdef ENABLE_PRINT_MSG
					printf("Unrecognized command\n");
#endif
				} else if (err == ESP_ERR_INVALID_ARG) {
					// command was empty
				} else if (err == ESP_OK && ret != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
					printf("Command returned non-zero error code: 0x%x (%s)\n", ret,esp_err_to_name(ret));
#endif

				} else if (err != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
					printf("Internal error: %s\n", esp_err_to_name(err));
#endif
				}
			}
		}
        cJSON_Delete(root);
        return;
    }
   cJSON_Delete(root);
}

static void Deinit_Actor(AMessage_st* s_Message_Rx)
{
	if(FirstiHubEntry)
	{
		FirstiHubEntry = false;
		Delete_task_flag = 1; // delete the IHUB task
		if(iHubHandle != NULL)
		{
			vTaskDelete(iHubHandle);
			iHubHandle = NULL;
		}
	}
}

// Function to update reported properties with connectivity information
static void updateReportedPropertiesWithConnectivityInfo(char* buffer)
{
    AzureIoTResult_t xResult;
    ulReportedPropertiesUpdateLength = strlen(buffer);
    if (ulReportedPropertiesUpdateLength > sizeof(ucReportedPropertiesUpdate))
    {
        // Handle error: reported properties string too long
       	printf("\n Error! reported properties string too long \n\n");
        return;
    }
    strcpy((char *)ucReportedPropertiesUpdate, buffer);
    xResult = AzureIoTHubClient_SendPropertiesReported(&xAzureIoTHubClient,
                                                       ucReportedPropertiesUpdate,
                                                       ulReportedPropertiesUpdateLength,
                                                       NULL);
    if (xResult != eAzureIoTSuccess)
    {
        // Handle error: failed to send reported properties
    	printf("\n Error! AzureIoTHubClient_SendPropertiesReported failed. xResult = %d \n\n",xResult);
    	return;
    }
}

static void sendLedState(const char *stateName, int duration, char *category) {
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    cJSON_AddStringToObject(responseObject, "category", category);

    // Print the JSON object as a string
	memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
	cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);

    // Free the allocated memory
    cJSON_Delete(responseObject);

    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoadData_IHUB, strlen(payLoadData_IHUB), "SETSTATE");


}

static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {

	uint8_t out_val[128]  	=  {0}; //(uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
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
	memset(payLoadData_Server,0,sizeof(payLoadData_Server));
	cJSON_PrintPreallocated(my_JSON, payLoadData_Server, sizeof(payLoadData_Server), false);
	uint8_t *newpointer = (uint8_t*) heap_caps_calloc((strlen((char*) payLoadData_Server) + 1), sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payLoadData_Server);
	s_Message_Tx.payload_size = strlen((char*)payLoadData_Server);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor

// Function to set device online status and call set_to_other_actor with "BLE" and "ONLINE_STATUS"
static void update_device_online_status(uint8_t status) {
    set_to_other_actor("BLE", U_INT8, "ONLINE_STATUS", &status);
}

static void periodic_timer_callback(void* arg)
{
	const int64_t now = esp_timer_get_time();

	//printf("After 10 second time, check health \n");

	if (boot_time_us == 0) {

		boot_time_us = now;
	}

	if (!connectivity_supervisor_is_armed(now)) {
		return;
	}

	const bool healthy_connection = ihub_connected_now &&
		((now - last_successful_activity_us) <= CONNECTIVITY_ACTIVITY_TIMEOUT_US);

	if (healthy_connection) {
		last_successful_activity_us = now;
		disconnect_start_us = 0;
		return;
	}


	if (disconnect_start_us == 0) {
		disconnect_start_us = now;
	}

	if ((now - disconnect_start_us) >= CONNECTIVITY_DISCONNECT_TIMEOUT_US) {
		printf("\n\n IHUB disconnected for %" PRId64 " seconds. Resetting the device... \n\n",
		       (now - disconnect_start_us) / 1000000);
		esp_timer_stop(periodic_timer);
		Restart_ESP_Xface(1);
	}
}

static void Get_Device_Info(void *pvParameters __attribute__((unused)))
{
	vTaskDelay(2 * 60000 / portTICK_PERIOD_MS);   // delay is given to get correct sunrise and sunset time after NTP sync

//	cJSON *my_JSON1  	= cJSON_CreateArray();
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LOCOFFSET"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("MIDPOFFSET"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LEDSPACINGCH1"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SET_LED_S_CH1"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("OFFSETCH1"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REVDIRCH1"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SCALECH1"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LEDSPACINGCH2"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SET_LED_S_CH2"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("OFFSETCH2"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REVDIRCH2"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SCALECH2"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LEDSPACINGCH3"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SET_LED_S_CH3"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("OFFSETCH3"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REVDIRCH3"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SCALECH3"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LEDSPACINGCH4"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SET_LED_S_CH4"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("OFFSETCH4"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REVDIRCH4"));
//	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SCALECH4"));
//	cJSON *jsonObject1 = cJSON_CreateObject();
//	cJSON_AddItemToObject(jsonObject1, "Property_Names", my_JSON1);
//
//    // Convert the JSON object to a string
//	memset(g_jsonBuffer,0,sizeof(g_jsonBuffer));
//	cJSON_PrintPreallocated(jsonObject1, g_jsonBuffer, sizeof(g_jsonBuffer), false);
//    cJSON_Delete(jsonObject1);
//    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", g_jsonBuffer, strlen(g_jsonBuffer), "GET");

	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", "/0", 0, "WHO_AM_I");

	DeviceInfo_Handle = NULL ;
	vTaskDelete(DeviceInfo_Handle);
}

/**
 * @brief Convert date and time to epoch timestamp.
 *
 * @param year  The year (e.g., 2024).
 * @param month The month (1-12).
 * @param day   The day of the month (1-31).
 * @param hour  The hour (0-23).
 * @param minute The minute (0-59).
 * @param second The second (0-59).
 * @return The epoch timestamp, or -1 if the conversion fails.
 */
static time_t convert_to_epoch(int year, int month, int day, int hour, int minute, int second) {
    struct tm timeinfo = {0};

    // Fill the struct tm with provided values
    timeinfo.tm_year = year - 1900; // tm_year is year since 1900
    timeinfo.tm_mon  = month - 1;  // tm_mon is 0-based (0 = January)
    timeinfo.tm_mday = day;
    timeinfo.tm_hour = hour;
    timeinfo.tm_min  = minute;
    timeinfo.tm_sec  = second;

    // Convert to epoch time
    time_t epoch = (mktime(&timeinfo))*1000;	//Convert in millisecond

    if (epoch == -1) {
        // mktime failed
        printf("Error: Unable to convert time to epoch.\n");
    }
    return epoch;
}


/**
 * @brief Parse a date-time string and extract date, month, year, hour, and minute.
 *
 * @param datetime The date-time string in "YYYY/MM/DD HH:MM" format.
 * @param year Pointer to store the year.
 * @param month Pointer to store the month.
 * @param day Pointer to store the day.
 * @param hour Pointer to store the hour.
 * @param minute Pointer to store the minute.
 * @return 0 on success, -1 on failure.
 */
static int parse_datetime(const char* datetime, int* year, int* month, int* day, int* hour, int* minute) {
    // Validate the input
    if (!datetime || !year || !month || !day || !hour || !minute) {
        return -1;
    }
    // Parse the date-time string
    int result = sscanf(datetime, "%d/%d/%d %d:%d", year, month, day, hour, minute);
    if (result != 5) {
        return -1; // Parsing failed
    }
    return 0; // Success
}

#define TOKEN_EXPIRY_THRESHOLD_SEC  120  // Refresh if less than 2 minutes left
static uint64_t lastTokenRefreshTime = 0;

static void Disconnect_IHUB(AMessage_st* s_Message_Rx_new)
{
	AzureIoTResult_t xResult;
	char Error[120] = {0};
	xResult = AzureIoTHubClient_UnsubscribeProperties( &xAzureIoTHubClient );
	if( xResult != eAzureIoTSuccess )
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1, "Azure IoTHub client unsubscribe properties failed, Error = %s", Error);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
	}

	xResult = AzureIoTHubClient_UnsubscribeCommand( &xAzureIoTHubClient );
	if( xResult != eAzureIoTSuccess )
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1, "Azure IoTHub client unsubscribe command failed, Error = %s", Error);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
	}

	xResult = AzureIoTHubClient_UnsubscribeCloudToDeviceMessage(&xAzureIoTHubClient);
	if(xResult != eAzureIoTSuccess)
	{
		Add_Response_msg("AzureIoTHubClient_UnsubscribeCloudToDeviceMessage failed",s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
	}

	AzureIoTHubClient_Disconnect(&xAzureIoTHubClient);
	TLS_Socket_Disconnect(&xNetworkContext);
}

static void Connect_IHUB(AMessage_st* s_Message_Rx_new)
{
	uint32_t ulStatus = 255;  // give high initial value
	AzureIoTResult_t xResult;
	char Error[120] = {0};
	uint32_t pulIothubHostnameLength =0;
	uint32_t pulIothubDeviceIdLength =0;
	uint32_t IothubConnectionStrLength =0;
	memset(&xAzureIoTHubClient, 0, sizeof(AzureIoTHubClient_t));
    memset(&xNetworkContext,0,sizeof(xNetworkContext));
	memset(&xNetworkCredentials, 0, sizeof(xNetworkCredentials));
    memset(&xTransport,0,sizeof(xTransport));
    memset(&xTlsTransportParams,0,sizeof(xTlsTransportParams));
    memset(&xHubOptions,0,sizeof(xHubOptions));

    xNetworkContext.pParams = NULL;
	bool sessionPresent = false;

	prvSetupNetworkCredentials( &xNetworkCredentials );
	xNetworkContext.pParams = &xTlsTransportParams;

	  uint8_t * pucIotHubHostname = ( uint8_t * ) &iHub_Para.HostName;
	  uint8_t * pucIotHubDeviceId = ( uint8_t * ) DeviceId;
	  uint8_t * IotHubConnectionStr = ( uint8_t * ) &iHub_Para.PrimaryKey;
	  pulIothubHostnameLength = strlen((char*) &iHub_Para.HostName );
	  pulIothubDeviceIdLength = strlen((char*)DeviceId );
	  IothubConnectionStrLength = strlen((char*)&iHub_Para.PrimaryKey );


	ulStatus = prvConnectToServerWithBackoffRetries( ( const char * ) pucIotHubHostname,
													 democonfigIOTHUB_PORT,
													 &xNetworkCredentials, &xNetworkContext );

	if( ulStatus != 0 )
	{
		update_device_online_status(0);
		sprintf(IHUB_str1, "Connect To server with back off retries failed, NTP_Sync_Status = %d", sntp_get_sync_status());
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
		cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
		memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
		cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_IHUB, strlen(payLoadData_IHUB), "SAVE_WIFI_AUDIT_LOG");
		cJSON_Delete(responseObject);
		Delete_task_flag = 1;
	}

	/* Fill in Transport Interface send and receive function pointers. */
	xTransport.pxNetworkContext = &xNetworkContext;
	xTransport.xSend = TLS_Socket_Send;
	xTransport.xRecv = TLS_Socket_Recv;
	/* Init IoT Hub option */
	xResult = AzureIoTHubClient_OptionsInit( &xHubOptions );
	if( xResult != eAzureIoTSuccess)
	{
		update_device_online_status(0);
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client options Init failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");

		Delete_task_flag = 1;
		struct timeval currentTime;
		_gettimeofday_r(NULL, &currentTime, NULL);
		uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		iHub_Para.IHUB_status_time_u64  = current_epos_sec;
		cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
		cJSON_AddStringToObject(responseObject, "IHUB_CONNECTION_STATUS", "DISCONNECTED");  //(char*)pxMessage->pucCommandName
		cJSON_AddNumberToObject(responseObject, "ERROR_NUM",xResult);
		memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
		cJSON_PrintPreallocated(responseObject, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
		cJSON_Delete(responseObject);
		return;
	}
	xHubOptions.pucModuleID = ( const uint8_t * ) democonfigMODULE_ID;
	xHubOptions.ulModuleIDLength = sizeof( democonfigMODULE_ID ) - 1;
	xHubOptions.pucModelID = ( const uint8_t * ) sampleazureiotMODEL_ID;
	xHubOptions.ulModelIDLength = sizeof( sampleazureiotMODEL_ID ) - 1;

	xResult = AzureIoTHubClient_Init( &xAzureIoTHubClient,
			pucIotHubHostname, pulIothubHostnameLength,
			pucIotHubDeviceId, pulIothubDeviceIdLength,
		    &xHubOptions,
		    ucMQTTMessageBuffer, sizeof( ucMQTTMessageBuffer ),
		    ullGetUnixTime,
		    &xTransport );
	if( xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client Init failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		return;
	}
	xResult = AzureIoTHubClient_SetSymmetricKey( &xAzureIoTHubClient,
												 ( const uint8_t * ) IotHubConnectionStr,
												 IothubConnectionStrLength,
												 Crypto_HMAC );
	if( xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client set symmetric key failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		return;
	}

	xResult = AzureIoTHubClient_Connect(&xAzureIoTHubClient,
										true,  // cleanSession = false to preserve subscriptions
										&sessionPresent,   // sessionEstablishment
										sampleazureiotCONNACK_RECV_TIMEOUT_MS);


	if (xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult, Error);
		sprintf(IHUB_str1, "Failed to reconnect after SAS token refresh, Error = %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
	    return;
	}

	xResult = AzureIoTHubClient_SubscribeCommand( &xAzureIoTHubClient, prvHandleCommand,
												  &xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT );
	if( xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client subscribe command failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		return;
	}

	xResult = AzureIoTHubClient_SubscribeProperties( &xAzureIoTHubClient, prvHandleProperties,
													 &xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT );

	if( xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client subscribe properties failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		return;
	}

	xResult = AzureIoTHubClient_SubscribeCloudToDeviceMessage( &xAzureIoTHubClient, prvHandleCtoDCommand,
			&xAzureIoTHubClient, sampleazureiotSUBSCRIBE_TIMEOUT);
	if( xResult != eAzureIoTSuccess)
	{
		ErrortoStringConverter(xResult , Error);
		sprintf(IHUB_str1,"Azure IoT Hub client subscribe CloudToDevice message failed, Error is %s", Error);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
		return;
	}

	iHub_conn_status_set(IHUB_CONNECTED);
}

/*
 * Phase 1: Attempt to recover the IHUB MQTT/TLS session from within prvAzureDemoTask,
 * without tearing down WiFi/HTTP or signalling the monitor to run a full stack reset.
 *
 * Returns 1 if the session was restored (conn_status == IHUB_CONNECTED), 0 otherwise.
 *
 *  - xLoopErr is the error that triggered recovery. For eAzureIoTErrorMQTTTimeout
 *    (keepalive/0xa) and stale connections we force a fresh SAS token on reconnect.
 *  - Each attempt drops the broken session (Disconnect_IHUB) and reconnects
 *    (Connect_IHUB, which re-subscribes and sets conn_status on success).
 *  - The loop honours an external DISCONNECT/DEINIT (Delete_task_flag) and aborts
 *    cleanly so the caller can tear down.
 */
static uint8_t prvAttemptInTaskReconnect(AMessage_st *s_Message_Rx_new, AzureIoTResult_t xLoopErr, int64_t *plastPingTime)
{
	uint8_t ucRecovered = 0;
	uint32_t ulBackoffMs = sampleazureiotRETRY_BACKOFF_BASE_MS;

	/* Stay "in progress" (not DISCONNECTED) so the monitor does not reset the stack. */
	iHub_conn_status_set(IHUB_CONNECTION_IN_PROGRESS);

	for( uint32_t ulAttempt = 1U; ulAttempt <= sampleazureiotINTASK_RECONNECT_MAX_ATTEMPTS; ulAttempt++ )
	{
		/* Honour an external DISCONNECT/DEINIT requested while we were recovering. */
		if( Delete_task_flag == 1 )
		{
			break;
		}

		/* Drop the broken MQTT/TLS session (leaves WiFi/HTTP actors untouched). */
		Disconnect_IHUB(s_Message_Rx_new);

		/* Keepalive/MQTT timeout or stale link: force a fresh SAS token on reconnect. */
		if( xLoopErr == eAzureIoTErrorMQTTTimeout )
		{
			lastTokenRefreshTime = 0;
		}

		/* Back off in small slices so an external teardown stays responsive. */
		for( uint32_t ulWaited = 0U; ulWaited < ulBackoffMs; ulWaited += 100U )
		{
			if( Delete_task_flag == 1 )
			{
				break;
			}
			vTaskDelay( pdMS_TO_TICKS( 100U ) );
		}
		if( Delete_task_flag == 1 )
		{
			break;
		}

		/* No link: nothing to reconnect to. Back off and re-check next iteration. */
		if( !xAzureSample_IsConnectedToInternet() )
		{
			ulBackoffMs = ( ulBackoffMs * 2U > sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS ) ?
						  sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS : ( ulBackoffMs * 2U );
			continue;
		}

		sprintf(IHUB_str1, "IHUB in-task recovery attempt %lu/%u...",
				(unsigned long)ulAttempt, (unsigned)sampleazureiotINTASK_RECONNECT_MAX_ATTEMPTS);
		Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");

		/* Full TLS+MQTT connect and re-subscribe. Sets conn_status==IHUB_CONNECTED
		 * on success; may set Delete_task_flag internally on a hard failure. */
		Connect_IHUB(s_Message_Rx_new);

		if( iHub_Para.conn_status == IHUB_CONNECTED )
		{
			ucRecovered = 1;
			Delete_task_flag = 0;   /* clear any stray internal flag */
			deinit_Flag = 0;
			update_device_online_status(1);
			ihubTimeActive = 0;     /* restart D2C heartbeat cadence */
			if( plastPingTime != NULL )
			{
				*plastPingTime = get_current_time_ms();
			}

			struct timeval tvNow;
			_gettimeofday_r(NULL, &tvNow, NULL);
			iHub_Para.IHUB_status_time_u64 = (uint64_t)(tvNow.tv_sec * 1000L) + (uint64_t)(tvNow.tv_usec / 1000L);

			sprintf(IHUB_str1, "IHUB in-task recovery succeeded on attempt %lu.", (unsigned long)ulAttempt);
			Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
			Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
			break;
		}

		/* Failed attempt: clear the stray Delete_task_flag that Connect_IHUB may
		 * have set on an internal failure so the loop can keep retrying, then
		 * grow the back-off. */
		Delete_task_flag = 0;
		ulBackoffMs = ( ulBackoffMs * 2U > sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS ) ?
					  sampleazureiotRETRY_MAX_BACKOFF_DELAY_MS : ( ulBackoffMs * 2U );
	}

	return ucRecovered;
}


void Azure_RefreshToken_IfExpiring(AMessage_st* s_Message_Rx_new)
{
    AzureIoTResult_t xResult;
    char Error[120] = {0};
    char IHUB_str1[256] = {0};
    uint32_t ulPasswordLength = 0;
    int64_t currentTime = get_current_time_ms();
    uint64_t expiryEpoch = lastTokenRefreshTime + azureiotconfigDEFAULT_TOKEN_TIMEOUT_IN_SEC;

    if(lastTokenRefreshTime == 0)
    {
    	lastTokenRefreshTime = currentTime/1000;
    	expiryEpoch = lastTokenRefreshTime + azureiotconfigDEFAULT_TOKEN_TIMEOUT_IN_SEC;
    }
    if ((expiryEpoch - (currentTime / 1000)) < TOKEN_EXPIRY_THRESHOLD_SEC)
    {
        Add_Response_msg("Token expiring soon. Refreshing SAS token and reconnecting...", s_Message_Rx_new, payLoadData_IHUB);
        // Prepare new password buffer (SAS token)
        AzureIoTMQTTConnectInfo_t xConnectInfo = {0};
        xConnectInfo.pcUserName = xAzureIoTHubClient._internal.pucWorkingBuffer;
//        xConnectInfo.pcUserName = (char *)xAzureIoTHubClient._internal.pucWorkingBuffer;
        xConnectInfo.pcPassword = xConnectInfo.pcUserName + azureiotconfigUSERNAME_MAX;

        // Generate a new SAS token using internal function
        xResult = xAzureIoTHubClient._internal.pxTokenRefresh(
            &xAzureIoTHubClient,
            xAzureIoTHubClient._internal.xTimeFunction() + azureiotconfigDEFAULT_TOKEN_TIMEOUT_IN_SEC,
            xAzureIoTHubClient._internal.pucSymmetricKey,
            xAzureIoTHubClient._internal.ulSymmetricKeyLength,
            (uint8_t *)xConnectInfo.pcPassword,
            azureiotconfigPASSWORD_MAX,
            &ulPasswordLength);

        if (xResult != eAzureIoTSuccess)
        {
            ErrortoStringConverter(xResult, Error);
            sprintf(IHUB_str1, "Failed to generate new SAS token, Error = %s", Error);
            Add_Response_msg(IHUB_str1, s_Message_Rx_new, payLoadData_IHUB);
            Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", IHUB_str1, strlen(IHUB_str1), "SAVE_WIFI_AUDIT_LOG");
            lastTokenRefreshTime = currentTime / 1000;
            return;
        }

        // Disconnect and reconnect with new SAS token
		Disconnect_IHUB(s_Message_Rx_new);
		Connect_IHUB(s_Message_Rx_new);
        lastTokenRefreshTime = currentTime / 1000;
    }
}

static void TwinCommandProcessTask(void *pvParameters __attribute__((unused)))
{
	char str[200]={0}; // Error[120] = {0};
	AMessage_st s_Message_Tx_new;
	s_Message_Tx_new.Dest_ID_a8 = IHUB;
	s_Message_Tx_new.Src_ID_u8 =SYSTEM;
	strcpy((char*)s_Message_Tx_new.dest_Actor_a8,"IHUB");
	strcpy((char*)s_Message_Tx_new.src_Actor_a8,"SYSTEM");
	strcpy((char*)s_Message_Tx_new.cmdFun_a8,"EVENT");
	s_Message_Tx_new.payload_p8	= (uint8_t*)Twin_msg_payload;
	while(1)
	{
		if (pdTRUE == xQueueReceive(Process_Twin_Commands_Queue, (void*)payLoadData_TwinCmmand, portMAX_DELAY))
		{
			s_Message_Tx_new.payload_size = strlen((char*)payLoadData_TwinCmmand);  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
			cJSON *root = cJSON_CreateObject();
			cJSON *responseObject = cJSON_Parse((char*)payLoadData_TwinCmmand);
			if (responseObject == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,&s_Message_Tx_new, payLoadData_IHUB);
				cJSON_Delete(root);
				continue;
			}

			cJSON *reported_prop = cJSON_GetObjectItem(responseObject, "reported");
			if(reported_prop != NULL)
			{
				//printf("\n\n reported properties = %s \n\n", cJSON_Print(reported_prop));
				Reported_Properties[0] = '\0';
				cJSON_PrintPreallocated(reported_prop, Reported_Properties, sizeof(Reported_Properties), false);
			}

			if(root != NULL)
			{
				cJSON_AddItemToObject(root, "TWIN_MESSAGE", responseObject);
				memset(payLoadData_IHUB,0,sizeof(payLoadData_IHUB));
				cJSON_PrintPreallocated(root, payLoadData_IHUB, sizeof(payLoadData_IHUB), false);
				strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_IHUB);
				cJSON_Delete(root);
				console_send_responce_to_console_xface(&s_Message_Tx_new);
			}
			vTaskDelay(5000/portTICK_PERIOD_MS);
			if(uxQueueMessagesWaiting(Process_Twin_Commands_Queue) == 0)
			{
				process_Twin_data((const char*)payLoadData_TwinCmmand);
			}
		}
	}
}
