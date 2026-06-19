/*
 * device_info.c
 *
 *  Created on: 07-Dec-2023
 *      Author: Sai
 */

#include "device_info.h"

// Characteristics of the device information service
device_info_char_value_t char_value;
dis_indicator_info_t indicator_info = {0};

bool volatile dis_notification_flag[DIS_TOTAL_CHARATERISTIC_NUM] = {false};

#if PRINT_INFO || PRINT_DBG || TEST
extern void debug_print(const char *msg);
#endif

// Initialize the dis service related information
void dis_init_service(dis_gatt_service_handler_t *device_info_serv)
{
	uint16_t length = 0;
	uint8_t default_value_flag = 1;

	device_info_serv->serv_handle = 0;
	device_info_serv->serv_uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_uuid.uuid[0] = (uint8_t) DIS_SERVICE_UUID;
	device_info_serv->serv_uuid.uuid[1] = (uint8_t) (DIS_SERVICE_UUID >> 8);

	// Characteristic Info for Manufacturer Name String
	device_info_serv->serv_chars[0].char_val_handle = 0;
	device_info_serv->serv_chars[0].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[0].uuid.uuid[0] = (uint8_t) DIS_CHAR_MANUFACTURER_NAME_UUID;
	device_info_serv->serv_chars[0].uuid.uuid[1] = (uint8_t) (DIS_CHAR_MANUFACTURER_NAME_UUID >> 8);
	device_info_serv->serv_chars[0].properties = AT_BLE_CHAR_READ;

	// If manufacturer name has been set it else use default value
	if (indicator_info.manufacturer_name[0] != 0x00) {
		default_value_flag = 0;
		length = strlen((char *)indicator_info.manufacturer_name);
		memcpy(char_value.manufacturer_name, indicator_info.manufacturer_name, length);
	} else {
		memcpy(char_value.manufacturer_name, DEFAULT_MANUFACTURER_NAME, DIS_CHAR_MANUFACTURER_NAME_INIT_LEN);
	}

	device_info_serv->serv_chars[0].init_value = char_value.manufacturer_name;
	if (!default_value_flag) {
		device_info_serv->serv_chars[0].value_init_len = length;
	} else {
		device_info_serv->serv_chars[0].value_init_len = DIS_CHAR_MANUFACTURER_NAME_INIT_LEN;
	}

	// Reset variables
	length = 0;
	default_value_flag = 1;

	device_info_serv->serv_chars[0].value_max_len = DIS_CHAR_MANUFACTURER_NAME_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[0].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
	device_info_serv->serv_chars[0].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
	device_info_serv->serv_chars[0].user_desc = NULL;
	device_info_serv->serv_chars[0].user_desc_len = 0;
	device_info_serv->serv_chars[0].user_desc_max_len = 0;
	device_info_serv->serv_chars[0].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[0].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[0].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[0].user_desc_handle = 0;
	device_info_serv->serv_chars[0].client_config_handle = 0;
	device_info_serv->serv_chars[0].server_config_handle = 0;
	device_info_serv->serv_chars[0].presentation_format = NULL;

	// Characteristic Info for Model Number String
	device_info_serv->serv_chars[1].char_val_handle = 0;
	device_info_serv->serv_chars[1].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[1].uuid.uuid[0] = (uint8_t) DIS_CHAR_MODEL_NUMBER_UUID;
	device_info_serv->serv_chars[1].uuid.uuid[1] = (uint8_t) (DIS_CHAR_MODEL_NUMBER_UUID >> 8);
	device_info_serv->serv_chars[1].properties = AT_BLE_CHAR_READ;

	// If model number has been set use it else use default value
	if (indicator_info.model_number[0] != 0x00) {
		default_value_flag = 0;
		length = strlen((char *)indicator_info.model_number);
		memcpy(char_value.default_model_number, indicator_info.model_number, length);
	} else {
		memcpy(char_value.default_model_number, DEFAULT_MODEL_NUMBER, DIS_CHAR_MODEL_NUMBER_INIT_LEN);
	}

	device_info_serv->serv_chars[1].init_value = char_value.default_model_number;

	if (!default_value_flag) {
		device_info_serv->serv_chars[1].value_init_len = length;
	} else {
		device_info_serv->serv_chars[1].value_init_len = DIS_CHAR_MODEL_NUMBER_INIT_LEN;
	}

	// Reset variables
	length = 0;
	default_value_flag = 1;

	device_info_serv->serv_chars[1].value_max_len = DIS_CHAR_MODEL_NUMBER_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[1].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
	device_info_serv->serv_chars[1].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
	device_info_serv->serv_chars[1].user_desc = NULL;
	device_info_serv->serv_chars[1].user_desc_len = 0;
	device_info_serv->serv_chars[1].user_desc_max_len = 0;
	device_info_serv->serv_chars[1].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[1].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[1].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[1].user_desc_handle = 0;
	device_info_serv->serv_chars[1].client_config_handle = 0;
	device_info_serv->serv_chars[1].server_config_handle = 0;
	device_info_serv->serv_chars[1].presentation_format = NULL;

	// Characteristic Info for Software Revision
	device_info_serv->serv_chars[2].char_val_handle = 0;
	device_info_serv->serv_chars[2].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[2].uuid.uuid[0] = (uint8_t) DIS_CHAR_SOFTWARE_REVISION_UUID;
	device_info_serv->serv_chars[2].uuid.uuid[1] = (uint8_t) (DIS_CHAR_SOFTWARE_REVISION_UUID >> 8);
	device_info_serv->serv_chars[2].properties = AT_BLE_CHAR_READ;

	// If software revision has been set use it else use default value
	if (indicator_info.software_revision[0] != 0x00) {
		default_value_flag = 0;
		length = strlen((char *)indicator_info.software_revision);
		memcpy(char_value.default_software_revision, indicator_info.software_revision, length);
	} else {
		memcpy(char_value.default_software_revision, DEFAULT_SOFTWARE_REVISION, DIS_CHAR_SOFTWARE_REVISION_INIT_LEN);
	}

	device_info_serv->serv_chars[2].init_value = char_value.default_software_revision;

	if (!default_value_flag) {
		device_info_serv->serv_chars[2].value_init_len = length;
	} else {
		device_info_serv->serv_chars[2].value_init_len = DIS_CHAR_SOFTWARE_REVISION_INIT_LEN;
	}

	// Reset variables
	length = 0;
	default_value_flag = 1;

	device_info_serv->serv_chars[2].value_max_len = DIS_CHAR_SOFTWARE_REVISION_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;
#else
	device_info_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;
#endif
	device_info_serv->serv_chars[2].user_desc = NULL;
	device_info_serv->serv_chars[2].user_desc_len = 0;
	device_info_serv->serv_chars[2].user_desc_max_len = 0;
	device_info_serv->serv_chars[2].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[2].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[2].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;
	device_info_serv->serv_chars[2].user_desc_handle = 0;
	device_info_serv->serv_chars[2].client_config_handle = 0;
	device_info_serv->serv_chars[2].server_config_handle = 0;
	device_info_serv->serv_chars[2].presentation_format = NULL;
}

