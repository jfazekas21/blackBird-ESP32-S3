/*
 * 195_Actor.c — Model 225 Indicator actor (dummy variable store)
 *
 * Maintains an in-RAM copy of every configuration variable defined in
 * Appendix A of the Socket Comms Wrapper Spec v23.  All values are
 * initialised to the spec defaults.  The actor accepts GET / SET commands
 * via the standard console queue so the BLE app can read and write variables
 * directly in addition to browsing them through the MENU system.
 *
 * m225_actor_get() is the getter callback consumed by menu_builder.c.
 */

#include "195_Actor.h"
#include "m225_methods.h"
#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "cJSON.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"

/* ─── Actor identity ─────────────────────────────────────────────────────── */
static const char *THIS_ACTOR    __attribute__((unused)) = "Model225";
static const char  THIS_ACTOR_ID __attribute__((unused)) = MODEL_225;

/* ─── Task / queue resources ─────────────────────────────────────────────── */
#define M225_QUEUE_LENGTH   5
#define M225_TASK_STACK     (8 * 1024)
#define M225_TASK_PRIORITY  2

static QueueHandle_t    s_rx_queue        = NULL;
static StaticQueue_t    s_queue_buf;
static StaticTask_t     s_task_buf;

PSRAM_ATTR_BSS static uint8_t     s_queue_storage[M225_QUEUE_LENGTH * sizeof(AMessage_st)];
PSRAM_ATTR_BSS static StackType_t s_task_stack[M225_TASK_STACK / sizeof(StackType_t)];
PSRAM_ATTR_BSS static char        s_rx_buf[MAX_JSON_PAYLOAD_BYTES / 2];
PSRAM_ATTR_BSS static char        s_payload_buf[MAX_JSON_PAYLOAD_BYTES / 2];

static bool s_initialised = false;

/* ─── Actor context for property disambiguation ──────────────────────────── */
static const char *s_actor_ctx = "";
void m225_set_menu_ctx(const char *ctx) { s_actor_ctx = ctx ? ctx : ""; }

/* ─── State instance — spec Appendix A defaults ──────────────────────────── */
static m225_state_t s_m225 = {
    /* IndicatorSetup */
    .usa                  = true,
    .lft                  = false,
    .oiml                 = false,
    .time_format          = "12H",
    .date_format          = "MM-DD-YYYY",
    .consecutive_number   = 1,
    .clear_tare           = false,
    .clear_id             = false,
    .number_of_scales     = 1,
    .totalizer            = false,
    .mode_of_operation    = "Normal",

    /* ScaleSetup */
    .primary_units        = "lb",
    .secondary_units      = "kg",
    .zero_tracking        = "0.0d",
    .zero_limit           = false,
    .power_up_zero        = false,
    .sample_rate          = 4,
    .motion_range         = 3,
    .stable_count         = 3,
    .weight_intervals     = 1,
    .scale_type           = "Analog",
    .filter_type          = "None",
    .filter_level         = 2,
    .filter_break         = 1,
    .interval             = 10,
    .decimal_place        = 0,
    .capacity             = 50000,
    .low_interval         = 10,
    .low_decimal_place    = 0,
    .low_capacity         = 50000,
    .high_interval        = 10,
    .high_decimal_place   = 0,
    .high_capacity        = 100000,
    .prelim_filter_count  = 0,

    /* ScaleCalibration */
    .span_weight          = 0.0f,
    .span_count           = 0,
    .zero_count           = 0,
    .lc1 = 1000, .lc2 = 1000, .lc3 = 1000, .lc4 = 1000,
    .cell_trimming        = 1.0f,

    /* LoadCellAssignments */
    .cell_id              = 0,
    .cell_to_scale        = "Scale1",
    .cells_per_scale      = 0,
    .cell_trim            = 1.0f,

    /* ComSetup — SerialPorts */
    .serial_type          = "SMA",
    .baud_rate            = "9600",
    .data_bits            = "8",
    .stop_bits            = "1",
    .parity               = "None",
    .transfer_condition   = "Continuous",
    .serial_scale         = "CurrentSelected",
    .gross_only           = false,
    .manual_mode          = false,
    .message_slot         = "Disabled",
    .electrical_interface = "RS232",
    .sb_high_threshold    = 1500,

    /* ComSetup — Ethernet */
    .ethernet_enable      = false,
    .eth_dhcp             = true,
    .eth_ip               = "192.168.4.138",
    .eth_gateway          = "192.168.4.1",
    .eth_subnet           = "255.255.255.0",
    .server_port_a        = 10010,
    .server_port_b        = 10011,
    .server_port_c        = 10012,
    .client_server_port   = 10010,
    .eth_port_type        = "SB600",
    .eth_port_threshold   = 1500,
    .eth_message_slot     = "Disabled",

    /* ComSetup — WiFi */
    .wifi_enable          = true,
    .wifi_dhcp            = true,
    .wifi_ssid            = "195",
    .wifi_password        = "12345678",
    .wifi_port_type       = "SB600",
    .wifi_port_threshold  = 1500,
    .wifi_message_slot    = "Disabled",

    /* ComSetup — ISiteIP */
    .site_order           = "",
    .isite_dhcp           = true,
    .isite_ip             = "192.168.1.70",
    .isite_subnet         = "255.255.255.0",
    .isite_gateway        = "192.168.1.1",
    .dns1                 = "8.8.8.8",
    .dns2                 = "8.8.4.4",
    .enable_dns           = true,

    /* ComSetup — SendGross */
    .send_gross_enable    = false,
    .gross_weight_port    = "Com1",

    /* PrinterSetup */
    .printer_port         = "Com1",
    .auto_lf              = false,
    .ending_lf            = 0,
    .end_of_line          = "0D",
    .start_of_ticket      = "",
    .end_of_ticket        = "",
    .end_of_ticket_lf     = 0,
    .print_slot           = 0,
    .time_tab             = 1.0f,
    .date_tab             = 2.0f,
    .consecutive_tab      = 6.0f,
    .gross_tab            = 7.0f,
    .tare_tab             = 8.0f,
    .net_tab              = 9.0f,
    .gross_accum_tab      = 0.0f,
    .net_accum_tab        = 0.0f,
    .id_tab               = 3.05f,

    /* SystemConfig — Accumulators */
    .gen_accums           = true,
    .accumulator_scale1   = 0.0f,
    .accumulator_scale2   = 0.0f,
    .accumulator_scale3   = 0.0f,
    .totalizer_accumulator = 0.0f,
    .password             = "",
    .lr_port              = "Com1",

    /* SystemConfig — DACOutput */
    .dac_gross            = true,
    .dac_low_weight       = 0.0f,
    .dac_high_weight      = 50000.0f,
    .dac_volt_output      = 10.0f,
    .adjust_high          = 0,
    .adjust_low           = 0,
    .dac_scale            = "Scale1",

    /* SystemConfig — KeyLockout (all unlocked) */
    .zero_key_lock    = false, .tare_key_lock    = false,
    .net_key_lock     = false, .print_key_lock   = false,
    .unit_key_lock    = false, .green_key_lock   = false,
    .keypad_lock      = false, .id_key_lock      = false,
    .count_key_lock   = false, .mem_key_lock     = false,
    .preset_key_lock  = false, .delete_key_lock  = false,
    .start_key_lock   = false, .drop_key_lock    = false,
    .pause_key_lock   = false, .stop_key_lock    = false,
    .restart_key_lock = false, .dump_key_lock    = false,

    /* SystemConfig — BadgeReader */
    .reader1_port         = "Com1",
    .reader1_type         = "None",
    .reader2_port         = "Com1",
    .reader2_type         = "None",
    .threshold_weight     = 0.0f,
    .site_id              = false,

    /* SystemConfig — WINVRS */
    .computer1_port       = "Com1",
    .computer1_mode       = "Disabled",
    .computer2_port       = "Com1",
    .computer2_mode       = "Disabled",
    .print_passthrough    = false,
    .traffic_mode         = "Off",
    .traffic_on_threshold = 0.0f,
    .traffic_off_threshold= 0.0f,
    .traffic_off_delay    = 1,
    .enter_relay_command  = "3!4",
    .exit_relay_command   = "1!2",
    .traffic_display      = false,
    .winvrs_dfc_enable    = false,

    /* ModeConfig — IDStorage */
    .weight_alarm         = false,
    .alarm_threshold      = 1000.0f,
    .alarm_time_on        = 99,
    .id_count             = 1,
    .prompt1              = "ID",
    .prompt2              = "ID2",
    .prompt3              = "ID3",

    /* ModeConfig — DFC */
    .dfc_speed            = "SingleSpeed",
    .gate_sequence        = "ABB",
    .auto_trim            = false,
    .dfc_auto_print       = false,
    .multi_drop           = false,
    .dump_gate            = false,
    .auto_dump            = false,
    .decumulate           = false,
    .auto_tare            = false,
    .jog_to_cutoff        = false,
    .fast_cutoff          = 0.0f,
    .fill_weight          = 0.0f,
    .slow_cutoff          = 0.0f,
    .dfc_trim             = 0.0f,
    .drop_count           = 0,
    .zero_tolerance       = 0.0f,
    .gate_timer           = 0,
    .chatter              = 0.0f,

    /* ModeConfig — Batcher */
    .batcher_speed        = "SingleSpeed",
    .batcher_gate_seq     = "ABB",
    .batcher_auto_trim    = false,
    .batcher_auto_print   = false,
    .batcher_dump_gate    = false,
    .batcher_auto_dump    = false,
    .batcher_decumulate   = false,
    .bin_count            = 4,
    .batch_count          = 0,
    .batcher_zero_tolerance = 0.0f,
    .batcher_gate_timer   = 0,
    .settle_timer         = 0,

    /* ModeConfig — PackageWeigher */
    .pkg_id_count         = 1,
    .pkg_prompt1          = "ID",
    .pkg_prompt2          = "ID2",
    .pkg_prompt3          = "ID3",
    .retain_id            = false,

    /* ModeConfig — AxleWeigher */
    .aw_mode              = "Auto",
    .axle_pads            = false,
    .axle_threshold       = 500.0f,
    .stop_delay           = 0,
    .total_delay          = 0,
    .axle_counter         = 1,

    /* ModeConfig — CheckWeigher */
    .cw_outputs           = 3,
    .cw_auto_print        = false,
    .under_threshold      = 500.0f,
    .low_ok_threshold     = 1000.0f,
    .high_ok_threshold    = 1500.0f,
    .over_threshold       = 2000.0f,
    .color_under          = "Yellow",
    .color_low_ok         = "Pink",
    .color_high_ok        = "Blue",
    .color_accept_ok      = "Green",
    .color_over           = "Red",

    /* ModeConfig — PWC */
    .number_of_pwc_outputs = 1,
    .balance_on_print     = false,
    .monitor_zero         = false,
    .pwc_threshold        = 0.0f,
    .pwc_trim             = 0.0f,
    .output_scale         = "Scale1",

    /* ModeConfig — Livestock */
    .inclinometer         = false,
    .set_pitch            = 0.0f,
    .set_roll             = 0.0f,

    /* DLCSetup */
    .snap_mediabox        = false,
    .snap_id              = 0,
    .snap_channel         = 15,

    /* ReviewMenu */
    .scale_id             = 0,
    .calibration_counter  = 0,
    .configuration_counter = 0,
};

