/*
 * JFS_NOR_Actor.c
 *
 *  Created on: 21-Oct-2022
 *  Author: Amruta Dixit, Suraj Kushwaha
 */

#include "actor.h"
#include "Config.h"
#include "JFS_FLASH_Actor.h"
#include "console_Actor.h"
#include "esp_ota_ops.h"
#include "FS.h"
#include "rtc_wdt.h"
#include "SEGGER.h"
#include "esp_timer.h"
#include <assert.h>
#include "esp_partition.h"
#include "esp_app_format.h"
#include "File_Manager_Actor.h"

#define JFS_Root_Path "Root/" // Adjust this according to your file system
#define MAX_RETRIES 3 // Maximum number of retries for critical operations
#define JFS_READ_TIMEOUT_SECONDS 10
#define DELAY_BETWEEN_RETRIES_MS 500      // Delay between retries in milliseconds

static StaticQueue_t Monitor_pxQueueBuffer;
//------------------------ JFS Actor Resources -------------------------------------------------//
static SemaphoreHandle_t JFSHPWriteMutex = NULL;

static void JFS_FLASH_Rename_File(const char *sNameOld,	const char *sNameNew, AMessage_st* s_Message_Rx);				// Rename file in JFS
static void JFS_FLASH_Rename_Dir(const char *sNameOld	, const char *sNameNew, AMessage_st* s_Message_Rx);			// Rename Dir in JFS
static void Delete_folders(const char *path);							//Display the NOR flash memory information
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void sendLedState(const char *stateName, int duration);
static void COPY_JFS_SPIFFS_proc(AMessage_st* s_Message_Rx);
//----------------------------- Actor Tags -----------------------------------------------------//
void vWriteTask(void *pvParameters) ;
void InitWriteQueue() ;
static char 		*THIS_ACTOR 	= "FILE_SYSTEM";
//-------------------------- Actor Parameters --------------------------------------------------//

//static uint32_t MAX_Log_File_Size_u32 = 1.2 * 1024 * 1024;
#define MAX_Log_File_Size_u32   (1258291UL)
static uint32_t MAX_Event_Log_File_Size_u32 = 200 * 1024;
static uint32_t MAX_WIFI_Log_File_Size_u32 = 100 * 1024;
//-------------------------- Common Actor Resources ------------------------------//
char FirstTime=1;  //*Filename,
uint32_t File_length = 0;
static uint32_t gFileSize = 0x0FFFFFF;
extern TaskHandle_t Read_Handle;
static TaskHandle_t WriteTaskHandle = NULL;
BaseType_t JFS_WRITE;
typedef struct {
    char File_name[100];
    char *data;
    int16_t size;
    char OpenType[10];
    uint8_t Send_Ack;
} WriteMessage;

