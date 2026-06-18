/*
 * System_Files_Actor.c
 *
 *  Created on: 04-Sep-2024
 *      Author: Acer
 */


#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "System_Files_Actor.h"
#include "esp_spiffs.h"
#include <stdio.h>

//----------------------------- Actor Tags ---------------------------------------//

static const char 	* THIS_ACTOR 	= "SYS_FILES";
static const char 	THIS_ACTOR_ID 	= 	SYS_FILES;  // assign src id

//------------------------- SYS_FILES Actor Resources -----------------------------//
#define RX_QUE_COUNT 		100
#define MAX_CHUNK_SIZE 		MAX_JSON_PAYLOAD_BYTES/2 //4096 //2048
static const char Spiffs_base_path[] = "/spiffs2/";
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char DataPtr_Write[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char DataPtr_Read[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static uint8_t msg_Rx_Queue_QueStorage[RX_QUE_COUNT * sizeof(AMessage_st)];
static StaticQueue_t msg_Rx_Queue_QueBuffer;
static QueueHandle_t msg_Rx_Queue = NULL;
//static StaticTask_t xSPIFFSTaskBuffer;  //// Declare a static task control block
static TaskHandle_t SPIFFS_Handle = NULL;
static bool   FirstEntry_bool = false;
static BaseType_t 		Task_Monitor;

static void SPIFFS_file_write_variable(AMessage_st* s_Message_Rx);
static void SPIFFS_Read_Variable(AMessage_st* s_Message_Rx);
static void SPIFFS_file_read_Chunk(AMessage_st* s_Message_Rx);
static void SPIFFS_file_write_Parse(AMessage_st* s_Message_Rx);
static void SPIFFS_file_delete(AMessage_st* s_Message_Rx);
static esp_err_t SPIFFS_file_mnt(void);
static void SPIFFS_file_unmnt(void);
static void Get_File_list(AMessage_st* s_Message_Rx);
static void Copy_Files_SD_Card(AMessage_st* s_Message_Rx);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);

static void COPY_SPIFFS_JFS_proc(AMessage_st* s_Message_Rx);
//-------------------------- Common Actor Resources ------------------------------//

//static StackType_t *xTaskStack = NULL;
//static uint8_t *Monitor_pucQueueStorage = NULL;
//static StaticQueue_t *Monitor_pxQueueBuffer = NULL;
static AMessage_st 		  s_Message_Tx;

static void init(void *a, void *b);
static void Analyse_Response(AMessage_st* s_Message_Rx);
//static void Get_Property(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
//static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
//static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void monitor(void *pvParameters __attribute__((unused)));
//static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					//	Change a parameter
//static void get(char *prop, char *val);							//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);

//static struct SPIFFS_PARAMETER{
//	char file_name[50];
//}SPIFFS_para;

//PSRAM_ATTR static struct property prop[] = // Actor Property
//{
//    { &SPIFFS_para.file_name,   	   "FILE_NAME",	    STRING, 	"RW", 	"Name of the file" },
//};

//static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
//	uint8_t parameter_found = 0; // Flag to check if actor is found
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//	for (int i = 0; i < no_of_elements; i++) {
//		if (!strcmp(property, prop[i].str_name)) {
//
//			if (!strcmp(prop[i].access, "RW")) {
//
//			parameter_found = 1; // Set flag to indicate actor is found
//			switch (prop[i].type) {
//
//			case U_INT8:
//				*(uint8_t*) prop[i].name = atoi(value);
//				break;
//
//			case U_INT16:
//				*(uint16_t*) prop[i].name = atoi(value);
//				break;
//
//			case U_INT32:
//				*(uint32_t*) prop[i].name = atoi(value);
//				break;
//
//			case INT:
//				*(int*) prop[i].name = atoi(value);
//				break;
//			case FLOAT:
//				*(float*) prop[i].name = atof(value);
//				break;
//
//			case STRING:
//				strcpy((char*) prop[i].name, value);
//				break;
//
//			default:
//				break;
//			}
//		}
//		else
//		{
//			return 2;
//		}
//	}
//	}
//
//	if(parameter_found)
//		return 1;
//	else
//		return 0;
//}//	set

//static void get(char *str_prop, char *val_a8) {
//	//no of elements
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//	for (int i = 0; i < no_of_elements; i++) {
//		if (!strcmp(str_prop, prop[i].str_name)) {
//			switch (prop[i].type) {
//
//			case U_INT8:
//				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
//				break;
//
//			case U_INT16:
//				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
//				break;
//
//			case U_INT32:
//				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
//				break;
//
//			case U_INT64:
//				sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
//				break;
//
//			case INT:
//				sprintf(val_a8, "%d", *(int*) prop[i].name);
//				break;
//
//			case FLOAT:
//				sprintf(val_a8, "%f", *(float*) prop[i].name);
//				break;
//
//			case STRING:
//				strcpy(val_a8, prop[i].name);
//				break;
//
//			default:
//				break;
//			}
//		}
//	}
//}//	get


//static void get_actor_properties(AMessage_st* s_Message_Rx){
//
//	char val_a8[50] = {0};
//	char typeString[20] = {0};
//
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//
//	    // Create JSON arrays
//	    cJSON *jsonArrayName = cJSON_CreateArray();
//	    cJSON *jsonArrayType = cJSON_CreateArray();
//	    cJSON *jsonArrayValue = cJSON_CreateArray();
//	    cJSON *jsonArrayAccess = cJSON_CreateArray();
//	    cJSON *jsonArrayHelpString = cJSON_CreateArray();
//
//	    for (int i = 0; i < no_of_elements; i++) {
//			cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(prop[i].str_name));
//			// Convert DataType enum to string representation for property type
//			// Add value based on data type for property name
//			switch (prop[i].type) {
//				case U_INT8:
//					strcpy(typeString, "U_INT8");
//					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
//					break;
//
//				case U_INT16:
//					strcpy(typeString, "U_INT16");
//					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
//					break;
//
//				case U_INT32:
//					strcpy(typeString, "U_INT32");
//					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
//					break;
//
//				case U_INT64:
//					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
//					break;
//
//				case INT:
//					strcpy(typeString, "INT");
//					sprintf(val_a8, "%d", *(int*) prop[i].name);
//					break;
//
//				case FLOAT:
//					strcpy(typeString, "FLOAT");
//					sprintf(val_a8, "%f", *(float*) prop[i].name);
//					break;
//
//				case STRING:
//					strcpy(typeString, "STRING");
//					strcpy(val_a8, prop[i].name);
//					break;
//
//				default:
//					break;
//				}
//			cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
//			cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(val_a8));
//			cJSON_AddItemToArray(jsonArrayAccess, cJSON_CreateString(prop[i].access));
//			cJSON_AddItemToArray(jsonArrayHelpString, cJSON_CreateString(prop[i].HelpString));
//		}
//		// Create a JSON object and add the array to it
//		cJSON *jsonObject = cJSON_CreateObject();
//		cJSON_AddItemToObject(jsonObject, "Name", jsonArrayName);
//		cJSON_AddItemToObject(jsonObject, "Type", jsonArrayType);
//		cJSON_AddItemToObject(jsonObject, "Value", jsonArrayValue);
//		cJSON_AddItemToObject(jsonObject, "Access", jsonArrayAccess);
//		cJSON_AddItemToObject(jsonObject, "Help String", jsonArrayHelpString);
//		memset(payLoadData,0,sizeof(payLoadData));//\0';
//		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
//		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//
//		// Cleanup
//		cJSON_Delete(jsonObject);
//		console_send_responce_to_console_xface(s_Message_Rx);
//
//}	//	get_actor_properties

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
	cJSON_AddStringToObject(responseObject, "FILE_LIST()", "Get file list");
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

// Create a queue with storage in PSRAM
//static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer)
//{
//    QueueHandle_t xQueue;
////    uint8_t *pucQueueStorage;
////    StaticQueue_t *pxQueueBuffer;
//
//    // Allocate the queue storage area in PSRAM
//    *pucQueueStorage = (uint8_t *)heap_caps_malloc((uxQueueLength * uxItemSize), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//    if (*pucQueueStorage == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("\n Failed to allocate queue storage in PSRAM \n");
//#endif
//        return NULL;
//    }
//    memset(*pucQueueStorage,0,(uxQueueLength * uxItemSize));
//
//    // Allocate the queue structure itself in PSRAM
//    *pxQueueBuffer = (StaticQueue_t *)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//    if (*pxQueueBuffer == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("Failed to allocate queue structure in PSRAM\n");
//#endif
//        heap_caps_free(pucQueueStorage);
//        return NULL;
//    }
//    memset(*pxQueueBuffer,0,sizeof(StaticQueue_t));
//    // Create the queue with custom storage
//    xQueue = xQueueCreateStatic(uxQueueLength, uxItemSize, *pucQueueStorage, *pxQueueBuffer);
//
//    if (xQueue == NULL) {
//#ifdef ENABLE_PRINT_MSG
//        printf("Failed to create queue in PSRAM \n");
//#endif
//        heap_caps_free(*pucQueueStorage);
//        heap_caps_free(*pxQueueBuffer);
//    }
//    return xQueue;
//}

static void init(void *a, void *b) {
	if (FirstEntry_bool) {
		return;
	}
//	// Calculate the total size of memory required for the stack array
	//size_t stack_size = SPIFFS_MONITOR_TASK_STACK_DEPTH * sizeof(StackType_t);

	//Create Rx_Queue
//	msg_Rx_Queue = xQueueCreateInPSRAM(RX_QUE_COUNT, sizeof(AMessage_st), &Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
//	msg_Rx_Queue = xQueueCreate(RX_QUE_COUNT, sizeof(AMessage_st));
//	 printf("\n create que and task to send the attachment");
	if(msg_Rx_Queue == NULL)
	{
		msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), msg_Rx_Queue_QueStorage, &msg_Rx_Queue_QueBuffer);
	}

	if (msg_Rx_Queue == NULL) {
#ifdef ENABLE_PRINT_MSG
		printf("SPIFFS RX Queue is not created. \n");
#endif
		return;
	}

	Task_Monitor = xTaskCreate(monitor, THIS_ACTOR ,SPIFFS_MONITOR_TASK_STACK_DEPTH, NULL, 2, &SPIFFS_Handle);
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
		memset(&s_Message_Tx,0,sizeof(s_Message_Tx));
