/*
 * SD_Card_Actor.c
 *
 *  Created on: 26-Sep-2022
 *      Author: Amruta Dixit
 */
#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "SD_Card_Actor.h"
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "esp_timer.h"
#include "sdmmc_cmd.h"
#include "File_Manager_Actor.h"
extern TaskHandle_t Read_Handle_SD;

#define OBJ_QUE_COUNT	2
//#define MOUNT_POINT "/sdcard"
#define MAX_DATA_LEN  1024
#define HSPI_HOST SPI3_HOST

//#define PIN_NUM_MISO  12
//#define PIN_NUM_MOSI  13
//#define PIN_NUM_CLK   14
//#define PIN_NUM_CS    15

#if defined(B480) || defined(B543)|| defined(B542) || defined(B394)
#define PIN_NUM_MISO  GPIO_NUM_42
#define PIN_NUM_MOSI  GPIO_NUM_40
#define PIN_NUM_CLK   GPIO_NUM_41
#define PIN_NUM_CS    GPIO_NUM_46
#elif defined (B553)
#define PIN_NUM_MISO  GPIO_NUM_13
#define PIN_NUM_MOSI  GPIO_NUM_11
#define PIN_NUM_CLK   GPIO_NUM_12
#define PIN_NUM_CS    GPIO_NUM_46
#else
#define PIN_NUM_MISO  GPIO_NUM_NC
#define PIN_NUM_MOSI  GPIO_NUM_NC
#define PIN_NUM_CLK   GPIO_NUM_NC
#define PIN_NUM_CS    GPIO_NUM_NC
#endif

#define SPI_DMA_CHAN    1
PSRAM_ATTR_BSS static char str[100];

//static const char * THIS_ACTOR = "FILE_SYSTEM";

//static char keysTemp[MAX_OUT_SIZE];
//static char keysValue[MAX_OUT_SIZE];

sdmmc_card_t *card;
PSRAM_ATTR_BSS  static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
void Release_bus(void);
int init_SD_Card(void *a, void *b) {
//	gpio_pullup_en(GPIO_NUM_12);
	gpio_pullup_en(PIN_NUM_MISO);
	gpio_pullup_en(PIN_NUM_CS);
	esp_err_t ret;

	// Options for mounting the filesystem.
	// If format_if_mount_failed is set to true, SD card will be partitioned and
	// formatted in case when mounting fails.

	esp_vfs_fat_sdmmc_mount_config_t mount_config = {

#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
	        .format_if_mount_failed = true,
	#else
			.format_if_mount_failed = false,
#endif // EXAMPLE_FORMAT_IF_MOUNT_FAILED
			.max_files = 5,
			.allocation_unit_size = 16 * 1024 };

	const char mount_point[] = MOUNT_POINT;

	// Use settings defined above to initialize SD card and mount FAT filesystem.
	// Note: esp_vfs_fat_sdmmc/sdspi_mount is all-in-one convenience functions.
	// Please check its source code and implement error recovery when developing
	// production applications.

	sdmmc_host_t host = SDSPI_HOST_DEFAULT();
	spi_bus_config_t bus_cfg = { .mosi_io_num = PIN_NUM_MOSI, .miso_io_num =
			PIN_NUM_MISO, .sclk_io_num = PIN_NUM_CLK, .quadwp_io_num = -1,
			.quadhd_io_num = -1, .max_transfer_sz = 4000, };
#if defined(B480) || defined(B543)|| defined(B542) || defined(B394)
	ret = spi_bus_initialize(host.slot, &bus_cfg,SDSPI_DEFAULT_DMA);
#else
	ret = spi_bus_initialize(HSPI_HOST, &bus_cfg, SDSPI_DEFAULT_DMA);
#endif

	if (!((ret == ESP_OK) || (ret == ESP_ERR_INVALID_STATE))) {
		return -2;
	}

	// This initializes the slot without card detect (CD) and write protect (WP) signals.
	// Modify slot_config.gpio_cd and slot_config.gpio_wp if your board has these signals.
	sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
	slot_config.gpio_cs = PIN_NUM_CS;
#if defined(B480) || defined(B543)|| defined(B542) || defined(B394)
	slot_config.host_id = host.slot;
#else
	slot_config.host_id = HSPI_HOST;	    //host.slot;
#endif


	ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config,
			&mount_config, &card);
	return ret;

	// Card has been initialized, print its properties
	//sdmmc_card_print_info(stdout, card);
}