/* ─── Setter wrapper — matches BLE_Actor.c bool (*as)(name, value) ──────── */

static void m225_set_property(const char *name, const char *value,
                               AMessage_st *msg);

void m225_state_snapshot(m225_state_t *out)
{
    if (out) *out = s_m225;
}

void m225_state_load(const m225_state_t *in)
{
    if (in) s_m225 = *in;
}

bool m225_actor_set(const char *name, const char *value)
{
    if (!name || !value) return false;
    m225_clamp_begin();
    m225_set_property(name, value, NULL);
    m225_nvs_save();
    return true;
}

/* ─── Getter — used by menu_builder.c ───────────────────────────────────── */

#define G_BOOL(field)   snprintf(val_out, max_len, "%s", s_m225.field ? "true" : "false"); return true
#define G_U8(field)     snprintf(val_out, max_len, "%u",  (unsigned)s_m225.field);          return true
#define G_U16(field)    snprintf(val_out, max_len, "%u",  (unsigned)s_m225.field);          return true
#define G_U32(field)    snprintf(val_out, max_len, "%lu", (unsigned long)s_m225.field);     return true
#define G_I32(field)    snprintf(val_out, max_len, "%ld", (long)s_m225.field);              return true
#define G_FLOAT(field)  snprintf(val_out, max_len, "%g",  (double)s_m225.field);            return true
#define G_STR(field)    snprintf(val_out, max_len, "%s",  s_m225.field);                    return true