//		SPIFFS_file_mnt();
		FirstEntry_bool = true;
}// init

static void monitor(void *pvParameters __attribute__((unused))) {

//	cJSON *in_JSON 		= NULL;
//	cJSON *out_JSON 	= NULL;
//	cJSON *name_JSON 	= NULL;
//	cJSON *head_JSON 	= NULL;  //refer to head_JSON and tail as in linked list
//	uint8_t *val_p8 	= NULL;
	char str[100] = {0};
//	char Rx_buffer_used = 0;
//	uint8_t u8Result =0;
	int ret = SPIFFS_file_mnt();    // mount spiffs file system
	if(ret != ESP_OK)
	{
//		#ifdef ENABLE_PRINT_MSG
				printf(">%s.INIT(Error in mounting SPIFFS for system files.) \n", THIS_ACTOR);
//		#endif
	}

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
//		char Rx_buffer[6*1024];
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		  if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))  //5
		  {
//			Rx_buffer_used = 0;
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
//		    printf("\n SYS FILES msg_Rx_Queue S = %s, D = %s, C = %s\n\n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("\n SYS FILES DT = %s",s_Message_Rx->payload_p8);
//			}
		    // Write code as per received cmds
			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				init(0, 0);
				Add_Response_msg("SYSTEM_FILES Actor is initialized.", s_Message_Rx);
			}