void Release_bus(void)   // this function is not yet used
{
	esp_err_t ret;

	// Unmount the filesystem
	ret = esp_vfs_fat_sdcard_unmount(MOUNT_POINT, card);
	if (ret != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
		printf("Failed to unmount filesystem: %s\n", esp_err_to_name(ret));
#endif
	}

	// Deinitialize the SPI bus
	ret = spi_bus_free(SPI2_HOST);
	if (ret != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
		printf("Failed to free SPI bus: %s\n", esp_err_to_name(ret));
#endif
	}

	return;
}

int SD_card_Write(char *payload, AMessage_st* s_Message_Rx) {
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char filename[100] = {0},filedata[4096] = {0};

//	printf("\n\n SD card payload = %s \n\n", payload);
	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
	Add_Response_msg(str,s_Message_Rx, payLoadData);
	return ESP_FAIL;
	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	if(name_JSON->valuestring != NULL)
	{
		if(strlen(name_JSON->valuestring) != 0)
		{
			SD_Extract_folders(name_JSON->valuestring);
			strcpy(filename, MOUNT_POINT);
			strcat(filename, "/");
			strcat(filename,name_JSON->valuestring);

			name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_DATA");
			strcpy(filedata,name_JSON->valuestring);
			struct stat st;
			if (stat(filename, &st) == 0) {
				unlink(filename);
			}
			FILE *f = fopen(filename, "w");
			if (f == NULL) {
				cJSON_Delete(in_JSON);
				return ESP_FAIL;
			}
			fprintf(f, filedata);  // write string data
			fclose(f);
		}
	}
	cJSON_Delete(in_JSON);  // added newly
	return ESP_OK;
}

//void free_space(void)   //this function is not yet used
//{
//	FATFS *fs;
//	DWORD fre_clust, fre_sect, tot_sect;
//
//	/* Get volume information and free clusters of sdcard */
//	int res = f_getfree("/sdcard/", &fre_clust, &fs);
//	if (res) {
//		//return ES_Unspecified;
////		printf("\n Fail");
//	}
//
//	/* Get total sectors and free sectors */
//	tot_sect = (fs->n_fatent - 2) * fs->csize;
//	fre_sect = fre_clust * fs->csize;
//	int64_t tmp_total_bytes = tot_sect * FF_SS_SDCARD;
//	int64_t tmp_free_bytes = fre_sect * FF_SS_SDCARD;
//
//	/* Print the free space in bytes */
////	printf("%llu total bytes. %llu free bytes. sector_size=%u", tmp_total_bytes,
////			tmp_free_bytes, FF_SS_SDCARD);
//}

int SD_card_Read(char *payload, char *data,AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;

	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return ESP_FAIL;
		}
	char *filename = (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (filename == NULL)
	{
//		printf("Memory allocation failed\n");
		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData);
		return ESP_FAIL;
	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcpy(filename, MOUNT_POINT);
	strcat(filename, "/");
	strcat(filename,name_JSON->valuestring);
	//free_space();

	FILE *f = fopen(filename, "r");
	if (f == NULL) {
//			printf( "\n Failed to open file for reading\r\n");
		return -1;
	}


//	struct stat st;
//	if (stat(File_name, &st) == 0) {
//		//	printf("\n File size:%ld\n",st.st_size);
//	}
		while (fgets(data, MAX_DATA_LEN, f) != NULL)  // Read string
		{
			char *pos = strchr(data, '\n');
			if (pos) {
				*pos = '\0';
			}
		}

//	if (str_Hex == 2) {
//		//  printf("\n file size2 =%d",(int)st.st_size);
//
//		for (int i = 0; i < st.st_size; i++) // Read data/Binary
//				{
//			char read_data = fgetc(f);
//			line[i] = read_data;
//			// printf("%x  ",(int)line[i]);
//
//		}
//		send_hex_to_console(line, st.st_size);
//	}

	fclose(f);
	free(filename);
	//free(payload);
	cJSON_Delete(in_JSON);
	return 0;
}