PSRAM_ATTR_BSS  static uint8_t Monitor_pucQueueStorage [100 * sizeof(WriteMessage)];
PSRAM_ATTR_BSS  static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char payLoadData_File_List[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char DataPtr_Write[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char DataPtr_Read[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char DataPtr_Chunk_Read[2200];
PSRAM_ATTR_BSS  static char ota_write_data [JSON_FILE_SIZE];
PSRAM_ATTR_BSS static char payLoadData_Debug[MAX_JSON_PAYLOAD_BYTES];

// Define the queue handle
static QueueHandle_t xWriteQueue = NULL;

void JFS_actor_setup() {
	 int          r1, r2, r3, r4;
	 uint32_t  Journal_Size;

	FS_Init();			// Initialize the NOR flash memory
	FS_MountEx(VOLUME_NAME, FS_MOUNT_RW);  // mount the device
	FS_FAT_SupportLFN();  // Support for long file names
	FS_SetFileWriteMode(FS_WRITEMODE_SAFE);     // The journal works correctly only when the file write mode is set to SAFE.
	if(FS_FormatLLIfRequired(VOLUME_NAME) == 0)// Check if volume needs to be low level formatted.
	{
		printf("Low level formatting\n");
		(void) FS_FormatLow(VOLUME_NAME);
	}

	if (FS_IsHLFormatted(VOLUME_NAME) == 0)
	{
		FS_X_Log("High level formatting\n");
		int ret =  FS_Format(VOLUME_NAME, NULL);
		if(ret == 0)
			printf("\n\n JFS is formatted successfully.. \n\n");
		else
			printf("\n\n JFS is not formatted successfully..\n\n");
	}

#if FS_SUPPORT_JOURNAL
	Journal_Size = 512 * 1024; //16 * 1024;(reboot) // 128 * 1024;(5 bytes mismatch) //32 * 1024;(2 bytes mismatch) //64 * 1024;(5/10 FOTA pass) //((1024 * 1024) + (400*1024));
	r1 =  FS_JOURNAL_CreateEx(VOLUME_NAME, Journal_Size,1);
#endif

	r3 = FS_CreateDir("Root");
	if (r3 < 0) {
#ifdef ENABLE_PRINT_MSG
		printf("Error! Create Dir Audit [%d]:[%s]. \n", r3,FS_ErrorNo2Text(r3));
#endif
	}

	r1 = FS_CreateDir("Root/Audit");
	if (r1 < 0) {
#ifdef ENABLE_PRINT_MSG
		printf("Error! Create Dir Audit [%d]:[%s]. \n", r1,FS_ErrorNo2Text(r1));
#endif
	}

	r2 = FS_CreateDir("Root/System");
	if (r2 < 0) {
#ifdef ENABLE_PRINT_MSG
		printf("Error! Create Dir System[%d]:[%s].\n", r2,FS_ErrorNo2Text(r2));
#endif
	}

	r4 = FS_CreateDir("Root/Database");
	if (r4 < 0) {
#ifdef ENABLE_PRINT_MSG
		printf("Error! Create Dir System[%d]:[%s].\n", r4,FS_ErrorNo2Text(r4));
#endif
	}

	if((r1 >= 0) && (r2 >= 0) && (r3 >= 0) && (r4 >= 0)){
#ifdef ENABLE_PRINT_MSG
	printf("\n\n JFS is initialized.\n\n ");
#endif
	}
	 InitWriteQueue() ;
}//	actor_setup

//------------------------ JFS Actor Methods Starts ------------------------------------------------------------------------//

void JFS_Rename(const char *payload_p8, AMessage_st* s_Message_Rx){

	cJSON *my_JSON 		= NULL;
	cJSON *myName_JSON 	= NULL;
	char str[100] = {0};

	my_JSON = cJSON_Parse((char*) payload_p8);
	if (my_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  return;
		}
	char Type [30] = {0}, OldName[50] = {0}, NewName[50] = {0};		//	= (char*) heap_caps_calloc(JFS_LOG_FILE_NAME_LEN, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

	myName_JSON = cJSON_GetObjectItem(my_JSON, "TYPE");
	strcpy(Type,myName_JSON->valuestring);

	myName_JSON = cJSON_GetObjectItem(my_JSON, "OLD_NAME");
	strcpy(OldName,myName_JSON->valuestring);

	myName_JSON = cJSON_GetObjectItem(my_JSON, "NEW_NAME");
	strcpy(NewName,myName_JSON->valuestring);

	if (!strcasecmp(Type, "FILE")) {
		if(strcasecmp((char*)OldName,"Audit/Audit_Log.txt")==0)
		{
			Add_Response_msg("Cannot rename Audit log file.",s_Message_Rx, payLoadData);
			cJSON_Delete(my_JSON);
			return;
		}
		JFS_FLASH_Rename_File(OldName, NewName,s_Message_Rx);	// Rename an existing File to the JFS flash memory
	}
	if (!strcasecmp(Type, "DIR")) {
		JFS_FLASH_Rename_Dir(OldName, NewName,s_Message_Rx);		// Rename an existing File to the JFS flash memory
	}
	cJSON_Delete(my_JSON);
}//	JFS_Rename


void JFS_SampleGetVolumeInfo(AMessage_st* s_Message_Rx) { //Display the NOR flash memory information
	FS_DISK_INFO VolumeInfo;
	int r;
	char sFSType[20] = {0};
	memset(&VolumeInfo, 0, sizeof(VolumeInfo));
	r = FS_GetVolumeInfo("nor:0:", &VolumeInfo); // get the NOR flash memory information
	if (r == 0) {
		cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
		cJSON_AddNumberToObject(responseObject, "Total clusters", VolumeInfo.NumTotalClusters);  //display the total number of clusters.
		cJSON_AddNumberToObject(responseObject, "Free clusters", VolumeInfo.NumFreeClusters);   //display the Number of free clusters
		cJSON_AddNumberToObject(responseObject, "Available Memory", FS_GetVolumeFreeSpace("nor:0:"));  // Get free space of JFS
		switch (VolumeInfo.FSType) {
		case FS_TYPE_FAT12:
			strcpy(sFSType,"FAT12");
			break;
		case FS_TYPE_FAT16:
			strcpy(sFSType,"FAT16");
			break;
		case FS_TYPE_FAT32:
			strcpy(sFSType,"FAT32");
			break;
		case FS_TYPE_EFS:
			strcpy(sFSType,"EFS");
			break;
		default:
			strcpy(sFSType,"Unknown");
			break;
		}
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(responseObject);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
		Add_Response_msg("Error! Cannot find volume information.",s_Message_Rx, payLoadData);
}// JFS_SampleGetVolumeInfo

//--------------------------- JFS Actor Methods Ends -----------------------------------------//


void JFS_FLASH_Create_Dir(const char *Dir_name,AMessage_st* s_Message_Rx){

	uint8_t out [64] =  {0}; //(uint8_t*) heap_caps_calloc(64,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	FS_FAT_SupportLFN();    // support for long file name
	char dir_path[100] = {0};
	strcpy(dir_path, JFS_Root_Path);
	strcat(dir_path, Dir_name);
	int err = FS_CreateDir(dir_path);
	if (err < 0) {
		sprintf((char*)out, "Error! Create Dir [%d]:[%s].", err,FS_ErrorNo2Text(err));
	}
	else if (err == 1) {
			sprintf((char*)out, "Directory already exist.");
	}
	else {
		sprintf((char*)out, "[%s]: Directory is created.", Dir_name);
	}
	Add_Response_msg((char*)out,s_Message_Rx, payLoadData);
}//	JFS_FLASH_Create_Dir

void JFS_FLASH_Delete_Dir(const char *Dir_name, AMessage_st* s_Message_Rx){

	FS_FAT_SupportLFN();    // support for long file name
	char out [100] = {0}, dir_path[100] = {0};
	strcpy(dir_path, JFS_Root_Path);
	strcat(dir_path, Dir_name);
	int err 	 = FS_DeleteDir(dir_path,1); // here 2nd arg 1 is the directory delete level
	if(err != 0){
		sprintf((char*)out, "Error! Delete Dir [%d]:[%s].", err,FS_ErrorNo2Text(err));
	}
	else {
		sprintf((char*)out, "[%s] : Dir Deleted.", Dir_name);
	}
	Add_Response_msg((char*)out,s_Message_Rx, payLoadData);
}//	JFS_FLASH_Delete_Dir

static void JFS_FLASH_Rename_Dir(const char *sNameOld	, const char *sNameNew, AMessage_st* s_Message_Rx){
	char out [100]= {0},  dir_path_old[100] = {0};
	strcpy(dir_path_old, JFS_Root_Path);
	strcat(dir_path_old, sNameOld);
	int err = FS_Rename(dir_path_old, sNameNew);
	if (err != 0) {
		sprintf((char*)out, "Error! Rename Dir [%d]:[%s].", err,FS_ErrorNo2Text(err));
	} else {
		sprintf((char*) out, "[%s], Dir Renamed.", sNameNew);
	}
	Add_Response_msg((char*)out,s_Message_Rx, payLoadData);
}//	JFS_FLASH_Rename_Dir

void InitWriteQueue() {
	JFSHPWriteMutex = xSemaphoreCreateMutex();
	if (JFSHPWriteMutex == NULL)
	{
		ESP_LOGE("initialize_mutexes", "Failed to create mutex for JFS actor");
	}

	if (xWriteQueue == NULL) {
		xWriteQueue = xQueueCreateStatic(100, sizeof(WriteMessage), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
	}
	if (xWriteQueue != NULL) {
        JFS_WRITE = xTaskCreate(vWriteTask, (const char*) "Write Task", FILE_MANAGER_WRITE_TASK_STACK_DEPTH, NULL, FILE_MANAGER_TASK_PRIORITY, &WriteTaskHandle);
        if (WriteTaskHandle == NULL) {
        	 printf("Failed to create write task.\n");
        }
    } else {
        printf("Failed to create queue.\n");
    }
}
static void Extract_folders(const char *path) {
    // Make a copy of the path to tokenize it (strtok modifies the string)
    char path_copy[100] = {0}, Dir_name[100] ={0};
    strcpy(Dir_name,"Root");
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0'; // Ensure null termination

    // Tokenize the string by '/'
    char *token = strtok(path_copy, "/");

    // Iterate through all tokens except the last one (file name)
    while (token != NULL) {
    	strcat(Dir_name, "/");
        // Find the next token
        char *next_token = strtok(NULL, "/");

        // If next_token is NULL, we are at the last token (file name)
        if (next_token == NULL) {
            break;
        }

        // Print the folder name
        strcat(Dir_name, token);
        int err = FS_CreateDir(Dir_name);
		if (err < 0) {
			#ifdef ENABLE_PRINT_MSG
			printf("Error! Create Dir [%d]:[%s].\n", err,FS_ErrorNo2Text(err));
			#endif
		}
        // Move to the next token
        token = next_token;
    }
}

int WrittenBytes = 0;

int JFS_FLASH_Write_File_HP(char *File_name, const char *data, int16_t size, char *OpenType, uint8_t Send_Ack) {

	WriteMessage msg = {0};
	int result=-1;

    #ifdef ENABLE_PRINT_MSG
    printf("Queueing write request for file: %s\n", File_name);
    #endif
    strncpy(msg.File_name, File_name, sizeof(msg.File_name));

    #ifdef ENABLE_PRINT_MSG
    printf("Memory allocated in PSRAM.\n");
    #endif

    msg.size = size;
    strncpy(msg.OpenType, OpenType, sizeof(msg.OpenType));
    msg.Send_Ack = Send_Ack;

    #ifdef ENABLE_PRINT_MSG
    printf("Queued Item: File_name: %s, Size: %d, OpenType: %s, Offset: %lu\n", msg.File_name, msg.size, msg.OpenType, msg.Offset);
    #endif
    Extract_folders(msg.File_name);  // create folder if not exits

    result = JFS_FLASH_Write_File1(msg.File_name, data, msg.size, msg.OpenType, msg.Send_Ack);
    if(msg.Send_Ack == 1)
   			{
				AMessage_st s_Message_Rx_New;
				strcpy((char*)s_Message_Rx_New.dest_Actor_a8, "FILE_SYSTEM");
				strcpy((char*)s_Message_Rx_New.src_Actor_a8, "HTTP");
				strcpy((char*)s_Message_Rx_New.cmdFun_a8, "WRITE_FILE1");
				cJSON *out_JSON 	= cJSON_CreateObject();
				char resp[50] = {0};
				s_Message_Rx_New.payload_p8 = (uint8_t*) resp;
				if(out_JSON != NULL)
				{
					if(result == ESP_OK)
						cJSON_AddNumberToObject(out_JSON, "JFS_Resp", E_JFS_RESP_OK);
					else
						cJSON_AddNumberToObject(out_JSON, "JFS_Resp", E_JFS_RESP_ERR);										
		
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
					strcpy((char*)s_Message_Rx_New.payload_p8, payLoadData);
					cJSON_Delete(out_JSON);
					console_send_responce_to_console_xface(&s_Message_Rx_New);
				}
		else
			Add_Response_msg("Error! Failed to allocate memory for sending write response.",&s_Message_Rx_New, payLoadData);
}


    // Check and log the number of messages in the queue
	#ifdef ENABLE_PRINT_MSG
    UBaseType_t messages_waiting = uxQueueMessagesWaiting(xWriteQueue);
    printf("Number of requests currently queued: %d\n", messages_waiting);
    #endif

    #ifdef ENABLE_PRINT_MSG
    printf("Write request queued successfully.\n");
    #endif
    return 0;
}


int JFS_FLASH_Write_File(char *File_name, const char *data, int16_t size, char *OpenType, uint8_t Send_Ack, char* Payload) {
    WriteMessage msg = {0};

    #ifdef ENABLE_PRINT_MSG
    printf("Queueing write request for file: %s\n", File_name);
    #endif

//    Extract_folders(File_name);  // create folder if not exits
    strncpy(msg.File_name, File_name, sizeof(msg.File_name));
    msg.data = Payload; //data_buff;

    #ifdef ENABLE_PRINT_MSG
    printf("Memory allocated in PSRAM.\n");
    #endif
    memcpy(msg.data, data, size); // Copy data to PSRAM
    msg.size = size;
    strncpy(msg.OpenType, OpenType, sizeof(msg.OpenType));
    msg.Send_Ack = Send_Ack;

    #ifdef ENABLE_PRINT_MSG
    printf("Queued Item: File_name: %s, Size: %d, OpenType: %s, Offset: %lu\n", msg.File_name, msg.size, msg.OpenType, msg.Offset);
    #endif

    if (xQueueSend(xWriteQueue, &msg, 1) != pdPASS)  //portMAX_DELAY
    {
        #ifdef ENABLE_PRINT_MSG
        printf("Failed to send message to queue, freed allocated memory.\n");
        #endif
        return -1; // Queue full or error
    }
    return 0;
}

int JFS_FLASH_Write_File1(char *File_name, const char *data, int16_t size, char *OpenType, uint32_t Offset) {
    char Filename[100] = {0};
    int count = 0;
    int result = 0;
    int retry;

    // Construct the full path for the file
    if (xSemaphoreTake(JFSHPWriteMutex, portMAX_DELAY) == pdTRUE)
	 {
		strncpy(Filename, JFS_Root_Path, sizeof(Filename) - 1);
		strncat(Filename, File_name, sizeof(Filename) - strlen(Filename) - 1);


		FS_FILE *pFile = NULL;
		FS_FAT_SupportLFN(); // Enable support for long file names


		// Attempt to open the file with retries
		for (retry = 0; retry < MAX_RETRIES; retry++) {
			int ret = FS_FOpenEx(Filename, OpenType, &pFile);
			if (pFile != NULL) {
				break; // Successful open
			}
			 if (ret == FS_ERRCODE_FILE_IS_OPEN)
			 {
				 vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
			 }
			 else
			 {
				 printf("\n Error! Failed to open  %s file. Error is '%s'. \n", Filename, FS_ErrorNo2Text(ret));
				 break;
			 }
		}

		if (pFile == NULL) {
			printf("Failed to open file after %d attempts: %s\n", MAX_RETRIES, Filename);
			xSemaphoreGive(JFSHPWriteMutex);
			return -1;
		}

		// Write data to the file with retries
			count = FS_FWrite(data, 1, size, pFile);
			FS_SyncFile(pFile);
			if (count == size) {
				#ifdef ENABLE_PRINT_MSG
				printf("Successfully wrote %d bytes to file: %s\n", count, Filename);
				#endif
				WrittenBytes += count;
			}

		if (count != size) {
			printf("Failed to write data to file: %s, count = %d, size = %d,\n", Filename, count, size);
			result = -1;
		}
		if (FS_FClose(pFile) != 0)
		{
			result = -1;
		}
			xSemaphoreGive(JFSHPWriteMutex);
	 }
    return result;
}

#define SEGMENT_SIZE 8192 // Size of each segment for deleting files

void JFS_FLASH_Delete_File(const char *payload, AMessage_st* s_Message_Rx) {
    cJSON *root_JSON = NULL;
    cJSON *head_JSON = NULL;
    char str[100] = {0};
    int err = 0;
    uint8_t out[150] = {0}; // (uint8_t*) heap_caps_calloc(64, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    char FileName [100]= {0}; // (char*) heap_caps_calloc(100, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    int retry;

    // Construct the full path for the file
    strcpy(FileName, JFS_Root_Path);

    // Parse the JSON payload
    root_JSON = cJSON_Parse(payload);
    if (root_JSON == NULL) {
    	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        return;
    }

    // Extract the file name from the JSON
    head_JSON = cJSON_GetObjectItem(root_JSON, "FILE_NAME");
    if (head_JSON == NULL || head_JSON->valuestring == NULL) {
        sprintf(str, "File name not found in JSON input.");
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        cJSON_Delete(root_JSON);
        return;
    }

    strcat(FileName, head_JSON->valuestring);

    // Delete the file
    for (retry = 0; retry < MAX_RETRIES; retry++) {
        err = FS_Remove(FileName);
        if (err == 0) {
            sprintf((char*)out, "[%s], File Deleted.", FileName);
            Add_Response_msg((char*)out, s_Message_Rx, payLoadData);
            break; // Successfully deleted file
        }
        vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
    }

    if (retry == MAX_RETRIES) {
        sprintf((char*)out, "Error! Unable to delete %s file after %d attempts (%s).", FileName, MAX_RETRIES, FS_ErrorNo2Text(err));
        Add_Response_msg((char*)out, s_Message_Rx, payLoadData);
    }
}

/**
* @brief Renames a file in the file system, with retries for any error and attempts to close the file after 10 retries.
*
* @param sNameOld The old name of the file to rename.
* @param sNameNew The new name for the file.
* @param s_Message_Rx Pointer to the message structure to send responses back.
*/
static void JFS_FLASH_Rename_File(const char *sNameOld, const char *sNameNew, AMessage_st* s_Message_Rx) {
    char dir_path_old[100] = {0};         // Buffer to hold the full path of the old file
    char out[100] = {0};                  // Pointer for the response message
    int err = -1;                         // Variable to hold the error code for the rename operation
    int retries = 0;                      // Counter to track the number of retries
    // Step 1: Construct the full path for the old file name
    strcpy(dir_path_old, JFS_Root_Path);  // Copy the root path to the old file path buffer
    strcat(dir_path_old, sNameOld);       // Append the old file name to the root path

    // Step 3: Retry the file rename operation up to MAX_RETRIES times
    while (retries < MAX_RETRIES)
    {
        err = FS_Rename(dir_path_old, sNameNew);  // Attempt to rename the file
        if (err == 0) {
            // If renaming is successful, prepare the success message and break out of the loop
            sprintf((char*) out, "File renamed to [%s] after [%d] retries.", sNameNew, retries);
            break;
        }
        else {
				 if (err == FS_ERRCODE_FILE_IS_OPEN)
				 {
					retries++;
					vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
				 }
				 else
				 {
					 sprintf((char*) out, "Error! failed to rename %s file. Error is '%s'.", sNameOld, FS_ErrorNo2Text(err));
					 break;
				 }
        }
    }
    if (retries == MAX_RETRIES && err != 0)
    {
    	sprintf((char*) out, "Error! failed to rename %s file after %d retries. Error is '%s'.", sNameOld, retries, FS_ErrorNo2Text(err));
    }
    // Step 6: Send the response message containing the result of the operation
    Add_Response_msg((char*)out, s_Message_Rx, payLoadData);
}

int  JFS_FLASH_Read_File(char *File_name, char* File_data, uint16_t Read_data_len, uint32_t *file_ptr )	// read from the JFS flash memory
{

	FS_FILE * pFile 	= 	NULL;
	char Filename[100] = {0};
	strcpy(Filename, JFS_Root_Path);
	strcat(Filename, File_name);
	FS_FAT_SupportLFN();    // support for long file name
	FS_FOpenEx(Filename, "r", &pFile);		// Open the file in read mode
	if (pFile == NULL) {
		return (-1);
	}

	if(FirstTime==1)  //Read the file first time for COP communication
	{
		FirstTime	=	0;
		gFileSize = FS_GetFileSize(pFile);  //Get the file size
		if(gFileSize == 0xFFFFFFFF)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Error! File size [%ld].\n", gFileSize);
#endif
			FS_FClose(pFile);
			return (0);
		}
	}

   	uint32_t Read_len 		= 0;
	FS_FSeek(pFile, *file_ptr, FS_SEEK_SET);  //set the file position
	memset(File_data,0,Read_data_len);    // This creates problem and code restarts

	Read_len = FS_FRead((char*) File_data, 1, Read_data_len, pFile); // Read data from JFS Flash memory
	*file_ptr	=	*file_ptr	+	Read_len;
   	FS_FClose(pFile);
   	return(Read_len);
}//	JFS_FLASH_Read_File

void OTA_JFS_task(AMessage_st* s_Message_Rx_New)
 {
 	AMessage_st s_Message_Rx_data;
 	AMessage_st *s_Message_Rx = &s_Message_Rx_data;
 	char buffer[512] = {0};
 	strcpy((char*)s_Message_Rx->src_Actor_a8, (char*) s_Message_Rx_New->src_Actor_a8);
	strcpy((char*)s_Message_Rx->dest_Actor_a8, (char*) s_Message_Rx_New->dest_Actor_a8);
	strcpy((char*)s_Message_Rx->cmdFun_a8, (char*) s_Message_Rx_New->cmdFun_a8);
	s_Message_Rx->payload_p8 = (uint8_t*)buffer;
	strcpy((char*)s_Message_Rx->payload_p8, (char*) s_Message_Rx_New->payload_p8);
     esp_err_t err;
     char Filename[100] = {0}, File_Name[100] = {0}, count = 0;
     cJSON *in_JSON = NULL;
     cJSON *name_JSON = NULL;
     /* update handle : set by esp_ota_begin(), must be freed via esp_ota_end() */
     esp_ota_handle_t update_handle = 0 ;
     const esp_partition_t *update_partition = NULL;
     FS_FILE * pFile 	= 	NULL;
 	char str[200] = {0}, found = 0; // retry = 0;
 	uint32_t File_size = 0, Remaining_File_size = 0;
 	char fm_version[20] = {0};
 	char JFS_SD = 1;   // if JFS_SD = 1, then use JFS, if JFS_SD = 0, then use SD card for firmware update.
 	int i =0, data_read = 0;
 	FILE *fileptr = NULL;
 	struct stat st;
 	int retry_count = 3;
 	esp_app_desc_t new_app_info;
 	esp_app_desc_t running_app_info;
 	char ESP_Reset_flag = 1;

 	in_JSON 	  = cJSON_Parse((char*)s_Message_Rx->payload_p8);
 	if (in_JSON == NULL) {
 		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
 		Add_Response_msg(str,s_Message_Rx, payLoadData);
     	return;
 		}
 	name_JSON 	  = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
 	if((name_JSON != NULL) && (name_JSON->valuestring)!= NULL)
 		strcpy(File_Name, name_JSON->valuestring);
 	else
 	{
 		Add_Response_msg("Kindly give valid key as \"FILE_NAME\"",s_Message_Rx, payLoadData);
 		cJSON_Delete(in_JSON);
     	return;
 	}
 	cJSON_Delete(in_JSON);

 	if(strncasecmp(File_Name,"A:/",3) == 0)
 	{
 		JFS_SD = 1;
 	}
 	else if(strncasecmp(File_Name,"B:/",3) == 0)
 	{
 		JFS_SD = 0;
 	}
 	else
 	{
 		printf("Enter correct file path. Use drive 'A:/' for JFS and 'B:/' for SD card. \n");
     	return;
 	}

 	const char *dot = strrchr((char*)File_Name, '.');
 	// Check if the dot is present and if it is followed by "bin"
 	if ((dot == NULL) || ((strcmp(dot + 1, "bin") != 0)))
 	{
 		Add_Response_msg("Received file is not a binary file",s_Message_Rx, payLoadData);
     	return;
 	}
    for(i =0; i< strlen(File_Name); i++)
 	{
 		if(File_Name[i] == '/')
 		{
 			found = 1;
 			break;
 		}
 	}
 	if(found == 0)
 	{
 		Add_Response_msg("Enter correct file path. Use drive 'A:/' for JFS and 'B:/' for SD card.",s_Message_Rx, payLoadData);
     	return;
 	}
     for (int attempt = 0; attempt < retry_count; attempt++) {
         if (JFS_SD == 1) {
             // Construct the full path for the JFS file
             strcpy(Filename, JFS_Root_Path);
             strcat(Filename, File_Name + i + 1);
             pFile = FS_FOpen(Filename, "r");
             if (pFile != NULL) {
                 File_size = FS_GetFileSize(pFile); // Get the size of the file
                 break;  // Exit the retry loop if successful
             }
         } else {
             // Construct the full path for the SD card file
             strcpy(Filename, MOUNT_POINT);
             strcat(Filename, "/");
             strcat(Filename, File_Name + i + 1);
             fileptr = fopen(Filename, "r");
             if (fileptr != NULL && stat(Filename, &st) == 0) {
                 File_size = st.st_size; // Get the size of the file
                 Remaining_File_size = File_size;
                 break;  // Exit the retry loop if successful
             }
         }

         // If the file could not be opened, retry after a short delay
         if (attempt == retry_count - 1) {
             Add_Response_msg("Failed to open file after multiple attempts.", s_Message_Rx, payLoadData);
          	return;
         }
         vTaskDelay(1000 / portTICK_PERIOD_MS);  // Wait before retrying
     }
     const esp_partition_t *configured = esp_ota_get_boot_partition();
     const esp_partition_t *running = esp_ota_get_running_partition();

     if (configured != running) {
     	sprintf(str,"Error! Configured OTA boot partition at offset 0x%08ld, but running from offset 0x%08ld.This can happen if either the OTA boot data or preferred boot image become corrupted somehow.",configured->address, running->address );
     	Add_Response_msg(str,s_Message_Rx, payLoadData);
       }

 #ifdef CONFIG_EXAMPLE_SKIP_COMMON_NAME_CHECK
     config.skip_cert_common_name_check = true;
 #endif

     update_partition = esp_ota_get_next_update_partition(NULL);
     assert(update_partition != NULL);

     /*deal with all receive packet*/
     bool image_header_was_checked = false;
     File_length=0;
     while (1)
     {
       	if(JFS_SD == 1)
       	{
 			data_read = FS_FRead((char*) ota_write_data, 1, JSON_FILE_SIZE, pFile); // Read data from JFS Flash memory
       	}
 		else
 		{
 			if(Remaining_File_size > JSON_FILE_SIZE)
 				data_read = SD_card_Read_Chunk((char*) ota_write_data, JSON_FILE_SIZE, fileptr);
 			else
 				data_read = SD_card_Read_Chunk((char*) ota_write_data, Remaining_File_size, fileptr);
 			Remaining_File_size = Remaining_File_size - data_read;
 		}
         if (data_read < 0)
         {
         	Add_Response_msg("No data available for reading",s_Message_Rx, payLoadData);
             if(JFS_SD == 1)
             	FS_FClose(pFile);  // close JFS file
             else{
             	fclose(fileptr); }  // close SD card file}
         	return;
         }
         else if (data_read > 0)
         {
             if (image_header_was_checked == false)
             {
                 if (data_read > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
                 {
                     // check current version with downloading
                     memcpy(&new_app_info, &ota_write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
                     if(strncasecmp(new_app_info.project_name,"X-Series_Firmware",17) != 0)   // Check if new binary file is for Haven X- series
                     {
                     	Add_Response_msg("Error! New firmware is not for Haven X- series",s_Message_Rx, payLoadData);
                         if(JFS_SD == 1)
                         	FS_FClose(pFile);  // close JFS file
                         else
                         	fclose(fileptr);   // close SD card file
                      	return;
                     }
                     if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
                     {
                     	sprintf(str,"Running firmware version: %s",running_app_info.version);
                     	Add_Response_msg(str,s_Message_Rx, payLoadData);
                     }

                     const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
                     esp_app_desc_t invalid_app_info;
                     if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
                     {
 //                    	sprintf(str,"Last invalid firmware version: %s",invalid_app_info.version);
 //                    	Add_Response_msg(str,s_Message_Rx);
                     }

                     // check current version with last invalid partition
                     if (last_invalid_app != NULL)
                     {
                         if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
                         {
                         	//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "New version is the same as invalid version.");
                         	sprintf(str,"New version is the same as invalid version. Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
                         	Add_Response_msg(str,s_Message_Rx, payLoadData);
                             if(JFS_SD == 1)
                             	FS_FClose(pFile);  // close JFS file
                             else{
                             	fclose(fileptr); }  // close SD card file
                          	return;
                          }
                     }

                     if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
                     {
                     	Add_Response_msg("Current running version is the same as a new. We will not continue the update.",s_Message_Rx, payLoadData);
                         if(JFS_SD == 1)
                         	FS_FClose(pFile);  // close JFS file
                         else
                         	fclose(fileptr);   // close SD card file
                      	return;
                     }

                     image_header_was_checked = true;
                     err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                     if (err != ESP_OK)
                     {
                     	sprintf(str,"esp_ota_begin failed (%s)", esp_err_to_name(err));
                     	Add_Response_msg(str,s_Message_Rx, payLoadData);
                        esp_ota_abort(update_handle);
                        if(JFS_SD == 1)
                        	{FS_FClose(pFile);}  // close JFS file
                        else
                        	{fclose(fileptr);}   // close SD card file
                        return;
                     }
                     // Disable interrupts and events
                     {
//                     	Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "DEINIT");
 						cJSON *my_JSON = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
 						cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 0);
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
 						Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "SETSERVCONNFLAG");
 						cJSON_Delete(my_JSON); // Free the cJSON object
 						
 						cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
 						cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 0);
 						
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);
 						Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "SETEVTTASKFG");
 						cJSON_Delete(my_JSON1); // Free the cJSON object
// 						Send_CMD_To_Other_Actor(IHUB,"IHUB", "\0", 0, "DISCONNECT");
 						sendLedState("Download" , -1);
 						Add_Response_msg("OTA is in progress. Please wait....",s_Message_Rx, payLoadData);

 						uint8_t val = 0;
 						set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
 						set_to_other_actor("CONSOLE", U_INT8, "U0_DBG", &val);

 						Send_CMD_To_Other_Actor(IDLE, "IDLE", "\0", 0, "TEST_STOP");
            		    vTaskDelay(10000/portTICK_PERIOD_MS);
                     }
                 }
                 else
                 {
                 	Add_Response_msg("Received package is not fit len",s_Message_Rx, payLoadData);
                    esp_ota_abort(update_handle);
                    if(JFS_SD == 1)
                     	{FS_FClose(pFile);}  // close JFS file
                    else
                     	{fclose(fileptr);}   // close SD card file
                  	return;
                 }

 				for(int i= 0; i<strlen(new_app_info.version); i++)
 				{
 				   if(new_app_info.version[i]=='_')
 					   fm_version[i] = '.';
 				   else
 					   fm_version[i] = new_app_info.version[i];
 				}
             	sprintf(str,"New firmware version = %s, New Project name = %s ",fm_version, new_app_info.project_name);
             	Add_Response_msg(str,s_Message_Rx, payLoadData);
             }  //end of if (image_header_was_checked == false)
             err = esp_ota_write( update_handle, (const char *)ota_write_data, data_read);
             if (err != ESP_OK)
             {
                 esp_ota_abort(update_handle);
                 if(JFS_SD == 1)
                  	{FS_FClose(pFile);}  // close JFS file
                 else
                  	{fclose(fileptr);}   // close SD card file
              	 return;
             }
             File_length += data_read;
             count++;
 		   if(count > 50)  //80
 		   {
 			count = 0;
 			sprintf(str,"Firmware update progress:  %0.2f %%", ((float)((float)File_length/(float)File_size)*100));
 			Add_Response_msg(str,s_Message_Rx, payLoadData);
  		   }

         }  //end of if (data_read > 0)
         else if (data_read == 0)
         {
         	 //ESP_LOGI(TAG, "Connection closed");
         	 break;
         }
     }  //end of while(1)
     if(JFS_SD == 1)
     	{FS_FClose(pFile);}  // close JFS file
     else
     	{fclose(fileptr);}   // close SD card file

     err = esp_ota_end(update_handle);
     if (err != ESP_OK)
     {
		uint8_t val = 1;
		set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
	    sprintf(str, "esp_ota_end failed (%s). Preparing to restart system...", esp_err_to_name(err));
	    Add_Response_msg(str, s_Message_Rx, payLoadData);
        Restart_ESP_Xface(1);
     }
     err = esp_ota_set_boot_partition(update_partition);
     if (err != ESP_OK)
     {
 		uint8_t val = 1;
 		set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
        sprintf(str, "esp_ota_set_boot_partition failed (%s). Preparing to restart system...", esp_err_to_name(err));
        Add_Response_msg(str,s_Message_Rx, payLoadData);
        Restart_ESP_Xface(1);
     }
	cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
    cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
	cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", fm_version);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(object_JSON, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS",payLoadData,strlen(payLoadData),"WR_PARA"); 	
 	cJSON_Delete(object_JSON);

     if ((strncmp(new_app_info.version, "701", 3) == 0) && ((strncmp(running_app_info.version, "702", 3) == 0) || (strncmp(running_app_info.version, "703", 3) == 0)))
     {
    	 //Copy actor.json files from JFS to SPIFFS.
    	 COPY_JFS_SPIFFS_proc(s_Message_Rx);
    	 ESP_Reset_flag = 0;  // reset ESP from JFS after file transfer process
     }
	if(ESP_Reset_flag ==1)    //If ESP_Reset_flag = 1, then reset esp here otherwise reset from SPIFFS/JFS
	{
		Restart_ESP_Xface(1);
	}
 }

void JFS_Read_File(const uint8_t* payload,AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	int16_t Read_count 	=	0;
    char binary_file = 0, text_file = 0;
	uint8_t filename[50] = {0};
	char str[2200] = {0};
	uint32_t Read_len_Prev 	= 0;

	in_JSON 		= cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  return;
		}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcpy((char*)filename,name_JSON->valuestring);
	if(strcasecmp((char*)filename,"Audit/Audit_Log.txt")==0){
		printf("\n Audit File contents are: \n");
	}
	if(strcasecmp((char*)filename,"Audit/WIFI_Audit_Log.txt")==0){
		printf("\n WIFI Audit File contents are: \n");
	}

	const char *dot = strrchr((char*)filename, '.');
	    // Check if the dot is present and if it is followed by "bin"
	    if (dot != NULL && ((strcmp(dot + 1, "bin") == 0) || (strcmp(dot + 1, "db") == 0)))
	    {
	    	binary_file = 1; // The filename has a ".bin" or .db extension
#ifdef ENABLE_PRINT_MSG
	        printf("\n Binary file contents are:\n");
#endif
	    }
	dot = strrchr((char*)filename, '.');
	if (dot != NULL && (strcmp(dot + 1, "txt") == 0))
	{
		text_file = 1; // The filename has a ".txt" extension
	}
	while(1)  //do
	{
		memset(DataPtr_Read,0,MAX_JSON_PAYLOAD_BYTES);
		Read_count	= JFS_FLASH_Read_File((char*)filename, DataPtr_Read, MAX_JSON_PAYLOAD_BYTES, &Read_len_Prev);
		if(Read_count > 0)
		{
			memset(str,0,sizeof(str));
			if(text_file == 1)
			{
				printf("%s ", DataPtr_Read);  // Display the audit file content here
			    goto next;
			}
			cJSON *responseObject =   cJSON_Parse((char*) DataPtr_Read);  // check whether file data is json or not

			if ((((responseObject != NULL) && (cJSON_IsObject(responseObject)))) && (text_file == 0))   // file data is json
			{
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				cJSON_Delete(responseObject);
				console_send_responce_to_console_xface(s_Message_Rx);
			}
			else if(binary_file == 0)    // file data is string
			{
				cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
				cJSON_AddStringToObject(responseObject, "File Data", DataPtr_Read);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				cJSON_Delete(responseObject);
				console_send_responce_to_console_xface(s_Message_Rx);
			}
			if(binary_file == 1)     // file data is binary
			{
				for(int i=0; i<Read_count;i++)
					printf("%02x ", DataPtr_Read[i]);
			}
		}
		if(Read_count == -1)
		{
			sprintf(str, "%s file is not present in JFS.", filename);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			break;
		}
next:	if(Read_count == 0)
		{
			break;
		}

	}//while(Read_count!=0);

	if(strcasecmp((char*)filename,"Audit/Audit_Log.txt")==0){
			printf("\n Audit File contents are displayed \n");
	}
	if(strcasecmp((char*)filename,"Audit/WIFI_Audit_Log.txt")==0){
			printf("\n WIFI Audit File contents are displayed \n");
	}
	cJSON_Delete(in_JSON);
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

void JFS_Write_Variable(uint8_t* payload,AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	cJSON *Original_JSON = NULL;
	char filename[100] = {0}, retry = 0;
	char str[200] = {0};
	int ret = -1;
	int err = -1;
	int16_t Read_count 	=	0;
	uint32_t Read_len_Prev 	= 0;
	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"1 Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
		}

	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcat(filename, name_JSON->valuestring);
	retry = 0;
	do
	{
		Read_count	= JFS_FLASH_Read_File((char*)filename, DataPtr_Write, MAX_JSON_PAYLOAD_BYTES, &Read_len_Prev);
		if(Read_count > 0)
			break;
		else
		{
			retry++;
			vTaskDelay(50);
		}
	}while((Read_count <= 0) && (retry <3));

	if(Read_count > 0)
	{
		Original_JSON 	= cJSON_Parse(DataPtr_Write);
	}
	ret = update_json_object(&Original_JSON, in_JSON);
	cJSON_Delete(in_JSON);
	if(Original_JSON != NULL)
	{
		if(ret > 0)
		{
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(Original_JSON, payLoadData, sizeof(payLoadData), false);
			err = JFS_FLASH_Write_File1(filename, payLoadData, strlen(payLoadData), "w", 0);
		}
		cJSON_Delete(Original_JSON);
	}

	if(err == 0)
	{
		Add_Response_msg((char*)payload,s_Message_Rx, payLoadData);
		Add_Response_msg("variable(s) stored successfully",s_Message_Rx, payLoadData);
	}
	else
	{
		if(ret == 0)
		{
//			sprintf(str, "Received data of %s file is same.", filename);
//			Add_Response_msg(str,s_Message_Rx, payLoadData);
		}
		else if(ret == -1)
		{
			sprintf(str, "Error! Received data of %s file is NULL.", filename);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
		}
		else
		{
			Add_Response_msg("Error in storing Variable(s).",s_Message_Rx, payLoadData);
		}
	}
}

