/*
 * Console_Actor.c
 *
 *  Created on: 20-Jul-2022
 *      Author: Ashwini and Amruta
 */

//-----------Includes Actors------//
#include "Web_server_Actor.h"
#include "actor.h"
#include "console_Actor.h"
#include "LED_Actor.h"
#include "NTP_Actor.h"
#include "wifi_actor.h"
#include "HTTP_Actor.h"
#include "UART_Actor.h"
#include "pushbutton_Actor.h"
#include "SPIFFS_Actor.h"
#include "device_config.h"
#include "ETH_Actor.h"
#include "JFS_FLASH_Actor.h"
#include "SMTP_Actor.h"
#include "SD_Card_Actor.h"
#include "iHub_Actor.h"
//#include "IDLE_Actor.h"
#include "File_Manager_Actor.h"
#include "BLE_Actor.h"
#include "Tcp_Server_Actor.h"
#include "UDP_Actor.h"
#include "SQL_Actor.h"
#include "esp_flash.h"
#include "SYSTEM_Actor.h"
#include "EVENT_Actor.h"
#include "System_Files_Actor.h"
#include "195_Actor.h"
#include "Config.h"
#include "pcf8563.h"
#include "esp_ota_ops.h"
#include "soc/rtc_cntl_reg.h"
//------------------------ Console Actor Resources -------------------------------------------------//
#define RX_QUE_COUNT		 1000