bool m225_actor_get(const char *name, char *val_out, size_t max_len)
{
    if (!name || !val_out || max_len == 0) return false;
    val_out[0] = '\0';
    const char *ctx = s_actor_ctx;

    /* ── Context-sensitive disambiguation ─────────────────────────────────── */

    if (!strcmp(ctx, "Ethernet")) {
        if (!strcmp(name, "DHCP"))          { G_BOOL(eth_dhcp); }
        if (!strcmp(name, "IPAddress"))     { G_STR(eth_ip); }
        if (!strcmp(name, "Gateway"))       { G_STR(eth_gateway); }
        if (!strcmp(name, "Subnet"))        { G_STR(eth_subnet); }
        if (!strcmp(name, "PortType"))      { G_STR(eth_port_type); }
        if (!strcmp(name, "PortThreshold")) { G_U32(eth_port_threshold); }
        if (!strcmp(name, "MessageSlot"))   { G_STR(eth_message_slot); }
    }
    if (!strcmp(ctx, "WiFi")) {
        if (!strcmp(name, "DHCP"))          { G_BOOL(wifi_dhcp); }
        if (!strcmp(name, "PortType"))      { G_STR(wifi_port_type); }
        if (!strcmp(name, "PortThreshold")) { G_U32(wifi_port_threshold); }
        if (!strcmp(name, "MessageSlot"))   { G_STR(wifi_message_slot); }
        if (!strcmp(name, "Password"))      { G_STR(wifi_password); }
    }
    if (!strcmp(ctx, "SerialPorts")) {
        if (!strcmp(name, "MessageSlot"))   { G_STR(message_slot); }
    }
    if (!strcmp(ctx, "ISiteIP")) {
        if (!strcmp(name, "DHCP"))          { G_BOOL(isite_dhcp); }
        if (!strcmp(name, "IPAddress"))     { G_STR(isite_ip); }
        if (!strcmp(name, "Subnet"))        { G_STR(isite_subnet); }
        if (!strcmp(name, "Gateway"))       { G_STR(isite_gateway); }
    }
    if (!strcmp(ctx, "Accumulators")) {
        if (!strcmp(name, "Password"))      { G_STR(password); }
    }
    if (!strcmp(ctx, "BadgeReader")) {
        if (!strcmp(name, "ThresholdWeight")) { G_FLOAT(threshold_weight); }
    }
    if (!strcmp(ctx, "AxleWeigher")) {
        if (!strcmp(name, "ThresholdWeight")) { G_FLOAT(axle_threshold); }
    }
    if (!strcmp(ctx, "IDStorage")) {
        if (!strcmp(name, "IDCount"))       { G_U8(id_count); }
        if (!strcmp(name, "Prompt1"))       { G_STR(prompt1); }
        if (!strcmp(name, "Prompt2"))       { G_STR(prompt2); }
        if (!strcmp(name, "Prompt3"))       { G_STR(prompt3); }
    }
    if (!strcmp(ctx, "PackageWeigher")) {
        if (!strcmp(name, "IDCount"))       { G_U8(pkg_id_count); }
        if (!strcmp(name, "Prompt1"))       { G_STR(pkg_prompt1); }
        if (!strcmp(name, "Prompt2"))       { G_STR(pkg_prompt2); }
        if (!strcmp(name, "Prompt3"))       { G_STR(pkg_prompt3); }
    }
    if (!strcmp(ctx, "DFC")) {
        if (!strcmp(name, "Speed"))         { G_STR(dfc_speed); }
        if (!strcmp(name, "GateSequence"))  { G_STR(gate_sequence); }
        if (!strcmp(name, "AutoTrim"))      { G_BOOL(auto_trim); }
        if (!strcmp(name, "AutoPrint"))     { G_BOOL(dfc_auto_print); }
        if (!strcmp(name, "DumpGate"))      { G_BOOL(dump_gate); }
        if (!strcmp(name, "AutoDump"))      { G_BOOL(auto_dump); }
        if (!strcmp(name, "Decumulate"))    { G_BOOL(decumulate); }
        if (!strcmp(name, "ZeroTolerance")) { G_FLOAT(zero_tolerance); }
        if (!strcmp(name, "GateTimer"))     { G_U16(gate_timer); }
        if (!strcmp(name, "Trim"))          { G_FLOAT(dfc_trim); }
    }
    if (!strcmp(ctx, "Batcher")) {
        if (!strcmp(name, "Speed"))         { G_STR(batcher_speed); }
        if (!strcmp(name, "GateSequence"))  { G_STR(batcher_gate_seq); }
        if (!strcmp(name, "AutoTrim"))      { G_BOOL(batcher_auto_trim); }
        if (!strcmp(name, "AutoPrint"))     { G_BOOL(batcher_auto_print); }
        if (!strcmp(name, "DumpGate"))      { G_BOOL(batcher_dump_gate); }
        if (!strcmp(name, "AutoDump"))      { G_BOOL(batcher_auto_dump); }
        if (!strcmp(name, "Decumulate"))    { G_BOOL(batcher_decumulate); }
        if (!strcmp(name, "ZeroTolerance")) { G_FLOAT(batcher_zero_tolerance); }
        if (!strcmp(name, "GateTimer"))     { G_U16(batcher_gate_timer); }
    }
    if (!strcmp(ctx, "CheckWeigher")) {
        if (!strcmp(name, "AutoPrint"))     { G_BOOL(cw_auto_print); }
    }
    if (!strcmp(ctx, "PWC")) {
        if (!strcmp(name, "Trim"))          { G_FLOAT(pwc_trim); }
    }

    /* ── Flat (unambiguous) lookup ────────────────────────────────────────── */

    /* IndicatorSetup */
    if (!strcmp(name, "USA"))                { G_BOOL(usa); }
    if (!strcmp(name, "LFT"))                { G_BOOL(lft); }
    if (!strcmp(name, "OIML"))               { G_BOOL(oiml); }
    if (!strcmp(name, "TimeFormat"))         { G_STR(time_format); }
    if (!strcmp(name, "DateFormat"))         { G_STR(date_format); }
    if (!strcmp(name, "ConsecutiveNumber"))  { G_U32(consecutive_number); }
    if (!strcmp(name, "ClearTare"))          { G_BOOL(clear_tare); }
    if (!strcmp(name, "ClearID"))            { G_BOOL(clear_id); }
    if (!strcmp(name, "NumberOfScales"))     { G_U8(number_of_scales); }
    if (!strcmp(name, "Totalizer"))          { G_BOOL(totalizer); }
    if (!strcmp(name, "ModeOfOperation"))    { G_STR(mode_of_operation); }

    /* ScaleSetup */
    if (!strcmp(name, "PrimaryUnits"))       { G_STR(primary_units); }
    if (!strcmp(name, "SecondaryUnits"))     { G_STR(secondary_units); }
    if (!strcmp(name, "ZeroTracking"))       { G_STR(zero_tracking); }
    if (!strcmp(name, "ZeroLimit"))          { G_BOOL(zero_limit); }
    if (!strcmp(name, "PowerUpZero"))        { G_BOOL(power_up_zero); }
    if (!strcmp(name, "SampleRate"))         { G_U8(sample_rate); }
    if (!strcmp(name, "MotionRange"))        { G_U8(motion_range); }
    if (!strcmp(name, "StableCount"))        { G_U8(stable_count); }
    if (!strcmp(name, "WeightIntervals"))    { G_U8(weight_intervals); }
    if (!strcmp(name, "ScaleType"))          { G_STR(scale_type); }
    if (!strcmp(name, "FilterType"))         { G_STR(filter_type); }
    if (!strcmp(name, "FilterLevel"))        { G_U8(filter_level); }
    if (!strcmp(name, "FilterBreak"))        { G_U8(filter_break); }
    if (!strcmp(name, "Interval"))           { G_U32(interval); }
    if (!strcmp(name, "DecimalPlace"))       { G_U8(decimal_place); }
    if (!strcmp(name, "Capacity"))           { G_U32(capacity); }
    if (!strcmp(name, "LowInterval"))        { G_U32(low_interval); }
    if (!strcmp(name, "LowDecimalPlace"))    { G_U8(low_decimal_place); }
    if (!strcmp(name, "LowCapacity"))        { G_U32(low_capacity); }
    if (!strcmp(name, "HighInterval"))       { G_U32(high_interval); }
    if (!strcmp(name, "HighDecimalPlace"))   { G_U8(high_decimal_place); }
    if (!strcmp(name, "HighCapacity"))       { G_U32(high_capacity); }
    if (!strcmp(name, "PrelimFilterCount"))  { G_U8(prelim_filter_count); }

    /* ScaleCalibration */
    if (!strcmp(name, "SpanWeight"))         { G_FLOAT(span_weight); }
    if (!strcmp(name, "SpanCount"))          { G_U32(span_count); }
    if (!strcmp(name, "ZeroCount"))          { G_U32(zero_count); }
    if (!strcmp(name, "LC1"))                { G_U32(lc1); }
    if (!strcmp(name, "LC2"))                { G_U32(lc2); }
    if (!strcmp(name, "LC3"))                { G_U32(lc3); }
    if (!strcmp(name, "LC4"))                { G_U32(lc4); }
    if (!strcmp(name, "CellTrimming"))       { G_FLOAT(cell_trimming); }
    if (!strcmp(name, "NSC"))                { snprintf(val_out, max_len, "N/A"); return true; }

    /* LoadCellAssignments */
    if (!strcmp(name, "CellID"))             { G_U8(cell_id); }
    if (!strcmp(name, "CellToScale"))        { G_STR(cell_to_scale); }
    if (!strcmp(name, "CellsPerScale"))      { G_U8(cells_per_scale); }
    if (!strcmp(name, "CellTrim"))           { G_FLOAT(cell_trim); }

    /* ComSetup — SerialPorts (non-ambiguous) */
    if (!strcmp(name, "Type"))               { G_STR(serial_type); }
    if (!strcmp(name, "BaudRate"))           { G_STR(baud_rate); }
    if (!strcmp(name, "DataBits"))           { G_STR(data_bits); }
    if (!strcmp(name, "StopBits"))           { G_STR(stop_bits); }
    if (!strcmp(name, "Parity"))             { G_STR(parity); }
    if (!strcmp(name, "TransferCondition"))  { G_STR(transfer_condition); }
    if (!strcmp(name, "Scale"))              { G_STR(serial_scale); }
    if (!strcmp(name, "GrossOnly"))          { G_BOOL(gross_only); }
    if (!strcmp(name, "ManualMode"))         { G_BOOL(manual_mode); }
    if (!strcmp(name, "ElectricalInterface")){ G_STR(electrical_interface); }
    if (!strcmp(name, "SBHighThreshold"))    { G_U32(sb_high_threshold); }

    /* ComSetup — Ethernet (non-ambiguous) */
    if (!strcmp(name, "EthernetEnable"))     { G_BOOL(ethernet_enable); }
    if (!strcmp(name, "ServerPortA"))        { G_U16(server_port_a); }
    if (!strcmp(name, "ServerPortB"))        { G_U16(server_port_b); }
    if (!strcmp(name, "ServerPortC"))        { G_U16(server_port_c); }
    if (!strcmp(name, "ClientServerPort"))   { G_U16(client_server_port); }

    /* ComSetup — WiFi (non-ambiguous) */
    if (!strcmp(name, "WiFiEnable"))         { G_BOOL(wifi_enable); }
    if (!strcmp(name, "SSID"))               { G_STR(wifi_ssid); }

    /* ComSetup — ISiteIP (non-ambiguous) */
    if (!strcmp(name, "SiteOrder"))          { G_STR(site_order); }
    if (!strcmp(name, "DNS1"))               { G_STR(dns1); }
    if (!strcmp(name, "DNS2"))               { G_STR(dns2); }
    if (!strcmp(name, "EnableDNS"))          { G_BOOL(enable_dns); }

    /* ComSetup — SendGross */
    if (!strcmp(name, "SendGrossEnable"))    { G_BOOL(send_gross_enable); }
    if (!strcmp(name, "GrossWeightPort"))    { G_STR(gross_weight_port); }

    /* PrinterSetup */
    if (!strcmp(name, "Port"))               { G_STR(printer_port); }
    if (!strcmp(name, "AutoLF"))             { G_BOOL(auto_lf); }
    if (!strcmp(name, "EndingLF"))           { G_U8(ending_lf); }
    if (!strcmp(name, "EndOfLine"))          { G_STR(end_of_line); }
    if (!strcmp(name, "StartOfTicket"))      { G_STR(start_of_ticket); }
    if (!strcmp(name, "EndOfTicket"))        { G_STR(end_of_ticket); }
    if (!strcmp(name, "EndOfTicketLineFeeds")){ G_U8(end_of_ticket_lf); }
    if (!strcmp(name, "PrintSlot"))          { G_U8(print_slot); }
    if (!strcmp(name, "TimeTab"))            { G_FLOAT(time_tab); }
    if (!strcmp(name, "DateTab"))            { G_FLOAT(date_tab); }
    if (!strcmp(name, "ConsecutiveTab"))     { G_FLOAT(consecutive_tab); }
    if (!strcmp(name, "GrossTab"))           { G_FLOAT(gross_tab); }
    if (!strcmp(name, "TareTab"))            { G_FLOAT(tare_tab); }
    if (!strcmp(name, "NetTab"))             { G_FLOAT(net_tab); }
    if (!strcmp(name, "GrossAccumTab"))      { G_FLOAT(gross_accum_tab); }
    if (!strcmp(name, "NetAccumTab"))        { G_FLOAT(net_accum_tab); }
    if (!strcmp(name, "IDTab"))              { G_FLOAT(id_tab); }

    /* SystemConfig — Accumulators (non-ambiguous) */
    if (!strcmp(name, "GenAccums"))          { G_BOOL(gen_accums); }
    if (!strcmp(name, "AccumulatorScale1"))  { G_FLOAT(accumulator_scale1); }
    if (!strcmp(name, "AccumulatorScale2"))  { G_FLOAT(accumulator_scale2); }
    if (!strcmp(name, "AccumulatorScale3"))  { G_FLOAT(accumulator_scale3); }
    if (!strcmp(name, "TotalizerAccumulator")){ G_FLOAT(totalizer_accumulator); }
    if (!strcmp(name, "LRPort"))             { G_STR(lr_port); }

    /* SystemConfig — DACOutput */
    if (!strcmp(name, "DACGross"))           { G_BOOL(dac_gross); }
    if (!strcmp(name, "DACLowWeight"))       { G_FLOAT(dac_low_weight); }
    if (!strcmp(name, "DACHighWeight"))      { G_FLOAT(dac_high_weight); }
    if (!strcmp(name, "DACVoltOutput"))      { G_FLOAT(dac_volt_output); }
    if (!strcmp(name, "AdjustHigh"))         { G_I32(adjust_high); }
    if (!strcmp(name, "AdjustLow"))          { G_I32(adjust_low); }
    if (!strcmp(name, "DACScale"))           { G_STR(dac_scale); }

    /* SystemConfig — KeyLockout */
    if (!strcmp(name, "ZeroKeyLock"))        { G_BOOL(zero_key_lock); }
    if (!strcmp(name, "TareKeyLock"))        { G_BOOL(tare_key_lock); }
    if (!strcmp(name, "NetKeyLock"))         { G_BOOL(net_key_lock); }
    if (!strcmp(name, "PrintKeyLock"))       { G_BOOL(print_key_lock); }
    if (!strcmp(name, "UnitKeyLock"))        { G_BOOL(unit_key_lock); }
    if (!strcmp(name, "GreenKeyLock"))       { G_BOOL(green_key_lock); }
    if (!strcmp(name, "KeypadLock"))         { G_BOOL(keypad_lock); }
    if (!strcmp(name, "IDKeyLock"))          { G_BOOL(id_key_lock); }
    if (!strcmp(name, "CountKeyLock"))       { G_BOOL(count_key_lock); }
    if (!strcmp(name, "MemKeyLock"))         { G_BOOL(mem_key_lock); }
    if (!strcmp(name, "PresetKeyLock"))      { G_BOOL(preset_key_lock); }
    if (!strcmp(name, "DeleteKeyLock"))      { G_BOOL(delete_key_lock); }
    if (!strcmp(name, "StartKeyLock"))       { G_BOOL(start_key_lock); }
    if (!strcmp(name, "DropKeyLock"))        { G_BOOL(drop_key_lock); }
    if (!strcmp(name, "PauseKeyLock"))       { G_BOOL(pause_key_lock); }
    if (!strcmp(name, "StopKeyLock"))        { G_BOOL(stop_key_lock); }
    if (!strcmp(name, "RestartKeyLock"))     { G_BOOL(restart_key_lock); }
    if (!strcmp(name, "DumpKeyLock"))        { G_BOOL(dump_key_lock); }

    /* SystemConfig — BadgeReader (non-ambiguous) */
    if (!strcmp(name, "Reader1Port"))        { G_STR(reader1_port); }
    if (!strcmp(name, "Reader1Type"))        { G_STR(reader1_type); }
    if (!strcmp(name, "Reader2Port"))        { G_STR(reader2_port); }
    if (!strcmp(name, "Reader2Type"))        { G_STR(reader2_type); }
    if (!strcmp(name, "SiteID"))             { G_BOOL(site_id); }

    /* SystemConfig — WINVRS */
    if (!strcmp(name, "Computer1Port"))      { G_STR(computer1_port); }
    if (!strcmp(name, "Computer1Mode"))      { G_STR(computer1_mode); }
    if (!strcmp(name, "Computer2Port"))      { G_STR(computer2_port); }
    if (!strcmp(name, "Computer2Mode"))      { G_STR(computer2_mode); }
    if (!strcmp(name, "PrintPassthrough"))   { G_BOOL(print_passthrough); }
    if (!strcmp(name, "TrafficMode"))        { G_STR(traffic_mode); }
    if (!strcmp(name, "TrafficOnThreshold")) { G_FLOAT(traffic_on_threshold); }
    if (!strcmp(name, "TrafficOffThreshold")){ G_FLOAT(traffic_off_threshold); }
    if (!strcmp(name, "TrafficOffDelay"))    { G_U8(traffic_off_delay); }
    if (!strcmp(name, "EnterRelayCommand"))  { G_STR(enter_relay_command); }
    if (!strcmp(name, "ExitRelayCommand"))   { G_STR(exit_relay_command); }
    if (!strcmp(name, "TrafficDisplay"))     { G_BOOL(traffic_display); }
    if (!strcmp(name, "DFCEnable"))          { G_BOOL(winvrs_dfc_enable); }

    /* ModeConfig — IDStorage (non-ambiguous) */
    if (!strcmp(name, "WeightAlarm"))        { G_BOOL(weight_alarm); }
    if (!strcmp(name, "AlarmThreshold"))     { G_FLOAT(alarm_threshold); }
    if (!strcmp(name, "AlarmTimeOn"))        { G_U8(alarm_time_on); }

    /* ModeConfig — DFC (non-ambiguous) */
    if (!strcmp(name, "MultiDrop"))          { G_BOOL(multi_drop); }
    if (!strcmp(name, "AutoTare"))           { G_BOOL(auto_tare); }
    if (!strcmp(name, "JogToCutoff"))        { G_BOOL(jog_to_cutoff); }
    if (!strcmp(name, "FastCutoff"))         { G_FLOAT(fast_cutoff); }
    if (!strcmp(name, "FillWeight"))         { G_FLOAT(fill_weight); }
    if (!strcmp(name, "SlowCutoff"))         { G_FLOAT(slow_cutoff); }
    if (!strcmp(name, "DropCount"))          { G_U8(drop_count); }
    if (!strcmp(name, "Chatter"))            { G_FLOAT(chatter); }

    /* ModeConfig — Batcher (non-ambiguous) */
    if (!strcmp(name, "BinCount"))           { G_U8(bin_count); }
    if (!strcmp(name, "BatchCount"))         { G_U32(batch_count); }
    if (!strcmp(name, "SettleTimer"))        { G_U16(settle_timer); }

    /* ModeConfig — PackageWeigher (non-ambiguous) */
    if (!strcmp(name, "RetainID"))           { G_BOOL(retain_id); }

    /* ModeConfig — AxleWeigher (non-ambiguous) */
    if (!strcmp(name, "AWMode"))             { G_STR(aw_mode); }
    if (!strcmp(name, "AxlePads"))           { G_BOOL(axle_pads); }
    if (!strcmp(name, "StopDelay"))          { G_U16(stop_delay); }
    if (!strcmp(name, "TotalDelay"))         { G_U16(total_delay); }
    if (!strcmp(name, "AxleCounter"))        { G_U32(axle_counter); }

    /* ModeConfig — CheckWeigher (non-ambiguous) */
    if (!strcmp(name, "Outputs"))            { G_U8(cw_outputs); }
    if (!strcmp(name, "UnderThreshold"))     { G_FLOAT(under_threshold); }
    if (!strcmp(name, "LowOKThreshold"))     { G_FLOAT(low_ok_threshold); }
    if (!strcmp(name, "HighOKThreshold"))    { G_FLOAT(high_ok_threshold); }
    if (!strcmp(name, "OverThreshold"))      { G_FLOAT(over_threshold); }
    if (!strcmp(name, "ColorUnder"))         { G_STR(color_under); }
    if (!strcmp(name, "ColorLowOK"))         { G_STR(color_low_ok); }
    if (!strcmp(name, "ColorHighOK"))        { G_STR(color_high_ok); }
    if (!strcmp(name, "ColorAcceptOK"))      { G_STR(color_accept_ok); }
    if (!strcmp(name, "ColorOver"))          { G_STR(color_over); }

    /* ModeConfig — PWC (non-ambiguous) */
    if (!strcmp(name, "NumberOfOutputs"))    { G_U8(number_of_pwc_outputs); }
    if (!strcmp(name, "BalanceOnPrint"))     { G_BOOL(balance_on_print); }
    if (!strcmp(name, "MonitorZero"))        { G_BOOL(monitor_zero); }
    if (!strcmp(name, "Threshold"))          { G_FLOAT(pwc_threshold); }
    if (!strcmp(name, "OutputScale"))        { G_STR(output_scale); }

    /* ModeConfig — Livestock */
    if (!strcmp(name, "Inclinometer"))       { G_BOOL(inclinometer); }
    if (!strcmp(name, "SetPitch"))           { G_FLOAT(set_pitch); }
    if (!strcmp(name, "SetRoll"))            { G_FLOAT(set_roll); }

    /* DLCSetup */
    if (!strcmp(name, "SnapMediabox"))       { G_BOOL(snap_mediabox); }
    if (!strcmp(name, "SnapID"))             { G_U8(snap_id); }
    if (!strcmp(name, "Channel"))            { G_U8(snap_channel); }

    /* ReviewMenu */
    if (!strcmp(name, "ScaleID"))            { G_U8(scale_id); }
    if (!strcmp(name, "CalibrationCounter")) { G_U32(calibration_counter); }
    if (!strcmp(name, "ConfigurationCounter")){ G_U32(configuration_counter); }

    return false; /* unknown property */
}