// Read variable from JFS file
void JFS_Read_Variable(const uint8_t* payload,AMessage_st* s_Message_Rx)
{
	    cJSON *in_JSON 		= 	NULL;
		cJSON *in_JSON1 		= 	NULL;
		cJSON *name_JSON 	= 	NULL;
		cJSON *head_JSON 	= 	NULL;
		cJSON *head_JSON1 	= 	NULL;
		cJSON *out_JSON1 	= 	NULL;
		cJSON *root 	= 	NULL;
		char filename[30] = {0};
		uint8_t Match_found_u8 = 0;
		uint16_t Read_count 	=	0;
		char str[50] = {0};
		char DataPtr[JSON_FILE_SIZE]		= {0};
		uint32_t Read_len_Prev 	= 0;

		in_JSON = cJSON_Parse((char*) payload);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
			}

		name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
		if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
			strcpy(filename, name_JSON->valuestring);
		else
			return;

		root 	= cJSON_CreateObject();
		Read_count	= JFS_FLASH_Read_File((char*)filename, DataPtr, JSON_FILE_SIZE, &Read_len_Prev);
		{
				head_JSON 	= in_JSON->child;    //take variable from payload
				head_JSON = head_JSON->next;  //point to 1st variable (format: Filename, var1, var2, ...)

				//loop

				do {
						Read_len_Prev = 0;
						Match_found_u8 = 0;
						while(1)
						{
							if(Read_count == 0)
							{
								Add_Response_msg("Error! File is NULL.",s_Message_Rx, payLoadData);
								cJSON_Delete(in_JSON);
								return;
							}


							if(Read_count !=0)
							{
								in_JSON1 	= cJSON_Parse(DataPtr);
								if (in_JSON1 == NULL) {
									sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
									Add_Response_msg(str,s_Message_Rx, payLoadData);
									goto exit;
									}
								out_JSON1 	= cJSON_CreateObject();
								head_JSON1 	= in_JSON1->child;

								//loop
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
										memset(payLoadData,0,sizeof(payLoadData));
										cJSON_PrintPreallocated(out_JSON1, payLoadData, sizeof(payLoadData), false);
										if(strcmp(head_JSON1->string,head_JSON->string)==0)
										{
											name_JSON = cJSON_Duplicate(head_JSON1, 1);
											cJSON_AddItemToObject(root, head_JSON1->string, name_JSON);
											Match_found_u8 = 1;	// strlen(filedata+2) is used to remove start and end brackets {}
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
									sprintf(str, "Error! %s is not found", head_JSON->string);
									Add_Response_msg(str,s_Message_Rx, payLoadData);
									break;
								}
							}
						}
					head_JSON = head_JSON->next;
				}while (head_JSON != NULL);
		}
		if((Match_found_u8 == 1))  //|| (root!=NULL)
		{
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
exit:	cJSON_Delete(in_JSON);
		cJSON_Delete(root);
}

