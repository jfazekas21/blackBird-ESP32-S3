/*
 * gatt_service.c — BSP GATT server for HavenB480 ESP32-S3
 *
 * Registers the BSP UART service (UUID 6e410001-b5a3-f393-e0a9-e50e24dcca9e):
 *   TX char (6e410002): NOTIFY       — ESP→App (ACK frames, responses)
 *   RX char (6e410003): WRITE_NO_RSP — App→ESP (BSP DATA/RST frames)
 *
 * Each WRITE_NO_RSP delivers one complete ATT payload.  The GATT callback
 * copies it into a pvPortMalloc'd bsp_pkt_t and posts the pointer to a
 * FreeRTOS queue.  bsp_proc_task dequeues and calls bsp_feed() exactly once
 * per packet — ensuring every DATA frame is individually ACK'd.
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

/* ── Backward-compat stubs ─────────────────────────────────────────────────
 * Referenced by BLE_Actor.c; never written by this file.
 */
bool    fg_BLE_Data_Rcvd               = false;
uint8_t gatt_svr_thrpt_static_write[512];
uint8_t gatt_svr_thrpt_static_Read[512];
uint16_t hrs_hrm_handle;

/* ── BSP UART globals ───────────────────────────────────────────────────── */
uint16_t          bsp_tx_val_handle  = 0;
bool              bsp_notify_enabled = false;
volatile uint16_t g_bsp_conn_handle  = 0;

/* ── Pointer queue (GATT callback → bsp_proc_task) ─────────────────────── */
#define BSP_QUEUE_DEPTH 64

typedef struct {
    uint16_t len;
    uint8_t  data[1];   /* variable: allocated as sizeof - 1 + len */
} bsp_pkt_t;

static QueueHandle_t s_bsp_queue = NULL;
static TaskHandle_t  s_bsp_task  = NULL;
static bsp_state_t   s_bsp_state;

/* ── UUID static storage — BLE_UUID128_INIT produces a valid ble_uuid128_t initializer */
static const ble_uuid128_t bsp_svc_uuid = BSP_SVC_UUID128_INIT;
static const ble_uuid128_t bsp_tx_uuid  = BSP_TX_CHR_UUID128_INIT;
static const ble_uuid128_t bsp_rx_uuid  = BSP_RX_CHR_UUID128_INIT;

/* ── RX characteristic access callback ─────────────────────────────────── */
static int bsp_rx_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return 0;

    uint16_t pkt_len = OS_MBUF_PKTLEN(ctxt->om);
    if (pkt_len == 0) return 0;

    bsp_pkt_t *pkt = pvPortMalloc(sizeof(bsp_pkt_t) - 1 + pkt_len);
    if (!pkt) {
        printf("[BSP] OOM drop %u bytes\n", pkt_len);
        return 0;
    }
    pkt->len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, pkt->data, pkt_len, &pkt->len) != 0) {
        vPortFree(pkt);
        return 0;
    }

    if (xQueueSend(s_bsp_queue, &pkt, 0) != pdTRUE) {
        printf("[BSP] queue full — drop %u bytes\n", pkt->len);
        vPortFree(pkt);
    }
    return 0;
}

/* ── TX (NOTIFY) characteristic — central reads not expected ─────────────── */
static int bsp_tx_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                              struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle; (void)attr_handle; (void)ctxt; (void)arg;
    return 0;
}

/* ── GATT service table ─────────────────────────────────────────────────── */
static const struct ble_gatt_svc_def bsp_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &bsp_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid       = &bsp_tx_uuid.u,
                .access_cb  = bsp_tx_chr_access,
                .val_handle = &bsp_tx_val_handle,
                .flags      = BLE_GATT_CHR_F_NOTIFY,
            },
            {
                .uuid      = &bsp_rx_uuid.u,
                .access_cb = bsp_rx_chr_access,
                .flags     = BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            { 0 },
        },
    },
    { 0 },
};

/* ── bsp_notify ─────────────────────────────────────────────────────────── */
void bsp_notify(uint16_t conn_handle, const uint8_t *data, uint16_t len)
{
    if (!bsp_notify_enabled || bsp_tx_val_handle == 0) return;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (om) ble_gatts_notify_custom(conn_handle, bsp_tx_val_handle, om);
}

/* ── bsp_send_record ────────────────────────────────────────────────────── */
/* Sends a BSP record as one or more DATA frames, each fitting within the
 * negotiated ATT MTU.  The Flutter BspRecordDemux streams bytes across frames
 * so large JSON responses (e.g. MENU.WIFI ≈ 3 KB) are reassembled correctly.
 *
 * Wire format per DATA frame:
 *   [0x01][seq:1][plen_lo:1][plen_hi:1][chunk_of_rec:N][crc:2]
 * First chunk starts with the BSP record header [channel:1][json_len:3LE].
 */
static uint8_t s_tx_seq = 0;

