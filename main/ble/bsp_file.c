/*
 * bsp_file.c — BSP FILE channel handler (channel 0x02)
 *
 * Protocol magics (must match bsp_file_transfer.dart):
 *   BFGE  GET request (path)
 *   BFCA  cancel
 *   BFPS  PUT start (filename)
 *   BFPE  PUT end (size + crc32)
 *   BFHD  download header (size + crc32)
 *   BFEF  download EOF (size)
 *   BFOK  PUT success
 *   BFER  error message
 */

#include "bsp_file.h"
#include "bsp.h"
#include "gatt_service.h"
#include "FS.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "BSP_FILE";

#define JFS_ROOT        "Root/"
#define SD_MOUNT        "/sdcard"
#define BSP_FILE_MAX    (4 * 1024 * 1024)
#define BSP_XFER_CHUNK  4096

#define MAGIC_GET   "BFGE"
#define MAGIC_CANCEL "BFCA"
#define MAGIC_PUT   "BFPS"
#define MAGIC_PUTEOF "BFPE"
#define MAGIC_HDR   "BFHD"
#define MAGIC_EOF   "BFEF"
#define MAGIC_OK    "BFOK"
#define MAGIC_ERR   "BFER"

typedef struct {
    bool     put_active;
    bool     is_jfs;
    char     put_path[256];
    FS_FILE *jfs_w;
    FILE    *sd_w;
    uint32_t put_rx;
    uint32_t put_crc;
} bsp_put_ctx_t;

static bsp_put_ctx_t s_put;

/* ── CRC32 (matches Dart crc32Bytes / crc32Update) ─────────────────────── */
static uint32_t crc32_byte(uint32_t crc, uint8_t b)
{
    crc ^= b;
    for (int i = 0; i < 8; i++) {
        crc = (crc >> 1) ^ (0xEDB88320u & (uint32_t)(-(int32_t)(crc & 1u)));
    }
    return crc;
}

static void bsp_file_send(const uint8_t *payload, uint32_t len)
{
    bsp_send_record(BSP_CH_FILE, payload, len);
}

static void bsp_file_send_err(const char *msg)
{
    size_t mlen = strlen(msg);
    if (mlen > 512) mlen = 512;
    uint8_t buf[4 + 2 + 512];
    memcpy(buf, MAGIC_ERR, 4);
    buf[4] = (uint8_t)(mlen & 0xFF);
    buf[5] = (uint8_t)((mlen >> 8) & 0xFF);
    memcpy(buf + 6, msg, mlen);
    bsp_file_send(buf, (uint32_t)(6 + mlen));
}

static void bsp_put_reset(void)
{
    if (s_put.jfs_w) {
        FS_FClose(s_put.jfs_w);
        s_put.jfs_w = NULL;
    }
    if (s_put.sd_w) {
        fclose(s_put.sd_w);
        s_put.sd_w = NULL;
    }
    memset(&s_put, 0, sizeof(s_put));
}

/* Resolve device path to host filesystem path.
 *   /data/foo.txt  -> Root/data/foo.txt  (JFS)
 *   A:/System/x    -> Root/System/x       (JFS)
 *   B:/foo         -> /sdcard/foo         (SD)
 */
static bool bsp_resolve_path(const char *in, char *out, size_t out_sz, bool *is_jfs)
{
    if (!in || !out || out_sz < 8) return false;
    out[0] = '\0';

    if (strncmp(in, "A:/", 3) == 0 || strncmp(in, "a:/", 3) == 0) {
        snprintf(out, out_sz, "%s%s", JFS_ROOT, in + 3);
        if (is_jfs) *is_jfs = true;
        return true;
    }
    if (strncmp(in, "B:/", 3) == 0 || strncmp(in, "b:/", 3) == 0) {
        snprintf(out, out_sz, "%s/%s", SD_MOUNT, in + 3);
        if (is_jfs) *is_jfs = false;
        return true;
    }
    if (in[0] == '/') {
        snprintf(out, out_sz, "%s%s", JFS_ROOT, in + 1);
        if (is_jfs) *is_jfs = true;
        return true;
    }
    /* bare filename -> JFS root */
    snprintf(out, out_sz, "%s%s", JFS_ROOT, in);
    if (is_jfs) *is_jfs = true;
    return true;
}