int Delete_file(char *payload, AMessage_st* s_Message_Rx)
{
	int ret = ESP_OK;
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char filename[50] = {0}; // (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (filename == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}
	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return (-1);
		}
	name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcpy(filename, MOUNT_POINT);
	strcat(filename, "/");
	strcat(filename,name_JSON->valuestring);

	struct stat st;
	if (stat(filename, &st) == 0)  // file to be deleted is present
	{
		ret = unlink(filename);
	}
	else
	{
		ret = ESP_FAIL;          // file to be deleted is not present
	}
//	free(filename);
	//free(payload);
	cJSON_Delete(in_JSON);
	return ret;
}

int JFS_FLASH_Rename_File(char *old_name, char *new_name)
{
	int ret = ESP_OK;
	char new_path[50] = {0}, old_path[50] = {0}; // 			= (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (new_path == NULL)
//	{
//		printf("Memory allocation failed\n");
//		return ESP_FAIL;
//	}
//	char *old_path 			= (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (old_path == NULL)
//	{
//		printf("Memory allocation failed\n");
//		return ESP_FAIL;
//	}

	strcpy(old_path, MOUNT_POINT);
	strcat(old_path, "/");
	strcat(old_path,old_name);

	strcpy(new_path, MOUNT_POINT);
	strcat(new_path, "/");
	strcat(new_path,new_name);

	ret = rename(old_path, new_path);   // rename the file

//	free(new_path);
//	free(old_path);
	return ret;
}


int SD_Rename_file(char *payload, AMessage_st* s_Message_Rx)
{
	int ret = ESP_FAIL;
	char Type[20] = {0},  OldName[50] = {0}, NewName[50] = {0};			//= (char*) heap_caps_calloc(10, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (Type == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}
//	char *OldName 		= (char*) heap_caps_calloc(20, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (OldName == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}
//	char *NewName 		= (char*) heap_caps_calloc(20, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (NewName == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}

	cJSON *my_JSON 		= NULL;
	cJSON *myName_JSON 	= NULL;

	my_JSON = cJSON_Parse((char*) payload);
	if (my_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return ret;
		}

	myName_JSON = cJSON_GetObjectItem(my_JSON, "type");  //for file rename, type=file and for directory rename, type=dire
	//Type      	= cJSON_GetStringValue(myName_JSON);
	strcpy(Type,myName_JSON->valuestring);

	myName_JSON = cJSON_GetObjectItem(my_JSON, "OLD_NAME");
	//OldName   	= cJSON_GetStringValue(myName_JSON);
	strcpy(OldName,myName_JSON->valuestring);

	myName_JSON = cJSON_GetObjectItem(my_JSON, "NEW_NAME");
	//NewName   	= cJSON_GetStringValue(myName_JSON);
	strcpy(NewName,myName_JSON->valuestring);
//	if((strcmp((char*)OldName,"Audit/AuditLog.txt")==0) || (strcmp((char*)OldName,"Audit")==0))
	if((strcasecmp((char*)OldName,"Audit/AuditLog.txt")==0) || (strcasecmp((char*)OldName,"Audit")==0))
	{
		return -2;   // Audit log or Audit directory cannot be renamed
	}

	ret = JFS_FLASH_Rename_File(OldName, NewName);	// Rename an existing File
	cJSON_Delete(my_JSON);
//	free(Type);
//	free(OldName);
//	free(NewName);
	//free(payload);
	return ret;
}

