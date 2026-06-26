/*
 * gatt_service.h
 *
 *  Created on: 11-Dec-2023
 *      Author: Sai
 */

#ifndef MAIN_BLE_GATT_SERVICE_H_
#define MAIN_BLE_GATT_SERVICE_H_

#include <stdint.h>
#include <stdbool.h>

/* Heart-rate configuration */
#define GATT_HRS_UUID                           0x180D
//#define GATT_HRS_MEASUREMENT_UUID               0x2A37
//#define GATT_HRS_BODY_SENSOR_LOC_UUID           0x2A38
#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24
//#define GATT_SW_VERSION_UUID                    0x2A29

extern uint16_t hrs_hrm_handle;

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

/** GATT server. */
#define GATT_SVR_SVC_ALERT_UUID               0x1811
#define GATT_SVR_CHR_SUP_NEW_ALERT_CAT_UUID   0x2A47
#define GATT_SVR_CHR_NEW_ALERT                0x2A46
#define GATT_SVR_CHR_SUP_UNR_ALERT_CAT_UUID   0x2A48
#define GATT_SVR_CHR_UNR_ALERT_STAT_UUID      0x2A45
#define GATT_SVR_CHR_ALERT_NOT_CTRL_PT        0x2A44

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatts_svr_init(void);

/* ── BSP UART service (matches Flutter BleUartService UUIDs) ──────────────
 * UUID string:  6e410001-b5a3-f393-e0a9-e50e24dcca9e
 * BLE_UUID128_INIT takes bytes in little-endian wire order:
 *   byte[0] = last octet of UUID string = 0x9e, ... byte[15] = 0x6e
 */
#define BSP_SVC_UUID128_INIT \
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0, \
                     0x93,0xf3,0xa3,0xb5,0x01,0x00,0x41,0x6e)

/* 6e410002 — TX characteristic: NOTIFY, ESP→App (ACKs, responses) */
#define BSP_TX_CHR_UUID128_INIT \
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0, \
                     0x93,0xf3,0xa3,0xb5,0x02,0x00,0x41,0x6e)

/* 6e410003 — RX characteristic: WRITE_NO_RSP, App→ESP (BSP DATA frames) */
#define BSP_RX_CHR_UUID128_INIT \
    BLE_UUID128_INIT(0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0, \
                     0x93,0xf3,0xa3,0xb5,0x03,0x00,0x41,0x6e)

/* Handle for the TX characteristic — used by BLE_Actor.c to send notifications */
extern uint16_t bsp_tx_val_handle;

/* Set true by BLE_GAP_EVENT_SUBSCRIBE when Flutter enables notifications */
extern bool bsp_notify_enabled;

/* App-driven flow control: set by a BSP PAUSE frame, cleared by RESUME.
 * While true, proactive/unsolicited senders (scale telemetry) suppress their
 * pushes. Command/response traffic and BSP-layer ACKs are unaffected. */
extern volatile bool bsp_tx_paused;

/* Set to the current NimBLE conn_handle on BLE_GAP_EVENT_CONNECT so the
 * BSP processing task can address ACK notifications to the right peer */
extern volatile uint16_t g_bsp_conn_handle;

/* Send len bytes as a BLE notification on the TX characteristic */
void bsp_notify(uint16_t conn_handle, const uint8_t *data, uint16_t len);

/* Build a BSP DATA frame wrapping one BSP record and notify.
 * channel: BSP_CH_JSON / BSP_CH_FILE / BSP_CH_BENCH (from bsp.h)
 * Uses an internal static TX sequence counter.
 */
void bsp_send_record(uint8_t channel, const uint8_t *payload, uint32_t len);

/* Called from BLE_GAP_EVENT_CONNECT (success): resets BSP state and records handle */
void bsp_on_connect(uint16_t conn_handle);

/* Called from BLE_GAP_EVENT_DISCONNECT: clears notify flag and marks BSP inactive */
void bsp_on_disconnect(void);

#endif /* MAIN_BLE_GATT_SERVICE_H_ */
