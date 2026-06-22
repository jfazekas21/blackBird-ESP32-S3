/*
 * ble_security.c — Phase 5 future: bonding/encryption + YModem OTA lane
 */

#include "ble_security.h"
#include "esp_log.h"

static const char *TAG = "BLE_SEC";

void ble_security_init(void)
{
    ESP_LOGI(TAG, "bonding/YModem lane not enabled (Phase 5 future)");
}

bool ble_security_bonding_enabled(void) { return false; }
bool ble_ymodem_lane_available(void) { return false; }
