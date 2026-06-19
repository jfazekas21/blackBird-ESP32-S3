/*
 * File_Manager_Actor.h
 *
 *  Created on: 02-Aug-2023
 *      Author: Amruta
 */

#include "Config.h"

#ifndef MAIN_FILE_MANAGER_ACTOR_H_
#define MAIN_FILE_MANAGER_ACTOR_H_
#define JSON_FILE_SIZE 	8192 //4096    //Used in logic written by Sanjeev sir in  file manager actor
								// used in write var, read var, read file, and read file for SMTP in JFS and SD card
#define MOUNT_POINT "/sdcard"
extern PSRAM_ATTR_BSS  char payLoadData_Chunk_Read[MAX_JSON_PAYLOAD_BYTES];
extern PSRAM_ATTR_BSS  char payLoadData_Event[MAX_JSON_PAYLOAD_BYTES];
extern PSRAM_ATTR_BSS  char payLoadData_Write[MAX_JSON_PAYLOAD_BYTES];
extern PSRAM_ATTR_BSS char payLoadData_Var[MAX_JSON_PAYLOAD_BYTES];
extern PSRAM_ATTR_BSS  char payLoadData_WIFI[MAX_JSON_PAYLOAD_BYTES];
extern PSRAM_ATTR_BSS  char JFS_READ_data_buffer[4096];
extern QueueHandle_t 	SMTP_Read_Queue;
extern uint8_t SMTP_READ_pucQueueStorage[FILE_SMTP_READ_TASK_QUEUE_LENGTH * FILE_SMTP_READ_TASK_QUEUE_ITEMSIZE];
extern StaticQueue_t SMTP_READ_pxQueueBuffer;
extern char Write_variable_FirstTime;
extern int Write_count;
extern uint8_t *SMTP_payload;
extern uint16_t msg_id_u16;
extern AMessage_st s_Message_Rx_New;
void File_ConsoleWriteToActor_xface(void *msg);
int SD_card_Read_Chunk(char *data, int size, FILE *fileptr);
void SD_Extract_folders(const char *path);
enum {
	FOTA_NOT_INITIALIZED = 0,
	FOTA_IN_PROGRESS = 1
};

#endif /* MAIN_FILE_MANAGER_ACTOR_H_ */