void bsp_send_record(uint8_t channel, const uint8_t *payload, uint32_t len)
{
    /* Build the flat BSP record: [channel:1][len:3LE][payload:N] */
    uint32_t rec_total = 4u + len;
    uint8_t *rec = pvPortMalloc(rec_total);
    if (!rec) {
        printf("[BSP] send_record OOM len=%lu\n", (unsigned long)len);
        return;
    }
    rec[0] = channel;
    rec[1] = (uint8_t)(len & 0xFFu);
    rec[2] = (uint8_t)((len >> 8u)  & 0xFFu);
    rec[3] = (uint8_t)((len >> 16u) & 0xFFu);
    if (len > 0u) memcpy(rec + 4, payload, len);

    /* Max record bytes per DATA frame: ATT_MTU - 3(ATT) - 4(BSP hdr) - 2(CRC) */
    uint16_t att_mtu = ble_att_mtu(g_bsp_conn_handle);
    if (att_mtu < 23u) att_mtu = 23u;
    uint32_t max_chunk = (uint32_t)(att_mtu - 9u);

    /* Send one DATA frame per chunk */
    uint32_t offset = 0u;
    while (offset < rec_total) {
        uint32_t chunk = rec_total - offset;
        if (chunk > max_chunk) chunk = max_chunk;

        uint32_t frame_len = 4u + chunk + 2u;
        uint8_t *frame = pvPortMalloc(frame_len);
        if (!frame) {
            printf("[BSP] send_record OOM frame chunk=%lu\n", (unsigned long)chunk);
            break;
        }

        frame[0] = BSP_TYPE_DATA;
        frame[1] = s_tx_seq++;
        frame[2] = (uint8_t)(chunk & 0xFFu);
        frame[3] = (uint8_t)((chunk >> 8u) & 0xFFu);
        memcpy(frame + 4, rec + offset, chunk);

        uint16_t crc = bsp_crc16(frame, (uint16_t)(frame_len - 2u));
        frame[frame_len - 2u] = (uint8_t)(crc & 0xFFu);
        frame[frame_len - 1u] = (uint8_t)(crc >> 8u);

        bsp_notify(g_bsp_conn_handle, frame, (uint16_t)frame_len);
        vPortFree(frame);
        offset += chunk;
    }

    vPortFree(rec);
}

/* ── BSP processing task ────────────────────────────────────────────────── */
/*
 * Throughput optimisation: drain ALL queued packets before sending any ACK.
 * Each bsp_notify() call (from bsp_send_ack) consumes one BLE connection
 * event (~60 ms on Windows).  Sending N ACKs for N frames costs N × 60 ms;
 * one cumulative ACK costs 60 ms total — an N× improvement.
 *
 * bsp_feed() now sets s->ack_pending instead of calling bsp_send_ack().
 * bsp_flush_ack() sends the single cumulative ACK after the drain.
 * OOO NACKs are still sent immediately inside bsp_feed() (not deferred).
 */
static void bsp_proc_task(void *arg)
{
    (void)arg;
    bsp_pkt_t *pkt;
    for (;;) {
        /* Block until at least one packet arrives */
        if (xQueueReceive(s_bsp_queue, &pkt, portMAX_DELAY) != pdTRUE) continue;
        bsp_feed(&s_bsp_state, pkt->data, pkt->len);
        vPortFree(pkt);

        /* 2 ms batching window: NimBLE delivers packets from the same BLE
         * connection event one at a time through the host stack.  This delay
         * lets it finish queuing all of them before we drain, so the cumulative
         * ACK covers the whole batch instead of firing once per packet. */
        vTaskDelay(pdMS_TO_TICKS(2));

        /* Drain every packet that arrived during the batching window */
        while (xQueueReceive(s_bsp_queue, &pkt, 0) == pdTRUE) {
            bsp_feed(&s_bsp_state, pkt->data, pkt->len);
            vPortFree(pkt);
        }

        /* One cumulative ACK for the entire batch */
        bsp_flush_ack(&s_bsp_state);
    }
}

void bsp_proc_task_start(void)
{
    if (s_bsp_task) return;
    xTaskCreatePinnedToCore(bsp_proc_task, "bsp_proc", 8192, NULL,
                            tskIDLE_PRIORITY + 3, &s_bsp_task, 1);
}

/* ── Connection lifecycle helpers ───────────────────────────────────────── */
void bsp_on_connect(uint16_t conn_handle)
{
    g_bsp_conn_handle  = conn_handle;
    bsp_notify_enabled = false;   /* cleared until Flutter subscribes */
    s_tx_seq           = 0;       /* Dart engine resets _rxExpected=0 on connect */
    bsp_reset(&s_bsp_state, conn_handle);
    printf("[BSP] connected conn=%d\n", conn_handle);
}

void bsp_on_disconnect(void)
{
    bsp_notify_enabled  = false;
    g_bsp_conn_handle   = 0;
    s_bsp_state.active  = false;
    printf("[BSP] disconnected\n");
}

/* ── gatt_svr_register_cb ───────────────────────────────────────────────── */
void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];
    (void)arg;
    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        printf("[GATT] service %s handle=%d\n",
               ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        printf("[GATT] characteristic %s def=%d val=%d\n",
               ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
               ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;
    default:
        break;
    }
}

/* ── gatts_svr_init ─────────────────────────────────────────────────────── */
int gatts_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    s_bsp_queue = xQueueCreate(BSP_QUEUE_DEPTH, sizeof(bsp_pkt_t *));
    if (!s_bsp_queue) {
        printf("[GATT] queue create failed\n");
        return BLE_HS_ENOMEM;
    }

    rc = ble_gatts_count_cfg(bsp_svcs);
    if (rc != 0) {
        printf("[GATT] count_cfg rc=%d\n", rc);
        return rc;
    }
    rc = ble_gatts_add_svcs(bsp_svcs);
    if (rc != 0) {
        printf("[GATT] add_svcs rc=%d\n", rc);
        return rc;
    }

    printf("[GATT] BSP service registered tx_handle=%d\n", bsp_tx_val_handle);
    bsp_proc_task_start();
    return 0;
}