/* ─── Actor SET — write a named field back into s_m225 ───────────────────── */

static void m225_set_property(const char *name, const char *value,
                               AMessage_st *msg)
{
    (void)msg;
    const char *ctx = s_actor_ctx;
    bool bval = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);

#define S_STR(field)  snprintf(s_m225.field, sizeof(s_m225.field), "%s", value)
#define S_BOOL(field) s_m225.field = bval
#define S_U8(field)   s_m225.field = (uint8_t)atoi(value)
#define S_U16(field)  s_m225.field = (uint16_t)atoi(value)
#define S_U32(field)  s_m225.field = (uint32_t)atol(value)
#define S_I32(field)  s_m225.field = (int32_t)atol(value)
#define S_FLT(field)  s_m225.field = (float)atof(value)

#define S_U8_CLAMP(field, minv, maxv) do { \
    int _cv = atoi(value); \
    char _cap[16]; \
    if (_cv < (minv)) { _cv = (minv); snprintf(_cap, sizeof(_cap), "%d", (minv)); m225_clamp_note(name, _cap); } \
    else if (_cv > (maxv)) { _cv = (maxv); snprintf(_cap, sizeof(_cap), "%d", (maxv)); m225_clamp_note(name, _cap); } \
    s_m225.field = (uint8_t)_cv; \
    s_m225.configuration_counter++; \
} while (0)