// Function to get file list and other information from JFS
void JFS_Get_File_List(char* buffer, AMessage_st* s_Message_Rx) {
    int r = -1, r1 = -1;
    FS_FILE *pFile = NULL;
    cJSON *root = NULL, *root_final = NULL;
    char str[100] = {0};
    FS_FIND_DATA find_data, find_data_new;
    char acFileName[100] = {0}, acFileName_new[100] = {0};
    char dir_name[100] = {0}, dir_name_new[100] = {0};
    uint32_t total_size = 0, remaining_size = 0;
    cJSON *in_JSON = cJSON_Parse(buffer);

    if (in_JSON == NULL) {
    	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
        Add_Response_msg(str, s_Message_Rx, payLoadData_File_List);
        return;
    }

    // Extract directory name from JSON input
    cJSON *Directory_name = cJSON_GetObjectItem(in_JSON, "DIR_NAME");
    if (Directory_name != NULL && Directory_name->valuestring != NULL) {
        strcpy(dir_name, JFS_Root_Path);
        if (strlen(Directory_name->valuestring) != 0) {
            strcat(dir_name, Directory_name->valuestring);
        } else {
            dir_name[strlen(dir_name) - 1] = '\0';
        }
    } else {
        dir_name[strlen(dir_name) - 1] = '\0';
    }

    // Enable support for long file names
    FS_FAT_SupportLFN();

    // Retry logic for finding the first file
    int retry_count = 0;
    while ((r = FS_FindFirstFile(&find_data, dir_name, acFileName, sizeof(acFileName))) != 0 && retry_count < MAX_RETRIES) {
        retry_count++;
    }
    if (r != 0) {
        Add_Response_msg("Error finding the first file.", s_Message_Rx, payLoadData_File_List);
        cJSON_Delete(in_JSON);
        return;
    }

    root = cJSON_CreateObject();
    cJSON *jsonArrayFileName = cJSON_CreateArray();
    root_final = cJSON_CreateObject();

    if (r == 0) {
        do {
            if (acFileName[0] == '.') {
                continue;
            }

            cJSON *fileInfo = cJSON_CreateObject();
            cJSON_AddItemToObject(fileInfo, "name", cJSON_CreateString(acFileName));

            if (find_data.Attributes & FS_ATTR_DIRECTORY) {
                strcpy(dir_name_new, dir_name);
                strcat(dir_name_new, "/");
                strcat(dir_name_new, acFileName);
                cJSON_AddItemToObject(fileInfo, "type", cJSON_CreateString("Directory"));

                cJSON *subDirFiles = cJSON_CreateArray();
                retry_count = 0;
                while ((r1 = FS_FindFirstFile(&find_data_new, dir_name_new, acFileName_new, sizeof(acFileName_new))) != 0 && retry_count < MAX_RETRIES) {
                    retry_count++;
                }
                if (r1 == 0) {
                    do {
                        if (acFileName_new[0] == '.') {
                            continue;
                        }
                        cJSON *subFileInfo = cJSON_CreateObject();
                        cJSON_AddItemToObject(subFileInfo, "name", cJSON_CreateString(acFileName_new));
                        switch (find_data_new.Attributes) {
                            case FS_ATTR_READ_ONLY: strcpy(str, "Read only"); break;
                            case FS_ATTR_HIDDEN: strcpy(str, "Hidden"); break;
                            case FS_ATTR_SYSTEM: strcpy(str, "Operating system"); break;
                            case FS_ATTR_ARCHIVE: strcpy(str, "Archive"); break;
                            case FS_ATTR_DIRECTORY: strcpy(str, "Directory"); break;
                            default: break;
                        }
                        cJSON_AddItemToObject(subFileInfo, "type", cJSON_CreateString(str));
                        strcpy(str, dir_name_new);
                        strcat(str, "/");
                        strcat(str, acFileName_new);
                        FS_FOpenEx(str, "r", &pFile);
                        if (pFile != NULL) {
                            uint32_t sizeu32 = FS_GetFileSize(pFile);
                            FS_FClose(pFile);
                            sprintf(str, "%ld", sizeu32);
                            cJSON_AddItemToObject(subFileInfo, "size", cJSON_CreateString(str));
                        }
                        cJSON_AddItemToArray(subDirFiles, subFileInfo);
                    } while (FS_FindNextFile(&find_data_new));
                }
                cJSON_AddItemToObject(fileInfo, "files", subDirFiles);
            } else {
                switch (find_data.Attributes) {
                    case FS_ATTR_READ_ONLY: strcpy(str, "Read only"); break;
                    case FS_ATTR_HIDDEN: strcpy(str, "Hidden"); break;
                    case FS_ATTR_SYSTEM: strcpy(str, "Operating system"); break;
                    case FS_ATTR_ARCHIVE: strcpy(str, "Archive"); break;
                    case FS_ATTR_DIRECTORY: strcpy(str, "Directory"); break;
                    default: break;
                }
                cJSON_AddItemToObject(fileInfo, "type", cJSON_CreateString(str));
                strcpy(str, dir_name);
                strcat(str, "/");
                strcat(str, acFileName);
                FS_FOpenEx(str, "r", &pFile);
                if (pFile != NULL) {
                    uint32_t sizeu32 = FS_GetFileSize(pFile);
                    FS_FClose(pFile);
                    sprintf(str, "%ld", sizeu32);
                    cJSON_AddItemToObject(fileInfo, "size", cJSON_CreateString(str));
                }
            }
            cJSON_AddItemToArray(jsonArrayFileName, fileInfo);
        } while (FS_FindNextFile(&find_data));

        // Add the list of files to the root JSON object
        cJSON_AddItemToObject(root, "files", jsonArrayFileName);

        // Get total size and remaining size of JFS
        total_size = FS_GetVolumeSize(VOLUME_NAME);
        remaining_size = FS_GetVolumeFreeSpace(VOLUME_NAME);
        cJSON_AddItemToObject(root, "total_size", cJSON_CreateNumber(total_size));
        cJSON_AddItemToObject(root, "remaining_size", cJSON_CreateNumber(remaining_size));
    }
    FS_FindClose(&find_data);

    if (r == 0) {
			memset(payLoadData_File_List,0,sizeof(payLoadData_File_List));
			cJSON_PrintPreallocated(root, payLoadData_File_List, sizeof(payLoadData_File_List), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_File_List);
			console_send_responce_to_console_xface(s_Message_Rx);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData_File_List);
    } else {
        Add_Response_msg("No files or directories available in the directory.", s_Message_Rx, payLoadData_File_List);
    }

    // Cleanup JSON objects
    cJSON_Delete(root);
    cJSON_Delete(root_final);
    cJSON_Delete(in_JSON);
}