//			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GETALL")) {
//				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//				getAll(prop, (char*) val_p8, s_Message_Rx);
//				free(val_p8);
//			}
//			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) { // Get Actor Properties
//
//				Get_Property(s_Message_Rx);
//			}
//			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SET")) // Set Actor Properties
//			{
//				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
//				if (name_JSON == NULL) {
//					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//					Add_Response_msg(str,s_Message_Rx);
//				    }
//				else{
//					head_JSON = name_JSON->child;
////					cJSON *root_JSON  = cJSON_CreateObject();
////					cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/HTTP.json");
//				   // Loop through each key-value pair
//				    do {
//				    	// Check if the value string is not NULL
//				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
//						{
//							// Set the key-value pair
//							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
////							if(strcmp(head_JSON->string, "CONNECTION_STATUS") !=0)
////							{
//							if(u8Result==1)
//							{
////							cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
//							}
//							else if(u8Result==2){
//								sprintf(str,"'%s' is a read only property", head_JSON->string);
//								 Add_Response_msg(str, s_Message_Rx);
//							}
////							else{
////							cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
////							}
//						} else {
//				            // Handle the case where value string is NULL (e.g., log an error or take appropriate action)
//				            sprintf(str, "Invalid parameter '%s'", head_JSON->string);
//				            Add_Response_msg(str,s_Message_Rx);
//				            // Handle the error as per your application's requirements
//				        }
//				        head_JSON = head_JSON->next;
//				    } while (head_JSON != 0);
//
//
//					// Free the parsed JSON
//					cJSON_Delete(name_JSON);
//
//				    if(u8Result==1){
//				    console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
//				    }
//				}
//			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "FILE_LIST")) {
				Get_File_list(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
//			{
//				get_actor_properties(s_Message_Rx);
//			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "WR_PARA"))
			{
				SPIFFS_file_write_variable(s_Message_Rx);
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
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "COPY_SPIFFS_JFS"))
			{
				COPY_SPIFFS_JFS_proc(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "FORMAT"))
			{
//			    ret = esp_spiffs_format("System_files");
				SPIFFS_file_unmnt();  // unmount the file system at first

			    int retry =0;
			    while(retry++ < 3)
			    {
			    	ret = esp_spiffs_format("System_files");
					if (ret != ESP_OK)
					{
						sprintf(str, "Failed to format SPIFFS (%s)", esp_err_to_name(ret));
						Add_Response_msg(str, s_Message_Rx);
						vTaskDelay(100);
					}
					else
						break;
			    }
//				else
//				{
//					Add_Response_msg("SPIFFS used for system files is formatted successfully. Restarting the system...", s_Message_Rx);
//					vTaskDelay(100);
//					esp_restart();
//				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "UNMOUNT"))
			{
				SPIFFS_file_unmnt();
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "COPY_FILES_SD"))
			{


				Copy_Files_SD_Card(s_Message_Rx);


//				char filename_src [50]; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//				char filename_dst [50];
//				char *filedata = NULL;
//				cJSON *in_JSON = NULL;
//				cJSON *nameSrc_JSON = NULL;
//				cJSON *nameDst_JSON = NULL;
//				printf("\n\nIn file copy.");
//
//				memset(filename_src,0,sizeof(filename_src));
//				memset(filename_dst,0,sizeof(filename_dst));



//				in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
//				if (in_JSON == NULL) {
//					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//					Add_Response_msg(str,s_Message_Rx);
//					;
//					}
//			//	head_JSON 	= in_JSON->child;
//				nameSrc_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME_SRC");
//				if((nameSrc_JSON!= NULL) && (nameSrc_JSON->valuestring != NULL))
//					strcpy(filename_src,nameSrc_JSON->valuestring);
//
//				nameDst_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME_SRC");
//							if((nameDst_JSON!= NULL) && (nameDst_JSON->valuestring != NULL))
//								strcpy(filename_dst,nameDst_JSON->valuestring);
//
//				printf("\n\nFilesrc [%s] --> File Dst [%s].", filename_src, filename_dst);
//				int err = FS_CopyFile(filename_src,filename_dst);
//				printf("\n\nFile copy status [%d]:[%s].", err, FS_ErrorNo2Text(err));
//				printf("\n\nFile copy status [%d]:[%s].", err, FS_ErrorNo2Text(err));
//
//				cJSON_Delete(in_JSON);
//				//printf("\n write filename=%s", filename);



			}
			else
			{
				//HTTP error message: invalid method
				Add_Response_msg("invalid method", s_Message_Rx);
			}

