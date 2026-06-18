/*
 * File_Manager_Actor.c
 *
 *  Created on: 02-Aug-2023
 *      Author: Acer
 */


#include "actor.h"
#include "Config.h"
#include "File_Manager_Actor.h"
#include "Console_Actor.h"
#include "SD_Card_Actor.h"
#include "JFS_FLASH_Actor.h"
#include "esp_mac.h"
#include "esp_sntp.h"
#include "FS.h"
#define MAX_DATA_SIZE 		2048	//1024

PSRAM_ATTR_BSS static char payLoadData_Copy_ALL_JFS_SD[1024];
static char * THIS_ACTOR = "FILE_SYSTEM";
static  char device_ID[32] ;
char Write_variable_FirstTime;
int Write_count=0;
BaseType_t FileMonitor, READ_DATA_Method, Task_Read, Task_Read_SD, Task_audit_log_test;
TaskHandle_t FileHandle = NULL, Read_Handle = NULL, Read_Handle_SD= NULL,CpyAllFilesHandle=NULL, Audit_Test_Handle = NULL;
QueueHandle_t File_Rx_Queue,CpyAllFiles_Que=NULL;; // File_Tx_Queue;
static StaticQueue_t CpyAllFiles_QueBuffer;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [FILE_MANAGER_RX_OBJ_QUE_COUNT * sizeof(AMessage_st)], CpyAllFiles_QueStorage[FMA_CPYALLFILES_QUE_COUNT * FMA_CPYALLFILES_QUEUE_ITEMSIZE];
PSRAM_ATTR_BSS uint8_t SMTP_READ_pucQueueStorage[FILE_SMTP_READ_TASK_QUEUE_LENGTH * FILE_SMTP_READ_TASK_QUEUE_ITEMSIZE];
static StaticQueue_t  Monitor_pxQueueBuffer;
PSRAM_ATTR_BSS static StackType_t xSMTP_Read_TaskStack [FMA_JFSSMTP_READ_TASK_STACK_DEPTH], xSMTP_SD_Read_TaskStack[FMA_SDSMTP_READ_TASK_STACK_DEPTH],  CpyAllFilesTaskStack [FMA_CPY_ALLFILES_TASK_STACK_DEPTH];
static StaticTask_t xSMTP_ReadTaskBuffer, xSMTP_SD_ReadTaskBuffer, xAuditLogTestTaskBuffer;
PSRAM_ATTR_BSS static AMessage_st s_Message_Rx_SMTP;
static StaticTask_t xCpyAllFilesTaskBuffer;  //// Declare a static task control block  xFILE_SYSTEMTaskBuffer
static int FirstEntry_bool = 0;

QueueHandle_t 	SMTP_Read_Queue 		= NULL;
StaticQueue_t SMTP_READ_pxQueueBuffer;
uint8_t *SMTP_payload = NULL;
static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
//static void Send_RecordLogData_Server(AMessage_st* s_Message_Rx);
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void write_audit_log_test_task(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void File_operation(AMessage_st* s_Message_Rx);
static void epoch_to_date_time_m(date_time_t* date_time,uint64_t epoch);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Copy_File_From_JFS_to_SD(AMessage_st* s_Message_Rx);
static void Calculate_CRC16_File_From_JFS(char *payload,AMessage_st* s_Message_Rx);
static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch);
static void CpyAllFiles_JFS_To_SD(void *pvParameters __attribute__((unused)));
void Release_bus(void);
void ExtractFilesFromJson(cJSON* json, const char* parentPath);
static void cleanup_CpyAllFiles_resources();
static void sendCpyStatus(char * filedata);
static void sendLedState(const char *stateName, int duration);
static void set_gmt_dst(AMessage_st* s_Message_Rx);
#define CarriageReturnValue  			0x0D
#define LineFeedValue		 			0x0A
#define Filename_log				    "System/Record.json"
#define Filename_Archive			    "System/Archive0.json"
#define Filename_Archive_base		 	"Archive"	//"System/Archive"
#define Filename_Scratch_Pad			"System/Scratch.json"
#define Filename_Archive_base_d		 	"System/Archive"
#define Filename_Scrt_Pad_Arc			"System/ScratchA0.json"
#define Filename_Scrt_Pad_Arc_base		"ScratchA"	//"System/Archive"
#define HTTP_endpoint_URL				"http://ase-hvnlght-customers-web-dev.azurewebsites.net/api/Device/Records/Open" //"http://cardinal-api.azurewebsites.net/api/Device/CreateDebug" //"http://havenwebservices-apiapp-test.azurewebsites.net/api/v2/Device/GetKeyV2/240AC4D62284"
#define LogFile_Size_Max 			    2048	//102400
#define LogFile_Size_Min 			    1024
#define LogFile_Count_Max 			    100
#define LogFile_Count_Min 			    1
#define ScratchFile_Count 			    4		// for 100 files it is sufficient and for file size and  file count
uint8_t  HTTP_endpoint_u8_Value[150];
static char deviceID[30]={0};
static char fma_deviceID[32]={0};
char Fetch_Record_log_flag = 0;
uint16_t msg_id_u16 = 0;
uint32_t Audit_count = 0;
static int16_t	gmt_val = 0;
static uint8_t	dst_val = 0;
//--------------For Record log data------------------

PSRAM_ATTR_BSS static struct File_parameter {
	uint16_t MaxLogID_u16;	//Max_Log_ID_in_File_u16 = 100;
	uint16_t Max_File_Count_u16;	// Max_File_Count_u16 = 100;
	uint16_t LogFileStr_Size_Conf_u16; //LogFileStr_Size_Conf_u16 = 10000;
	uint32_t Max_Log_File_size_u32;	//100 kByte	//Max_Log_File_size_u32 = 100*1024;	//100 kByte
	int32_t FileSize_u32;
	uint8_t  LogFileName_a8	[50];
	uint32_t Log_ID_Start_a8;	//Log_ID_Start_a8[8] = 0;
	uint32_t Log_ID_End_a8;	//Log_ID_End_a8[8] = 0;
	uint32_t Log_ID_Archive_a8;	//Log_ID_End_a8[8] = 0;
	char     FileStart_a8[50];	//FileStart_a8[50] = {"Record1"};
	char     FileMid_a8[50];	//FileStart_a8[50] = {"Record1"};
	char     FileEnd_a8[50];	//FileEnd_a8[50] = {"Record1"};
	char     FileScratch_a8[50];	//FileEnd_a8[50] = {"Record1"};
	uint8_t 	 postToHTTP ;
//	uint8_t  HTTP_endpoint_u8[150];
	uint8_t  Record_sync_DBname_u8[50];
	uint8_t 	 postAsD2C;
	uint8_t  D2CmessageHeader[20];
	uint8_t Mode_u8[10];
	uint8_t Fota_Status_u8;
	uint32_t numberWrites;
	uint32_t writeDelay;
} s_Para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &s_Para.MaxLogID_u16            , "MAXLOGID"           , U_INT16, "R",  "Maximum log ID" },
    { &s_Para.Max_File_Count_u16      , "MAX_FILE_COUNT"     , U_INT16, "RW",  "Maximum file count" },
    { &s_Para.LogFileStr_Size_Conf_u16, "LOGFILESTR_SIZE_CONF", U_INT16,"R",  "Size of log file string configuration" },
    { &s_Para.Max_Log_File_size_u32   , "MAX_LOG_FILE_SIZE"  , U_INT32, "RW",  "Maximum log file size" },
    { &s_Para.FileSize_u32            , "FILE_SIZE"          , INT, "R",  "Size of the file" },
    { &s_Para.LogFileName_a8          , "FILENAME"          , STRING , "RW",  "Name of the log file" },
    { &s_Para.Log_ID_Start_a8         , "LOG_ID_START"       , U_INT32, "R",  "Start log ID" },
    { &s_Para.Log_ID_End_a8           , "LOG_ID_END"         , U_INT32, "R",  "End log ID" },
    { &s_Para.Log_ID_Archive_a8       , "LOG_ID_ARCHIVE"     , U_INT32, "R",  "Archived log ID" },
    { &s_Para.FileStart_a8            , "FILESTART"          , STRING , "R",  "Start of the file" },
    { &s_Para.FileMid_a8              , "FILEMID"            , STRING , "R",  "Middle of the file" },
    { &s_Para.FileEnd_a8              , "FILEEND"            , STRING , "R",  "End of the file" },
    { &s_Para.FileScratch_a8          , "FILESCRATCH"        , STRING , "R",  "Scratch file" },
    { &s_Para.postToHTTP              , "POST_TO_HTTP"       , U_INT8 , "RW",  "Post to HTTP" },
    { &s_Para.Record_sync_DBname_u8   , "RECORD_SYNC_DBNAME" , STRING , "RW",  "Record sync database name" },
    { &s_Para.postAsD2C               , "POST_AS_D2C"       , U_INT8 ,  "RW",  "Post as D2C" },
    { &s_Para.D2CmessageHeader        , "D2C_MESSAGE_HEADER", U_INT8 ,  "RW",  "D2C message header" },
    { &s_Para.Mode_u8         		  , "MODE"				, STRING ,  "RW",  "WRITE/APPEND mode of file operation" },
	{ &s_Para.Fota_Status_u8          , "FOTA_STATUS"		, U_INT8 ,  "R",  "Status of FOTA not initialized or FOTA in progress." },
	{ &s_Para.numberWrites            , "NUMBERWRITES"      , U_INT32, "RW",  "Number of writes for Audit log test" },
	{ &s_Para.writeDelay              , "WRITEDELAY"        , U_INT32, "RW",  "Delay for writing Audit log test" }
};

