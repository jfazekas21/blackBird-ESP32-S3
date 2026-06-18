/*
 * BLE_Actor.h
 *
 *  Created on: 13-Oct-2023
 *  Author: Priyanka
 */

#ifndef MAIN_BLE_ACTOR_H_
#define MAIN_BLE_ACTOR_H_


/* The max length of characteristic value. When the gatt client write or prepare write,
*  the data length must be less than GATTS_EXAMPLE_CHAR_VAL_LEN_MAX.
*/

#define BLE_BTNAME_LEN  		32 			// 14 byte BLE Device name + 1 byte null character
#define BLE_UUID_LEN 			40
#define BLE_INFO_LEN            32

#define GATTS_EXAMPLE_CHAR_VAL_LEN_MAX 500
#define LONG_CHAR_VAL_LEN           500
#define SHORT_CHAR_VAL_LEN          10
#define GATTS_NOTIFY_FIRST_PACKET_LEN_MAX 20

#define PREPARE_BUF_MAX_SIZE        1024
#define CHAR_DECLARATION_SIZE       (sizeof(uint8_t))

#define ADV_CONFIG_FLAG             (1 << 0)
#define SCAN_RSP_CONFIG_FLAG        (1 << 1)
#define DEFAULT_ADV_NAME_DATA			"REDBIRD BLE"

#define ADV_DATA_NAME_TYPE				(0x09)
#define ADV_DATA_SET_MAX_LEN			(31)

#define MIN_CONN_INTERVAL				(0x0028)	// 50ms.
#define MAX_CONN_INTERVAL				(0x0190)	// 500ms.
#define SLAVE_LATENCY					(0x0000)	// No slave latency.
#define CONN_SUPERVISION_TIMEOUT		(0x03E8)	// 10s.

#define APP_FAST_ADV					(0x0030)	// 100 ms
#define APP_ADV_TIMEOUT					(0x0030)	// 100 Secs Advertising time-out between 0x0001 and 0x3FFF in seconds, 0x0000 disables time-out.

// Learn about appearance: http://developer.bluetooth.org/gatt/characteristics/Pages/CharacteristicViewer.aspx?u=org.bluetooth.characteristic.gap.appearance.xml
#define BLE_PERIPHERAL_APPEARANCE		(3200)		// Appearance key = 3200 "Generic: Weight Scale"

#define WEIGHT_SCALE_FEATURE_MAX_LEN    (4)
#define WEIGHT_MEASUREMENT_MAX_LEN      (7)
#define CELL_DATA_LEN					(2)
#define SERIAL_NUM_LEN					(12)

/* Attributes State Machine */
enum
{
    IDX_SVC,
    IDX_CHAR_A,
    IDX_CHAR_VAL_A,
    IDX_CHAR_CFG_A,

    IDX_CHAR_B,
    IDX_CHAR_VAL_B,
    IDX_CHAR_CFG_B,

    IDX_CHAR_C,
    IDX_CHAR_VAL_C,
    IDX_CHAR_CFG_C,
    IDX_CHAR_CFG_C_2,

    HRS_IDX_NB,
};

// BLE state enum
typedef enum {
	BLE_IDLE,		// 0
	ADVERTISING,	// 1
	BLE_CONNECTED	// 2
} BleStatesEnum;

#define send_File_Size 10*1024 //100*1024
#define send_File_chunk_size 64//128

void ble_ConsoleWriteToActor_xface(void *msg); // An interface function for the console to send msg to the actor


#endif /* MAIN_BLE_ACTOR_H_ */
