/*
 * menu_builder.c — Spec §9 MENU discovery for Cardinal ESP32-S3
 *
 * Each handler returns a cJSON array of typed menu-item objects.
 * Item fields per spec §9.3:
 *   name       — property key (matches get/set key)
 *   tip        — human-readable description
 *   type       — "Variable" | "readOnlyVariable" | "menuItem" | "method"
 *   dataType   — "string" | "uint8" | "uint16" | "uint32" | "bool" | "float" | "enum"
 *   value      — live value from actor getter (string form)
 *   values     — (enum only) array of valid option strings
 *   maxLength  — (string only) maximum character count
 *   min / max  — (numeric only) value range
 */

#include "menu_builder.h"
#include "cJSON.h"
#include <string.h>
#include <stdbool.h>
#include <stddef.h>

/* ── Actor getter externs (public wrappers added at end of each actor .c) ── */
extern bool wifi_actor_get  (const char *name, char *val_out, size_t max_len);
extern bool eth_actor_get   (const char *name, char *val_out, size_t max_len);
extern bool ntp_actor_get   (const char *name, char *val_out, size_t max_len);
extern bool uart_actor_get  (const char *name, char *val_out, size_t max_len);
extern bool system_actor_get(const char *name, char *val_out, size_t max_len);
extern bool ble_actor_get   (const char *name, char *val_out, size_t max_len);
/* Model 225 Indicator getter + context setter (195_Actor.c) */
extern bool m225_actor_get  (const char *name, char *val_out, size_t max_len);
extern void m225_set_menu_ctx(const char *ctx);

/* ── Private helpers ─────────────────────────────────────────────────────── */

typedef bool (*actor_getter_fn)(const char *, char *, size_t);

/* Base item builder: emits spec-correct "type" field derived from access flag.
   access "RW" → "Variable", anything else → "readOnlyVariable". */
static cJSON *s_item(const char *name, const char *tip,
                     const char *dataType, const char *access,
                     actor_getter_fn getter)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name",     name);
    cJSON_AddStringToObject(item, "tip",      tip);
    cJSON_AddStringToObject(item, "type",
        (strcmp(access, "RW") == 0) ? "Variable" : "readOnlyVariable");
    cJSON_AddStringToObject(item, "dataType", dataType);
    char val[256] = {0};
    if (getter) getter(name, val, sizeof(val));
    cJSON_AddStringToObject(item, "value", val);
    return item;
}

static cJSON *s_str_item(const char *name, const char *tip,
                         const char *access, int max_len,
                         actor_getter_fn getter)
{
    cJSON *item = s_item(name, tip, "string", access, getter);
    cJSON_AddNumberToObject(item, "maxLength", max_len);
    return item;
}

static cJSON *s_int_item(const char *name, const char *tip,
                         const char *access, int min, int max,
                         actor_getter_fn getter)
{
    cJSON *item = s_item(name, tip, "uint32", access, getter);
    cJSON_AddNumberToObject(item, "min", min);
    cJSON_AddNumberToObject(item, "max", max);
    return item;
}

/* Boolean item: 0/1 values, no min/max. */
static cJSON *s_bool_item(const char *name, const char *tip,
                          const char *access, actor_getter_fn getter)
{
    return s_item(name, tip, "bool", access, getter);
}

/* Enum item: value is one of the strings in the vals array.
   Use only when the actor getter returns the string form (not a numeric index). */
static cJSON *s_enum_item(const char *name, const char *tip,
                          const char *access, const char **vals, int n,
                          actor_getter_fn getter)
{
    cJSON *item = s_item(name, tip, "enum", access, getter);
    cJSON *values = cJSON_CreateArray();
    for (int i = 0; i < n; i++)
        cJSON_AddItemToArray(values, cJSON_CreateString(vals[i]));
    cJSON_AddItemToObject(item, "values", values);
    return item;
}

/* ── menu_root ───────────────────────────────────────────────────────────── */

