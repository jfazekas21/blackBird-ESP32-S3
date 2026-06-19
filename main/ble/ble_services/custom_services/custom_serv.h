/*
 * custom_serv.h
 *
 *  Created on: 08-Dec-2023
 *      Author: Sai
 */

#ifndef MAIN_BLE_BLE_SERVICES_CUSTOM_SERVICES_CUSTOM_SERV_H_
#define MAIN_BLE_BLE_SERVICES_CUSTOM_SERVICES_CUSTOM_SERV_H_

#include "ble_manager.h"

// Custom defines
#define CUSTOM_TOTAL_CHARATERISTIC_NUM			(2)
#define	CUSTOM_SMA_CMD_MIN_LEN					(1)
#define	CUSTOM_SMA_CMD_MAX_LEN					(40)	// BLE service can now send multiple messages of size of 20 bytes
#define CUSTOM_SERIAL_NUM_MAX_LEN				(12)

// Custom structures
typedef struct custom_gatt_service_handler{
	at_ble_uuid_t serv_uuid;
	at_ble_handle_t	serv_handle;
	at_ble_characteristic_t	serv_chars[CUSTOM_TOTAL_CHARATERISTIC_NUM];
}custom_gatt_service_handler_t;

// Custom variables
extern uint8_t card_service_uuid[];
extern uint8_t card_write_cmd_uuid[];
extern uint8_t card_notify_cmd_uuid[];
extern uint8_t card_write_cmd_data[];
extern uint8_t card_notify_cmd_data[];

// Custom prototypes
void custom_init_service(custom_gatt_service_handler_t *);
at_ble_status_t custom_primary_service_define(custom_gatt_service_handler_t *);
at_ble_status_t custom_sma_update_char_value(custom_gatt_service_handler_t *, uint8_t *, uint8_t);	//, uint8_t


// Other custom characteristics we may support in the future
/*
#if SERIAL_NUM_ENABLE
static uint8_t card_ind_serNum_uuid[16] = {0x54, 0x3e, 0xf8, 0xd5, 0xaa, 0xd7, 0x30, 0xab, 0xdd, 0x47, 0x99, 0x86, 0x03, 0x00, 0x7a, 0x90};
static uint8_t card_scl_serNum_uuid[16] = {0x54, 0x3e, 0xf8, 0xd5, 0xaa, 0xd7, 0x30, 0xab, 0xdd, 0x47, 0x99, 0x86, 0x04, 0x00, 0x7a, 0x90};
#endif
#if SERIAL_NUM_ENABLE
uint8_t card_ind_serNum_data[CUSTOM_SERIAL_NUM_MAX_LEN];
uint8_t card_scl_serNum_data[CUSTOM_SERIAL_NUM_MAX_LEN];
#endif
*/

#endif /* MAIN_BLE_BLE_SERVICES_CUSTOM_SERVICES_CUSTOM_SERV_H_ */
