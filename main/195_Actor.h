#pragma once
/*
 * 195_Actor.h — Model 225 Indicator actor (dummy variable store)
 *
 * Holds in-RAM state for every configuration variable described in
 * Appendix A of the Socket Communications Wrapper Spec v23.
 * Values are initialised to spec defaults and survive only until reboot
 * (no NVS persistence — this is the dummy/placeholder implementation).
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ─── Complete Model 225 state struct ────────────────────────────────────── */

typedef struct {

    /* ── IndicatorSetup ─────────────────────────────────────────────────── */
    bool     usa;
    bool     lft;
    bool     oiml;
    char     time_format[4];          /* "12H" | "24H"                      */
    char     date_format[12];         /* "MM-DD-YYYY" | "DD-MM-YYYY"        */
    uint32_t consecutive_number;
    bool     clear_tare;
    bool     clear_id;
    uint8_t  number_of_scales;
    bool     totalizer;
    char     mode_of_operation[20];   /* enum: Normal/IDStorage/Batcher/... */

    /* ── ScaleSetup ─────────────────────────────────────────────────────── */
    char     primary_units[4];        /* "lb" | "kg"                        */
    char     secondary_units[4];
    char     zero_tracking[6];        /* "0.0d".."3.0d"                     */
    bool     zero_limit;
    bool     power_up_zero;
    uint8_t  sample_rate;
    uint8_t  motion_range;
    uint8_t  stable_count;
    uint8_t  weight_intervals;        /* 1 = single, 2 = dual               */
    char     scale_type[8];           /* "Analog" | "DLC" | "Remote"        */
    char     filter_type[8];          /* "None" | "Average" | "Median"      */
    uint8_t  filter_level;
    uint8_t  filter_break;
    /* single-range */
    uint32_t interval;
    uint8_t  decimal_place;
    uint32_t capacity;
    /* dual-range */
    uint32_t low_interval;
    uint8_t  low_decimal_place;
    uint32_t low_capacity;
    uint32_t high_interval;
    uint8_t  high_decimal_place;
    uint32_t high_capacity;
    /* DLC only */
    uint8_t  prelim_filter_count;

    /* ── ScaleCalibration ───────────────────────────────────────────────── */
    float    span_weight;
    uint32_t span_count;
    uint32_t zero_count;
    uint32_t lc1, lc2, lc3, lc4;
    float    cell_trimming;

    /* ── LoadCellAssignments ────────────────────────────────────────────── */
    uint8_t  cell_id;
    char     cell_to_scale[8];        /* "Scale1" | "Scale2" | "Scale3"     */
    uint8_t  cells_per_scale;
    float    cell_trim;

    /* ── ComSetup — SerialPorts ─────────────────────────────────────────── */
    char     serial_type[12];         /* "SMA"|"SB600"|"Continuous"|"Disabled" */
    char     baud_rate[8];            /* "9600" etc.                        */
    char     data_bits[2];            /* "7" | "8"                          */
    char     stop_bits[2];            /* "1" | "2"                          */
    char     parity[6];               /* "None" | "Even" | "Odd"            */
    char     transfer_condition[12];  /* "Continuous"|"OnPrint"|...         */
    char     serial_scale[16];        /* "CurrentSelected"|"Scale1"|...     */
    bool     gross_only;
    bool     manual_mode;
    char     message_slot[12];        /* "Disabled"|"Slot1"|...             */
    char     electrical_interface[6]; /* "RS232"|"RS485"|"RS422"            */
    uint32_t sb_high_threshold;

    /* ── ComSetup — Ethernet ────────────────────────────────────────────── */
    bool     ethernet_enable;
    bool     eth_dhcp;
    char     eth_ip[16];
    char     eth_gateway[16];
    char     eth_subnet[16];
    uint16_t server_port_a;
    uint16_t server_port_b;
    uint16_t server_port_c;
    uint16_t client_server_port;
    char     eth_port_type[8];        /* "SB600"|"SMA"|"Disabled"           */
    uint32_t eth_port_threshold;
    char     eth_message_slot[12];

    /* ── ComSetup — WiFi ────────────────────────────────────────────────── */
    bool     wifi_enable;
    bool     wifi_dhcp;
    char     wifi_ssid[32];
    char     wifi_password[32];
    char     wifi_port_type[8];
    uint32_t wifi_port_threshold;
    char     wifi_message_slot[12];

    /* ── ComSetup — ISiteIP ─────────────────────────────────────────────── */
    char     site_order[32];
    bool     isite_dhcp;
    char     isite_ip[16];
    char     isite_subnet[16];
    char     isite_gateway[16];
    char     dns1[16];
    char     dns2[16];
    bool     enable_dns;

    /* ── ComSetup — SendGross ───────────────────────────────────────────── */
    bool     send_gross_enable;
    char     gross_weight_port[8];    /* "Com1"|"Com2"|"USB"|"OPC1"|"OPC2" */

    /* ── PrinterSetup ───────────────────────────────────────────────────── */
    char     printer_port[8];
    bool     auto_lf;
    uint8_t  ending_lf;
    char     end_of_line[4];
    char     start_of_ticket[32];
    char     end_of_ticket[32];
    uint8_t  end_of_ticket_lf;
    uint8_t  print_slot;
    float    time_tab;
    float    date_tab;
    float    consecutive_tab;
    float    gross_tab;
    float    tare_tab;
    float    net_tab;
    float    gross_accum_tab;
    float    net_accum_tab;
    float    id_tab;

    /* ── SystemConfig — Accumulators ────────────────────────────────────── */
    bool     gen_accums;
    float    accumulator_scale1;
    float    accumulator_scale2;
    float    accumulator_scale3;
    float    totalizer_accumulator;
    char     password[32];
    char     lr_port[8];

    /* ── SystemConfig — DACOutput ───────────────────────────────────────── */
    bool     dac_gross;
    float    dac_low_weight;
    float    dac_high_weight;
    float    dac_volt_output;
    int32_t  adjust_high;
    int32_t  adjust_low;
    char     dac_scale[8];

    /* ── SystemConfig — KeyLockout (18 keys) ────────────────────────────── */
    bool     zero_key_lock;
    bool     tare_key_lock;
    bool     net_key_lock;
    bool     print_key_lock;
    bool     unit_key_lock;
    bool     green_key_lock;
    bool     keypad_lock;
    bool     id_key_lock;
    bool     count_key_lock;
    bool     mem_key_lock;
    bool     preset_key_lock;
    bool     delete_key_lock;
    bool     start_key_lock;
    bool     drop_key_lock;
    bool     pause_key_lock;
    bool     stop_key_lock;
    bool     restart_key_lock;
    bool     dump_key_lock;

    /* ── SystemConfig — BadgeReader ─────────────────────────────────────── */
    char     reader1_port[8];
    char     reader1_type[12];        /* "None"|"AWID"|"HID"|"Proximity"    */
    char     reader2_port[8];
    char     reader2_type[12];
    float    threshold_weight;
    bool     site_id;

    /* ── SystemConfig — WINVRS ──────────────────────────────────────────── */
    char     computer1_port[8];
    char     computer1_mode[12];      /* "Disabled"|"Enabled"|"Continuous"|"Terminal" */
    char     computer2_port[8];
    char     computer2_mode[12];      /* "Disabled"|"Remote"                */
    bool     print_passthrough;
    char     traffic_mode[4];         /* "Off"|"RGR"|"GRG"                  */
    float    traffic_on_threshold;
    float    traffic_off_threshold;
    uint8_t  traffic_off_delay;
    char     enter_relay_command[8];
    char     exit_relay_command[8];
    bool     traffic_display;
    bool     winvrs_dfc_enable;

    /* ── ModeConfig — IDStorage ─────────────────────────────────────────── */
    bool     weight_alarm;
    float    alarm_threshold;
    uint8_t  alarm_time_on;
    uint8_t  id_count;
    char     prompt1[16];
    char     prompt2[16];
    char     prompt3[16];

    /* ── ModeConfig — DFC ───────────────────────────────────────────────── */
    char     dfc_speed[12];           /* "SingleSpeed" | "DualSpeed"        */
    char     gate_sequence[6];        /* "ABB" | "ABA" | "ABBA"            */
    bool     auto_trim;
    bool     dfc_auto_print;
    bool     multi_drop;
    bool     dump_gate;
    bool     auto_dump;
    bool     decumulate;
    bool     auto_tare;
    bool     jog_to_cutoff;
    float    fast_cutoff;
    float    fill_weight;
    float    slow_cutoff;
    float    dfc_trim;
    uint8_t  drop_count;
    float    zero_tolerance;
    uint16_t gate_timer;
    float    chatter;

    /* ── ModeConfig — Batcher ───────────────────────────────────────────── */
    char     batcher_speed[12];
    char     batcher_gate_seq[6];
    bool     batcher_auto_trim;
    bool     batcher_auto_print;
    bool     batcher_dump_gate;
    bool     batcher_auto_dump;
    bool     batcher_decumulate;
    uint8_t  bin_count;
    uint32_t batch_count;
    float    batcher_zero_tolerance;
    uint16_t batcher_gate_timer;
    uint16_t settle_timer;

    /* ── ModeConfig — PackageWeigher ────────────────────────────────────── */
    uint8_t  pkg_id_count;
    char     pkg_prompt1[16];
    char     pkg_prompt2[16];
    char     pkg_prompt3[16];
    bool     retain_id;

    /* ── ModeConfig — AxleWeigher ───────────────────────────────────────── */
    char     aw_mode[8];              /* "Auto" | "Manual"                  */
    bool     axle_pads;
    float    axle_threshold;
    uint16_t stop_delay;
    uint16_t total_delay;
    uint32_t axle_counter;

    /* ── ModeConfig — CheckWeigher ──────────────────────────────────────── */
    uint8_t  cw_outputs;
    bool     cw_auto_print;
    float    under_threshold;
    float    low_ok_threshold;
    float    high_ok_threshold;
    float    over_threshold;
    char     color_under[8];
    char     color_low_ok[8];
    char     color_high_ok[8];
    char     color_accept_ok[8];
    char     color_over[8];

    /* ── ModeConfig — PWC ───────────────────────────────────────────────── */
    uint8_t  number_of_pwc_outputs;
    bool     balance_on_print;
    bool     monitor_zero;
    float    pwc_threshold;
    float    pwc_trim;
    char     output_scale[8];

    /* ── ModeConfig — Livestock ─────────────────────────────────────────── */
    bool     inclinometer;
    float    set_pitch;
    float    set_roll;

    /* ── DLCSetup ───────────────────────────────────────────────────────── */
    bool     snap_mediabox;
    uint8_t  snap_id;
    uint8_t  snap_channel;

    /* ── ReviewMenu ─────────────────────────────────────────────────────── */
    uint8_t  scale_id;
    uint32_t calibration_counter;
    uint32_t configuration_counter;

} m225_state_t;

/* ─── Public interface ───────────────────────────────────────────────────── */

/*
 * Read a named property from the Model 225 state struct.
 * Writes a null-terminated string representation into val_out.
 * Returns true if the name was recognised, false otherwise.
 * Used by menu_builder.c as the actor_getter_fn.
 */
bool m225_actor_get(const char *name, char *val_out, size_t max_len);

/* Setter wrapper with the same signature used by BLE_Actor.c for all actors:
   bool (*as)(const char *name, const char *value).
   Stores the value into the named field; always returns true. */
bool m225_actor_set(const char *name, const char *value);

/*
 * Set the actor context used by m225_actor_get/set to disambiguate property
 * names shared across multiple sub-actors (e.g. DHCP, Speed, Password).
 * Called by menu_builder before building items, and by do_get/do_set with
 * the dest_Actor_a8 field from the incoming message.
 */
void m225_set_menu_ctx(const char *ctx);

/*
 * Standard actor interface function called by console_que_sel().
 * Lazily initialises the actor on first call, then queues the message.
 */
void model225_ConsoleWriteToActor_xface(void *msg);
