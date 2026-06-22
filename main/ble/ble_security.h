#pragma once

#include <stdbool.h>

/* Phase 5 placeholders — bonding + YModem OTA second lane */

void ble_security_init(void);
bool ble_security_bonding_enabled(void);
bool ble_ymodem_lane_available(void);
