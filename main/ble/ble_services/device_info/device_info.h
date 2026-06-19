/*
 * device_info.h
 *
 *  Created on: 07-Dec-2023
 *      Author: Sai
 */

#ifndef MAIN_BLE_BLE_SERVICES_DEVICE_INFO_DEVICE_INFO_H_
#define MAIN_BLE_BLE_SERVICES_DEVICE_INFO_DEVICE_INFO_H_

/*
 * device_info.h
 *
 * Created: 2/14/2019 2:45:00 PM
 *  Author: brenyn_j
 */
#include "ble_mgr/ble_manager.h"

// Number of device information service characteristics
#define DIS_TOTAL_CHARATERISTIC_NUM             0x03	//0x09

// Manufacturer
#define DEFAULT_MANUFACTURER_NAME				("Cardinal/Detecto")
#define DIS_CHAR_MANUFACTURER_NAME_INIT_LEN		(0x10)
#define DIS_CHAR_MANUFACTURER_NAME_MAX_LEN		(0x14)

// Model Number
#define DEFAULT_MODEL_NUMBER					("Redbird")
#define DIS_CHAR_MODEL_NUMBER_INIT_LEN			(0x07)
#define DIS_CHAR_MODEL_NUMBER_MAX_LEN			(0x14)

// Software Revision
#define DEFAULT_SOFTWARE_REVISION				("1.0.0")
#define DIS_CHAR_SOFTWARE_REVISION_INIT_LEN		(0x05)
#define DIS_CHAR_SOFTWARE_REVISION_MAX_LEN		(0x14)

/*
// LATER_TODO - if these need to be used, need to convert then from MACROS to normal code. They were imported like this.
// Macro used for updating manufacturing string after defining the service using dis_primary_service_define
#define UPDATE_MANUFACTURER_STRING(ptr,info_data, conn_handle) do {  \
	if ( (dis_info_update(ptr, DIS_MANUFACTURER_NAME, info_data, conn_handle)) != AT_BLE_SUCCESS) { \
		debug_print("Updating Manufacturer string failed");  \
	}\
} while (0)

// Macro used for updating model number after defining the service using dis_primary_service_define
#define UPDATE_MODEL_NUMBER(ptr,info_data, conn_handle) do {   \
	if (dis_info_update(ptr, DIS_MODEL_NUMBER, info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating model number failed");  \
	}\
} while (0)

// Macro used for updating software revision after defining the service using dis_primary_service_define
#define UPDATE_SOFTWARE_REVISION(ptr, info_data, conn_handle) do{   \
	if (dis_info_update(ptr, DIS_SOFTWARE_REVISION, info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating software revision failed");  \
	}\
} while (0)
*/

typedef uint16_t ble_handle_t;

/**@brief Presentation Formats
*/
typedef enum{
   AT_BLE_PRES_FORMAT_BOOLEAN = 0X01,
   AT_BLE_PRES_FORMAT_2BIT = 0X02,
   AT_BLE_PRES_FORMAT_NIBBLE = 0X03,
   AT_BLE_PRES_FORMAT_UINT8 = 0X04,
   AT_BLE_PRES_FORMAT_UINT12 = 0X05,
   AT_BLE_PRES_FORMAT_UINT16 = 0X06,
   AT_BLE_PRES_FORMAT_UINT24 = 0X07,
   AT_BLE_PRES_FORMAT_UINT32 = 0X08,
   AT_BLE_PRES_FORMAT_UINT48 = 0X09,
   AT_BLE_PRES_FORMAT_UINT64 = 0X0A,
   AT_BLE_PRES_FORMAT_UINT128 = 0X0B,
   AT_BLE_PRES_FORMAT_SINT8 = 0X0C,
   AT_BLE_PRES_FORMAT_SINT12 = 0X0D,
   AT_BLE_PRES_FORMAT_SINT16 = 0X0E,
   AT_BLE_PRES_FORMAT_SINT24 = 0X0F,
   AT_BLE_PRES_FORMAT_SINT32 = 0X10,
   AT_BLE_PRES_FORMAT_SINT48 = 0X11,
   AT_BLE_PRES_FORMAT_SINT64 = 0X12,
   AT_BLE_PRES_FORMAT_SINT128 = 0X13,
   AT_BLE_PRES_FORMAT_FLOAT32 = 0X14,
   AT_BLE_PRES_FORMAT_FLOAT64 = 0X15,
   AT_BLE_PRES_FORMAT_SFLOAT = 0X16,
   AT_BLE_PRES_FORMAT_FLOAT = 0X17,
   AT_BLE_PRES_FORMAT_DUINT16 = 0X18,
   AT_BLE_PRES_FORMAT_UTF8S = 0X19,
   AT_BLE_PRES_FORMAT_UTF16S = 0X1A,
   AT_BLE_PRES_FORMAT_STRUCT = 0X1B,
}ble_char_pres_format_t;

