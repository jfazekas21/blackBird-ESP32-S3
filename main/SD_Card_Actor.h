/*
 * SD_Card_Actor.h
 *
 *  Created on: 26-Sep-2022
 *      Author: Acer
 */

#ifndef MAIN_SD_CARD_ACTOR_H_
#define MAIN_SD_CARD_ACTOR_H_

//#define MOUNT_POINT "/sdcard"

void SD_Read(char *File_name, char str_Hex);
//void free_space(void);
void SD_Write(char *File_name, char *buffer, char str_Hex);
//void SD_Card_ConsoleWriteToActor_xface(void *msg);
int SD_card_Write(char *payload, AMessage_st* s_Message_Rx);
int SD_card_Read(char *payload, char* data, AMessage_st* s_Message_Rx);
int init_SD_Card(void *a, void *b);
int Delete_file(char *payload, AMessage_st* s_Message_Rx);
int SD_Rename_file(char *payload, AMessage_st* s_Message_Rx);
int SD_Create_Directory(char *payload, AMessage_st* s_Message_Rx);
int SD_Delete_Directory(char *payload, AMessage_st* s_Message_Rx);
//int SD_Get_File_List(char *data);
void SD_Get_File_List(AMessage_st* s_Message_Rx);
int SD_Write_Variables(const char* payload, AMessage_st* s_Message_Rx);
void SD_Read_Variables(const char* payload, AMessage_st* s_Message_Rx);
void SD_SMTP_Read_File(void *pvParameters __attribute__((unused)));
int32_t SD_GET_FILE_SIZE(char* payload, AMessage_st* s_Message_Rx);
#endif /* MAIN_SD_CARD_ACTOR_H_ */
