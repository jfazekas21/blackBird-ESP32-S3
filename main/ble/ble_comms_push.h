#pragma once

void ble_comms_push_json(const char *json_text);
void ble_comms_push_async_err(const char *actor, int error_code, const char *reason);
void ble_comms_telemetry_start(void);
void ble_comms_telemetry_stop(void);
void ble_comms_push_scale_telemetry(const char *gross, const char *net);