// Device information enumerations
typedef enum {
	/* manufacturer name characteristic */
	DIS_MANUFACTURER_NAME= 0,
	/* model number characteristic */
	DIS_MODEL_NUMBER,
	/* serial number characteristic */
	DIS_SERIAL_NUMBER,
	/* Hardware revision characteristic */
	DIS_HARDWARE_REVISION,
	/* Firmware revision characteristic */
	DIS_FIRMWARE_REVISION,
	/* Software revision characteristic */
	DIS_SOFTWARE_REVISION,
	/* System id characteristic */
	DIS_SYSTEM_ID,
	/* PnP ID characteristic */
	DIS_PNP_ID,
	/* IEEE regulatory certification data list characteristic */
	DIS_IEEE_REG_CERT_DATA_LIST,
	/* must be the last element */
	DIS_END_VALUE,
} dis_info_type;

typedef struct dis_indicator_info{
	uint8_t manufacturer_name[21];		// 20 bytes + 0x00
	uint8_t model_number[21];			// 20 bytes + 0x00
	uint8_t software_revision[9];			// 9 bytes + 0x00
} dis_indicator_info_t;

extern dis_indicator_info_t indicator_info;

/**@brief Characteristic properties (Each enum value is a single bit; multiple properties can be indicated simultaneously.)
*/
typedef enum{
   AT_BLE_CHAR_BROADCST = (1 << 0),
   AT_BLE_CHAR_READ = (1 << 1),
   AT_BLE_CHAR_WRITE_WITHOUT_RESPONSE = (1 << 2),
   AT_BLE_CHAR_WRITE = (1 << 3),
   AT_BLE_CHAR_NOTIFY = (1 << 4),
   AT_BLE_CHAR_INDICATE = (1 << 5),
   AT_BLE_CHAR_SIGNED_WRITE = (1 << 6),
   AT_BLE_CHAR_RELIABLE_WRITE = (1 << 7),
   AT_BLE_CHAR_WRITEABLE_AUX = (1 << 8),
}ble_char_properties_t;

/**@brief Attribute Permissions
*/
typedef enum{
   AT_BLE_ATTR_NO_PERMISSIONS                  = 0x00,

   AT_BLE_ATTR_READABLE_NO_AUTHN_NO_AUTHR      = 0x01,
   AT_BLE_ATTR_READABLE_REQ_AUTHN_NO_AUTHR     = 0x02,
   AT_BLE_ATTR_READABLE_NO_AUTHN_REQ_AUTHR     = 0x03,
   AT_BLE_ATTR_READABLE_REQ_AUTHN_REQ_AUTHR    = 0x04,

   AT_BLE_ATTR_WRITABLE_NO_AUTHN_NO_AUTHR      = 0x10,
   AT_BLE_ATTR_WRITABLE_REQ_AUTHN_NO_AUTHR     = 0x20,
   AT_BLE_ATTR_WRITABLE_NO_AUTHN_REQ_AUTHR     = 0x30,
   AT_BLE_ATTR_WRITABLE_REQ_AUTHN_REQ_AUTHR    = 0x40,

}ble_attr_permissions_t;

/** @brief Characteristic presentation format
*/
typedef struct
{
    ble_char_pres_format_t format; /**< Value format */
    int8_t exponent; /**< Value Exponent */
    uint16_t unit; /**<  as defined in GATT spec Part G, Section 3.3.3.5.4 */
    uint8_t name_space; /**<  as defined in GATT spec Part G, Section 3.3.3.5.5 */
    uint16_t description; /**<  as defined in GATT spec Part G, Section 3.3.3.5.6 */

}ble_char_presentation_t;

