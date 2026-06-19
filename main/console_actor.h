/*
 * Consol_Actor.h
 *
 *  Created on: 20-Jul-2022
 *      Author: Ashwini
 */

#include "stdio.h"

#ifndef _CONSOLE_ACTOR_H_
#define _CONSOLE_ACTOR_H_

#define DEF_U0_DBG_STATE	 	1
#define DEF_U1_DBG_STATE	 	0
#define DEF_JFS_DBG_STATE	 	0
#define DEF_sdcard_DBG_STATE 	0
#define DEF_EEPROM_DBG_STATE 	0
//#define PAYLOAD_SIZE			2048	//1024
#define NUMBER_OF_ACTORS 		24
#define Device_File		 "Device_Information.json"   //SPIFFS file to store device information

extern TickType_t QUE_DELAY;
extern char Hardware_RevH_flag;
extern date_time_t UserDateTimeL_1;

// Need to decide where to place this enum, because consol_actor.h is included to all actors.(Suraj)
enum {
	NTP = 1,
	LED,
	UART,
	PUSHBUTTON,
	FILE_SYSTEM,
    HTTP,
	WIFI,
	ETH,
	BLE,
	IHUB,
	WEB_SERVER,
	SMTP_CLIENT,
	IDLE,
	SPIFFS,
	TCP_SERVER,
	UDP,
	SQL,
	CONSOLE,
	LIGHTING,
	SYSTEM,
	EVENT_ACTOR,
	SYS_FILES,
	B394_DI,
	MODEL_225,
	//as per requirement other queue will be insert
};

typedef enum {
	E_NET_ZERO_STATE = 0,
	E_NET_NOT_CONNECTED,
	E_NET_CONNECTED,
}net_et;

typedef enum {
	Disable = 0,
	Enable,
}status;

typedef struct {          //Actor Queue
	char name[MAX_CMD_LEN];
	int  type;
}ActorQueue;

extern ActorQueue A_Queue[NUMBER_OF_ACTORS];

enum {
	MSG_STR = 1,
	JSON_STR,
};
#define MAX_FILE_URL_LEN 200
#define MAX_FILE_NAME_LEN 50
#define MAX_FILE_MODE_LEN 10

typedef struct {
//    char url[MAX_FILE_URL_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    char mode[MAX_FILE_MODE_LEN];
    int64_t file_size;
    char *buffer;
} FileDownload_t;

void mSecDelay(uint32_t DelayCnt);
void Console_Initialize();
void console_send_responce_to_console_xface(AMessage_st *s_Message_new) ;
void console_ActorWriteToConsole_xface(AMessage_st *s_Message);
void console_MessageRelease_xface(char *argBuf);
void console_que_sel(AMessage_st* s_Message);
uint64_t get_current_rtc_time_ms(uint8_t setval);
int16_t get_gmt(void);
uint8_t get_dst(void);
#endif /* MAIN_CONSOLE_ACTOR_H_ */