//			if (Rx_buffer_used == 0)
//			{
//			  if(s_Message_Rx->payload_p8 !=  NULL)
//			  {
//				console_MessageRelease_xface((char*) s_Message_Rx->payload_p8);
//			  }
////			  if(Rx_buffer != NULL)
////			  {
////				free(Rx_buffer);
////				Rx_buffer = NULL;
////			  }
//			}
		}
	}
}// monitor

void System_Files_ConsoleWriteToActor_xface(void *msg)
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
//#ifdef ENABLE_PRINT_MSG
			printf("<%s.ERROR(SPIFFS RX Queue is full)\n", THIS_ACTOR);
//#endif
		}
		else
		{
//#ifdef ENABLE_PRINT_MSG
			printf("<%s.ERROR(SPIFFS RX Queue send unsuccessful)\n", THIS_ACTOR);
//#endif
		}
	}
}//	SPIFFS_ConsolWriteToActor

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
//	char keyValue[50] = {0};
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		printf("\n sys files (char*) s_Message_Rx->payload_p8 = %s \n", (char*) s_Message_Rx->payload_p8);
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

//static void Get_Property(AMessage_st* s_Message_Rx)
//{
//	cJSON *in_JSON   = NULL;
//	cJSON *out_JSON  = NULL;
//	cJSON *head_JSON = NULL;
//	char str[100] = {0};
//	char val_p8[256] = {0};
//	int Array_size = 0;
//
//	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//	if (in_JSON == NULL) {
//		sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
//		Add_Response_msg(str,s_Message_Rx);
//		return;
//	}
//	out_JSON 	= cJSON_CreateObject();
//	head_JSON = cJSON_GetObjectItem(in_JSON, "Property_Names");
//	Array_size = cJSON_GetArraySize(head_JSON);
//	if(Array_size > 0)
//	{
//		for(int i=0; i<Array_size; i++)
//		{
//			cJSON *element = cJSON_GetArrayItem(head_JSON, i);
//			if (cJSON_IsString(element) && (element->valuestring != NULL))
//			{
//				if(strlen(element->valuestring) == 0)
//					continue;
//				memset(val_p8, 0, sizeof (val_p8));
//				get(element->valuestring, val_p8);
//				cJSON_AddStringToObject(out_JSON, element->valuestring, (char*) val_p8);
//			}
//		}
//		memset(payLoadData,0,sizeof(payLoadData));
//		cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
//		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//		console_send_responce_to_console_xface(s_Message_Rx);
//	}
//	else
//	{
//		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx);
//	}
//	cJSON_Delete(out_JSON);
//	cJSON_Delete(in_JSON);
//}
//

////long int server_Time;
//static void Get_Property(AMessage_st* s_Message_Rx)
//{
////	uint8_t *val_p8  = NULL;
//	cJSON *in_JSON   = NULL;
//	cJSON *out_JSON  = NULL;
//	cJSON *head_JSON = NULL;
//	char str[100] = {0}, val_p8[256] = {0};
//
//	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//	if (in_JSON == NULL) {
//		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//		Add_Response_msg(str,s_Message_Rx);
//		return;
//	}
//	out_JSON 	= cJSON_CreateObject();
//	head_JSON 	= in_JSON->child;
//
//	//loop
//	do {
//		memset(val_p8, 0, sizeof (val_p8));
//		get(head_JSON->string, (char*) val_p8);
//		cJSON_AddStringToObject(out_JSON, head_JSON->string, (char*) val_p8);
//		head_JSON = head_JSON->next;
//	} while (head_JSON != NULL);
//
//	memset(payLoadData,0,sizeof(payLoadData));//\0';
//	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
//	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//
//	cJSON_Delete(out_JSON);
//	cJSON_Delete(head_JSON);
//	cJSON_Delete(in_JSON);
//	console_send_responce_to_console_xface(s_Message_Rx);
//
////	free(val_p8);
//}

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
		      .base_path = "/spiffs2",
		      .partition_label = "System_files",
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
	        			printf(">%s.INIT(Failed to mount or format filesystem)\n", THIS_ACTOR);
	        		}
	        	else if (ret == ESP_ERR_NOT_FOUND)
	        		{
	        			printf (">%s.INIT(Failed to find SPIFFS partition)\n", THIS_ACTOR);
	        		}
	        	else
	        		{
	        			printf (">%s.INIT(Failed to initialize SPIFFS (%s))\n", THIS_ACTOR, esp_err_to_name(ret));
	        		}
	        return ret;
	    	}
	    ret = esp_spiffs_info(conf.partition_label, &total, &used);
	    if (ret != ESP_OK)
			{
				printf( ">%s.INIT(Failed to get SPIFFS partition information (%s))", THIS_ACTOR, esp_err_to_name(ret));
			}