#define S_U16_CLAMP(field, minv, maxv) do { \
    int _cv = atoi(value); \
    char _cap[16]; \
    if (_cv < (minv)) { _cv = (minv); snprintf(_cap, sizeof(_cap), "%d", (minv)); m225_clamp_note(name, _cap); } \
    else if (_cv > (maxv)) { _cv = (maxv); snprintf(_cap, sizeof(_cap), "%d", (maxv)); m225_clamp_note(name, _cap); } \
    s_m225.field = (uint16_t)_cv; \
    s_m225.configuration_counter++; \
} while (0)

    if (!strcmp(ctx, "Ethernet")) {
        if (!strcmp(name, "DHCP"))          { S_BOOL(eth_dhcp);          return; }
        if (!strcmp(name, "IPAddress"))     { S_STR(eth_ip);             return; }
        if (!strcmp(name, "Gateway"))       { S_STR(eth_gateway);        return; }
        if (!strcmp(name, "Subnet"))        { S_STR(eth_subnet);         return; }
        if (!strcmp(name, "PortType"))      { S_STR(eth_port_type);      return; }
        if (!strcmp(name, "PortThreshold")) { S_U32(eth_port_threshold); return; }
        if (!strcmp(name, "MessageSlot"))   { S_STR(eth_message_slot);   return; }
    }
    if (!strcmp(ctx, "WiFi")) {
        if (!strcmp(name, "DHCP"))          { S_BOOL(wifi_dhcp);          return; }
        if (!strcmp(name, "PortType"))      { S_STR(wifi_port_type);      return; }
        if (!strcmp(name, "PortThreshold")) { S_U32(wifi_port_threshold); return; }
        if (!strcmp(name, "MessageSlot"))   { S_STR(wifi_message_slot);   return; }
        if (!strcmp(name, "Password"))      { S_STR(wifi_password);       return; }
    }
    if (!strcmp(ctx, "SerialPorts")) {
        if (!strcmp(name, "MessageSlot"))   { S_STR(message_slot); return; }
    }
    if (!strcmp(ctx, "ISiteIP")) {
        if (!strcmp(name, "DHCP"))          { S_BOOL(isite_dhcp);    return; }
        if (!strcmp(name, "IPAddress"))     { S_STR(isite_ip);       return; }
        if (!strcmp(name, "Subnet"))        { S_STR(isite_subnet);   return; }
        if (!strcmp(name, "Gateway"))       { S_STR(isite_gateway);  return; }
    }
    if (!strcmp(ctx, "Accumulators")) {
        if (!strcmp(name, "Password"))      { S_STR(password); return; }
    }
    if (!strcmp(ctx, "BadgeReader")) {
        if (!strcmp(name, "ThresholdWeight")) { S_FLT(threshold_weight); return; }
    }
    if (!strcmp(ctx, "AxleWeigher")) {
        if (!strcmp(name, "ThresholdWeight")) { S_FLT(axle_threshold); return; }
    }
    if (!strcmp(ctx, "IDStorage")) {
        if (!strcmp(name, "IDCount"))  { S_U8(id_count);   return; }
        if (!strcmp(name, "Prompt1"))  { S_STR(prompt1);   return; }
        if (!strcmp(name, "Prompt2"))  { S_STR(prompt2);   return; }
        if (!strcmp(name, "Prompt3"))  { S_STR(prompt3);   return; }
    }
    if (!strcmp(ctx, "PackageWeigher")) {
        if (!strcmp(name, "IDCount"))  { S_U8(pkg_id_count);  return; }
        if (!strcmp(name, "Prompt1"))  { S_STR(pkg_prompt1);  return; }
        if (!strcmp(name, "Prompt2"))  { S_STR(pkg_prompt2);  return; }
        if (!strcmp(name, "Prompt3"))  { S_STR(pkg_prompt3);  return; }
    }
    if (!strcmp(ctx, "DFC")) {
        if (!strcmp(name, "Speed"))         { S_STR(dfc_speed);       return; }
        if (!strcmp(name, "GateSequence"))  { S_STR(gate_sequence);   return; }
        if (!strcmp(name, "AutoTrim"))      { S_BOOL(auto_trim);       return; }
        if (!strcmp(name, "AutoPrint"))     { S_BOOL(dfc_auto_print);  return; }
        if (!strcmp(name, "DumpGate"))      { S_BOOL(dump_gate);       return; }
        if (!strcmp(name, "AutoDump"))      { S_BOOL(auto_dump);       return; }
        if (!strcmp(name, "Decumulate"))    { S_BOOL(decumulate);      return; }
        if (!strcmp(name, "ZeroTolerance")) { S_FLT(zero_tolerance);   return; }
        if (!strcmp(name, "GateTimer"))     { S_U16(gate_timer);       return; }
        if (!strcmp(name, "Trim"))          { S_FLT(dfc_trim);         return; }
    }
    if (!strcmp(ctx, "Batcher")) {
        if (!strcmp(name, "Speed"))         { S_STR(batcher_speed);          return; }
        if (!strcmp(name, "GateSequence"))  { S_STR(batcher_gate_seq);       return; }
        if (!strcmp(name, "AutoTrim"))      { S_BOOL(batcher_auto_trim);      return; }
        if (!strcmp(name, "AutoPrint"))     { S_BOOL(batcher_auto_print);     return; }
        if (!strcmp(name, "DumpGate"))      { S_BOOL(batcher_dump_gate);      return; }
        if (!strcmp(name, "AutoDump"))      { S_BOOL(batcher_auto_dump);      return; }
        if (!strcmp(name, "Decumulate"))    { S_BOOL(batcher_decumulate);     return; }
        if (!strcmp(name, "ZeroTolerance")) { S_FLT(batcher_zero_tolerance);  return; }
        if (!strcmp(name, "GateTimer"))     { S_U16(batcher_gate_timer);      return; }
    }
    if (!strcmp(ctx, "CheckWeigher")) {
        if (!strcmp(name, "AutoPrint"))     { S_BOOL(cw_auto_print); return; }
    }
    if (!strcmp(ctx, "PWC")) {
        if (!strcmp(name, "Trim"))          { S_FLT(pwc_trim); return; }
    }

    /* ── Flat (unambiguous) setters ───────────────────────────────────────── */

    /* IndicatorSetup */
    if (!strcmp(name, "USA"))                  { S_BOOL(usa);               return; }
    if (!strcmp(name, "LFT"))                  { S_BOOL(lft);               return; }
    if (!strcmp(name, "OIML"))                 { S_BOOL(oiml);              return; }
    if (!strcmp(name, "TimeFormat"))           { S_STR(time_format);        return; }
    if (!strcmp(name, "DateFormat"))           { S_STR(date_format);        return; }
    if (!strcmp(name, "ConsecutiveNumber"))    { S_U32(consecutive_number); return; }
    if (!strcmp(name, "ClearTare"))            { S_BOOL(clear_tare);        return; }
    if (!strcmp(name, "ClearID"))              { S_BOOL(clear_id);          return; }
    if (!strcmp(name, "NumberOfScales"))       { S_U8(number_of_scales);    return; }
    if (!strcmp(name, "Totalizer"))            { S_BOOL(totalizer);         return; }
    if (!strcmp(name, "ModeOfOperation"))      { S_STR(mode_of_operation);  return; }

    /* ScaleSetup */
    if (!strcmp(name, "PrimaryUnits"))         { S_STR(primary_units);      return; }
    if (!strcmp(name, "SecondaryUnits"))       { S_STR(secondary_units);    return; }
    if (!strcmp(name, "ZeroTracking"))         { S_STR(zero_tracking);      return; }
    if (!strcmp(name, "ZeroLimit"))            { S_BOOL(zero_limit);        return; }
    if (!strcmp(name, "PowerUpZero"))          { S_BOOL(power_up_zero);     return; }
    if (!strcmp(name, "SampleRate"))           { S_U8_CLAMP(sample_rate, 1, 16); return; }
    if (!strcmp(name, "MotionRange"))          { S_U8_CLAMP(motion_range, 1, 10); return; }
    if (!strcmp(name, "StableCount"))          { S_U8_CLAMP(stable_count, 1, 20); return; }
    if (!strcmp(name, "FilterLevel"))          { S_U8_CLAMP(filter_level, 0, 9); return; }
    if (!strcmp(name, "FilterBreak"))          { S_U8_CLAMP(filter_break, 0, 9); return; }
    if (!strcmp(name, "NumberOfScales"))       { S_U8_CLAMP(number_of_scales, 1, 3); return; }
    if (!strcmp(name, "DecimalPlace"))         { S_U8_CLAMP(decimal_place, 0, 4); return; }
    if (!strcmp(name, "PrelimFilterCount"))    { S_U8_CLAMP(prelim_filter_count, 0, 255); return; }
    if (!strcmp(name, "WeightIntervals"))      { S_U8(weight_intervals);      return; }
    if (!strcmp(name, "ScaleType"))            { S_STR(scale_type);           return; }
    if (!strcmp(name, "FilterType"))           { S_STR(filter_type);          return; }
    if (!strcmp(name, "Interval"))             { S_U32(interval);           return; }
    if (!strcmp(name, "Capacity"))             { S_U32(capacity);           return; }
    if (!strcmp(name, "LowInterval"))          { S_U32(low_interval);       return; }
    if (!strcmp(name, "LowDecimalPlace"))      { S_U8(low_decimal_place);   return; }
    if (!strcmp(name, "LowCapacity"))          { S_U32(low_capacity);       return; }
    if (!strcmp(name, "HighInterval"))         { S_U32(high_interval);      return; }
    if (!strcmp(name, "HighDecimalPlace"))     { S_U8(high_decimal_place);  return; }
    if (!strcmp(name, "HighCapacity"))         { S_U32(high_capacity);      return; }

    /* ScaleCalibration */
    if (!strcmp(name, "SpanWeight"))           { S_FLT(span_weight);        return; }
    if (!strcmp(name, "SpanCount"))            { S_U32(span_count);         return; }
    if (!strcmp(name, "ZeroCount"))            { S_U32(zero_count);         return; }
    if (!strcmp(name, "LC1"))                  { S_U32(lc1);                return; }
    if (!strcmp(name, "LC2"))                  { S_U32(lc2);           return; }
    if (!strcmp(name, "LC3"))                  { S_U32(lc3);           return; }
    if (!strcmp(name, "LC4"))                  { S_U32(lc4);           return; }
    if (!strcmp(name, "CellTrimming"))         { S_FLT(cell_trimming); return; }

    /* LoadCellAssignments */
    if (!strcmp(name, "CellID"))               { S_U8(cell_id);         return; }
    if (!strcmp(name, "CellToScale"))          { S_STR(cell_to_scale);  return; }
    if (!strcmp(name, "CellsPerScale"))        { S_U8(cells_per_scale); return; }
    if (!strcmp(name, "CellTrim"))             { S_FLT(cell_trim);      return; }

    /* SerialPorts (non-ambiguous) */
    if (!strcmp(name, "Type"))                 { S_STR(serial_type);         return; }
    if (!strcmp(name, "BaudRate"))             { S_STR(baud_rate);           return; }
    if (!strcmp(name, "DataBits"))             { S_STR(data_bits);           return; }
    if (!strcmp(name, "StopBits"))             { S_STR(stop_bits);           return; }
    if (!strcmp(name, "Parity"))               { S_STR(parity);              return; }
    if (!strcmp(name, "TransferCondition"))    { S_STR(transfer_condition);  return; }
    if (!strcmp(name, "Scale"))                { S_STR(serial_scale);        return; }
    if (!strcmp(name, "GrossOnly"))            { S_BOOL(gross_only);         return; }
    if (!strcmp(name, "ManualMode"))           { S_BOOL(manual_mode);        return; }
    if (!strcmp(name, "ElectricalInterface"))  { S_STR(electrical_interface);return; }
    if (!strcmp(name, "SBHighThreshold"))      { S_U32(sb_high_threshold);   return; }

    /* Ethernet (non-ambiguous) */
    if (!strcmp(name, "EthernetEnable"))       { S_BOOL(ethernet_enable);    return; }
    if (!strcmp(name, "ServerPortA"))          { S_U16(server_port_a);       return; }
    if (!strcmp(name, "ServerPortB"))          { S_U16(server_port_b);       return; }
    if (!strcmp(name, "ServerPortC"))          { S_U16(server_port_c);       return; }
    if (!strcmp(name, "ClientServerPort"))     { S_U16(client_server_port);  return; }

    /* WiFi (non-ambiguous) */
    if (!strcmp(name, "WiFiEnable"))           { S_BOOL(wifi_enable); return; }
    if (!strcmp(name, "SSID"))                 { S_STR(wifi_ssid);    return; }

    /* ISiteIP (non-ambiguous) */
    if (!strcmp(name, "SiteOrder"))            { S_STR(site_order);   return; }
    if (!strcmp(name, "DNS1"))                 { S_STR(dns1);         return; }
    if (!strcmp(name, "DNS2"))                 { S_STR(dns2);         return; }
    if (!strcmp(name, "EnableDNS"))            { S_BOOL(enable_dns);  return; }

    /* SendGross */
    if (!strcmp(name, "SendGrossEnable"))      { S_BOOL(send_gross_enable);  return; }
    if (!strcmp(name, "GrossWeightPort"))      { S_STR(gross_weight_port);   return; }

    /* PrinterSetup */
    if (!strcmp(name, "Port"))                 { S_STR(printer_port);       return; }
    if (!strcmp(name, "AutoLF"))               { S_BOOL(auto_lf);           return; }
    if (!strcmp(name, "EndingLF"))             { S_U8(ending_lf);           return; }
    if (!strcmp(name, "EndOfLine"))            { S_STR(end_of_line);        return; }
    if (!strcmp(name, "StartOfTicket"))        { S_STR(start_of_ticket);    return; }
    if (!strcmp(name, "EndOfTicket"))          { S_STR(end_of_ticket);      return; }
    if (!strcmp(name, "EndOfTicketLineFeeds")) { S_U8(end_of_ticket_lf);    return; }
    if (!strcmp(name, "PrintSlot"))            { S_U8(print_slot);          return; }
    if (!strcmp(name, "TimeTab"))              { S_FLT(time_tab);           return; }
    if (!strcmp(name, "DateTab"))              { S_FLT(date_tab);           return; }
    if (!strcmp(name, "ConsecutiveTab"))       { S_FLT(consecutive_tab);    return; }
    if (!strcmp(name, "GrossTab"))             { S_FLT(gross_tab);          return; }
    if (!strcmp(name, "TareTab"))              { S_FLT(tare_tab);           return; }
    if (!strcmp(name, "NetTab"))               { S_FLT(net_tab);            return; }
    if (!strcmp(name, "GrossAccumTab"))        { S_FLT(gross_accum_tab);    return; }
    if (!strcmp(name, "NetAccumTab"))          { S_FLT(net_accum_tab);      return; }
    if (!strcmp(name, "IDTab"))                { S_FLT(id_tab);             return; }

    /* Accumulators (non-ambiguous) */
    if (!strcmp(name, "GenAccums"))            { S_BOOL(gen_accums);  return; }
    if (!strcmp(name, "LRPort"))               { S_STR(lr_port);      return; }

    /* DACOutput */
    if (!strcmp(name, "DACGross"))             { S_BOOL(dac_gross);       return; }
    if (!strcmp(name, "DACLowWeight"))         { S_FLT(dac_low_weight);  return; }
    if (!strcmp(name, "DACHighWeight"))        { S_FLT(dac_high_weight); return; }
    if (!strcmp(name, "DACVoltOutput"))        { S_FLT(dac_volt_output); return; }
    if (!strcmp(name, "AdjustHigh"))           { S_I32(adjust_high);     return; }
    if (!strcmp(name, "AdjustLow"))            { S_I32(adjust_low);      return; }
    if (!strcmp(name, "DACScale"))             { S_STR(dac_scale);       return; }

    /* KeyLockout */
    if (!strcmp(name, "ZeroKeyLock"))          { S_BOOL(zero_key_lock);    return; }
    if (!strcmp(name, "TareKeyLock"))          { S_BOOL(tare_key_lock);    return; }
    if (!strcmp(name, "NetKeyLock"))           { S_BOOL(net_key_lock);     return; }
    if (!strcmp(name, "PrintKeyLock"))         { S_BOOL(print_key_lock);   return; }
    if (!strcmp(name, "UnitKeyLock"))          { S_BOOL(unit_key_lock);    return; }
    if (!strcmp(name, "GreenKeyLock"))         { S_BOOL(green_key_lock);   return; }
    if (!strcmp(name, "KeypadLock"))           { S_BOOL(keypad_lock);      return; }
    if (!strcmp(name, "IDKeyLock"))            { S_BOOL(id_key_lock);      return; }
    if (!strcmp(name, "CountKeyLock"))         { S_BOOL(count_key_lock);   return; }
    if (!strcmp(name, "MemKeyLock"))           { S_BOOL(mem_key_lock);     return; }
    if (!strcmp(name, "PresetKeyLock"))        { S_BOOL(preset_key_lock);  return; }
    if (!strcmp(name, "DeleteKeyLock"))        { S_BOOL(delete_key_lock);  return; }
    if (!strcmp(name, "StartKeyLock"))         { S_BOOL(start_key_lock);   return; }
    if (!strcmp(name, "DropKeyLock"))          { S_BOOL(drop_key_lock);    return; }
    if (!strcmp(name, "PauseKeyLock"))         { S_BOOL(pause_key_lock);   return; }
    if (!strcmp(name, "StopKeyLock"))          { S_BOOL(stop_key_lock);    return; }
    if (!strcmp(name, "RestartKeyLock"))       { S_BOOL(restart_key_lock); return; }
    if (!strcmp(name, "DumpKeyLock"))          { S_BOOL(dump_key_lock);    return; }

    /* BadgeReader (non-ambiguous) */
    if (!strcmp(name, "Reader1Port"))          { S_STR(reader1_port);  return; }
    if (!strcmp(name, "Reader1Type"))          { S_STR(reader1_type);  return; }
    if (!strcmp(name, "Reader2Port"))          { S_STR(reader2_port);  return; }
    if (!strcmp(name, "Reader2Type"))          { S_STR(reader2_type);  return; }
    if (!strcmp(name, "SiteID"))               { S_BOOL(site_id);      return; }

    /* WINVRS */
    if (!strcmp(name, "Computer1Port"))        { S_STR(computer1_port);         return; }
    if (!strcmp(name, "Computer1Mode"))        { S_STR(computer1_mode);         return; }
    if (!strcmp(name, "Computer2Port"))        { S_STR(computer2_port);         return; }
    if (!strcmp(name, "Computer2Mode"))        { S_STR(computer2_mode);         return; }
    if (!strcmp(name, "PrintPassthrough"))     { S_BOOL(print_passthrough);     return; }
    if (!strcmp(name, "TrafficMode"))          { S_STR(traffic_mode);           return; }
    if (!strcmp(name, "TrafficOnThreshold"))   { S_FLT(traffic_on_threshold);   return; }
    if (!strcmp(name, "TrafficOffThreshold"))  { S_FLT(traffic_off_threshold);  return; }
    if (!strcmp(name, "TrafficOffDelay"))      { S_U8(traffic_off_delay);       return; }
    if (!strcmp(name, "EnterRelayCommand"))    { S_STR(enter_relay_command);    return; }
    if (!strcmp(name, "ExitRelayCommand"))     { S_STR(exit_relay_command);     return; }
    if (!strcmp(name, "TrafficDisplay"))       { S_BOOL(traffic_display);       return; }
    if (!strcmp(name, "DFCEnable"))            { S_BOOL(winvrs_dfc_enable);     return; }

    /* IDStorage (non-ambiguous) */
    if (!strcmp(name, "WeightAlarm"))          { S_BOOL(weight_alarm);    return; }
    if (!strcmp(name, "AlarmThreshold"))       { S_FLT(alarm_threshold);  return; }
    if (!strcmp(name, "AlarmTimeOn"))          { S_U8(alarm_time_on);     return; }

    /* DFC (non-ambiguous) */
    if (!strcmp(name, "MultiDrop"))            { S_BOOL(multi_drop);    return; }
    if (!strcmp(name, "AutoTare"))             { S_BOOL(auto_tare);     return; }
    if (!strcmp(name, "JogToCutoff"))          { S_BOOL(jog_to_cutoff); return; }
    if (!strcmp(name, "FastCutoff"))           { S_FLT(fast_cutoff);    return; }
    if (!strcmp(name, "FillWeight"))           { S_FLT(fill_weight);    return; }
    if (!strcmp(name, "SlowCutoff"))           { S_FLT(slow_cutoff);    return; }
    if (!strcmp(name, "DropCount"))            { S_U8(drop_count);      return; }
    if (!strcmp(name, "Chatter"))              { S_FLT(chatter);        return; }

    /* Batcher (non-ambiguous) */
    if (!strcmp(name, "BinCount"))             { S_U8(bin_count);        return; }
    if (!strcmp(name, "BatchCount"))           { S_U32(batch_count);     return; }
    if (!strcmp(name, "SettleTimer"))          { S_U16(settle_timer);    return; }

    /* PackageWeigher (non-ambiguous) */
    if (!strcmp(name, "RetainID"))             { S_BOOL(retain_id); return; }

    /* AxleWeigher (non-ambiguous) */
    if (!strcmp(name, "AWMode"))               { S_STR(aw_mode);      return; }
    if (!strcmp(name, "AxlePads"))             { S_BOOL(axle_pads);   return; }
    if (!strcmp(name, "StopDelay"))            { S_U16(stop_delay);   return; }
    if (!strcmp(name, "TotalDelay"))           { S_U16(total_delay);  return; }
    if (!strcmp(name, "AxleCounter"))          { S_U32(axle_counter); return; }

    /* CheckWeigher (non-ambiguous) */
    if (!strcmp(name, "Outputs"))              { S_U8(cw_outputs);           return; }
    if (!strcmp(name, "UnderThreshold"))       { S_FLT(under_threshold);     return; }
    if (!strcmp(name, "LowOKThreshold"))       { S_FLT(low_ok_threshold);    return; }
    if (!strcmp(name, "HighOKThreshold"))      { S_FLT(high_ok_threshold);   return; }
    if (!strcmp(name, "OverThreshold"))        { S_FLT(over_threshold);      return; }
    if (!strcmp(name, "ColorUnder"))           { S_STR(color_under);         return; }
    if (!strcmp(name, "ColorLowOK"))           { S_STR(color_low_ok);        return; }
    if (!strcmp(name, "ColorHighOK"))          { S_STR(color_high_ok);       return; }
    if (!strcmp(name, "ColorAcceptOK"))        { S_STR(color_accept_ok);     return; }
    if (!strcmp(name, "ColorOver"))            { S_STR(color_over);          return; }

    /* PWC (non-ambiguous) */
    if (!strcmp(name, "NumberOfOutputs"))      { S_U8(number_of_pwc_outputs); return; }
    if (!strcmp(name, "BalanceOnPrint"))       { S_BOOL(balance_on_print);    return; }
    if (!strcmp(name, "MonitorZero"))          { S_BOOL(monitor_zero);        return; }
    if (!strcmp(name, "Threshold"))            { S_FLT(pwc_threshold);        return; }
    if (!strcmp(name, "OutputScale"))          { S_STR(output_scale);         return; }

    /* Livestock */
    if (!strcmp(name, "Inclinometer"))         { S_BOOL(inclinometer); return; }
    if (!strcmp(name, "SetPitch"))             { S_FLT(set_pitch);     return; }
    if (!strcmp(name, "SetRoll"))              { S_FLT(set_roll);      return; }

    /* DLCSetup */
    if (!strcmp(name, "SnapMediabox"))         { S_BOOL(snap_mediabox); return; }
    if (!strcmp(name, "SnapID"))               { S_U8(snap_id);         return; }
    if (!strcmp(name, "Channel"))              { S_U8(snap_channel);    return; }

    /* ReviewMenu */
    if (!strcmp(name, "ScaleID"))              { S_U8(scale_id); return; }