void Save_Audit_log(AMessage_st* s_Message_Rx)
{
    uint32_t fileSize = 0;     // Variable to store file size
    int err = 0;               // Variable to store error codes
    int retries = 0;           // Retry counter
    FS_FIND_DATA pFileFindData;
    // Ensure long file name support is enabled
    FS_FAT_SupportLFN();
    char filename[100] = {0};

    if(FS_FindFirstFile(&pFileFindData, "Root/Audit/", filename, sizeof(filename)) == 0)
	{
		do
		{
			// Ignore "." and ".." entries.
			if(pFileFindData.sFileName[0] == '.')
			{
				continue;
			}
			if(strncasecmp(pFileFindData.sFileName, "Audit_Log.txt", 13) == 0)
			{
				fileSize = pFileFindData.FileSize;
				break;
			}
		} while(FS_FindNextFile(&pFileFindData));
	}
    // Check if the file size exceeds the maximum allowed size
    if ((fileSize > MAX_Log_File_Size_u32)  || (FS_GetVolumeFreeSpace(VOLUME_NAME) < JFS_BUFFER_SIZE)) {
        // Delete old log file if it exists
        err = FS_Remove("Root/Audit/Audit_Log_Old.txt");
        if ((err != 0) && (err != FS_ERRCODE_FILE_DIR_NOT_FOUND)) {
            // Error occurred while deleting the old log file
            printf("Error deleting old log: %d (%s)\n", err, FS_ErrorNo2Text(err));
          //  Add_Response_msg("Error deleting old audit log.", s_Message_Rx);
            return;
        }

        // Rename current log file to old log file
        retries = 0;
        while (retries < MAX_RETRIES) {
            // Attempt to rename the log file
            err = FS_Rename("Root/Audit/Audit_Log.txt", "Audit_Log_Old.txt");
            if (err == 0) {
                // Successfully renamed the file
//                Add_Response_msg("Audit log renamed successfully.", s_Message_Rx , payLoadData);
                break;
            }
            else
            {
				 if (err == FS_ERRCODE_FILE_IS_OPEN)
				 {
					 retries++;
					 vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
				 }
				 else
				 {
					 printf("\n Error! failed to rename 'Audit/Audit_Log.txt' file. Error is '%s'. \n", FS_ErrorNo2Text(err));
					 break;
				 }
            }
        }
        if (retries >= MAX_RETRIES && err != 0) {
            // Failed to rename the file after maximum retries
            printf("Error renaming log file: %d (%s)\n", err, FS_ErrorNo2Text(err));
            return;
        }
    }

    // Write new log data to the file
    err = JFS_FLASH_Write_File1("Audit/Audit_Log.txt",
                                (char*)s_Message_Rx->payload_p8,
                                strlen((char*)s_Message_Rx->payload_p8),
                                "a", 0);

    if (err != 0)
    {
    	 printf("Error! Failed to write to Audit_Log.txt. Error is %s. \n", FS_ErrorNo2Text(err));

    }
}