int SD_Create_Directory(char *payload, AMessage_st* s_Message_Rx)
{
	int ret = ESP_OK;
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
//	printf("\n SD_card_Write, payload=%s", payload);
	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return ret;
		}
	char dir_path [50]= {0}; //(char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (dir_path == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "DIR_NAME");
	strcpy(dir_path, MOUNT_POINT);
	strcat(dir_path, "/");
	strcat(dir_path,name_JSON->valuestring);

	ret = mkdir(dir_path, 0777);   // create directory
//	free(dir_path);
	//free(payload);
	cJSON_Delete(in_JSON);
	return ret;
}

int SD_Delete_Directory(char *payload, AMessage_st* s_Message_Rx)
{
	int ret = ESP_FAIL;
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;


//	printf("\n SD_card_Write, payload=%s", payload);
	in_JSON = cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return(ret);
		}
	char dir_path [50]= {0}; //(char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (dir_path == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return ESP_FAIL;
//	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "DIR_NAME");
	strcpy(dir_path, MOUNT_POINT);
	strcat(dir_path, "/");
	strcat(dir_path,name_JSON->valuestring);

	ret = rmdir(dir_path);   // remove/ delete the directory
//	free(dir_path);
	//free(payload);
	cJSON_Delete(in_JSON);
	return ret;
}

//#define MAX_RECURSION_DEPTH  5
//
//void SD_Get_File_List_Recursive(const char *base_path, cJSON *json_array, AMessage_st* s_Message_Rx, int depth) {
//    if (depth > MAX_RECURSION_DEPTH) {
//        char str[100];
//        snprintf(str, sizeof(str), "Maximum recursion depth reached: %d", depth);
//        Add_Response_msg(str, s_Message_Rx, payLoadData);
//        return;
//    }
//
//    DIR *dir = opendir(base_path);
//    if (dir == NULL) {
//        char str[400];
//        snprintf(str, sizeof(str), "Failed to open directory: %s", base_path);
//        Add_Response_msg(str, s_Message_Rx, payLoadData);
//        return;
//    }
//
//    struct dirent *entry;
//    while ((entry = readdir(dir)) != NULL) {
//        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
//            continue;
//        }
//
//        char full_path[256];
//        if (snprintf(full_path, sizeof(full_path), "%s/%s", base_path, entry->d_name) >= sizeof(full_path)) {
//            char str[400];
//            snprintf(str, sizeof(str), "Path too long: %s/%s", base_path, entry->d_name);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        struct stat file_stat;
//        if (stat(full_path, &file_stat) != 0) {
//            char str[400];
//            snprintf(str, sizeof(str), "Failed to stat file: %s", full_path);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        cJSON *file_obj = cJSON_CreateObject();
//        if (file_obj == NULL) {
//            char str[400];
//            snprintf(str, sizeof(str), "Failed to create JSON object for file: %s", entry->d_name);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        cJSON_AddStringToObject(file_obj, "name", entry->d_name);
//        cJSON_AddStringToObject(file_obj, "type", S_ISDIR(file_stat.st_mode) ? "directory" : "file");
//        if (S_ISREG(file_stat.st_mode)) {
//            cJSON_AddNumberToObject(file_obj, "size", file_stat.st_size);
//        }
//
//        cJSON_AddItemToArray(json_array, file_obj);
//
//        if (S_ISDIR(file_stat.st_mode)) {
//            cJSON *subdir_array = cJSON_CreateArray();
//            if (subdir_array == NULL) {
//                char str[400];
//                snprintf(str, sizeof(str), "Failed to create JSON array for subdirectory: %s", entry->d_name);
//                Add_Response_msg(str, s_Message_Rx, payLoadData);
//                continue;
//            }
//
//            cJSON_AddItemToObject(file_obj, "contents", subdir_array);
//            SD_Get_File_List_Recursive(full_path, subdir_array, s_Message_Rx, depth + 1);
//        }
//    }
//
//    closedir(dir);
//}

