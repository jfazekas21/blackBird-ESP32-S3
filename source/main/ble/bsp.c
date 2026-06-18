/*
 * bsp.c — BSP UART protocol receiver for HavenB480 ESP32-S3
 *
 * Implements the ESP32 receiver side of the Blackbird BSP sliding-window
 * protocol. Matches bsp_engine.dart, bsp_record.dart, bsp_crc.dart, and
 * bsp_file_transfer.dart in the Flutter app exactly.
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "bsp.h"
#include "gatt_service.h"

static const char *TAG = "BSP";

/* ═══════════════════════════════════════════════════════════════════════════
 * CRC-CCITT: poly=0x1021, init=0xFFFF, no final XOR.
 * Must match bspCrc16Ccitt() in bsp_crc.dart.
 * ═══════════════════════════════════════════════════════════════════════════ */
uint16_t bsp_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++)
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BSP state management
 * ═══════════════════════════════════════════════════════════════════════════ */
void bsp_reset(bsp_state_t *s, uint16_t conn_handle)
{
    s->rx_expected = 0;
    s->rx_window   = BSP_WINDOW;
    s->conn_handle = conn_handle;
    s->active      = true;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * bsp_send_ack: build and send a 5-byte ACK frame.
 * Wire format (from bsp_engine.dart::buildAck):
 *   [typeAck:1][seq:1][window:1][crc16_lo:1][crc16_hi:1]
 * ═══════════════════════════════════════════════════════════════════════════ */
void bsp_send_ack(bsp_state_t *s, uint8_t seq, uint8_t window)
{
    uint8_t buf[5];
    buf[0] = BSP_TYPE_ACK;
    buf[1] = seq;
    buf[2] = window;
    uint16_t crc = bsp_crc16(buf, 3);
    buf[3] = (uint8_t)(crc & 0xFF);
    buf[4] = (uint8_t)(crc >> 8);
    bsp_notify(s->conn_handle, buf, 5);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * bsp_feed: decode one BSP frame from a raw ATT write payload.
 * DATA frame wire format (from bsp_engine.dart::buildData):
 *   [type:1][seq:1][len_lo:1][len_hi:1][payload:N][crc_lo:1][crc_hi:1]
 * ═══════════════════════════════════════════════════════════════════════════ */
void bsp_feed(bsp_state_t *s, const uint8_t *pkt, uint16_t pkt_len)
{
    if (!s->active || pkt_len < 1) return;

    uint8_t ptype = pkt[0];

    if (ptype == BSP_TYPE_RST) {
        /* RST resets sequence numbers on both sides */
        if (pkt_len < 3) return;
        uint16_t crc = bsp_crc16(pkt, 1);
        if (crc != ((uint16_t)pkt[1] | ((uint16_t)pkt[2] << 8))) return;
        s->rx_expected = 0;
        s->rx_window   = BSP_WINDOW;
        ESP_LOGI(TAG, "RST received — seq reset");
        return;
    }

    if (ptype != BSP_TYPE_DATA) return;   /* PAUSE/RESUME: ignore on RX path */
    if (pkt_len < 7) return;              /* min: 4-byte hdr + 1 payload + 2 CRC */

    uint8_t  seq  = pkt[1];
    uint16_t plen = (uint16_t)pkt[2] | ((uint16_t)pkt[3] << 8);

    /* Sanity: payload must fit in packet */
    if ((uint32_t)(4u + plen + 2u) > (uint32_t)pkt_len) return;

    /* Verify CRC over [type, seq, len_lo, len_hi, payload…] */
    uint16_t calc  = bsp_crc16(pkt, (uint16_t)(4 + plen));
    uint16_t rxcrc = (uint16_t)pkt[4 + plen] | ((uint16_t)pkt[5 + plen] << 8);
    if (calc != rxcrc) {
        ESP_LOGW(TAG, "CRC error seq=%d calc=%04x rx=%04x", seq, calc, rxcrc);
        return;   /* silent drop — Flutter will retransmit on ACK timeout */
    }

    /* Out-of-order: re-ACK last good seq to trigger retransmit from sender */
    if (seq != s->rx_expected) {
        uint8_t last_good = (uint8_t)((s->rx_expected - 1u) & 0xFFu);
        ESP_LOGW(TAG, "OOO seq=%d expected=%d", seq, s->rx_expected);
        bsp_send_ack(s, last_good, s->rx_window);
        return;
    }

    /* Feed payload into record reassembler */
    bsp_record_feed(pkt + 4, plen);

    /* Advance expected sequence, always advertise full window */
    s->rx_expected = (uint8_t)((s->rx_expected + 1u) & 0xFFu);
    bsp_send_ack(s, seq, BSP_WINDOW);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * BSP record reassembler
 *
 * A BSP record header (from bsp_record.dart):
 *   [u8 channel][u24 length LE] = 4 bytes
 * followed by `length` payload bytes.
 *
 * Records are delivered in-order by the BSP layer but may be fragmented
 * across multiple DATA frames or multiple records may follow each other.
 * ═══════════════════════════════════════════════════════════════════════════ */
static struct {
    uint8_t  hdr[4];
    uint8_t  hdr_fill;   /* bytes accumulated so far in hdr[] */
    uint8_t  channel;
    uint32_t rec_len;    /* total payload bytes */
    uint32_t rec_fill;   /* bytes received so far */
    uint8_t *buf;        /* PSRAM allocation, rec_len bytes */
} g_rec;

void bsp_record_feed(const uint8_t *data, uint16_t len)
{
    uint16_t pos = 0;

    while (pos < len) {

        /* Phase A: accumulate 4-byte header */
        if (g_rec.hdr_fill < 4) {
            uint16_t need = (uint16_t)(4u - g_rec.hdr_fill);
            uint16_t take = (len - pos < need) ? (len - pos) : need;
            memcpy(g_rec.hdr + g_rec.hdr_fill, data + pos, take);
            g_rec.hdr_fill = (uint8_t)(g_rec.hdr_fill + take);
            pos += take;
            if (g_rec.hdr_fill < 4) return;   /* still incomplete */

            g_rec.channel  = g_rec.hdr[0];
            g_rec.rec_len  = (uint32_t)g_rec.hdr[1]
                           | ((uint32_t)g_rec.hdr[2] << 8)
                           | ((uint32_t)g_rec.hdr[3] << 16);
            g_rec.rec_fill = 0;
            g_rec.buf      = NULL;

            /* Guard against corrupt length (matches Dart demux guard of 4 MB) */
            if (g_rec.rec_len > 4u * 1024u * 1024u) {
                ESP_LOGE(TAG, "record len %lu too large — reset demux", (unsigned long)g_rec.rec_len);
                memset(&g_rec, 0, sizeof(g_rec));
                return;
            }

            if (g_rec.rec_len == 0) {
                bsp_on_record(g_rec.channel, NULL, 0);
                g_rec.hdr_fill = 0;
                continue;
            }

            /* Allocate payload buffer from PSRAM */
            g_rec.buf = heap_caps_malloc(g_rec.rec_len,
                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
            if (!g_rec.buf) {
                ESP_LOGE(TAG, "OOM for record len=%lu — dropping", (unsigned long)g_rec.rec_len);
                /* Keep hdr_fill=4 so Phase B discards the bytes */
            }
        }

        /* Phase B: fill payload */
        uint32_t need = g_rec.rec_len - g_rec.rec_fill;
        uint32_t avail = (uint32_t)(len - pos);
        uint32_t take  = (avail < need) ? avail : need;

        if (g_rec.buf) {
            memcpy(g_rec.buf + g_rec.rec_fill, data + pos, take);
        }
        g_rec.rec_fill += take;
        pos += (uint16_t)take;

        if (g_rec.rec_fill >= g_rec.rec_len) {
            if (g_rec.buf) {
                bsp_on_record(g_rec.channel, g_rec.buf, g_rec.rec_len);
                free(g_rec.buf);
                g_rec.buf = NULL;
            }
            /* Reset for next record */
            g_rec.hdr_fill = 0;
            g_rec.rec_len  = 0;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * bsp_on_record: dispatch a complete record to the right channel handler.
 * Channel handlers are defined in BLE_Actor.c.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Weak stubs — overridden by BLE_Actor.c implementations when present */
__attribute__((weak)) void bsp_handle_json (const uint8_t *p, uint32_t l) { (void)p; (void)l; }
__attribute__((weak)) void bsp_handle_file (const uint8_t *p, uint32_t l) { (void)p; (void)l; }
__attribute__((weak)) void bsp_handle_bench(const uint8_t *p, uint32_t l) { (void)p; (void)l; }

void bsp_on_record(uint8_t channel, const uint8_t *payload, uint32_t len)
{
    switch (channel) {
    case BSP_CH_JSON:   bsp_handle_json (payload, len); break;
    case BSP_CH_FILE:   bsp_handle_file (payload, len); break;
    case BSP_CH_BENCH:  bsp_handle_bench(payload, len); break;
    case BSP_CH_RAW:    break;   /* not routed on this board */
    default:
        ESP_LOGW(TAG, "unknown channel 0x%02x len=%lu", channel, (unsigned long)len);
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Speed-test bench state (armed/disarmed by JSON commands in BLE_Actor.c)
 * ═══════════════════════════════════════════════════════════════════════════ */
bsp_bench_state_t g_bsp_bench = {0, 0, false, false};
