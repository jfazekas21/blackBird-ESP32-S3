#pragma once

#include "cJSON.h"
#include <stdbool.h>

bool m225_dispatch_method(const char *actor, const char *method, cJSON *params,
                          cJSON *ack_body, int mid);

bool m225_set_was_clamped(void);
const char *m225_set_clamp_message(void);
void m225_clamp_begin(void);
void m225_clamp_note(const char *prop, const char *applied);
void m225_nvs_init(void);
void m225_nvs_save(void);
