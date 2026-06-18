/*
 * bsp.h — BSP UART protocol implementation for HavenB480 ESP32-S3
 *
 * Implements the receiver side of the BSP sliding-window protocol used by
 * the Blackbird Flutter app (bsp_engine.dart). Frames, CRC, ACKs, record
 * demux, and channel handlers are all defined here.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ── Frame type bytes (must match bsp_engine.dart constants) ─────────────── */
#define BSP_TYPE_DATA   0x01
#define BSP_TYPE_ACK    0x02
#define BSP_TYPE_RST    0x03
#define BSP_TYPE_PAUSE  0x04
#define BSP_TYPE_RESUME 0x05

/* ── Protocol constants ───────────────────────────────────────────────────── */
/* Window size must match Flutter windowSize=16 */
#define BSP_WINDOW     16
/* Max bytes in one ATT PDU at MTU=517: 517 - 3 ATT header = 514 payload.
 * A DATA frame wraps that in 4-byte header + 2-byte CRC = 508 payload bytes.
 * Allocate 520 to give safe headroom. */
#define BSP_MAX_PDU    520

/* ── BSP record channel IDs (must match BspChannel in bsp_record.dart) ───── */
#define BSP_CH_JSON    0x01   /* JSON commands to existing actor system       */
#define BSP_CH_FILE    0x02   /* File PUT/data/EOF to SPIFFS                  */
#define BSP_CH_RAW     0x03   /* Raw UART mirror (not forwarded on this board)*/
#define BSP_CH_BENCH   0x04   /* Speed-test bulk data (rx_bytes counter)      */

/* ── File-transfer magic bytes (must match bsp_file_transfer.dart) ───────── */
#define BSP_FT_PUT     "BFPS"   /* PUT start: magic(4)+pathlen(2)+path        */
#define BSP_FT_EOF     "BFPE"   /* PUT end:   magic(4)+size(4)+crc32(4)       */
#define BSP_FT_OK      "BFOK"   /* ACK success (4 bytes)                      */
#define BSP_FT_ERR     "BFER"   /* ACK error:  magic(4)+msglen(2)+msg         */

/* ── Per-connection BSP state ─────────────────────────────────────────────── */
typedef struct {
    uint8_t  rx_expected;   /* next expected DATA sequence number (mod 256)  */
    uint8_t  rx_window;     /* credits currently advertised to sender        */
    uint16_t conn_handle;   /* NimBLE connection handle                      */
    bool     active;        /* true while a connection is live               */
} bsp_state_t;

/* ── Speed-test bench state (armed/disarmed by JSON commands) ─────────────── */
typedef struct {
    volatile uint32_t rx_bytes;
    volatile uint32_t crc32;
    volatile bool     armed;
    volatile bool     active;
} bsp_bench_state_t;

extern bsp_bench_state_t g_bsp_bench;

/* ── Public API ───────────────────────────────────────────────────────────── */

/*
 * CRC16-CCITT: poly=0x1021, init=0xFFFF, no final XOR.
 * Must match bsp_crc.dart bspCrc16Ccitt().
 */
uint16_t bsp_crc16(const uint8_t *data, uint16_t len);

/* Reset BSP state for a new connection */
void bsp_reset(bsp_state_t *s, uint16_t conn_handle);

/*
 * Feed one raw ATT write payload (one BSP DATA or RST frame) into the
 * protocol engine. Verifies CRC, checks sequence, calls bsp_record_feed,
 * and sends an ACK via bsp_notify.
 */
void bsp_feed(bsp_state_t *s, const uint8_t *pkt, uint16_t pkt_len);

/* Send a BSP ACK frame (5 bytes) via bsp_notify */
void bsp_send_ack(bsp_state_t *s, uint8_t seq, uint8_t window);

/*
 * Feed raw payload bytes into the BSP record reassembler.
 * A BSP DATA frame payload may span multiple records, or a record may
 * span multiple DATA frames. This function buffers until a complete
 * record [channel:1][len:3][payload:N] is assembled, then calls
 * bsp_on_record.
 */
void bsp_record_feed(const uint8_t *data, uint16_t len);

/*
 * Called when a complete BSP record is ready.
 * Dispatches to bsp_handle_json / bsp_handle_file / bsp_handle_bench.
 */
void bsp_on_record(uint8_t channel, const uint8_t *payload, uint32_t len);

/*
 * Start the BSP processing FreeRTOS task (priority BLE_TASK_PRIORITY+1,
 * Core 1, 8 KB stack). Called once from gatts_svr_init().
 */
void bsp_proc_task_start(void);