typedef struct {
    char host_path[256];
    bool is_jfs;
} bsp_get_job_t;

static void bsp_get_task(void *arg)
{
    bsp_get_job_t *job = (bsp_get_job_t *)arg;
    uint32_t fsize = 0;
    uint32_t fcrc = 0;

    if (job->is_jfs) {
        FS_FILE *pf = FS_FOpen(job->host_path, "r");
        if (!pf) {
            bsp_file_send_err("file not found");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        fsize = FS_GetFileSize(pf);
        if (fsize > BSP_FILE_MAX) {
            FS_FClose(pf);
            bsp_file_send_err("file too large");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        uint8_t *buf = malloc(BSP_XFER_CHUNK);
        if (!buf) {
            FS_FClose(pf);
            bsp_file_send_err("out of memory");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        fcrc = 0;
        uint32_t rd_total = 0;
        while (rd_total < fsize) {
            U32 n = FS_Read(pf, buf, BSP_XFER_CHUNK);
            if (n == 0) break;
            for (U32 i = 0; i < n; i++) fcrc = crc32_byte(fcrc, buf[i]);
            rd_total += n;
        }
        fcrc &= 0xFFFFFFFFu;
        FS_FClose(pf);

        pf = FS_FOpen(job->host_path, "r");
        if (!pf) {
            free(buf);
            bsp_file_send_err("file reopen failed");
            free(job);
            vTaskDelete(NULL);
            return;
        }

        uint8_t hdr[12];
        memcpy(hdr, MAGIC_HDR, 4);
        hdr[4] = (uint8_t)(fsize & 0xFF);
        hdr[5] = (uint8_t)((fsize >> 8) & 0xFF);
        hdr[6] = (uint8_t)((fsize >> 16) & 0xFF);
        hdr[7] = (uint8_t)((fsize >> 24) & 0xFF);
        hdr[8] = (uint8_t)(fcrc & 0xFF);
        hdr[9] = (uint8_t)((fcrc >> 8) & 0xFF);
        hdr[10] = (uint8_t)((fcrc >> 16) & 0xFF);
        hdr[11] = (uint8_t)((fcrc >> 24) & 0xFF);
        bsp_file_send(hdr, 12);

        rd_total = 0;
        while (rd_total < fsize) {
            U32 n = FS_Read(pf, buf, BSP_XFER_CHUNK);
            if (n == 0) break;
            bsp_file_send(buf, n);
            rd_total += n;
            vTaskDelay(1);
        }
        FS_FClose(pf);
        free(buf);

        uint8_t eof[8];
        memcpy(eof, MAGIC_EOF, 4);
        eof[4] = (uint8_t)(fsize & 0xFF);
        eof[5] = (uint8_t)((fsize >> 8) & 0xFF);
        eof[6] = (uint8_t)((fsize >> 16) & 0xFF);
        eof[7] = (uint8_t)((fsize >> 24) & 0xFF);
        bsp_file_send(eof, 8);
    } else {
        struct stat st;
        if (stat(job->host_path, &st) != 0 || !S_ISREG(st.st_mode)) {
            bsp_file_send_err("file not found");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        fsize = (uint32_t)st.st_size;
        if (fsize > BSP_FILE_MAX) {
            bsp_file_send_err("file too large");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        FILE *fp = fopen(job->host_path, "rb");
        if (!fp) {
            bsp_file_send_err("file open failed");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        uint8_t *buf = malloc(BSP_XFER_CHUNK);
        if (!buf) {
            fclose(fp);
            bsp_file_send_err("out of memory");
            free(job);
            vTaskDelete(NULL);
            return;
        }
        fcrc = 0;
        fseek(fp, 0, SEEK_SET);
        for (uint32_t off = 0; off < fsize; ) {
            size_t n = fread(buf, 1, BSP_XFER_CHUNK, fp);
            if (n == 0) break;
            for (size_t i = 0; i < n; i++) fcrc = crc32_byte(fcrc, buf[i]);
            off += (uint32_t)n;
        }
        fcrc &= 0xFFFFFFFFu;
        fseek(fp, 0, SEEK_SET);

        uint8_t hdr[12];
        memcpy(hdr, MAGIC_HDR, 4);
        hdr[4] = (uint8_t)(fsize & 0xFF);
        hdr[5] = (uint8_t)((fsize >> 8) & 0xFF);
        hdr[6] = (uint8_t)((fsize >> 16) & 0xFF);
        hdr[7] = (uint8_t)((fsize >> 24) & 0xFF);
        hdr[8] = (uint8_t)(fcrc & 0xFF);
        hdr[9] = (uint8_t)((fcrc >> 8) & 0xFF);
        hdr[10] = (uint8_t)((fcrc >> 16) & 0xFF);
        hdr[11] = (uint8_t)((fcrc >> 24) & 0xFF);
        bsp_file_send(hdr, 12);

        for (uint32_t off = 0; off < fsize; ) {
            size_t n = fread(buf, 1, BSP_XFER_CHUNK, fp);
            if (n == 0) break;
            bsp_file_send(buf, (uint32_t)n);
            off += (uint32_t)n;
            vTaskDelay(1);
        }
        fclose(fp);
        free(buf);

        uint8_t eof[8];
        memcpy(eof, MAGIC_EOF, 4);
        eof[4] = (uint8_t)(fsize & 0xFF);
        eof[5] = (uint8_t)((fsize >> 8) & 0xFF);
        eof[6] = (uint8_t)((fsize >> 16) & 0xFF);
        eof[7] = (uint8_t)((fsize >> 24) & 0xFF);
        bsp_file_send(eof, 8);
    }

    ESP_LOGI(TAG, "GET complete %s", job->host_path);
    free(job);
    vTaskDelete(NULL);
}

static void bsp_handle_get(const uint8_t *payload, uint32_t len)
{
    if (len < 6) {
        bsp_file_send_err("BFGE too short");
        return;
    }
    uint16_t plen = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
    if (len < (uint32_t)(6 + plen) || plen == 0 || plen > 192) {
        bsp_file_send_err("invalid path length");
        return;
    }
    char path[193];
    memcpy(path, payload + 6, plen);
    path[plen] = '\0';
    if (strstr(path, "..")) {
        bsp_file_send_err("path traversal denied");
        return;
    }

    bsp_get_job_t *job = calloc(1, sizeof(*job));
    if (!job) {
        bsp_file_send_err("out of memory");
        return;
    }
    if (!bsp_resolve_path(path, job->host_path, sizeof(job->host_path), &job->is_jfs)) {
        free(job);
        bsp_file_send_err("invalid path");
        return;
    }

    xTaskCreate(bsp_get_task, "bsp_get", 8192, job, 5, NULL);
}

static void bsp_handle_put_start(const uint8_t *payload, uint32_t len)
{
    bsp_put_reset();
    if (len < 6) {
        bsp_file_send_err("BFPS too short");
        return;
    }
    uint16_t plen = (uint16_t)payload[4] | ((uint16_t)payload[5] << 8);
    if (len < (uint32_t)(6 + plen) || plen == 0 || plen > 192) {
        bsp_file_send_err("invalid filename length");
        return;
    }
    char name[193];
    memcpy(name, payload + 6, plen);
    name[plen] = '\0';
    if (strchr(name, '/') || strchr(name, '\\') || strstr(name, "..")) {
        bsp_file_send_err("invalid filename");
        return;
    }

    if (!bsp_resolve_path(name, s_put.put_path, sizeof(s_put.put_path), &s_put.is_jfs)) {
        bsp_file_send_err("path resolve failed");
        return;
    }
    s_put.put_active = true;
    s_put.put_rx = 0;
    s_put.put_crc = 0;

    if (s_put.is_jfs) {
        s_put.jfs_w = FS_FOpen(s_put.put_path, "w");
        if (!s_put.jfs_w) {
            bsp_put_reset();
            bsp_file_send_err("cannot create file");
            return;
        }
    } else {
        s_put.sd_w = fopen(s_put.put_path, "wb");
        if (!s_put.sd_w) {
            bsp_put_reset();
            bsp_file_send_err("cannot create file");
            return;
        }
    }
    ESP_LOGI(TAG, "PUT start %s", s_put.put_path);
}

static void bsp_handle_put_data(const uint8_t *payload, uint32_t len)
{
    if (!s_put.put_active || len == 0) return;
    for (uint32_t i = 0; i < len; i++) {
        s_put.put_crc = crc32_byte(s_put.put_crc, payload[i]);
    }
    if (s_put.is_jfs && s_put.jfs_w) {
        FS_Write(s_put.jfs_w, payload, len);
    } else if (s_put.sd_w) {
        fwrite(payload, 1, len, s_put.sd_w);
    }
    s_put.put_rx += len;
}

static void bsp_handle_put_end(const uint8_t *payload, uint32_t len)
{
    if (!s_put.put_active) {
        bsp_file_send_err("no active PUT");
        return;
    }
    if (len < 12) {
        bsp_put_reset();
        bsp_file_send_err("BFPE too short");
        return;
    }
    uint32_t expected_size = (uint32_t)payload[4] | ((uint32_t)payload[5] << 8) |
                             ((uint32_t)payload[6] << 16) | ((uint32_t)payload[7] << 24);
    uint32_t expected_crc = (uint32_t)payload[8] | ((uint32_t)payload[9] << 8) |
                            ((uint32_t)payload[10] << 16) | ((uint32_t)payload[11] << 24);

    if (s_put.jfs_w) FS_FClose(s_put.jfs_w);
    if (s_put.sd_w) fclose(s_put.sd_w);
    s_put.jfs_w = NULL;
    s_put.sd_w = NULL;

    s_put.put_crc &= 0xFFFFFFFFu;
    if (s_put.put_rx != expected_size || s_put.put_crc != expected_crc) {
        ESP_LOGW(TAG, "PUT CRC/size mismatch rx=%lu/%lu crc=%08lx/%08lx",
                 (unsigned long)s_put.put_rx, (unsigned long)expected_size,
                 (unsigned long)s_put.put_crc, (unsigned long)expected_crc);
        bsp_put_reset();
        bsp_file_send_err("CRC or size mismatch");
        return;
    }

    bsp_put_reset();
    bsp_file_send((const uint8_t *)MAGIC_OK, 4);
    ESP_LOGI(TAG, "PUT complete");
}

void bsp_handle_file(const uint8_t *payload, uint32_t len)
{
    if (!payload || len < 4) return;

    if (memcmp(payload, MAGIC_GET, 4) == 0) {
        bsp_handle_get(payload, len);
        return;
    }
    if (memcmp(payload, MAGIC_CANCEL, 4) == 0) {
        bsp_put_reset();
        return;
    }
    if (memcmp(payload, MAGIC_PUT, 4) == 0) {
        bsp_handle_put_start(payload, len);
        return;
    }
    if (memcmp(payload, MAGIC_PUTEOF, 4) == 0) {
        bsp_handle_put_end(payload, len);
        return;
    }
    if (s_put.put_active) {
        bsp_handle_put_data(payload, len);
    }
}