// Define device information service
at_ble_status_t dis_primary_service_define(dis_gatt_service_handler_t *dis_primary_service)
{
	return(at_ble_primary_service_define(&dis_primary_service->serv_uuid, &dis_primary_service->serv_handle,
			NULL, 0, dis_primary_service->serv_chars, DIS_TOTAL_CHARATERISTIC_NUM));
}

// Update the DIS characteristic value
at_ble_status_t dis_info_update(dis_gatt_service_handler_t *dis_serv , dis_info_type info_type, dis_info_data* info_data, at_ble_handle_t conn_handle)
{
	if (info_data->data_len > dis_serv->serv_chars[info_type].value_max_len)
	{
#if PRINT_DBG
		debug_print("invalid length parameter");
#endif
		return AT_BLE_FAILURE;
	}

	// Updating device information att data
	memcpy(&(dis_serv->serv_chars[info_type].init_value), info_data->info_data, info_data->data_len);

	// Updating the specific device information characteristic value
	if ((at_ble_characteristic_value_set(dis_serv->serv_chars[info_type].char_val_handle,
		info_data->info_data, info_data->data_len)) != AT_BLE_SUCCESS){
#if PRINT_DBG
	debug_print("Device info characteristic value update has failed");
#endif
	} else {
		return AT_BLE_SUCCESS;
	}
	//ALL_UNUSED(conn_handle);
	return AT_BLE_FAILURE;
}