char Hardware_RevH_flag = true;
static AMessage_st 		s_Dbg_Message;
static TaskHandle_t 	Task_Msg_Debug_Handle  	= NULL, Device_Information_Handle = NULL,InitActors_Handle=NULL;	// Msg Debug Task Handler
static QueueHandle_t 	msg_Debug_Queue 		= NULL, Device_Information_Que = NULL;	// Msg Debug Queue
static StaticTask_t xConsoleTaskBuffer,xConsoleInitTaskBuffer, xDebugTaskBuffer,xWhoAMITaskBuffer;
PSRAM_ATTR_BSS static StackType_t xTaskStack [CONSOLE_TASK_STACK_DEPTH], WhoAMI_TaskStack [CONSOLE_DEV_INFO_TASK_STACK_DEPTH], Debug_TaskStack[CONSOLE_MSG_DBG_TASK_STACK_DEPTH],InitActors_TaskStack [CONSOLE_INIT_TASK_STACK_DEPTH];
TickType_t QUE_DELAY  =  1;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage[RX_QUE_COUNT * sizeof(AMessage_st)], DevInfo_pucQueueStorage [CONSOLE_DEV_INFO_TASK_QUEUE_LENGTH * CONSOLE_DEV_INFO_TASK_QUEUE_ITEMSIZE], Debug_pucQueueStorage [CONSOLE_MSG_DBG_TASK_QUEUE_LENGTH * sizeof(AMessage_st)];
static StaticQueue_t Monitor_pxQueueBuffer, DevInfo_pxQueueBuffer , Debug_pxQueueBuffer ;
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadData_Xface[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static char payLoadData_Debug[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadData_Device_Info[2048];
PSRAM_ATTR_BSS static char JFS_Debug_MSG_u8[MAX_JSON_PAYLOAD_BYTES+1024];
PSRAM_ATTR_BSS static char Device_Info_data_buffer[2048];
static SemaphoreHandle_t ConsoleActorMutex = NULL;

date_time_t UserDateTimeL_1;
uint16_t  	Tx_uMessageID_u16= 0;
char Read_Response, Read_Response_WIFI;
uint64_t WIFI_MAC_ID;
static bool 			msg_debug_flag 			= 1;
static bool msg_is_debug_enabled(void);
static void msg_debug_init(void);
static void msg_Debug_Task(void *pvParameters __attribute__((unused)));
static void UART0_Debug(AMessage_st *s_Message);
static void JFS_Debug(AMessage_st *s_Message);
static void add_msg_to_msg_debug_queue(AMessage_st *s_Message);
static void msg_debug_deinit(void);
static void getAll(char *, char *,  AMessage_st *); // Get or Read all parameters
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void updateConnectionStatus(void);
static void Actor_list (AMessage_st* s_Message_Rx);
static void Device_Information (void *pvParameters __attribute__((unused)));
static void InitActors(void *pvParameters __attribute__((unused)));
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void epoch_to_date_time_m(date_time_t* date_time,uint64_t epoch);
static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason);
//----------------------------- Actor Tags -----------------------------------------------------//
static const char * THIS_ACTOR 	= "CONSOLE";
static const char 			THIS_ACTOR_ID 	= 	CONSOLE;
//------------------------------------------------
static int16_t	gmt_val;
static uint8_t	dst_val;
//static uint8_t deinitWifi_Flag = 0;
//-------------------------- Actor Parameters ------------------------------------------------------//
PSRAM_ATTR_BSS static struct actor_parameter {
	uint8_t  U0_dbg_u8;
	uint8_t  JFS_dbg_u8;
	uint8_t WIFI_status;
	uint8_t ETH_status;
	uint8_t NET_status;
	uint8_t Heap_Memory;
	uint8_t Enable_WIFI_u8;
	uint8_t Enable_BLE_u8;
	uint8_t Enable_WebServer_u8;
	uint8_t Enable_ETH_u8;
	uint8_t Enable_IHUB_u8;
	uint64_t ETH_status_time_u64;
	uint64_t WIFI_status_time_u64;
	uint64_t WIFI_En_status_time_u64;
}s_Para;

//Actor Property
PSRAM_ATTR static struct property prop[] =
{
    { &s_Para.U0_dbg_u8        			, "U0_DBG"          	, U_INT8,  "RW",  "Debug value for U0" },
    { &s_Para.JFS_dbg_u8       			, "JFS_DBG"         	, U_INT8,  "RW",  "Debug value for JFS" },
    { &s_Para.WIFI_status      			, "WIFI_STATUS"     	, U_INT8,  "RW",  "Status of WIFI connection" },
    { &s_Para.ETH_status       			, "ETH_STATUS"      	, U_INT8,  "RW",  "Status of Ethernet connection" },
    { &s_Para.NET_status       			, "NET_STATUS"      	, U_INT8,  "R",   "Overall network status" },
	{ &s_Para.Heap_Memory      			, "HEAP_MEMORY"     	, U_INT8,  "RW",  "Check free heap memory" },
	{ &s_Para.Enable_WIFI_u8   			, "ENABLE_WIFI"     	, U_INT8,  "RW",  "Enable/Disable WIFI" },
	{ &s_Para.Enable_BLE_u8   			, "ENABLE_BLE"       	, U_INT8,  "RW",  "Enable/Disable BLE" },
	{ &s_Para.Enable_WebServer_u8		, "ENABLE_WEBSERVER"    , U_INT8,  "RW",  "Enable/Disable Web server" },
	{ &s_Para.Enable_ETH_u8   			, "ENABLE_ETH"       	, U_INT8,  "RW",  "Enable/Disable ETH" },
	{ &s_Para.Enable_IHUB_u8   			, "ENABLE_IHUB"     	, U_INT8,  "RW",  "Enable/Disable IHUB" },
	{ &s_Para.ETH_status_time_u64   	, "ETH_UPDATE_TIME"     , U_INT64, "R",   "ETH status last update time" },
	{ &s_Para.WIFI_status_time_u64   	, "WIFI_UPDATE_TIME"    , U_INT64, "R",   "WIFI status last update time" },
	{ &s_Para.WIFI_En_status_time_u64   , "WIFI_EN_UPDATE_TIME" , U_INT64, "R",   "WIFI enable status last update time" }
};

ActorQueue A_Queue[NUMBER_OF_ACTORS] = {                         // Queue property
		{ "NTP"				, NTP 			},
		{ "LED"				, LED 			},
		{ "UART"			, UART 			},
		{ "PUSHBUTTON"		, PUSHBUTTON	},
		{ "SPIFFS"			, SPIFFS		},
		{ "WIFI"			, WIFI 			},
		{ "WEB_SERVER"		, WEB_SERVER 	},
		{ "SMTP_CLIENT"		, SMTP_CLIENT 	},
		{ "HTTP"			, HTTP 			},
		{ "ETH"				, ETH 			},
		{ "BLE"				, BLE 			},
		{ "IHUB"			, IHUB 			},
	    { "IDLE"			, IDLE			},
		{ "FILE_SYSTEM"		, FILE_SYSTEM	},
		{ "TCP_SERVER"		, TCP_SERVER	},
		{ "UDP"			    , UDP 			},
		{ "SQL"				, SQL			},
		{ "CONSOLE"			, CONSOLE	    },
		{ "LIGHTING"		, LIGHTING		},
		{ "SYSTEM"			, SYSTEM		},
		{ "EVENT_ACTOR"		, EVENT_ACTOR	},
		{ "SYS_FILES"		, SYS_FILES		},
		{ "B394_DI"			, B394_DI		},
		{ "Model225"		, MODEL_225		},
};

static const unsigned short days[4][12] =
{
   {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
   { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
   { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
   {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

//-------------------------- Common Actor Resources ------------------------------//

static AMessage_st  s_Message_Tx;  //s_Message_Rx,


static bool				FirstEntry_bool	= false;
static TaskHandle_t 	Task_Handle  = NULL; 	// Console Task Handler
static QueueHandle_t 	msg_Rx_Queue = NULL;	// Console RX Queue

static void init		(void *a, void *b); // Init actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) ;					// Set or Change a parameter
static void get			(char *prop, char *val_a8);					// Get or Read a Parameter
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor		(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void actor_setup	(void);
static void set_rtc_time(AMessage_st* s_Message_Rx);
static uint64_t get_rtc_time(AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
static void set_gmt_dst(AMessage_st* s_Message_Rx);

//----------------------- Common Actor Methods ------------------------------------------------------------------//

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
	char Prev_WIFI_state = s_Para.Enable_WIFI_u8;
	char Prev_ETH_state = s_Para.Enable_ETH_u8;
	char Prev_BLE_state = s_Para.Enable_BLE_u8;
	char Prev_Webserver_state = s_Para.Enable_WebServer_u8;
	char Prev_IHUB_state = s_Para.Enable_IHUB_u8;
	char Prev_WIFI_status = s_Para.WIFI_status;

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

			if(!(strcmp(prop[i].str_name, "ENABLE_WIFI")))
			{
				if(s_Para.Enable_WIFI_u8 != Prev_WIFI_state)
				{
					if(s_Para.Enable_WIFI_u8 == Enable)
					{
						Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "INIT");
					}
					if(s_Para.Enable_WIFI_u8 == Disable)
					{
						Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "DEINIT");
					}
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_BLE")))
			{
				if(s_Para.Enable_BLE_u8 != Prev_BLE_state)
				{
					if(s_Para.Enable_BLE_u8 == Enable)
						Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "INIT");
					if(s_Para.Enable_BLE_u8 == Disable)
						Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "DEINIT");
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_WEBSERVER")))
			{
				if(s_Para.Enable_WebServer_u8 != Prev_Webserver_state)
				{
					if(s_Para.Enable_WebServer_u8 == Enable)
						Send_CMD_To_Other_Actor(WEB_SERVER, "WEB_SERVER", "\0", 0, "INIT");
					if(s_Para.Enable_WebServer_u8 == Disable)
						Send_CMD_To_Other_Actor(WEB_SERVER, "WEB_SERVER", "\0", 0, "DEINIT");
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_ETH")))
			{
				if(s_Para.Enable_ETH_u8 != Prev_ETH_state)
				{
					if(s_Para.Enable_ETH_u8 == Enable)
						Send_CMD_To_Other_Actor(ETH, "ETH", "\0", 0, "INIT");
					if(s_Para.Enable_ETH_u8 == Disable)
						Send_CMD_To_Other_Actor(ETH, "ETH", "\0", 0, "DEINIT");
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_IHUB")))
			{
				if(s_Para.Enable_IHUB_u8 != Prev_IHUB_state)
				{
					if(s_Para.Enable_IHUB_u8 == Enable)
						Send_CMD_To_Other_Actor(IHUB, "IHUB", "\0", 0, "INIT");
					if(s_Para.Enable_IHUB_u8 == Disable)
						Send_CMD_To_Other_Actor(IHUB, "IHUB", "\0", 0, "DEINIT");
				}
			}
			if((s_Para.U0_dbg_u8 == 1) || (s_Para.JFS_dbg_u8==1))
			{
				actor_setup();
			}
			if((s_Para.U0_dbg_u8 == 0) && (s_Para.JFS_dbg_u8==0))
			{
				msg_debug_flag = false;
				msg_debug_deinit();
			}
			if(!(strcmp(prop[i].str_name, "ETH_STATUS")))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				s_Para.ETH_status_time_u64  = current_epos_sec;
			}
			if(!(strcmp(prop[i].str_name, "WIFI_STATUS")))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
//				if(deinitWifi_Flag == 0)

				if((s_Para.WIFI_status == E_wifi_NOT_CONNECTED) || (s_Para.WIFI_status == E_wifi_CONNECTED_LOW))
				{
					if(s_Para.WIFI_status != Prev_WIFI_status)
					{
						s_Para.WIFI_status_time_u64  = current_epos_sec;
	//					deinitWifi_Flag = 1;
					}
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_WIFI")))
			{
				struct timeval currentTime;
				_gettimeofday_r(NULL, &currentTime, NULL);
				uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
				s_Para.WIFI_En_status_time_u64  = current_epos_sec;
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

static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx){

	cJSON *out_JSON  = cJSON_CreateObject();
	cJSON_AddStringToObject(out_JSON, "FILE_NAME", "A:/System/CONSOL.json");

	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {

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
		if((strcmp(prop[i].str_name, "WIFI_STATUS") !=0) && (strcmp(prop[i].str_name, "ETH_STATUS")) && (strcmp(prop[i].str_name, "NET_STATUS")) && (strcmp(prop[i].str_name, "WIFI_EN_UPDATE_TIME")) && (strcmp(prop[i].str_name, "WIFI_UPDATE_TIME")) && (strcmp(prop[i].str_name, "ETH_UPDATE_TIME")) && (strcmp(prop[i].str_name, "JFS_DBG")) && (strcmp(prop[i].str_name, "U0_DBG")))
		{
			cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
		}
	}	
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(out_JSON);
	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");

}	//	getAll


static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[32] = {0};
	char typeString[20] = {0};

	int no_of_elements = sizeof(prop) / sizeof(struct property);

	    // Create JSON arrays
	    cJSON *jsonArrayName = cJSON_CreateArray();
	    cJSON *jsonArrayType = cJSON_CreateArray();
	    cJSON *jsonArrayValue = cJSON_CreateArray();
	    cJSON *jsonArrayAccess = cJSON_CreateArray();
	    cJSON *jsonArrayHelpString = cJSON_CreateArray();

	    for (int i = 0; i < no_of_elements; i++) {
	    	memset(val_a8,     0, sizeof(val_a8));
	    	memset(typeString, 0, sizeof(typeString));
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
			if(!(strcmp(prop[i].str_name, "WIFI_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_wifi_ZERO_STATE:
					strcpy(val_a8, "WIFI IDLE STATE");
					break;

				case E_wifi_NOT_CONNECTED:
					strcpy(val_a8, "WIFI NOT CONNECTED");
					break;

				case E_wifi_CONNECTED_LOW:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_HIGH:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_AP:
					strcpy(val_a8, "WIFI CONNECTED IN AP MODE");
					break;

				default:
					break;
				}
			}

			if(!(strcmp(prop[i].str_name, "ETH_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_ETH_ZERO_STATE:
					strcpy(val_a8, "ETHERNET INIT STATE");
					break;

				case E_ETH_NOT_CONNECTED:
					strcpy(val_a8, "ETHERNET NOT CONNECTED");
					break;

				case E_ETH_CONNECTED:
					strcpy(val_a8, "ETHERNET CONNECTED");
					break;

				case E_ETH_LINK_UP:
					strcpy(val_a8, "ETHERNET LINK UP");
					break;

				case E_ETH_STARTED:
					strcpy(val_a8, "ETHERNET STARTED");
					break;

				case E_ETH_STOPPED:
					strcpy(val_a8, "ETHERNET STOPPED");
					break;

				default:
					break;
				}
			}

			if(!(strcmp(prop[i].str_name, "NET_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_NET_ZERO_STATE:
					strcpy(val_a8, "INTERNET INIT STATE");
					break;

				case E_NET_NOT_CONNECTED:
					strcpy(val_a8, "INTERNET NOT CONNECTED");
					break;

				case E_NET_CONNECTED:
					strcpy(val_a8, "INTERNET CONNECTED");
					break;

				default:
					break;
				}
			}

			if(!(strcmp(prop[i].str_name, "U0_DBG")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "UART 0 DEBUG DISABLED");
					break;

				case 1:
					strcpy(val_a8, "UART 0 DEBUG ENABLED");
					break;

				default:
					break;
				}
			}

			if(!(strcmp(prop[i].str_name, "JFS_DBG")))
			{
				switch (*(char*)prop[i].name)
				{
				case 0:
					strcpy(val_a8, "AUDIT LOG DISABLED");
					break;

				case 1:
					strcpy(val_a8, "AUDIT LOG ENABLED");
					break;

				default:
					break;
				}
			}  //
			if(!(strcmp(prop[i].str_name, "ENABLE_WIFI")))
			{
				switch (*(char*)prop[i].name)
				{
				case Disable:
					strcpy(val_a8, "WIFI DISABLED");
					break;

				case Enable:
					strcpy(val_a8, "WIFI ENABLED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_BLE")))
			{
				switch (*(char*)prop[i].name)
				{
				case Disable:
					strcpy(val_a8, "BLE DISABLED");
					break;

				case Enable:
					strcpy(val_a8, "BLE ENABLED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_WEBSERVER")))
			{
				switch (*(char*)prop[i].name)
				{
				case Disable:
					strcpy(val_a8, "WEB SERVER DISABLED");
					break;

				case Enable:
					strcpy(val_a8, "WEB SERVER ENABLED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_ETH")))
			{
				switch (*(char*)prop[i].name)
				{
				case Disable:
					strcpy(val_a8, "ETH DISABLED");
					break;

				case Enable:
					strcpy(val_a8, "ETH ENABLED");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "ENABLE_IHUB")))
			{
				switch (*(char*)prop[i].name)
				{
				case Disable:
					strcpy(val_a8, "IHUB DISABLED");
					break;

				case Enable:
					strcpy(val_a8, "IHUB ENABLED");
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
		// Convert JSON object to string
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the CONSOLE actor.");
	cJSON_AddStringToObject(responseObject, "SET(string U0_DBG, string JFS_DBG)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET_RTC_TIME()", "Get RTC time.");
	cJSON_AddStringToObject(responseObject, "SET_RTC_TIME(U64 EPOCH_UTC_MS)","Set RTC time.");
	cJSON_AddStringToObject(responseObject, "ACTOR_LIST()", "Obtain the list of activated actors.");
	cJSON_AddStringToObject(responseObject, "WHO_AM_I()", "Obtain the device information.");
	cJSON_AddStringToObject(responseObject, "ESP_RESET()", "Reset the ESP");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void init(void *a, void *b) {

	if(FirstEntry_bool){
		return;
	}
	actor_setup();
	memset(&s_Message_Tx,0,sizeof(s_Message_Tx));

    ConsoleActorMutex = xSemaphoreCreateMutex();
    if (ConsoleActorMutex == NULL)
    {
        printf("Failed to create mutex for console actor \n");
    }

	//Create msg_Rx_Queue
    msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

    if (msg_Rx_Queue == NULL) {
		printf("Error! in creating console RX queue");
		return;
	}

	Task_Handle = xTaskCreateStatic(
	    monitor,                 // Task function
	    "Console Monitor",            // Task name
		CONSOLE_TASK_STACK_DEPTH,        // Stack size in words
	    NULL,                    // Task parameters (not used here)
		CONSOLE_TASK_PRIORITY,                       // Task priority
	    xTaskStack,              // Pointer to task stack (allocated in PSRAM)
	    &xConsoleTaskBuffer             // Pointer to task control block
	);

	if (Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
	    printf("Failed to create task\n");
#endif
	    // Handle error
	}

	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	s_Para.WIFI_status_time_u64 = 0;
	s_Para.ETH_status_time_u64 = 0;
	s_Para.WIFI_En_status_time_u64 = 0;
	s_Para.Heap_Memory = 0;
	s_Para.U0_dbg_u8  		= 0;
	s_Para.JFS_dbg_u8 		= 1;
	FirstEntry_bool = true;
	JFS_actor_setup();
	InitActors_Handle = xTaskCreateStatic(
											InitActors,                 // Task function
											"INIT_ACTORS",            // Task name
											CONSOLE_INIT_TASK_STACK_DEPTH,        // Stack size in words
											NULL,                    // Task parameters (not used here)
											CONSOLE_INIT_TASK_PRIORITY,                       // Task priority
											InitActors_TaskStack,              // Pointer to task stack (allocated in PSRAM)
											&xConsoleInitTaskBuffer             // Pointer to task control block
										);

		if (InitActors_Handle == NULL) {
	#ifdef ENABLE_PRINT_MSG
		    printf("Failed to create task\n");
	#endif
		    // Handle error
		}

#ifdef ENABLE_TIMER
	//======================= Config 1 msec general Timer =============================
	const esp_timer_create_args_t GenPeriodic_timer_args = { .callback = &Generic_timer_callback,	.name = "Generic_Periodic_Timer"	};

	esp_timer_handle_t Periodic_Timer;
	ESP_ERROR_CHECK(esp_timer_create(&GenPeriodic_timer_args, &Periodic_Timer));
	// The timer has been created but is not running yet

	// Start the timers
	ESP_ERROR_CHECK(esp_timer_start_periodic(Periodic_Timer, 1000));
#endif
}	// init

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	uint8_t u8Result =0;
	while (1)
	{
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))  //10
		{
			memcpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN-1));

			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if((msg_debug_flag == 1) &&
					strcmp((char*)s_Message_Rx->cmdFun_a8, "SAVE_AUDIT_LOG") &&
					strcmp((char*)s_Message_Rx->cmdFun_a8, "JFS_FORMAT") &&
					strcmp((char*)s_Message_Rx->cmdFun_a8, "COPY_JFS_SD") &&
					strcmp((char*)s_Message_Rx->cmdFun_a8, "SMTP_READ") &&
					strcmp((char*)s_Message_Rx->cmdFun_a8, "MAIL_BINARY") &&
					(((strcmp((char*)s_Message_Rx->src_Actor_a8, "SMTP_CLIENT") != 0) && (strcmp((char*)s_Message_Rx->dest_Actor_a8, "SMTP_CLIENT")) != 0)|| (strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE") != 0)))
			{
				add_msg_to_msg_debug_queue(s_Message_Rx);
//				printf("console debug msg_Rx_Queue S = %s, D = %s, C = %s, size = %d\n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8, s_Message_Rx->payload_size);
//				if(s_Message_Rx->payload_p8 != NULL)
//				{
//					if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//						printf("DT = %s\n",s_Message_Rx->payload_p8);
//				}
			}
//			printf("\n\nconsole msg_Rx_Queue S = %s, D = %s, C = %s, size = %d\n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8, s_Message_Rx->payload_size);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("DT = %s\n\n",s_Message_Rx->payload_p8);
//			}

			if (!strcmp((char*) s_Message_Rx->dest_Actor_a8, "CONSOLE"))
			{
				if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) {    // Init
					init(0, 0);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) { // Get Actor Properties
					Get_Property(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SET")) // Set Actor Properties
				{
					u8Result =0;
					name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
					if (name_JSON == NULL) {
						sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
					    }
					else{
						head_JSON = name_JSON->child;
						cJSON *root_JSON  = NULL;

						if((strcmp(head_JSON->string, "WIFI_STATUS") !=0) && (strcmp(head_JSON->string, "ETH_STATUS") !=0) && (strcmp(head_JSON->string, "NET_STATUS") !=0) && (strcmp(head_JSON->string, "JFS_DBG") !=0) && (strcmp(head_JSON->string, "U0_DBG") !=0)) ///*&& (strcmp(head_JSON->string, "ENABLE_WIFI") !=0) && (strcmp(head_JSON->string, "ENABLE_BLE") !=0) && (strcmp(head_JSON->string, "ENABLE_WEBSERVER") !=0) && (strcmp(head_JSON->string, "ENABLE_ETH") !=0) && (strcmp(head_JSON->string, "ENABLE_IHUB") !=0)*/ )
						{
							root_JSON = cJSON_CreateObject();
							cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/CONSOL.json");
						}
					   // Loop through each key-value pair
					    do {
					    	// Check if the value string is not NULL
					    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
							{
								// Set the key-value pair
								u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
							if((strcmp(head_JSON->string, "WIFI_STATUS") !=0) && (strcmp(head_JSON->string, "ETH_STATUS") !=0) && (strcmp(head_JSON->string, "NET_STATUS") !=0) /*&& (strcmp(head_JSON->string, "ENABLE_WIFI") !=0) && (strcmp(head_JSON->string, "ENABLE_BLE") !=0) && (strcmp(head_JSON->string, "ENABLE_WEBSERVER") !=0) && (strcmp(head_JSON->string, "ENABLE_ETH") !=0) && (strcmp(head_JSON->string, "ENABLE_IHUB") !=0)*/)
							{
								if((u8Result==1) && (root_JSON != NULL))
								{
									cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
								}
								else if(u8Result==2){
									sprintf(str,"'%s' is a read only property", head_JSON->string);
									Add_Response_msg(str, s_Message_Rx, payLoadData);
								}
								else
								{
									cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
								}
							  }
							}
							else {
					            // Handle the case where value string is NULL (e.g., log an error or take appropriate action)
					            sprintf(str, "Invalid parameter '%s'", head_JSON->string);
					            Add_Response_msg(str,s_Message_Rx, payLoadData);
					            // Handle the error as per your application's requirements
					        }
					        head_JSON = head_JSON->next;
					    } while (head_JSON != 0);
					    if(u8Result==1){
					  	//  save parameters to JFS
					    if((cJSON_IsObject(root_JSON))  && (cJSON_IsString(root_JSON->child)))
					    {
							memset(payLoadData,0,sizeof(payLoadData));
							cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
						   	cJSON_Delete(root_JSON);
						   	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
					    }

					    console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
					    }
					    // Free the parsed JSON
					    cJSON_Delete(name_JSON);
						}

					// Update Internet connection status from WiFi and Ethernet
						updateConnectionStatus();
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
					help(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "ACTOR_LIST")){
					Actor_list(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "WHO_AM_I")){
					if(Device_Information_Que == NULL)
					{
						Device_Information_Que = xQueueCreateStatic(CONSOLE_DEV_INFO_TASK_QUEUE_LENGTH, CONSOLE_DEV_INFO_TASK_QUEUE_ITEMSIZE, DevInfo_pucQueueStorage, &DevInfo_pxQueueBuffer);
					}
					if (Device_Information_Que == NULL)
					{
						Add_Response_msg("Error! Device_Information_Que is not created.", s_Message_Rx, payLoadData);
					}
					if(Device_Information_Handle == NULL)
					{
						AMessage_st s_Message_Rx_data;
						memset(Device_Info_data_buffer,0,sizeof(Device_Info_data_buffer));
						s_Message_Rx_data.payload_p8 = (uint8_t*)Device_Info_data_buffer;
						strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
						strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
						strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);
						Device_Information_Handle = xTaskCreateStaticPinnedToCore(
									Device_Information,                 // Task function
									"WHO AM I",            // Task name
									CONSOLE_DEV_INFO_TASK_STACK_DEPTH,        // Stack size in words
									&s_Message_Rx_data,                    // Task parameters (not used here)
									CONSOLE_DEV_INFO_TASK_PRIORITY,                       // Task priority
									WhoAMI_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xWhoAMITaskBuffer,             // Pointer to task control block
									1
						);
						if (Device_Information_Handle == NULL)
						{
							#ifdef ENABLE_PRINT_MSG
									printf("Failed to create the Debug task");
							#endif

						}
					}
				}
				// Check if responce has come from a actor
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
					Analyse_Response(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET_RTC_TIME")) {
					get_rtc_time(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SET_RTC_TIME")) {
					set_rtc_time(s_Message_Rx);
				}
				else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "ESP_RESET")) {

					Add_Response_msg("Resetting the device. Kindly wait...", s_Message_Rx, payLoadData);
					Restart_ESP_Xface(1);
				}
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
				 {
					get_actor_properties(s_Message_Rx);
				 }
				else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "GMT_DST"))
				 {
					set_gmt_dst(s_Message_Rx);
				 }
				else
				{
					Add_Response_msg("Invalid method", s_Message_Rx, payLoadData);
				}
			}  // end of if (!strcmp((char*) s_Message_Rx->dest_Actor_a8, "CONSOLE"))

			// if msg is for other actor
			else
			{
				console_que_sel(s_Message_Rx);
			}
		}
	}
}//	monitor

//------------- Console functions -------------------------------------//

void Console_Initialize(){
	init(0,0);
}//	Console_Initialize

static void actor_setup(void) {
	if(msg_is_debug_enabled()){
		msg_debug_init();
	}
}//	actor_setup

void console_que_sel(AMessage_st* s_Message) {
	uint8_t actor_found = 0; // Flag to check if actor is found
	char payload[100] = {0};
	AMessage_st s_Message_Tx_new = {0};
	s_Message_Tx_new.payload_p8 = heap_caps_calloc((s_Message->payload_size+1),sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (s_Message_Tx_new.payload_p8 == NULL)
	{
		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message, payload);
		return;
	}
	s_Message_Tx_new.Dest_ID_a8 = s_Message->Dest_ID_a8;
	s_Message_Tx_new.Src_ID_u8 = s_Message->Src_ID_u8;
	s_Message_Tx_new.payload_size = s_Message->payload_size;
	// required memcpy to store binary file in JFS and to transfer binary data to any actor.
	memcpy((char*)s_Message_Tx_new.payload_p8, (char*)s_Message->payload_p8, s_Message->payload_size );
	strcpy((char*)s_Message_Tx_new.cmdFun_a8, (char*)s_Message->cmdFun_a8);
	strcpy((char*)s_Message_Tx_new.dest_Actor_a8, (char*)s_Message->dest_Actor_a8);
	strcpy((char*)s_Message_Tx_new.src_Actor_a8, (char*)s_Message->src_Actor_a8);
	int no_of_elements = sizeof(A_Queue) / sizeof(ActorQueue);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp((char*)s_Message_Tx_new.dest_Actor_a8 , A_Queue[i].name)) {
			actor_found = 1; // Set flag to indicate actor is found
			switch (A_Queue[i].type) {

				case UART			:	UART_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
				case WIFI			:	if(s_Para.Enable_WIFI_u8 == Enable)
											wifi_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										else if(strcmp((char*)s_Message_Tx_new.cmdFun_a8, "DEINIT") == 0)
										{
											wifi_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										}
										else
										{
											Add_Response_msg("WIFI is disabled",s_Message, payload);
											if (s_Message_Tx_new.payload_p8 != NULL)
											{
												heap_caps_free(s_Message_Tx_new.payload_p8);
											}
										}
										break;
				case BLE			:	if(s_Para.Enable_BLE_u8 == Enable)
											ble_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										else if(strcmp((char*)s_Message_Tx_new.cmdFun_a8, "DEINIT") == 0)
										{
											ble_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										}
										else
										{
											Add_Response_msg("BLE is disabled",s_Message, payload);
											if (s_Message_Tx_new.payload_p8 != NULL)
											{
												heap_caps_free(s_Message_Tx_new.payload_p8);
											}
										}
										break;
				case HTTP			:	http_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
//				case IDLE			:	Idle_ConsolWriteToActor_xface (&s_Message_Tx_new);			break;
				case NTP			:	NTP_ConsolWriteToActor_xface(&s_Message_Tx_new);			break;
				case LED			:	LED_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
				case PUSHBUTTON		:	PUSHBUTTON_ConsoleWriteToActor_xface(&s_Message_Tx_new);	break;
				case WEB_SERVER		:	if(s_Para.Enable_WebServer_u8 == Enable)
											WEB_SERVER_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										else if(strcmp((char*)s_Message_Tx_new.cmdFun_a8, "DEINIT") == 0)
										{
											WEB_SERVER_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										}
										else
										{
											Add_Response_msg("Web Server is disabled",s_Message, payload);
											if (s_Message_Tx_new.payload_p8 != NULL)
											{
												heap_caps_free(s_Message_Tx_new.payload_p8);
											}
										}
										break;
				case FILE_SYSTEM	:	File_ConsoleWriteToActor_xface(&s_Message_Tx_new);		    break;
#if defined(B480) || defined(B543) || defined(B542) || defined(B394)  || defined(B527)  //Here B480 check is given to avoid continuous reboot in B553 device which occurs if Ethernet is initialized.
			  //(If consol.json file contains Ethernet_enable =1, then continuous reboot occurs.)
				case ETH			:	if(s_Para.Enable_ETH_u8 == Enable)
											ETH_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										else if(strcmp((char*)s_Message_Tx_new.cmdFun_a8, "DEINIT") == 0)
										{
											ETH_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										}
										else
										{
											Add_Response_msg("Ethernet is disabled",s_Message, payload);
											if (s_Message_Tx_new.payload_p8 != NULL)
											{
												heap_caps_free(s_Message_Tx_new.payload_p8);
											}
										}
										break;
#endif
				case SMTP_CLIENT	:	SMTP_CLIENT_ConsolWriteToActor_xface(&s_Message_Tx_new);	break;
				case SPIFFS			:	SPIFFS_ConsoleWriteToActor_xface(&s_Message_Tx_new);		    break;
				case IHUB			:
										if(s_Para.Enable_IHUB_u8 == Enable)
										IHUB_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										else if(strcmp((char*)s_Message_Tx_new.cmdFun_a8, "DEINIT") == 0)
										{
											IHUB_ConsoleWriteToActor_xface(&s_Message_Tx_new);
										}
										else
										{
											Add_Response_msg("IHUB is disabled",s_Message, payload);
											if (s_Message_Tx_new.payload_p8 != NULL)
											{
												heap_caps_free(s_Message_Tx_new.payload_p8);
											}
										}
										break;
				case TCP_SERVER		:   TCP_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
				case UDP			:	UDP_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
				case SQL			:	SQL_ConsoleWriteToActor_xface(&s_Message_Tx_new);			break;
#if !defined(B394) //&& !defined(B527)
				case LIGHTING		:	LIGHT_ConsoleWriteToActor_xface(&s_Message_Tx_new);         break;
#endif
				case SYSTEM			:	SYSTEM_ConsoleWriteToActor_xface(&s_Message_Tx_new);        break;
				case EVENT_ACTOR    :	EVENT_ConsoleWriteToActor_xface(&s_Message_Tx_new);         break;
				case SYS_FILES		:   System_Files_ConsoleWriteToActor_xface(&s_Message_Tx_new);	break;
#if defined(B394)
				case B394_DI		:	B394_DI_ConsoleWriteToActor_xface(&s_Message_Tx_new);	break;
#endif
				case MODEL_225		:	model225_ConsoleWriteToActor_xface(&s_Message_Tx_new);	break;
			    default			    :	break;
			}
			break; // Exit loop once actor is found and processed
		}

	}
	/* Route all Model 225 sub-actors (menu navigation actors) to 195_Actor */
	if (!actor_found) {
		static const char * const m225_subs[] = {
			"IndicatorSetup","ScaleSetup","ScaleCalibration","LoadCellAssignments",
			"ComSetup","SerialPorts","Ethernet","WiFi","ISiteIP","SendGross","BankMode",
			"PrinterSetup","SystemConfig","Accumulators","DACOutput","KeyLockout",
			"BadgeReader","WINVRS","ModeConfig","IDStorage","DFC","Batcher",
			"PackageWeigher","AxleWeigher","CheckWeigher","PWC","Livestock",
			"DLCSetup","ReviewMenu","195Menu"
		};
		for (int si = 0; si < (int)(sizeof(m225_subs)/sizeof(m225_subs[0])); si++) {
			if (!strcmp((char*)s_Message_Tx_new.dest_Actor_a8, m225_subs[si])) {
				model225_ConsoleWriteToActor_xface(&s_Message_Tx_new);
				actor_found = 1;
				break;
			}
		}
	}

	if (!actor_found) {
	    // Actor not found, add error message
		if(s_Message_Tx_new.payload_p8 != NULL)
		{
			if(strlen((char*)s_Message_Tx_new.payload_p8) != 0)
				 printf(">CONSOLE.ERROR(Invalid actor), Payload = %s\n", s_Message_Tx_new.payload_p8);

			//delete the payload if it valid json. this logic is added to avoid reboot at the time of multiple command execution (corrupted payload).
			cJSON *root = cJSON_Parse((char*)s_Message_Tx_new.payload_p8);
			if(root != NULL)
			{
				heap_caps_free(s_Message_Tx_new.payload_p8);  // free the payload
				cJSON_Delete(root);
			}
		}
	}
}//	console_que_sel

static void add_msg_to_msg_debug_queue(AMessage_st *s_Message) {
	AMessage_st s_DbgMessage;
	char *DbgPayload = (char*) heap_caps_calloc(strlen((char*) s_Message->payload_p8) + 1, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if(DbgPayload != NULL)
	{
		strcpy(DbgPayload, (char*) s_Message->payload_p8);
		s_DbgMessage.payload_size = s_Message->payload_size;
		s_DbgMessage.payload_p8 = (uint8_t*) DbgPayload;
		strcpy((char*) s_DbgMessage.src_Actor_a8	, 	(char*) s_Message->src_Actor_a8	);
		strcpy((char*) s_DbgMessage.dest_Actor_a8	,	(char*) s_Message->dest_Actor_a8);
		strcpy((char*) s_DbgMessage.cmdFun_a8		, 	(char*) s_Message->cmdFun_a8	);
		uint8_t state = xQueueSend(msg_Debug_Queue, &s_DbgMessage, 0);
		if(state != pdTRUE)
		{
			#ifdef ENABLE_PRINT_MSG
			printf("Could not send logs to msg_Debug_Queue \n");
			#endif
			free(DbgPayload);
			s_DbgMessage.payload_p8 = NULL;
			DbgPayload = NULL;
		}
	}
	else
	{
		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message, payLoadData);
	}
}//	add_msg_to_msg_debug_queue


static bool msg_is_debug_enabled(void){

	bool debugEnabled = false;
	if(s_Para.U0_dbg_u8 == 1){
		debugEnabled = true;
	}
	if(s_Para.JFS_dbg_u8 == 1){
		//Console_Send_data_to_Other_Actor_xface("\0", THIS_ACTOR, "JFS", "INIT" );  //enable JFS
		debugEnabled = true;
	}
	msg_debug_flag = debugEnabled;
	return debugEnabled;
}//	msg_is_debug_enabled

static void msg_debug_init(void){

	// Creating Msg Debug Queue
	if(msg_Debug_Queue == NULL)
	{
		msg_Debug_Queue = xQueueCreateStatic(CONSOLE_MSG_DBG_TASK_QUEUE_LENGTH, sizeof(AMessage_st), Debug_pucQueueStorage, &Debug_pxQueueBuffer);
	}
	// Creating Msg Debug Task
	if(Task_Msg_Debug_Handle == NULL)
	{
			Task_Msg_Debug_Handle = xTaskCreateStatic(
							msg_Debug_Task,                 // Task function
							"Debug",            // Task name
							CONSOLE_MSG_DBG_TASK_STACK_DEPTH,        // Stack size in words
							NULL,                    // Task parameters (not used here)
							CONSOLE_MSG_DBG_TASK_PRIORITY,                       // Task priority
							Debug_TaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xDebugTaskBuffer             // Pointer to task control block
			);
			if (Task_Msg_Debug_Handle == NULL)
			{
#ifdef ENABLE_PRINT_MSG
		printf("Failed to create the Debug task");
#endif
			}
	}
}// msg_debug_init

static void msg_debug_deinit(void)
{
	if(Task_Msg_Debug_Handle != NULL)
	{
		 vTaskDelete(Task_Msg_Debug_Handle);
		 Task_Msg_Debug_Handle = NULL;
	}
}// msg_debug_init

static void msg_Debug_Task(void *pvParameters __attribute__((unused))){
	while (1) {
		if (pdTRUE == xQueueReceive(msg_Debug_Queue, (void*) (&s_Dbg_Message), portMAX_DELAY)) { //Msg debug queue handler
			if(s_Para.U0_dbg_u8){
				UART0_Debug(&s_Dbg_Message);
			}
			if(s_Para.JFS_dbg_u8){
				JFS_Debug(&s_Dbg_Message);
			}
			console_MessageRelease_xface((char*) s_Dbg_Message.payload_p8);
		}
	}
}//	msg_Debug_Task

static void UART0_Debug(AMessage_st *s_Message){
	cJSON *in_JSON 		= NULL;
	if((s_Message->payload_p8) != NULL)
	{
		if(strlen((char*) s_Message->payload_p8) != 0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message->payload_p8);
				if (in_JSON != NULL)
				{
					// Obtain the COMMAND and RESPONSE keys
					cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

					if (commandKey != NULL && responseKey != NULL)
					{
						memset(payLoadData_Debug,0,sizeof(payLoadData_Debug));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData_Debug, sizeof(payLoadData_Debug), false);
						printf(">%s.%s(%s, %s)\n", s_Message->src_Actor_a8, commandKey->valuestring,  payLoadData_Debug,  THIS_ACTOR);  // response
					}
					else
					{
						printf("<%s.%s(%s)\n", s_Message->dest_Actor_a8, s_Message->cmdFun_a8,  s_Message->payload_p8);  // command
					}
					cJSON_Delete(in_JSON);
				}
		 }
	}
	else
		printf("<%s.%s()\n", s_Message->dest_Actor_a8, s_Message->cmdFun_a8); // command
			// Free the parsed JSON
}//	UART0_Debug

static void JFS_Debug(AMessage_st *s_Message){
	 date_time_t  sdate_tim;
	uint64_t current_epos_msec;//mills;
	uint64_t Temp_msec_1;
	struct timeval currentTime;
    memset(JFS_Debug_MSG_u8,0,sizeof(JFS_Debug_MSG_u8));
    cJSON *in_JSON         = NULL;
    _gettimeofday_r(NULL, &currentTime, NULL);
		current_epos_msec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
#ifdef ENABLE_PRINT_MSG
		printf("Current_epos_sec: %lld\n", current_epos_msec);
#endif
		Temp_msec_1 = EPOSCH_TO_30_YEAR;
		Temp_msec_1 = Temp_msec_1*1000;
		current_epos_msec = current_epos_msec - Temp_msec_1;

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

	if((s_Message->payload_p8) != NULL)
	{
		if(strlen((char*) s_Message->payload_p8) != 0)
		{
		   in_JSON         = cJSON_Parse((char*) s_Message->payload_p8);
			if (in_JSON != NULL)
			 {
					// Obtain the COMMAND and RESPONSE keys
					cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

					if (commandKey != NULL && responseKey != NULL)
					{
						memset(payLoadData_Debug,0,sizeof(payLoadData_Debug));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData_Debug, sizeof(payLoadData_Debug), false);
						sprintf(JFS_Debug_MSG_u8,"%02d/%02d/%02d %02d:%02d:%02d.%03d | >%s.%s(%s, %s)\n", UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, s_Message->src_Actor_a8, commandKey->valuestring,  payLoadData_Debug,  s_Message->dest_Actor_a8);
					}
					else
					{
						memset(payLoadData_Debug,0,sizeof(payLoadData_Debug));//\0';
						cJSON_PrintPreallocated(in_JSON, payLoadData_Debug, sizeof(payLoadData_Debug), false);
						sprintf(JFS_Debug_MSG_u8,"%02d/%02d/%02d %02d:%02d:%02d.%03d | <%s.%s(%s, %s)\n",UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, s_Message->dest_Actor_a8, s_Message->cmdFun_a8,  payLoadData_Debug,  s_Message->src_Actor_a8);
					}
					// Free the parsed JSON
					cJSON_Delete(in_JSON);
			 }
		}
	}
	else
	{
		sprintf(JFS_Debug_MSG_u8,"%02d/%02d/%02d %02d:%02d:%02d.%03d | <%s.%s(%s)\n", UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, s_Message->dest_Actor_a8, s_Message->cmdFun_a8,  s_Message->src_Actor_a8);
	}

	if((strlen(JFS_Debug_MSG_u8)!=0))
	{
		Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", JFS_Debug_MSG_u8, strlen(JFS_Debug_MSG_u8), "SAVE_AUDIT_LOG");
	}
}//    JFS_Debug

static void console_add_responce_to_console_Rx_Queue(AMessage_st *s_Message_new) {
	AMessage_st s_Message;
	char *PayloadPtrVal = (char*) heap_caps_calloc((strlen((char*)s_Message_new->payload_p8) + 1),sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (PayloadPtrVal == NULL)
	{
		printf("Memory allocation failed in console actor\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_new, payLoadData);
		return;
	}
	strcpy(PayloadPtrVal, (char*)s_Message_new->payload_p8);
	s_Message.payload_p8 = (uint8_t*) PayloadPtrVal;
	s_Message.payload_size = strlen(PayloadPtrVal);
	strcpy((char*)s_Message.cmdFun_a8, "RESPONSE");
	strcpy((char*)s_Message.dest_Actor_a8, (char*) s_Message_new->src_Actor_a8);
	strcpy((char*)s_Message.src_Actor_a8, (char*) s_Message_new->dest_Actor_a8);
	xQueueSend(msg_Rx_Queue, &s_Message, QUE_DELAY);
} //	send_responce_to_console_xface

//------------- Actor Interface Methods --------------------------------------------------//

void console_send_responce_to_console_xface(AMessage_st *s_Message_new) {

	 if (xSemaphoreTake(ConsoleActorMutex, portMAX_DELAY) == pdTRUE)
	 {
		if(s_Message_new->payload_p8 == NULL)
		{
			xSemaphoreGive(ConsoleActorMutex);
			return;
		}

		if(strlen((char*)s_Message_new->payload_p8) == 0)
		{
			xSemaphoreGive(ConsoleActorMutex);
			return;
		}

		cJSON *responseObject= NULL;
		cJSON *root = cJSON_CreateObject();
		char str[100]={0};
		if(root != NULL)
		{
			cJSON_AddStringToObject(root, "COMMAND",(char*) s_Message_new->cmdFun_a8);
			responseObject = cJSON_Parse((char*)s_Message_new->payload_p8);  	// Create a RESPONSE object and add it to the RESPONSE array
			if (responseObject == NULL)
			{
				//printf("\n\n console_send_responce_to_console_xface s_Message_new->payload_p8 = %s \n\n", (char*)s_Message_new->payload_p8);
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_new, payLoadData_Xface);
				cJSON_Delete(root);
				root = NULL;
				xSemaphoreGive(ConsoleActorMutex);
				return;
			}
			else
			{
					cJSON_AddItemToObject(root, "RESPONSE", responseObject);
					s_Message_new->payload_p8[0] = '\0';
					cJSON_PrintPreallocated(root, (char*) s_Message_new->payload_p8, MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN, false);
					cJSON_Delete(root);
					root = NULL;
					console_add_responce_to_console_Rx_Queue(s_Message_new);
			}
		}
		xSemaphoreGive(ConsoleActorMutex);
	 }
}

void console_ActorWriteToConsole_xface(AMessage_st *s_Message) {
	AMessage_st *s_Message_New = s_Message;
	char retry = 0;
	do
	{
		uint8_t state = xQueueSend(msg_Rx_Queue, s_Message_New, QUE_DELAY); //5,1
		if (state == pdTRUE)
		{
			break;
		}
		else
		{
			if (state == errQUEUE_FULL)
			{
				printf("<CONSOLE.ERROR(CONSOLE RX Queue is full. Resetting the ESP...)\n");
				Restart_ESP_Xface(1);
			}
			else
			{
				printf("<CONSOLE.ERROR(CONSOLE RX Queue send unsuccessful)\n");
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
		}
	} while(++retry < 3);

	if(retry >= 3)
	{
		printf("<CONSOLE.ERROR(CONSOLE RX Queue send unsuccessful. Resetting the ESP...)\n");
		Restart_ESP_Xface(1);
	}
}	//	ActorWriteToConsole_xface
int prev_heap=0, prev_heap_psram = 0;

void console_MessageRelease_xface(char *argBuf) {
	if(argBuf != NULL)
	{
		//printf("\n argBuf = %s \n", argBuf);
		heap_caps_free(argBuf);
		argBuf = NULL;
	}
	if(s_Para.Heap_Memory == 1)
	{
		size_t  ha = xPortGetFreeHeapSize(); //xPortGetFreeHeapSize();
		size_t ha1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM); //esp_get_minimum_free_heap_size();
		if((ha != prev_heap) || (ha1 != prev_heap_psram))
		{
			printf("Internal Free heap Now : %d\n", ha);
			printf("PSRAM Free heap Now : %d\n", ha1);
			prev_heap = ha;
			prev_heap_psram = ha1;
		}
	}
	size_t  ha = xPortGetFreeHeapSize(); //();
	if((ha < 10000)  && (ha != prev_heap))
	{
		printf("\n Internal Free heap Now : %d\n", ha);
		prev_heap = ha;
	}
	size_t  ha1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM); //esp_get_minimum_free_heap_size();
	if(ha1 < 10000)
		printf("\n PSRAM Free heap Now : %d\n", ha1);
}  //	MessageRelease_xface

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	char keyValue [50] = {0};
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n CONSOLE s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
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
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData,THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
		}

#if defined(B394)
	/* B394: forward EVENT from B394_DI to IHUB as D2C so input events are sent to cloud */
	if (strcmp((char*)s_Message_Rx->src_Actor_a8, "B394_DI") == 0)
	{
		in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON != NULL)
		{

			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (cJSON_IsString(commandKey) && strcmp(commandKey->valuestring, "EVENT") == 0 && responseKey != NULL)
			{

				cJSON *wrapper = cJSON_CreateObject();
				if (wrapper != NULL)
				{

					cJSON_AddItemToObject(wrapper, "PAYLOAD", cJSON_Duplicate(responseKey, 1));
					memset(payLoadData, 0, sizeof(payLoadData));
					if (cJSON_PrintPreallocated(wrapper, payLoadData, sizeof(payLoadData), false))
					{

						if (s_Para.Enable_IHUB_u8 == Enable)
							Send_CMD_To_Other_Actor(IHUB, "IHUB", payLoadData, (int16_t)strlen(payLoadData), "D2C_MESSAGE");
					}
					cJSON_Delete(wrapper);
				}
			}
			cJSON_Delete(in_JSON);
		}
		return;
	}
#endif

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"WIFI")==0)
		{
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				return;
			}
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
			{
				cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
				if (responseKey != NULL && cJSON_IsString(responseKey->child))
				{
					cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "STA_IP_ADDR");
					if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					{
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(Device_Information_Que != NULL)
							xQueueSend(Device_Information_Que, payLoadData, QUE_DELAY);
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"ETH")==0)
			{
				in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (in_JSON == NULL) {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
					return;
				}
				cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
				if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
				{
					cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
					if (responseKey != NULL && cJSON_IsString(responseKey->child))
					{
						cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "IP");
						if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
						{
							memset(payLoadData,0,sizeof(payLoadData));//\0';
							cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
							if(Device_Information_Que != NULL)
								xQueueSend(Device_Information_Que, payLoadData, QUE_DELAY);
						}
					}
				}
				cJSON_Delete(in_JSON);
				return;
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
						if(!strcasecmp(currentItem->valuestring, "System/CONSOL.json"))
						{
							currentItem = currentItem->next;
							while (currentItem != NULL)
							{
								if (cJSON_IsString(currentItem))   // Check the type of the value
								{
									if((strcmp(currentItem->string, "JFS_DBG") !=0)  && (strcmp(currentItem->string, "U0_DBG") !=0))
									{
										set(currentItem->string, currentItem->valuestring,s_Message_Rx);
									}
								}
								else if (cJSON_IsNumber(currentItem))
								{
									if((strcmp(currentItem->string, "JFS_DBG") !=0)  && (strcmp(currentItem->string, "U0_DBG") !=0))
									{
										sprintf(keyValue, "%d", currentItem->valueint);
										set(currentItem->string, keyValue,s_Message_Rx);
									}
								}
								currentItem = currentItem->next;    // Move to the next key-value pair
							}
						}
						else
						{
							char val[100];
							memset(val,0, sizeof(val));
							getAll(NULL,val,NULL);
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
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "FIRMWARE");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Device_Information_Que != NULL)
						xQueueSend(Device_Information_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"EVENT_ACTOR")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DST");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));//\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Device_Information_Que != NULL)
						xQueueSend(Device_Information_Que, payLoadData, QUE_DELAY);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
    return;
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
	xQueueSend(msg_Rx_Queue, &s_Message_Tx_new, QUE_DELAY); //5,1 , 10
} //	COP_add_response_to_COP_Tx_Queue

