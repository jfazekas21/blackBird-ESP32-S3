/*
 * SPIFFS_Actor.c
 *
 *  Created on: 23-Sep-2022
 *      Author: ACER
 */

#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "SPIFFS_Actor.h"
#include "esp_spiffs.h"
#include <stdio.h>
#include "esp_partition.h"
#include "esp_app_desc.h"
#include "esp_ota_ops.h"
//----------------------------- Actor Tags ---------------------------------------//

static const char 	* THIS_ACTOR 	= "SPIFFS";
static const char 	THIS_ACTOR_ID 	= 	SPIFFS;  // assign src id

//------------------------- SPIFFS Actor Resources -----------------------------//
#define RX_QUE_COUNT 		5
#define MAX_CHUNK_SIZE 		1024   //2048
#define SPIFFS_STARTUP_READY_BIT BIT0
#define SPIFFS_MOUNT_RETRY_DELAY_MS 2000
#define SPIFFS_MOUNT_MAX_RETRIES 10
#define	BOOTLOADER_VERSION	"0.22"
#define	MANUFACTURER_NAME	"HAVEN"
#define	MANUFACTURER_DATE	"1st February 2025"

#if defined(B480)
#define	MODEL_NAME			"X-SERIES"
#define	HARDWARE_VERSION	"B480 Rev Hx"
#elif defined(B543)
#define	MODEL_NAME			"X-POE"
#define	HARDWARE_VERSION	"B543 Rev C"
#elif defined(B542)
#define	MODEL_NAME			"Q-POE"
#define	HARDWARE_VERSION	"B542 Rev C"
#elif defined(B394)
#define	MODEL_NAME			"A-LINK"	//"AUT-LINK"
#define	HARDWARE_VERSION	"B394 Rev D"
#elif defined(B527)
#define	MODEL_NAME			"SMART-OUTLET"
#define	HARDWARE_VERSION	"B527 Rev B"
#else
#define	MODEL_NAME			"X-MINI"
#define	HARDWARE_VERSION	"B553 Rev C"
#endif

static const char Spiffs_base_path[] = "/spiffs1/";
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/4];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/4];
PSRAM_ATTR_BSS static char DataPtr_Write[MAX_CHUNK_SIZE];
PSRAM_ATTR_BSS static char DataPtr_Read[MAX_CHUNK_SIZE];
PSRAM_ATTR_BSS static char DataPtr_Read_Chunk[MAX_CHUNK_SIZE];
PSRAM_ATTR_BSS static char DataPtr_Default[MAX_CHUNK_SIZE];
static QueueHandle_t msg_Rx_Queue = NULL;
static TaskHandle_t SPIFFS_Handle = NULL;
static EventGroupHandle_t SPIFFS_Startup_EventGroup = NULL;
static bool   FirstEntry_bool = false;
static bool   SPIFFS_Mount_Ready = false;
static BaseType_t 		Task_Monitor;

static int SPIFFS_file_write_variable(AMessage_st* s_Message_Rx);
static void SPIFFS_Read_Variable(AMessage_st* s_Message_Rx);
static void SPIFFS_file_read_Chunk(AMessage_st* s_Message_Rx);
static void SPIFFS_file_write_Parse(AMessage_st* s_Message_Rx);
static void SPIFFS_file_delete(AMessage_st* s_Message_Rx);
static esp_err_t SPIFFS_file_mnt(void);
static void SPIFFS_file_unmnt(void);
static void Write_data_First_Time(AMessage_st* s_Message_Rx);
static int check_default_values(void);
static void SPIFFS_EnsureStartupEventGroup(void);
//-------------------------- Common Actor Resources ------------------------------//

static AMessage_st 		  s_Message_Tx;
static uint8_t Device_Fw_Ver[30];
static void init(void *a, void *b);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void monitor(void *pvParameters __attribute__((unused)));
static void get(char *prop, char *val);							//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);

PSRAM_ATTR_BSS static struct SPIFFS_PARAMETER{
	char file_name[50];
	char SPIFFS_Status_u8;
	uint64_t SPIFFS_status_time_u64;
}SPIFFS_para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &SPIFFS_para.file_name,   	   "FILE_NAME",	    STRING, 	"RW", 	"Name of the file" },
    { &SPIFFS_para.SPIFFS_Status_u8,   "SPIFFS_STATUS", U_INT8, 	"R", 	"Status of SPIFFS" },
	{ &SPIFFS_para.SPIFFS_status_time_u64   , "SPIFFS_UPDATE_TIME"     , U_INT64, "R", "SPIFFS status last update time" }
};

static void SPIFFS_EnsureStartupEventGroup(void)
{
	if (SPIFFS_Startup_EventGroup == NULL)
	{
		SPIFFS_Startup_EventGroup = xEventGroupCreate();
	}
}