static char entry_u8 = 0;
#define max_entery  32

void SD_Get_File_List_Recursive(const char *base_path, cJSON *json_array, AMessage_st* s_Message_Rx) {
    struct dirent *entry;
//    char entry_u8 = 0;
    char full_path[256];
    struct stat file_stat;
    char str[400] = {0};

    entry_u8++;
    if(entry_u8 > max_entery)   // to avoid infinite loop
    {
    	return;
    }
    DIR *dir = opendir(base_path);
    if (dir == NULL) {
        sprintf(str, "Failed to open directory: %s", base_path);
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." entries
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Clear full_path and construct it manually
        memset(full_path, 0, sizeof(full_path));
        strcpy(full_path, base_path);

        // Ensure the combined path fits within full_path
        size_t base_path_len = strlen(base_path);
        if (base_path_len + strlen(entry->d_name) + 2 > sizeof(full_path)) { // +2 for '/' and '\0'
            sprintf(str, "Path too long: %s/%s", base_path, entry->d_name);
            Add_Response_msg(str, s_Message_Rx, payLoadData);
            continue;
        }

        // Append '/' if not already present
        if (base_path[base_path_len - 1] != '/') {
            strcat(full_path, "/");
        }

        // Append the entry name
        strcat(full_path, entry->d_name);

        // Get file stats
        if (stat(full_path, &file_stat) != 0) {
            sprintf(str, "Failed to stat file: %s", full_path);
            Add_Response_msg(str, s_Message_Rx, payLoadData);
            continue;
        }

        // Create a JSON object for this entry
        cJSON *file_obj = cJSON_CreateObject();
        if (file_obj == NULL) {
            sprintf(str, "Failed to create JSON object for file: %s", entry->d_name);
            Add_Response_msg(str, s_Message_Rx, payLoadData);
            continue;
        }

        cJSON_AddStringToObject(file_obj, "name", entry->d_name);
        cJSON_AddStringToObject(file_obj, "type", S_ISDIR(file_stat.st_mode) ? "directory" : "file");
        if (S_ISREG(file_stat.st_mode)) {
            cJSON_AddNumberToObject(file_obj, "size", file_stat.st_size);
        }

        // Add the file object to the JSON array
        cJSON_AddItemToArray(json_array, file_obj);

        // If the entry is a directory, recursively process it
        if (S_ISDIR(file_stat.st_mode)) {
            cJSON *subdir_array = cJSON_CreateArray();
            if (subdir_array == NULL) {
                sprintf(str, "Failed to create JSON array for subdirectory: %s", entry->d_name);
                Add_Response_msg(str, s_Message_Rx, payLoadData);
                continue;
            }

            // Add the subdirectory array to the current directory JSON object
            cJSON_AddItemToObject(file_obj, "contents", subdir_array);

            // Recursively list the subdirectory
            SD_Get_File_List_Recursive(full_path, subdir_array, s_Message_Rx);
        }
    }

    closedir(dir);
}

void SD_Get_File_List(AMessage_st* s_Message_Rx) {
    char base_path[100] = {0};
    strcpy(base_path, MOUNT_POINT);

    // Create a JSON array for the result
    cJSON *json_array = cJSON_CreateArray();
    if (json_array == NULL) {
        Add_Response_msg("Failed to create JSON array", s_Message_Rx, payLoadData);
        return;
    }

    // Start recursive file listing
    entry_u8 = 0;

    SD_Get_File_List_Recursive(base_path, json_array, s_Message_Rx);

    cJSON *json_file = cJSON_CreateObject();
    if (json_file == NULL) {
        Add_Response_msg("Failed to create JSON file object", s_Message_Rx, payLoadData);
        cJSON_Delete(json_array);
        return;
    }
    cJSON_AddItemToObject(json_file, "FILES", json_array);
    // Prepare the payload
    memset(payLoadData, 0, sizeof(payLoadData));
    cJSON_PrintPreallocated(json_file, payLoadData, sizeof(payLoadData), false);
    strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
    cJSON_Delete(json_file);
    console_send_responce_to_console_xface(s_Message_Rx);
}