static const unsigned short days[4][12] =
{
   {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
   { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
   { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
   {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

//PSRAM_ATTR_BSS static uint8_t memory_CpyAllFilesTaskStack[FMA_CPY_ALLFILES_TASK_STACK_DEPTH * sizeof(StackType_t)];
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Write_buffer[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char payLoadData_CPY_FILE[1024];
PSRAM_ATTR_BSS  char payLoadData_Chunk_Read[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  char payLoadData_Event[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  char payLoadData_Write[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS char payLoadData_Var[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  char payLoadData_WIFI[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static  char CpyAllFilesdata_buffer[4500];
PSRAM_ATTR_BSS  char JFS_READ_data_buffer[4096];
PSRAM_ATTR_BSS static char SD_READ_data_buffer[4096];
PSRAM_ATTR_BSS static char datau8[MAX_DATA_SIZE];
PSRAM_ATTR_BSS static char Read_bufferCPY_JFS_SD[4500];
PSRAM_ATTR_BSS static uint8_t Read_buffer_CRC[1024];
PSRAM_ATTR_BSS static char Audit_log_test_buffer[2048];
PSRAM_ATTR_BSS static char Audit_log_1_buffer[100];
PSRAM_ATTR_BSS static StackType_t  xTaskStack5[FILE_MANAGER_AUDIT_TEST_TASK_STACK_DEPTH];
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
//	AMessage_st s_Message_Tx_new;
	uint8_t parameter_found = 0; // Flag to check if actor is found
//	char str[100] ={0};
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	if(!strcmp(property, "MAX_FILE_COUNT"))
		{
//		update_file_count(value, s_Message_Rx);
		return 1;
	}
	if(!strcmp(property, "MAX_LOG_FILE_SIZE"))
	{
//		update_file_size(value);
		return 1;
	}
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
			if(!(strcmp(prop[i].str_name, "FOTA_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case FOTA_NOT_INITIALIZED:
					strcpy(val_a8, "FOTA NOT INITIALIZED");
					break;

				case FOTA_IN_PROGRESS:
					strcpy(val_a8, "FOTA IN PROGRESS");
					break;

				default:
					break;
				}
			}
		}
	}
}//	get



static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	console_send_responce_to_console_xface(s_Message_Rx);
	cJSON_Delete(responseObject);
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  = (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  //strlen((char*)out_val + 1)
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

static void init(void *a, void *b) {
	int16_t ret_s16;
	uint8_t mac[6];
	if (FirstEntry_bool == 0)
	{
#ifndef B527
		ret_s16 = init_SD_Card(0,0);
		if(ret_s16 == -2)
		{
			//Add_Response_msg("In SD card, failed to initialize SPI bus.",&s_Message_Rx_New);
		}
		else
		{
			if(ret_s16 == ESP_FAIL)
			{
			//	Add_Response_msg("Failed to mount filesystem in the SD card.",&s_Message_Rx_New);
			}
			else
				{
					if(ret_s16 != ESP_OK)
					{
						//printf("Failed to initialize the card (%s). Make sure SD card lines have pull-up resistors in place.", esp_err_to_name(ret_s16));
					}
				}
		}
#endif
	   	if (File_Rx_Queue == NULL)
	   		File_Rx_Queue = xQueueCreateStatic(FILE_MANAGER_RX_OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
		if (File_Rx_Queue == NULL) {
			printf(">FILE_SYSTEM.ERROR(File RX Queue is not created.)\n");
		}
		s_Para.Fota_Status_u8  = FOTA_NOT_INITIALIZED;
		FileMonitor = xTaskCreatePinnedToCore(monitor, (const char*) "File Monitor", FILE_MANAGER_TASK_STACK_DEPTH, NULL, FILE_MANAGER_TASK_PRIORITY, &FileHandle,1);
//--------------For Record log data------------------

		if(FileHandle == NULL)
			printf(">FILE_SYSTEM.ERROR(File monitor task is not created.)\n");
		s_Para.numberWrites	= 0;
		s_Para.writeDelay   = 0;
		esp_read_mac(mac, ESP_MAC_WIFI_STA);

	   // Print the WIFI address
	    sprintf(device_ID, "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		strcpy((char*)&s_Para.Record_sync_DBname_u8, "Record");
//--------------For Record log data------------------
	    FirstEntry_bool = 1;
#ifdef ENABLE_PRINT_MSG
		printf("\n File manager actor is initialized. \n");
#endif
	}
}


static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0}, path[100] = {0};
	char  found = 0;
	uint8_t u8Result =0;
	int i =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(File_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
			memcpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;

//			printf("\n file manager msg_Rx_Queue S = %s, D = %s, C = %s, size = %d \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8, s_Message_Rx->payload_size);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("File DT = %s\n",s_Message_Rx->payload_p8);
//			}
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties

				if (FirstEntry_bool == 0)
					init(0, 0);
				if(FirstEntry_bool)
				{
					Add_Response_msg("File manager actor is initialized.",s_Message_Rx, payLoadData);
				}

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
				{
					Get_Property(s_Message_Rx);
				}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				u8Result =0;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				    }
				else{
				head_JSON = name_JSON->child;
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
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			{
//				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//				getAll((char*)prop,(char*) val_p8,s_Message_Rx);
//				free(val_p8);
//			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "READ"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "WRITE"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DEL_FILE"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "CREATE_DIR"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DEL_DIR"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RENAME"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "WRITE_VAR"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "READ_VAR"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET_FILE_SIZE"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET_FILE_LIST"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SAVE_AUDIT_LOG"))
			{
				Save_Audit_log(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "AUDITLOGTEST"))
			{
				cJSON *root = cJSON_Parse((char*)s_Message_Rx->payload_p8);
				if (root == NULL)
					return;

				cJSON *numWrites = cJSON_GetObjectItem(root, "numberWrites");
				cJSON *delay     = cJSON_GetObjectItem(root, "writeDelay");

				s_Para.numberWrites = (numWrites && cJSON_IsNumber(numWrites)) ? numWrites->valueint : 0;
				s_Para.writeDelay   = (delay && cJSON_IsNumber(delay)) ? delay->valueint : 0;

				cJSON_Delete(root);

				if(s_Para.numberWrites != 0)
				{

					if(s_Para.writeDelay == 0)
					{
						s_Para.writeDelay = 20;
					}

					if(Audit_Test_Handle == NULL)
					{
						Audit_count = 0;
						AMessage_st s_Message_Rx_data;
						memset(Audit_log_test_buffer,0,sizeof(Audit_log_test_buffer));
						s_Message_Rx_data.payload_p8 = (uint8_t*)Audit_log_test_buffer;
						strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
						strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
						strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);

						Audit_Test_Handle = xTaskCreateStaticPinnedToCore(write_audit_log_test_task, (const char*) "write_audit_log_test_task", FILE_MANAGER_AUDIT_TEST_TASK_STACK_DEPTH, &s_Message_Rx_data, FILE_MANAGER_AUDIT_TEST_TASK_PRIORITY, xTaskStack5, &xAuditLogTestTaskBuffer,1);

						if(Audit_Test_Handle == NULL)
							printf(">FILE_SYSTEM.ERROR(Test Audit Log task is not created.)\n");
						else
						{
							Add_Response_msg("Audit Log testing started", s_Message_Rx, payLoadData);
						}

					}
					else
					{
						printf(">FILE_SYSTEM.ERROR(Test Audit Log task already created.)\n");
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SAVE_EVENT_LOG"))
			{
				Save_Event_log(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "WRITE_FILE"))
			{
				JFS_FLASH_Write_File((char*)s_Para.LogFileName_a8, (char *)s_Message_Rx->payload_p8,s_Message_Rx->payload_size, (char*)s_Para.Mode_u8,0, payLoadData_Write); //B331_5.bin, Write to the flash memory
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "WRITE_FILE1"))
			{
				typedef struct {
				    char FileName[100];
				    char *data;
				    int16_t size;
				    char OpenType[10];
				    uint32_t Offset;
				    int64_t file_size_s64;
				} WriteRequest_t;
				WriteRequest_t request;
				FileDownload_t *fFileDownloadPtr=NULL;
			  fFileDownloadPtr=(FileDownload_t *)s_Message_Rx->payload_p8;
			  strcpy((char*)path,(char*)fFileDownloadPtr->file_name);
				for(i =0; i< strlen(path); i++)
				{
					if(path[i] == '/')
					{
						found = 1;
						break;
					}
				}
				  if(found == 1)
				  {
					strcpy(request.FileName, path+i+1);

				  }
				request.size = s_Message_Rx->payload_size-sizeof(FileDownload_t);
				strcpy(request.OpenType, (char*)fFileDownloadPtr->mode);

				request.Offset = 0;
				request.data = fFileDownloadPtr->buffer;
				request.file_size_s64 = fFileDownloadPtr->file_size;
				memset(Write_buffer, 0, sizeof(Write_buffer));
				memcpy(Write_buffer, fFileDownloadPtr->buffer, request.size);

				char binary_file = 0;
				const char *dot = strrchr((char*)request.FileName, '.');
				// Check if the dot is present and if it is followed by "bin"
				if (dot != NULL && ((strcmp(dot + 1, "bin") == 0)))
				{
					binary_file = 1;
				}
				if((request.OpenType[0] == 'w') && (binary_file == 1))   // check .bin is present or not. If present delete it.
				{
					FS_Remove("Root/Audit/Audit_Log_Old.txt");  // delete audit_log_old.txt file to free the JFS memory
				}

				if(binary_file == 1)   // write .bin file to OTA partition
				{
					Write_Bin_File_OTA(Write_buffer, request.size, request.file_size_s64, s_Message_Rx);
				}
				else
				{
					int ret = JFS_FLASH_Write_File_HP((char*)request.FileName, Write_buffer, request.size, (char*)request.OpenType,1); //B331_5.bin, Write to the flash memory
					if(ret != 0)
						Add_Response_msg("Error! Failed to perform write operation.",s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "ESP_FW_UPDATE"))
			{
				s_Para.Fota_Status_u8 = FOTA_IN_PROGRESS;
				Firmware_Update(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "JFS_FORMAT"))
			{
				Send_CMD_To_Other_Actor(SYS_FILES, "SYS_FILES","\0",0,"FORMAT");
				unmount_JFS();
				vTaskDelay(1000/ portTICK_PERIOD_MS);
				int retry =0;
				int ret = -1;
				while(retry++ < 3)
				{
					ret = FS_Format(VOLUME_NAME, NULL);
					if (ret != 0)
					{
						Add_Response_msg("Failed to format JFS", s_Message_Rx, payLoadData);
						vTaskDelay(100/ portTICK_PERIOD_MS);
					}
					else
						break;
				}
				Add_Response_msg("JFS and SYS_FILES are formatted. Restarting the ESP... ",s_Message_Rx, payLoadData);
				Restart_ESP_Xface(1);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "JFS_FORMAT_ONLY"))
			{
				unmount_JFS();
				vTaskDelay(1000/ portTICK_PERIOD_MS);
				int retry =0;
				int ret = -1;
				while(retry++ < 3)
				{
					ret = FS_Format(VOLUME_NAME, NULL);
					if (ret != 0)
					{
						Add_Response_msg("Failed to format JFS", s_Message_Rx, payLoadData);
						vTaskDelay(100/ portTICK_PERIOD_MS);
					}
					else
						break;
				}
				Add_Response_msg("Only JFS is formatted. Restarting the ESP... ",s_Message_Rx, payLoadData);
				Restart_ESP_Xface(1);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SAVE_WIFI_AUDIT_LOG"))
			{
				Save_WIFI_Audit_log(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SMTP_READ"))
			{
				File_operation(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET_VOLUME_INFO"))
			{
				JFS_SampleGetVolumeInfo(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "COPY_JFS_SD"))
			 {
				Copy_File_From_JFS_to_SD(s_Message_Rx);
			 }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "CPY_ALLJFS_TO_SD"))
			 {

				if(CpyAllFiles_Que == NULL)
					CpyAllFiles_Que = xQueueCreateStatic(FMA_CPYALLFILES_QUE_COUNT, FMA_CPYALLFILES_QUEUE_ITEMSIZE, CpyAllFiles_QueStorage, &CpyAllFiles_QueBuffer);
				if (CpyAllFiles_Que == NULL)
					Add_Response_msg("ERROR! CpyAllFiles_Que is not created",s_Message_Rx, payLoadData);
				if(CpyAllFilesHandle == NULL)
				{
					AMessage_st s_Message_Rx_data;
					memset(CpyAllFilesdata_buffer,0,sizeof(CpyAllFilesdata_buffer));
					if(strlen((char*) s_Message_Rx->payload_p8) != 0)
					{
						strcpy(CpyAllFilesdata_buffer, (char*) s_Message_Rx->payload_p8);
					}
					s_Message_Rx_data.payload_p8 = (uint8_t*)CpyAllFilesdata_buffer;
					strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
					strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
					strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);
					CpyAllFilesHandle = xTaskCreateStaticPinnedToCore(
							CpyAllFiles_JFS_To_SD,                 // Task function
							"CpyAllFiles_JFS_To_SD",            // Task name
							FMA_CPY_ALLFILES_TASK_STACK_DEPTH,        // Stack size in words
							&s_Message_Rx_data,                    // Task parameters (not used here)
							FMA_CPY_ALLFILES_TASK_PRIORITY,                       // Task priority
							CpyAllFilesTaskStack,              // Pointer to task stack (allocated in PSRAM)
							&xCpyAllFilesTaskBuffer,             // Pointer to task control block
							1
							);
					if (CpyAllFilesHandle == NULL) {
		#ifdef ENABLE_PRINT_MSG
							printf("Failed to create CpyAllFiles task\n");
		#endif
							Add_Response_msg("Failed to create CpyAllFiles task", s_Message_Rx, payLoadData);
							continue;
						}
				}
				else
				{
					Add_Response_msg("Task for CpyAllFiles command is already created.", s_Message_Rx, payLoadData);
				}
			 }
			else if(!strcmp((char*)s_Message_Rx->cmdFun_a8 , "GET_FILE_CRC"))
			{
				File_operation(s_Message_Rx);
			}
			else if(!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UNMOUNT"))
			{
				unmount_JFS();
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "FILE_SYSTEM_GMT_DST"))
			{
				set_gmt_dst(s_Message_Rx);
			}
			else
			{
				//File manager error message: invalid method
				Add_Response_msg("Invalid method", s_Message_Rx, payLoadData);
			}
		  }
	}
}

void File_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstEntry_bool == 0)
	init(0,0);
	uint8_t state = xQueueSend(File_Rx_Queue, s_Message, QUE_DELAY); //1
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<FILE_SYSTEM.ERROR(FILE_SYSTEM RX Queue is full)\n");
		}
		else
		{
			printf("<FILE_SYSTEM.ERROR(FILE_SYSTEM RX Queue send unsuccessful)\n");
		}
	}

}//	File_ConsolWriteToActor

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
			if(!(strcmp(prop[i].str_name, "FOTA_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case FOTA_NOT_INITIALIZED:
					strcpy(val_a8, "FOTA NOT INITIALIZED");
					break;

				case FOTA_IN_PROGRESS:
					strcpy(val_a8, "FOTA IN PROGRESS");
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

		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the FILE_SYSTEM actor.");
	cJSON_AddStringToObject(responseObject, "SET(string FILENAME)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "READ(string FILE_NAME)", "Read file from JFS or SD card.");
	cJSON_AddStringToObject(responseObject, "WRITE(string FILE_NAME, string FILE_DATA)", "Save file to JFS or SD card.");
	cJSON_AddStringToObject(responseObject, "DEL_FILE(string FILE_NAME)", "Delete the file.");
	cJSON_AddStringToObject(responseObject, "CREATE_DIR(string DIR_NAME)", "Create the directory.");
	cJSON_AddStringToObject(responseObject, "DEL_DIR(string DIR_NAME)", "Delete the directory.");
	cJSON_AddStringToObject(responseObject, "RENAME(string TYPE, string OLD_NAME, string NEW_NAME)", "Rename the file or directory.");
	cJSON_AddStringToObject(responseObject, "WRITE_VAR(string FILE_NAME, string VARIABLE_NAME)", "Store variable/variables in the JFS file.");
	cJSON_AddStringToObject(responseObject, "READ_VAR(string FILE_NAME,string VARIABLE_NAME)", "Read variable/variables from the JFS file.");
	cJSON_AddStringToObject(responseObject, "GET_FILE_LIST(string DIR_NAME)", "Display the list of files present in the Audit folder of the JFS.");
	cJSON_AddStringToObject(responseObject, "ESP_FW_UPDATE(string FILE_NAME)", "It uses .bin file present in the JFS to upgrade the ESP firmware.");
	cJSON_AddStringToObject(responseObject, "GET_FILE_SIZE(string FILE_NAME)", "Read the file size");
	cJSON_AddStringToObject(responseObject, "AUDITLOGTEST(int numberWrites, int writeDelay)", "Audit Log Test");
	cJSON_AddStringToObject(responseObject, "JFS_FORMAT()", "It formats the JFS and SPIFFS volume 2.");
	cJSON_AddStringToObject(responseObject, "JFS_FORMAT_ONLY()", "It formats the JFS only.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "GET_VOLUME_INFO()", "Display the details of JFS like total number of clusters, free clusters, JFS memory available, etc.");
	cJSON_AddStringToObject(responseObject, "COPY_JFS_SD(string JFS_FILE_NAME, string SD_FILE_NAME)", "Read the file size");
	cJSON_AddStringToObject(responseObject, "GET_FILE_CRC(string FILE_NAME)", "Get the CRC of file");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

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

static void write_audit_log_test_task(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx_data;
	memset(Audit_log_test_buffer,0,sizeof(Audit_log_test_buffer));
	s_Message_Rx_data.payload_p8 = (uint8_t*)Audit_log_test_buffer;
	strcpy((char*)s_Message_Rx_data.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
	strcpy((char*)s_Message_Rx_data.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
	strcpy((char*)s_Message_Rx_data.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);

	date_time_t  sdate_tim;
	uint64_t current_epos_msec;//mills;
	uint64_t Temp_msec_1;
	struct timeval currentTime;
	char Audit_log_text_teting[500];

	while (1)
	{
		Audit_count++;

	    memset(Audit_log_text_teting,0,sizeof(Audit_log_text_teting));
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

		sprintf(Audit_log_text_teting,"%02d/%02d/%02d %02d:%02d:%02d.%03d | >({\"%s\": %ld})\n", UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, "Test_count", Audit_count);

		strcpy(((char*)s_Message_Rx_data.payload_p8),Audit_log_text_teting);

		Save_Audit_log(&s_Message_Rx_data);

		if(Audit_count >= s_Para.numberWrites)
		{
			memset(Audit_log_1_buffer,0,sizeof(Audit_log_1_buffer));
			Add_Response_msg("Audit Log testing Completed", &s_Message_Rx_data, Audit_log_1_buffer);
			goto exit;
		}

		vTaskDelay((s_Para.writeDelay) / portTICK_PERIOD_MS); // Delay between transmissions
	}
exit:
	Audit_Test_Handle = NULL;
	vTaskDelete(Audit_Test_Handle); // Delete the task
}

static void File_operation(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *root_JSON 	= 	NULL;
	cJSON *head_JSON 	= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	char path[100] = {0}, filename[50] = {0};
	int err = -1, i = 0;
    char str[100] = {0}, found = 0;
	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s, payload = %s, command = %s",__LINE__,__FUNCTION__, (char*)s_Message_Rx->payload_p8, (char*)s_Message_Rx->cmdFun_a8);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	else
	{

	cJSON* new_object = cJSON_CreateObject();
	head_JSON 		= in_JSON->child;

	if((strcmp(head_JSON->string, "DIR_NAME")==0) || (strcmp(head_JSON->string, "FILE_NAME") == 0))
	{
		strcpy((char*)path,head_JSON->valuestring);
	    for(i =0; i< strlen(path); i++)
		{
			if(path[i] == '/')
			{
				found = 1;
			    break;
			}
		}
        if(found == 1)
        {
			strcpy(filename, path+i+1);

        }
        else
        {
        	Add_Response_msg("Kindly enter the correct filepath. Use drive 'A:/' for JFS and 'B:/' for SD card.", s_Message_Rx, payLoadData);
        	goto exit;
        }
		cJSON_AddStringToObject(new_object, head_JSON->string, filename);
	}

	else if(strcmp(head_JSON->string, "TYPE")==0)   //OLD_NAME
	{
		cJSON_AddStringToObject(new_object, head_JSON->string, head_JSON->valuestring);
		cJSON *old_name = cJSON_GetObjectItem(in_JSON, "OLD_NAME");
		if((old_name != NULL) && (old_name->valuestring != NULL))
		{
			strcpy((char*)path,old_name->valuestring);
			for(i =0; i< strlen(path); i++)
			{
				if(path[i] == '/')
				{
					found = 1;
					break;
				}
			}
			if(found == 1)
			{
				strcpy(filename, path+i+1);
			}
			else
			{
				Add_Response_msg("Kindly enter the correct filepath. Use drive 'A:/' for JFS and 'B:/' for SD card.", s_Message_Rx, payLoadData);
				goto exit;
			}
			cJSON_AddStringToObject(new_object, old_name->string, filename);
		}
		head_JSON = head_JSON->next;
	}

	head_JSON = head_JSON->next;   // append remaining payload to the new_object
	do
	{
		if(head_JSON != NULL)
		{
			switch (head_JSON->type)
			{

				case cJSON_Number: 	cJSON_AddNumberToObject(new_object, head_JSON->string, head_JSON->valuedouble);
									break;

				case cJSON_String: 	cJSON_AddStringToObject(new_object, head_JSON->string, head_JSON->valuestring);
									break;

				case cJSON_True: 	cJSON_AddBoolToObject(new_object, head_JSON->string,true);
									break;

				case cJSON_False: 	cJSON_AddBoolToObject(new_object, head_JSON->string,false);
									break;

				case cJSON_NULL: 	cJSON_AddNullToObject(new_object, head_JSON->string);
									break;

				case cJSON_Array: 	name_JSON = cJSON_Duplicate(head_JSON, 1);
									cJSON_AddItemToObject(new_object, head_JSON->string, name_JSON);
									break;

				case cJSON_Object: 	name_JSON = cJSON_Duplicate(head_JSON, 1);
									cJSON_AddItemToObject(new_object, head_JSON->string, name_JSON);//cJSON_AddObjectToObject(out_JSON1, head_JSON1->string);
									break;

				case cJSON_Raw: 	name_JSON = cJSON_Duplicate(head_JSON, 1);
									cJSON_AddItemToObject(new_object, head_JSON->string, name_JSON); //cJSON_AddRawToObject(out_JSON1, head_JSON1->string,head_JSON1->string);
									break;

				default:            break;
			}
			head_JSON = head_JSON->next;
		}
		else
			break;

	}while(head_JSON != NULL);

	memset(payLoadData,0,sizeof(payLoadData));  // Obtain the complete payload without drive letter
	cJSON_PrintPreallocated(new_object, payLoadData, sizeof(payLoadData), false);
//	if(strncmp(path,"A",1) == 0)  // perform operations on JFS   //filename
	if(strncasecmp(path,"A",1) == 0)  // perform operations on JFS   //filename
	{
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"WRITE")==0)
		{
			JFS_Write_File(payLoadData,s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"READ")==0)
		{
			JFS_Read_File((uint8_t*)payLoadData,s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"SMTP_READ")==0)
		{
			if(SMTP_Read_Queue == NULL)
				SMTP_Read_Queue = xQueueCreateStatic(FILE_SMTP_READ_TASK_QUEUE_LENGTH, FILE_SMTP_READ_TASK_QUEUE_ITEMSIZE, SMTP_READ_pucQueueStorage, &SMTP_READ_pxQueueBuffer);

			if (SMTP_Read_Queue == NULL)
			{
				Add_Response_msg("Error! SMTP_Read_Queue is not created.", s_Message_Rx, payLoadData);
			}

			if(Read_Handle == NULL)
			{
				memset(JFS_READ_data_buffer,0,sizeof(JFS_READ_data_buffer));
				if(strlen((char*)payLoadData) != 0)
				{
					strcpy(JFS_READ_data_buffer, payLoadData);
				}
				s_Message_Rx_SMTP.payload_p8 = (uint8_t*)JFS_READ_data_buffer;
				strcpy((char*)s_Message_Rx_SMTP.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
				strcpy((char*)s_Message_Rx_SMTP.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
				strcpy((char*)s_Message_Rx_SMTP.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);
				Read_Handle = xTaskCreateStaticPinnedToCore(JFS_SMTP_Read_File, "Read File", FMA_JFSSMTP_READ_TASK_STACK_DEPTH,&s_Message_Rx_SMTP, FMA_JFSSMTP_READ_TASK_PRIORITY, xSMTP_Read_TaskStack,&xSMTP_ReadTaskBuffer,1);
				vTaskDelay(200/portTICK_PERIOD_MS);
			}
			else
			{
				Add_Response_msg("Task for SMTP_READ command is already created.", s_Message_Rx, payLoadData);
			}
			if(SMTP_Read_Queue != NULL)
			{
				cJSON *out_JSON 	= cJSON_CreateObject();
				cJSON_AddNumberToObject(out_JSON, "SMTP_RESP", E_JFS_RESP_OK);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
				cJSON_Delete(out_JSON);
				xQueueSend(SMTP_Read_Queue, payLoadData, QUE_DELAY);
			}
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"DEL_FILE")==0)
		{
			JFS_FLASH_Delete_File(payLoadData,s_Message_Rx);// Delete a existing File from the JFS flash memory
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"RENAME")==0)
		{
			 JFS_Rename((char*)payLoadData,s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"CREATE_DIR")==0)
		{
			char DirName [50]= {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			root_JSON 	  = cJSON_Parse(payLoadData);
			if (root_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			}
			else{
				head_JSON 	  = cJSON_GetObjectItem(root_JSON, "DIR_NAME");
				strcpy(DirName,head_JSON->valuestring);
				JFS_FLASH_Create_Dir(DirName,s_Message_Rx);	// Create Dir in JFS flash memory
				cJSON_Delete(root_JSON);
		    }
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"DEL_DIR")==0)
		{
			char DirName[50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			root_JSON = cJSON_Parse(payLoadData);
			if (root_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
			}
			else{
				head_JSON = cJSON_GetObjectItem(root_JSON, "DIR_NAME");
				strcpy(DirName,head_JSON->valuestring);
				JFS_FLASH_Delete_Dir(DirName,s_Message_Rx);// Delete a existing Dir from the JFS flash memory
				cJSON_Delete(root_JSON);
		    }
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"GET_FILE_LIST")==0)
		{
			JFS_Get_File_List(payLoadData, s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"WRITE_VAR")==0)
		{
			JFS_Write_Variable((uint8_t*)payLoadData,s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"READ_VAR")==0)
		{
			JFS_Read_Variable((uint8_t*)payLoadData,s_Message_Rx);
		}
		else if(strcmp((char*)s_Message_Rx->cmdFun_a8,"GET_FILE_SIZE")==0)
		{
			int32_t size = JFS_GET_FILE_SIZE(payLoadData,s_Message_Rx);
			{
				cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
				cJSON_AddStringToObject(responseObject, "FILE_NAME", filename);
				cJSON_AddNumberToObject(responseObject, "FILE_SIZE", size);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				console_send_responce_to_console_xface(s_Message_Rx);
				cJSON_Delete(responseObject);
			}
		}
		else if(!strcmp((char*)s_Message_Rx->cmdFun_a8 , "GET_FILE_CRC"))
		{
			Calculate_CRC16_File_From_JFS(payLoadData,s_Message_Rx);
		}
		goto exit;
	}
	if(strncasecmp(path,"B",1) == 0)   // Perform operations on SD card
	{
		//File_add_response_to_File_Tx_Queue("SD", buffer, strlen(buffer), command_funct, msg_id);
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"WRITE")==0)
		{

			err = SD_card_Write((char*)payLoadData, s_Message_Rx);
			if(err != ESP_OK)
			{
				Add_Response_msg("Failed to open SD card file for writing.",s_Message_Rx, payLoadData);
			}
			else
			{
				Add_Response_msg("File is stored in SD card successfully",s_Message_Rx, payLoadData);
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"READ")==0)
		{
			err = SD_card_Read((char*)payLoadData, datau8, s_Message_Rx);
			if(err != ESP_OK)
			{
				Add_Response_msg("Failed to open SD card file for reading",s_Message_Rx, payLoadData);
			}
			else
			{
				Add_Response_msg(datau8,s_Message_Rx, payLoadData);
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"DEL_FILE")==0)
		{
			err = Delete_file((char*)payLoadData, s_Message_Rx);
			if(err == ESP_OK)
			{
				Add_Response_msg("File is deleted",s_Message_Rx, payLoadData);
			}
			else
			{
				if(err == ESP_FAIL)
				{
					Add_Response_msg("File not found",s_Message_Rx, payLoadData);
				}

				else
				{
					Add_Response_msg("Error in deleting the file.",s_Message_Rx, payLoadData);
				}
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"RENAME")==0)
		{
			err = SD_Rename_file((char*)payLoadData, s_Message_Rx);
			if(err == ESP_OK)
			{
				Add_Response_msg("File is renamed",s_Message_Rx, payLoadData);
			}

			else
			{
				if(err == -2)
				{
					Add_Response_msg("Cannot rename the file/directory. Access is denied.",s_Message_Rx, payLoadData);
				}
				else
				{
					sprintf(str, "Error in renaming file: %s", strerror(errno));
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				}
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"CREATE_DIR")==0)
		{
			err = SD_Create_Directory((char*)payLoadData, s_Message_Rx);
			if(err == ESP_OK)
			{
				Add_Response_msg("Directory is created",s_Message_Rx, payLoadData);
			}

			else
			{
				sprintf(str, "Error in creating the directory: %s", strerror(errno));
				Add_Response_msg(str,s_Message_Rx, payLoadData);
			}

		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"DEL_DIR")==0)
		{
			err = SD_Delete_Directory((char*)payLoadData, s_Message_Rx);
			if(err == ESP_OK)
			{
				Add_Response_msg("Directory is deleted",s_Message_Rx, payLoadData);
			}
			else
			{
				sprintf(str, "Error in deleting the directory: %s", strerror(errno));
				Add_Response_msg(str,s_Message_Rx, payLoadData);
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"GET_FILE_LIST")==0)
		{
			SD_Get_File_List(s_Message_Rx);
		}

		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"SMTP_READ")==0)
		{
			strcpy((char*)SMTP_payload, payLoadData);
			if(Read_Handle_SD == NULL)
			{
				static AMessage_st s_Message_Rx_SMTP;
				memset(SD_READ_data_buffer,0,sizeof(SD_READ_data_buffer));
				if(strlen(payLoadData) != 0)
				{
					strcpy(SD_READ_data_buffer, payLoadData);
				}
				s_Message_Rx_SMTP.payload_p8 = (uint8_t*)SD_READ_data_buffer;
				strcpy((char*)s_Message_Rx_SMTP.cmdFun_a8,(char*) s_Message_Rx->cmdFun_a8);
				strcpy((char*)s_Message_Rx_SMTP.dest_Actor_a8,(char*) s_Message_Rx->dest_Actor_a8);
				strcpy((char*)s_Message_Rx_SMTP.src_Actor_a8,(char*) s_Message_Rx->src_Actor_a8);
				Read_Handle_SD = xTaskCreateStaticPinnedToCore(SD_SMTP_Read_File, "Read SD File", FMA_SDSMTP_READ_TASK_STACK_DEPTH,&s_Message_Rx_SMTP, FMA_SDSMTP_READ_TASK_PRIORITY, xSMTP_SD_Read_TaskStack,&xSMTP_SD_ReadTaskBuffer,1);
			}
			else
			{
				Add_Response_msg("Task for SMTP_READ command is already created.", s_Message_Rx, payLoadData);
			}
		}
		if(strcmp((char*)s_Message_Rx->cmdFun_a8,"GET_FILE_SIZE")==0)
		{
			int32_t size = SD_GET_FILE_SIZE(payLoadData, s_Message_Rx);
			if(size == -1)
			{
				Add_Response_msg("Error! Cannot open file.",s_Message_Rx, payLoadData);
			}
			else
			{
				cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
				cJSON_AddNumberToObject(responseObject, "FILE_SIZE", size);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				console_send_responce_to_console_xface(s_Message_Rx);
				cJSON_Delete(responseObject);
			}
		}
	}
exit:
	cJSON_Delete(in_JSON);
	cJSON_Delete(new_object);
	}  // end of else
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n FILE s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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
			printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData,  THIS_ACTOR);

		}
		cJSON_Delete(in_JSON);
	}

	if((strcmp((char*)s_Message_Rx->src_Actor_a8,"SMTP_CLIENT")==0) || (strcmp((char*)s_Message_Rx->src_Actor_a8,"HTTP")==0))
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(in_JSON, payLoadData, sizeof(payLoadData), false);
		if(SMTP_Read_Queue != NULL)
			xQueueSend(SMTP_Read_Queue, payLoadData, QUE_DELAY);
		cJSON_Delete(in_JSON);
		return;
	}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		name_JSON 		= cJSON_GetObjectItem(in_JSON, "SERIAL_NUM");
		if(name_JSON != NULL)
		{
			strcpy(deviceID,name_JSON->valuestring);
		}
		cJSON_Delete(in_JSON);
		return;
	}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
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
				name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICEID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					strcpy((char*)fma_deviceID,name_JSON->valuestring);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	return;
}


static void Copy_File_From_JFS_to_SD(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	char str[100] = {0}, JFS_File[100] = {0}, SD_File[100] = {0}, count = 0;
	FS_FILE * pFile 	= 	NULL;
	uint32_t File_size = 0;
	int  buffer_len = 4500, File_length = 0;
	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);  //It is deleted in http_stream_reader() function
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_CPY_FILE);
		sendCpyStatus("Error");
		return;
	}
	cJSON *JFS_Filename = cJSON_GetObjectItem(in_JSON, "JFS_FILE_NAME");
	if((JFS_Filename != NULL) && (cJSON_IsString(JFS_Filename)))
	{
		if(strlen(JFS_Filename->valuestring) != 0)
		{
			strcpy(JFS_File, JFS_Root_Path);
			strcat(JFS_File, JFS_Filename->valuestring);
		}
		else
		{
			Add_Response_msg("JFS file name is empty.",s_Message_Rx, payLoadData_CPY_FILE);
			cJSON_Delete(in_JSON);
			sendCpyStatus("Error");
			return;
		}
	}
	cJSON *SD_Filename = cJSON_GetObjectItem(in_JSON, "SD_FILE_NAME");
	if((SD_Filename != NULL) && (cJSON_IsString(SD_Filename)))
	{
		if(strlen(SD_Filename->valuestring) != 0)
		{
			SD_Extract_folders(SD_Filename->valuestring);
			strcpy(SD_File, MOUNT_POINT);
			strcat(SD_File, "/");
			strcat(SD_File, SD_Filename->valuestring);
		}
		else
		{
			Add_Response_msg("SD card file name is empty.",s_Message_Rx, payLoadData_CPY_FILE);
			cJSON_Delete(in_JSON);
			sendCpyStatus("Error");
			return;
		}
	}
	pFile = FS_FOpen(JFS_File, "r");		// open the JFS file in the read mode
	if (pFile == NULL) {
		Add_Response_msg("Error! Failed to open JFS file",s_Message_Rx, payLoadData_CPY_FILE);
		cJSON_Delete(in_JSON);
		sendCpyStatus("Error");
		return;
	}
	File_size = FS_GetFileSize(pFile);
	struct stat st;
	if (stat(SD_File, &st) == 0)
	{
        if (unlink(SD_File) != 0)
        {
            Add_Response_msg("Error! Failed to delete existing SD card file", s_Message_Rx, payLoadData_CPY_FILE);
            FS_FClose(pFile);
            cJSON_Delete(in_JSON);
    		sendCpyStatus("Error");
            return;
        }
	}

	FILE *f = fopen(SD_File, "w");
	if (f == NULL)
	{
		Add_Response_msg("Failed to open SD card file for writing.",s_Message_Rx, payLoadData_CPY_FILE);
		FS_FClose(pFile);
		cJSON_Delete(in_JSON);
		sendCpyStatus("Error");
		return;
	}

	while(1)
	{
	   	int data_read = FS_FRead((char*) Read_bufferCPY_JFS_SD, 1, buffer_len, pFile); // Read data from JFS Flash memory
		if (data_read < 0)
		{
			Add_Response_msg("No data available for reading",s_Message_Rx, payLoadData_CPY_FILE);
			sendCpyStatus("Error");
			goto exit;
		}
		else if (data_read > 0)
		{
			size_t written = fwrite(Read_bufferCPY_JFS_SD, 1, data_read, f);
			if (written != data_read) {
				Add_Response_msg("Failed to write data to SD card file",s_Message_Rx, payLoadData_CPY_FILE);
				sendCpyStatus("Error");
				goto exit;
			}

		   File_length = File_length + data_read;
		   count++;
		   if(count > 20)
		   {
				count = 0;
				sprintf(str,"JFS to SD card file transfer progress:  %0.2f %%", ((float)((float)File_length/(float)File_size)*100));
				Add_Response_msg(str,s_Message_Rx, payLoadData_CPY_FILE);
		   }
		}
		 else
		{
			 Add_Response_msg("File is stored in SD card successfully",s_Message_Rx, payLoadData_CPY_FILE);
			 sendCpyStatus("File is stored in SD card successfully");
			 goto exit;
		}
	}
exit:
   if (f != NULL) {
       fclose(f);
       f = NULL;
   }
   if (pFile != NULL) {
       FS_FClose(pFile);
       pFile = NULL;
   }
   if (in_JSON != NULL) {
       cJSON_Delete(in_JSON);
   }
}

static void Calculate_CRC16_File_From_JFS(char *payload,AMessage_st* s_Message_Rx)
{
    cJSON *in_JSON = NULL;
    char str[100] = {0}, JFS_File[50] = {0};
    FS_FILE *pFile = NULL;
    int buffer_len = 1024;
    uint16_t crc = 0x00;
    in_JSON = cJSON_Parse((char*)payload);
    if (in_JSON == NULL) {
    	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        return;
    }

    cJSON *JFS_Filename = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
    if ((JFS_Filename != NULL) && (cJSON_IsString(JFS_Filename))) {
        if (strlen(JFS_Filename->valuestring) != 0) {
            strcpy(JFS_File, JFS_Root_Path);
            strcat(JFS_File, JFS_Filename->valuestring);
        } else {
            Add_Response_msg("JFS file name is empty.", s_Message_Rx, payLoadData);
            cJSON_Delete(in_JSON);
            return;
        }
    } else {
        Add_Response_msg("JFS file name is missing in JSON.", s_Message_Rx, payLoadData);
        cJSON_Delete(in_JSON);
        return;
    }

    pFile = FS_FOpen(JFS_File, "rb"); // Open the JFS file in read mode
    if (pFile == NULL) {
        Add_Response_msg("Error! Failed to open JFS file.", s_Message_Rx, payLoadData);
        cJSON_Delete(in_JSON);

		cJSON *jsonObject = cJSON_CreateObject();
		cJSON_AddStringToObject(jsonObject, "CRC16", "NULL");
		cJSON_AddNumberToObject(jsonObject, "FILE_SIZE", -1);
		cJSON_AddStringToObject(jsonObject, "FileName",JFS_File );
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
		cJSON_Delete(jsonObject);
        return;
    }
    while (1) {
        int data_read = FS_FRead((char*)Read_buffer_CRC, 1, buffer_len, pFile); // Read data from JFS Flash memory
        if (data_read < 0) {
            Add_Response_msg("No data available for reading.", s_Message_Rx, payLoadData);
            goto exit;
        } else if (data_read > 0) {
            crc = gen_crc161(crc, Read_buffer_CRC, data_read);
        } else {
            break; // EOF reached
        }
    }
	// Create a JSON object and add the array to it
    	char CRC16Arr[5];
    	sprintf(CRC16Arr,"%04X",crc);
		cJSON *jsonObject = cJSON_CreateObject();
		cJSON_AddStringToObject(jsonObject, "CRC16", CRC16Arr);
		uint32_t filesize = FS_GetFileSize(pFile);
		cJSON_AddNumberToObject(jsonObject, "FILE_SIZE", filesize);
		cJSON_AddStringToObject(jsonObject, "FileName",JFS_File );
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
		cJSON_Delete(jsonObject);
exit:
    if (pFile != NULL) {
        FS_FClose(pFile);
        pFile = NULL;
    }
    if (in_JSON != NULL) {
        cJSON_Delete(in_JSON);
    }
}

static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch)
{
   date_time->second = epoch%60; epoch /= 60;
   date_time->minute = epoch%60; epoch /= 60;
   date_time->hour   = epoch%24; epoch /= 24;
   unsigned long years = epoch/(365*4+1)*4; epoch %= 365*4+1;
   unsigned long year;
   const unsigned short days[4][12] =
   {
      {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
      { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
      { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
      {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
   };
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

PSRAM_ATTR char file_paths[30][256];  // Array to store file paths
int path_count = 0;  // Counter for the number of paths stored

static void CpyAllFiles_JFS_To_SD(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	date_time_t  sdate_timUTC;
	uint64_t current_epos_sec;//mills;
	struct timeval currentTime;
	cJSON *in_JSON 		= NULL;
	char str[200];
	cJSON *my_JSON 		= NULL;
	char queBuffer [500]= {0}; // (char*) heap_caps_calloc(500, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char timestamp[100];
	uint8_t retryCpyCount = 0;
	sendLedState("Upload", -1);

	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Copy_ALL_JFS_SD,0,sizeof(payLoadData_Copy_ALL_JFS_SD));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Copy_ALL_JFS_SD, sizeof(payLoadData_Copy_ALL_JFS_SD), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM",payLoadData_Copy_ALL_JFS_SD,strlen(payLoadData_Copy_ALL_JFS_SD),"GET");
	vTaskDelay(2000 / portTICK_PERIOD_MS);
	my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "DIR_NAME", "");
	memset(payLoadData_Copy_ALL_JFS_SD,0,sizeof(payLoadData_Copy_ALL_JFS_SD));
	cJSON_PrintPreallocated(my_JSON, payLoadData_Copy_ALL_JFS_SD, sizeof(payLoadData_Copy_ALL_JFS_SD), false);
	JFS_Get_File_List(payLoadData_Copy_ALL_JFS_SD, s_Message_Rx);
	cJSON_Delete(my_JSON);
	in_JSON 		= cJSON_Parse((char*)s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input of CpyAllFiles get listat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx,payLoadData_Copy_ALL_JFS_SD);
		cleanup_CpyAllFiles_resources();
	}
	else
	{
		ExtractFilesFromJson(in_JSON, "");
	}

	cJSON_Delete(in_JSON);
	_gettimeofday_r(NULL, &currentTime, NULL);
	current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
#ifdef ENABLE_PRINT_MSG
		printf("Current_epos_sec: %lld\n", current_epos_sec);
#endif
		current_epos_sec =(current_epos_sec/1000)-EPOSCH_TO_30_YEAR;  //Convert epoch msec to seconds and subtract offset of 30 years
		epoch_to_date_time(&sdate_timUTC,current_epos_sec);
#ifdef ENABLE_PRINT_MSG
		printf("UTC TIME: %02d/%02d/%02d %02d:%02d:%02d\n",sdate_timUTC.date, sdate_timUTC.month,sdate_timUTC.year,sdate_timUTC.hour,sdate_timUTC.minute,sdate_timUTC.second); // Years since 1900
#endif


//		char timestamp[100];
		    snprintf(timestamp, sizeof(timestamp), "Haven/X-Series/Logs/%02d%02d%02d %02d%02d%02d (%s)",
		    		sdate_timUTC.year,sdate_timUTC.month,sdate_timUTC.date,
		             sdate_timUTC.hour, sdate_timUTC.minute, sdate_timUTC.second, fma_deviceID);

		    // Buffer to store new file path
		    char new_file_path[400];

		    char SD_file_paths[400];  // Array to store file paths

		cJSON *responseObject;
		if( path_count!=0)
		{
			for (int i = 0; i < path_count; )
			{
			  // Clear the new_file_path buffer
			  memset(new_file_path, 0, sizeof(new_file_path));
			  memset(SD_file_paths, 0, sizeof(SD_file_paths));
			  snprintf(new_file_path, sizeof(new_file_path), "%s/%s", timestamp, file_paths[i]);

			  // Copy the new file path back to the original array
			  strncpy(SD_file_paths, new_file_path, 256 - 1);

			  responseObject = cJSON_CreateObject();
			  if (responseObject == NULL) {
				// Handle error
//					printf("\n\n handle error");
				Add_Response_msg("JSON Object creation failed in CPY ALL FILES",s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
				cleanup_CpyAllFiles_resources();
			  }

			  cJSON_AddStringToObject(responseObject, "JFS_FILE_NAME", file_paths[i]);
			  cJSON_AddStringToObject(responseObject, "SD_FILE_NAME", SD_file_paths);
			  memset(payLoadData_Copy_ALL_JFS_SD,0,sizeof(payLoadData_Copy_ALL_JFS_SD));
			  cJSON_PrintPreallocated(responseObject, payLoadData_Copy_ALL_JFS_SD, sizeof(payLoadData_Copy_ALL_JFS_SD), false);
			  cJSON_Delete(responseObject);
			  Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Copy_ALL_JFS_SD, strlen(payLoadData_Copy_ALL_JFS_SD), "COPY_JFS_SD");

			if (pdTRUE == xQueueReceive(CpyAllFiles_Que, (void*)queBuffer, portMAX_DELAY))
			{
				++i;
				if(strcmp(queBuffer,(char*)"Error")==0)
				{
					if(++retryCpyCount>3)
					{
						sprintf(str, "Failed to copy all files. Please reinitiate copy");
						Add_Response_msg(str,s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
						i = path_count;
					}
					else
					{
						i = 0;
						sprintf(str, "Retrying copy all files, retry count:%d", retryCpyCount);
						Add_Response_msg(str,s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
						Release_bus();
						int16_t ret_s16;
						ret_s16 = init_SD_Card(0,0);
						if(ret_s16 == -2)
						{
							//printf("\n\nIn SD card, failed to initialize SPI bus.\n\n");
							Add_Response_msg("Failed to initialize SD card/SPI bus. Deleting Task CPY ALL FILES. ",s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
//								Add_Response_msg("Deleting Task CPY ALL FILES",s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
							Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE");
							cleanup_CpyAllFiles_resources();
						}
						else
						{
							if(ret_s16 == ESP_FAIL)
							{
								Add_Response_msg("Failed to mount filesystem in the SD card. Deleting Task CPY ALL FILES.",s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
								Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE");
								cleanup_CpyAllFiles_resources();
							}
							else
							{
								if(ret_s16 != ESP_OK)
								{

									Add_Response_msg("Failed to initialize SD card. Deleting Task CPY ALL FILES.",s_Message_Rx, payLoadData_Copy_ALL_JFS_SD);
									Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE");
									cleanup_CpyAllFiles_resources();
								}
								else
								{
									Add_Response_msg("SD card is initialized successfully.",s_Message_Rx,payLoadData_Copy_ALL_JFS_SD);
								}

							}
						}
					}

				}
			}
		}
	}
	else
		Add_Response_msg("No files found in JFS",s_Message_Rx,payLoadData_Copy_ALL_JFS_SD);

	cJSON *JSON_Copy = cJSON_CreateObject();
	cJSON_AddStringToObject(JSON_Copy, "FILE_PATH", timestamp);
	payLoadData_Copy_ALL_JFS_SD[0] = '\0';
	cJSON_PrintPreallocated(JSON_Copy, payLoadData_Copy_ALL_JFS_SD, sizeof(payLoadData_Copy_ALL_JFS_SD), false);
	cJSON_Delete(JSON_Copy);
	Send_CMD_To_Other_Actor(SYS_FILES, "SYS_FILES",payLoadData_Copy_ALL_JFS_SD,strlen(payLoadData_Copy_ALL_JFS_SD),"COPY_FILES_SD");
	cleanup_CpyAllFiles_resources();
}

static void cleanup_CpyAllFiles_resources() {
    if (CpyAllFilesHandle != NULL) {
        CpyAllFilesHandle = NULL;
    	vTaskDelete(CpyAllFilesHandle);
    }
}

void ExtractFilesFromJson(cJSON* json, const char* parentPath) {
    if (!cJSON_IsObject(json)) return;

    cJSON* filesArray = cJSON_GetObjectItem(json, "files");
    if (!cJSON_IsArray(filesArray)) return;

    cJSON* fileItem;
    cJSON_ArrayForEach(fileItem, filesArray) {
        cJSON* nameItem = cJSON_GetObjectItem(fileItem, "name");
        cJSON* typeItem = cJSON_GetObjectItem(fileItem, "type");

        if (cJSON_IsString(nameItem) && cJSON_IsString(typeItem)) {
            const char* name = nameItem->valuestring;
            const char* type = typeItem->valuestring;

            char fullPath[256];
            if (parentPath && strlen(parentPath) > 0) {
                snprintf(fullPath, sizeof(fullPath), "%s/%s", parentPath, name);
            } else {
                snprintf(fullPath, sizeof(fullPath), "%s", name);
            }
            if (strcmp(type, "Directory") == 0) {
                // Recursively process subdirectories
                ExtractFilesFromJson(fileItem, fullPath);
            } else if (strcmp(type, "Archive") == 0) {
                {
					strncpy(file_paths[path_count], fullPath, 100 - 1);
					file_paths[path_count][100 - 1] = '\0';  // Ensure null-termination
					path_count++;
				}
            }
        }
    }
}

static void sendCpyStatus(char *filedata)
{
	if(CpyAllFilesHandle != NULL)
	{
		uint8_t state = xQueueSend(CpyAllFiles_Que, filedata, QUE_DELAY);
		if (state != pdTRUE)
		{
			if (state == errQUEUE_FULL){
				printf("<FILE_SYSTEM.ERROR(CpyAllFiles_Que is full)<CR>\n");
			}					else{
				printf("<FILE_SYSTEM.ERROR(CpyAllFiles_Que send unsuccessful)<CR>");
			}
		}
	}
}

static void sendLedState(const char *stateName, int duration) {
	char payLoad[100] = {0};
    // Create a new JSON object

    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    // Print the JSON object as a string
   // payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoad, sizeof(payLoad), false);
    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoad, strlen(payLoad), "SETSTATE");

    cJSON_Delete(responseObject);
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
#ifdef ENABLE_PRINT_MSG
//		printf("gmt_value1 = %d, dst_value1 = %d, \n",gmt_val, dst_val);
#endif
	}
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