#undef S_STR
#undef S_BOOL
#undef S_U8
#undef S_U16
#undef S_U32
#undef S_I32
#undef S_FLT
}

/* ─── GET response — reply with all properties as JSON ──────────────────── */

static void do_get(AMessage_st *msg)
{
    m225_set_menu_ctx((char *)msg->dest_Actor_a8);

    cJSON *in_j  = cJSON_Parse((char *)msg->payload_p8);
    cJSON *out_j = cJSON_CreateObject();
    if (!in_j) goto send;

    cJSON *names = cJSON_GetObjectItem(in_j, "Property_Names");
    int n = cJSON_GetArraySize(names);
    for (int i = 0; i < n; i++) {
        cJSON *el = cJSON_GetArrayItem(names, i);
        if (!cJSON_IsString(el)) continue;
        char val[256] = {0};
        m225_actor_get(el->valuestring, val, sizeof(val));
        cJSON_AddStringToObject(out_j, el->valuestring, val);
    }
    cJSON_Delete(in_j);

send:
    memset(s_payload_buf, 0, sizeof(s_payload_buf));
    cJSON_PrintPreallocated(out_j, s_payload_buf, sizeof(s_payload_buf), false);
    cJSON_Delete(out_j);
    strcpy((char *)msg->payload_p8, s_payload_buf);
    console_send_responce_to_console_xface(msg);
}