static uint64_t get_rtc_time(AMessage_st* s_Message_Rx)
{
	struct tm timeinfo;
    char strftime_buf[100] = {0};
    uint64_t mills;
    time_t now;
    time(&now);
	tzset();
	localtime_r(&now, &timeinfo);
	strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);

	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	mills = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);

	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddNumberToObject(responseObject, "EPOCH_UTC_MS", mills);

	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(responseObject);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	console_send_responce_to_console_xface(s_Message_Rx);
	return(mills);
}
static void set_rtc_time(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	uint64_t epochUTCmilliseconds;
	char str[100] = {0};
	struct timeval currentTime;
	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
	}
	else{
	name_JSON = cJSON_GetObjectItem(in_JSON, "EPOCH_UTC_MS");
	epochUTCmilliseconds = name_JSON->valuedouble;
	cJSON_Delete(in_JSON);
	currentTime.tv_sec = epochUTCmilliseconds /1000L;
	currentTime.tv_usec = (epochUTCmilliseconds%1000)*1000;
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddNumberToObject(responseObject, "EPOCH_UTC_MS", epochUTCmilliseconds);
	settimeofday(&currentTime, NULL);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(responseObject);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	console_send_responce_to_console_xface(s_Message_Rx);
	}
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

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	cJSON_Delete(responseObject);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void updateConnectionStatus(void) {

	if ((s_Para.WIFI_status == E_wifi_CONNECTED_LOW)  ||
		(s_Para.WIFI_status == E_wifi_CONNECTED_HIGH) ||
		(s_Para.ETH_status  == E_ETH_CONNECTED))
	{
//		deinitWifi_Flag = 0;

		s_Para.NET_status = E_NET_CONNECTED;
	}
	else if(s_Para.WIFI_status == E_wifi_CONNECTED_AP)
	{
		s_Para.NET_status = E_wifi_CONNECTED_AP;
	}
	else {
		s_Para.NET_status = E_NET_NOT_CONNECTED;
	}
}

