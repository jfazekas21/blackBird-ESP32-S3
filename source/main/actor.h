#ifndef _ACTOR_H_
#define _ACTOR_H_

#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <dirent.h>
#include <stdint.h>
#include <ctype.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/param.h>

#include "esp_timer.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include "esp_wifi_default.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_console.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_console.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "esp_vfs_fat.h"
#include "esp_http_server.h"
#include "esp_attr.h"
#include "esp_sntp.h"
#include "esp_spiffs.h"
#include "esp_sleep.h"
#include "spi_flash_mmap.h"
//#include "esp_spi_flash.h"
#include "esp_eth_mac.h"
#include "esp_chip_info.h"
#include "esp_mac.h"

#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/ledc.h"
#include "driver/sdspi_host.h"
#include "driver/spi_common.h"
#include "driver/rtc_io.h"
#include "driver/i2c.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "protocol_examples_common.h"
#include "linenoise/linenoise.h"
#include "argtable3/argtable3.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "sdmmc_cmd.h"
#include "soc/soc_caps.h"
#include "sdkconfig.h"
#include "cJson.h"
#include "ping.h"

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
#include <mbedtls/base64.h>
#include "portmacro.h"

#ifndef PSRAM_ATTR
#define PSRAM_ATTR __attribute__((section(".ext_ram.data")))
#define PSRAM_ATTR_BSS __attribute__((section(".ext_ram.bss")))
#endif

#define MAX_JSON_PAYLOAD_BYTES 16384	//8192
#define COMMAND_LEN           4096	//2048

#define MAX_Actor_LEN					16
#define MAX_CMD_LEN						32
#define MAX_OUT_SIZE	                128
#define MAX_BUFF_SIZE                   128 //1024 //256
//#define B480
//#define ENABLE_PRINT_MSG
#define JFS_BUFFER_SIZE					400*1024

typedef struct {
    struct property* propTable;					// Actor Property table
    void (*get)(char*,char*);					// Actor Get Property method
    void (*set)(char*,char*);					// Actor Set Property method
    void (*Init)(void*,void*);					// Actor method Init
    void (*print)(char*);						// Actor Log method
    void (*task)(void*);						// SuperVisory Task
    void (*help)(void);
    QueueHandle_t (*TxQueue);
    QueueHandle_t (*RxQueue);
/******************************************/
    void (*method1)(void*,void*);
    void (*method2)(void*,void*);
    void (*method3)(void*,void*);
    void (*method4)(void*,void*);
    void (*method5)(void*,void*);
    void (*method6)(void*,void*);
}Actor_st;

struct property{
  void* name;
  char str_name[32];        //string to match
  int type;
  char access[4];        //string to match
  char HelpString[100];        //string to match
};

//struct ActorQueue{          //Actor Queue
//	char name[MAX_CMD_LEN];
//	int  type;
//};
//extern struct ActorQueue;
typedef struct{
	uint16_t	payload_size;
	uint8_t  	Src_ID_u8;
	uint8_t 	Dest_ID_a8; 	//	[MAX_Actor_LEN];
	uint8_t 	src_Actor_a8	[MAX_CMD_LEN];
	uint8_t 	dest_Actor_a8	[MAX_CMD_LEN];
	uint8_t 	cmdFun_a8		[MAX_CMD_LEN];
	uint8_t    *payload_p8;
}AMessage_st;

typedef struct {
	char*    Packet_Data_p8; 	// 32bit
	uint16_t data_Length_u16;
	uint16_t Frame_ID_u16;
	uint16_t Packet_ID_u16;
	uint16_t Crc16_u16;   		// payload CRC
	uint8_t  Log_Type_u8;
	uint8_t  CRLF_Flag_u8;
	uint8_t  Dummy1_u8;
	uint8_t  Dummy2_u8;
}FilePacket_st;

enum {
	U_INT8 = 1,  // uint8_t
	U_INT16,     // uint16_t
	U_INT32,     // uint32_t
	U_INT64,     // uint32_t
	INT32,     // int32_t
	INT,
	INT16,
	FLOAT,
	DOUBLE,
	STRING
};

typedef enum {
	E_ETH_ZERO_STATE = 0,
	E_ETH_CONNECTED,
	E_ETH_NOT_CONNECTED,
	E_ETH_LINK_UP,
	E_ETH_STARTED,
	E_ETH_STOPPED
}eth_et;

enum {
	E_wifi_ZERO_STATE = 0,
	E_wifi_NOT_CONNECTED,
	E_wifi_CONNECTED_LOW,
	E_wifi_CONNECTED_HIGH = 3,
	E_wifi_CONNECTED_AP,
//	E_SERVER_NOT_CONNECTED,
//	E_SERVER_CONNECTED
};

enum {
	SPIFFS_INITIALIZED =0,
	SPIFFS_FILE_EMPTY,
	SPIFFS_DEFAULT_VALUES,
	SPIFFS_CORRUPTED,
	SPIFFS_POPULATED,
	SPIFFS_PARTIAL,
	SPIFFS_PARTITION_NOT_FOUND
};

typedef enum
{
	E_ALI_RESP_ERR = 1,
	E_ALI_RESP_OK  = 2
}Audit_file_interface_Response_e;

typedef enum
{
	E_JFS_RESP_WAIT =0,
	E_JFS_RESP_ERR = 1,
	E_JFS_RESP_OK = 2,
	E_JFS_RESP_TERMINATE = 3
}JFS_Response_e;

typedef enum
{
	E_ALI_TYPE_COMMUNICATION_LOG 		= 1,
	E_ALI_TYPE_HARDWARE_DEBUG_LOG  		= 2,
	E_ALI_TYPE_EVENTS_AND_COMMANDS_LOG  = 3

}Audit_file_interface_Log_Type_e;

typedef enum
{
	LED_BLINK_STATE_OFF = 0,
	LED_BLINK_STATE1 = 1,
    LED_BLINK_STATE2 = 2,
    LED_BLINK_STATE3 = 3,
    LED_BLINK_STATE4 = 4,
    LED_BLINK_STATE5 = 5,
    LED_BLINK_STATE6 = 6,
    LED_BLINK_STATE7 = 7,
    LED_BLINK_STATE8 = 8,
    LED_BLINK_STATE9 = 9,
    LED_BLINK_STATE10 = 10,
    LED_BLINK_STATE_FULLON = 11,
	LED_BLINK_STATE_CHASE_MODE = 12,
	LED_STATE_BLE_MODE = 13,
	LED_STATE_UPLOAD_MODE = 14,
	LED_STATE_DOWNLOAD_MODE = 15,
	LED_STATE_HOLD_WARNING = 16,
	LED_STATE_THREE_FLASH = 17,
	LED_STATE_SOLID_MESSAGE = 18,
	LED_STATE_REBOOT_MODE = 19,
	LED_STATE_NET_CONNECTED = 20
} LedBlinkStates;

typedef struct
{
   uint16_t  milSec; // 0-999
   unsigned char second; // 0-59
   unsigned char minute; // 0-59
   unsigned char hour;   // 0-23
   unsigned char date;    // 1-31
   unsigned char month;  // 1-12
   unsigned char year;   // 0-99 (representing 2000-2099)
}
date_time_t;

#define EPOSCH_TO_30_YEAR    946684800

void Restart_ESP_Xface(char BLUE_LED);
esp_err_t esp_console_run_Custom(const char *cmdline, int *cmd_ret, const char *source_actor);
//extern void disable_all_watchdogs(void);
//extern void reconfig_watchdogs(uint32_t timeout_ms);

#endif
