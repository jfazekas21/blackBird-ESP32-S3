/*
 * m225_methods.c — method dispatch + NVS persistence helpers
 */

#include "m225_methods.h"
#include "195_Actor.h"
#include "nxp_bridge.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "M225";
static const char *NVS_NS = "m225_cfg";
static const char *NVS_KEY = "state";

static bool s_clamped = false;
static char s_clamp_msg[128];

bool m225_set_was_clamped(void) { return s_clamped; }
const char *m225_set_clamp_message(void) { return s_clamp_msg; }

void m225_clamp_begin(void)
{
    s_clamped = false;
    s_clamp_msg[0] = '\0';
}

void m225_clamp_note(const char *prop, const char *applied)
{
    s_clamped = true;
    snprintf(s_clamp_msg, sizeof(s_clamp_msg), "%s clamped to %s", prop, applied);
}

void m225_nvs_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    nxp_bridge_init();

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    size_t sz = sizeof(m225_state_t);
    m225_state_t tmp;
    if (nvs_get_blob(h, NVS_KEY, &tmp, &sz) == ESP_OK && sz == sizeof(m225_state_t)) {
        m225_state_load(&tmp);
        ESP_LOGI(TAG, "loaded config from NVS");
    }
    nvs_close(h);
}

void m225_nvs_save(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    m225_state_t snap;
    m225_state_snapshot(&snap);
    nvs_set_blob(h, NVS_KEY, &snap, sizeof(snap));
    nvs_commit(h);
    nvs_close(h);
}

static bool method_ok(cJSON *ack_body, const char *actor, const char *method,
                      const char *detail)
{
    cJSON_AddStringToObject(ack_body, "act", actor);
    cJSON *mout = cJSON_CreateObject();
    if (detail) cJSON_AddStringToObject(mout, "result", detail);
    cJSON_AddItemToObject(ack_body, method, mout);
    return true;
}

bool m225_dispatch_method(const char *actor, const char *method, cJSON *params,
                          cJSON *ack_body, int mid)
{
    (void)params;
    (void)mid;
    char result[128];
    if (nxp_bridge_method(actor, method, params ? "" : NULL, result, sizeof(result))) {
        return method_ok(ack_body, actor, method, result);
    }

    if (!strcmp(actor, "ScaleCalibration")) {
        if (!strcmp(method, "Zero") || !strcmp(method, "Span") ||
            !strcmp(method, "Reset") || !strcmp(method, "Test") ||
            !strcmp(method, "CalibrateSpan") || !strcmp(method, "SpanAdjustment")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }
    if (!strcmp(actor, "IndicatorSetup")) {
        if (!strcmp(method, "ResetCounters") || !strcmp(method, "FactoryReset")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }
    if (!strcmp(actor, "ScaleSetup")) {
        if (!strcmp(method, "ApplyDefaults")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }
    if (!strcmp(actor, "PrinterSetup")) {
        if (!strcmp(method, "TestPrint")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }
    if (!strcmp(actor, "ReviewMenu")) {
        if (!strcmp(method, "Refresh")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }
    if (!strcmp(actor, "NTP") || !strcmp(actor, "SYSTEM")) {
        if (!strcmp(method, "SetTime")) {
            return method_ok(ack_body, actor, method, "ok");
        }
    }

    ESP_LOGW(TAG, "unhandled method %s.%s", actor, method);
    return false;
}
