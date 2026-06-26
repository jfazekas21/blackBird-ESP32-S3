/*
 * ble_comms_push.c — async msg/err push per Comms Wrapper Spec §7
 */

#include "ble_comms_push.h"
#include "bsp.h"
#include "gatt_service.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "COMMS_PUSH";
static TaskHandle_t s_telemetry_task = NULL;
static bool s_telemetry_running = false;

void ble_comms_push_json(const char *json_text)
{
    if (!json_text || !bsp_notify_enabled) return;
    bsp_send_record(BSP_CH_JSON, (const uint8_t *)json_text, (uint32_t)strlen(json_text));
}

void ble_comms_push_async_err(const char *actor, int error_code, const char *reason)
{
    if (!actor || !reason || !bsp_notify_enabled) return;

    cJSON *root = cJSON_CreateObject();
    cJSON *err  = cJSON_CreateObject();
    cJSON *body = cJSON_CreateObject();
    cJSON_AddNumberToObject(body, "errorCode", error_code);
    cJSON_AddStringToObject(body, "reason", reason);
    cJSON_AddStringToObject(err, "act", actor);
    cJSON_AddItemToObject(err, actor, body);
    cJSON_AddItemToObject(root, "err", err);

    char *s = cJSON_PrintUnformatted(root);
    if (s) {
        ble_comms_push_json(s);
        free(s);
    }
    cJSON_Delete(root);
    ESP_LOGI(TAG, "async err act=%s code=%d", actor, error_code);
}

static void telemetry_task(void *arg)
{
    (void)arg;
    while (s_telemetry_running) {
        if (bsp_notify_enabled && !bsp_tx_paused) {
            char gross[16], net[16];
            uint32_t w = (esp_random() % 5000u) + 1u;
            uint32_t n = (esp_random() % 5000u) + 1u;
            snprintf(gross, sizeof(gross), "%u", (unsigned)w);
            snprintf(net, sizeof(net), "%u", (unsigned)n);
            ble_comms_push_scale_telemetry(gross, net);
        }
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    s_telemetry_task = NULL;
    vTaskDelete(NULL);
}

void ble_comms_telemetry_start(void)
{
    if (s_telemetry_task) return;
    s_telemetry_running = true;
    xTaskCreate(telemetry_task, "ble_telemetry", 4096, NULL, 3, &s_telemetry_task);
}

void ble_comms_telemetry_stop(void)
{
    s_telemetry_running = false;
}

void ble_comms_push_scale_telemetry(const char *gross, const char *net)
{
    if (!bsp_notify_enabled || bsp_tx_paused) return;
    cJSON *root = cJSON_CreateObject();
    cJSON *msg  = cJSON_CreateObject();
    cJSON *sc   = cJSON_CreateObject();
    if (gross) cJSON_AddStringToObject(sc, "gross", gross);
    if (net)   cJSON_AddStringToObject(sc, "net", net);
    cJSON_AddStringToObject(msg, "act", "Scale");
    cJSON_AddItemToObject(msg, "Scale", sc);
    cJSON_AddItemToObject(root, "msg", msg);
    char *s = cJSON_PrintUnformatted(root);
    if (s) {
        ble_comms_push_json(s);
        free(s);
    }
    cJSON_Delete(root);
}