void JFS_Write_File(char* payload, AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	int err = -1;
	char filename [50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char str[100] = {0};

	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
		}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON!= NULL) && (name_JSON->valuestring != NULL))
	strcpy(filename,name_JSON->valuestring);

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
		err = JFS_FLASH_Write_File(filename, payLoadData, strlen(payLoadData), "w", 0, payLoadData_Write);
	}
	cJSON_Delete(in_JSON);
	if(err != ESP_OK)
		Add_Response_msg("Failed to open JFS file for writing",s_Message_Rx, payLoadData);
	else
	{
		sprintf(str, "%s file is stored in JFS successfully", filename);
		Add_Response_msg(str, s_Message_Rx, payLoadData);
	}
}

int32_t JFS_GET_FILE_SIZE(char* payload, AMessage_st* s_Message_Rx)
{
	    FS_FILE * pFile 	= 	NULL;
		uint32_t filesize = 0;
		char File_name[100] = {0};
		char str[100] = {0};
		cJSON *in_JSON = NULL;
		cJSON *name_JSON = NULL;

		in_JSON 	  = cJSON_Parse(payload);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return (-1);
			}
		strcpy(File_name, JFS_Root_Path);
		name_JSON 	  = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
		strcat(File_name,name_JSON->valuestring);
		FS_FAT_SupportLFN();    // support for long file name
		FS_FOpenEx(File_name, "r", &pFile);		// Open the file in read mode
		if (pFile == NULL) {
			return (-1);
		}
		filesize = FS_GetFileSize(pFile);  //Get the file size
	   	FS_FClose(pFile);
	   	cJSON_Delete(in_JSON);
	   	return(filesize);
}