static void Actor_list (AMessage_st* s_Message_Rx)
{
	int no_of_elements = sizeof(A_Queue) / sizeof(A_Queue[0]);
	cJSON *jsonArray = cJSON_CreateArray();
	for (int i = 0; i < no_of_elements; i++)
	{
		cJSON *actorObject = cJSON_CreateString(A_Queue[i].name);
		cJSON_AddItemToArray(jsonArray, actorObject);
	}
	// Create a JSON object and add the array to it
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Actor Names", jsonArray);

	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(jsonObject);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Device_Information (void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	memset(payLoadData_Device_Info,0,sizeof(payLoadData_Device_Info));
	char buffer[600] = {0}, FIRMWARE_VERSION[30] = {0}, str[100] = {0}, Device_ID[30] = {0},  HARDWARE_VERSION[30] = {0}, WIFI_DEVICE_IP_ADDRESS[30] = {0}, WIFI_DEVICE_status[20] = {0}, ETH_DEVICE_IP_ADDRESS[30] = {0}, ETH_DEVICE_status[20] = {0};
	char model[50] = {0}, Manufacturer[50] = {0}, BOOTLOADER_VERSION[30] = {0}, found = 0, Manufacturer_date[50]={0};
	char DST_VAL[10], GMT_VAL[10], SR_TIME_VAL[30], SS_TIME_VAL[30], LATITUDE_VAL[10], LONGITUDE_VAL[10];
	cJSON *name_JSON = NULL;
	int Reboot_HH_VAL = 0, Reboot_MM_VAL = 0;
	int Reboot_Flag_VAL = 0;

	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("FIRMWARE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("BOOTLOADER_VERSION"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("HARDWARE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("MANUFACTURER_NAME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("MODEL_NAME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("MANUFACTURER_DATE"));

	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Device_Info,0, sizeof(payLoadData_Device_Info));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Device_Info, sizeof(payLoadData_Device_Info), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData_Device_Info, strlen(payLoadData_Device_Info), "GET");

	my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DST"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("GMT"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SR_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("SS_TIME"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LATITUDE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("LONGITUDE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REBOOT_FLAG"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REBOOT_HH"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("REBOOT_MM"));

	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Device_Info,0, sizeof(payLoadData_Device_Info));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Device_Info, sizeof(payLoadData_Device_Info), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData_Device_Info, strlen(payLoadData_Device_Info), "GET");

	if(s_Para.Enable_WIFI_u8 == 1)
	{
		my_JSON1  	= cJSON_CreateArray();
		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("STA_IP_ADDR"));
		jsonObject = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
		memset(payLoadData_Device_Info,0, sizeof(payLoadData_Device_Info));
		cJSON_PrintPreallocated(jsonObject, payLoadData_Device_Info, sizeof(payLoadData_Device_Info), false);
		cJSON_Delete(jsonObject);
		Send_CMD_To_Other_Actor(WIFI,"WIFI", payLoadData_Device_Info, strlen(payLoadData_Device_Info), "GET");
	}

	if(s_Para.Enable_ETH_u8 == 1)
	{
		my_JSON1  	= cJSON_CreateArray();
		cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("IP"));
		jsonObject = cJSON_CreateObject();
		cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
		memset(payLoadData_Device_Info,0, sizeof(payLoadData_Device_Info));
		cJSON_PrintPreallocated(jsonObject, payLoadData_Device_Info, sizeof(payLoadData_Device_Info), false);
		cJSON_Delete(jsonObject);
		Send_CMD_To_Other_Actor(ETH,"ETH", payLoadData_Device_Info, strlen(payLoadData_Device_Info), "GET");
	}

	while(1)
	{
		if (pdTRUE == xQueueReceive(Device_Information_Que, (void*)buffer, portMAX_DELAY))
		{
			cJSON *In_JSON 	  = cJSON_Parse((char*)buffer);
			if (In_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_Device_Info);
				goto exit;
			}

			name_JSON 		= cJSON_GetObjectItem(In_JSON, "FIRMWARE");
			if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
			{
				strcpy(FIRMWARE_VERSION, name_JSON->valuestring );
				cJSON *Boot_ver = cJSON_GetObjectItem(In_JSON, "BOOTLOADER_VERSION");
				if((Boot_ver != NULL) && (cJSON_IsString(Boot_ver)))
					strcpy(BOOTLOADER_VERSION, Boot_ver->valuestring);
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "DEVICEID");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(Device_ID, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "HARDWARE");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(HARDWARE_VERSION, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "MANUFACTURER_NAME");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(Manufacturer, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "MODEL_NAME");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(model, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "MANUFACTURER_DATE");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(Manufacturer_date, name_JSON->valuestring );
				found++;
			}

			name_JSON 		= cJSON_GetObjectItem(In_JSON, "DST");
			if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
			{
					strcpy(DST_VAL, name_JSON->valuestring );
				name_JSON = cJSON_GetObjectItem(In_JSON, "GMT");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(GMT_VAL, name_JSON->valuestring);
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "SR_TIME");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(SR_TIME_VAL, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "SS_TIME");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(SS_TIME_VAL, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "LATITUDE");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(LATITUDE_VAL, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "LONGITUDE");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(LONGITUDE_VAL, name_JSON->valuestring );
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "REBOOT_FLAG");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					Reboot_Flag_VAL = atoi (name_JSON->valuestring);
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "REBOOT_HH");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					Reboot_HH_VAL = atoi (name_JSON->valuestring);
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "REBOOT_MM");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					Reboot_MM_VAL = atoi (name_JSON->valuestring);
				found++;
			}

			if(s_Para.Enable_WIFI_u8 == 1)
			{
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "STA_IP_ADDR");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
				{
					strcpy(WIFI_DEVICE_IP_ADDRESS, name_JSON->valuestring );
					if(s_Para.WIFI_status >= 2)
					{
						strcpy(WIFI_DEVICE_status, "Connected" );
					}
					else
					{
						strcpy(WIFI_DEVICE_status, "Disconnected" );
					}
					found++;
				}
			}

			if(s_Para.Enable_ETH_u8 == 1)
			{
				name_JSON 		= cJSON_GetObjectItem(In_JSON, "IP");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
				{
						strcpy(ETH_DEVICE_IP_ADDRESS, name_JSON->valuestring );
						if(s_Para.ETH_status == E_ETH_CONNECTED)
						{
							strcpy(ETH_DEVICE_status, "Connected" );
						}
						else
						{
							strcpy(ETH_DEVICE_status, "Disconnected" );
						}
					found++;
				}
			}
			cJSON_Delete(In_JSON);
		}

		if(found == 4)
			break;
		else if((s_Para.Enable_WIFI_u8 == 0) && (s_Para.Enable_ETH_u8 == 0))
		{
			if(found == 2)
				break;
		}
		else if((s_Para.Enable_WIFI_u8 == 0) || (s_Para.Enable_ETH_u8 == 0))
		{
			if(found == 3)
				break;
		}
	}

	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "DeviceID",(char*) Device_ID);
	cJSON_AddStringToObject(root, "Firmware_Version",(char*) FIRMWARE_VERSION);
	cJSON_AddStringToObject(root, "Bootloader_Version",(char*) BOOTLOADER_VERSION);
	cJSON_AddStringToObject(root, "Hardware_Version",(char*) HARDWARE_VERSION);
	cJSON_AddStringToObject(root, "Model_Name",(char*) model);
	cJSON_AddStringToObject(root, "Manufacturer_Name",(char*) Manufacturer);
	cJSON_AddStringToObject(root, "Manufacturer_Date",(char*) Manufacturer_date);

	cJSON_AddStringToObject(root, "DST",(char*) DST_VAL);
	cJSON_AddStringToObject(root, "GMT",(char*) GMT_VAL);
	cJSON_AddStringToObject(root, "SR_TIME",(char*) SR_TIME_VAL);
	cJSON_AddStringToObject(root, "SS_TIME",(char*) SS_TIME_VAL);
	cJSON_AddStringToObject(root, "LATITUDE",(char*) LATITUDE_VAL);
	cJSON_AddStringToObject(root, "LONGITUDE",(char*) LONGITUDE_VAL);
	cJSON_AddBoolToObject(root, "REBOOT_FLAG",Reboot_Flag_VAL);
	cJSON_AddNumberToObject(root, "REBOOT_HH", Reboot_HH_VAL);
	cJSON_AddNumberToObject(root, "REBOOT_MM",Reboot_MM_VAL);

	if(s_Para.Enable_WIFI_u8 == 1)
	{
		cJSON_AddStringToObject(root, "WIFI_Device_IP",(char*) WIFI_DEVICE_IP_ADDRESS);
		cJSON_AddStringToObject(root, "WIFI_STATUS",(char*) WIFI_DEVICE_status);
	}
	else
	{
		cJSON_AddStringToObject(root, "WIFI_Device_IP","");
		cJSON_AddStringToObject(root, "WIFI_STATUS","WIFI DISABLED");
	}
	if (s_Para.Enable_ETH_u8 == 1)
	{
		cJSON_AddStringToObject(root, "ETH_Device_IP",(char*) ETH_DEVICE_IP_ADDRESS);
		cJSON_AddStringToObject(root, "ETH_STATUS",(char*) ETH_DEVICE_status);
	}
	else
	{
		cJSON_AddStringToObject(root, "ETH_Device_IP","");
		cJSON_AddStringToObject(root, "ETH_STATUS","ETH DISABLED");
	}
	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	uint64_t mills = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
	cJSON_AddNumberToObject(root, "UTC_EPOCH",mills);
	memset(payLoadData_Device_Info,0, sizeof(payLoadData_Device_Info));
	cJSON_PrintPreallocated(root, payLoadData_Device_Info, sizeof(payLoadData_Device_Info), false);
	strcpy(((char*)s_Message_Rx->payload_p8),payLoadData_Device_Info);
	cJSON_Delete(root);
	console_send_responce_to_console_xface(s_Message_Rx);
