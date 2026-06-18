/*
 * JFS_NOR_Actor.h
 *
 *  Created on: 21-Oct-2022
 *      Author: Acer
 */

#ifndef _JFS_FLASH_ACTOR_H_
#define _JFS_FLASH_ACTOR_H_

#define VOLUME_NAME       "nor:0:"
#define UNIT_NO           0


#define JFS_LOG_FILE_NAME_LEN 32
#define JFS_FILE_NAME_MAX_LEN 64
#define JFS_FILE_DATA_MAX_LEN 2048  //64

#define  JFS_Root_Path   "Root/"


//------------------------- Temp Test workspace ------------------------------------------------//
void JFS_actor_setup();
void JFS_SampleGetVolumeInfo(AMessage_st* s_Message_Rx);
int JFS_FLASH_Read_File(char *File_name	, char* File_data,	const uint16_t Read_data_len, uint32_t *file_ptr);	// read from the JFS flash memory
int JFS_FLASH_Write_File(char *File_name, const char *data, int16_t size, char *OpenType, uint8_t Send_Ack, char* Payload);
void JFS_Read_File(const uint8_t* payload,AMessage_st* s_Message_Rx);
void JFS_Get_File_List(char* buffer, AMessage_st* s_Message_Rx);
void JFS_Rename(const char *payload_p8, AMessage_st* s_Message_Rx);
void JFS_FLASH_Delete_File(const char *payload,AMessage_st* s_Message_Rx);									// Delete File from JFS flash memory using name
void JFS_FLASH_Create_Dir(const char *Dir_name,AMessage_st* s_Message_Rx);										// Create Directory with name in JFS flash memory
void JFS_FLASH_Delete_Dir(const char *Dir_name, AMessage_st* s_Message_Rx);										// Delete Dir from JFS flash memory using name
void JFS_Write_Variable(uint8_t* payload,AMessage_st* s_Message_Rx);
void JFS_Read_Variable(const uint8_t* payload,AMessage_st* s_Message_Rx);
void JFS_Write_File(char* payload,AMessage_st* s_Message_Rx);
void Save_Audit_log (AMessage_st* s_Message_Rx);
void OTA_JFS_task(AMessage_st* s_Message_Rx_New);
int32_t JFS_GET_FILE_SIZE(char* payload, AMessage_st* s_Message_Rx);
void Save_WIFI_Audit_log (AMessage_st* s_Message_Rx);
void JFS_SMTP_Read_File(void *pvParameters __attribute__((unused)));
void Create_Directories(AMessage_st* s_Message_Rx);
int JFS_FLASH_Write_File_HP(char *File_name, const char *data, int16_t size, char *OpenType, uint8_t Send_Ack);
int JFS_FLASH_Write_File1(char *File_name, const char *data, int16_t size, char *OpenType, uint32_t Offset) ;
void unmount_JFS(void);
void Save_Event_log(AMessage_st* s_Message_Rx);
void Write_Bin_File_OTA(char* write_data, int16_t size, int64_t File_size, AMessage_st* s_Message_Rx);
void Firmware_Update(AMessage_st* s_Message_Rx);
uint16_t gen_crc161(uint16_t crc, const uint8_t *buf, uint16_t len);
#endif /* MAIN_JFS_FLASH_ACTOR_H_ */
