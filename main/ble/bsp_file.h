#pragma once
/*
 * bsp_file.h — BSP channel-2 file transfer (matches bsp_file_transfer.dart)
 */

#include <stdint.h>

void bsp_handle_file(const uint8_t *payload, uint32_t len);
