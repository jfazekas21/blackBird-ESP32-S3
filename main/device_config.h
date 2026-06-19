/*
 * device_config.h
 *
 * Single source of truth for company branding and product identity.
 * Change DEVICE_COMPANY here to rebrand the entire firmware.
 *
 * BLE budget (verified against BLE_Actor.c advertisement builder):
 *   Primary packet  : Flags(3) + ShortName(8) + MfgData(14) = 25 / 31 bytes
 *   Scan response   : Length(1) + Type(1) + FullName = 2 + name_len / 31 bytes
 *   Spec §2 limit   : name ≤ 16 chars  →  scan response ≤ 18 bytes
 *   BLE hard limit  : name ≤ 29 chars  →  scan response ≤ 31 bytes
 */

#pragma once

/* ── Branding ──────────────────────────────────────────────────────────────── */
#define DEVICE_COMPANY   "Cardinal"
#define DEVICE_MODEL     "A-Link"
#define DEVICE_FULL_NAME DEVICE_COMPANY " " DEVICE_MODEL   /* "Cardinal A-Link" = 15 chars */

/* Compile-time guard: changing DEVICE_COMPANY to something long will fail here
   rather than silently overflowing the BLE scan response packet. */
_Static_assert(sizeof(DEVICE_FULL_NAME) - 1 <= 16,
    "DEVICE_FULL_NAME exceeds spec §2 limit of 16 chars — BLE scan response will overflow");

/* ── Product identity ──────────────────────────────────────────────────────── */
#define PRODUCT_ID_SB600      1
#define PRODUCT_ID_MODEL_195  2
#define PRODUCT_ID_MODEL_225  3

/* This device is a Model 195 (A-Link / B480).
   Previously misidentified as 0x03 (Model 225). */
#define DEVICE_PRODUCT_ID     PRODUCT_ID_MODEL_195