static const unsigned short days[4][12] =
{
   {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
   { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
   { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
   {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

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

static uint64_t get_current_time_ms() {
	return get_current_rtc_time_ms(0);
}

void Save_WIFI_Audit_log (AMessage_st* s_Message_Rx)
{
	FS_FILE *pFile;
	uint32_t FileSize_u32 = 0;
	char str [100] = {0};
	int err;
	uint64_t current_epos_msec;//mills;
	uint64_t Temp_msec_1;
	date_time_t  sdate_tim;

	FS_FAT_SupportLFN();    // support for long file name
		{
			pFile = FS_FOpen("Root/Audit/WIFI_Audit_Log.txt", "a");		// open the file in the read mode
			if (pFile == NULL) {
				Add_Response_msg("Error! Open WIFI_Audit_Log.txt file Failed!",s_Message_Rx, payLoadData);
				return;
			}
			FileSize_u32 =	FS_GetFileSize(pFile);
			FS_FClose(pFile);
			if((FileSize_u32 > MAX_WIFI_Log_File_Size_u32) || (FS_GetVolumeFreeSpace(VOLUME_NAME) < JFS_BUFFER_SIZE))
			{
				pFile = FS_FOpen("Root/Audit/WIFI_Audit_Log_Old.txt", "r");		// open the file in the read mode
				if (pFile == NULL)
				{
					FS_FClose(pFile);
					JFS_FLASH_Rename_File("Audit/WIFI_Audit_Log.txt", "WIFI_Audit_Log_Old.txt",s_Message_Rx); // Audit_Log_old.txt file is not present. So, rename audit log file name.
				}
				else
				{
					FS_FClose(pFile);
					err = FS_Remove("Root/Audit/WIFI_Audit_Log_Old.txt");
						if (err != 0) {
							sprintf((char*)str, "Error! Delete File [%d]:[%s].", err,FS_ErrorNo2Text(err));
							Add_Response_msg(str,s_Message_Rx, payLoadData);
							return;
						}
					JFS_FLASH_Rename_File("Audit/WIFI_Audit_Log.txt", "WIFI_Audit_Log_Old.txt",s_Message_Rx);  // Rename audit log file name.
				}
			}
			current_epos_msec = get_current_time_ms();
	#ifdef ENABLE_PRINT_MSG
			printf("Current_epos_sec: %lld\n", current_epos_msec);
	#endif
			Temp_msec_1 = EPOSCH_TO_30_YEAR;
			Temp_msec_1 = Temp_msec_1*1000;
			current_epos_msec = current_epos_msec - Temp_msec_1;

			current_epos_msec = current_epos_msec + (get_gmt() * 60*1000);//Reverse of set_rtc
			if(get_dst() == 1)
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

			payLoadData_Debug[0] = 0;
			sprintf(payLoadData_Debug, "%02d/%02d/%02d %02d:%02d:%02d.%03d | %s\n",UserDateTimeL_1.year, UserDateTimeL_1.month,UserDateTimeL_1.date,UserDateTimeL_1.hour,UserDateTimeL_1.minute,UserDateTimeL_1.second,UserDateTimeL_1.milSec, (char*) s_Message_Rx->payload_p8);
			JFS_FLASH_Write_File("Audit/WIFI_Audit_Log.txt", payLoadData_Debug, strlen(payLoadData_Debug), "a", 0, payLoadData_WIFI );
		}
}

void JFS_SMTP_Read_File(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
	int32_t Read_count 	=	0;
    int Read_byte_count = 2040; //((int)(JSON_FILE_SIZE/6)*6) - 12;  //modification done for mailing binary/db file correctly.
	char filename[50] = {0};
	uint8_t SMTP_response_Flag = E_JFS_RESP_OK;
	char buffer[100] = {0}; // DataPtr[2200]		= {0};
	char str[200] = {0};
	char src_file[100] = {0}, dest_file[100] = {0}, dest_file_temp[100] = {0};
	int retry = 0, ret = -1;
	uint32_t SMTP_Read_len_Prev 	= 0;

	if((char*) s_Message_Rx->payload_p8 != NULL)
	{
		if(strlen((char*) s_Message_Rx->payload_p8) == 0)
		{
			Add_Response_msg("Error! Received payload is empty. Deleting the JFS_SMTP_Read_File task..",s_Message_Rx, payLoadData_Chunk_Read);
			Read_Handle = NULL;
			vTaskDelete(Read_Handle);
		}
	}
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_Chunk_Read);
		goto exit;
		}

	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
	{
		strcpy((char*)filename,name_JSON->valuestring);
	}
	else
	{
		Add_Response_msg("'FILE_NAME' key is missing in JFS_SMTP_Read_File task.",s_Message_Rx, payLoadData_Chunk_Read);
		cJSON_Delete(in_JSON);
		goto exit;
	}

	cJSON_Delete(in_JSON);
		strcpy(src_file, JFS_Root_Path);
		strcat(src_file, filename);
		strcpy(dest_file, JFS_Root_Path);
		strcat(dest_file, "Temp/");
		strcat(dest_file, filename);

		strcat(dest_file_temp, "Temp/");
		strcat(dest_file_temp, filename);
		Extract_folders(dest_file_temp);
		vTaskDelay(100/ portTICK_PERIOD_MS);
		retry = 0;
		do
		{
			ret = FS_CopyFile(src_file, dest_file); //"ram:\\Dest.txt"
			if(ret == ESP_OK)
			{
				break;
			}
			else
			{
				retry++;
				printf("\n error in file copied retry = %d, ret = %d\n", retry, ret);
				 vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
			}
		}while(retry < MAX_RETRIES);
		if((retry >= MAX_RETRIES) && ((ret != ESP_OK) && (ret != FS_ERRCODE_FILE_DIR_NOT_FOUND)))
		{
			sprintf(str, "Operation on %s file is in progress. Kindly try again.", src_file);
			Send_CMD_To_Other_Actor(SMTP_CLIENT, "SMTP_CLIENT", str, strlen(str), "MAIL_BINARY");
	        Delete_folders(dest_file_temp);
	        Read_Handle = NULL;
	        vTaskDelete(Read_Handle);
		}
		strcpy(filename, dest_file_temp);

		char File_name[50] = {0};
		strcpy(File_name, JFS_Root_Path);
		strcat(File_name, filename);

		//printf("\n\n File_name = %s  \n\n", File_name);
		FS_FAT_SupportLFN();    // support for long file name
		FS_FILE * pFile 	= 	NULL;
		FS_FOpenEx(File_name, "r", &pFile);		// Open the file in read mode
		if (pFile == NULL) {
			printf("\n\n pFile is NULL \n\n");
	        Delete_folders(dest_file_temp);
	        Read_Handle = NULL;
	        vTaskDelete(Read_Handle);
		}
		FS_FClose(pFile);  // close original file
		FS_FOpenEx(dest_file, "r", &pFile);		// Open the copied file in read mode
		int32_t filesize = FS_GetFileSize(pFile);  //Get the file size
	   	FS_FClose(pFile);
	   	//printf("\n\n filename = %s, file size = %ld \n\n", filename, filesize);
		cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
		cJSON_AddStringToObject(responseObject, "FILE_NAME", filename);
		cJSON_AddNumberToObject(responseObject, "FILE_SIZE", filesize);
		memset(payLoadData_Chunk_Read,0,sizeof(payLoadData_Chunk_Read));
		cJSON_PrintPreallocated(responseObject, payLoadData_Chunk_Read, sizeof(payLoadData_Chunk_Read), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Chunk_Read);
		console_send_responce_to_console_xface(s_Message_Rx);
		cJSON_Delete(responseObject);
		//Add_Response_msg("Sending file data to SMTP..... ",s_Message_Rx, payLoadData_Chunk_Read);
	while(1)  //do
	{
		if (pdTRUE == xQueueReceive(SMTP_Read_Queue, (void*)buffer, 20000))
		{
			if(strlen(buffer) != 0)
			{
				in_JSON 		= cJSON_Parse((char*) buffer);
				name_JSON 		= cJSON_GetObjectItem(in_JSON, "SMTP_RESP");
				SMTP_response_Flag	= (uint8_t)cJSON_GetNumberValue(name_JSON);
				cJSON_Delete(in_JSON);
			}
			if(SMTP_response_Flag == E_JFS_RESP_OK)
			{
				SMTP_response_Flag = E_JFS_RESP_WAIT;
				memset(DataPtr_Chunk_Read,0,sizeof(DataPtr_Chunk_Read));
				Read_count	= JFS_FLASH_Read_File((char*)filename, DataPtr_Chunk_Read, Read_byte_count, &SMTP_Read_len_Prev);
				if(Read_count > 0)
				{
					Send_CMD_To_Other_Actor(SMTP_CLIENT, "SMTP_CLIENT", DataPtr_Chunk_Read, Read_count, "MAIL_BINARY");
				}
				if(Read_count == -1)
				{
					sprintf(str, "%s file is not present in JFS.", src_file);
					Send_CMD_To_Other_Actor(SMTP_CLIENT, "SMTP_CLIENT", str, strlen(str), "MAIL_BINARY");
					break;
				}
				if(Read_count == 0)
				{
					break;
				}
			}  //end of if(SMTP_response_Flag == 2)
		} // end of que receive
		else
		{
			Add_Response_msg("Queue is not received. Deleting the JFS_SMTP_Read_File task.",s_Message_Rx, payLoadData_Chunk_Read);
			goto exit;
		}
		vTaskDelay(50/ portTICK_PERIOD_MS);
	}//while(Read_count!=0);
exit:
		ret = FS_Remove(dest_file);
        if((ret != 0) && (Read_count == 0))
        {
        	sprintf(str, "Error! Could not delete %s file", dest_file_temp);
			Add_Response_msg(str,s_Message_Rx, payLoadData_Chunk_Read);
        }
        Delete_folders(dest_file_temp);
	Read_Handle = NULL;
	vTaskDelete(Read_Handle);
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
//	printf("\n COP_add_response_to_COP_Tx_Queue");
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer = NULL;

	if(strcmp(function, "MAIL_BINARY") == 0)
	{
		newpointer  		= (uint8_t*) heap_caps_calloc(size,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (newpointer == NULL)
		{
			printf("Memory allocation failed\n");
			return;
		}
		memcpy(newpointer, response, size);
	}
	else
	{
		newpointer  = (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
		if (newpointer == NULL)
		{
			printf("Memory allocation failed\n");
			return;
		}
		strcpy((char*)newpointer, response);
	}
	s_Message_Tx_new.payload_p8 	=  newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
} //	COP_add_response_to_COP_Tx_Queue

static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {
	AMessage_st 		s_Message_Tx;
	char out_val[200] = {0};
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

	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
			
	uint8_t *newpointer = (uint8_t*) heap_caps_calloc(strlen((char*) payLoadData) + 1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payLoadData);
	s_Message_Tx.payload_size = strlen((char*)payLoadData);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor

PSRAM_ATTR_BSS static char buffer[MAX_JSON_PAYLOAD_BYTES];
// Task to process the queue and write to files
void vWriteTask(void *pvParameters) {
    WriteMessage msg;

    while (1) {
        #ifdef ENABLE_PRINT_MSG
        printf("Waiting to receive message from queue...\n");
        #endif

        memset(buffer, 0, sizeof (buffer));
        if (xQueueReceive(xWriteQueue, &msg, portMAX_DELAY) == pdPASS) {
        	memcpy(buffer, msg.data, msg.size);

            #ifdef ENABLE_PRINT_MSG
            printf("Received message from queue.\n");
            #endif
            int result = -1;
            {
                #ifdef ENABLE_PRINT_MSG
                printf("Attempting to write to file: %s, Retry count: %d\n", msg.File_name, 3 - retry_count);
                #endif
                Extract_folders(msg.File_name);  // create folder if not exits
                result = JFS_FLASH_Write_File1(msg.File_name, buffer, msg.size, msg.OpenType, msg.Send_Ack);
                if (result == 0) {
                    #ifdef ENABLE_PRINT_MSG
                    printf("Write successful.\n");
                    #endif
                  //  break;
                } else {
                    #ifdef ENABLE_PRINT_MSG
                    printf("Write failed, retrying...\n");
                    #endif
                }
            }
            if(result != 0)
            	printf("\n Write failed \n");
         }
        else {
            #ifdef ENABLE_PRINT_MSG
            printf("Failed to receive message from queue.\n");
            #endif
        }
    }
}

void unmount_JFS(void )
{
	FS_Unmount(VOLUME_NAME);
}

void Save_Event_log(AMessage_st* s_Message_Rx)
{
    uint32_t fileSize = 0;     // Variable to store file size
    int err = 0;               // Variable to store error codes
    int retries = 0;           // Retry counter
    // Ensure long file name support is enabled
    FS_FAT_SupportLFN();
    FS_FIND_DATA pFileFindData;
    char filename[100] = {0};

    if(FS_FindFirstFile(&pFileFindData, "Root/Audit/", filename, sizeof(filename)) == 0)
	{
		do
		{
			// Ignore "." and ".." entries.
			if(pFileFindData.sFileName[0] == '.')
			{
				continue;
			}
			if(strncasecmp(pFileFindData.sFileName, "Event_Log.txt", 13) == 0)
			{
				fileSize = pFileFindData.FileSize;
				break;
			}
		} while(FS_FindNextFile(&pFileFindData));
	}
    // Check if the file size exceeds the maximum allowed size
    if ((fileSize > MAX_Event_Log_File_Size_u32) || (FS_GetVolumeFreeSpace(VOLUME_NAME) < JFS_BUFFER_SIZE))
    {
        // Delete old log file if it exists
        err = FS_Remove("Root/Audit/Event_Log_Old.txt");
        if ((err != 0) && (err != FS_ERRCODE_FILE_DIR_NOT_FOUND)) {
            // Error occurred while deleting the old log file
            printf("Error deleting old event log file: %d (%s)\n", err, FS_ErrorNo2Text(err));
          //  Add_Response_msg("Error deleting old audit log.", s_Message_Rx);
            return;
        }

        // Rename current log file to old log file
        retries = 0;
        while (retries < MAX_RETRIES) {
            // Attempt to rename the log file
            err = FS_Rename("Root/Audit/Event_Log.txt", "Event_Log_Old.txt");
            if (err == 0) {
                // Successfully renamed the file
                Add_Response_msg("Event log renamed successfully.", s_Message_Rx , payLoadData);
                break;
            }
            else
            {
				 if (err == FS_ERRCODE_FILE_IS_OPEN)
				 {
					 retries++;
					 vTaskDelay(DELAY_BETWEEN_RETRIES_MS / portTICK_PERIOD_MS);
				 }
				 else
				 {
					 printf("\n Error! failed to rename 'Audit/Event_Log.txt' file. Error is '%s'. \n", FS_ErrorNo2Text(err));
					 break;
				 }
            }
        }
        if (retries >= MAX_RETRIES && err != 0) {
            // Failed to rename the file after maximum retries
            printf("Error renaming event log file: %d (%s)\n", err, FS_ErrorNo2Text(err));
            return;
        }
    }

    err = JFS_FLASH_Write_File1("Audit/Event_Log.txt",
                                       (char*)s_Message_Rx->payload_p8,
                                       strlen((char*)s_Message_Rx->payload_p8),
                                       "a", 0);
   if (err != 0) {
	   printf("Error! Failed to write to Event_Log.txt \n");
   }
}

static void Delete_folders(const char *path) {
    // Make a copy of the path to tokenize it (strtok modifies the string)
    char path_copy[100] = {0}, Dir_name[100] ={0};
    strcpy(Dir_name,"Root");
    strncpy(path_copy, path, sizeof(path_copy));
    path_copy[sizeof(path_copy) - 1] = '\0'; // Ensure null termination

    // Tokenize the string by '/'
    char *token = strtok(path_copy, "/");

    // Iterate through all tokens except the last one (file name)
    while (token != NULL) {
    	strcat(Dir_name, "/");
        // Find the next token
        char *next_token = strtok(NULL, "/");

        // If next_token is NULL, we are at the last token (file name)
        if (next_token == NULL) {
            break;
        }

        // Print the folder name
        strcat(Dir_name, token);
        int err = FS_DeleteDir(Dir_name,1);
		if (err < 0) {
			#ifdef ENABLE_PRINT_MSG
			printf("Error! Create Dir [%d]:[%s].\n", err,FS_ErrorNo2Text(err));
			#endif
		}
        // Move to the next token
        token = next_token;
    }
}

static void sendLedState(const char *stateName, int duration) {
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    // Print the JSON object as a string
    payLoadData [0]= '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoadData, strlen(payLoadData), "SETSTATE");

    // Free the allocated memory
    cJSON_Delete(responseObject);
}

static void COPY_JFS_SPIFFS_proc(AMessage_st* s_Message_Rx)
{
	cJSON *root = NULL;
	char str[100] = {0}, filename[100] = {0}, dest_filename[100] = {0}, File_Path[100] = "{\"DIR_NAME\" : \"System\"}";
	FS_FILE * pFile 	= 	NULL;
	int ret = -1, Read_len = 0;

	JFS_Get_File_List(File_Path,s_Message_Rx);
	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}

	cJSON *fileArray = cJSON_GetObjectItemCaseSensitive(root, "files");
	if ((fileArray != NULL) && (cJSON_IsArray(fileArray)))
	{
		int arraySize = cJSON_GetArraySize(fileArray);
		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(fileArray, i);
			cJSON *file_name = cJSON_GetObjectItemCaseSensitive(element, "name");
			if (cJSON_IsString(file_name) && (file_name->valuestring != NULL))
			{
				if(strlen(file_name->valuestring) == 0)
					continue;

				memset(filename, 0, sizeof(filename));
				memset(dest_filename, 0, sizeof(dest_filename));
				strcpy((char*)dest_filename, "System/");
				strcat((char*)dest_filename, file_name->valuestring);
				strcpy((char*)filename, JFS_Root_Path);
				strcat((char*)filename, "System/");
				strcat((char*)filename, file_name->valuestring);
				FS_FAT_SupportLFN();    // support for long file name
				ret = FS_FOpenEx(filename, "r", &pFile);		// Open the file in read mode
				if (pFile == NULL) {
					sprintf(str, "Error! cannot open '%s' file. Error is %s", file_name->valuestring, FS_ErrorNo2Text(ret));
					Add_Response_msg(str,s_Message_Rx, payLoadData);
					continue;
				}

				do
				{
					memset(DataPtr_Read,0,sizeof(DataPtr_Read));
					Read_len = FS_FRead(DataPtr_Read, 1, 2048, pFile);
					if(Read_len > 0)
					{
						cJSON *write_data = cJSON_CreateObject();
						cJSON_AddStringToObject(write_data, "FILE_NAME", dest_filename);
						cJSON_AddStringToObject(write_data, "FILE_DATA", DataPtr_Read);
						payLoadData[0] = '\0';
						cJSON_PrintPreallocated(write_data, payLoadData, sizeof(payLoadData), false);
						cJSON_Delete(write_data);
						Send_CMD_To_Other_Actor(SYS_FILES, "SYS_FILES",payLoadData,strlen(payLoadData),"WRITE");
					}
					else
					{
						break;
					}
				}while(Read_len > 0);
				FS_FClose(pFile);  // close the file
			}
			 vTaskDelay(200/ portTICK_PERIOD_MS);
		}
		cJSON_Delete(root);
		Add_Response_msg("JSON Files are copied to SPIFFS volume 2. Now resetting the ESP...",s_Message_Rx, payLoadData);
		vTaskDelay(10000/portTICK_PERIOD_MS);  // wait for completing file copy process
		Restart_ESP_Xface(1);
		return;
	}
}

static  esp_partition_t *update_partition = NULL;
static	char fm_version[20] = {0};
static	esp_app_desc_t new_app_info;
static 	esp_app_desc_t running_app_info;
static esp_ota_handle_t update_handle = 0;
static bool image_header_was_checked = false;
static esp_partition_t *running = NULL;
static uint16_t CRC_OTA = 0x00;

void Write_Bin_File_OTA(char* write_data, int16_t size, int64_t File_size, AMessage_st* s_Message_Rx)
{
 	char str[200] = {0};
 	char count = 0;
 	int err = -1;
 	cJSON *root = NULL;

    if(image_header_was_checked == false)  // Execute this code only 1st time
    {
        const esp_partition_t *configured = esp_ota_get_boot_partition();
        running = esp_ota_get_running_partition();

        if (configured != running) {
        	sprintf(str,"Error! Configured OTA boot partition at offset 0x%08ld, but running from offset 0x%08ld.This can happen if either the OTA boot data or preferred boot image become corrupted somehow.",configured->address, running->address );
        	Add_Response_msg(str,s_Message_Rx, payLoadData);
	    }
		update_partition = esp_ota_get_next_update_partition(NULL);
		assert(update_partition != NULL);
		File_length=0;
		if(update_partition == NULL)
			printf("\n\n update_partition is NULL \n\n");
    }
//    while (1)
    {
		if (image_header_was_checked == false)
		{
			if (size > sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t))
			{
				// check current version with downloading
				memcpy(&new_app_info, &write_data[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)], sizeof(esp_app_desc_t));
				const esp_app_desc_t *Current_app_info = esp_app_get_description();
				if(strncasecmp(new_app_info.project_name,Current_app_info->project_name,14) != 0)   // Check if new binary file is for Haven devices
				{
					Add_Response_msg("Error! New firmware is not for this device.",s_Message_Rx, payLoadData);
					image_header_was_checked = false;
				    root 	= cJSON_CreateObject();
				    cJSON_AddNumberToObject(root, "JFS_Resp", E_JFS_RESP_TERMINATE);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
					cJSON_Delete(root);
					console_send_responce_to_console_xface(s_Message_Rx);
					return;
				}
				if (esp_ota_get_partition_description(running, &running_app_info) == ESP_OK)
				{
					sprintf(str,"Running firmware version: %s",running_app_info.version);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				}

				const esp_partition_t* last_invalid_app = esp_ota_get_last_invalid_partition();
				esp_app_desc_t invalid_app_info;
				if (esp_ota_get_partition_description(last_invalid_app, &invalid_app_info) == ESP_OK)
				{
//                    	sprintf(str,"Last invalid firmware version: %s",invalid_app_info.version);
//                    	Add_Response_msg(str,s_Message_Rx);
				}

				// check current version with last invalid partition
				if (last_invalid_app != NULL)
				{
					if (memcmp(invalid_app_info.version, new_app_info.version, sizeof(new_app_info.version)) == 0)
					{
						//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "New version is the same as invalid version.");
						sprintf(str,"New version is the same as invalid version. Previously, there was an attempt to launch the firmware with %s version, but it failed.", invalid_app_info.version);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
						image_header_was_checked = false;
					    root 	= cJSON_CreateObject();
					    cJSON_AddNumberToObject(root, "JFS_Resp", E_JFS_RESP_TERMINATE);
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
						cJSON_Delete(root);
						console_send_responce_to_console_xface(s_Message_Rx);
						return;
					 }
				}

				if (memcmp(new_app_info.version, running_app_info.version, sizeof(new_app_info.version)) == 0)
				{
					Add_Response_msg("Current running version is the same as a new. We will not continue the update.",s_Message_Rx, payLoadData);
					image_header_was_checked = false;
				    root 	= cJSON_CreateObject();
				    cJSON_AddNumberToObject(root, "JFS_Resp", E_JFS_RESP_TERMINATE);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
					cJSON_Delete(root);
					console_send_responce_to_console_xface(s_Message_Rx);
					return;
				}

				image_header_was_checked = true;
				err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
				if (err != ESP_OK)
				{
					sprintf(str,"esp_ota_begin failed (%s)", esp_err_to_name(err));
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				   esp_ota_abort(update_handle);
				   image_header_was_checked = false;
				    root 	= cJSON_CreateObject();
				    cJSON_AddNumberToObject(root, "JFS_Resp", E_JFS_RESP_TERMINATE);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
					cJSON_Delete(root);
					console_send_responce_to_console_xface(s_Message_Rx);
				   return;
				}
				// Disable interrupts and events
				{
//					Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "DEINIT");
					cJSON *my_JSON = cJSON_CreateObject();
					cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 0);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
					Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "SETSERVCONNFLAG");
					cJSON_Delete(my_JSON); // Free the cJSON object

					cJSON *my_JSON1 = cJSON_CreateObject();
					cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 0);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);
					Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "SETEVTTASKFG");
					cJSON_Delete(my_JSON1); // Free the cJSON object

					Add_Response_msg("OTA is in progress. Please wait....",s_Message_Rx, payLoadData);

					uint8_t val = 0;
					set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
					set_to_other_actor("CONSOLE", U_INT8, "U0_DBG", &val);
					vTaskDelay(10000/portTICK_PERIOD_MS);
				}
			}
			else
			{
				Add_Response_msg("Received package is not fit len",s_Message_Rx, payLoadData);
			    esp_ota_abort(update_handle);
			    image_header_was_checked = false;
			    root 	= cJSON_CreateObject();
			    cJSON_AddNumberToObject(root, "JFS_Resp", E_JFS_RESP_TERMINATE);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
				cJSON_Delete(root);
				console_send_responce_to_console_xface(s_Message_Rx);
				return;
			}

			for(int i= 0; i<strlen(new_app_info.version); i++)
			{
			   if(new_app_info.version[i]=='_')
				   fm_version[i] = '.';
			   else
				   fm_version[i] = new_app_info.version[i];
			}
			sprintf(str,"New firmware version = %s, New Project name = %s ",fm_version, new_app_info.project_name);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
		}  //end of if (image_header_was_checked == false)
		err = esp_ota_write( update_handle, (const char *)write_data, size);
		cJSON *out_JSON 	= cJSON_CreateObject();
		if(out_JSON != NULL)
		{
			if(err == ESP_OK)
			{
				File_length += size;
				cJSON_AddNumberToObject(out_JSON, "JFS_Resp", E_JFS_RESP_OK);
				CRC_OTA = gen_crc161(CRC_OTA, (const uint8_t*) write_data, size);
			}
			else
			{
				esp_ota_abort(update_handle);
				cJSON_AddNumberToObject(out_JSON, "JFS_Resp", E_JFS_RESP_ERR);
			}
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(out_JSON);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
		count++;
	   if(count > 10)  //50
	   {
			count = 0;
			sprintf(str,"Firmware update progress:  %0.2f %%", ((float)((float)File_length/(float)File_size)*100));
			Add_Response_msg(str,s_Message_Rx, payLoadData);
	   }
    }  //end of while(1)

    if(File_length >= File_size)
    {
    	char CRC16Arr[5];        //send CRC to HTTP actor
    	sprintf(CRC16Arr,"%04X",CRC_OTA);
		cJSON *jsonObject = cJSON_CreateObject();
		cJSON_AddStringToObject(jsonObject, "CRC16", CRC16Arr);
		cJSON_AddNumberToObject(jsonObject, "FILE_SIZE", File_size);
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
		cJSON_Delete(jsonObject);

		err = esp_ota_end(update_handle);
		if (err != ESP_OK)
		{
			uint8_t val = 1;
			set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
			sprintf(str, "esp_ota_end failed (%s). Preparing to restart system...", esp_err_to_name(err));
			Add_Response_msg(str, s_Message_Rx, payLoadData);
		   Restart_ESP_Xface(1);
		}
		else
		{
			Add_Response_msg("esp_ota_end successful", s_Message_Rx, payLoadData);
		}
		image_header_was_checked = false;  // Reinitialize all parameters if 'ESP update firmware' command will not receive
		File_length = 0;
		update_handle = 0;
		CRC_OTA = 0x00;
    }
}