typedef struct
{
    ble_handle_t char_val_handle; /**< Here the stack will store the char. value handle for future use */
    ble_uuid_t uuid; /**< Characteristic UUID */
    ble_char_properties_t properties; /**< Characteristic properties, values for Client Characteristic Configuration Descriptor and Server Characteristic Configuration Descriptor will be decided from this value*/

    uint8_t* init_value; /**< initial value of this characteristic  */
    uint16_t value_init_len; /**< initial value length */
    uint16_t value_max_len; /**< maximum possible length of the char. value */
    ble_attr_permissions_t value_permissions; /**< Value permissions */ //MICROCHIP_TODO: can this value be deduced from properties field ?

    uint8_t* user_desc; /**< a user friendly description, this value will be stored in the relevant descriptor, if no user description is desired set to NULL */
    uint16_t user_desc_len; /**< the user friendly description length, this value will be stored in the relevant descriptor, if no user description is desired set to 0*/
    uint16_t user_desc_max_len; /**< Maximum possible length for the user friendly description, this value will be stored in the relevant descriptor, if no user description is desired set to 0 */
    ble_attr_permissions_t user_desc_permissions;
    ble_attr_permissions_t client_config_permissions;
    ble_attr_permissions_t server_config_permissions;
    ble_handle_t user_desc_handle;
    ble_handle_t client_config_handle;
    ble_handle_t server_config_handle;

    ble_char_presentation_t* presentation_format; /**< Characteristic presentation format, this value will be stored in the relevant descriptor, if no presentation format is necessary set to NULL */

}ble_characteristic_t;

// Device information structures
typedef struct dis_gatt_service_handler
{
	ble_uuid_t	 serv_uuid;
	ble_handle_t serv_handle;
	ble_characteristic_t serv_chars[DIS_TOTAL_CHARATERISTIC_NUM];
}dis_gatt_service_handler_t;

// Characteristic value information
typedef struct {
	/// manufacturer name
	uint8_t manufacturer_name[DIS_CHAR_MANUFACTURER_NAME_MAX_LEN];
	/// model number
	uint8_t default_model_number[DIS_CHAR_MODEL_NUMBER_MAX_LEN];
	// serial number
	//uint8_t default_serial_number[DIS_CHAR_SERIAL_NUMBER_MAX_LEN];
	// hardware revision
	//uint8_t default_hardware_revision[DIS_CHAR_HARDWARE_REVISION_MAX_LEN];
	// default firmware revision
	//uint8_t default_firmware_revision[DIS_CHAR_FIRMWARE_REVISION_MAX_LEN];
	// software revision
	uint8_t default_software_revision[DIS_CHAR_SOFTWARE_REVISION_MAX_LEN];
	// system id
	//system_id_char_value_t default_system_id;

	// LATER_TODO - Possibly add this in to hold Redbird information since other data should be tied to indicator values
	// PnP ID
	//pnp_id_char_value_t default_pnp_id;
	// ieee regulatory certification data list
	//uint8_t ieee_reg_cert_data_list[DIS_CHAR_IEEE_REG_CERT_DATA_LIST_MAX_LEN];
}device_info_char_value_t;

// Configurable Client characteristic data for a given dis info type
typedef struct{
	// length of the data to be updated
	uint16_t data_len;
	// data to be updated
	uint8_t *info_data;
}dis_info_data;

// Device information functions
//at_ble_status_t dis_info_update(dis_gatt_service_handler_t *dis_serv , dis_info_type info_type, dis_info_data* info_data, ble_handle_t conn_handle);
//void dis_init_service(dis_gatt_service_handler_t *device_info_serv );
//at_ble_status_t dis_primary_service_define(dis_gatt_service_handler_t *dis_primary_service);


