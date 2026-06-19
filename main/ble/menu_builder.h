#pragma once
/*
 * menu_builder.h — Spec §9 MENU discovery system for Cardinal A-Link (ESP32-S3)
 *
 * Each function returns a cJSON array of menu-item objects as defined
 * in the Comms Wrapper Spec v23 §9.3.  Callers must cJSON_Delete the
 * returned array when done.
 *
 * Actor mapping:
 *   Cardinal ESP32-S3 actors: BLE, WIFI, ETH, NTP, UART, SYSTEM
 *   Model 225 Indicator actors (Appendix A): IndicatorSetup, ScaleSetup,
 *     ScaleCalibration, LoadCellAssignments, ComSetup, SerialPorts,
 *     Ethernet (225), WiFi (225), ISiteIP, SendGross, PrinterSetup,
 *     SystemConfig, Accumulators, DACOutput, KeyLockout, BadgeReader,
 *     WINVRS, ModeConfig, IDStorage, DFC, Batcher, PackageWeigher,
 *     AxleWeigher, CheckWeigher, PWC, Livestock, DLCSetup, ReviewMenu
 */

#include "cJSON.h"
#include <stdbool.h>

/* ── ESP32-S3 actor menus ───────────────────────────────────────────────── */
cJSON *menu_root(void);
cJSON *menu_ble(void);
cJSON *menu_wifi(void);
cJSON *menu_ethernet(void);
cJSON *menu_ntp(void);
cJSON *menu_uart(void);
cJSON *menu_system(void);

/* ── Model 225 Indicator menus (Appendix A) ─────────────────────────────── */
cJSON *menu_195_menu(void);
cJSON *menu_indicator_setup(void);
cJSON *menu_scale_setup(void);
cJSON *menu_scale_calibration(void);
cJSON *menu_load_cell_assignments(void);
cJSON *menu_com_setup(void);
cJSON *menu_serial_ports(void);
cJSON *menu_ethernet_225(void);
cJSON *menu_wifi_225(void);
cJSON *menu_isite_ip(void);
cJSON *menu_send_gross(void);
cJSON *menu_printer_setup(void);
cJSON *menu_system_config(void);
cJSON *menu_accumulators(void);
cJSON *menu_dac_output(void);
cJSON *menu_key_lockout(void);
cJSON *menu_badge_reader(void);
cJSON *menu_winvrs(void);
cJSON *menu_mode_config(void);
cJSON *menu_id_storage(void);
cJSON *menu_dfc(void);
cJSON *menu_batcher(void);
cJSON *menu_package_weigher(void);
cJSON *menu_axle_weigher(void);
cJSON *menu_check_weigher(void);
cJSON *menu_pwc(void);
cJSON *menu_livestock(void);
cJSON *menu_dlc_setup(void);
cJSON *menu_review_menu(void);

/*
 * Dispatch helper: maps an actor name string to the appropriate handler.
 * Returns NULL if the actor is unknown.
 */
cJSON *menu_dispatch(const char *actor_name);