//void SD_Get_File_List(AMessage_st* s_Message_Rx) {
//    // Open the directory
//    char base_path[100] = {0}, str[400] = {0};
//    strcpy(base_path, MOUNT_POINT);
//    DIR *dir = opendir(base_path);
//    if (dir == NULL) {
//        Add_Response_msg("Failed to open SD card directory", s_Message_Rx, payLoadData);
//        return;
//    }
//
//    // Create a JSON object for the result
//    cJSON *json_array = cJSON_CreateArray();
//    if (json_array == NULL) {
//        Add_Response_msg("Failed to create JSON array", s_Message_Rx, payLoadData);
//        closedir(dir);
//        return;
//    }
//
//    // Traverse the directory
//    struct dirent *entry;
//    char full_path[256];
//    struct stat file_stat;
//
//    while ((entry = readdir(dir)) != NULL) {
//        // Clear full_path and construct it manually
//        memset(full_path, 0, sizeof(full_path));
//        strcpy(full_path, base_path);
//        size_t base_path_len = strlen(base_path);
//
//        // Ensure the combined path fits within full_path
//        if (base_path_len + strlen(entry->d_name) + 2 > sizeof(full_path)) { // +2 for '/' and '\0'
//            sprintf(str, "Path too long: %s/%s", base_path, entry->d_name);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue; // Skip this entry
//        }
//
//        // Append '/' if not already present
//        if (base_path[base_path_len - 1] != '/') {
//            strcat(full_path, "/");
//        }
//
//        // Append the entry name
//        strcat(full_path, entry->d_name);
//
//        // Get file stats
//        if (stat(full_path, &file_stat) != 0) {
//            sprintf(str, "Failed to stat file: %s", full_path);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        // Create a JSON object for each entry
//        cJSON *file_obj = cJSON_CreateObject();
//        if (file_obj == NULL) {
//            sprintf(str, "Failed to create JSON object for file: %s", entry->d_name);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        // Add name, type, and size to the JSON object
//        cJSON_AddStringToObject(file_obj, "name", entry->d_name);
//        cJSON_AddStringToObject(file_obj, "type", S_ISDIR(file_stat.st_mode) ? "directory" : "file");
//        if (S_ISREG(file_stat.st_mode)) {
//            cJSON_AddNumberToObject(file_obj, "size", file_stat.st_size);
//        }
//
//        // Add the file object to the JSON array
//        cJSON_AddItemToArray(json_array, file_obj);
//    }
//
//    closedir(dir);
//
//    // Prepare the payload
//    memset(payLoadData, 0, sizeof(payLoadData));
//    cJSON_PrintPreallocated(json_array, payLoadData, sizeof(payLoadData), false);
//    strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//    cJSON_Delete(json_array);
//    console_send_responce_to_console_xface(s_Message_Rx);
//}

