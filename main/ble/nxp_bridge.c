/*
 * nxp_bridge.c — UART bridge stub to NXP 195/225 (Path A)
 *
 * Wire protocol to NXP is TBD; this module provides the integration seam.
 * When NXP is not connected, operations delegate to the local m225 store.
 */

#include "nxp_bridge.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "NXP_BRIDGE";
static bool s_bridge_ready = false;

void nxp_bridge_init(void)
{
    /* TODO: open UART to NXP coprocessor and perform handshake */
    s_bridge_ready = false;
    ESP_LOGI(TAG, "bridge init (local fallback until NXP UART online)");
}

bool nxp_bridge_enabled(void)
{
    return s_bridge_ready;
}

bool nxp_bridge_get(const char *name, char *val_out, size_t max_len)
{
    if (s_bridge_ready) {
        /* TODO: UART request/response to NXP */
        (void)name;
        (void)val_out;
        (void)max_len;
        return false;
    }
    return m225_actor_get(name, val_out, max_len);
}

bool nxp_bridge_set(const char *name, const char *value)
{
    if (s_bridge_ready) {
        (void)name;
        (void)value;
        return false;
    }
    return m225_actor_set(name, value);
}

bool nxp_bridge_method(const char *actor, const char *method, const char *params_json,
                       char *result_out, size_t result_max)
{
    (void)params_json;
    if (result_out && result_max > 0) result_out[0] = '\0';
    if (s_bridge_ready) {
        ESP_LOGW(TAG, "NXP method %s.%s not wired", actor, method);
        return false;
    }
    if (result_out && result_max > 1) {
        snprintf(result_out, result_max, "local:%s.%s", actor, method);
    }
    return true;
}