exit:
	Device_Information_Handle = NULL;
	vTaskDelete(Device_Information_Handle);  // Delete the task
}

static void InitActors(void *pvParameters __attribute__((unused))) {
	char payload[200] = {0};
	char str[300] = {0};
	uint8_t Device_Fw_Ver[30];
	char CPU0_REST_REA[30] = {0};
	char CPU1_REST_REA[30] = {0};
	date_time_t  sdate_tim;
	uint64_t current_epos_msec;//mills;
	uint64_t Temp_msec_1;
	struct timeval currentTime;

	s_Para.Enable_WIFI_u8 = Enable;
	s_Para.Enable_BLE_u8 = Enable;
	s_Para.Enable_WebServer_u8 = Enable;
#if defined(B480) || defined(B543)|| defined(B542) || defined(B394)	|| defined(B527)
	s_Para.Enable_ETH_u8 = Enable;
#else
	s_Para.Enable_ETH_u8 = Disable;
#endif
	s_Para.Enable_IHUB_u8 = Enable;
	cJSON *responseObject = NULL;
	
	//Add message in the wifi and event logs
	//Get firmware version
	memset(Device_Fw_Ver, 0, sizeof(Device_Fw_Ver));
	const esp_app_desc_t *new_app_info = esp_app_get_description();
	int i = 0;
	for(i = 0; i<strlen(new_app_info->version); i++)
	{
	   if(new_app_info->version[i]=='_')
		   Device_Fw_Ver[i] = '.';
	   else
		   Device_Fw_Ver[i] = new_app_info->version[i];
	}
	Device_Fw_Ver[i] = '\0';

    #if defined(B480)
	if(Hardware_RevH_flag == true)
	{
		sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE X-SERIES *****",Device_Fw_Ver);
	}
	else
	{
		sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE REV E *****",Device_Fw_Ver);
	}
	#elif defined(B543)
	 sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE X-POE *****",Device_Fw_Ver);
	#elif defined(B542)
	 sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE Q-POE *****",Device_Fw_Ver);
	#elif defined(B394)
	 sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE AUTOMATION LINK *****",Device_Fw_Ver);
	#elif defined(B527)
	 sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE SMART OUTLET *****",Device_Fw_Ver);
	#else
	 sprintf(payload,"***** " DEVICE_COMPANY " FIRMWARE %s HARDWARE X-MINI *****",Device_Fw_Ver);
	#endif

	// Get CPU reset reasons
	Get_ESP_Reset_Reason(CPU0_REST_REA, CPU1_REST_REA);

	cJSON *root = cJSON_CreateObject();
	cJSON_AddStringToObject(root, "bootMessage", payload);
	cJSON_AddStringToObject(root, "CPU0ResetReason", CPU0_REST_REA);
	cJSON_AddStringToObject(root, "CPU1ResetReason", CPU1_REST_REA);
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(root, payload, sizeof(payload), false);
	cJSON_Delete(root);
	sprintf(str,">SYSTEM.EVENT(%s)\n", payload);
	printf("\r\n\r\n %s \r\n\r\n", str);
    // save boot message to the audit log
    _gettimeofday_r(NULL, &currentTime, NULL);
		current_epos_msec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		Temp_msec_1 = EPOSCH_TO_30_YEAR;
		Temp_msec_1 = Temp_msec_1*1000;
		current_epos_msec = current_epos_msec - Temp_msec_1;

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
	sprintf(str,"%02d/%02d/%02d %02d:%02d:%02d.%03d | >SYSTEM.EVENT(%s, CONSOLE)\n", UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, payload);
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str, strlen(str), "SAVE_AUDIT_LOG");
	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str, strlen(str), "SAVE_EVENT_LOG");
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str, strlen(str), "SAVE_WIFI_AUDIT_LOG");

	responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/CONSOL.json");
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(responseObject, payload, sizeof(payload), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payload, strlen(payload), "READ");

	responseObject = cJSON_CreateObject();    // delete pushbutton.json file **This is done to solve continuous reboot issue in 701.15
	cJSON_AddStringToObject(responseObject, "FILE_NAME","System/PUSHBUTTON.json");
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(responseObject, payload, sizeof(payload), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(SYS_FILES,"SYS_FILES", payload, strlen(payload), "DELETE");

	responseObject = cJSON_CreateObject();     // delete firmware.bin file **This is done to solve reboot issue in SMTP after updating the firmware.
	cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/firmware.bin");
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(responseObject, payload, sizeof(payload), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payload, strlen(payload), "DEL_FILE");

	responseObject = cJSON_CreateObject();     // delete Temp/Audit directory **This is done to solve JFS memory 0MB issue. It occurs if device reboots in the middle of SMTP and copied file is not deleted
	cJSON_AddStringToObject(responseObject, "DIR_NAME","A:/Temp/Audit");
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(responseObject, payload, sizeof(payload), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payload, strlen(payload), "DEL_DIR");

	responseObject = cJSON_CreateObject();     // delete Temp/Database directory **This is done to solve JFS memory 0MB issue. It occurs if device reboots in the middle of SMTP and copied file is not deleted
	cJSON_AddStringToObject(responseObject, "DIR_NAME","A:/Temp/Database");
	memset(payload,0,sizeof(payload));
	cJSON_PrintPreallocated(responseObject, payload, sizeof(payload), false);
	cJSON_Delete(responseObject);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payload, strlen(payload), "DEL_DIR");


	SPIFFS_StartupGateReset();
	Send_CMD_To_Other_Actor(SPIFFS, "SPIFFS", "\0", 0, "INIT");
	if (SPIFFS_WaitForStartupReady(portMAX_DELAY) != pdTRUE)
	{
		printf(">SPIFFS.INIT(Startup gate wait failed; continuing connection flow.)\n");
	}
	s_Para.JFS_dbg_u8 		= 1;
	s_Para.U0_dbg_u8 		= 0;

	if(s_Para.Enable_WIFI_u8 == Enable)
	{
		Send_CMD_To_Other_Actor(WIFI, "WIFI", "\0", 0, "INIT");
	}

	if(s_Para.Enable_ETH_u8 == Enable){
		Send_CMD_To_Other_Actor(ETH, "ETH", "\0", 0, "INIT");
	}
	Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", "\0", 0, "INIT"); //Speed-up
	Send_CMD_To_Other_Actor(LED, "LED", "\0", 0, "INIT");
	Send_CMD_To_Other_Actor(PUSHBUTTON, "PUSHBUTTON", "\0", 0, "INIT");
	Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "INIT");
	Send_CMD_To_Other_Actor(NTP, "NTP", "\0", 0, "INIT");
	vTaskDelay(15000 / portTICK_PERIOD_MS);   //This delay is given to initialize light actor after Ihub connection (to avoid light flickering)
	Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "INIT");
	Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "UPDATE_SCH_TABLE");
	Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "UPDATE_LOC");
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "INIT");  //Speed-up
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    //printf("Init READ_VIRTUAL_TABLE \n");
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "READ_VIRTUAL_TABLE");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
   // printf("Init READ_COMMAND_TABLE \n");
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "READ_COMMAND_TABLE");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
   // printf("Init READ_PLAYLIST_TABLE \n");
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "READ_PLAYLIST_TABLE");//	Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", "\0", 0, "READ_PLAYLIST_TABLE");  //Speed-up
	
	InitActors_Handle = NULL;
	vTaskDelete(InitActors_Handle);  // Delete the task
}

