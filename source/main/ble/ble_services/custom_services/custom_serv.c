/*
 * custom_serv.c
 *
 *  Created on: 08-Dec-2023
 *      Author: Sai
 */

#include "custom_serv.h"

// Custom 128-bit service UUIDs
uint8_t card_service_uuid[16] = {0x54, 0x3e, 0xf8, 0xd5, 0xaa, 0xd7, 0x30, 0xab, 0xdd, 0x47, 0x99, 0x86, 0x00, 0x00, 0x7a, 0x90};
uint8_t card_write_cmd_uuid[16] = {0x54, 0x3e, 0xf8, 0xd5, 0xaa, 0xd7, 0x30, 0xab, 0xdd, 0x47, 0x99, 0x86, 0x01, 0x00, 0x7a, 0x90};
uint8_t card_notify_cmd_uuid[16] = {0x54, 0x3e, 0xf8, 0xd5, 0xaa, 0xd7, 0x30, 0xab, 0xdd, 0x47, 0x99, 0x86, 0x02, 0x00, 0x7a, 0x90};

// Custom variables
uint8_t card_write_cmd_data[CUSTOM_SMA_CMD_MAX_LEN] = {0};
uint8_t card_notify_cmd_data[CUSTOM_SMA_CMD_MAX_LEN] = {0};

extern ble_connected_t ble_connected_dev_info[MAX_DEVICE_CONNECTED];

#if PRINT_INFO || PRINT_DBG || TEST
extern char debug_msg[];
extern void debug_print(const char *msg);
#endif

// Custom service functions
void custom_init_service(custom_gatt_service_handler_t *custom_serv){
	// Custom Cardinal Service UUID
	custom_serv->serv_handle = 0;
	custom_serv->serv_uuid.type = AT_BLE_UUID_128;
	memcpy(custom_serv->serv_uuid.uuid, card_service_uuid, AT_BLE_UUID_128_LEN);

	// Characteristic Info for Custom Get Command
	custom_serv->serv_chars[0].char_val_handle = 0;
	custom_serv->serv_chars[0].uuid.type = AT_BLE_UUID_128;
	memcpy(custom_serv->serv_chars[0].uuid.uuid, card_write_cmd_uuid, AT_BLE_UUID_128_LEN);
	custom_serv->serv_chars[0].properties = AT_BLE_CHAR_WRITE;

	custom_serv->serv_chars[0].init_value = card_write_cmd_data;
	custom_serv->serv_chars[0].value_init_len = CUSTOM_SMA_CMD_MAX_LEN;
	custom_serv->serv_chars[0].value_max_len = CUSTOM_SMA_CMD_MAX_LEN;
#if BLE_PAIR_ENABLE
	custom_serv->serv_chars[0].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
	custom_serv->serv_chars[0].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
	custom_serv->serv_chars[0].user_desc = NULL;
	custom_serv->serv_chars[0].user_desc_len = 0;
	custom_serv->serv_chars[0].user_desc_max_len = 0;
	custom_serv->serv_chars[0].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[0].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[0].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[0].user_desc_handle = 0;
	custom_serv->serv_chars[0].client_config_handle = 0;
	custom_serv->serv_chars[0].server_config_handle = 0;
	custom_serv->serv_chars[0].presentation_format = NULL;

	// Characteristic Info for Custom Send Command
	custom_serv->serv_chars[1].char_val_handle = 0;
	custom_serv->serv_chars[1].uuid.type = AT_BLE_UUID_128;
	memcpy(custom_serv->serv_chars[1].uuid.uuid, card_notify_cmd_uuid, AT_BLE_UUID_128_LEN);
	custom_serv->serv_chars[1].properties = AT_BLE_CHAR_NOTIFY; // Properties (| AT_BLE_CHAR_READ used for testing)

	custom_serv->serv_chars[1].init_value = card_notify_cmd_data;
	custom_serv->serv_chars[1].value_init_len = CUSTOM_SMA_CMD_MAX_LEN;
	custom_serv->serv_chars[1].value_max_len = CUSTOM_SMA_CMD_MAX_LEN;
#if BLE_PAIR_ENABLE
	custom_serv->serv_chars[1].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
	custom_serv->serv_chars[1].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
	custom_serv->serv_chars[1].user_desc = NULL;
	custom_serv->serv_chars[1].user_desc_len = 0;
	custom_serv->serv_chars[1].user_desc_max_len = 0;
	custom_serv->serv_chars[1].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[1].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[1].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	custom_serv->serv_chars[1].user_desc_handle = 0;
	custom_serv->serv_chars[1].client_config_handle = 0;
	custom_serv->serv_chars[1].server_config_handle = 0;
	custom_serv->serv_chars[1].presentation_format = NULL;
}