//void SD_Get_File_List(AMessage_st* s_Message_Rx)
//{
//    char directory[] = MOUNT_POINT;
//    char str[300] = {0};
//    DIR *dir = opendir(directory);
//    if (dir == NULL) {
//        sprintf(str, "Failed to open directory: %s", directory);
//        Add_Response_msg(str, s_Message_Rx, payLoadData);
//        return;
//    }
//
//    cJSON *root = cJSON_CreateObject();
//    cJSON *jsonArrayFileName = cJSON_CreateArray();
//    struct dirent *entry;
//    char file_path[256];  // Larger buffer to hold full file path
//    struct stat file_stat;         // Structure to store file details
//
//    while ((entry = readdir(dir)) != NULL) {
//        // Safely create the full file path
//        int written = snprintf(file_path, sizeof(file_path), "%s/%s", directory, entry->d_name);
//        if (written < 0 || written >= sizeof(file_path)) {
//            sprintf(str, "Path too long for file: %s", entry->d_name);
//            Add_Response_msg(str, s_Message_Rx, payLoadData);
//            continue;
//        }
//
//        if (entry->d_type == DT_REG) { // If the entry is a regular file
//            if (stat(file_path, &file_stat) == 0) { // Get file stats
//                cJSON *file_object = cJSON_CreateObject();
//                cJSON_AddStringToObject(file_object, "name", entry->d_name);
//                cJSON_AddNumberToObject(file_object, "size", file_stat.st_size); // Add file size
//                cJSON_AddItemToArray(jsonArrayFileName, file_object);
////                cJSON_AddItemToObject(root, "FILE", file_object);
//            } else {
//                sprintf(str, "Failed to stat file: %s", file_path);
//                Add_Response_msg(str, s_Message_Rx, payLoadData);
//            }
//        } else if (entry->d_type == DT_DIR) { // If the entry is a directory
//        	 cJSON_AddItemToArray(jsonArrayFileName, cJSON_CreateString(entry->d_name));
////            cJSON_AddStringToObject(root, "DIRECTORY", entry->d_name);
//        } else {
//        	 cJSON_AddItemToArray(jsonArrayFileName, cJSON_CreateString(entry->d_name));
////            cJSON_AddStringToObject(root, "OTHERS", entry->d_name);
//        }
//    }
//
//    cJSON_AddItemToObject(root, "files", jsonArrayFileName);
//    memset(payLoadData, 0, sizeof(payLoadData));
//    cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
//    strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
//
//    cJSON_Delete(root);
//    closedir(dir);
//    console_send_responce_to_console_xface(s_Message_Rx);
//    strncpy((char*)s_Message_Rx->payload_p8, payLoadData, strlen(payLoadData));
//}

//int SD_Get_File_List(char *data)  // get the list of files present in 'Audit' folder
//{
//	int ret = ESP_OK, i=1, j=1;
//	char str1[300]={0};
//	char dir_path[50] = {0}; //(char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
////	if (dir_path == NULL)
////	{
////		printf("Memory allocation failed\n");
////		return ESP_FAIL;
////	}
//
//	strcpy(dir_path, MOUNT_POINT);
//
//	DIR *dir = opendir(dir_path);
//	if (dir == NULL) {
//#ifdef ENABLE_PRINT_MSG
//		printf("\n Failed to open directory.");
//#endif
//		return ESP_FAIL;
//	}
//	strcpy(data,"{");
//	struct dirent *entry;
//	while ((entry = readdir(dir)) != NULL)
//	{
//		if (entry->d_type == DT_REG)
//		{
//			memset(str1,0,sizeof(str));
//			sprintf(str1, "\"File %d\": \"%s\"", i, entry->d_name);
//			strcat(data, str1);
//			strcat(data, ",");
//			i++;
//			//printf("\n File: %s", entry->d_name);
//		}
//		else
//			if (entry->d_type == DT_DIR)
//			{
//				memset(str1,0,sizeof(str));
//				sprintf(str1, "\"Directory %d\": \"%s\"", j, entry->d_name);
//				strcat(data, str1);
//				strcat(data, ",");
//				j++;
//				//printf("\n Directory: %s", entry->d_name);
//			}
//	}
//	strcpy(data+strlen(data)-1,"}");  // replace last "," by "}"
//	closedir(dir);
////	free(dir_path);
//	return ret;
//}

int SD_Write_JSON_Variables(char *filename, char* data)   // write JSON variables in the file
{
	FILE *f = fopen(filename, "w");
	if (f == NULL)
	{
		//printf("\n Failed to open file for writing");
		return ESP_FAIL;
	}
	fprintf(f, data);  // write string data
	fclose(f);
	return ESP_OK;
}