cJSON *menu_root(void)
{
    static const struct { const char *name; const char *tip; } actors[] = {
        { "BLE",     "Bluetooth LE configuration"                   },
        { "WIFI",    "Wi-Fi network settings"                       },
        { "ETH",     "Ethernet settings"                            },
        { "NTP",     "Time sync settings"                           },
        { "UART",    "Serial port settings"                         },
        { "SYSTEM",  "System information"                           },
        { "195Menu", "Model 225 Indicator — full settings menu"     },
    };
    cJSON *arr = cJSON_CreateArray();
    for (int i = 0; i < (int)(sizeof(actors)/sizeof(actors[0])); i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", actors[i].name);
        cJSON_AddStringToObject(obj, "type", "menuItem");
        cJSON_AddStringToObject(obj, "tip",  actors[i].tip);
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

/* ── menu_195_menu — top-level Model 225 sub-menu ────────────────────────── */

cJSON *menu_195_menu(void)
{
    static const struct {
        const char *name;
        const char *tip;
        const char *cond;   /* NULL = always visible */
    } items[] = {
        { "IndicatorSetup",      "Indicator configuration, clock, and mode settings",        NULL           },
        { "ScaleSetup",          "Scale measurement and filter settings",                     NULL           },
        { "ScaleCalibration",    "Span, zero, and load cell calibration",                     NULL           },
        { "LoadCellAssignments", "DLC load cell assignment and trim",                         "ScaleType=DLC"},
        { "ComSetup",            "Serial, Ethernet, and Wi-Fi communication settings",        NULL           },
        { "PrinterSetup",        "Ticket printer port and print tab configuration",           NULL           },
        { "SystemConfig",        "Accumulators, password, DAC, key lockout, badge reader",    NULL           },
        { "ModeConfig",          "Settings for the active mode of operation",                 NULL           },
        { "DLCSetup",            "DLC cell diagnostics and SNAP communication",               "ScaleType=DLC"},
        { "ReviewMenu",          "Scale ID and read-only calibration counters",               NULL           },
    };
    cJSON *arr = cJSON_CreateArray();

    /* Emit ScaleType as a readOnlyVariable so the app caches it in
       _currentValues and can evaluate the "ScaleType=DLC" condition on
       LoadCellAssignments and DLCSetup without having visited ScaleSetup. */
    m225_set_menu_ctx("ScaleSetup");
    cJSON_AddItemToArray(arr, s_item("ScaleType", "Scale hardware type",
                                     "enum", "R", m225_actor_get));

    for (int i = 0; i < (int)(sizeof(items)/sizeof(items[0])); i++) {
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "name", items[i].name);
        cJSON_AddStringToObject(obj, "type", "menuItem");
        cJSON_AddStringToObject(obj, "tip",  items[i].tip);
        if (items[i].cond)
            cJSON_AddStringToObject(obj, "condition", items[i].cond);
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

/* ── menu_ble ────────────────────────────────────────────────────────────── */

cJSON *menu_ble(void)
{
    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = ble_actor_get;

    cJSON_AddItemToArray(arr, s_int_item  ("PKT_SIZE",      "BSP packet size in bytes",              "R",  0, 65535, g));
    cJSON_AddItemToArray(arr, s_bool_item ("CONN_STATUS",   "BLE connection status",                 "R",  g));
    cJSON_AddItemToArray(arr, s_str_item  ("DEVICE_NAME",   "Advertised BLE device name",            "R",  32, g));
    cJSON_AddItemToArray(arr, s_str_item  ("UUID",          "BLE service UUID",                      "RW", 64, g));
    cJSON_AddItemToArray(arr, s_bool_item ("ADVERT_MODE",   "Advertising active",                    "R",  g));
    cJSON_AddItemToArray(arr, s_bool_item ("CONN_MODE",     "Connection mode",                       "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item ("ONLINE_STATUS", "Device online status",                  "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item ("CRED_STATUS",   "Credential status",                     "RW", g));

    return arr;
}

/* ── menu_wifi ───────────────────────────────────────────────────────────── */

cJSON *menu_wifi(void)
{
    static const char *wifi_modes[] = { "STA", "AP", "STA+AP" };

    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = wifi_actor_get;

    cJSON_AddItemToArray(arr, s_int_item  ("STATUS",           "WiFi connection status (0–3)",      "R",  0, 3,  g));
    cJSON_AddItemToArray(arr, s_bool_item ("AUTO_CONNECT",     "Auto-connect on startup",           "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item ("MODE",             "WiFi operating mode",               "R",  wifi_modes, 3, g));
    cJSON_AddItemToArray(arr, s_str_item  ("SSID1",            "Primary WiFi SSID",                 "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("PASS1",            "Primary WiFi password",             "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("SSID2",            "Secondary WiFi SSID",               "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("PASS2",            "Secondary WiFi password",           "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("SSID3",            "Tertiary WiFi SSID",                "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("PASS3",            "Tertiary WiFi password",            "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("RESCUE_SSID",      "Rescue WiFi SSID",                  "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("RESCUE_PASS",      "Rescue WiFi password",              "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("CONNECTED_SSID",   "Currently connected SSID",          "R",  64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("SSID_AP",          "Access-point SSID",                 "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("PASS_AP",          "Access-point password",             "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("MAC_ADD",          "WiFi module MAC address",           "R",  18, g));
    cJSON_AddItemToArray(arr, s_bool_item ("DHCP",             "DHCP enable",                       "RW", g));
    cJSON_AddItemToArray(arr, s_str_item  ("STA_IP_ADDR",      "Station static IP address",         "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("STA_GW_ADDR",      "Station gateway IP",                "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("STA_SM_ADDR",      "Station subnet mask",               "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("AP_IP_ADDR",       "Access-point IP address",           "R",  16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("AP_GW_ADDR",       "Access-point gateway",              "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("AP_SM_ADDR",       "Access-point subnet mask",          "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("WIFI_GATEWAY_MAC_ADDR", "Gateway MAC address",          "R",  18, g));

    return arr;
}

/* ── menu_ethernet ───────────────────────────────────────────────────────── */

cJSON *menu_ethernet(void)
{
    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = eth_actor_get;

    cJSON_AddItemToArray(arr, s_bool_item ("STATUS",   "Ethernet link status",                "R",  g));
    cJSON_AddItemToArray(arr, s_bool_item ("DHCP",     "DHCP enable",                         "RW", g));
    cJSON_AddItemToArray(arr, s_str_item  ("IP",       "Ethernet IP address",                 "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("SM",       "Subnet mask",                         "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("GW",       "Gateway IP address",                  "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("DNS1",     "Primary DNS server",                  "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("DNS2",     "Secondary DNS server",                "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item  ("PINGS1",   "Ping test server",                    "RW", 64, g));
    cJSON_AddItemToArray(arr, s_str_item  ("MAC_ADD",  "Ethernet MAC address",                "R",  18, g));
    cJSON_AddItemToArray(arr, s_str_item  ("ETH_GATEWAY_MAC_ADDR", "Gateway MAC address",     "R",  18, g));

    return arr;
}

/* ── menu_ntp ────────────────────────────────────────────────────────────── */

cJSON *menu_ntp(void)
{
    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = ntp_actor_get;

    cJSON_AddItemToArray(arr, s_str_item  ("URL",           "NTP server hostname",             "RW", 128, g));
    cJSON_AddItemToArray(arr, s_str_item  ("TIME_ZONE",     "POSIX time-zone string",          "RW", 64,  g));
    cJSON_AddItemToArray(arr, s_int_item  ("SYNC_INTERVAL", "NTP sync interval (seconds)",     "R",  60, 86400, g));
    cJSON_AddItemToArray(arr, s_bool_item ("RTC_STATUS",    "RTC sync status",                 "R",  g));
    cJSON_AddItemToArray(arr, s_bool_item ("NTP_MODE",      "NTP mode (0=WiFi, 1=Ethernet)",   "R",  g));

    return arr;
}

/* ── menu_uart ───────────────────────────────────────────────────────────── */

cJSON *menu_uart(void)
{
    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = uart_actor_get;

    /* PARITY and DATA_BITS are stored as integers by the UART actor.
       They remain uint32 until the actor is updated to store/return string values. */
    cJSON_AddItemToArray(arr, s_int_item ("BAUD_RATE", "Serial baud rate",             "RW", 300, 921600, g));
    cJSON_AddItemToArray(arr, s_int_item ("DATA_BITS", "Data bits (5–8)",              "RW", 5,   8,      g));
    cJSON_AddItemToArray(arr, s_int_item ("PARITY",    "Parity (0=none,1=odd,2=even)", "RW", 0,   2,      g));
    cJSON_AddItemToArray(arr, s_int_item ("STOP_BITS", "Stop bits (1=1bit,2=2bits)",   "RW", 1,   2,      g));

    return arr;
}

/* ── menu_system ─────────────────────────────────────────────────────────── */

cJSON *menu_system(void)
{
    cJSON *arr = cJSON_CreateArray();
    actor_getter_fn g = system_actor_get;

    cJSON_AddItemToArray(arr, s_str_item  ("MANUFACTURER_NAME",  "Manufacturer name",         "R",  64,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("MODEL_NAME",         "Device model name",         "R",  64,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("FIRMWARE",           "Firmware version string",   "R",  32,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("BOOTLOADER_VERSION", "Bootloader version",        "R",  32,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("HARDWARE",           "Hardware revision",         "R",  32,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("DEVICEID",           "Unique device identifier",  "R",  64,  g));
    cJSON_AddItemToArray(arr, s_str_item  ("MANUFACTURER_DATE",  "Manufacture date",          "R",  32,  g));
    cJSON_AddItemToArray(arr, s_bool_item ("AUTO_EXEC",          "Auto-execute on boot",      "RW", g));
    cJSON_AddItemToArray(arr, s_str_item  ("API_KEY",            "Cloud API key",             "RW", 128, g));
    cJSON_AddItemToArray(arr, s_str_item  ("DEVICE_ANNOUNCE_URL","Device announcement URL",   "RW", 256, g));

    return arr;
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Model 225 Indicator menus  (Spec v23 Appendix A)
 * ═══════════════════════════════════════════════════════════════════════════ */

/* Convenience: add a "condition" key to an existing item object. */
static void add_cond(cJSON *item, const char *cond)
{
    if (item && cond && cond[0])
        cJSON_AddStringToObject(item, "condition", cond);
}

/* Method item builder — params and response are pre-built cJSON objects. */
static cJSON *s_method(const char *name, const char *tip,
                        cJSON *params, cJSON *response)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name",     name);
    cJSON_AddStringToObject(item, "tip",      tip);
    cJSON_AddStringToObject(item, "type",     "method");
    cJSON_AddItemToObject  (item, "params",   params   ? params   : cJSON_CreateObject());
    cJSON_AddItemToObject  (item, "response", response ? response : cJSON_CreateObject());
    return item;
}

/* readOnlyVariable builder */
static cJSON *s_ro_item(const char *name, const char *tip,
                         const char *dataType, actor_getter_fn getter)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name",     name);
    cJSON_AddStringToObject(item, "tip",      tip);
    cJSON_AddStringToObject(item, "type",     "readOnlyVariable");
    cJSON_AddStringToObject(item, "dataType", dataType);
    char val[256] = {0};
    if (getter) getter(name, val, sizeof(val));
    cJSON_AddStringToObject(item, "value", val);
    return item;
}

/* Typed int item using an explicit dataType string (uint8 / uint16 / uint32 / int32 / float). */
static cJSON *s_typed_item(const char *name, const char *tip,
                            const char *dataType, const char *access,
                            actor_getter_fn getter)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name",     name);
    cJSON_AddStringToObject(item, "tip",      tip);
    cJSON_AddStringToObject(item, "type",
        (strcmp(access, "RW") == 0) ? "Variable" : "readOnlyVariable");
    cJSON_AddStringToObject(item, "dataType", dataType);
    char val[256] = {0};
    if (getter) getter(name, val, sizeof(val));
    cJSON_AddStringToObject(item, "value", val);
    return item;
}

/* Typed numeric item with min / max */
static cJSON *s_typed_range(const char *name, const char *tip,
                              const char *dataType, const char *access,
                              double mn, double mx,
                              actor_getter_fn getter)
{
    cJSON *item = s_typed_item(name, tip, dataType, access, getter);
    cJSON_AddNumberToObject(item, "min", mn);
    if (mx > mn) cJSON_AddNumberToObject(item, "max", mx);
    return item;
}

/* menuItem folder entry */
static cJSON *s_folder(const char *name, const char *tip)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "name", name);
    cJSON_AddStringToObject(item, "type", "menuItem");
    cJSON_AddStringToObject(item, "tip",  tip);
    return item;
}

/* ── A.1 IndicatorSetup ──────────────────────────────────────────────────── */

cJSON *menu_indicator_setup(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();

    static const char *tf[]  = { "12H", "24H" };
    static const char *df[]  = { "MM-DD-YYYY", "DD-MM-YYYY" };
    static const char *moo[] = { "Normal","IDStorage","Batcher","DFC",
                                  "PackageWeigher","AxleWeigher","CheckWeigher",
                                  "PWC","Livestock" };

    cJSON_AddItemToArray(arr, s_bool_item ("USA",                "Enable USA legal-for-trade mode",           "RW", g));
    cJSON_AddItemToArray(arr, s_ro_item   ("NSC",                "National Scale Count (always N/A)",         "string", g));
    cJSON_AddItemToArray(arr, s_bool_item ("LFT",                "Legal For Trade",                           "RW", g));

    cJSON *oiml = s_bool_item("OIML", "OIML compliance mode", "RW", g);
    add_cond(oiml, "USA=false");
    cJSON_AddItemToArray(arr, oiml);

    cJSON_AddItemToArray(arr, s_enum_item ("TimeFormat",         "Clock display format",                      "RW", tf,  2, g));
    cJSON_AddItemToArray(arr, s_enum_item ("DateFormat",         "Date display format",                       "RW", df,  2, g));
    cJSON_AddItemToArray(arr, s_typed_range("ConsecutiveNumber", "Starting consecutive ticket number",        "uint32", "RW", 0, 0, g));
    {
        cJSON *it = s_bool_item("ClearTare", "Clear tare on power up", "RW", g);
        add_cond(it, "CalSealed=YES, USA=YES, LFT=NO");
        cJSON_AddItemToArray(arr, it);
    }
    {
        cJSON *it = s_bool_item("ClearID", "Clear ID on power up", "RW", g);
        add_cond(it, "CalSealed=YES, USA=YES, LFT=NO");
        cJSON_AddItemToArray(arr, it);
    }
    {
        cJSON *it = s_typed_range("NumberOfScales", "Number of active scales", "uint8", "RW", 1, 3, g);
        add_cond(it, "ScaleCard != SingleScaleAnalog");
        cJSON_AddItemToArray(arr, it);
    }
    {
        cJSON *it = s_bool_item("Totalizer", "Enable multi-scale totalizer", "RW", g);
        add_cond(it, "NumberOfScales > 1");
        cJSON_AddItemToArray(arr, it);
    }
    cJSON_AddItemToArray(arr, s_enum_item("ModeOfOperation", "Active weighing mode", "RW", moo, 9, g));

    /* SetTime method */
    {
        cJSON *p = cJSON_CreateObject();
        cJSON *pt = cJSON_CreateObject();
        cJSON_AddStringToObject(pt, "dataType", "string");
        cJSON_AddStringToObject(pt, "format",   "HH:MM:SS");
        cJSON_AddItemToObject(p, "time", pt);
        cJSON *r = cJSON_CreateObject();
        cJSON *rt = cJSON_CreateObject();
        cJSON_AddStringToObject(rt, "dataType", "string");
        cJSON_AddItemToObject(r, "time", rt);
        cJSON_AddItemToArray(arr, s_method("SetTime", "Set the indicator real-time clock", p, r));
    }
    /* SetDate method */
    {
        cJSON *p = cJSON_CreateObject();
        cJSON *pd = cJSON_CreateObject();
        cJSON_AddStringToObject(pd, "dataType", "string");
        cJSON_AddStringToObject(pd, "format",   "MM-DD-YYYY");
        cJSON_AddItemToObject(p, "date", pd);
        cJSON *r = cJSON_CreateObject();
        cJSON *rd = cJSON_CreateObject();
        cJSON_AddStringToObject(rd, "dataType", "string");
        cJSON_AddItemToObject(r, "date", rd);
        cJSON_AddItemToArray(arr, s_method("SetDate", "Set the indicator date", p, r));
    }

    return arr;
}

/* ── A.2 ScaleSetup ──────────────────────────────────────────────────────── */

cJSON *menu_scale_setup(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();

    static const char *units[]  = { "lb", "kg" };
    static const char *zt[]     = { "0.0d","0.5d","1.0d","2.0d","3.0d" };
    static const char *stype[]  = { "Analog","DLC","Remote" };
    static const char *ftype[]  = { "None","Average","Median" };

    cJSON_AddItemToArray(arr, s_enum_item ("PrimaryUnits",     "Primary weighing unit",                    "RW", units, 2, g));
    cJSON_AddItemToArray(arr, s_enum_item ("SecondaryUnits",   "Secondary weighing unit",                  "RW", units, 2, g));
    cJSON_AddItemToArray(arr, s_enum_item ("ZeroTracking",     "Auto zero tracking range in disp. divs",   "RW", zt,    5, g));
    cJSON_AddItemToArray(arr, s_bool_item ("ZeroLimit",        "Restrict zero key to 2% of capacity",      "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item ("PowerUpZero",      "Automatically zero on power up",           "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("SampleRate",      "A/D conversions per second",               "uint8",  "RW", 1, 16,   g));
    cJSON_AddItemToArray(arr, s_typed_range("MotionRange",     "Motion detection threshold in disp. divs", "uint8",  "RW", 1, 99,   g));
    cJSON_AddItemToArray(arr, s_typed_range("StableCount",     "Consecutive stable readings required",     "uint8",  "RW", 1, 99,   g));
    cJSON_AddItemToArray(arr, s_typed_range("WeightIntervals", "Single or dual range weighing",            "uint8",  "RW", 1, 2,    g));
    cJSON_AddItemToArray(arr, s_enum_item ("ScaleType",        "Scale hardware type",                      "RW", stype, 3, g));
    cJSON_AddItemToArray(arr, s_enum_item ("FilterType",       "Digital filter algorithm",                 "RW", ftype, 3, g));
    cJSON_AddItemToArray(arr, s_typed_range("FilterLevel",     "Filter strength",                          "uint8",  "RW", 1, 9,    g));
    cJSON_AddItemToArray(arr, s_typed_range("FilterBreak",     "Filter break range in display divisions",  "uint8",  "RW", 1, 9,    g));

    /* single-range — conditional on WeightIntervals=1 */
    { cJSON *it = s_typed_range("Interval",     "Display graduation size (single range)",   "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("DecimalPlace", "Display decimal position (single range)",  "uint8",  "RW", 0, 4, g); add_cond(it, "WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("Capacity",     "Maximum scale capacity (single range)",    "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }

    /* dual-range — conditional on WeightIntervals=2 */
    { cJSON *it = s_typed_range("LowInterval",      "Low range graduation size",    "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("LowDecimalPlace",  "Low range decimal position",   "uint8",  "RW", 0, 4, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("LowCapacity",      "Low range capacity",           "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("HighInterval",     "High range graduation size",   "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("HighDecimalPlace", "High range decimal position",  "uint8",  "RW", 0, 4, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_range("HighCapacity",     "High range capacity",          "uint32", "RW", 1, 0, g); add_cond(it, "WeightIntervals=2"); cJSON_AddItemToArray(arr, it); }

    /* DLC only */
    { cJSON *it = s_typed_range("PrelimFilterCount", "Preliminary filter sample count (DLC)", "uint8", "RW", 0, 0, g); add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it); }

    return arr;
}

/* ── A.3 ScaleCalibration ────────────────────────────────────────────────── */

cJSON *menu_scale_calibration(void)
{
    m225_set_menu_ctx("ScaleCalibration");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();

    { cJSON *it = s_typed_range("SpanWeight", "Known reference weight for span cal (Analog)", "float",  "RW", 0, 0, g); add_cond(it, "ScaleType=Analog"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_ro_item("SpanCount", "A/D count at span weight (read only)", "uint32", g); add_cond(it, "ScaleType=Analog"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_ro_item("ZeroCount", "A/D count at zero (read only)",        "uint32", g); add_cond(it, "ScaleType=Analog"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_item("LC1", "Load cell 1 A/D count", "uint32", "RW", g); add_cond(it, "ScaleType=Analog, WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_item("LC2", "Load cell 2 A/D count", "uint32", "RW", g); add_cond(it, "ScaleType=Analog, WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_item("LC3", "Load cell 3 A/D count", "uint32", "RW", g); add_cond(it, "ScaleType=Analog, WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_typed_item("LC4", "Load cell 4 A/D count", "uint32", "RW", g); add_cond(it, "ScaleType=Analog, WeightIntervals=1"); cJSON_AddItemToArray(arr, it); }

    /* CalibrateSpan method */
    {
        cJSON *p = cJSON_CreateObject(); cJSON *sp = cJSON_CreateObject();
        cJSON_AddStringToObject(sp, "dataType", "float"); cJSON_AddStringToObject(sp, "unit", "lb");
        cJSON_AddNumberToObject(sp, "min", 1); cJSON_AddNumberToObject(sp, "max", 50000); cJSON_AddNumberToObject(sp, "default", 50.0);
        cJSON_AddItemToObject(p, "span_weight", sp);
        cJSON *r = cJSON_CreateObject();
        cJSON *rsc = cJSON_CreateObject(); cJSON_AddStringToObject(rsc, "dataType", "uint32"); cJSON_AddItemToObject(r, "span_count", rsc);
        cJSON *rzc = cJSON_CreateObject(); cJSON_AddStringToObject(rzc, "dataType", "uint32"); cJSON_AddItemToObject(r, "zero_count", rzc);
        cJSON *rs  = cJSON_CreateObject(); cJSON_AddStringToObject(rs,  "dataType", "string"); cJSON_AddItemToObject(r, "status",     rs);
        cJSON *it  = s_method("CalibrateSpan", "Perform analog span calibration", p, r);
        add_cond(it, "ScaleType=Analog");
        cJSON_AddItemToArray(arr, it);
    }
    /* SmartCalibration method */
    {
        cJSON *r = cJSON_CreateObject(); cJSON *rs = cJSON_CreateObject(); cJSON_AddStringToObject(rs, "dataType", "string"); cJSON_AddItemToObject(r, "status", rs);
        cJSON *it = s_method("SmartCalibration", "Interactive DLC calibration wizard", NULL, r);
        add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it);
    }
    /* ZeroCalibration method */
    {
        cJSON *r = cJSON_CreateObject();
        cJSON *rzc = cJSON_CreateObject(); cJSON_AddStringToObject(rzc, "dataType", "uint32"); cJSON_AddItemToObject(r, "zero_count", rzc);
        cJSON *rs  = cJSON_CreateObject(); cJSON_AddStringToObject(rs,  "dataType", "string"); cJSON_AddItemToObject(r, "status",     rs);
        cJSON *it  = s_method("ZeroCalibration", "Perform DLC zero calibration", NULL, r);
        add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it);
    }
    { cJSON *it = s_typed_range("CellTrimming", "Per-cell trim factor (DLC)", "float", "RW", 0.1, 10.0, g); add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it); }
    /* SpanAdjustment method */
    {
        cJSON *p = cJSON_CreateObject(); cJSON *sp = cJSON_CreateObject();
        cJSON_AddStringToObject(sp, "dataType", "float"); cJSON_AddNumberToObject(sp, "min", 1); cJSON_AddNumberToObject(sp, "max", 50000); cJSON_AddNumberToObject(sp, "default", 50.0);
        cJSON_AddItemToObject(p, "span_weight", sp);
        cJSON *r = cJSON_CreateObject();
        cJSON *rc = cJSON_CreateObject(); cJSON_AddStringToObject(rc, "dataType", "float");  cJSON_AddItemToObject(r, "correction", rc);
        cJSON *rs = cJSON_CreateObject(); cJSON_AddStringToObject(rs, "dataType", "string"); cJSON_AddItemToObject(r, "status",     rs);
        cJSON *it = s_method("SpanAdjustment", "Interactive DLC span adjustment", p, r);
        add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it);
    }
    return arr;
}

/* ── A.4 LoadCellAssignments ─────────────────────────────────────────────── */

cJSON *menu_load_cell_assignments(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *scales[] = { "Scale1","Scale2","Scale3" };
    cJSON_AddItemToArray(arr, s_typed_range("CellID",       "Load cell hardware identifier",     "uint8", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("CellToScale",  "Scale assignment for this cell",    "RW", scales, 3, g));
    cJSON_AddItemToArray(arr, s_typed_range("CellsPerScale","Number of cells assigned to scale", "uint8", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_typed_range("CellTrim",     "Individual cell trim factor",       "float", "RW", 0.1, 10.0, g));
    return arr;
}

/* ── A.5 ComSetup ────────────────────────────────────────────────────────── */

cJSON *menu_com_setup(void)
{
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_folder("SerialPorts", "COM1, COM2, USB-B, OPC1, OPC2 serial port settings"));
    cJSON_AddItemToArray(arr, s_folder("Ethernet",    "Ethernet network settings"));
    cJSON_AddItemToArray(arr, s_folder("WiFi",        "Wi-Fi network settings"));
    cJSON_AddItemToArray(arr, s_folder("BankMode",    "Bank mode configuration"));
    {
        cJSON *it = s_folder("ISiteIP",  "iSite IP settings (DLC only)");
        add_cond(it, "ScaleType=DLC"); cJSON_AddItemToArray(arr, it);
    }
    {
        cJSON *it = s_folder("SendGross","Send gross weight output (non-DLC only)");
        add_cond(it, "ScaleType!=DLC"); cJSON_AddItemToArray(arr, it);
    }
    return arr;
}

/* ── A.6 BankMode stub (spec lists it but defines no variables) ──────────── */

static cJSON *menu_bank_mode(void)
{
    return cJSON_CreateArray();   /* intentionally empty */
}

/* ── A.5.1 SerialPorts ───────────────────────────────────────────────────── */

cJSON *menu_serial_ports(void)
{
    m225_set_menu_ctx("SerialPorts");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *stypes[] = { "SMA","SB600","Continuous","Disabled" };
    static const char *bauds[]  = { "1200","2400","4800","9600","19200","38400","57600","115200" };
    static const char *dbits[]  = { "7","8" };
    static const char *sbits[]  = { "1","2" };
    static const char *parity[] = { "None","Even","Odd" };
    static const char *tcon[]   = { "Continuous","OnPrint","OnStable","OnDemand" };
    static const char *scl[]    = { "CurrentSelected","Scale1","Scale2","Scale3" };
    static const char *mslot[]  = { "Disabled","Slot1","Slot2","Slot3" };
    static const char *ei[]     = { "RS232","RS485","RS422" };
    cJSON_AddItemToArray(arr, s_enum_item ("Type",                "Serial protocol type",           "RW", stypes, 4, g));
    cJSON_AddItemToArray(arr, s_enum_item ("BaudRate",            "Serial baud rate",               "RW", bauds,  8, g));
    cJSON_AddItemToArray(arr, s_enum_item ("DataBits",            "Number of data bits",            "RW", dbits,  2, g));
    cJSON_AddItemToArray(arr, s_enum_item ("StopBits",            "Number of stop bits",            "RW", sbits,  2, g));
    cJSON_AddItemToArray(arr, s_enum_item ("Parity",              "Parity bit setting",             "RW", parity, 3, g));
    cJSON_AddItemToArray(arr, s_enum_item ("TransferCondition",   "When to transmit data",          "RW", tcon,   4, g));
    cJSON_AddItemToArray(arr, s_enum_item ("Scale",               "Scale channel to transmit",      "RW", scl,    4, g));
    cJSON_AddItemToArray(arr, s_bool_item ("GrossOnly",           "Transmit gross weight only",     "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item ("ManualMode",          "Enable manual print mode",       "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item ("MessageSlot",         "Assigned message slot",          "RW", mslot,  4, g));
    { cJSON *it = s_enum_item("ElectricalInterface","Electrical interface standard (COM2 only)", "RW", ei, 3, g); add_cond(it, "Port=COM2"); cJSON_AddItemToArray(arr, it); }
    cJSON_AddItemToArray(arr, s_typed_range("SBHighThreshold",    "SB-series high threshold",       "uint32", "RW", 0, 0, g));
    return arr;
}

/* ── A.5.2 Ethernet (Model 225) ──────────────────────────────────────────── */

cJSON *menu_ethernet_225(void)
{
    m225_set_menu_ctx("Ethernet");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ptype[] = { "SB600","SMA","Disabled" };
    static const char *mslot[] = { "Disabled","Slot1","Slot2","Slot3" };
    cJSON_AddItemToArray(arr, s_bool_item  ("EthernetEnable",   "Enable Ethernet interface",               "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DHCP",             "Use DHCP for IP address assignment",      "RW", g));
    { cJSON *it = s_str_item("IPAddress",  "Static IP address",     "RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_str_item("Gateway",    "Default gateway address","RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_str_item("Subnet",     "Subnet mask",           "RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    cJSON_AddItemToArray(arr, s_typed_range("ServerPortA",      "Server port A", "uint16", "RW", 1, 65535, g));
    cJSON_AddItemToArray(arr, s_typed_range("ServerPortB",      "Server port B", "uint16", "RW", 1, 65535, g));
    cJSON_AddItemToArray(arr, s_typed_range("ServerPortC",      "Server port C", "uint16", "RW", 1, 65535, g));
    cJSON_AddItemToArray(arr, s_typed_range("ClientServerPort", "Client server port", "uint16", "RW", 1, 65535, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("PortType",         "Protocol type for all Ethernet ports",    "RW", ptype, 3, g));
    cJSON_AddItemToArray(arr, s_typed_range("PortThreshold",    "SB-series threshold",                    "uint32", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("MessageSlot",      "Assigned message slot",                  "RW", mslot, 4, g));
    return arr;
}

/* ── A.5.3 WiFi (Model 225) ──────────────────────────────────────────────── */

cJSON *menu_wifi_225(void)
{
    m225_set_menu_ctx("WiFi");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ptype[] = { "SB600","SMA","Disabled" };
    static const char *mslot[] = { "Disabled","Slot1","Slot2","Slot3" };
    cJSON_AddItemToArray(arr, s_bool_item  ("WiFiEnable",    "Enable Wi-Fi interface",              "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DHCP",          "Use DHCP",                            "RW", g));
    cJSON_AddItemToArray(arr, s_str_item   ("SSID",          "Wi-Fi network name",                  "RW", 32, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Password",      "Wi-Fi network password",              "RW", 32, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("PortType",      "Protocol type for all Wi-Fi ports",   "RW", ptype, 3, g));
    cJSON_AddItemToArray(arr, s_typed_range("PortThreshold", "SB-series threshold",                 "uint32", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("MessageSlot",   "Assigned message slot",               "RW", mslot, 4, g));
    return arr;
}

/* ── A.5.4 ISiteIP ───────────────────────────────────────────────────────── */

cJSON *menu_isite_ip(void)
{
    m225_set_menu_ctx("ISiteIP");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_str_item   ("SiteOrder",  "Site/Order identifier", "RW", 32, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DHCP",       "Use DHCP",              "RW", g));
    { cJSON *it = s_str_item("IPAddress", "Static IP address", "RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_str_item("Subnet",    "Subnet mask",       "RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_str_item("Gateway",   "Default gateway",   "RW", 16, g); add_cond(it, "DHCP=false"); cJSON_AddItemToArray(arr, it); }
    cJSON_AddItemToArray(arr, s_str_item   ("DNS1",       "Primary DNS server",    "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item   ("DNS2",       "Secondary DNS server",  "RW", 16, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("EnableDNS",  "Enable DNS resolution", "RW", g));
    return arr;
}

/* ── A.5.5 SendGross ─────────────────────────────────────────────────────── */

cJSON *menu_send_gross(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ports[] = { "Com1","Com2","USB","OPC1","OPC2" };
    cJSON_AddItemToArray(arr, s_bool_item ("SendGrossEnable",  "Enable gross weight output",     "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item ("GrossWeightPort",  "Port for gross weight output",   "RW", ports, 5, g));
    return arr;
}

/* ── A.6 PrinterSetup ────────────────────────────────────────────────────── */

cJSON *menu_printer_setup(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ports[] = { "Com1","Com2","USB","OPC1","OPC2" };
    cJSON_AddItemToArray(arr, s_enum_item  ("Port",               "Printer output port",                          "RW", ports, 5, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoLF",             "Auto add line feed after each line",           "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("EndingLF",           "Line feeds at end of ticket",                  "uint8",  "RW", 0, 99,  g));
    cJSON_AddItemToArray(arr, s_str_item   ("EndOfLine",          "Line terminator character (hex)",              "RW", 4,  g));
    cJSON_AddItemToArray(arr, s_str_item   ("StartOfTicket",      "String sent at start of ticket",               "RW", 32, g));
    cJSON_AddItemToArray(arr, s_str_item   ("EndOfTicket",        "String sent at end of ticket",                 "RW", 32, g));
    cJSON_AddItemToArray(arr, s_typed_range("EndOfTicketLineFeeds","Line feeds appended at end of ticket",        "uint8",  "RW", 0, 99,  g));
    cJSON_AddItemToArray(arr, s_typed_range("PrintSlot",          "Active print tab slot",                        "uint8",  "RW", 0, 0,   g));
    cJSON_AddItemToArray(arr, s_typed_item ("TimeTab",            "Print tab: time field (RR.CC = row.col)",      "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("DateTab",            "Print tab: date field",                        "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("ConsecutiveTab",     "Print tab: consecutive number field",          "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("GrossTab",           "Print tab: gross weight field",                "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("TareTab",            "Print tab: tare field",                        "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("NetTab",             "Print tab: net weight field",                  "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("GrossAccumTab",      "Print tab: gross accumulator",                 "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("NetAccumTab",        "Print tab: net accumulator",                   "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("IDTab",              "Print tab: ID field",                          "float",  "RW", g));
    return arr;
}

/* ── A.7 SystemConfig ────────────────────────────────────────────────────── */

cJSON *menu_system_config(void)
{
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_folder("Accumulators", "Gross and net accumulators per scale"));
    cJSON_AddItemToArray(arr, s_folder("DACOutput",    "Analog DAC output configuration"));
    cJSON_AddItemToArray(arr, s_folder("KeyLockout",   "Individual key lockout settings"));
    cJSON_AddItemToArray(arr, s_folder("BadgeReader",  "Badge reader port and type settings"));
    cJSON_AddItemToArray(arr, s_folder("WINVRS",       "WINVRS computer interface settings"));
    return arr;
}

/* ── A.7.1 Accumulators ──────────────────────────────────────────────────── */

cJSON *menu_accumulators(void)
{
    m225_set_menu_ctx("Accumulators");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ports[] = { "Com1","Com2","USB","OPC1","OPC2" };
    cJSON_AddItemToArray(arr, s_bool_item("GenAccums",         "Enable gross accumulator",                     "RW", g));
    cJSON_AddItemToArray(arr, s_ro_item  ("AccumulatorScale1", "Accumulated weight for Scale 1 (read only)",   "float", g));
    { cJSON *it = s_ro_item("AccumulatorScale2", "Accumulated weight for Scale 2",       "float", g); add_cond(it, "NumberOfScales >= 2"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_ro_item("AccumulatorScale3", "Accumulated weight for Scale 3",       "float", g); add_cond(it, "NumberOfScales >= 3"); cJSON_AddItemToArray(arr, it); }
    { cJSON *it = s_ro_item("TotalizerAccumulator","Multi-scale totalizer accumulator",  "float", g); add_cond(it, "Totalizer=true"); cJSON_AddItemToArray(arr, it); }
    cJSON_AddItemToArray(arr, s_str_item ("Password",          "Setup menu access password",                   "RW", 32, g));
    { cJSON *it = s_enum_item("LRPort","Remote scale communication port","RW",ports,5,g); add_cond(it,"ScaleType=Remote"); cJSON_AddItemToArray(arr, it); }
    /* ClearAccumulators method */
    {
        cJSON *r = cJSON_CreateObject(); cJSON *rc = cJSON_CreateObject(); cJSON_AddStringToObject(rc, "dataType", "bool"); cJSON_AddItemToObject(r, "cleared", rc);
        cJSON_AddItemToArray(arr, s_method("ClearAccumulators", "Clear all accumulator values", NULL, r));
    }
    return arr;
}

/* ── A.7.2 DACOutput ─────────────────────────────────────────────────────── */

cJSON *menu_dac_output(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *scales[] = { "Scale1","Scale2","Scale3" };
    cJSON_AddItemToArray(arr, s_bool_item  ("DACGross",      "Output gross weight (vs net)",             "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("DACLowWeight",  "Weight value at DAC minimum output",       "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("DACHighWeight", "Weight value at DAC maximum output",       "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("DACVoltOutput", "Maximum DAC voltage output",               "float", "RW", 0, 10.0, g));
    cJSON_AddItemToArray(arr, s_typed_item ("AdjustHigh",    "DAC high-end trim adjustment",             "int32", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("AdjustLow",     "DAC low-end trim adjustment",              "int32", "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item  ("DACScale",      "Scale channel for DAC output",             "RW", scales, 3, g));
    return arr;
}

/* ── A.7.3 KeyLockout ────────────────────────────────────────────────────── */

cJSON *menu_key_lockout(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const struct { const char *n; const char *t; } keys[] = {
        {"ZeroKeyLock",    "Lock Zero key"},    {"TareKeyLock",    "Lock Tare key"},
        {"NetKeyLock",     "Lock Net key"},     {"PrintKeyLock",   "Lock Print key"},
        {"UnitKeyLock",    "Lock Unit key"},    {"GreenKeyLock",   "Lock Green key"},
        {"KeypadLock",     "Lock entire keypad"},{"IDKeyLock",     "Lock ID key"},
        {"CountKeyLock",   "Lock Count key"},   {"MemKeyLock",     "Lock Memory key"},
        {"PresetKeyLock",  "Lock Preset key"},  {"DeleteKeyLock",  "Lock Delete key"},
        {"StartKeyLock",   "Lock Start key"},   {"DropKeyLock",    "Lock Drop key"},
        {"PauseKeyLock",   "Lock Pause key"},   {"StopKeyLock",    "Lock Stop key"},
        {"RestartKeyLock", "Lock Restart key"}, {"DumpKeyLock",    "Lock Dump key"},
    };
    for (int i = 0; i < (int)(sizeof(keys)/sizeof(keys[0])); i++)
        cJSON_AddItemToArray(arr, s_bool_item(keys[i].n, keys[i].t, "RW", g));
    return arr;
}

/* ── A.7.4 BadgeReader ───────────────────────────────────────────────────── */

cJSON *menu_badge_reader(void)
{
    m225_set_menu_ctx("BadgeReader");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ports[]  = { "Com1","Com2","USB","OPC1","OPC2" };
    static const char *rtypes[] = { "None","AWID","HID","Proximity" };
    cJSON_AddItemToArray(arr, s_enum_item  ("Reader1Port",       "Port for badge reader 1",             "RW", ports,  5, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Reader1Type",       "Badge reader 1 type",                 "RW", rtypes, 4, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Reader2Port",       "Port for badge reader 2",             "RW", ports,  5, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Reader2Type",       "Badge reader 2 type",                 "RW", rtypes, 4, g));
    cJSON_AddItemToArray(arr, s_typed_range("ThresholdWeight",   "Min weight to trigger badge read",    "float", "RW", 0, 0, g));
    { cJSON *it = s_bool_item("SiteID","Include site ID in badge output","RW",g); add_cond(it,"Reader1Type=AWID or Reader2Type=AWID"); cJSON_AddItemToArray(arr, it); }
    return arr;
}

/* ── A.7.5 WINVRS ────────────────────────────────────────────────────────── */

cJSON *menu_winvrs(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *ports[]   = { "Com1","Com2","USB","OPC1","OPC2" };
    static const char *c1modes[] = { "Disabled","Enabled","Continuous","Terminal" };
    static const char *c2modes[] = { "Disabled","Remote" };
    static const char *tmodes[]  = { "Off","RGR","GRG" };
    cJSON_AddItemToArray(arr, s_enum_item  ("Computer1Port",        "Communication port for Computer 1",       "RW", ports,   5, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Computer1Mode",        "Computer 1 operating mode",               "RW", c1modes, 4, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Computer2Port",        "Communication port for Computer 2",       "RW", ports,   5, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("Computer2Mode",        "Computer 2 operating mode",               "RW", c2modes, 2, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("PrintPassthrough",     "Pass print output through to Computer 2", "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item  ("TrafficMode",          "Traffic light control mode",              "RW", tmodes,  3, g));
    cJSON_AddItemToArray(arr, s_typed_range("TrafficOnThreshold",   "Weight to activate traffic signal",       "float", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_typed_range("TrafficOffThreshold",  "Weight to deactivate traffic signal",     "float", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_typed_range("TrafficOffDelay",      "Delay before traffic signal deactivates", "uint8", "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_str_item   ("EnterRelayCommand",    "Relay command sent on vehicle entry",     "RW", 8, g));
    cJSON_AddItemToArray(arr, s_str_item   ("ExitRelayCommand",     "Relay command sent on vehicle exit",      "RW", 8, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("TrafficDisplay",       "Show traffic status on indicator display","RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DFCEnable",            "Enable Digital Fill Control in WINVRS",   "RW", g));
    return arr;
}

/* ── A.8 ModeConfig ──────────────────────────────────────────────────────── */

cJSON *menu_mode_config(void)
{
    cJSON *arr = cJSON_CreateArray();
    static const struct { const char *n; const char *t; const char *c; } modes[] = {
        {"IDStorage",      "ID storage mode settings",              "ModeOfOperation=IDStorage"},
        {"DFC",            "Digital fill control settings",         "ModeOfOperation=DFC"},
        {"Batcher",        "Batcher mode settings",                 "ModeOfOperation=Batcher"},
        {"PackageWeigher", "Package weigher settings",              "ModeOfOperation=PackageWeigher"},
        {"AxleWeigher",    "Axle weigher settings",                 "ModeOfOperation=AxleWeigher"},
        {"CheckWeigher",   "Check weigher zone and color settings", "ModeOfOperation=CheckWeigher"},
        {"PWC",            "Preset weight comparator settings",     "ModeOfOperation=PWC"},
        {"Livestock",      "Livestock inclinometer settings",       "ModeOfOperation=Livestock"},
    };
    for (int i = 0; i < (int)(sizeof(modes)/sizeof(modes[0])); i++) {
        cJSON *it = s_folder(modes[i].n, modes[i].t);
        add_cond(it, modes[i].c);
        cJSON_AddItemToArray(arr, it);
    }
    return arr;
}

/* ── A.8.1 IDStorage ─────────────────────────────────────────────────────── */

cJSON *menu_id_storage(void)
{
    m225_set_menu_ctx("IDStorage");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_bool_item  ("WeightAlarm",    "Enable weight alarm",               "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("AlarmThreshold", "Weight alarm trigger threshold",    "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("AlarmTimeOn",    "Alarm on duration in seconds",      "uint8",  "RW", 0, 99, g));
    cJSON_AddItemToArray(arr, s_typed_range("IDCount",        "Number of ID prompts",              "uint8",  "RW", 1, 3,  g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt1",        "Label for ID prompt 1",             "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt2",        "Label for ID prompt 2",             "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt3",        "Label for ID prompt 3",             "RW", 16, g));
    return arr;
}

/* ── A.8.2 DFC ───────────────────────────────────────────────────────────── */

cJSON *menu_dfc(void)
{
    m225_set_menu_ctx("DFC");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *speeds[] = { "SingleSpeed","DualSpeed" };
    static const char *gseqs[]  = { "ABB","ABA","ABBA" };
    cJSON_AddItemToArray(arr, s_enum_item  ("Speed",        "Fill speed mode",               "RW", speeds, 2, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("GateSequence", "Gate actuation sequence",        "RW", gseqs,  3, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoTrim",     "Enable automatic trim",          "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoPrint",    "Print ticket automatically",     "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("MultiDrop",    "Enable multi-drop mode",         "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DumpGate",     "Enable dump gate",               "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoDump",     "Automatically actuate dump gate","RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("Decumulate",   "Enable decumulate mode",         "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoTare",     "Automatically tare before fill", "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("JogToCutoff",  "Jog gate to cutoff point",       "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("FastCutoff",   "Fast speed cutoff weight",       "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("FillWeight",   "Target fill weight",             "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("SlowCutoff",   "Slow speed cutoff weight",       "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("Trim",         "Fill trim correction",           "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("DropCount",    "Number of drops per cycle",      "uint8",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("ZeroTolerance","Zero acceptance tolerance",      "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("GateTimer",    "Gate actuation timer in ms",     "uint16", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("Chatter",      "Gate chatter compensation",      "float",  "RW", g));
    return arr;
}

/* ── A.8.3 Batcher ───────────────────────────────────────────────────────── */

cJSON *menu_batcher(void)
{
    m225_set_menu_ctx("Batcher");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *speeds[] = { "SingleSpeed","DualSpeed" };
    static const char *gseqs[]  = { "ABB","ABA","ABBA" };
    cJSON_AddItemToArray(arr, s_enum_item  ("Speed",         "Batcher speed mode",           "RW", speeds, 2, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("GateSequence",  "Gate actuation sequence",      "RW", gseqs,  3, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoTrim",      "Enable automatic trim",        "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("BinCount",      "Number of bins",               "uint8",  "RW", 1, 8,   g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoPrint",     "Print ticket automatically",   "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("DumpGate",      "Enable dump gate",             "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoDump",      "Automatically actuate dump",   "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("Decumulate",    "Enable decumulate mode",       "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("BatchCount",    "Number of batches to run",     "uint32", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("ZeroTolerance", "Zero acceptance tolerance",    "float",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("GateTimer",     "Gate actuation timer in ms",   "uint16", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("SettleTimer",   "Settle timer in ms",           "uint16", "RW", g));
    return arr;
}

/* ── A.8.4 PackageWeigher ────────────────────────────────────────────────── */

cJSON *menu_package_weigher(void)
{
    m225_set_menu_ctx("PackageWeigher");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_typed_range("IDCount",  "Number of ID prompts", "uint8", "RW", 1, 3, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt1",  "Label for prompt 1",   "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt2",  "Label for prompt 2",   "RW", 16, g));
    cJSON_AddItemToArray(arr, s_str_item   ("Prompt3",  "Label for prompt 3",   "RW", 16, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("RetainID", "Retain ID between weighments", "RW", g));
    return arr;
}

/* ── A.8.5 AxleWeigher ───────────────────────────────────────────────────── */

cJSON *menu_axle_weigher(void)
{
    m225_set_menu_ctx("AxleWeigher");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *modes[] = { "Auto","Manual" };
    cJSON_AddItemToArray(arr, s_enum_item  ("AWMode",           "Axle detection mode",           "RW", modes, 2, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AxlePads",         "Enable axle pad inputs",        "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("ThresholdWeight",  "Minimum weight to detect axle", "float",  "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_typed_item ("StopDelay",      "Stop delay in ms",              "uint16", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("TotalDelay",     "Total delay in ms",             "uint16", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("AxleCounter",    "Starting axle count",           "uint32", "RW", 1, 0, g));
    return arr;
}

/* ── A.8.6 CheckWeigher ──────────────────────────────────────────────────── */

cJSON *menu_check_weigher(void)
{
    m225_set_menu_ctx("CheckWeigher");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *colors[] = { "Red","Yellow","Green","Blue","Pink","White" };
    cJSON_AddItemToArray(arr, s_typed_range("Outputs",    "Number of output zones",      "uint8", "RW", 1, 5,  g));
    cJSON_AddItemToArray(arr, s_bool_item  ("AutoPrint",  "Print ticket automatically",  "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("UnderThreshold",  "Under zone upper boundary",   "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("LowOKThreshold",  "Low OK zone upper boundary",  "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("HighOKThreshold", "High OK zone upper boundary", "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("OverThreshold",   "Over zone lower boundary",    "float", "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item  ("ColorUnder",      "Display color for Under zone",    "RW", colors, 6, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("ColorLowOK",      "Display color for Low OK zone",   "RW", colors, 6, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("ColorHighOK",     "Display color for High OK zone",  "RW", colors, 6, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("ColorAcceptOK",   "Display color for Accept zone",   "RW", colors, 6, g));
    cJSON_AddItemToArray(arr, s_enum_item  ("ColorOver",       "Display color for Over zone",     "RW", colors, 6, g));
    return arr;
}

/* ── A.8.7 PWC ───────────────────────────────────────────────────────────── */

cJSON *menu_pwc(void)
{
    m225_set_menu_ctx("PWC");
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    static const char *scales[] = { "Scale1","Scale2","Scale3" };
    cJSON_AddItemToArray(arr, s_typed_range("NumberOfOutputs","Number of comparator outputs",        "uint8", "RW", 1, 0, g));
    cJSON_AddItemToArray(arr, s_bool_item  ("BalanceOnPrint", "Balance outputs on print",            "RW", g));
    cJSON_AddItemToArray(arr, s_bool_item  ("MonitorZero",    "Monitor zero between weighments",     "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("Threshold",      "Per-output trip threshold (array)",   "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("Trim",           "Per-output trim correction (array)",  "float", "RW", g));
    cJSON_AddItemToArray(arr, s_enum_item  ("OutputScale",    "Scale channel for each output",       "RW", scales, 3, g));
    return arr;
}

/* ── A.8.8 Livestock ─────────────────────────────────────────────────────── */

cJSON *menu_livestock(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_bool_item  ("Inclinometer", "Enable inclinometer tilt compensation", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("SetPitch",     "Tilt pitch angle in degrees",           "float", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_item ("SetRoll",      "Tilt roll angle in degrees",            "float", "RW", g));
    /* DefaultAngleCalibration method */
    {
        cJSON *r = cJSON_CreateObject();
        cJSON *rp = cJSON_CreateObject(); cJSON_AddStringToObject(rp, "dataType", "float");  cJSON_AddItemToObject(r, "pitch",  rp);
        cJSON *rr = cJSON_CreateObject(); cJSON_AddStringToObject(rr, "dataType", "float");  cJSON_AddItemToObject(r, "roll",   rr);
        cJSON *rs = cJSON_CreateObject(); cJSON_AddStringToObject(rs, "dataType", "string"); cJSON_AddItemToObject(r, "status", rs);
        cJSON_AddItemToArray(arr, s_method("DefaultAngleCalibration", "Run default angle calibration procedure", NULL, r));
    }
    return arr;
}

/* ── A.9 DLCSetup ────────────────────────────────────────────────────────── */

cJSON *menu_dlc_setup(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    /* CellDiagnostics method */
    {
        cJSON *r = cJSON_CreateObject(); cJSON *rc = cJSON_CreateObject(); cJSON_AddStringToObject(rc, "dataType", "string"); cJSON_AddItemToObject(r, "cell_data", rc);
        cJSON_AddItemToArray(arr, s_method("CellDiagnostics", "Real-time load cell diagnostic readings", NULL, r));
    }
    cJSON_AddItemToArray(arr, s_bool_item  ("SnapMediabox", "Enable SNAP Mediabox communication", "RW", g));
    cJSON_AddItemToArray(arr, s_typed_range("SnapID",       "SNAP network node ID",               "uint8", "RW", 0, 0,  g));
    cJSON_AddItemToArray(arr, s_typed_range("Channel",      "SNAP communication channel",         "uint8", "RW", 0, 15, g));
    return arr;
}

/* ── A.10 ReviewMenu ─────────────────────────────────────────────────────── */

cJSON *menu_review_menu(void)
{
    actor_getter_fn g = m225_actor_get;
    cJSON *arr = cJSON_CreateArray();
    cJSON_AddItemToArray(arr, s_typed_range("ScaleID",               "Scale identification number",                     "uint8",  "RW", 0, 0, g));
    cJSON_AddItemToArray(arr, s_ro_item    ("CalibrationCounter",    "Calibration event count (auto-increments)",       "uint32", g));
    cJSON_AddItemToArray(arr, s_ro_item    ("ConfigurationCounter",  "Configuration event count (auto-increments)",     "uint32", g));
    return arr;
}

/* ── menu_dispatch ───────────────────────────────────────────────────────── */

cJSON *menu_dispatch(const char *actor_name)
{
    if (!actor_name) return NULL;

    /* ESP32-S3 actors */
    if (strcmp(actor_name, "Root")   == 0) return menu_root();
    if (strcmp(actor_name, "BLE")    == 0) return menu_ble();
    if (strcmp(actor_name, "WIFI")   == 0) return menu_wifi();
    if (strcmp(actor_name, "ETH")    == 0) return menu_ethernet();
    if (strcmp(actor_name, "NTP")    == 0) return menu_ntp();
    if (strcmp(actor_name, "UART")   == 0) return menu_uart();
    if (strcmp(actor_name, "SYSTEM") == 0) return menu_system();

    /* Model 225 top-level folder */
    if (strcmp(actor_name, "195Menu")              == 0) return menu_195_menu();

    /* Model 225 Indicator actors (Appendix A) */
    if (strcmp(actor_name, "IndicatorSetup")       == 0) return menu_indicator_setup();
    if (strcmp(actor_name, "ScaleSetup")           == 0) return menu_scale_setup();
    if (strcmp(actor_name, "ScaleCalibration")     == 0) return menu_scale_calibration();
    if (strcmp(actor_name, "LoadCellAssignments")  == 0) return menu_load_cell_assignments();
    if (strcmp(actor_name, "ComSetup")             == 0) return menu_com_setup();
    if (strcmp(actor_name, "SerialPorts")          == 0) return menu_serial_ports();
    if (strcmp(actor_name, "Ethernet")             == 0) return menu_ethernet_225();
    if (strcmp(actor_name, "WiFi")                 == 0) return menu_wifi_225();
    if (strcmp(actor_name, "BankMode")             == 0) return menu_bank_mode();
    if (strcmp(actor_name, "ISiteIP")              == 0) return menu_isite_ip();
    if (strcmp(actor_name, "SendGross")            == 0) return menu_send_gross();
    if (strcmp(actor_name, "PrinterSetup")         == 0) return menu_printer_setup();
    if (strcmp(actor_name, "SystemConfig")         == 0) return menu_system_config();
    if (strcmp(actor_name, "Accumulators")         == 0) return menu_accumulators();
    if (strcmp(actor_name, "DACOutput")            == 0) return menu_dac_output();
    if (strcmp(actor_name, "KeyLockout")           == 0) return menu_key_lockout();
    if (strcmp(actor_name, "BadgeReader")          == 0) return menu_badge_reader();
    if (strcmp(actor_name, "WINVRS")               == 0) return menu_winvrs();
    if (strcmp(actor_name, "ModeConfig")           == 0) return menu_mode_config();
    if (strcmp(actor_name, "IDStorage")            == 0) return menu_id_storage();
    if (strcmp(actor_name, "DFC")                  == 0) return menu_dfc();
    if (strcmp(actor_name, "Batcher")              == 0) return menu_batcher();
    if (strcmp(actor_name, "PackageWeigher")       == 0) return menu_package_weigher();
    if (strcmp(actor_name, "AxleWeigher")          == 0) return menu_axle_weigher();
    if (strcmp(actor_name, "CheckWeigher")         == 0) return menu_check_weigher();
    if (strcmp(actor_name, "PWC")                  == 0) return menu_pwc();
    if (strcmp(actor_name, "Livestock")            == 0) return menu_livestock();
    if (strcmp(actor_name, "DLCSetup")             == 0) return menu_dlc_setup();
    if (strcmp(actor_name, "ReviewMenu")           == 0) return menu_review_menu();

    return NULL;
}