at_ble_status_t custom_primary_service_define(custom_gatt_service_handler_t *custom_primary_serv){
	return(at_ble_primary_service_define(&custom_primary_serv->serv_uuid, &custom_primary_serv->serv_handle,
			NULL, 0, custom_primary_serv->serv_chars, CUSTOM_TOTAL_CHARATERISTIC_NUM));
}

at_ble_status_t custom_sma_update_char_value(custom_gatt_service_handler_t *custom_sma_serv, uint8_t *char_data, uint8_t data_length)	//, uint8_t data_length
{
	// Updating the custom sma value in the att data base
	//if (at_ble_characteristic_value_set(custom_sma_serv->serv_chars[1].char_val_handle, &char_data[0], sizeof(uint8_t)*strlen((char *)&char_data[0]))
	if (at_ble_characteristic_value_set(custom_sma_serv->serv_chars[1].char_val_handle, &char_data[0], sizeof(uint8_t)*data_length)
			!= AT_BLE_SUCCESS){	//sizeof(uint8_t)*CUSTOM_SMA_CMD_MAX_LEN)
#if PRINT_INFO
	debug_print("Custom characteristic value update has failed");
#endif
		return AT_BLE_FAILURE;
	}

	// Sending notification to the peer about change in the sma command
	if(at_ble_notification_send(ble_connected_dev_info[0].handle, custom_sma_serv->serv_chars[1].char_val_handle) != AT_BLE_SUCCESS) {
#if PRINT_INFO
	debug_print("Custom SMA notification to the peer failed");
#endif
		return AT_BLE_FAILURE;
	} else {
		return AT_BLE_SUCCESS;
	}
}

// --------------- Code for Additional Custom Services ---------------
/*
#if SERIAL_NUM_ENABLE
/ /Characteristic Info for Custom Serial Number Command
custom_serv->serv_chars[2].char_val_handle = 0;
custom_serv->serv_chars[2].uuid.type = AT_BLE_UUID_128;
memcpy(custom_serv->serv_chars[2].uuid.uuid, card_ind_serNum_uuid, AT_BLE_UUID_128_LEN);
custom_serv->serv_chars[2].properties = AT_BLE_CHAR_READ;

custom_serv->serv_chars[2].init_value = card_ind_serNum_data;
custom_serv->serv_chars[2].value_init_len = CUSTOM_SERIAL_NUM_MAX_LEN;
custom_serv->serv_chars[2].value_max_len = CUSTOM_SERIAL_NUM_MAX_LEN;
#if BLE_PAIR_ENABLE
custom_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
custom_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
custom_serv->serv_chars[2].user_desc = NULL;
custom_serv->serv_chars[2].user_desc_len = 0;
custom_serv->serv_chars[2].user_desc_max_len = 0;
custom_serv->serv_chars[2].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[2].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[2].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[2].user_desc_handle = 0;
custom_serv->serv_chars[2].client_config_handle = 0;
custom_serv->serv_chars[2].server_config_handle = 0;
custom_serv->serv_chars[2].presentation_format = NULL;

//Characteristic Info for Custom Serial SCL Command
custom_serv->serv_chars[3].char_val_handle = 0;
custom_serv->serv_chars[3].uuid.type = AT_BLE_UUID_128;
memcpy(custom_serv->serv_chars[3].uuid.uuid, card_scl_serNum_uuid, AT_BLE_UUID_128_LEN);
custom_serv->serv_chars[3].properties = AT_BLE_CHAR_READ;

custom_serv->serv_chars[3].init_value = card_scl_serNum_data;
custom_serv->serv_chars[3].value_init_len = CUSTOM_SMA_CMD_MAX_LEN;
custom_serv->serv_chars[3].value_max_len = CUSTOM_SMA_CMD_MAX_LEN;
#if BLE_PAIR_ENABLE
custom_serv->serv_chars[3].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
custom_serv->serv_chars[3].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
custom_serv->serv_chars[3].user_desc = NULL;
custom_serv->serv_chars[3].user_desc_len = 0;
custom_serv->serv_chars[3].user_desc_max_len = 0;
custom_serv->serv_chars[3].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[3].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[3].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
custom_serv->serv_chars[3].user_desc_handle = 0;
custom_serv->serv_chars[3].client_config_handle = 0;
custom_serv->serv_chars[3].server_config_handle = 0;
custom_serv->serv_chars[3].presentation_format = NULL;
#endif
*/