void SD_SMTP_Read_File(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
//	int16_t Read_count 	=	0;

	uint8_t filename[50] = {0};
	char DataPtr [JSON_FILE_SIZE];
	char str[100] = {0};

	in_JSON 		= cJSON_Parse((char*) SMTP_payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_Chunk_Read);
		goto exit;
		}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcpy((char*)filename, MOUNT_POINT);
	strcat((char*)filename, "/");
	strcat((char*)filename,name_JSON->valuestring);
#ifdef ENABLE_PRINT_MSG
	printf("\n SD filename= %s", filename);
#endif
	FILE *f = fopen((char*)filename, "r");
	if (f == NULL)
	{
		sprintf(str, "%s file is not present in SD card.", filename);
		Add_Response_msg(str,s_Message_Rx, payLoadData_Chunk_Read);
		cJSON_Delete(in_JSON);
		free(SMTP_payload);
		goto exit;
	}

//	DataPtr = (char*) heap_caps_calloc((JSON_FILE_SIZE+1), sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//	if (DataPtr == NULL)
//	{
////		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData_Chunk_Read);
//		goto exit;
//	}
	while(1)  //do
	{
//		if(SMTP_response_Flag == E_JFS_RESP_OK)  // need to add que
		{
//			SMTP_response_Flag = E_JFS_RESP_WAIT;
			memset(DataPtr,0,(JSON_FILE_SIZE));
			if(fgets(DataPtr, JSON_FILE_SIZE, f) != NULL)
			{
				char *pos = strchr(DataPtr, '\n');
				if (pos)
					*pos = '\0';

//				if(DataPtr[0] == 0x22)
//					sprintf(str,"{\"File Data\":%s}",DataPtr);
//				else
//					sprintf(str,"{\"File Data\":\"%s\"}",DataPtr);

				Add_Response_msg(DataPtr,s_Message_Rx, payLoadData_Chunk_Read);
			}
			else
			{
				fclose(f);
				break;
			}
		}  //end of if(SMTP_response_Flag == 1)
		vTaskDelay(10);
	}//while(Read_count!=0);
	cJSON_Delete(in_JSON);
	exit:
//	if(DataPtr != NULL)
//	{
//		free(DataPtr);
//		DataPtr = NULL;
//	}
	if(SMTP_payload != NULL)
	{
		free(SMTP_payload);
		SMTP_payload = NULL;
	}
	Read_Handle_SD = NULL;
	vTaskDelete(Read_Handle_SD);
}

int32_t SD_GET_FILE_SIZE(char* payload, AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= 	NULL;
	cJSON *name_JSON 	= 	NULL;
//	int16_t Read_count 	=	0;
	uint8_t filename[50] = {0};
	struct stat st;

	//printf("\n SD_GET_FILE_SIZE");
	in_JSON 		= cJSON_Parse((char*) payload);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return (-1);
		}
	name_JSON 		= cJSON_GetObjectItem(in_JSON, "FILE_NAME");
	strcpy((char*)filename, MOUNT_POINT);
	strcat((char*)filename, "/");
	strcat((char*)filename,name_JSON->valuestring);

	if (stat((char*)filename, &st) == 0) {
			cJSON_Delete(in_JSON);
			return ((int32_t)st.st_size);
	}

	cJSON_Delete(in_JSON);
	return (-1);

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

int SD_card_Read_Chunk(char *data, int size, FILE *fileptr)
{
	char read_data = 0;
	int i = 0;
	for (i = 0; i < size; i++) // Read data/Binary
	{
		read_data = fgetc(fileptr);
		data[i] = read_data;
	}
	//printf("\n read count = %d", i);
	return i;
}

void SD_Extract_folders(const char *path) {
    // Make a copy of the path to tokenize it (strtok modifies the string)
    char path_copy[100] = {0}, Dir_name[100] ={0};
    strcpy(Dir_name, MOUNT_POINT);
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
        int err = mkdir(Dir_name, 0777);
		if (err < 0) {
			#ifdef ENABLE_PRINT_MSG
			printf("Error! in creating directory");
			#endif
		}
        // Move to the next token
        token = next_token;
    }
}