static void epoch_to_date_time_m(date_time_t* date_time, uint64_t epoch)
{
   date_time->milSec = epoch%1000; epoch /= 1000;
   date_time->second = epoch%60; epoch /= 60;
   date_time->minute = epoch%60; epoch /= 60;
   date_time->hour   = epoch%24; epoch /= 24;
   unsigned long years = epoch/(365*4+1)*4; epoch %= 365*4+1;
   unsigned long year;
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

int16_t get_gmt(void)
{
	return gmt_val;
}

uint8_t get_dst(void)
{
	return dst_val;
}

uint64_t get_current_rtc_time_ms(uint8_t setval) {
    static struct timeval ref_tv       = {0};    // saved epoch time at last sync
    static uint64_t       ref_timer_us = 0;      // esp_timer timestamp (�s) when ref_tv was captured

    struct timeval tv;
    uint64_t rtc_time_ms;

    if (setval) {
        // >>>>> SEED from whichever RTC is present <<<<<
        if (Hardware_RevH_flag) {
            // 1) Read external PCF8563 (gives tm + centiseconds)
            struct tm    t           = {0};
            uint8_t      centisec;
            i2c_dev_t   *rtcHandle   = pcf8563_get_device_handle();
            pcf8563_get_time_ms(rtcHandle, &t, &centisec);

            // 2) Adjust for mktime()
            t.tm_year -= 1900;
            time_t secs = mktime(&t);

            // 3) Build a timeval from sec + centisecond
            ref_tv.tv_sec  = secs;
            ref_tv.tv_usec = (uint32_t)centisec * 10000;  // 1 cs = 10 ms = 10 000 �s
        } else {
            // SEED from internal RTC via gettimeofday()
            gettimeofday(&ref_tv, NULL);
        }

        // 4) Capture high-res tick at the moment of seeding
        ref_timer_us = esp_timer_get_time();

        // Prime tv so we return exactly the seed time on this call
        tv = ref_tv;
    }
    else {
        // >>>>> ADVANCE by elapsed �s since last seed <<<<<
        uint64_t now_us   = esp_timer_get_time();
        uint64_t delta_us = now_us - ref_timer_us;

        // Add whole seconds + leftover �s to ref_tv
        tv.tv_sec  = ref_tv.tv_sec  + (delta_us / 1000000ULL);
        tv.tv_usec = ref_tv.tv_usec + (delta_us % 1000000ULL);

        // Normalize if �s overflowed past 1 000 000
        if (tv.tv_usec >= 1000000L) {
            tv.tv_sec  += 1;
            tv.tv_usec -= 1000000L;
        }
    }

    // >>>>> Convert to a 64-bit millisecond count <<<<<
    rtc_time_ms = (uint64_t)tv.tv_sec * 1000ULL
                + (tv.tv_usec  / 1000ULL);

    return rtc_time_ms;
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