void Firmware_Update(AMessage_st* s_Message_Rx)
{
	int err = -1;
	char str[200] = {0};
	char ESP_Reset_flag = 1;

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
		uint8_t val = 1;
		set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
       sprintf(str, "esp_ota_set_boot_partition failed (%s). Preparing to restart system...", esp_err_to_name(err));
       Add_Response_msg(str,s_Message_Rx, payLoadData);
       Restart_ESP_Xface(1);
    }
	cJSON* object_JSON = cJSON_CreateObject();  // write received firmware version in the SPIFFS file.
    cJSON_AddStringToObject(object_JSON, "FILE_NAME", (char*)Device_File);
	cJSON_AddStringToObject(object_JSON, "FIRMWARE_VERSION", fm_version);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(object_JSON, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(SPIFFS,"SPIFFS",payLoadData,strlen(payLoadData),"WR_PARA");
	cJSON_Delete(object_JSON);

    if ((strncmp(new_app_info.version, "701", 3) == 0) && ((strncmp(running_app_info.version, "702", 3) == 0) || (strncmp(running_app_info.version, "703", 3) == 0)))
    {
   	 //Copy actor.json files from JFS to SPIFFS.
    	COPY_JFS_SPIFFS_proc(s_Message_Rx);
   	    ESP_Reset_flag = 0;  // reset ESP from JFS after file transfer process
    }
	if(ESP_Reset_flag ==1)    //If ESP_Reset_flag = 1, then reset esp here otherwise reset from SPIFFS/JFS
	{
		Restart_ESP_Xface(1);
	}
}

#define POLYNOMIAL 0xA001  // Define the polynomial used for CRC calculation

uint16_t gen_crc161(uint16_t crc, const uint8_t *buf, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        crc ^= buf[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= POLYNOMIAL;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}
