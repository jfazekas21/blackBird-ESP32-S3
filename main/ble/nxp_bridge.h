#pragma once

#include <stdbool.h>
#include "195_Actor.h"

/* Path A: UART bridge to NXP 195/225 firmware.
 * When disabled, get/set fall back to local s_m225 store. */
void nxp_bridge_init(void);
bool nxp_bridge_enabled(void);
bool nxp_bridge_get(const char *name, char *val_out, size_t max_len);
bool nxp_bridge_set(const char *name, const char *value);
bool nxp_bridge_method(const char *actor, const char *method, const char *params_json,
                       char *result_out, size_t result_max);
