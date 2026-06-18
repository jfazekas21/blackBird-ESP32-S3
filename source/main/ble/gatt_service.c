/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "gatt_service.h"
#include "bsp.h"

/* ── Backward-compat stubs kept so BLE_Actor.c compiles unchanged ─────────
 * The old BLE_Read_Write task waits on BLE_Read_Write_Task_Handle via
 * ulTaskNotifyTake; that notify is no longer sent, so the task blocks
 * harmlessly forever. fg_BLE_Data_Rcvd and the static buffers are never
 * written but must be exported to satisfy the linker.
 */
bool    fg_BLE_Data_Rcvd = false;
uint8_t gatt_svr_thrpt_static_write[512];
uint8_t gatt_svr_thrpt_static_Read[512];

/* hrs_hrm_handle referenced by other actors */
uint16_t hrs_hrm_handle;

/* ── BSP UART GATT service globals ────────────────────────────────────────── */
uint16_t          bsp_tx_val_handle   = 0;
bool              bsp_notify_enabled  = false;
/* Updated by BLE_Actor.c on BLE_GAP_EVENT_CONNECT; read by bsp_proc_task */
volatile uint16_t g_bsp_conn_handle   = 0;

/* Pointer queue: one bsp_pkt_t* per WRITE_NO_RSP = one complete BSP frame.
 * Storing pointers (4 bytes each) instead of inline data avoids a 16 KB queue
 * allocation and eliminates the large memcpy under FreeRTOS critical section
 * that xQueueSend performs when item size is large. */
typedef struct {
    uint8_t  data[BSP_MAX_PDU];
    uint16_t len;
} bsp_pkt_t;

#define BSP_QUEUE_LEN   32      /* covers window=16 plus burst headroom */
static QueueHandle_t bsp_rx_queue = NULL;   /* queue of bsp_pkt_t* pointers */

/* ── RX access callback: called by NimBLE host task on every WRITE_NO_RSP ── */
static int bsp_rx_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return 0;

    bsp_pkt_t *pkt = pvPortMalloc(sizeof(bsp_pkt_t));
    if (!pkt) return 0;

    pkt->len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, pkt->data, sizeof(pkt->data), &pkt->len) != 0 || pkt->len == 0) {
        vPortFree(pkt);
        return 0;
    }

    /* Enqueue pointer — only 4 bytes under critical section.
     * Drop on overflow; BSP window=16 prevents sustained overflow. */
    if (xQueueSend(bsp_rx_queue, &pkt, 0) != pdTRUE)
        vPortFree(pkt);
    return 0;
}

/* ── BSP GATT service table ──────────────────────────────────────────────── */
static const ble_uuid128_t bsp_svc_uuid =
    BSP_SVC_UUID128_INIT;
static const ble_uuid128_t bsp_tx_uuid =
    BSP_TX_CHR_UUID128_INIT;
static const ble_uuid128_t bsp_rx_uuid =
    BSP_RX_CHR_UUID128_INIT;

static const struct ble_gatt_svc_def bsp_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &bsp_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                /* TX: ESP→App notifications (ACKs, JSON responses) */
                .uuid       = &bsp_tx_uuid.u,
                .access_cb  = bsp_rx_chr_access,
                .val_handle = &bsp_tx_val_handle,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            {
                /* RX: App→ESP BSP DATA frames, write-without-response */
                .uuid      = &bsp_rx_uuid.u,
                .access_cb = bsp_rx_chr_access,
                .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 },
        },
    },
    { 0 },
};

/* ── bsp_notify: send data as a BLE notification on the TX characteristic ── */
void bsp_notify(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    if (bsp_tx_val_handle == 0) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return;
    ble_gatts_notify_custom(conn_handle, bsp_tx_val_handle, om);
}

/* ── BSP processing task: one bsp_feed() call per queued packet ──────────── */
static void bsp_proc_task(void *arg)
{
    bsp_state_t state;
    bsp_reset(&state, 0);

    bsp_pkt_t *pkt;
    while (1) {
        if (xQueueReceive(bsp_rx_queue, &pkt, portMAX_DELAY) != pdTRUE) continue;

        uint16_t ch = (uint16_t)g_bsp_conn_handle;
        if (!state.active || state.conn_handle != ch)
            bsp_reset(&state, ch);

        bsp_feed(&state, pkt->data, pkt->len);
        vPortFree(pkt);
    }
}

void bsp_proc_task_start(void)
{
    /* Priority BLE_TASK_PRIORITY+1 = 3, Core 1 (same as NimBLE host) */
    xTaskCreatePinnedToCore(bsp_proc_task, "bsp_proc",
                            8 * 1024, NULL, 3, NULL, 1);
}

/* ── gatt_svr_register_cb: unchanged ─────────────────────────────────────── */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;
    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;
    default:
        assert(0);
        break;
    }
}

/* ── gatts_svr_init ───────────────────────────────────────────────────────── */
int gatts_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(bsp_svcs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(bsp_svcs);
    if (rc != 0) return rc;

    /* Queue stores bsp_pkt_t* pointers — 32 × 4 bytes = 128 bytes total */
    bsp_rx_queue = xQueueCreate(BSP_QUEUE_LEN, sizeof(bsp_pkt_t *));
    bsp_proc_task_start();

    return 0;
}