void SPIFFS_StartupGateReset(void)
{
	SPIFFS_EnsureStartupEventGroup();
	if (SPIFFS_Startup_EventGroup != NULL)
	{
		xEventGroupClearBits(SPIFFS_Startup_EventGroup, SPIFFS_STARTUP_READY_BIT);
	}
}

BaseType_t SPIFFS_WaitForStartupReady(TickType_t timeout_ticks)
{
	EventBits_t bits;

	SPIFFS_EnsureStartupEventGroup();
	if (SPIFFS_Startup_EventGroup == NULL)
	{
		return pdFALSE;
	}

	bits = xEventGroupWaitBits(SPIFFS_Startup_EventGroup, SPIFFS_STARTUP_READY_BIT, pdFALSE, pdTRUE, timeout_ticks);
	return ((bits & SPIFFS_STARTUP_READY_BIT) != 0U) ? pdTRUE : pdFALSE;
}

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
			if(!(strcmp(prop[i].str_name, "SPIFFS_STATUS")))
			{
				//printf("\n prop[i].str_name = %s, prop[i].name= %d",prop[i].str_name,*(char*)prop[i].name);
				switch (*(char*)prop[i].name)
				{
				case SPIFFS_INITIALIZED:
					strcpy(val_a8, "SPIFFS_INITIALIZED");
					break;

				case SPIFFS_FILE_EMPTY:
					strcpy(val_a8, "SPIFFS_FILE_EMPTY");
					break;

				case SPIFFS_DEFAULT_VALUES:
					strcpy(val_a8, "SPIFFS_DEFAULT_VALUES");
					break;

				case SPIFFS_CORRUPTED:
					strcpy(val_a8, "SPIFFS_CORRUPTED");
					break;

				case SPIFFS_POPULATED:
					strcpy(val_a8, "SPIFFS_POPULATED");
					break;

				case SPIFFS_PARTIAL:
					strcpy(val_a8, "SPIFFS_PARTIAL");
					break;

				case SPIFFS_PARTITION_NOT_FOUND:
					strcpy(val_a8, "SPIFFS_PARTITION_NOT_FOUND");
					break;

				default:
					break;
				}
			}
		}
	}
}//	get

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[50] = {0};
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
			if(!(strcmp(prop[i].str_name, "SPIFFS_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case SPIFFS_INITIALIZED:
					strcpy(val_a8, "SPIFFS_INITIALIZED");
					break;

				case SPIFFS_FILE_EMPTY:
					strcpy(val_a8, "SPIFFS_FILE_EMPTY");
					break;

				case SPIFFS_DEFAULT_VALUES:
					strcpy(val_a8, "SPIFFS_DEFAULT_VALUES");
					break;

				case SPIFFS_CORRUPTED:
					strcpy(val_a8, "SPIFFS_CORRUPTED");
					break;

				case SPIFFS_POPULATED:
					strcpy(val_a8, "SPIFFS_POLULATED");
					break;

				case SPIFFS_PARTIAL:
					strcpy(val_a8, "SPIFFS_PARTIAL");
					break;

				case SPIFFS_PARTITION_NOT_FOUND:
					strcpy(val_a8, "SPIFFS_PARTITION_NOT_FOUND");
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
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		

		// Cleanup
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);

}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the HTTP actor.");
	cJSON_AddStringToObject(responseObject, "SET(string FILE_NAME)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "WR_PARA(string FILE_NAME, string VARIABLE_NAME)", "Store variable/variables in the reserved partition.");
	cJSON_AddStringToObject(responseObject, "READ_PARA(string FILE_NAME,string VARIABLE_NAME)", "Read variable/variables from the reserved partition");
	cJSON_AddStringToObject(responseObject, "READ(string FILE_NAME)", "Read file from reserved partition.");
	cJSON_AddStringToObject(responseObject, "WRITE(string FILE_NAME, string FILE_DATA)", "Save file to reserved partition.");
	cJSON_AddStringToObject(responseObject, "DELETE(string FILE_NAME)", "Delete the file.");
	cJSON_AddStringToObject(responseObject, "FORMAT()", "Format the SPIFFS");
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void init(void *a, void *b) {
	if (FirstEntry_bool) {
		return;
	}
	msg_Rx_Queue = xQueueCreate(RX_QUE_COUNT, sizeof(AMessage_st));
	if (msg_Rx_Queue == NULL) {
#ifdef ENABLE_PRINT_MSG
		printf("SPIFFS RX Queue is not created. \n");
#endif
		return;
	}

	Task_Monitor = xTaskCreate(monitor, THIS_ACTOR ,SPIFFS_MONITOR_TASK_STACK_DEPTH, NULL, 2, &SPIFFS_Handle);
	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	memset(&s_Message_Tx,0,sizeof(s_Message_Tx));

	//Get firmware version
	const esp_app_desc_t *new_app_info = esp_app_get_description();
	for(int i= 0; i<strlen(new_app_info->version); i++)
	{
	   if(new_app_info->version[i]=='_')
		   Device_Fw_Ver[i] = '.';
	   else
		   Device_Fw_Ver[i] = new_app_info->version[i];
	}
	FirstEntry_bool = true;
}// init

static void monitor(void *pvParameters __attribute__((unused))) {
	char str[100] = {0};
	int ret = SPIFFS_file_mnt();    // mount spiffs file system

	uint8_t err_cnt = 0;
	SPIFFS_Mount_Ready = (ret == ESP_OK);

	if(ret != ESP_OK)
	{
		printf(">%s.INIT(Error in mounting SPIFFS: %s.) \n",THIS_ACTOR, esp_err_to_name(ret));

		do
		{
			vTaskDelay(pdMS_TO_TICKS(SPIFFS_MOUNT_RETRY_DELAY_MS));
			ret = SPIFFS_file_mnt();    // mount spiffs file system
			if(ret != ESP_OK)
			{
				printf(">%s.INIT(Retry %u failed to mount SPIFFS: %s.) \n",THIS_ACTOR, (unsigned)(err_cnt + 1), esp_err_to_name(ret));
			}
			else
			{
				SPIFFS_Mount_Ready = true;
				break;
			}
			err_cnt++;
		}while(err_cnt < SPIFFS_MOUNT_MAX_RETRIES);
	}

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		  if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))  //5
		  {
//			printf("SPIFFS msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("SPIFFS DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/4)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
		    // Write code as per received cmds
			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				init(0, 0);
				if (SPIFFS_Mount_Ready == true)
				{
					Write_data_First_Time(s_Message_Rx);
				}
				else
				{
					Add_Response_msg("SPIFFS mount failed after retrying. Continuing startup without SPIFFS.", s_Message_Rx);
				}
				Add_Response_msg("SPIFFS is initialized.", s_Message_Rx);
				SPIFFS_EnsureStartupEventGroup();
				if (SPIFFS_Startup_EventGroup != NULL)
				{
					xEventGroupSetBits(SPIFFS_Startup_EventGroup, SPIFFS_STARTUP_READY_BIT);
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) { // Get Actor Properties

				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			{
				get_actor_properties(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "WR_PARA"))
			{
				int ret = SPIFFS_file_write_variable(s_Message_Rx);
				if(ret == ESP_OK)
				{
					SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_POPULATED;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_PARA"))
			{
				SPIFFS_Read_Variable(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ"))
			{
				SPIFFS_file_read_Chunk(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "WRITE"))
			{
				SPIFFS_file_write_Parse(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "DELETE"))
			{
				SPIFFS_file_delete(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "FORMAT"))
			{
				SPIFFS_file_unmnt();  // unmount the file system at first

			    ret = esp_spiffs_format("storage1");
				if (ret != ESP_OK)
				{
					SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
					sprintf(str, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
					Add_Response_msg(str, s_Message_Rx);
				}
				else
				{
					SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_FILE_EMPTY;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
					Add_Response_msg("SPIFFS is formatted successfully. Restarting the system...", s_Message_Rx);
					Restart_ESP_Xface(1);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UNMOUNT"))
			{
				SPIFFS_file_unmnt();
			}
			else
			{
				//HTTP error message: invalid method
				Add_Response_msg("invalid method", s_Message_Rx);
			}
		}
	}
}// monitor

void SPIFFS_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstEntry_bool == false)
	{
		init(0,0);
	}
	uint8_t state = xQueueSend(msg_Rx_Queue, s_Message, QUE_DELAY); //100
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<SPIFFS.ERROR(SPIFFS RX Queue is full)\n");
		}
		else
		{
			printf("<SPIFFS.ERROR(SPIFFS RX Queue send unsuccessful)\n");
		}
	}
}//	SPIFFS_ConsolWriteToActor

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n SPIFFS s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}
	else
	 {
			// Obtain the COMMAND and RESPONSE keys
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (commandKey != NULL && responseKey != NULL)
			{				
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
		}
	return;
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
		Add_Response_msg(str,s_Message_Rx);
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
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static esp_err_t SPIFFS_file_mnt(void)
{
	size_t total = 0, used = 0;
		esp_vfs_spiffs_conf_t conf =
			{
		      .base_path = "/spiffs1",
		      .partition_label = "storage1",
		      .max_files = 5,
		      .format_if_mount_failed = true
		    };

	    // Use settings defined above to initialize and mount SPIFFS filesystem.
	    // Note: esp_vfs_spiffs_register is an all-in-one convenience function.
	    esp_err_t ret = esp_vfs_spiffs_register(&conf);

	    if (ret != ESP_OK)
	    	{
	        	if (ret == ESP_FAIL)
	        		{
	        			SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;

	        			printf("\n Failed to mount or format filesystem");
	        		}
	        	else if (ret == ESP_ERR_NOT_FOUND)
	        		{
	        			SPIFFS_para.SPIFFS_Status_u8 =SPIFFS_PARTITION_NOT_FOUND;
						struct timeval currentTime;
						_gettimeofday_r(NULL, &currentTime, NULL);
						uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
						SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
	        			printf ("\n Failed to find SPIFFS partition");
	        		}
	        return ret;
	    	}
	    ret = esp_spiffs_info(conf.partition_label, &total, &used);
	    if (ret != ESP_OK)
			{
	    		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
				printf( "\n Failed to get SPIFFS partition information (%s)", esp_err_to_name(ret));
			}
	    if (used > total)
	    {
	            printf("\n Number of used bytes cannot be larger than total. Performing SPIFFS_check().");
	            ret = esp_spiffs_check(conf.partition_label);
	            // Could be also used to mend broken files, to clean unreferenced pages, etc.
	            // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
	            if (ret != ESP_OK) {
	            	SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
	            	printf("SPIFFS_check() failed (%s)", esp_err_to_name(ret));
	                return ret;
	            } else {
	            	printf("SPIFFS_check() successful");
	            }
	      }
	    return ret;
}

static void SPIFFS_file_unmnt(void)
{
		esp_vfs_spiffs_conf_t conf = {
		      .base_path = Spiffs_base_path,
		      .partition_label = "storage1",
		      .max_files = 5,
		      .format_if_mount_failed = true
		    };
		esp_vfs_spiffs_unregister(conf.partition_label);
}

static int SPIFFS_file_write(char *file_name, char* data)
{
	char FileName[50] = {0};
	FILE* f;
	strcpy(FileName,Spiffs_base_path);
	strcat(FileName,file_name);

	f = fopen(FileName, "w");
	if (f == NULL)
	{
	   printf( "\n Failed to open file for writing");
	   return -1;
	}
	fprintf(f, data);
	fclose(f);
	return 0;
}

static int SPIFFS_file_read(char *file_name, char *data)
{
	char FileName[50] = {0};
	strcpy(FileName,Spiffs_base_path);
	strcat(FileName, file_name);
	FILE* f;
	f = fopen(FileName, "r");
	if (f == NULL)
	{
		return -1;
	}
	while(fgets(data, MAX_CHUNK_SIZE, f) != NULL)
	{
		char* pos = strchr(data, '\n');
		if (pos)
		{
			*pos = '\0';
		}
	}
	fclose(f);
	return(strlen(data));
}

// Function to update original JSON object based on new input JSON object
static int update_json_object(cJSON **original, cJSON *new_input) {
	int Diff_Data_flag = 0;
    if (new_input == NULL) {
        return -1;
    }
    if (*original == NULL) {
        *original = cJSON_CreateObject();
    }
    cJSON *current_element = NULL;
    cJSON_ArrayForEach(current_element, new_input) {
        // Check if the key exists in the original object
        cJSON *original_item = cJSON_GetObjectItem(*original, current_element->string);
        if (original_item) {
        	// Replace the value in the original object with the new value
            switch (current_element->type) {
                case cJSON_String:
                	if(strcmp(original_item->valuestring, current_element->valuestring) != 0)
                	{
                		Diff_Data_flag++;
               			cJSON_SetValuestring(original_item, current_element->valuestring);
                	}
                    break;
                case cJSON_Number:
                	if(original_item->valuedouble != current_element->valuedouble)
                	{
                		Diff_Data_flag++;
                		original_item->valuedouble = current_element->valuedouble;
                	}
                	if(original_item->valueint != current_element->valueint)
                	{
                		Diff_Data_flag++;
                		original_item->valueint = current_element->valueint;
                	}
                    break;
                case cJSON_True:
                case cJSON_False:
                	if(original_item->type != current_element->type)
                	{
                		Diff_Data_flag++;
                		original_item->type = current_element->type;
                	}
//                    original_item->type = current_element->type;
                    break;
                case cJSON_NULL:
                	Diff_Data_flag++;
                    cJSON_ReplaceItemInObject(*original, current_element->string, cJSON_CreateNull());
                    break;
                case cJSON_Array:
                case cJSON_Object:
                	Diff_Data_flag++;
                    cJSON_ReplaceItemInObject(*original, current_element->string, cJSON_Duplicate(current_element, 1));
                    break;
                default:
                    break;
            }
        } else {
            // If the key does not exist in the original object, add it
        	Diff_Data_flag++;
            cJSON_AddItemToObject(*original, current_element->string, cJSON_Duplicate(current_element, 1));
        }
    }
    return Diff_Data_flag;
}

// Write_Variable function  to write variables to the SPIFFS JSON document
static int SPIFFS_file_write_variable(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	cJSON *Original_JSON = NULL;
	char filename[100] = {0}, retry = 0;
	char str[200] = {0};
	int err = -1, ret = -1;
	uint16_t Read_count 	=	0;
	in_JSON = cJSON_Parse((char*) (char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"1 Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return -1;
		}

	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcat(filename, name_JSON->valuestring);
		retry = 0;
		do
		{
			Read_count	= SPIFFS_file_read((char*)filename, DataPtr_Write);
			if(Read_count > 0)
				break;
			else
			{
				retry++;
				vTaskDelay(50);
			}
		}while((Read_count <= 0) && (retry <3));
		{
			if(Read_count > 0)
			{
				Original_JSON 	= cJSON_Parse(DataPtr_Write);
			}
			ret = update_json_object(&Original_JSON, in_JSON);
		}
		cJSON_Delete(in_JSON);
		if(Original_JSON != NULL)
		{
			if(ret > 0)  // New data is present. Write this new data to .json file
			{
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(Original_JSON, payLoadData, sizeof(payLoadData), false);
				err = SPIFFS_file_write(filename, payLoadData);
			}
			cJSON_Delete(Original_JSON);
		}
		if(err == 0)
		{
			Add_Response_msg((char*)s_Message_Rx->payload_p8,s_Message_Rx);
			Add_Response_msg("variable(s) stored successfully",s_Message_Rx);
		}
		else
		{
			if(ret == 0)
			{
				sprintf(str, "Received data of %s file is same.", filename);
				Add_Response_msg(str,s_Message_Rx);
			}
			else if(ret == -1)
			{
				sprintf(str, "Error! Received data of %s file is NULL.", filename);
				Add_Response_msg(str,s_Message_Rx);
			}
			else
			{
				Add_Response_msg("Error in storing Variable(s).",s_Message_Rx);
			}
		}
	return err;
}

// Read variable from SPIFFS file
static void SPIFFS_Read_Variable(AMessage_st* s_Message_Rx)
{
	    cJSON *in_JSON 		= 	NULL;
		cJSON *in_JSON1 	= 	NULL;
		cJSON *name_JSON 	= 	NULL;
		cJSON *head_JSON 	= 	NULL;
		cJSON *head_JSON1 	= 	NULL;
		cJSON *out_JSON1 	= 	NULL;
		cJSON *root 	= 	NULL;
		char filename[30] = {0};
		uint8_t Match_found_u8 = 0;
		uint16_t Read_count 	=	0;
		char str[200] = {0};
		int retry_count = 0;

		in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
			return;
			}

		name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
		if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
			strcpy(filename, name_JSON->valuestring);
		else
		{
			Add_Response_msg("Error! FILE_NAME is not given.",s_Message_Rx);
			return;
		}

		root 	= cJSON_CreateObject();
		while (retry_count < 3)
		{
		Read_count	= SPIFFS_file_read((char*)filename, DataPtr_Read);
		{
				head_JSON 	= in_JSON->child;    //take variable from payload
				head_JSON = head_JSON->next;  //point to 1st variable (format: Filename, var1, var2, ...)
				do {
						Match_found_u8 = 0;
						while(1)
						{
							if(Read_count == 0)
							{
								Add_Response_msg("File is empty",s_Message_Rx);
								cJSON_Delete(in_JSON);
								return;
							}
							if(Read_count !=0)
							{
								in_JSON1 	= cJSON_Parse(DataPtr_Read);
								if (in_JSON1 == NULL)
								{
									printf("\n Read_count=%d \n\n", Read_count);
									sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
									Add_Response_msg(str, s_Message_Rx);
									goto exit;
								}
								out_JSON1 	= cJSON_CreateObject();
								head_JSON1 	= in_JSON1->child;

								do {
										switch (head_JSON1->type)
										{
											case cJSON_Number: 	cJSON_AddNumberToObject(out_JSON1, head_JSON1->string, head_JSON1->valuedouble);
																break;

											case cJSON_String: 	cJSON_AddStringToObject(out_JSON1, head_JSON1->string, head_JSON1->valuestring);
																break;

											case cJSON_True: 	cJSON_AddBoolToObject(out_JSON1, head_JSON1->string,true);
																break;

											case cJSON_False: 	cJSON_AddBoolToObject(out_JSON1, head_JSON1->string,false);
																break;

											case cJSON_NULL: 	cJSON_AddNullToObject(out_JSON1, head_JSON1->string);
																break;

											case cJSON_Array: 	name_JSON = cJSON_Duplicate(head_JSON1, 1);
							 	 	 	 						cJSON_AddItemToObject(out_JSON1, head_JSON1->string, name_JSON);
																break;

											case cJSON_Object: 	name_JSON = cJSON_Duplicate(head_JSON1, 1);
		 	 	 	 											cJSON_AddItemToObject(out_JSON1, head_JSON1->string, name_JSON);//cJSON_AddObjectToObject(out_JSON1, head_JSON1->string);
																break;

											case cJSON_Raw: 	cJSON_Duplicate(head_JSON1, 1);
		 	 	 	 											cJSON_AddItemToObject(out_JSON1, head_JSON1->string, name_JSON); //cJSON_AddRawToObject(out_JSON1, head_JSON1->string,head_JSON1->string);
																break;

											default:            break;
										}
										memset(payLoadData,0,sizeof(payLoadData));//\0';
										cJSON_PrintPreallocated(out_JSON1, payLoadData, sizeof(payLoadData), false);
										if(strcmp(head_JSON1->string,head_JSON->string)==0)
										{
											name_JSON = cJSON_Duplicate(head_JSON1, 1);
											cJSON_AddItemToObject(root, head_JSON1->string, name_JSON);
											Match_found_u8 = 1;											
											break;  // while (head_JSON1 != NULL);
										}
										head_JSON1 = head_JSON1->next;
									}while (head_JSON1 != NULL);

								cJSON_Delete(in_JSON1);
								cJSON_Delete(out_JSON1);

								if(Match_found_u8 == 1)
									break;  //while(1)

								if(Match_found_u8 == 0)
								{
									sprintf(str, "Error! %s is not found.", head_JSON->string);
									Add_Response_msg(str, s_Message_Rx);
									break;
								}
							}
						}
					head_JSON = head_JSON->next;
				}while (head_JSON != NULL);
		}
		if (Match_found_u8 == 1) {
		            break;
		        } else {
		            retry_count++;
		            sprintf(str,"Retrying (%d/%d)...\n", retry_count, 3);
		            Add_Response_msg(str, s_Message_Rx);
		        }
		    }
		if((Match_found_u8 == 1))  //|| (root!=NULL)
		{			
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			console_send_responce_to_console_xface(s_Message_Rx);	
		}
exit:	cJSON_Delete(in_JSON);
		cJSON_Delete(root);
}

static void SPIFFS_file_read_Chunk(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	uint8_t filename[50] = {0};
	char json_file = 0;
	char str[200] = {0};
	FILE* f;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		  return;
		}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
	{
		strcpy((char*)filename,Spiffs_base_path);
		strcat((char*)filename, name_JSON->valuestring);
	}
	else
	{
		Add_Response_msg("Error! FILE_NAME is null.",s_Message_Rx);
		cJSON_Delete(in_JSON);
		return;
	}
	const char *dot = strrchr((char*)filename, '.');
	// Check if the dot is present and if it is followed by "bin"
	if (dot != NULL && (strcmp(dot + 1, "json") == 0))
	{
		json_file = 1; // The filename has a ".json" extension
	}
	f = fopen((char*)filename, "r");
	if (f == NULL)
	{
		sprintf(str, "'%s' file is not present in SPIFFS.", name_JSON->valuestring);
		Add_Response_msg(str,s_Message_Rx);
		cJSON_Delete(in_JSON);
		return;
	}
	while(fgets(DataPtr_Read_Chunk, MAX_CHUNK_SIZE, f) != NULL)
	{
		char* pos = strchr(DataPtr_Read_Chunk, '\n');
		if (pos)
		{
			*pos = '\0';
		}
		if(json_file == 1)   // file data is json
		{
			cJSON *responseObject =   cJSON_Parse((char*) DataPtr_Read_Chunk);  // check whether file data is json or not
			if (responseObject != NULL)
			{				
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				console_send_responce_to_console_xface(s_Message_Rx);	
			}
			cJSON_Delete(responseObject);
		}
		else // file data is string
		{
			cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
			cJSON_AddStringToObject(responseObject, "File Data", DataPtr_Read_Chunk);
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
	}
	fclose(f);
	cJSON_Delete(in_JSON);
}

static void SPIFFS_file_write_Parse(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	int err = -1;
	char filename [50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char str[200] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
		}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON!= NULL) && (name_JSON->valuestring != NULL))
		strcpy(filename,name_JSON->valuestring);
	else
	{
		Add_Response_msg("Error! FILE_NAME is null.",s_Message_Rx);
		return;
	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_DATA");
	if(name_JSON!= NULL)
	{
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(name_JSON, payLoadData, sizeof(payLoadData), false);
		err = SPIFFS_file_write(filename, payLoadData);
	}
	cJSON_Delete(in_JSON);
	if(err != ESP_OK)
		Add_Response_msg("Failed to open SPIFFS file for writing",s_Message_Rx);
	else
		Add_Response_msg("File is stored in SPIFFS successfully",s_Message_Rx);
}

static void SPIFFS_file_delete(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	int ret = -1;
	char filename [50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char str[200] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
		}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON!= NULL) && (name_JSON->valuestring != NULL))
	{
		strcpy(filename, Spiffs_base_path);
		strcat(filename,name_JSON->valuestring);
	}
	else
	{
		Add_Response_msg("Error! FILE_NAME is null.",s_Message_Rx);
		return;
	}
	ret = unlink(filename);
	if ( ret == 0)  // file to be deleted is present
	{
		sprintf(str, "%s file is deleted.", name_JSON->valuestring);
		Add_Response_msg(str,s_Message_Rx);
		if(strcasecmp(filename,"/spiffs1/Device_Information.json") == 0)
		{
			SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_FILE_EMPTY;
			struct timeval currentTime;
			_gettimeofday_r(NULL, &currentTime, NULL);
			uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
			SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
		}
	}
	else
	{
		sprintf(str, "Error! %s file is not deleted (%s).",  name_JSON->valuestring , esp_err_to_name(ret));
		Add_Response_msg(str,s_Message_Rx);
	}
	cJSON_Delete(in_JSON);
}