//	    else
//			{
//				printf( "\n Partition size: total: %d, used: %d, ret = %d\n", total, used, ret);
//			}
	    if (used > total)
	    {
	            printf(">%s.INIT(Number of used bytes cannot be larger than total. Performing SPIFFS_check().)",THIS_ACTOR);
	            ret = esp_spiffs_check(conf.partition_label);

	            //esp_spiffs_info(conf.partition_label, &total, &used);


	            // Could be also used to mend broken files, to clean unreferenced pages, etc.
	            // More info at https://github.com/pellepl/spiffs/wiki/FAQ#powerlosses-contd-when-should-i-run-spiffs_check
	            if (ret != ESP_OK) {
	            	printf(">%s.INIT(SPIFFS_check() failed (%s))", THIS_ACTOR, esp_err_to_name(ret));
	                return ret;
	            }
//	            else {
//	            	printf("SPIFFS_check() successful");
//	            }
	      }
//	    if(ret == ESP_OK)
//	    	SPIFFS_para.SPIFFS_Status_u8 = SPIFFS_INITIALIZED;
	    return ret;
}

static void SPIFFS_file_unmnt(void)
{
		esp_vfs_spiffs_conf_t conf = {
		      .base_path = Spiffs_base_path,
		      .partition_label = "System_files",
		      .max_files = 5,
		      .format_if_mount_failed = true
		    };
		esp_vfs_spiffs_unregister(conf.partition_label);
		//printf("\n\n spiffs vol 2 is ummounted \n\n");
}

static int SPIFFS_file_write(char *file_name, char* data)
{
	char FileName[50] = {0};
	FILE* f;
	strcpy(FileName,Spiffs_base_path);
	strcat(FileName,file_name);

//	SPIFFS_file_mnt();
	f = fopen(FileName, "w");
	if (f == NULL)
	{
	   printf( "\n Failed to open file for writing");
	   return -1;
	}
	fprintf(f, data);
	fclose(f);
//	SPIFFS_file_unmnt();
	return 0;
}