// --------------- Code for Additional Device Information Services ---------------
/*
// @brief Macro used for updating serial number after defining the service using dis_primary_service_define
#define UPDATE_SERIAL_NUMBER(ptr,info_data, conn_handle) do{   \
	if (dis_info_update(ptr,DIS_SERIAL_NUMBER,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating serial number failed");  \
	}\
} while (0)

// @brief Macro used for updating hardware revision after defining the service using dis_primary_service_define
#define UPDATE_HARDWARE_REVISION(ptr,info_data, conn_handle) do{   \
	if (dis_info_update(ptr,DIS_HARDWARE_REVISION,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating hardware revision failed");  \
	}\
} while (0)

// @brief Macro used for updating firmware revision after defining the service using dis_primary_service_define
#define UPDATE_FIRMWARE_REVISION(ptr,info_data, conn_handle) do{   \
	if (dis_info_update(ptr,DIS_FIRMWARE_REVISION,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating firmware revision failed");  \
	}\
} while (0)

// @brief Macro used for updating system ID after defining the service using dis_primary_service_define
#define UPDATE_SYSTEM_ID(ptr,info_data, conn_handle) do {   \
	if (dis_info_update(ptr,DIS_SYSTEM_ID,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating system id failed");  \
	}\
} while (0)

// @brief Macro used for updating PnP ID after defining the service using dis_primary_service_define
#define UPDATE_PNP_ID(ptr,info_data, conn_handle) do {   \
	if (dis_info_update(ptr,DIS_PNP_ID,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating PnP ID failed");  \
	}\
} while (0)

// @brief Macro used for updating IEEE regulatory certification data list after defining the service using dis_primary_service_define
#define UPDATE_IEEE_REG_CERT_DATA_LIST(ptr,info_data, conn_handle) do {   \
	if (dis_info_update(ptr,DIS_IEEE_REG_CERT_DATA_LIST,info_data, conn_handle) != AT_BLE_SUCCESS) { \
		debug_print("Updating IEEE regulatory certification data list failed");  \
	}\
} while (0)

// Serial Number
#define DEFAULT_SERIAL_NUMBER					("BTLC1000/SAMB11")
#define DIS_CHAR_SERIAL_NUMBER_INIT_LEN			(0x0f)
#define DIS_CHAR_SERIAL_NUMBER_MAX_LEN			(0x14)

// Hardware Revision
#define DEFAULT_HARDWARE_REVISION				("Rev A")
#define DIS_CHAR_HARDWARE_REVISION_INIT_LEN		(0x05)
#define DIS_CHAR_HARDWARE_REVISION_MAX_LEN		(0x14)

// Firmware Revision
#define DEFAULT_FIRMWARE_REVISION				("FW_BETA")
#define DIS_CHAR_FIRMWARE_REVISION_INIT_LEN		0x07
#define DIS_CHAR_FIRMWARE_REVISION_MAX_LEN		0x14

// System id characteristic
#define DIS_CHAR_SYSTEM_ID_INIT_LEN sizeof(system_id_char_value_t)
#define DIS_CHAR_SYSTEM_ID_MAX_LEN	sizeof(system_id_char_value_t)

//PnP id characteristic
#define DIS_CHAR_PNP_ID_INIT_LEN				0x07
#define DIS_CHAR_PNP_ID_MAX_LEN					0x07

// IEEE regulatory certification data list
#define DIS_CHAR_IEEE_REG_CERT_DATA_LIST_INIT_LEN	0x01
#define DIS_CHAR_IEEE_REG_CERT_DATA_LIST_MAX_LEN	0x0a

// @brief PnP ID characteristic value configure by user
#define PNP_ID_VENDOR_ID_SOURCE		0x01
#define PNP_ID_VENDOR_ID			0x2222
#define PNP_ID_PRODUCT_ID			0x3333
#define PNP_ID_PRODUCT_VERSION		0x0001

// @brief system ID characteristic default values
#define SYSTEM_ID_MANUFACTURER_ID_LEN	0x07
#define SYSTEM_ID_ORG_UNIQUE_ID_LEN		0x03
#define SYSTEM_ID_MANUFACTURER_ID		"\x00\x00\x00\x00\x00"
#define SYSTEM_ID_ORG_UNIQUE_ID			"\x00\x04\x25"

// System ID characteristic value information
typedef struct{
	// manufacturer identifier
	uint8_t manufacturer_id[5];
	// organizational unique identifier
	uint8_t org_unique_id[3];
}system_id_char_value_t;

COMPILER_PACK_SET(1)

// @brief pnp characteristic value information
typedef struct {
	// vendor id source
	uint8_t vendor_id_source;
	// vendor id
	uint16_t vendor_id;
	// product id
	uint16_t product_id;
	// product version
	uint16_t product_version;
}pnp_id_char_value_t;

COMPILER_PACK_RESET()
*/


#endif /* MAIN_BLE_BLE_SERVICES_DEVICE_INFO_DEVICE_INFO_H_ */