static void Write_data_First_Time(AMessage_st* s_Message_Rx)
{
	FILE* pFile 	= 	NULL;
	char filename[100] = {0},  Write_variable_FirstTime =1, DeviceID[50] = {0};
	uint8_t mac[6] = {0};
	int retry = 3, i =0;
	char str[100] = {0};

	for(i = 0; i < retry; i++)
	{
		strcpy(filename, Spiffs_base_path);
		strcat(filename, (char*)Device_File);

		pFile = fopen(filename, "r");
		if (pFile == NULL)
		{// check whether file exists or not
			Write_variable_FirstTime = 0;  // file does not exist
			SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_FILE_EMPTY;
		}
		else
		{
			Write_variable_FirstTime = 1;   // File already exist
			fclose(pFile);
			break;
		}
		fclose(pFile);
		vTaskDelay(500 / portTICK_PERIOD_MS);
	}

	int ret = check_default_values();

	if(ret == ESP_OK)
		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_DEFAULT_VALUES;
	else if(ret == 2)
	{
		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_FILE_EMPTY;
		Write_variable_FirstTime = 0;
	}
	else if(ret == 3)
	{
		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
		ret = unlink("/spiffs1/Device_Information.json");
		Write_variable_FirstTime = 0;
	}
	else if(ret == ESP_FAIL)
	{
		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_PARTIAL;
		Write_variable_FirstTime = 0;
	}
	else
		SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_POPULATED;

	switch (SPIFFS_para.SPIFFS_Status_u8)
	{
		case SPIFFS_INITIALIZED:
			strcpy(str, "SPIFFS_INITIALIZED");
			break;

		case SPIFFS_FILE_EMPTY:
			strcpy(str, "SPIFFS_FILE_EMPTY");
			break;

		case SPIFFS_DEFAULT_VALUES:
			strcpy(str, "SPIFFS_DEFAULT_VALUES");
			break;

		case SPIFFS_CORRUPTED:
			strcpy(str, "SPIFFS_CORRUPTED");
			break;

		case SPIFFS_POPULATED:
			strcpy(str, "SPIFFS_POPULATED");
			break;

		case SPIFFS_PARTIAL:
			strcpy(str, "SPIFFS_PARTIAL");
			break;

		case SPIFFS_PARTITION_NOT_FOUND:
			strcpy(str, "SPIFFS_PARTITION_NOT_FOUND");
			break;

		default:
			break;
	}
	Add_Response_msg(str, s_Message_Rx);

	if(Write_variable_FirstTime == 0)
	{
		cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
		cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
		cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", (char*) Device_Fw_Ver);
	    const esp_partition_t *test_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, "test");

	    if (test_partition == NULL)
	    {
	        Add_Response_msg("Test app partition not found", s_Message_Rx);
	        cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", (char*) BOOTLOADER_VERSION);
	    }
	    else
	    {
			esp_app_desc_t app_info;
			esp_err_t err = esp_ota_get_partition_description(test_partition, &app_info);
			if (err != ESP_OK)
			{
				sprintf(str, "Failed to get app description: %s \n\n", esp_err_to_name(err));
				Add_Response_msg(str, s_Message_Rx);
				cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", (char*) BOOTLOADER_VERSION);
			}
			else
			{
				char Test_Fw_Ver[20] = {0};
				for(int i= 0; i<strlen(app_info.version); i++)
				{
				   if(app_info.version[i]=='_')
					   Test_Fw_Ver[i] = '.';
				   else
					   Test_Fw_Ver[i] = app_info.version[i];
				}
				 cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", Test_Fw_Ver);
			}
	    }

#if defined(B480)
		if(Hardware_RevH_flag == true)
		{
			cJSON_AddStringToObject(object_JSON, "HARDWARE_VERSION", "B480 Rev Hx");
		}
		else
		{
			cJSON_AddStringToObject(object_JSON, "HARDWARE_VERSION", "B480 Rev E");
		}
#else
		cJSON_AddStringToObject(object_JSON, "HARDWARE_VERSION", (char*)HARDWARE_VERSION);
#endif

		cJSON_AddStringToObject(object_JSON, "MANUFACTURER_NAME", (char*) MANUFACTURER_NAME);
		cJSON_AddStringToObject(object_JSON, "MODEL_NAME", (char*)MODEL_NAME);
		cJSON_AddStringToObject(object_JSON, "MANUFACTURER_DATE", (char*)MANUFACTURER_DATE);
		esp_read_mac(mac, ESP_MAC_WIFI_STA);
	    sprintf(DeviceID, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	    cJSON_AddStringToObject(object_JSON, "DEVICE_ID", DeviceID);

	    for(i = 0; i < retry; i++)
	    {			
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(object_JSON, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			ret = SPIFFS_file_write_variable(s_Message_Rx);

			if(ret == ESP_OK)
			{
				SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_DEFAULT_VALUES;
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
				break;
			}
			else   //error in storing the variables
			{
				ret = esp_spiffs_format("storage1");
				if (ret != ESP_OK)
				{
					SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_CORRUPTED;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
					sprintf(str, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
					Add_Response_msg(str, s_Message_Rx);
				}
				else
				{
					SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_FILE_EMPTY;
					struct timeval currentTime;
					_gettimeofday_r(NULL, &currentTime, NULL);
					uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
					SPIFFS_para.SPIFFS_status_time_u64  = current_epos_sec;
					Add_Response_msg("SPIFFS is formatted successfully....", s_Message_Rx);
				}
			}
	    }
	    cJSON_Delete(object_JSON);
	    object_JSON = NULL;
	}
	if(i == retry)
	{
		Add_Response_msg("Error! Failed to save default values in SPIFFS.", s_Message_Rx);
	}
	else
	{
		cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
		cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
		cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", (char*) Device_Fw_Ver);
		// Add bootloader version
	    const esp_partition_t *test_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_TEST, "test");

	    if (test_partition == NULL)
	    {
	        Add_Response_msg("Test app partition not found", s_Message_Rx);
	        cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", (char*) BOOTLOADER_VERSION);
	    }
	    else
	    {
			esp_app_desc_t app_info;
			esp_err_t err = esp_ota_get_partition_description(test_partition, &app_info);
			if (err != ESP_OK)
			{
				sprintf(str, "Failed to get app description: %s \n\n", esp_err_to_name(err));
				Add_Response_msg(str, s_Message_Rx);
				cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", (char*) BOOTLOADER_VERSION);
			}
			else
			{
				char Test_Fw_Ver[20] = {0};
				for(int i= 0; i<strlen(app_info.version); i++)
				{
				   if(app_info.version[i]=='_')
					   Test_Fw_Ver[i] = '.';
				   else
					   Test_Fw_Ver[i] = app_info.version[i];
				}
				 cJSON_AddStringToObject(object_JSON, "BOOTLOADER_VERSION", Test_Fw_Ver);
			}
	    }
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(object_JSON, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		SPIFFS_file_write_variable(s_Message_Rx);
		cJSON_Delete(object_JSON);
	}
}

static int check_default_values(void)
{
	int Read_count 	=	0;
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	int default_count = 0, populated_count = 0;

	Read_count	= SPIFFS_file_read((char*)Device_File, DataPtr_Default);
	if(Read_count <= 0 )
		return 2;
	in_JSON = cJSON_Parse(DataPtr_Default);
	if (in_JSON == NULL)
		return 3;

	name_JSON = cJSON_GetObjectItem(in_JSON, "FIRMWARE_VERSION");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) Device_Fw_Ver) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;

	name_JSON = cJSON_GetObjectItem(in_JSON, "HARDWARE_VERSION");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) HARDWARE_VERSION) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;

	name_JSON = cJSON_GetObjectItem(in_JSON, "MANUFACTURER_NAME");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) MANUFACTURER_NAME) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;

	name_JSON = cJSON_GetObjectItem(in_JSON, "MODEL_NAME");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) MODEL_NAME) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;

	name_JSON = cJSON_GetObjectItem(in_JSON, "MANUFACTURER_DATE");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) MANUFACTURER_DATE) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;

	name_JSON = cJSON_GetObjectItem(in_JSON, "BOOTLOADER_VERSION");
	if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
	{
		if(strcmp(name_JSON->valuestring, (char*) BOOTLOADER_VERSION) != 0)
			populated_count++;
		else
			default_count++;
	}
	if(name_JSON == NULL)
		return ESP_FAIL;
	if((populated_count + default_count)< 6)  // count of default parameters are less than 6
		return ESP_FAIL;
	else if(default_count >= 6)   //default values are present
		return ESP_OK;
	else
		return 4;   //Parameters are populated
}
//------------------------ SPIFFS Actor Methods End  ------------------------------------------------------------------------//