static int SPIFFS_file_read(char *file_name, char *data)
{
	char FileName[50] = {0};
	strcpy(FileName,"/spiffs2/");
	strcat(FileName, file_name);
	FILE* f;

//	printf("1 FileName read = %s\n", FileName);
//	SPIFFS_file_mnt();

	f = fopen(FileName, "r");
	if (f == NULL)
	{
		// printf("Failed to open file '%s': %s\n", FileName, strerror(errno));
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
//	SPIFFS_file_unmnt();
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
void SPIFFS_file_write_variable(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	cJSON *Original_JSON = NULL;
	char filename[100] = {0}, retry = 0;
	char str[200] = {0};
	int ret = -1;

	int err = -1;
	int16_t Read_count 	=	0;
	in_JSON = cJSON_Parse((char*) (char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"1 Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
		}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcat(filename, name_JSON->valuestring);
	retry = 0;

	do
	{
		Read_count	= SPIFFS_file_read((char*)filename, DataPtr_Write);
		retry++;
		vTaskDelay(50);
	}while((Read_count <= 0) && (retry <3));

	if(Read_count > 0)
	{
		Original_JSON 	= cJSON_Parse(DataPtr_Write);
	}
	ret = update_json_object(&Original_JSON, in_JSON);

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
//		uint32_t offset =0;
		char filename[30] = {0};
//		char *  filedata;
		uint8_t Match_found_u8 = 0;
		uint16_t Read_count 	=	0;
//		uint64_t tickstart, tickend;
		char str[200] = {0};
//		char *DataPtr		= NULL;
		int retry_count = 0;
		memset(DataPtr_Read, 0, sizeof(DataPtr_Read));
		in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
			return;
			}
		memset(DataPtr_Read, 0, sizeof(DataPtr_Read));
		//strcpy(filename, (char*)JFS_Root_Path);
		name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
		if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
			strcpy(filename, name_JSON->valuestring);
		else
		{
			Add_Response_msg("Error! FILE_NAME is not given.",s_Message_Rx);
			return;
		}

		//printf("\n read var filename = %s",filename);
		root 	= cJSON_CreateObject();
//		DataPtr = (char*) heap_caps_calloc(MAX_CHUNK_SIZE, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//		if(DataPtr == NULL)
//		{
//			Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
////			free(str);
//			return;
//		}

//		tickstart = esp_timer_get_time();
//		Read_len_Prev =0;
		while (retry_count < 3)
		{
		Read_count	= SPIFFS_file_read((char*)filename, DataPtr_Read);
		//printf("\nDataPtr = %s\n", DataPtr);
		{
				head_JSON 	= in_JSON->child;    //take variable from payload
				head_JSON = head_JSON->next;  //point to 1st variable (format: Filename, var1, var2, ...)

				//loop

				do {
						//printf("\n head_JSON->string=%s\n", head_JSON->string);
//						Read_len_Prev = 0;
						Match_found_u8 = 0;
						while(1)
						{
							if(Read_count == 0)
							{
								Add_Response_msg("File is empty",s_Message_Rx);
								cJSON_Delete(in_JSON);
//								free(DataPtr);
								//free(payload);
								return;
							}


							if(Read_count !=0)
							{
								//printf("\n Read_count=%d", Read_count);
								in_JSON1 	= cJSON_Parse(DataPtr_Read);
								if (in_JSON1 == NULL)
								{
									sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
									Add_Response_msg(str, s_Message_Rx);
									goto exit;
								}
								out_JSON1 	= cJSON_CreateObject();
								head_JSON1 	= in_JSON1->child;

								//loop
								do {
									//	printf("\n\n head_JSON1->string = %s, head_JSON1->type=%d", head_JSON1->string, head_JSON1->type);
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
//										else
//										{
//											offset = strlen(payLoadData);
//										}

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
//	int16_t Read_count 	=	0;
	uint8_t filename[50] = {0}, filename_new[50] = {0};
	char json_file = 0;
//	char DataPtr[6*1024]		= {0};
	char str[200] = {0};
	FILE* f;
	int32_t file_size = 0;
	struct stat st;

	memset(DataPtr_Read, 0, sizeof(DataPtr_Read));
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
		strcpy((char*)filename_new, name_JSON->valuestring);
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
//		printf("file is json \n");
	}

	f = fopen((char*)filename, "r");
	if (f == NULL)
	{
		sprintf(str, "'%s' file is not present in SPIFFS used for system files.", name_JSON->valuestring);
		Add_Response_msg(str,s_Message_Rx);
//		SPIFFS_file_unmnt();
		cJSON_Delete(in_JSON);
		return;
	}
	cJSON_Delete(in_JSON);

	if (stat((char*)filename, &st) == 0) {
		file_size = (int32_t)st.st_size;
	}



	while(fgets(DataPtr_Read, MAX_CHUNK_SIZE, f) != NULL)
	{
		char* pos = strchr(DataPtr_Read, '\n');
		if (pos)
		{
			*pos = '\0';
		}

		if ((strlen(DataPtr_Read) < 10)  || (file_size <= 10))
		{

			sprintf(str, "'%s' file is empty in SPIFFS used for system files.", filename_new);
			Add_Response_msg(str,s_Message_Rx);
			fclose(f);
			return;
		}
		if(json_file == 1)   // file data is json
		{
			cJSON *responseObject =   cJSON_Parse((char*) DataPtr_Read);  // check whether file data is json or not
			if ((responseObject != NULL) && (cJSON_IsObject(responseObject)))
			{				
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				cJSON_Delete(responseObject);
				console_send_responce_to_console_xface(s_Message_Rx);	
			}
		}
		else // file data is string
		{
			cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
			cJSON_AddStringToObject(responseObject, "File Data", DataPtr_Read);
			memset(payLoadData,0,sizeof(payLoadData));//\0';
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
	}

	if ((strlen(DataPtr_Read) < 10)  || (file_size <= 10))
	{
		sprintf(str, "'%s' file is empty in SPIFFS used for system files.", name_JSON->valuestring);
		Add_Response_msg(str,s_Message_Rx);
//		SPIFFS_file_unmnt();
//		cJSON_Delete(in_JSON);
		fclose(f);
		return;
	}
	fclose(f);
//	cJSON_Delete(in_JSON);
}

static void SPIFFS_file_write_Parse(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
//	cJSON *head_JSON = NULL;
	int err = -1;
	char filename [50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	char *filedata = NULL;
	char str[200] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
		}
//	head_JSON 	= in_JSON->child;
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON!= NULL) && (name_JSON->valuestring != NULL))
		strcpy(filename,name_JSON->valuestring);
	else
	{
		Add_Response_msg("Error! FILE_NAME is null.",s_Message_Rx);
		return;
	}
	//printf("\n write filename=%s", filename);

	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_DATA");
	if(name_JSON!= NULL)
	{
		if (cJSON_IsString(name_JSON) && (name_JSON->valuestring != NULL))
		{
			memset(payLoadData,0,sizeof(payLoadData));
			strcpy(payLoadData, name_JSON->valuestring);
		}
		else
		{
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(name_JSON, payLoadData, sizeof(payLoadData), false);
		}
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
//	cJSON *head_JSON = NULL;
	int ret = -1;
	char filename [50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	char *filedata = NULL;
	char str[200] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
		}
//	head_JSON 	= in_JSON->child;
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

//	SPIFFS_file_mnt();
	ret = unlink(filename);
	if ( ret == 0)  // file to be deleted is present
	{
		sprintf(str, "%s file is deleted.", name_JSON->valuestring);
		Add_Response_msg(str,s_Message_Rx);
	}
	else
	{
		sprintf(str, "Error! %s file is not deleted (%s).",  name_JSON->valuestring , esp_err_to_name(ret));
		Add_Response_msg(str,s_Message_Rx);
	}
	cJSON_Delete(in_JSON);

//	SPIFFS_file_unmnt();
}


#define FILE_PATH_MAX 256  // Define a larger buffer size for file paths

static void Get_File_list(AMessage_st* s_Message_Rx)
{
    char directory[] = "/spiffs2";
    char str[300] = {0};
    DIR *dir = opendir(directory);
    if (dir == NULL) {
        sprintf(str, "Failed to open directory: %s", directory);
        Add_Response_msg(str, s_Message_Rx);
        return;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *jsonArrayFileName = cJSON_CreateArray();
    struct dirent *entry;
    char file_path[FILE_PATH_MAX];  // Larger buffer to hold full file path
    struct stat file_stat;         // Structure to store file details

    while ((entry = readdir(dir)) != NULL) {
        // Safely create the full file path
        int written = snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);
        if (written < 0 || written >= sizeof(file_path)) {
            sprintf(str, "Path too long for file: %s", entry->d_name);
            Add_Response_msg(str, s_Message_Rx);
            continue;
        }

        if (entry->d_type == DT_REG) { // If the entry is a regular file
            if (stat(file_path, &file_stat) == 0) { // Get file stats
                cJSON *file_object = cJSON_CreateObject();
                cJSON_AddStringToObject(file_object, "name", entry->d_name);
                cJSON_AddNumberToObject(file_object, "size", file_stat.st_size); // Add file size
                cJSON_AddItemToArray(jsonArrayFileName, file_object);
//                cJSON_AddItemToObject(root, "FILE", file_object);
            } else {
                sprintf(str, "Failed to stat file: %s", file_path);
                Add_Response_msg(str, s_Message_Rx);
            }
        } else if (entry->d_type == DT_DIR) { // If the entry is a directory
        	 cJSON_AddItemToArray(jsonArrayFileName, cJSON_CreateString(entry->d_name));
//            cJSON_AddStringToObject(root, "DIRECTORY", entry->d_name);
        } else {
        	 cJSON_AddItemToArray(jsonArrayFileName, cJSON_CreateString(entry->d_name));
//            cJSON_AddStringToObject(root, "OTHERS", entry->d_name);
        }
    }

    cJSON_AddItemToObject(root, "files", jsonArrayFileName);
    memset(payLoadData, 0, sizeof(payLoadData));
    cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
    strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

    cJSON_Delete(root);
    closedir(dir);
    console_send_responce_to_console_xface(s_Message_Rx);
//    strncpy((char*)s_Message_Rx->payload_p8, payLoadData, strlen(payLoadData));
    strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
}


//static void Get_File_list(AMessage_st* s_Message_Rx)
//{
//	char directory[] = "/spiffs2";
//	char str[50] = {0};
//    DIR *dir = opendir(directory);
//    if (dir == NULL) {
//    	sprintf(str,"Failed to open directory: %s", directory);
//    	Add_Response_msg(str,s_Message_Rx);
//        return;
//    }
//
//    cJSON *root = cJSON_CreateObject();
//    struct dirent *entry;
//    while ((entry = readdir(dir)) != NULL) {
//        if (entry->d_type == DT_REG) { // If the entry is a regular file
////            ESP_LOGI(TAG, "File: %s", entry->d_name);
//        	cJSON_AddStringToObject(root, "FILE", entry->d_name);
//        } else if (entry->d_type == DT_DIR) { // If the entry is a directory
//        	cJSON_AddStringToObject(root, "DIRECTORY", entry->d_name);
////            ESP_LOGI(TAG, "Directory: %s", entry->d_name);
//        } else {
////            ESP_LOGI(TAG, "Other: %s", entry->d_name);
//        	cJSON_AddStringToObject(root, "OTHERS", entry->d_name);
//        }
//    }
//	memset(payLoadData,0,sizeof(payLoadData));//\0';
//	cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
//	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//
//    cJSON_Delete(root);
//    closedir(dir);
//    console_send_responce_to_console_xface(s_Message_Rx);
//}


static void Copy_Files_SD_Card(AMessage_st* s_Message_Rx)
{
	cJSON *root = NULL;
	char str[100] = {0}, filename[100] = {0}, dest_filename[100] = {0}, File_Path[100] = {0};
	FILE* fptr;

//	printf("\n\n Copy_Files_SD_Card s_Message_Rx->payload_p8 = %s \n\n",s_Message_Rx->payload_p8);
	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}
	cJSON *filepath = cJSON_GetObjectItemCaseSensitive(root, "FILE_PATH");
	if ((filepath != NULL) && (cJSON_IsString(filepath)))
	{
		strcpy(File_Path, filepath->valuestring);
		strcat(File_Path, "/");
	}

	Get_File_list(s_Message_Rx);
//	printf("\n\n File list = %s \n\n", (char*)s_Message_Rx->payload_p8);

	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}

	cJSON *fileArray = cJSON_GetObjectItemCaseSensitive(root, "files");
	if ((fileArray != NULL) && (cJSON_IsArray(fileArray)))
	{
		int arraySize = cJSON_GetArraySize(fileArray);
//		printf("\n\n arraySize = %d \n\n",arraySize);
		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(fileArray, i);
			cJSON *file_name = cJSON_GetObjectItemCaseSensitive(element, "name");
//			printf("\n\n file_name->type = %d \n\n",file_name->type);
//			printf("\n\n file_name = %s \n\n",cJSON_Print(file_name));
			if (cJSON_IsString(file_name) && (file_name->valuestring != NULL))
			{
				if(strlen(file_name->valuestring) == 0)
					continue;
//				printf("\n\n file_name->valuestring = %s \n\n",file_name->valuestring);
				memset(filename, 0, sizeof(filename));
				memset(dest_filename, 0, sizeof(dest_filename));

				strcpy((char*)dest_filename, "B:/");
				strcat((char*)dest_filename, File_Path);
				strcat((char*)dest_filename, file_name->valuestring);

				strcpy((char*)filename, Spiffs_base_path);
				strcat((char*)filename, file_name->valuestring);
				fptr = fopen((char*)filename, "r");
				if (fptr == NULL)
				{
					sprintf(str, "Error! cannot open '%s' file. Error is %s", file_name->valuestring, strerror(errno));
					Add_Response_msg(str,s_Message_Rx);
					continue;
				}
				while(fgets(DataPtr_Read, MAX_CHUNK_SIZE, fptr) != NULL)
				{
					char* pos = strchr(DataPtr_Read, '\n');
					if (pos)
					{
						*pos = '\0';
					}
					cJSON *write_data = cJSON_CreateObject();
					cJSON_AddStringToObject(write_data, "FILE_NAME", dest_filename);
					cJSON_AddStringToObject(write_data, "FILE_DATA", DataPtr_Read);
//					memset(payLoadData,0,sizeof(payLoadData));//\0';
					payLoadData[0] = '\0';
					cJSON_PrintPreallocated(write_data, payLoadData, sizeof(payLoadData), false);
					cJSON_Delete(write_data);
					Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE");
				}
				fclose(fptr);  // close the file
			}
			 vTaskDelay(200/ portTICK_PERIOD_MS);
		}
		cJSON_Delete(root);

		Add_Response_msg("All files are copied to SD card",s_Message_Rx);
		Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE");
		return;
	}
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



static void COPY_SPIFFS_JFS_proc(AMessage_st* s_Message_Rx)
{
	cJSON *root = NULL;
	char str[100] = {0}, filename[100] = {0}, dest_filename[100] = {0};// File_Path[100] = "Root/";
	FILE* fptr;

//	printf("\n\n Copy_Files_SD_Card s_Message_Rx->payload_p8 = %s \n\n",s_Message_Rx->payload_p8);
//	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
////	if (root == NULL) {
////		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
////		Add_Response_msg(str,s_Message_Rx);
////		return;
////	}
////	cJSON *filepath = cJSON_GetObjectItemCaseSensitive(root, "FILE_PATH");
////	if ((filepath != NULL) && (cJSON_IsString(filepath)))
////	{
////		strcpy(File_Path, filepath->valuestring);
////		strcat(File_Path, "/");
////	}

	Get_File_list(s_Message_Rx);
//	printf("\n\n File list = %s \n\n", (char*)s_Message_Rx->payload_p8);

	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		return;
	}

	cJSON *fileArray = cJSON_GetObjectItemCaseSensitive(root, "files");
	if ((fileArray != NULL) && (cJSON_IsArray(fileArray)))
	{
		int arraySize = cJSON_GetArraySize(fileArray);
		//printf("\n\n arraySize = %d \n\n",arraySize);
		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(fileArray, i);
			cJSON *file_name = cJSON_GetObjectItemCaseSensitive(element, "name");
//			printf("\n\n file_name->type = %d \n\n",file_name->type);
//			printf("\n\n file_name = %s \n\n",cJSON_Print(file_name));
			if (cJSON_IsString(file_name) && (file_name->valuestring != NULL))
			{
				if(strlen(file_name->valuestring) == 0)
					continue;
//				printf("\n\n file_name->valuestring = %s \n\n",file_name->valuestring);
				memset(filename, 0, sizeof(filename));
				memset(dest_filename, 0, sizeof(dest_filename));

				strcpy((char*)dest_filename, "A:/");
//				strcat((char*)dest_filename, File_Path);
				strcat((char*)dest_filename, file_name->valuestring);

				strcpy((char*)filename, Spiffs_base_path);
				strcat((char*)filename, file_name->valuestring);
				fptr = fopen((char*)filename, "r");
				if (fptr == NULL)
				{
					sprintf(str, "Error! cannot open '%s' file. Error is %s", file_name->valuestring, strerror(errno));
					Add_Response_msg(str,s_Message_Rx);
					continue;
				}
				memset(DataPtr_Read,0, sizeof(DataPtr_Read));
				while(fgets(DataPtr_Read, MAX_CHUNK_SIZE, fptr) != NULL)
				{
					char* pos = strchr(DataPtr_Read, '\n');
					if (pos)
					{
						*pos = '\0';
					}
					cJSON *write_data = cJSON_CreateObject();
					cJSON_AddStringToObject(write_data, "FILE_NAME", dest_filename);
					cJSON_AddStringToObject(write_data, "FILE_DATA", DataPtr_Read);
//					memset(payLoadData,0,sizeof(payLoadData));//\0';
					payLoadData[0] = '\0';
					cJSON_PrintPreallocated(write_data, payLoadData, sizeof(payLoadData), false);
					cJSON_Delete(write_data);
					//printf("\n SPIFFS payLoadData = %s \n",payLoadData);
					Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE");
				}
				fclose(fptr);  // close the file
			}
			 vTaskDelay(200/ portTICK_PERIOD_MS);  //500
		}
		cJSON_Delete(root);

		if(arraySize != 0)
		{
			Add_Response_msg("All files are copied to JFS. ",s_Message_Rx);  //Now resetting the ESP...
		}
//		vTaskDelay(10000/portTICK_PERIOD_MS);  // wait for completing file copy process
//		Restart_ESP_Xface(1);
		return;
	}
}

////------------------------ SYS_FILES Actor Methods End  ------------------------------------------------------------------------//