// --------------- Code for Additional Device Information Services ---------------
/*
	// Characteristic Info for Serial String
	device_info_serv->serv_chars[2].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[2].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[2].uuid.uuid[0] = (uint8_t) DIS_CHAR_SERIAL_NUMBER_UUID;          // UUID : Hardware Revision String
	device_info_serv->serv_chars[2].uuid.uuid[1] = (uint8_t) (DIS_CHAR_SERIAL_NUMBER_UUID >> 8);          // UUID : Hardware Revision String
	device_info_serv->serv_chars[2].properties = AT_BLE_CHAR_READ; // Properties

	memcpy(char_value.default_serial_number,DEFAULT_SERIAL_NUMBER,DIS_CHAR_SERIAL_NUMBER_INIT_LEN);
	device_info_serv->serv_chars[2].init_value = char_value.default_serial_number;

	device_info_serv->serv_chars[2].value_init_len = DIS_CHAR_SERIAL_NUMBER_INIT_LEN;
	device_info_serv->serv_chars[2].value_max_len = DIS_CHAR_SERIAL_NUMBER_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[2].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[2].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[2].user_desc_len = 0;
	device_info_serv->serv_chars[2].user_desc_max_len = 0;
	device_info_serv->serv_chars[2].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[2].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[2].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[2].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[2].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[2].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[2].presentation_format = NULL;       // presentation format


	// Characteristic Info for Hardware Revision String
	device_info_serv->serv_chars[3].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[3].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[3].uuid.uuid[0] = (uint8_t) DIS_CHAR_HARDWARE_REVISION_UUID;          // UUID : Firmware Revision String
	device_info_serv->serv_chars[3].uuid.uuid[1] = (uint8_t) (DIS_CHAR_HARDWARE_REVISION_UUID >> 8);          // UUID : Firmware Revision String
	device_info_serv->serv_chars[3].properties = AT_BLE_CHAR_READ; // Properties

	memcpy(char_value.default_hardware_revision,DEFAULT_HARDWARE_REVISION,DIS_CHAR_HARDWARE_REVISION_INIT_LEN);
	device_info_serv->serv_chars[3].init_value = char_value.default_hardware_revision;

	device_info_serv->serv_chars[3].value_init_len = DIS_CHAR_HARDWARE_REVISION_INIT_LEN;
	device_info_serv->serv_chars[3].value_max_len = DIS_CHAR_HARDWARE_REVISION_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[3].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[3].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[3].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[3].user_desc_len = 0;
	device_info_serv->serv_chars[3].user_desc_max_len = 0;
	device_info_serv->serv_chars[3].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[3].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[3].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[3].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[3].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[3].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[3].presentation_format = NULL;       // presentation format


	// Characteristic Info for Firmware Revision
	device_info_serv->serv_chars[4].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[4].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[4].uuid.uuid[0] = (uint8_t) DIS_CHAR_FIRMWARE_REIVSION_UUID;          // UUID : Software Revision
	device_info_serv->serv_chars[4].uuid.uuid[1] = (uint8_t) (DIS_CHAR_FIRMWARE_REIVSION_UUID >> 8);          // UUID : Software Revision
	device_info_serv->serv_chars[4].properties = AT_BLE_CHAR_READ; // Properties

	memcpy(char_value.default_firmware_revision,DEFAULT_FIRMWARE_REIVSION,DIS_CHAR_FIRMWARE_REIVSION_INIT_LEN);
	device_info_serv->serv_chars[4].init_value = char_value.default_firmware_revision;

	device_info_serv->serv_chars[4].value_init_len = DIS_CHAR_FIRMWARE_REIVSION_INIT_LEN;
	device_info_serv->serv_chars[4].value_max_len = DIS_CHAR_FIRMWARE_REIVSION_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[4].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[4].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[4].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[4].user_desc_len = 0;
	device_info_serv->serv_chars[4].user_desc_max_len = 0;
	device_info_serv->serv_chars[4].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[4].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[4].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[4].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[4].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[4].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[4].presentation_format = NULL;       // presentation format

	// Characteristic Info for SystemID  Number
	device_info_serv->serv_chars[6].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[6].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[6].uuid.uuid[0] = (uint8_t) DIS_CHAR_SYSTEM_ID_UUID;          // UUID : Software Revision
	device_info_serv->serv_chars[6].uuid.uuid[1] = (uint8_t) (DIS_CHAR_SYSTEM_ID_UUID >> 8);          // UUID : Software Revision
	device_info_serv->serv_chars[6].properties = AT_BLE_CHAR_READ; // Properties

	memcpy(char_value.default_system_id.manufacturer_id, SYSTEM_ID_MANUFACTURER_ID, SYSTEM_ID_MANUFACTURER_ID_LEN);
	memcpy(char_value.default_system_id.org_unique_id, SYSTEM_ID_ORG_UNIQUE_ID, SYSTEM_ID_ORG_UNIQUE_ID_LEN);
	device_info_serv->serv_chars[6].init_value = (uint8_t *) &char_value.default_system_id;					//Initial Value

	device_info_serv->serv_chars[6].value_init_len = DIS_CHAR_SYSTEM_ID_INIT_LEN;
	device_info_serv->serv_chars[6].value_max_len = DIS_CHAR_SYSTEM_ID_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[6].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[6].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[6].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[6].user_desc_len = 0;
	device_info_serv->serv_chars[6].user_desc_max_len = 0;
	device_info_serv->serv_chars[6].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[6].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[6].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[6].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[6].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[6].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[6].presentation_format = NULL;       // presentation format

	// Characteristic Info for PnP ID
	device_info_serv->serv_chars[7].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[7].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[7].uuid.uuid[0] = (uint8_t) DIS_CHAR_PNP_ID_UUID;          // UUID : Software Revision
	device_info_serv->serv_chars[7].uuid.uuid[1] = (uint8_t) (DIS_CHAR_PNP_ID_UUID >> 8);          // UUID : Software Revision
	device_info_serv->serv_chars[7].properties = AT_BLE_CHAR_READ; // Properties

	char_value.default_pnp_id.vendor_id_source = PNP_ID_VENDOR_ID_SOURCE;					//characteristic value initialization
	char_value.default_pnp_id.vendor_id = PNP_ID_VENDOR_ID;
	char_value.default_pnp_id.product_id= PNP_ID_PRODUCT_ID;
	char_value.default_pnp_id.product_version= PNP_ID_PRODUCT_VERSION;
	device_info_serv->serv_chars[7].init_value = (uint8_t *) &char_value.default_pnp_id;					//Initial Value

	device_info_serv->serv_chars[7].value_init_len = DIS_CHAR_PNP_ID_INIT_LEN;
	device_info_serv->serv_chars[7].value_max_len = DIS_CHAR_PNP_ID_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[7].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[7].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[7].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[7].user_desc_len = 0;
	device_info_serv->serv_chars[7].user_desc_max_len = 0;
	device_info_serv->serv_chars[7].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[7].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[7].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[7].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[7].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[7].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[7].presentation_format = NULL;       // presentation format

	// Characteristic Info for IEEE 11073-20601 Regulatory Certification Data List
	device_info_serv->serv_chars[8].char_val_handle = 0;          // handle stored here
	device_info_serv->serv_chars[8].uuid.type = AT_BLE_UUID_16;
	device_info_serv->serv_chars[8].uuid.uuid[0] = (uint8_t) DIS_CHAR_IEEE_REG_CERT_DATA_LIST_UUID;          // UUID : Software Revision
	device_info_serv->serv_chars[8].uuid.uuid[1] = (uint8_t) (DIS_CHAR_IEEE_REG_CERT_DATA_LIST_UUID >> 8);          // UUID : Software Revision
	device_info_serv->serv_chars[8].properties = AT_BLE_CHAR_READ; // Properties
	device_info_serv->serv_chars[8].init_value = char_value.ieee_reg_cert_data_list;					//Initial Value
	device_info_serv->serv_chars[8].value_init_len = DIS_CHAR_IEEE_REG_CERT_DATA_LIST_INIT_LEN;
	device_info_serv->serv_chars[8].value_max_len = DIS_CHAR_IEEE_REG_CERT_DATA_LIST_MAX_LEN;
#if BLE_PAIR_ENABLE
	device_info_serv->serv_chars[8].value_permissions = AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR;   // permissions
#else
	device_info_serv->serv_chars[8].value_permissions = AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR;   // permissions
#endif
	device_info_serv->serv_chars[8].user_desc = NULL;           // user defined name
	device_info_serv->serv_chars[8].user_desc_len = 0;
	device_info_serv->serv_chars[8].user_desc_max_len = 0;
	device_info_serv->serv_chars[8].user_desc_permissions = AT_BLE_ATTR_NO_PERMISSIONS;             //user description permissions
	device_info_serv->serv_chars[8].client_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //client config permissions
	device_info_serv->serv_chars[8].server_config_permissions = AT_BLE_ATTR_NO_PERMISSIONS;         //server config permissions
	device_info_serv->serv_chars[8].user_desc_handle = 0;             //user desc handles
	device_info_serv->serv_chars[8].client_config_handle = 0;         //client config handles
	device_info_serv->serv_chars[8].server_config_handle = 0;         //server config handles
	device_info_serv->serv_chars[8].presentation_format = NULL;       // presentation format
*/