/* ─── SET handler ────────────────────────────────────────────────────────── */

static void do_set(AMessage_st *msg)
{
    m225_set_menu_ctx((char *)msg->dest_Actor_a8);
    cJSON *in_j = cJSON_Parse((char *)msg->payload_p8);
    if (!in_j) { console_send_responce_to_console_xface(msg); return; }

    cJSON *item = in_j->child;
    while (item) {
        if (cJSON_IsString(item))
            m225_set_property(item->string, item->valuestring, msg);
        item = item->next;
    }
    cJSON_Delete(in_j);
    console_send_responce_to_console_xface(msg);
}

/* ─── Monitor task ───────────────────────────────────────────────────────── */

static void monitor(void *pv __attribute__((unused)))
{
    while (1) {
        AMessage_st rx;
        memset(&rx, 0, sizeof(rx));
        if (pdTRUE != xQueueReceive(s_rx_queue, &rx, portMAX_DELAY))
            continue;

        memset(s_rx_buf, 0, sizeof(s_rx_buf));
        if (rx.payload_p8) {
            strncpy(s_rx_buf, (char *)rx.payload_p8, sizeof(s_rx_buf) - 1);
            console_MessageRelease_xface((char *)rx.payload_p8);
            rx.payload_p8 = NULL;
        }
        rx.payload_p8 = (uint8_t *)s_rx_buf;

        if      (!strcmp((char *)rx.cmdFun_a8, "GET"))  do_get(&rx);
        else if (!strcmp((char *)rx.cmdFun_a8, "SET"))  do_set(&rx);
        else if (!strcmp((char *)rx.cmdFun_a8, "INIT")) console_send_responce_to_console_xface(&rx);
        else                                             console_send_responce_to_console_xface(&rx);
    }
}

/* ─── Init ───────────────────────────────────────────────────────────────── */

static void actor_init(void)
{
    m225_nvs_init();
    s_rx_queue = xQueueCreateStatic(M225_QUEUE_LENGTH, sizeof(AMessage_st),
                                    s_queue_storage, &s_queue_buf);
    xTaskCreateStaticPinnedToCore(
        monitor, "M225Monitor", M225_TASK_STACK / sizeof(StackType_t),
        NULL, M225_TASK_PRIORITY, s_task_stack, &s_task_buf, 1);
    s_initialised = true;
}

/* ─── Interface function (called by console_que_sel) ────────────────────── */

void model225_ConsoleWriteToActor_xface(void *msg)
{
    AMessage_st *m = (AMessage_st *)msg;
    if (!s_initialised) actor_init();

    uint8_t rc = xQueueSend(s_rx_queue, m, QUE_DELAY);
    if (rc != pdTRUE && m->payload_p8) {
        free(m->payload_p8);
        m->payload_p8 = NULL;
    }
}
