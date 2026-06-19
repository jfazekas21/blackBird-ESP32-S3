/*
 * trimLightActor.c
 *
 *  Created on: Nov 23, 2023
 *      Author: Aniket
 */
	#include "trimLightActor.h"
	#include "actor.h"
	#include "Console_Actor.h"
	#include <math.h>
	#include <stdbool.h>
	#include "Config.h"
	#include <string.h>
	#include <strings.h>
	#include "freertos/FreeRTOS.h"
	#include "freertos/task.h"
	#include "esp_log.h"
	#include "driver/rmt_tx.h"
	#include "led_strip_encoder.h"
	#include <stdio.h>
	#include "freertos/FreeRTOS.h"
	#include "freertos/task.h"
	#include "driver/uart.h"
	#include "driver/gpio.h"
	#include "sdkconfig.h"
	#include "esp_log.h"
	#include "driver/rmt_tx.h"
    #include "driver/rmt_common.h"
	#include "esp_random.h"
 	#include "driver/mcpwm_prelude.h"
	#include <mbedtls/base64.h>
	#include "esp_heap_caps.h"

static const char *TAG = "LIGHTING";

//#define ENABLE_PRINT_MSG

#define TWO_PI        6.28318530717958647692f
#define INV_TWO_PI    (1.0f/TWO_PI)

static const float PIX2 = (2 * M_PI);
#define LIGHTING_OBJ_QUE_COUNT                100	//2

#define EXAMPLE_LED_NUMBERS         1024	//300	//256		//450		//512		//200
#define EXAMPLE_CHASE_SPEED_MS      10

#define EPS1 1e-6f

#ifndef B542
#if defined(B480) || defined (B553)
#define RMT_LED_STRIP_GPIO_NUM_CH1 GPIO_NUM_16
#define RMT_LED_STRIP_GPIO_NUM_CH2 GPIO_NUM_15
#define RMT_LED_STRIP_GPIO_NUM_CH3 GPIO_NUM_6
#define RMT_LED_STRIP_GPIO_NUM_CH4 GPIO_NUM_5
#elif defined(B543)
#define RMT_LED_STRIP_GPIO_NUM_CH1 GPIO_NUM_15
#define RMT_LED_STRIP_GPIO_NUM_CH2 GPIO_NUM_16
#define RMT_LED_STRIP_GPIO_NUM_CH3 GPIO_NUM_6
#define RMT_LED_STRIP_GPIO_NUM_CH4 GPIO_NUM_6
#elif defined(B394)
#define RMT_LED_STRIP_GPIO_NUM_CH1 GPIO_NUM_6
#define RMT_LED_STRIP_GPIO_NUM_CH2 GPIO_NUM_5
#define RMT_LED_STRIP_GPIO_NUM_CH3 GPIO_NUM_6
#define RMT_LED_STRIP_GPIO_NUM_CH4 GPIO_NUM_5
#else
#define RMT_LED_STRIP_GPIO_NUM_CH1 GPIO_NUM_16
#define RMT_LED_STRIP_GPIO_NUM_CH2 GPIO_NUM_15
#define RMT_LED_STRIP_GPIO_NUM_CH3 GPIO_NUM_16
#define RMT_LED_STRIP_GPIO_NUM_CH4 GPIO_NUM_15
#endif
#endif
#define LED_TASK_RATE (10)  //(60)	// 10 (30)
#define DRIVER_TASK_RATE (100)


#ifndef B542
#define RMT_LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define RMT_LED_STRIP_GPIO_NUM      15
#endif

#if defined(B527)
#define LIGHT1 GPIO_NUM_4
#define LIGHT2 GPIO_NUM_5
#endif


#define EXAMPLE_CHASE_SPEED_MS      10

#define CONFIG_EXAMPLE_UART_PORT_NUM 0
#define CONFIG_EXAMPLE_UART_BAUD_RATE 460800
#define CONFIG_EXAMPLE_UART_RXD 44
#define CONFIG_EXAMPLE_UART_TXD 43
#define CONFIG_EXAMPLE_TASK_STACK_SIZE 4096

#define ECHO_TEST_TXD (CONFIG_EXAMPLE_UART_TXD)
#define ECHO_TEST_RXD (CONFIG_EXAMPLE_UART_RXD)
#define ECHO_TEST_RTS (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS (UART_PIN_NO_CHANGE)

#define ECHO_UART_PORT_NUM      (CONFIG_EXAMPLE_UART_PORT_NUM)
#define ECHO_UART_BAUD_RATE     (CONFIG_EXAMPLE_UART_BAUD_RATE)
#define ECHO_TASK_STACK_SIZE    (CONFIG_EXAMPLE_TASK_STACK_SIZE)

#define Filename_Image_base		 	"IMAGE"

#define MAX_COLORS 32
#define NUMBER_OF_CHANNELS 4

#define PI                  			3.14159265

#define SPARKLE_DISTANCE_IN_INCH      	900	//768

#define MAX_COLORS_RGB_ARRAY 			100

typedef struct {
    uint16_t r;
    uint16_t g;
    uint16_t b;
} Color16;


const uint16_t max_Value = 65535;         // Maximum value for any color
const float inv_max_Value = (float)1/max_Value;         // Maximum value for any color

    float slope_R 	= 0.0f;
    float slope_G 	= 0.0f;
    float slope_B 	= 0.0f;

    float inv_slope_R 	= 0.0f;
    float inv_slope_G 	= 0.0f;
    float inv_slope_B 	= 0.0f;

    const unsigned short days_1[4][12] =
    {
       {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
       { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
       { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
       {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
    };

    const char source[] = "<LIGHTING.OFF({\"CH\":[-1] , \"Brightness\" : 100.0,\"Function\":\"HSV\" , \"Config\" : {\"HUE\" : 26.00, \"SAT\" :94.00, \"VAL\" : 100.00}})";

// Define constants at compile time to avoid recalculations
#define DECAY_CONSTANT (log(3.0f))
// Inline function to reduce function call
#define MAX_COLOR_ORDER 100	//60
//#define MAX_ACTION_ORDER 3000	//100

    PSRAM_ATTR_BSS static char payLoadDataEvtExe[MAX_JSON_PAYLOAD_BYTES];

static inline float decay_function(float time, float rate)
{
	float Dec=1.0f - expf(-DECAY_CONSTANT  *time * rate);
	if(Dec>0.98)Dec=1.00;
    return Dec;
}

bool use_ping_buffer[NUMBER_OF_CHANNELS] = {false,false,false,false};

static uint8_t JFS_Response		= 2;

static uint8_t functionNullFlag = 0;

static uint8_t command_run_at_power_up = 0;

bool channel_equal(int ch);

//------playlist-------------
// Playlist engine definitions
typedef enum {
    COMMAND_TYPE_ON = 0,
	COMMAND_TYPE_OFF,
	COMMAND_TYPE_COLOR,		// 	COMMAND_TYPE_PWM, COMMAND_TYPE_HSV, COMMAND_TYPE_colorIndex,
	COMMAND_TYPE_PATTERN,
	COMMAND_TYPE_EFFECT,		//	COMMAND_TYPE_tapeMeasure, COMMAND_TYPE_Ripple, COMMAND_TYPE_Sparkle, COMMAND_TYPE_Custom,
	COMMAND_TYPE_SCENE,
	COMMAND_TYPE_LIGHT_SHOW,
	COMMAND_TYPE_PLAYLIST,
	COMMAND_TYPE_COUNT
} CommandType;

typedef enum {
    TARGET_ALL_CHANNELS = 1,
    TARGET_SELECTED_CHANNELS,
    TARGET_VIRTUAL_GROUPS,
    TARGET_TYPE_COUNT
} PlaylistTargetType;

#define MAX_PLAYLIST_STEPS 768	//64
#define MAX_PLAYLIST_RECORDS 768	//512
#define MAX_COMMAND_ENTRIES 256	//64
#define MAX_VIRTUAL_GROUPS 32
//#define READ_VIRTUAL_TABLE_TASK_STACK_DEPTH READ_VIRTUAL_TABLE_TASK_STACK_DEPTH
#define READ_VIRTUAL_TABLE_TASK_PRIORITY READ_COMMAND_TABLE_TASK_PRIORITY

typedef struct {
    uint16_t command_id;
    uint8_t type;
    float brightness;
    uint8_t parsed_exec_kind;     /* Parsed template kind */
    void *parsed_exec_blob;       /* Snapshot of parsed template struct */
    uint16_t parsed_exec_blob_size;
    uint16_t nested_playlist_id;  /* Used when type == COMMAND_TYPE_PLAYLIST */
} CommandEntry;

typedef struct {
    uint16_t playlist_entry_id;
    uint16_t playlist_id;
    uint16_t command_id;
    uint32_t duration_ms;
    uint8_t target_type;
    uint8_t target_bitfield;
} PlaylistEntryRecord;

typedef struct {
    uint16_t command_id;
    uint32_t duration_ms;       /* absolute offset from cycle start (from Playlist_Table Duration) */
    const CommandEntry *command;
    uint8_t target_type;        /* from Playlist_Table per entry */
    uint8_t target_bitfield;
} PlaylistStep;

typedef struct {
    PlaylistStep *steps;
    uint16_t count;
    uint16_t capacity;
    uint64_t total_duration_ms;
} PlaylistSequence;

typedef struct {
    uint16_t playlist_id;
    uint64_t epocStartTime;
    uint64_t durationMsec;
    float brightness_override; /* < 0 means use Command_Table brightness per Command_ID */
    uint8_t target_type_override;
    uint8_t target_bitfield_override;
    uint8_t has_target_bitfield_override;
} PlaylistRequest;

#define MAX_ACTIVE_PLAYLISTS 25

typedef struct {
    uint8_t active;
    uint16_t playlist_id;
    uint64_t initial_start_ms;
    uint32_t total_duration_override_sec;
    float brightness_override;  /* < 0 = use command brightness from Command_Table */
    uint8_t has_target_override;
    uint8_t target_type_override;
    uint8_t target_bitfield_override;
    uint8_t has_target_bitfield_override;
    const PlaylistSequence *sequence_ref;
    uint16_t current_step_index;
    uint64_t cycle_start_ms;
    uint64_t cycle_duration_ms;  /* total cycle duration in ms */
    uint64_t next_step_start_ms; /* next absolute step time in system ms */
    PlaylistRequest request;    /* filled for executeCommand */
} PlaylistSlot;

#define MAX_PLAYLIST_CACHE 64
#define COMMAND_INDEX_TABLE_SIZE 1024

typedef struct {
    uint16_t playlist_id;
    uint8_t valid;
    PlaylistSequence sequence;
} PlaylistSequenceCacheEntry;

typedef struct {
    int id;
    char name[32];
    uint8_t channel_mask;
} VirtualGroup;

static const struct {
    int id;
    const char *name;
    uint8_t channel_mask;
} default_virtual_group_defs[] = {
    {1, "ALL", 0xFF},
    {2, "ODD", 0x55},
    {3, "EVEN", 0xAA},
    {4, "Lower Left", 0x01},
    {5, "Lower Center", 0x02},
    {6, "Lower Right", 0x03},
    {7, "Middle Left", 0x04},
    {8, "Middle Center", 0x05},
    {9, "Middle Right", 0x06},
    {10, "Upper Left", 0x07},
    {11, "Upper Center", 0x08},
    {12, "Upper Right", 0x09},
};

static PlaylistEntryRecord *playlist_records = NULL;
static size_t playlist_record_count = 0;
static size_t playlist_record_capacity = 0;
static CommandEntry *command_table = NULL;
static size_t command_table_size = 0;
static size_t command_table_capacity = 0;
PSRAM_ATTR_BSS static VirtualGroup virtual_groups[MAX_VIRTUAL_GROUPS];
static size_t virtual_group_count = 0;
PSRAM_ATTR_BSS static PlaylistSequenceCacheEntry playlist_sequence_cache[MAX_PLAYLIST_CACHE];
static size_t playlist_sequence_cache_count = 0;
static int16_t command_index_table[COMMAND_INDEX_TABLE_SIZE];

/* Up to 4 playlists (Playlist_ID 1-4) run simultaneously; each slot stores params and state */
PSRAM_ATTR_BSS static PlaylistSlot playlist_slots[MAX_ACTIVE_PLAYLISTS];
static bool playlist_started_flag = false;

/* Pending command apply: set by executeCommand so the task fills arrays first, then PrepareDataWithModeSetting runs */
static bool playlist_apply_pending[NUMBER_OF_CHANNELS];
static const CommandEntry *playlist_pending_command[NUMBER_OF_CHANNELS];
static float playlist_pending_brightness[NUMBER_OF_CHANNELS];

static void apply_playlist_command_to_channel(int Chan, const CommandEntry *command, float brightness_override);

// Context passed to callback
typedef struct {
    int channel_index;
} tx_context_t;

volatile bool tx_busy_flags[NUMBER_OF_CHANNELS] = {false};
static tx_context_t tx_contexts[NUMBER_OF_CHANNELS];
//static int numActions = 0;
static void stopPlaylist();
static void executePlaylistFunc(int Chan, uint64_t u64CurrentTime);
static void executeCommand(int command_ID, int Chan, const PlaylistRequest *request);
//static void Send_Light_off_After_Playlist(void);
static int executePlaylist(int playlist_id, float brightness_override, uint32_t total_duration_override_sec, uint64_t local_start_time_ms, bool has_target_override, PlaylistTargetType target_type_override, uint8_t target_bitfield_override, bool has_target_bitfield_override);
static const CommandEntry *find_command_entry(int command_ID);
static uint32_t resolve_target_channel_mask(PlaylistTargetType target_type, uint32_t bitfield);
static uint32_t resolve_playlist_entry_target_mask(uint8_t entry_target_type, uint8_t entry_target_bitfield, bool has_target_override, PlaylistTargetType target_type_override, bool has_target_bitfield_override, uint8_t target_bitfield_override);
static bool parse_command_row(cJSON *row, CommandEntry *entry);
static bool parse_playlist_row(cJSON *row, PlaylistEntryRecord *record);
static PlaylistTargetType parse_target_type_override(const char *value);
static cJSON *find_json_field_case_insensitive(const cJSON *object, const char *const keys[], size_t key_count);
static int json_get_int_value(const cJSON *object, const char *const keys[], size_t key_count, int default_value);
static uint8_t json_get_u8_value(const cJSON *object, const char *const keys[], size_t key_count, uint8_t default_value);
static const char *json_get_string_value(const cJSON *object, const char *const keys[], size_t key_count);
static CommandType command_type_from_value(const cJSON *value);
static bool build_playlist_sequence(int channel, const PlaylistRequest *request, PlaylistSequence *sequence, int recursion_depth);
static bool build_playlist_sequence_by_id(int playlist_id, PlaylistSequence *sequence, int recursion_depth);
static void playlist_sequence_reset(PlaylistSequence *sequence);
static bool playlist_sequence_append(PlaylistSequence *sequence, const PlaylistStep *step);
static void clear_playlist_sequence_cache(void);
static const PlaylistSequence *get_compiled_playlist_sequence(uint16_t playlist_id);
static void rebuild_command_index_table(void);
static int playlist_entry_compare(const void *a, const void *b);
static PlaylistTargetType playlist_target_type_from_row(const cJSON *row);
static bool parse_virtual_group_row(cJSON *row, VirtualGroup *group);
static void clear_virtual_groups(void);
static void clear_playlist_records(void);
static void load_default_virtual_groups(void);
static const VirtualGroup *find_virtual_group_by_id(int id);
static void queue_sql_response(QueueHandle_t queue, char *buffer, size_t buffer_size, cJSON *response);
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void log_playlist_memory_usage(const char *phase);

void Execute_RacingEffect(int Chan, uint64_t u64CurrentTime);

IRAM_ATTR static bool  rmt_tx_done_callback(rmt_channel_handle_t tx_chan,
                                 const rmt_tx_done_event_data_t *edata,
                                 void *user_ctx)
{
	tx_context_t *ctx = (tx_context_t *)user_ctx;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	tx_busy_flags[ctx->channel_index] = false;  // clear busy flag
    return xHigherPriorityTaskWoken == pdTRUE;
}

static void Racing_InitChannel(int Chan);
static void Racing_Advance(int Chan, uint64_t now_ms);

#if defined(B542)
// PWM configuration parameters
#define PWM_FREQ_HZ       178
//===========================QPOE PWM FUNCTIONS======================================================================================

static void apply_rgb_pwm_to_channel(int ch, uint16_t r16, uint16_t g16, uint16_t b16);
void config_pto(uint8_t point, uint32_t frequency, uint8_t duty, uint8_t polarity, uint8_t enable);
static inline void pto_set_duty(uint8_t point, uint32_t frequency, uint8_t duty_percent, uint8_t polarity);
//=====================================================================================================================================
#endif

typedef struct
{
    float hue;        // Hue (0-360 degrees)
    float saturation; // Saturation (0-1)
    float brightness; // Brightness (0-1)
} Color;

typedef struct {
    Color 	colorSelections[MAX_COLORS];
    Color 	paddingColor;
    int   	numColors; // Number of colors in colorSelections
    float 	colorLength;
    float 	paddingLength;
    uint8_t transitionType;
    uint8_t mirror;
    float 	mirrorPosition;
    float 	oscAmplitude;
    float 	oscPeriod;
    float 	movingSpeed;
    uint8_t spacingOverride; // 1 if lengths/speed are expressed in pixels instead of inches
} customImage;

/*** ---------------------- TUNABLES & OPTIONS ---------------------- ***/

/* Collision avoidance:
   0 = allow overlaps (blended overtakes),
   1 = follower maintains a safety gap behind leader. */


/*** ----------- LIGHTWEIGHT RNG HELPERS ----------- ***/
#if defined(ESP_PLATFORM)
  #include "esp_system.h"
  static inline uint32_t fx_rand32(void) { return esp_random(); }            // hw RNG on ESP
#else
  #include <stdlib.h>
  static inline uint32_t fx_rand32(void) { return (uint32_t)rand(); }        // fallback RNG
#endif

static inline float randf_01(void) {
    return (fx_rand32() & 0x00FFFFFF) / 16777215.0f;                          // 24-bit uniform [0..1)
}
static inline float randf_range(float a, float b) {
    return a + (b - a) * randf_01();                                          // uniform in [a..b]
}

/*** ----------- DEFAULTS (copied into params at init) ----------- ***/
// These are just defaults; you can overwrite fields at runtime before Apply.

#define DEF_ENABLE_COLLISION_AVOIDANCE     0       // 0=allow overlaps; 1=keep minimum gap
#define DEF_OVERRIDE_LED_PITCH_IN          1       // 1=force pitch_in_inches; 0=use ChannelParam
#define DEF_LED_PITCH_INCHES               3.0f    // exactly 3 inches for your build (300 LEDs)

#define DEF_FIXED_CARS                     16      // start with 16 cars on ~900" strip; tweak live
#define DEF_MAX_CARS_CAP                   32      // absolute per-channel cap (storage sized for this)

#define DEF_MIN_LEN_IN                     12.0f   // min car length in inches
#define DEF_MAX_LEN_IN                     36.0f   // max car length in inches
#define DEF_MIN_START_SPACING_IN            6.0f   // spacing between cars at t=0 (on-screen spawn)
#define DEF_REENTRY_GAP_IN                  6.0f   // off-screen gap before entering (inches)

#define DEF_MIN_SPEED_IN_S                 30.0f   // min speed in inches/sec
#define DEF_MAX_SPEED_IN_S                150.0f   // max speed in inches/sec
#define DEF_RETARGET_MIN_MS               500u     // min ms before retargeting speed
#define DEF_RETARGET_JITTER_MS           1500u     // plus [0..this] ms random per retarget
#define DEF_MAX_ACCEL_IN_S2               120.0f   // max accel magnitude in inches/sec^2

#define DEF_MIN_COLLISION_GAP_IN            6.0f   // follower head must remain behind leader tail by this

#define DEF_SPAWN_MODE                      1      // 0=all on-screen; 1=half on-screen + half off-screen
#define DEF_MAX_DT_S                        0.10f  // clamp long frame gaps

/*** ----------- RUNTIME PARAMETER STRUCT ----------- ***/
typedef struct {
    // Geometry & mapping
    uint8_t enable_collision_avoidance;    // on/off at runtime
    uint8_t override_pitch_in;             // if 1, use pitch_in_inches; else use ChannelParam mm -> inches
    float   pitch_in_inches;               // LED spacing in inches when override is enabled

    // Population
    int     fixed_cars;                    // desired number of cars
    int     max_cars_cap;                  // safety cap (<= storage capacity)

    // Car geometry & spacing
    float   min_len_in;                    // min car length
    float   max_len_in;                    // max car length
    float   min_start_spacing_in;          // spacing at t=0 for on-screen spawns
    float   reentry_gap_in;                // gap off-screen before entering

    // Speed dynamics
    float   min_speed_in_s;                // min speed
    float   max_speed_in_s;                // max speed
    uint32_t retarget_min_ms;              // min ms before retarget
    uint32_t retarget_jitter_ms;           // add [0..jitter] ms
    float   max_accel_in_s2;               // max accel/decel magnitude

    // Collision avoidance
    float   min_collision_gap_in;          // minimum head-to-tail gap

    // Spawn policy
    uint8_t spawn_mode;                    // 0=all on-screen, 1=half on-screen + half off-screen

    // Safety
    float   max_dt_s;                      // clamp long dt
    int 	num_colors;
    uint16_t colors[DEF_MAX_CARS_CAP][3]; // RGB triplets
} RacingParams;

/*** ----------- COLOR / CAR / CHANNEL STATE ----------- ***/
typedef struct {
    float hue, saturation, brightness;     // HSV (0..360, 0..100, 0..100)
} FxColor;

typedef struct {
    float    tail_in;                      // tail position (inches)
    float    length_in;                    // length in inches (constant per run)
    float    speed_in_s;                   // current speed (in/s)
    float    target_speed_in_s;            // target speed (in/s)
    uint64_t next_retarget_ms;             // next time to pick a new target speed
    FxColor  color;                        // fixed per-car color identity
    // Cached RGB for this car (avoid per-LED HSV)
    uint16_t rgb_r;                        // 16-bit per channel (your pipeline)
    uint16_t rgb_g;
    uint16_t rgb_b;
} RacingCar;

typedef struct {
    uint8_t  initialized;                  // 0=not ready, 1=ready
    RacingParams params;                   // runtime params snapshot per channel
    float    pitch_in;                     // pitch in inches (resolved)
    float    inv_pitch;                    // 1 / pitch_in (precomputed)
    float    strip_len_in;                 // active strip length in inches
    int      active_leds;                  // ChannelParamObject[Chan].SetLEDstripal_u16
    int      forward_map;                  // 1=forward, 0=reverse
    int      numCars;                      // actual number of cars
    RacingCar cars[DEF_MAX_CARS_CAP];      // storage up to cap
    uint64_t last_update_ms;               // time of last update
} RacingState;

PSRAM_ATTR_BSS static RacingState g_racing_state[NUMBER_OF_CHANNELS];
//static RacingState g_racing_state[NUMBER_OF_CHANNELS] = {0};

/*** ----------- PARAM DEFAULTS / APPLY ----------- ***/
static inline void Racing_DefaultParams(RacingParams *p) {
    p->enable_collision_avoidance = DEF_ENABLE_COLLISION_AVOIDANCE;           // copy default
    p->override_pitch_in          = DEF_OVERRIDE_LED_PITCH_IN;                // copy default
    p->pitch_in_inches            = DEF_LED_PITCH_INCHES;                     // copy default

    p->fixed_cars                 = DEF_FIXED_CARS;                           // copy default
    p->max_cars_cap               = DEF_MAX_CARS_CAP;                         // copy default

    p->min_len_in                 = DEF_MIN_LEN_IN;                           // copy default
    p->max_len_in                 = DEF_MAX_LEN_IN;                           // copy default
    p->min_start_spacing_in       = DEF_MIN_START_SPACING_IN;                 // copy default
    p->reentry_gap_in             = DEF_REENTRY_GAP_IN;                       // copy default

    p->min_speed_in_s             = DEF_MIN_SPEED_IN_S;                       // copy default
    p->max_speed_in_s             = DEF_MAX_SPEED_IN_S;                       // copy default
    p->retarget_min_ms            = DEF_RETARGET_MIN_MS;                      // copy default
    p->retarget_jitter_ms         = DEF_RETARGET_JITTER_MS;                   // copy default
    p->max_accel_in_s2            = DEF_MAX_ACCEL_IN_S2;                      // copy default

    p->min_collision_gap_in       = DEF_MIN_COLLISION_GAP_IN;                 // copy default

    p->spawn_mode                 = DEF_SPAWN_MODE;                           // copy default
    p->max_dt_s                   = DEF_MAX_DT_S;                             // copy default
}

// Maximum number of color segments allowed in the marquee effect
#define MAX_MARQUEE_COLORS 32

// Maximum number of color segments allowed in the MultiColorSparkle effect
#define MAX_MultiColorSparkle_COLORS 8

// Structure to hold one color segment (HSV and its length in inches)
typedef struct {
    float hue;           // Hue value (0..360)
    float saturation;    // Saturation (0..100)
    float brightness;    // Brightness (0..100)
    float lengthInches;  // Length of this color segment in inches
} MarqueeColor_t;

// Structure to hold the complete marquee configuration for one channel
typedef struct {
    int   numColors;             // Number of color segments used
    float totalLengthInches;     // Total length of the marquee (sum of all segments)
    MarqueeColor_t colors[MAX_MARQUEE_COLORS]; // Array of color segments
    // Additional configuration parameters (similar to your custom image settings)
    int   transitionType;   // 0 = "None" (no gradient), 1 = gradient transition
    int   enableMirror;     // Mirror effect flag (0 = off, 1 = on)
    float mirrorPosition;   // Mirror position value
    float oscAmp;           // Oscillation amplitude
    float oscPeriod;        // Oscillation period
    float movingSpeed;      // Speed of the marquee movement
    float brightnessWavelength;
    float brightnessAmplitude;
    float brightnessSpeed;
    uint8_t spacingOverride; // 1 if lengths/speed are expressed in pixels instead of inches
} marqueeImage_t;


// One marquee configuration per channel (for start and ramp/end states)
PSRAM_ATTR_BSS static marqueeImage_t marqueeImage_start[NUMBER_OF_CHANNELS];
PSRAM_ATTR_BSS static marqueeImage_t marqueeImage_end[NUMBER_OF_CHANNELS];
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static customImage ImageConfig_start[NUMBER_OF_CHANNELS];
PSRAM_ATTR_BSS static customImage ImageConfig_end[NUMBER_OF_CHANNELS];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static char payLoadData_SETCMD[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS  static char payLoadData_Running[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char payLoadData_Color_table[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Read_Color_Table_str[2500]; // Buffer for constructing SQL query strings
PSRAM_ATTR_BSS static char Read_Color_Table_buffer[2000];
PSRAM_ATTR_BSS static char payLoadData_Command_table[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Read_Command_Table_str[2500];
PSRAM_ATTR_BSS static char Read_Command_Table_buffer[2000];
PSRAM_ATTR_BSS static char payLoadData_Playlist_table[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char Read_Playlist_Table_str[2500]; // Buffer for constructing SQL query strings
PSRAM_ATTR_BSS static char Read_Playlist_Table_buffer[2000];
//PSRAM_ATTR_BSS static char payLoadData_Virtual_table[MAX_JSON_PAYLOAD_BYTES];
//PSRAM_ATTR_BSS static char Read_Virtual_Table_str[2500];
//PSRAM_ATTR_BSS static char Read_Virtual_Table_buffer[2000];
PSRAM_ATTR_BSS static char payLoadData_Virtual_table[1000];
PSRAM_ATTR_BSS static char Read_Virtual_Table_str[1000];
PSRAM_ATTR_BSS static char Read_Virtual_Table_buffer[1000];
PSRAM_ATTR_BSS static char line_Cmd[COMMAND_LEN];
PSRAM_ATTR_BSS static char line_Set_Cmd[COMMAND_LEN];
PSRAM_ATTR_BSS static char line_setLastCommand[COMMAND_LEN];
PSRAM_ATTR_BSS static char actor_prop_val_a8[COMMAND_LEN];
//PSRAM_ATTR_BSS static char apply_playlist_command_to_channel_buffer[COMMAND_LEN];

PSRAM_ATTR_BSS static char direct_array_testing_str[COMMAND_LEN];

PSRAM_ATTR_BSS static StackType_t  xTaskStack5[TRIM_SUBTASK_STACK_DEPTH]; //xTaskStack1[TRIM_SUBTASK_STACK_DEPTH],

static int	gmt_val = 0;
static uint8_t	dst_val = 0;

typedef struct
{
    float amplitude;
    float wavelength;  // in feet
    float speed;       // in feet per second
} SineWave;

enum
{
	IC_TM1914_A = 0,
	IC_TM1934_IC,
	IC_UCS8903,
	IC_WS2812B
};

typedef struct
{
	char  Name[32];
	uint8_t	ColorIndex;
	double  Hue;
	double Saturation;
	double Value;

}Struct_COLOR_TABLE;

typedef struct {
    int id;               // Corresponds to the "Id" column
    char name[50];        // Corresponds to the "Name" column (adjusted size for longer names)
    int colorIndex;       // Corresponds to the "ColorIndex" column
    int hue;              // Corresponds to the "Hue" column
    int saturation;       // Corresponds to the "Saturation" column
    int value;            // Corresponds to the "Value" column
} color_t;

#if defined(B542)
const color_t default_colors[] = {
    {1, "Red", 0, 0, 100, 100},
    {2, "Fire", 1, 2, 100, 100},
    {3, "Pumpkin", 2, 6, 100, 100},
    {4, "Amber", 3, 15, 100, 100},
    {5, "Tangerine", 4, 19, 100, 100},
    {6, "Merigold", 5, 24, 100, 100},
    {7, "Sunset", 6, 30, 100, 100},
    {8, "Yellow", 7, 46, 100, 100},
    {9, "Lime", 8, 69, 100, 100},
    {10, "Light Green", 9, 89, 100, 100},
    {11, "Green", 10, 120, 100, 100},
    {12, "Sea Foam", 11, 138, 100, 100},
    {13, "Turquoise", 12, 195, 100, 100},
    {14, "Ocean", 13, 202, 100, 100},
    {15, "Deep Blue", 14, 240, 100, 100},
    {16, "Violet", 15, 274, 100, 100},
    {17, "Purple", 16, 290, 100, 100},
    {18, "Lavender", 17, 300, 100, 100},
    {19, "Pink", 18, 300, 100, 100},
    {20, "Hot Pink", 19, 317, 100, 100},
    {21, "5000", 20, 21, 51, 100},
    {22, "4700", 21, 21, 55, 100},
    {23, "4100", 22, 27, 61, 100},
    {24, "4000", 23, 30, 71, 100},
    {25, "3700", 24, 33, 79, 100},
    {26, "3500", 25, 33, 82, 100},
    {27, "3000", 26, 31, 89, 100},
    {28, "2700", 27, 31, 92, 100},
    {29, "Red", 30, 0, 100, 100},
    {30, "Fire", 31, 3, 100, 100},
    {31, "Pumpkin", 32, 6, 100, 100},
    {32, "Orange", 33, 12, 100, 100},
    {33, "Sunset", 34, 16, 100, 100},
    {34, "Merigold", 35, 24, 100, 100},
    {35, "Lemon", 36, 32, 100, 100},
    {36, "Yellow", 37, 40, 100, 100},
    {37, "Lime", 38, 55, 100, 100},
    {38, "Light Green", 39, 67, 100, 100},
    {39, "Pale Green", 40, 80, 100, 100},
    {40, "Apple Green", 41, 107, 100, 100},
    {41, "Emerald", 42, 115, 100, 100},
    {42, "Green", 43, 120, 100, 100},
    {43, "Sea Foam", 44, 128, 100, 100},
    {44, "Artic", 45, 137, 100, 100},
    {45, "Aqua", 46, 152, 100, 100},
    {46, "Sky", 47, 172, 100, 100},
    {47, "Water", 48, 200, 100, 100},
    {48, "Light Blue", 49, 220, 100, 100},
    {49, "Sapphire", 50, 227, 100, 100},
    {50, "Deep Blue", 51, 240, 100, 100},
    {51, "Royal Blue", 52, 249, 100, 100},
    {52, "Orchid", 53, 260, 100, 100},
    {53, "Purple", 54, 273, 100, 100},
    {54, "Lavender", 55, 280, 100, 100},
    {55, "Lilac", 56, 292, 100, 100},
    {56, "Pink", 57, 310, 100, 100},
    {57, "Bubblegum", 58, 316, 100, 100},
    {58, "Flamingo", 59, 325, 100, 100},
    {59, "Hot Pink", 60, 341, 100, 100},
    {60, "Deep Pink", 61, 346, 100, 100},
};
#else
const color_t default_colors[] = {
    {1, "Red", 0, 0, 100, 100},
    {2, "Fire", 1, 1, 100, 100},
    {3, "Pumpkin", 2, 4, 100, 100},
    {4, "Amber", 3, 10, 100, 100},
    {5, "Tangerine", 4, 12, 100, 100},
    {6, "Merigold", 5, 15, 100, 100},
    {7, "Sunset", 6, 21, 100, 100},
    {8, "Yellow", 7, 40, 100, 100},
    {9, "Lime", 8, 77, 100, 100},
    {10, "Light Green", 9, 101, 100, 100},
    {11, "Green", 10, 120, 100, 100},
    {12, "Sea Foam", 11, 125, 100, 100},
    {13, "Turquoise", 12, 164, 100, 100},
    {14, "Ocean", 13, 180, 100, 100},
    {15, "Deep Blue", 14, 240, 100, 100},
    {16, "Violet", 15, 285, 100, 100},
    {17, "Purple", 16, 314, 100, 100},
    {18, "Lavender", 17, 326, 100, 100},
    {19, "Pink", 18, 330, 100, 100},
    {20, "Hot Pink", 19, 343, 100, 100},
    {21, "5000", 20, 37, 70, 100},
    {22, "4700", 21, 33, 73, 100},
    {23, "4100", 22, 35, 75, 100},
    {24, "4000", 23, 32, 82, 100},
    {25, "3700", 24, 31, 86, 100},
    {26, "3500", 25, 31, 87, 100},
    {27, "3000", 26, 26, 92, 100},
    {28, "2700", 27, 26, 94, 100},
    {29, "Red", 30, 0, 100, 100},
    {30, "Fire", 31, 1, 100, 100},
    {31, "Pumpkin", 32, 3, 100, 100},
    {32, "Orange", 33, 7, 100, 100},
    {33, "Sunset", 34, 11, 100, 100},
    {34, "Merigold", 35, 15, 100, 100},
    {35, "Lemon", 36, 23, 100, 100},
    {36, "Yellow", 37, 32, 100, 100},
    {37, "Lime", 38, 53, 100, 100},
    {38, "Light Green", 39, 75, 100, 100},
    {39, "Pale Green", 40, 92, 100, 100},
    {40, "Apple Green", 41, 113, 100, 100},
    {41, "Emerald", 42, 117, 100, 100},
    {42, "Green", 43, 120, 100, 100},
    {43, "Sea Foam", 44, 122, 100, 100},
    {44, "Artic", 45, 125, 100, 100},
    {45, "Aqua", 46, 130, 100, 100},
    {46, "Sky", 47, 143, 100, 100},
    {47, "Water", 48, 176, 100, 100},
    {48, "Light Blue", 49, 210, 100, 100},
    {49, "Sapphire", 50, 220, 100, 100},
    {50, "Deep Blue", 51, 240, 100, 100},
    {51, "Royal Blue", 52, 250, 100, 100},
    {52, "Orchid", 53, 264, 100, 100},
    {53, "Purple", 54, 280, 100, 100},
    {54, "Lavender", 55, 292, 100, 100},
    {55, "Lilac", 56, 317, 100, 100},
    {56, "Pink", 57, 335, 100, 100},
    {57, "Bubblegum", 58, 342, 100, 100},
    {58, "Flamingo", 59, 346, 100, 100},
    {59, "Hot Pink", 60, 354, 100, 100},
    {60, "Deep Pink", 61, 356, 100, 100},
};

#endif

PSRAM_ATTR_BSS static Struct_COLOR_TABLE Color_table[MAX_COLOR_ORDER];
// Semaphore handle
SemaphoreHandle_t binary_semaphore;

const sAddressableStrip sAddressableStipArr[NUMBER_OF_CHIPS_SUPPORTED] =
{
    // Array of sAddressableStrip structures initialized with NUMBER_OF_CHIPS_SUPPORTED elements
    // Each element represents the characteristics of an addressable LED strip
    // The elements are initialized with curly braces {}
    // Each element contains values for the fields defined in sAddressableStrip structure

	// Element 0 (IC_TM1914_A):
	{
		// Duration for bit 0 when sending 0
		.f32bit0Duration0 = 0.3f,
		// Duration for bit 0 when sending 1
		.f32bit0Duration1 = 0.9f,
		// Duration for bit 1 when sending 0
		.f32bit1Duration0 = 0.9f,
		// Duration for bit 1 when sending 1
		.f32bit1Duration1 = 0.3f,
		// Duration for the reset bit when sending 0
		.f32bitResetDuration0 = 50.0f,
		// Duration for the reset bit when sending 1
		.f32bitResetDuration1 = 50.0f,
		// Flag to toggle the output
		.u8FlagToggleOP = 1,
		// Number of bits (initialized to 0)
		.u8NumberOfbits = 0,
		// Mode setting (initialized to 6)
		.u8ModeSetting = 6
	},
    // Element 1	(IC_TM1934_IC):
    {
        // Initialize with default values similar to Element 1
	        // Duration for bit 0 when sending 0
	        .f32bit0Duration0 = 0.35f,
	        // Duration for bit 0 when sending 1
	        .f32bit0Duration1 = 0.9f,
	        // Duration for bit 1 when sending 0
			.f32bit1Duration0 = 0.72f,
			// Duration for bit 1 when sending 1
			.f32bit1Duration1 = 0.53f,
			// Duration for the reset bit when sending 0
			.f32bitResetDuration0 = 200.0f,
			// Duration for the reset bit when sending 1
			.f32bitResetDuration1 = 200.0f,
			// Flag to toggle the output
			.u8FlagToggleOP = 1,
			// Number of bits (initialized to 0)
			.u8NumberOfbits = 0,
			// Mode setting (initialized to 6)
			.u8ModeSetting = 6
    },

    // Element 2	(IC_UCS8903):
    {
        // Initialize with default values similar to Element 2
	        // Duration for bit 0 when sending 0
	        .f32bit0Duration0 = 0.4f,
	        // Duration for bit 0 when sending 1
	        .f32bit0Duration1 = 0.85f,
	        // Duration for bit 1 when sending 0
	        .f32bit1Duration0 = 0.8f,
	        // Duration for bit 1 when sending 1
	        .f32bit1Duration1 = 0.45f,
	        // Duration for the reset bit when sending 0
	        .f32bitResetDuration0 = 1000.0f,
	        // Duration for the reset bit when sending 1
	        .f32bitResetDuration1 = 1000.0f,
	        // Flag to toggle the output
	        .u8FlagToggleOP = 0,
	        // Number of bits (initialized to 0)
	        .u8NumberOfbits = 1,
			// Mode setting (initialized to 6)
			.u8ModeSetting = 0
    },

	// Element 3	(IC_WS2812B):
	{
		// Initialize with default values similar to Element 2
			// Duration for bit 0 when sending 0
			.f32bit0Duration0 = 0.30f,
			// Duration for bit 0 when sending 1
			.f32bit0Duration1 = 0.79f,
			// Duration for bit 1 when sending 0
			.f32bit1Duration0 = 0.79f,
			// Duration for bit 1 when sending 1
			.f32bit1Duration1 = 0.32f,
			// Duration for the reset bit when sending 0
			.f32bitResetDuration0 = 280.0f,
			// Duration for the reset bit when sending 1
			.f32bitResetDuration1 = 280.0f,
			// Flag to toggle the output
			.u8FlagToggleOP = 0,
			// Number of bits (initialized to 0)
			.u8NumberOfbits = 0,
			// Mode setting (initialized to 0)
			.u8ModeSetting = 0
	}
};

volatile uint8_t IC_Type_Var;

#define MODE_SETTING	0

#define MAX_PAYLOAD_SIZE COMMAND_LEN

#define InOneFeetmm 305

static const char * THIS_ACTOR = "LIGHTING";	//"TRIM";		//"LIGHT";
static const char 			THIS_ACTOR_ID 	= 	LIGHTING;	//TRIM;  // assign src id
BaseType_t lightMonitor,LightRunningMonitor;
QueueHandle_t light_Rx_Queue,vColor_Table_Que =NULL;
QueueHandle_t vCommand_Table_Que = NULL;
QueueHandle_t vPlaylist_Table_Que = NULL;
QueueHandle_t vVirtual_Table_Que = NULL;
TaskHandle_t lightHandle= NULL,LightRunningHandle0 = NULL,LightRunningHandle1= NULL,LightRunningHandle2= NULL,LightRunningHandle3= NULL,LightRunningHandle4= NULL,ReadColorTable_Handle=NULL;
TaskHandle_t ReadCommandTable_Handle = NULL;
TaskHandle_t ReadPlaylistTable_Handle = NULL;
TaskHandle_t ReadVirtualTable_Handle = NULL;
static StaticTask_t xTRIMTaskBuffer, xTRIMSubTaskBuffer4;
static StaticTask_t xReadColorTableTaskBuffer;
static StaticTask_t xReadCommandTableTaskBuffer;
static StaticTask_t xReadPlaylistTableTaskBuffer;
static StaticTask_t xReadVirtualTableTaskBuffer;
PSRAM_ATTR_BSS static StackType_t xReadColorTableTaskStack2[READ_COLOR_TABLE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xReadCommandTableTaskStack[READ_COMMAND_TABLE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xReadPlaylistTableTaskStack[READ_PLAYLIST_TABLE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xReadVirtualTableTaskStack[READ_VIRTUAL_TABLE_TASK_STACK_DEPTH];
static StaticQueue_t ReadColtab_pxQueueBuffer, Monitor_pxQueueBuffer, ReadCommand_pxQueueBuffer, ReadPlaylist_pxQueueBuffer, ReadVirtual_pxQueueBuffer;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [LIGHTING_OBJ_QUE_COUNT * sizeof(AMessage_st)];
PSRAM_ATTR_BSS static uint8_t ReadColtab_pucQueueStorage [100 * 1000];
PSRAM_ATTR_BSS static uint8_t ReadCommand_pucQueueStorage [READ_SQL_TABLE_QUEUE_DEPTH * READ_SQL_TABLE_QUEUE_ENTRY_SIZE];
PSRAM_ATTR_BSS static uint8_t ReadPlaylist_pucQueueStorage [READ_SQL_TABLE_QUEUE_DEPTH * READ_SQL_TABLE_QUEUE_ENTRY_SIZE];
PSRAM_ATTR_BSS static uint8_t ReadVirtual_pucQueueStorage [READ_SQL_TABLE_V_QUEUE_DEPTH * READ_SQL_TABLE_QUEUE_ENTRY_SIZE];
static  uint32_t delay_same_array = 1;
static volatile uint8_t pendingRestoreLastCommand[NUMBER_OF_CHANNELS] = {0};
static uint64_t led_refresh_hold_start_ms = 0;
static uint64_t get_current_time_ms();
static uint32_t get_led_refresh_timer_ms(void);
static void update_led_refresh_timer_state(uint64_t current_time_ms);
static AMessage_st s_Message_Tx;
static int FirstlightEntry = 0;
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Get_Property(AMessage_st* s_Message_Rx);
static void configure_rmt_channel(gpio_num_t gpio_num, rmt_channel_handle_t *channel);
static void configure_rmt_channel_encoder(rmt_encoder_handle_t *encoder, rmt_channel_handle_t *channel);
static void configure_and_enable_rmt_channels(int i) ;
static int send_rmt_data(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder, uint16_t *data, size_t size);
static inline void set_led_color(uint8_t channels,uint16_t position,uint16_t red,uint16_t green,uint16_t blue) ;
static void LEdRunningmonitor(void *pvParameters __attribute__((unused))) ;
static void LEdRunningmonitor4(void *pvParameters __attribute__((unused))) ;
static void Read_Color_Table(void *pvParameters __attribute__((unused)));
static void Read_Command_Table(void *pvParameters __attribute__((unused)));
static void Read_Playlist_Table(void *pvParameters __attribute__((unused)));
static void Read_Virtual_Table(void *pvParameters __attribute__((unused)));
static void setLastCommand( int Chan, int BsaveFlag);
static void OnPower_setLastC( int Chan);
//static int setColor(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int hueSat(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
static int trim_On(AMessage_st* s_Message_Rx);
static int get_rgb_value(AMessage_st* s_Message_Rx);
static int get_channel_status(AMessage_st* s_Message_Rx);
static int put_array(AMessage_st* s_Message_Rx);

static void Send_D2C(void);
static void Set_Command_Property(uint8_t Chan);
//static int tapeMeasure(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int colorIndex(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int ExecuteMarquee(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int executeCustom(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int executeRacing(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);

//static int pattern(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int sparkle(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int multicolorsparkle(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
//static int ripple(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem);
static int parse_pattern_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_pattern_template(AMessage_st* s_Message_Rx, int channel);
static int parse_huesat_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_huesat_template(AMessage_st* s_Message_Rx, int channel);
static int parse_sparkle_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_sparkle_template(AMessage_st* s_Message_Rx, int channel);
static int parse_multicolorsparkle_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_multicolorsparkle_template(AMessage_st* s_Message_Rx, int channel);
static int parse_ripple_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_ripple_template(AMessage_st* s_Message_Rx, int channel);
static int parse_executecustom_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_executecustom_template(AMessage_st* s_Message_Rx, int channel);
static int parse_executeracing_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_executeracing_template(AMessage_st* s_Message_Rx, int channel);
static int parse_setcolor_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_setcolor_template(AMessage_st* s_Message_Rx, int channel);
static int parse_tapemeasure_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_tapemeasure_template(AMessage_st* s_Message_Rx, int channel);
static int parse_colorindex_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_colorindex_template(AMessage_st* s_Message_Rx, int channel);
static int parse_executemarquee_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem);
static int execute_executemarquee_template(AMessage_st* s_Message_Rx, int channel);
static int trimLightOFF(AMessage_st* s_Message_Rx);
static void StripChanOFF(uint8_t Chan);
static void saveLastCommand(AMessage_st* s_Message_Rx, int channel, float brightness, float duration, char *source, char *function, cJSON *configItem, char *changeReason, uint64_t timechange, char state1, int optionalChangeID);
static int executeFunction(AMessage_st* s_Message_Rx, int channel, char *function, float *brightness, cJSON *configItem);
static int processChannel(AMessage_st* s_Message_Rx, int channel, float brightness, float duration, char *source, char *function, cJSON *configItem, char *changeReason, uint64_t timechange, cJSON *brightnessItem, cJSON *brightnessIndexItem, cJSON *durationItem, cJSON *sourceItem, cJSON *functionItem, char state1, int optionalChangeID);
static void loadFromLastCommand(int channel, float *brightness, float *duration, char **source, char **function, cJSON **configItem);
static inline int generate_random(int min, int max);
static float Marquee_Moving_Tap_Offset(int Chan, uint64_t u64CurrentTime, int fill_data);
static float Moving_Tap_Offset(int Chan, uint64_t u64CurrentTime, int fill_data);
static void decayledProc(int Chan, uint64_t u64CurrentTime, int fill_data);
static void MultiColordecayledProc(int Chan, uint64_t u64CurrentTime, int fill_data);
static void patternExecuteProc(int Chan, uint64_t u64CurrentTime, int fill_data);
void rgb16_to_hsv(uint16_t R, uint16_t G, uint16_t B, float *H, float *S, float *V);
//void ScalingOnImage(float scale, int channels);
void PrepareDataWithModeSetting(int offset, int Chan, int Start_Offset_Flag);
void Execute_PrepareDataWithModeSetting(float offset, int Chan, int Start_Offset_Flag, int fill_data);
void ExecuteMarquee_PrepareDataWithModeSetting(float offset1, int Chan, int Start_Offset_Flag, uint64_t u32CurrentTime, int fill_data);
void RampAndDwellFunction(int Chan, uint64_t u64CurrentTime);
void MirrorData(int Pos, int Chan);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
void hsv_to_rgb_16bit(float h, float s, float v, uint16_t *r, uint16_t *g, uint16_t *b);
void interpolate_color(Color *result, const Color *start, const Color *end, float t);
void initializeSparkleParameters(int numChannels);
void initializeMultiColorSparkleParameters(int numChannels);
void freeSparkleParameters();
void freeMultiColorSparkleParameters();

void RippleContinious(int Chan, uint64_t u32CurrentTime, int fill_data);
static void TurnFlagsOff(int channel);
float  fast_sinf(float x);
static inline void swap_buffers(int i);
static inline void restrict_and_scale_RGB(uint16_t *r, uint16_t *g, uint16_t *b, float v);
static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch);
static void set_gmt_dst(AMessage_st* s_Message_Rx);

static const char *const command_id_keys[] = {"Command_ID", "CommandId", "CommandID", "command_id", "commandid"};
static const char *const command_type_keys[] = {"Command_Type", "CommandType", "command_type", "commandtype"};
static const char *const brightness_keys[] = {"Brightness", "brightness"};
static const char *const command_payload_keys[] = {"Command_Payload", "CommandPayload", "Payload"};
static const char *const playlist_entry_id_keys[] = {"PlaylistEntryID", "Playlist_Entry_ID", "PlaylistEntry_Id", "playlist_entry_id"};
static const char *const playlist_id_keys[] = {"Playlist_ID", "PlaylistID", "playlist_id"};
static const char *const playlist_duration_keys[] = {"Duration", "duration", "DurationMs", "Duration_MS", "Duration_msec", "Duration_ms", "DurationMsec"};
static const char *const playlist_target_type_keys[] = {"Target_Type", "TargetType", "target_type", "TargetTypeOverride"};
static const char *const playlist_target_bitfield_keys[] = {"Target_Bitfield", "TargetBitField", "target_bitfield", "TargetBitfield"};
static const char *const virtual_group_id_keys[] = {"VirtualGroupID", "Virtual_Group_ID", "VirtualGroup_Id", "Id", "ID"};
static const char *const virtual_group_name_keys[] = {"Name", "VirtualGroupName", "Virtual_Group_Name", "GroupName"};
static const char *const virtual_group_mask_keys[] = {"Channel_Mask", "ChannelMask", "ChannelMaskValue", "Channel_Mask_Value"};
static const char *const virtual_group_row_keys[] = {"Channel_Mask", "ChannelMask", "VirtualGroupID"};

#ifdef B527
static void Init_Lights(void);
#endif

uint8_t source_save[20];        //

//----------------------
typedef struct {
	uint64_t lastCommandTime;
    cJSON *config;
    float brightness;
    float duration;
    char  state;
    char *source;
    char *function;
    char *payload; // Dynamically allocated payload
    char changeReason[MAX_CMD_LEN];
    uint32_t optionalChangeID;
    uint64_t timeChanged;
} LastCommand_t;
//----------------------

PSRAM_ATTR_BSS static struct Light_parameter
{
	uint16_t 	SetLEDstripalCh1_u16;
	uint16_t 	SetLEDstripalCh2_u16;
	uint16_t 	SetLEDstripalCh3_u16;
	uint16_t 	SetLEDstripalCh4_u16;
	uint8_t 	ICType_u8;
	uint8_t 	revDirCh1_u8;
	float 		LEDspacingCh1_float;	//mmPerSegCh1_float;
	float 		scaleCh1_float;
	float 		offsetCh1_float;
	uint8_t 	revDirCh2_u8;
	float 		LEDspacingCh2_float;
	float 		scaleCh2_float;
	float 		offsetCh2_float;
	uint8_t 	revDirCh3_u8;
	float 		LEDspacingCh3_float;
	float 		scaleCh3_float;
	float 		offsetCh3_float;
	uint8_t 	revDirCh4_u8;
	float 		LEDspacingCh4_float;
	float 		scaleCh4_float;
	float 		offsetCh4_float;
	float 		contrMaxB_float;
	float 		chan1MaxB_float;
	float 		chan2MaxB_float;
	float 		chan3MaxB_float;
	float 		chan4MaxB_float;
	uint8_t 	chan1LastCommand[COMMAND_LEN];
	uint8_t 	chan2LastCommand[COMMAND_LEN];
	uint8_t 	chan3LastCommand[COMMAND_LEN];
	uint8_t 	chan4LastCommand[COMMAND_LEN];
    float 		slope_R_float;
    float 		slope_G_float;
    float 		slope_B_float;
    float 		max_R_float;
    float 		max_G_float;
    float 		max_B_float;
    float 		max_Total_float;
    float 		locationOffset_float;
    float 		locationMidpoint_float;
    uint16_t    ledRefreshTimerSec_u16;
} light_para;

typedef struct
{
	uint16_t SetLEDstripal_u16;
	uint8_t revDirCh_u8;
	float  	LEDspacingCh_float;
	float 	scaleCh_float;
	float 	offsetCh_float;
	uint8_t speedrevDirCh_u8;
}ChannelParameters;

ChannelParameters ChannelParamObject[NUMBER_OF_CHANNELS];

typedef struct
{
	float 	StartColor_float[3];
	float 	EndColor_float[3];

	float 	Intensity;
	float 	Width;
	float 	Decaytime;
	uint64_t *u64CurrentTime;
	uint64_t u64RandomGenTime;
}SparkleParameters;

SparkleParameters *SparkleParamObject_start = NULL, *SparkleParamObject_end = NULL;

typedef struct
{
    int   numColors;             // Number of color segments used

	uint16_t 	MultiColor1_uint16[3];
	uint16_t 	MultiColor2_uint16[3];
	uint16_t 	MultiColor3_uint16[3];
	uint16_t 	MultiColor4_uint16[3];
	uint16_t 	MultiColor5_uint16[3];
	uint16_t 	MultiColor6_uint16[3];
	uint16_t 	MultiColor7_uint16[3];
	uint16_t 	MultiColor8_uint16[3];

	uint16_t 	EndColor_uint16[3];

	float 	Intensity;
	float 	Width;
	float 	Decaytime;
	uint64_t *u64CurrentTime;
	uint64_t u64RandomGenTime;
	uint8_t *u8ColorNum;
}MultiColorSparkleParameters;

MultiColorSparkleParameters *MultiColorSparkleParamObject_start = NULL, *MultiColorSparkleParamObject_end = NULL;

//---------------------------------
// Structure to store ramp data
typedef struct {
    float hue_start[EXAMPLE_LED_NUMBERS], sat_start[EXAMPLE_LED_NUMBERS], val_start[EXAMPLE_LED_NUMBERS];  // Initial HSV values
    float hue_end[EXAMPLE_LED_NUMBERS], sat_end[EXAMPLE_LED_NUMBERS], val_end[EXAMPLE_LED_NUMBERS];        // Target HSV values
    uint32_t RampTimeSceneVal;                // ramp time
    uint32_t DwellTimeSceneVal;               // dwell time

    uint64_t RampStartTime;
    char function_start[20], function_end[20];
    uint16_t r_start[EXAMPLE_LED_NUMBERS], g_start[EXAMPLE_LED_NUMBERS], b_start[EXAMPLE_LED_NUMBERS];  // Initial rgb values
    uint16_t r_end[EXAMPLE_LED_NUMBERS], g_end[EXAMPLE_LED_NUMBERS], b_end[EXAMPLE_LED_NUMBERS];        // Target rgb values

} RampData_t;

PSRAM_ATTR static RampData_t rampData[NUMBER_OF_CHANNELS];  // Store ramp data for each channel

//-----------------------------

PSRAM_ATTR static uint64_t initCommandTime = 0;

PSRAM_ATTR static uint64_t powerUpCommandTime[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static int One_LED_time[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int One_LED_time_back[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static uint8_t enableMirror_uint8[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int MirrorLedNum[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static int oscP_Flag[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static uint64_t oscStart_time[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static int oscOffsetMax[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int oscOffsetMin[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int oscOffset[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int oscOffset_forward[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int oscOffset_back[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static int commandID_Channel[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};


PSRAM_ATTR static uint64_t duration_Start_time[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static uint32_t duration_time[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static float brightness_RunTimeChan[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static uint8_t Last_CommandFlag[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static uint8_t Power_Cycle[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR LastCommand_t light_LastCommandPara[NUMBER_OF_CHANNELS];

PSRAM_ATTR static uint16_t NewPattern_u16Hue_start[NUMBER_OF_CHANNELS][3];
PSRAM_ATTR float periodPattern_float_start[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static uint64_t pattern_Start_time_start[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR static uint16_t NewPattern_u16Hue_end[NUMBER_OF_CHANNELS][3];
PSRAM_ATTR float periodPattern_float_end[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};
PSRAM_ATTR static uint64_t pattern_Start_time_end[NUMBER_OF_CHANNELS] = {0, 0, 0, 0};

PSRAM_ATTR_BSS static char ON_com[COMMAND_LEN];

#ifndef B542
static int tx_gpio_number[NUMBER_OF_CHANNELS] = {RMT_LED_STRIP_GPIO_NUM_CH1, RMT_LED_STRIP_GPIO_NUM_CH2,RMT_LED_STRIP_GPIO_NUM_CH3,RMT_LED_STRIP_GPIO_NUM_CH4};
#endif

static rmt_channel_handle_t tx_channels[NUMBER_OF_CHANNELS];

rmt_encoder_handle_t led_encoders[NUMBER_OF_CHANNELS] = {NULL};

PSRAM_ATTR_BSS static uint16_t data_channels1_1[(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING) * 2];
PSRAM_ATTR_BSS static  uint16_t data_channels1_2[(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING) * 2];
PSRAM_ATTR_BSS static uint16_t data_channels1_3[(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING) * 2];
PSRAM_ATTR_BSS static uint16_t data_channels1_4[(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING) * 2];

#define CHANNEL_BUFFER_WORDS  ((EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING) * 2)
//size_t bytes_per_channel = 200; //CHANNEL_BUFFER_WORDS * sizeof(uint16_t);
size_t bytes_per_channel[NUMBER_OF_CHANNELS] = {200,200,200,200}; //CHANNEL_BUFFER_WORDS * sizeof(uint16_t);

PSRAM_ATTR_BSS static StackType_t xTaskStack[TRIM_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static  uint16_t data_channels[NUMBER_OF_CHANNELS][(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING)*2];
PSRAM_ATTR_BSS static uint16_t data_channels_ping[NUMBER_OF_CHANNELS][(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING)*2];

static const uint16_t default_data_16[3] = {0, 0, 0};	//{0xFFFF, 0, 0};		//Red

// Sine wave parameters for three different waves
PSRAM_ATTR_BSS SineWave waves_start[NUMBER_OF_CHANNELS][3],waves1_start[NUMBER_OF_CHANNELS][3];
PSRAM_ATTR_BSS Color startColor_start[NUMBER_OF_CHANNELS], peakColor_start[NUMBER_OF_CHANNELS], valleyColor_start[NUMBER_OF_CHANNELS];

PSRAM_ATTR_BSS SineWave waves_end[NUMBER_OF_CHANNELS][3],waves1_end[NUMBER_OF_CHANNELS][3];
PSRAM_ATTR_BSS Color startColor_end[NUMBER_OF_CHANNELS], peakColor_end[NUMBER_OF_CHANNELS], valleyColor_end[NUMBER_OF_CHANNELS];

PSRAM_ATTR uint8_t RippleStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t SparkleStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t MultiColorSparkleStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t PatternStartFlag_start[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t ExecuteCustomStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t RacingStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t TapeMeasureStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint16_t TapeMeasureStartFlag_offset[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint16_t ExecuteCustomStartFlag_offset[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint16_t ExecuteCustomStartFlag_offset1[NUMBER_OF_CHANNELS]={0,0,0,0};

PSRAM_ATTR uint8_t  MarqueeExecuteCustomStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};

PSRAM_ATTR uint16_t MarqueeCustomStartFlag_offset[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint16_t MarqueeCustomStartFlag_offset1[NUMBER_OF_CHANNELS]={0,0,0,0};

PSRAM_ATTR uint8_t HueSatStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t setColorStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t colorIndexStartFlag[NUMBER_OF_CHANNELS]={0,0,0,0};
PSRAM_ATTR uint8_t ExecuteSceneRampFlag[NUMBER_OF_CHANNELS]={0,0,0,0};

PSRAM_ATTR uint8_t PatternStartFlag_end[NUMBER_OF_CHANNELS]={0,0,0,0};

PSRAM_ATTR float ImageSize_forMode[NUMBER_OF_CHANNELS]={0,0,0,0};

static uint8_t flag_not_rmt = 0;
static uint8_t Power_up_counter_d2c = 0;


static uint8_t flag_direct_array_testing = 0;
static uint8_t flag_direct_array_testing_2 = 0;

SemaphoreHandle_t rmt_mutexes[NUMBER_OF_CHANNELS];

SemaphoreHandle_t xPowerUpMutex[NUMBER_OF_CHANNELS];
SemaphoreHandle_t xLightCommandMutex[NUMBER_OF_CHANNELS];

// install sync manager
rmt_sync_manager_handle_t synchro = NULL;

void init_data_channels(); 

SemaphoreHandle_t dataChannelsSemaphoreNew[NUMBER_OF_CHANNELS];

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &light_para.SetLEDstripalCh1_u16, "SET_LED_S_CH1",     U_INT16, "RW", "Command to set number of LED in a strip (Maximum upto EXAMPLE_LED_NUMBERS = 512)" },
	{ &light_para.SetLEDstripalCh2_u16, "SET_LED_S_CH2",     U_INT16, "RW", "Command to set number of LED in a strip (Maximum upto EXAMPLE_LED_NUMBERS = 512)" },
	{ &light_para.SetLEDstripalCh3_u16, "SET_LED_S_CH3",     U_INT16, "RW", "Command to set number of LED in a strip (Maximum upto EXAMPLE_LED_NUMBERS = 512)" },
	{ &light_para.SetLEDstripalCh4_u16, "SET_LED_S_CH4",     U_INT16, "RW", "Command to set number of LED in a strip (Maximum upto EXAMPLE_LED_NUMBERS = 512)" },
    { &light_para.ICType_u8,            "ICTYPE",            U_INT8,  "RW", "IC_TM1914_A - 0 IC_TM1934_IC - 1 IC_UCS8903 - 2 IC_WS2812B - 3" },
    { &light_para.revDirCh1_u8,         "REVDIRCH1",         U_INT8,  "RW", "Reverse direction flag for channel 1" },
    { &light_para.LEDspacingCh1_float,  "LEDSPACINGCH1",     FLOAT,   "RW", "In inch per segment for channel 1" },
    { &light_para.scaleCh1_float,       "SCALECH1",          FLOAT,   "RW", "Scaling factor for channel 1" },
    { &light_para.offsetCh1_float,      "OFFSETCH1",         FLOAT,   "RW", "Offset value for channel 1 in inch" },
    { &light_para.revDirCh2_u8,         "REVDIRCH2",         U_INT8,  "RW", "Reverse direction flag for channel 2" },
    { &light_para.LEDspacingCh2_float,  "LEDSPACINGCH2",     FLOAT,   "RW", "In inch per segment for channel 2" },
    { &light_para.scaleCh2_float,       "SCALECH2",          FLOAT,   "RW", "Scaling factor for channel 2" },
    { &light_para.offsetCh2_float,      "OFFSETCH2",         FLOAT,   "RW", "Offset value for channel 2 in inch" },
    { &light_para.revDirCh3_u8,         "REVDIRCH3",         U_INT8,  "RW", "Reverse direction flag for channel 3" },
    { &light_para.LEDspacingCh3_float,  "LEDSPACINGCH3",     FLOAT,   "RW", "In inch per segment for channel 3" },
    { &light_para.scaleCh3_float,       "SCALECH3",          FLOAT,   "RW", "Scaling factor for channel 3" },
    { &light_para.offsetCh3_float,      "OFFSETCH3",         FLOAT,   "RW", "Offset value for channel 3 in inch" },
    { &light_para.revDirCh4_u8,         "REVDIRCH4",         U_INT8,  "RW", "Reverse direction flag for channel 4" },
    { &light_para.LEDspacingCh4_float,  "LEDSPACINGCH4",     FLOAT,   "RW", "In inch per segment for channel 4" },
    { &light_para.scaleCh4_float,       "SCALECH4",          FLOAT,   "RW", "Scaling factor for channel 4" },
    { &light_para.offsetCh4_float,      "OFFSETCH4",         FLOAT,   "RW", "Offset value for channel 4 in inch" },
	{ &light_para.contrMaxB_float,      "CONTRMAXB",         FLOAT,   "RW", "Controller Max Brightness for  all channel in range 0-100%" },
	{ &light_para.chan1MaxB_float,      "CHAN1MAXB",         FLOAT,   "RW", "channel Max Brightness for  channel 1 in range 0-100%" },
	{ &light_para.chan2MaxB_float,      "CHAN2MAXB",         FLOAT,   "RW", "channel Max Brightness for  channel 2 in range 0-100%" },
	{ &light_para.chan3MaxB_float,      "CHAN3MAXB",         FLOAT,   "RW", "channel Max Brightness for  channel 3 in range 0-100%" },
	{ &light_para.chan4MaxB_float,      "CHAN4MAXB",         FLOAT,   "RW", "channel Max Brightness for  channel 4 in range 0-100%" },
    { &light_para.chan1LastCommand,  	"CHAN1LASTCOMMAND",  STRING,  "RW", "Last command for channel 1" },
	{ &light_para.chan2LastCommand,  	"CHAN2LASTCOMMAND",  STRING,  "RW", "Last command for channel 2" },
	{ &light_para.chan3LastCommand,  	"CHAN3LASTCOMMAND",  STRING,  "RW", "Last command for channel 3" },
	{ &light_para.chan4LastCommand,  	"CHAN4LASTCOMMAND",  STRING,  "RW", "Last command for channel 4" },
    { &light_para.slope_R_float, 		"SLOPE_R",     		 FLOAT,   "RW", "Slope for red channel" },
	{ &light_para.slope_G_float, 		"SLOPE_G",      	 FLOAT,   "RW", "Slope for green channel" },
	{ &light_para.slope_B_float, 		"SLOPE_B",     		 FLOAT,   "RW", "Slope for blue channel" },
	{ &light_para.max_R_float, 			"MAX_R",     		 FLOAT,   "RW", "Maximum Current for red channel" },
    { &light_para.max_G_float, 			"MAX_G",    		 FLOAT,   "RW", "Maximum Current for green channel" },
	{ &light_para.max_B_float, 			"MAX_B",     		 FLOAT,   "RW", "Maximum Current for blue channel" },
	{ &light_para.max_Total_float, 		"MAX_TOTAL",     	 FLOAT,   "RW", "Maximum Current for total (R + G + B) channel" },
	{ &light_para.locationOffset_float, "LOCOFFSET",         FLOAT,   "RW", "Location Offset value for all channel in inch" },
	{ &light_para.locationMidpoint_float, "MIDPOFFSET",      FLOAT,   "RW", "Midpoint Offset value for all channel in inch for Mirror effects" },
	{ &light_para.ledRefreshTimerSec_u16, "LED_REFRESH_TIMER", U_INT16, "RW", "LED refresh timer in seconds for resending solid-color channel data" }
};
static uint64_t get_current_time_ms() {
	 return get_current_rtc_time_ms(0);
}

static uint32_t get_led_refresh_timer_ms(void)
{
    uint32_t refresh_seconds = light_para.ledRefreshTimerSec_u16;
    if (refresh_seconds == 0) {
        refresh_seconds = 300;
    }
    return refresh_seconds * 1000u;
}

static void update_led_refresh_timer_state(uint64_t current_time_ms)
{
    if (flag_not_rmt == 0) {
        led_refresh_hold_start_ms = 0;
        return;
    }

    if (led_refresh_hold_start_ms == 0) {
        led_refresh_hold_start_ms = current_time_ms;
        return;
    }

    uint64_t elapsed_ms = (current_time_ms >= led_refresh_hold_start_ms)
        ? (current_time_ms - led_refresh_hold_start_ms)
        : (led_refresh_hold_start_ms - current_time_ms);

    if (elapsed_ms >= get_led_refresh_timer_ms()) {
        delay_same_array = 1;
        flag_not_rmt = 0;
        led_refresh_hold_start_ms = 0;
    }
}

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx)
{
	uint8_t parameter_found = 0; // Flag to check if actor is found
#ifdef ENABLE_PRINT_MSG
	printf("\n\n proper:%s,val:%s",property,value);
#endif
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++)
	{
		if (!strcmp(property, prop[i].str_name))
		{
#ifdef ENABLE_PRINT_MSG
			printf("\n\n property match str name");
#endif
			if (!strcmp(prop[i].access, "RW"))
			{
#ifdef ENABLE_PRINT_MSG
				printf("\n\n RW");
#endif
				parameter_found = 1; // Set flag to indicate actor is found
				switch (prop[i].type)
				{
#ifdef ENABLE_PRINT_MSG
				printf("\n\n proper type:%d",prop[i].type);
#endif
					case U_INT8:
					{
						uint8_t revDirCh1_u8_temp = 0, revDirCh2_u8_temp = 0, revDirCh3_u8_temp = 0, revDirCh4_u8_temp = 0;
						int k3 = 0;

						if(!strcmp(property, "REVDIRCH1"))
						{
							revDirCh1_u8_temp = light_para.revDirCh1_u8;	//save old values
						}

						if(!strcmp(property, "REVDIRCH2"))
						{
							revDirCh2_u8_temp = light_para.revDirCh2_u8;	//save old values
						}

						if(!strcmp(property, "REVDIRCH3"))
						{
							revDirCh3_u8_temp = light_para.revDirCh3_u8;	//save old values
						}

						if(!strcmp(property, "REVDIRCH4"))
						{
							revDirCh4_u8_temp = light_para.revDirCh4_u8;	//save old values
						}

						*(uint8_t*) prop[i].name = atoi(value);

						if(!strcmp(property, "REVDIRCH1"))
						{
							ChannelParamObject[0].revDirCh_u8 = light_para.revDirCh1_u8;

							//compare old vs new value if change than execute last command
							if( revDirCh1_u8_temp != light_para.revDirCh1_u8 )
							{
								k3 = 0;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "REVDIRCH2"))
						{
							ChannelParamObject[1].revDirCh_u8 = light_para.revDirCh2_u8;

							//compare old vs new value if change than execute last command
							if( revDirCh2_u8_temp != light_para.revDirCh2_u8 )
							{
								k3 = 1;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "REVDIRCH3"))
						{
							ChannelParamObject[2].revDirCh_u8 = light_para.revDirCh3_u8;

							//compare old vs new value if change than execute last command
							if( revDirCh3_u8_temp != light_para.revDirCh3_u8 )
							{
								k3 = 2;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "REVDIRCH4"))
						{
							ChannelParamObject[3].revDirCh_u8 = light_para.revDirCh4_u8;

							//compare old vs new value if change than execute last command
							if( revDirCh4_u8_temp != light_para.revDirCh4_u8 )
							{
								k3 = 3;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}
					}
					break;

					case U_INT16:
					{
						*(uint16_t*) prop[i].name = atoi(value);

						if(!strcmp(property, "SET_LED_S_CH1"))
						{
							if(light_para.SetLEDstripalCh1_u16 > EXAMPLE_LED_NUMBERS)		//Maximum Led
							{
								light_para.SetLEDstripalCh1_u16 = EXAMPLE_LED_NUMBERS;
							}

							ChannelParamObject[0].SetLEDstripal_u16 = light_para.SetLEDstripalCh1_u16;
						}
						if(!strcmp(property, "SET_LED_S_CH2"))
						{
							if(light_para.SetLEDstripalCh2_u16 > EXAMPLE_LED_NUMBERS)		//Maximum Led
							{
								light_para.SetLEDstripalCh2_u16 = EXAMPLE_LED_NUMBERS;
							}

							ChannelParamObject[1].SetLEDstripal_u16 = light_para.SetLEDstripalCh2_u16;
						}
						if(!strcmp(property, "SET_LED_S_CH3"))
						{
							if(light_para.SetLEDstripalCh3_u16 > EXAMPLE_LED_NUMBERS)		//Maximum Led
							{
								light_para.SetLEDstripalCh3_u16 = EXAMPLE_LED_NUMBERS;
							}

							ChannelParamObject[2].SetLEDstripal_u16 = light_para.SetLEDstripalCh3_u16;
						}
						if(!strcmp(property, "SET_LED_S_CH4"))
						{
							if(light_para.SetLEDstripalCh4_u16 > EXAMPLE_LED_NUMBERS)	//768)		//Maximum Led
							{
								light_para.SetLEDstripalCh4_u16 = EXAMPLE_LED_NUMBERS;	//768;
							}

							ChannelParamObject[3].SetLEDstripal_u16 = light_para.SetLEDstripalCh4_u16;
						}
					}
					break;

					case U_INT32:
						*(uint32_t*) prop[i].name = atoi(value);
						break;

					case U_INT64:
						*(uint64_t*) prop[i].name = strtoll(value, NULL, 10);

						break;

					case INT:
						*(int*) prop[i].name = atoi(value);
						break;

					case FLOAT:
					{
						float scaleCh1_float_temp = 0, scaleCh2_float_temp = 0, scaleCh3_float_temp = 0, scaleCh4_float_temp = 0;
						float offsetCh1_float_temp = 0, offsetCh2_float_temp = 0, offsetCh3_float_temp = 0, offsetCh4_float_temp = 0;

						float LEDspacingCh1_float_temp = 0, LEDspacingCh2_float_temp = 0, LEDspacingCh3_float_temp = 0, LEDspacingCh4_float_temp = 0;
						int k3 = 0;

						if(!strcmp(property, "SCALECH1"))
						{
							scaleCh1_float_temp = light_para.scaleCh1_float;	//save old values
						}
						if(!strcmp(property, "SCALECH2"))
						{
							scaleCh2_float_temp = light_para.scaleCh2_float;	//save old values
						}
						if(!strcmp(property, "SCALECH3"))
						{
							scaleCh3_float_temp = light_para.scaleCh3_float;	//save old values
						}
						if(!strcmp(property, "SCALECH4"))
						{
							scaleCh4_float_temp = light_para.scaleCh4_float;	//save old values
						}

						//Ofset
						if(!strcmp(property, "OFFSETCH1"))
						{
							offsetCh1_float_temp = light_para.offsetCh1_float;	//save old values
						}
						if(!strcmp(property, "OFFSETCH2"))
						{
							offsetCh2_float_temp = light_para.offsetCh2_float;	//save old values
						}
						if(!strcmp(property, "OFFSETCH3"))
						{
							offsetCh3_float_temp = light_para.offsetCh3_float;	//save old values
						}
						if(!strcmp(property, "OFFSETCH4"))
						{
							offsetCh4_float_temp = light_para.offsetCh4_float;	//save old values
						}

						//LED spacing
						if(!strcmp(property, "LEDSPACINGCH1"))
						{
							LEDspacingCh1_float_temp = light_para.LEDspacingCh1_float;	//save old values
						}
						if(!strcmp(property, "LEDSPACINGCH2"))
						{
							LEDspacingCh2_float_temp = light_para.LEDspacingCh2_float;	//save old values
						}
						if(!strcmp(property, "LEDSPACINGCH3"))
						{
							LEDspacingCh3_float_temp = light_para.LEDspacingCh3_float;	//save old values
						}
						if(!strcmp(property, "LEDSPACINGCH4"))
						{
							LEDspacingCh4_float_temp = light_para.LEDspacingCh4_float;	//save old values
						}

						*(float*) prop[i].name = atof(value);
						if(!strcmp(property, "SCALECH1"))
						{
							ChannelParamObject[0].scaleCh_float = light_para.scaleCh1_float;
#ifdef ENABLE_PRINT_MSG
							printf("ChannelParamObject[0]scaleCh_float = %f \n", ChannelParamObject[0].scaleCh_float);
#endif
							//compare old vs new value if change than execute last command
							if( scaleCh1_float_temp != light_para.scaleCh1_float )
							{
								k3 = 0;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "OFFSETCH1"))
						{
							ChannelParamObject[0].offsetCh_float = light_para.offsetCh1_float;

							//compare old vs new value if change than execute last command
							if( offsetCh1_float_temp != light_para.offsetCh1_float )
							{
								k3 = 0;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "SCALECH2"))
						{
							ChannelParamObject[1].scaleCh_float = light_para.scaleCh2_float;

							//compare old vs new value if change than execute last command
							if( scaleCh2_float_temp != light_para.scaleCh2_float )
							{
								k3 = 1;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "OFFSETCH2"))
						{
							ChannelParamObject[1].offsetCh_float = light_para.offsetCh2_float;

							//compare old vs new value if change than execute last command
							if( offsetCh2_float_temp != light_para.offsetCh2_float )
							{
								k3 = 1;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "SCALECH3"))
						{
							ChannelParamObject[2].scaleCh_float = light_para.scaleCh3_float;

							//compare old vs new value if change than execute last command
							if( scaleCh3_float_temp != light_para.scaleCh3_float )
							{
								k3 = 2;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "OFFSETCH3"))
						{
							ChannelParamObject[2].offsetCh_float = light_para.offsetCh3_float;

							//compare old vs new value if change than execute last command
							if( offsetCh3_float_temp != light_para.offsetCh3_float )
							{
								k3 = 2;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "SCALECH4"))
						{
							ChannelParamObject[3].scaleCh_float = light_para.scaleCh4_float;

							//compare old vs new value if change than execute last command
							if( scaleCh4_float_temp != light_para.scaleCh4_float )
							{
								k3 = 3;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "OFFSETCH4"))
						{
							ChannelParamObject[3].offsetCh_float = light_para.offsetCh4_float;

							//compare old vs new value if change than execute last command
							if( offsetCh4_float_temp != light_para.offsetCh4_float )
							{
								k3 = 3;
								if(command_run_at_power_up == 0)
								setLastCommand(k3, 2);
							}
						}

						if(!strcmp(property, "LEDSPACINGCH1"))
						{
							if(light_para.LEDspacingCh1_float != 0)
							{
								ChannelParamObject[0].LEDspacingCh_float = (light_para.LEDspacingCh1_float)*25.4;	//In Inch LED segment

								//compare old vs new value if change than execute last command
								if( LEDspacingCh1_float_temp != light_para.LEDspacingCh1_float )
								{
									k3 = 0;
									if(command_run_at_power_up == 0)
									setLastCommand(k3, 2);
								}
							}
							else  // Restore light_para.LEDspacingCh1_float value if it is 0
							{
								light_para.LEDspacingCh1_float = LEDspacingCh1_float_temp;
							}
						}

						if(!strcmp(property, "LEDSPACINGCH2"))
						{
							if(light_para.LEDspacingCh2_float != 0)
							{
								ChannelParamObject[1].LEDspacingCh_float = (light_para.LEDspacingCh2_float)*25.4;	//In Inch LED segment

								//compare old vs new value if change than execute last command
								if( LEDspacingCh2_float_temp != light_para.LEDspacingCh2_float )
								{
									k3 = 1;
									if(command_run_at_power_up == 0)
									setLastCommand(k3, 2);
								}
							}
							else  // Restore light_para.LEDspacingCh2_float value if it is 0
							{
								light_para.LEDspacingCh2_float = LEDspacingCh2_float_temp;
							}
						}

						if(!strcmp(property, "LEDSPACINGCH3"))
						{
							if(light_para.LEDspacingCh3_float != 0)
							{
								ChannelParamObject[2].LEDspacingCh_float = (light_para.LEDspacingCh3_float)*25.4;	//In Inch LED segment

								//compare old vs new value if change than execute last command
								if( LEDspacingCh3_float_temp != light_para.LEDspacingCh3_float )
								{
									k3 = 2;
									if(command_run_at_power_up == 0)
									setLastCommand(k3, 2);
								}
							}
							else  // Restore light_para.LEDspacingCh3_float value if it is 0
							{
								light_para.LEDspacingCh3_float = LEDspacingCh3_float_temp;
							}
						}

						if(!strcmp(property, "LEDSPACINGCH4"))
						{
							if(light_para.LEDspacingCh4_float != 0)
							{
								ChannelParamObject[3].LEDspacingCh_float = (light_para.LEDspacingCh4_float)*25.4;	//In Inch LED segment

								//compare old vs new value if change than execute last command
								if( LEDspacingCh4_float_temp != light_para.LEDspacingCh4_float )
								{
									k3 = 3;

									if(command_run_at_power_up == 0)
									setLastCommand(k3, 2);
								}
							}
							else  // Restore light_para.LEDspacingCh4_float value if it is 0
							{
								light_para.LEDspacingCh4_float = LEDspacingCh4_float_temp;
							}
						}

						if(!strcmp(property, "SLOPE_R"))
						{
							slope_R 	= light_para.slope_R_float;
							if(slope_R != 0.0f)
							inv_slope_R 	= 1/slope_R;
						}

						if(!strcmp(property, "SLOPE_G"))
						{
							slope_G 	= light_para.slope_G_float;
							if(slope_G != 0.0f)
						    inv_slope_G 	= 1/slope_G;
						}

						if(!strcmp(property, "SLOPE_B"))
						{
							slope_B 	= light_para.slope_B_float;
							if(slope_B != 0.0f)
						    inv_slope_B 	= 1/slope_B;
						}

					}
					break;

					case STRING:
					{
						strcpy((char*) prop[i].name, value);

						if(command_run_at_power_up == 1)
						{
							int i = 0;
							if(!strcmp(property, "CHAN1LASTCOMMAND"))
							{
								i = 0;

								if (xSemaphoreTake(xPowerUpMutex[i], portMAX_DELAY) == pdTRUE)
								{
									powerUpCommandTime[i] = get_current_time_ms(); // Update power up command time in milliseconds
									xSemaphoreGive(xPowerUpMutex[i]);
								}
							}
							if(!strcmp(property, "CHAN2LASTCOMMAND"))
							{
								i = 1;

								if (xSemaphoreTake(xPowerUpMutex[i], portMAX_DELAY) == pdTRUE)
								{
									powerUpCommandTime[i] = get_current_time_ms(); // Update power up command time in milliseconds
									xSemaphoreGive(xPowerUpMutex[i]);
								}
							}
							if(!strcmp(property, "CHAN3LASTCOMMAND"))
							{
								i = 2;

								if (xSemaphoreTake(xPowerUpMutex[i], portMAX_DELAY) == pdTRUE)
								{
									powerUpCommandTime[i] = get_current_time_ms(); // Update power up command time in milliseconds
									xSemaphoreGive(xPowerUpMutex[i]);
								}
							}
							if(!strcmp(property, "CHAN4LASTCOMMAND"))
							{
								i = 3;

								if (xSemaphoreTake(xPowerUpMutex[i], portMAX_DELAY) == pdTRUE)
								{
									powerUpCommandTime[i] = get_current_time_ms(); // Update power up command time in milliseconds
									xSemaphoreGive(xPowerUpMutex[i]);
								}
							}
						}
					}
						break;

					default:
						break;
				}
			}
			else
			{
				return 2;
			}
		}
	}

	if(parameter_found)
		return 1;
	else
		return 0;
}//	set

static void get(char *str_prop, char *val_a8)
{
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);

	for (int i = 0; i < no_of_elements; i++)
	{
		if (!strcmp(str_prop, prop[i].str_name))
		{
			switch (prop[i].type)
			{

				case U_INT8:
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
					break;

				case INT:
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case FLOAT:
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(val_a8, prop[i].name);
					break;

				default:
					break;
			}
		}
	}
}//	get

static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx)
{
	cJSON *out_JSON  = cJSON_CreateObject();
	cJSON_AddStringToObject(out_JSON, "FILE_NAME", "A:/System/LIGHTING.json");

	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++)
	{
		switch (prop[i].type)
		{
			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
				break;


			case U_INT64:
				sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);

				break;

			case INT:
				sprintf(val_a8, "%d", *(int*) prop[i].name);
				break;

			case FLOAT:
				sprintf(val_a8, "%f", *(float*) prop[i].name);
				break;

			case STRING:
				strcpy(val_a8, prop[i].name);
				break;

			default: // printf("\n trim default prop[i].str_name= %s\n",prop[i].str_name);
				break;
		}
		if ((strcmp(prop[i].str_name, "CHAN1LASTCOMMAND") != 0) && (strcmp(prop[i].str_name, "CHAN2LASTCOMMAND") != 0) && (strcmp(prop[i].str_name, "CHAN3LASTCOMMAND") != 0) && (strcmp(prop[i].str_name, "CHAN4LASTCOMMAND") != 0))
		{
			cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
		}
	}
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
	cJSON_Delete(out_JSON);
}
// Create a queue with storage in PSRAM

//-----------------

static void init(void *a, void *b)
{
	if (FirstlightEntry == 0)
	{
		for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
		{
			rmt_mutexes[i] = xSemaphoreCreateMutex();
			if (rmt_mutexes[i] == NULL)
			{
				ESP_LOGE("initialize_mutexes", "Failed to create mutex for channel %d", i);
			}

			xPowerUpMutex[i] = xSemaphoreCreateMutex();
			if (xPowerUpMutex[i] == NULL) {
				// Handle error for powerUpCommandTime mutex creation
			}

			xLightCommandMutex[i] = xSemaphoreCreateMutex();
			if (xLightCommandMutex[i] == NULL) {
				// Handle error for light_LastCommandPara mutex creation
			}
		}

		int i= 0;

	    // Initialize the dynamic structures
	    initializeSparkleParameters(NUMBER_OF_CHANNELS);
		initializeMultiColorSparkleParameters(NUMBER_OF_CHANNELS);
		for(i=0;i<NUMBER_OF_CHANNELS;i++)
		{
			dataChannelsSemaphoreNew[i] = xSemaphoreCreateMutex();
			if (dataChannelsSemaphoreNew[i] == NULL)
			{
				// Handle semaphore creation failure
				ESP_LOGE(TAG, "Semaphore creation failed");
			}

		}

		/*	*********************************
		 * Create queues here
		 */

	    if (light_Rx_Queue == NULL) {
	    	light_Rx_Queue = xQueueCreateStatic(LIGHTING_OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
	        if (light_Rx_Queue == NULL) {
	    #ifdef ENABLE_PRINT_MSG
	            printf("LIGHTING RX Queue is not created.\n ");
	    #endif
	        }
	    }
        lightHandle = xTaskCreateStaticPinnedToCore(
                                monitor,                 // Task function
                                "LIGHTING Monitor",            // Task name
								TRIM_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								TRIM_TASK_PRIORITY,                       // Task priority
								xTaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xTRIMTaskBuffer,             // Pointer to task control block
								0
				);


		if (lightHandle == NULL)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
                // Handle error
            }

		light_para.SetLEDstripalCh1_u16 = EXAMPLE_LED_NUMBERS; //300 led in a strip
		light_para.SetLEDstripalCh2_u16 = EXAMPLE_LED_NUMBERS; //300 led in a strip
		light_para.SetLEDstripalCh3_u16 = EXAMPLE_LED_NUMBERS; //300 led in a strip
		light_para.SetLEDstripalCh4_u16 = EXAMPLE_LED_NUMBERS;	//768; //300 led in a strip

		light_para.ICType_u8 = IC_UCS8903;	//IC_TM1914_A;	//IC_WS2812B	//IC_TM1914_A;	//IC_TM1934_IC;	//IC_UCS8903;	//1;	//update as per ic type

		IC_Type_Var = light_para.ICType_u8;

		light_para.revDirCh1_u8 = 0;		//false;
		light_para.LEDspacingCh1_float = 3;	//In Inch LED segment
		light_para.scaleCh1_float = 1.000;
		light_para.offsetCh1_float = 0.000;

		light_para.revDirCh2_u8 = 0;		//false;
		light_para.LEDspacingCh2_float = 3;	//In Inch LED segment
		light_para.scaleCh2_float = 1.000;
		light_para.offsetCh2_float = 0.000;

		light_para.revDirCh3_u8 = 0;		//false;
		light_para.LEDspacingCh3_float = 3;	//In Inch LED segment
		light_para.scaleCh3_float = 1.000;
		light_para.offsetCh3_float = 0.000;

		light_para.revDirCh4_u8 = 0;		//false;
		light_para.LEDspacingCh4_float = 3;	//In Inch LED segment
		light_para.scaleCh4_float = 1.000;
		light_para.offsetCh4_float = 0.000;

		//Default Brightness
		light_para.contrMaxB_float = 100.000;
		light_para.chan1MaxB_float = 100.000;
		light_para.chan2MaxB_float = 100.000;
		light_para.chan3MaxB_float = 100.000;
		light_para.chan4MaxB_float = 100.000;

		//Default R, G, B, Total Value
	    light_para.slope_R_float 	= 12.671669;
		light_para.slope_G_float 	= 7.339063;
		light_para.slope_B_float 	= 12.76286;
		light_para.max_R_float	 	= 8.75; //7;
	    light_para.max_G_float		= 8;  	//6.5;
		light_para.max_B_float		= 8.5; 	// 7;
		light_para.max_Total_float	= 8.8; 	//7.5;

		light_para.locationOffset_float = 0.000;
		light_para.locationMidpoint_float = 0.000;
		light_para.ledRefreshTimerSec_u16 = 3;

		slope_R 	= light_para.slope_R_float;
		slope_G 	= light_para.slope_G_float;
		slope_B 	= light_para.slope_B_float;

		if(slope_R != 0.0f)
	    inv_slope_R 	= 1/slope_R;

		if(slope_G != 0.0f)
	    inv_slope_G 	= 1/slope_G;

		if(slope_B != 0.0f)
	    inv_slope_B 	= 1/slope_B;

		for(i=0; i<NUMBER_OF_CHANNELS; i++)
		{
			ChannelParamObject[i].SetLEDstripal_u16 = EXAMPLE_LED_NUMBERS;
			ChannelParamObject[i].revDirCh_u8 = 0;
			ChannelParamObject[i].speedrevDirCh_u8 = 0;
			ChannelParamObject[i].LEDspacingCh_float = (3)*25.4;	//In Inch LED segment

			ChannelParamObject[i].scaleCh_float = 1.000;
			ChannelParamObject[i].offsetCh_float = 0.000;

			SparkleParamObject_start[i].Width= 0;
			SparkleParamObject_end[i].Width= 0;
			MultiColorSparkleParamObject_start[i].Width= 0;
			MultiColorSparkleParamObject_end[i].Width= 0;
			TurnFlagsOff(i+1);
		    RacingState *st = &g_racing_state[i];                                   // channel state
		    st->initialized = 0;                                                    // if not ready
		}
		size_t num_colors = sizeof(default_colors) / sizeof(default_colors[0]);

		for(i = 0; i < num_colors; i++)
		{
			strcpy(Color_table[i].Name, default_colors[i].name);
			Color_table[i].ColorIndex = default_colors[i].colorIndex;
			Color_table[i].Hue = default_colors[i].hue;
			Color_table[i].Saturation = default_colors[i].saturation;
			Color_table[i].Value = default_colors[i].value;
		}

		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor

		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/LIGHTING.json");
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		cJSON_Delete(responseObject);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		vTaskDelay(100 / portTICK_PERIOD_MS);
		
		cJSON *responseObject_new = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject_new, "FILE_NAME","A:/System/Light_Command.json");
		memset(payLoadData,0,sizeof(payLoadData));//\0';
		cJSON_PrintPreallocated(responseObject_new, payLoadData, sizeof(payLoadData), false);
		cJSON_Delete(responseObject_new);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");

		cJSON *my_JSON1  	= cJSON_CreateArray();
		if (my_JSON1 != NULL)
		{
			cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("GMT"));
			cJSON *jsonObject = cJSON_CreateObject();
			cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
			memset(payLoadData,0, sizeof(payLoadData));
			cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
			cJSON_Delete(jsonObject);
			Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "GET");
		}

		FirstlightEntry = 1;
#if defined (B480) || defined (B553) || defined (B543)
		for(i=0;i<NUMBER_OF_CHANNELS;i++)
		{
			configure_and_enable_rmt_channels(i);
		}
		init_data_channels();

#elif defined (B542)
		for(i=0; i<3; i++)
		{
			   config_pto(i, 178, 50, 1, 1);
		}

#endif
		initCommandTime = get_current_time_ms();

#ifdef ENABLE_PRINT_MSG
		printf("initCommandTime = %lld \n", initCommandTime);

	   	printf("\n Data sent through all RMT channels ");
#endif

#if defined(B480) || defined(B553) || defined (B542) || defined (B543)

	   	xTaskCreatePinnedToCore(LEdRunningmonitor, (const char*) "CH1", (10*1024), NULL, TRIM_SUBTASK_PRIORITY, &LightRunningHandle0,0);
			if (LightRunningHandle0 == NULL)
			{
				printf("Failed to create task\n");

			}
		LightRunningHandle4 = xTaskCreateStaticPinnedToCore(
				LEdRunningmonitor4,                 // Task function
			    "Save_command",            // Task name
				TRIM_SUBTASK_STACK_DEPTH,        // Stack size in words
				 (void*)3,                    // Task parameters (not used here)
			    2,                       // Task priority
			    xTaskStack5,              // Pointer to task stack (allocated in PSRAM)
			    &xTRIMSubTaskBuffer4,            // Pointer to task control block
				1
		);

		if (LightRunningHandle4 == NULL)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
			// Handle error
		}
#endif
	}
}

static void monitor(void *pvParameters __attribute__((unused)))
{
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list

	char str[100] = {0};

	uint8_t u8Result =0;

	while (1)
	{
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;

		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(light_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{

			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
#ifdef ENABLE_PRINT_MSG
			printf("LIGHT msg_Rx_Queue S = %s, D = %s, C = %s, size = %d\n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8, s_Message_Rx->payload_size);
			if(s_Message_Rx->payload_p8 != NULL)
			{
				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
					printf("LIGHT DT = %s\n",s_Message_Rx->payload_p8);
			}
#endif
			/*
			 * Match and acall functions
			 */

			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties
				if (FirstlightEntry == 0)
					init(0, 0);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				u8Result = 0;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL)
				{
					Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
				}
				else
				{
					cJSON *Last_Command_JSON = NULL;
					cJSON *root_JSON  =  NULL;
					head_JSON = name_JSON->child;

					do
					{
						if ((strcmp(head_JSON->string, "CHAN1LASTCOMMAND") == 0) || (strcmp(head_JSON->string, "CHAN2LASTCOMMAND") == 0) || (strcmp(head_JSON->string, "CHAN3LASTCOMMAND") == 0)|| (strcmp(head_JSON->string, "CHAN4LASTCOMMAND") == 0))
						{
#ifdef ENABLE_PRINT_MSG
							printf("\n\n head_JSON->string = %s \n\n", head_JSON->string);
#endif
							if(Last_Command_JSON == NULL)
							{
								Last_Command_JSON  = cJSON_CreateObject();
								if(Last_Command_JSON != NULL)
								{
									cJSON_AddStringToObject(Last_Command_JSON, "FILE_NAME", "A:/System/Light_Command.json");
								}
							}
							cJSON_AddStringToObject(Last_Command_JSON, head_JSON->string, head_JSON->valuestring);
						}
						else
						{
							if(root_JSON == NULL)
							{
								root_JSON  = cJSON_CreateObject();
								if(root_JSON != NULL)
								{
									cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/LIGHTING.json");
								}
							}
							cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
						}
						// Check if the value string is not NULL
						if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
						{
							// Set the key-value pair
							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
							if(u8Result==2){
							sprintf(str,"'%s' is a read only property", head_JSON->string);
							Add_Response_msg(str, s_Message_Rx, payLoadData);
							}
							else if(u8Result==0){
								sprintf(str,"'%s' is a invalid Key", head_JSON->string);
								Add_Response_msg(str,s_Message_Rx, payLoadData);
							}
						}
						else
						{
							// Handle the case where value string is NULL (e.g., log an error or take appropriate command)
							sprintf(str, "Invalid parameter '%s'", head_JSON->string);
							Add_Response_msg(str,s_Message_Rx, payLoadData);
							// Handle the error as per your application's requirements
						}
						head_JSON = head_JSON->next;
					} while (head_JSON != 0);
					// Free the parsed JSON
					cJSON_Delete(name_JSON);
					if((u8Result==1) && (Last_Command_JSON == NULL) && (root_JSON != NULL))
					{
						//  save parameters to JFS
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);						
						Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
						cJSON_Delete(root_JSON);
						console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
					}
					if((u8Result==1) && (Last_Command_JSON != NULL))
					{
						//  save parameters to JFS
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(Last_Command_JSON, payLoadData, sizeof(payLoadData), false);
						cJSON_Delete(Last_Command_JSON);
						Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
						//console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			{
//				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//				getAll(prop, (char*) val_p8,s_Message_Rx);
//				free(val_p8);
//			}
			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET_RGB_VALUE")))
			{
				if(get_rgb_value(s_Message_Rx) != -1)
				{
					if(s_Message_Rx->payload_p8 != NULL)
					console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
				//call your function
			}
			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "STATUS")))
			{
				if(get_channel_status(s_Message_Rx) != -1)
				{
				}
				//call your function
			}
			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "PUT0")))
			{
				if(put_array(s_Message_Rx) != -1)
				{
				}
				//call your function
			}
			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "ON")))
			{
					if (playlist_started_flag) {
						stopPlaylist();
					}
					flag_direct_array_testing = 0;
					flag_direct_array_testing_2 = 0;

					if(trim_On(s_Message_Rx) != -1)
					{
						if(strlen((char*)s_Message_Rx->payload_p8) != 0)
						console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
					}
#if defined (B542)
					else
					{
						Add_Response_msg("QPOE does not support this command.", s_Message_Rx, payLoadData);
					}
#endif
				//call your function
			}

			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "OFF")))
			{
				if (playlist_started_flag) {
					stopPlaylist();
				}
				flag_direct_array_testing = 0;
				flag_direct_array_testing_2 = 0;
				if(trimLightOFF(s_Message_Rx) != -1)
				{
					if(strlen((char*)s_Message_Rx->payload_p8) != 0)
					console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}

			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_COLOR_TABLE"))
			{
				if (vColor_Table_Que == NULL) {
					vColor_Table_Que = xQueueCreateStatic(61, 1000, ReadColtab_pucQueueStorage, &ReadColtab_pxQueueBuffer);
					if (vColor_Table_Que == NULL) {
				#ifdef ENABLE_PRINT_MSG
						printf("ERROR(Schedule_Record_Que is not created)\n ");
				#endif
					}
				}

				if(ReadColorTable_Handle == NULL)
				{
					ReadColorTable_Handle = xTaskCreateStaticPinnedToCore(
								Read_Color_Table,                 // Task function
								"READ_COLOR_TABLE",            // Task name
								READ_COLOR_TABLE_TASK_STACK_DEPTH,        // Stack size in words
								s_Message_Rx,                    // Task parameters (not used here)
								READ_COLOR_TABLE_TASK_PRIORITY,                       // Task priority
								xReadColorTableTaskStack2,              // Pointer to task stack (allocated in PSRAM)
								&xReadColorTableTaskBuffer,             // Pointer to task control block
								0
					);

					if (ReadColorTable_Handle == NULL)
					{
					#ifdef ENABLE_PRINT_MSG
						printf("Failed to create task\n");
					#endif
						// Handle error
					}
                }
                else
                {
                    Add_Response_msg("READ_COLOR_TABLE task already created.", s_Message_Rx, payLoadData);
                }
            }
            else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_COMMAND_TABLE"))
            {
                if (vCommand_Table_Que == NULL) {
                    vCommand_Table_Que = xQueueCreateStatic(READ_SQL_TABLE_QUEUE_DEPTH, READ_SQL_TABLE_QUEUE_ENTRY_SIZE, ReadCommand_pucQueueStorage, &ReadCommand_pxQueueBuffer);
                    if (vCommand_Table_Que == NULL) {
#ifdef ENABLE_PRINT_MSG
                        printf("ERROR(Command table queue is not created)\n ");
#endif
                    }
                }

                if(ReadCommandTable_Handle == NULL)
                {
                    ReadCommandTable_Handle = xTaskCreateStaticPinnedToCore(
                                Read_Command_Table,
                                "READ_COMMAND_TABLE",
                                READ_COMMAND_TABLE_TASK_STACK_DEPTH,
                                s_Message_Rx,
                                READ_COMMAND_TABLE_TASK_PRIORITY,
                                xReadCommandTableTaskStack,
                                &xReadCommandTableTaskBuffer,
                                0
                    );

                    if (ReadCommandTable_Handle == NULL)
                    {
#ifdef ENABLE_PRINT_MSG
                        printf("Failed to create READ_COMMAND_TABLE task\n");
#endif
                    }
                }
                else
                {
                    Add_Response_msg("READ_COMMAND_TABLE task already created.", s_Message_Rx, payLoadData);
                }
            }
            else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_PLAYLIST_TABLE"))
            {
                if (vPlaylist_Table_Que == NULL) {
                    vPlaylist_Table_Que = xQueueCreateStatic(READ_SQL_TABLE_QUEUE_DEPTH, READ_SQL_TABLE_QUEUE_ENTRY_SIZE, ReadPlaylist_pucQueueStorage, &ReadPlaylist_pxQueueBuffer);
                    if (vPlaylist_Table_Que == NULL) {
#ifdef ENABLE_PRINT_MSG
                        printf("ERROR(Playlist table queue is not created)\n ");
#endif
                    }
                }

                if(ReadPlaylistTable_Handle == NULL)
                {
                    ReadPlaylistTable_Handle = xTaskCreateStaticPinnedToCore(
                            Read_Playlist_Table,
                            "READ_PLAYLIST_TABLE",
                            READ_PLAYLIST_TABLE_TASK_STACK_DEPTH,
                            s_Message_Rx,
                            READ_PLAYLIST_TABLE_TASK_PRIORITY,
                            xReadPlaylistTableTaskStack,
                            &xReadPlaylistTableTaskBuffer,
                            0
                    );

                    if (ReadPlaylistTable_Handle == NULL)
                    {
#ifdef ENABLE_PRINT_MSG
                        printf("Failed to create READ_PLAYLIST_TABLE task\n");
#endif
                    }
                }
                else
                {
                    Add_Response_msg("READ_PLAYLIST_TABLE task already created.", s_Message_Rx, payLoadData);
                }
            }
			else if ((!strcmp((char*)s_Message_Rx->cmdFun_a8, "STOP_PLAYLIST")))
			{
				stopPlaylist();
//				flag_direct_array_testing = 0;
//				flag_direct_array_testing_2 = 0;
//				if(trimLightOFF(s_Message_Rx) != -1)
//				{
//					if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
//				}

			}
            else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "EXECUTE_COMMAND"))
            {
                cJSON *payload = (s_Message_Rx->payload_p8 != NULL && s_Message_Rx->payload_p8[0] != '\0')
                    ? cJSON_Parse((char*)s_Message_Rx->payload_p8) : NULL;
                int command_id = 0;
                int channel = 0;
                float brightness_override = -1.0f;  /* < 0 = use Command_Table brightness */

                if (payload != NULL) {
                    cJSON *j = cJSON_GetObjectItem(payload, "command_id");
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "Command_ID");
                    }
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "CommandId");
                    }
                    if (cJSON_IsNumber(j)) {
                        command_id = j->valueint;
                    }

                    j = cJSON_GetObjectItem(payload, "channel");
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "Channel");
                    }
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "CH");
                    }
                    if (cJSON_IsNumber(j)) {
                        channel = j->valueint;
                    }

                    j = cJSON_GetObjectItem(payload, "brightness_override");
                    if (j != NULL && cJSON_IsNumber(j)) {
                        brightness_override = (float)cJSON_GetNumberValue(j);
                    }

                    cJSON_Delete(payload);
                }

                uint32_t all_channel_mask = (1u << NUMBER_OF_CHANNELS) - 1u;

                if (command_id <= 0) {
                    Add_Response_msg("EXECUTE_COMMAND requires command_id.", s_Message_Rx, payLoadData);
                } else if (channel != -1 && (channel < 1 || (uint32_t)channel > all_channel_mask)) {
                    Add_Response_msg("EXECUTE_COMMAND requires channel -1 or a bitmask from 1 to 15.", s_Message_Rx, payLoadData);
                } else if (find_command_entry(command_id) == NULL) {
                    Add_Response_msg("EXECUTE_COMMAND command_id not found. Load Command Table first.", s_Message_Rx, payLoadData);
                } else {
                    PlaylistRequest request = {0};
                    request.brightness_override = brightness_override;
                    playlist_started_flag = true;

                    if (channel == -1) {
                        for (int ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
                            executeCommand(command_id, ch, &request);
                        }
                    } else {
                        uint32_t target_mask = (uint32_t)channel & all_channel_mask;
                        for (int ch = 0; ch < NUMBER_OF_CHANNELS; ch++) {
                            if ((target_mask & (1u << ch)) != 0u) {
                                executeCommand(command_id, ch, &request);
                            }
                        }
                    }
                    Add_Response_msg("EXECUTE_COMMAND triggered.", s_Message_Rx, payLoadData);
                }
            }
            else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "EXECUTE_PLAYLIST"))
            {
//            	printf("s_Message_Rx->payload_p8 = %s \n", s_Message_Rx->payload_p8);

                cJSON *payload = (s_Message_Rx->payload_p8 != NULL && s_Message_Rx->payload_p8[0] != '\0')
                    ? cJSON_Parse((char*)s_Message_Rx->payload_p8) : NULL;
                int playlist_id = 0;
                float brightness_override = -1.0f;  /* < 0 = use Command_Table brightness per Command_ID */
                uint32_t total_duration_override_sec = 0;
                int32_t total_duration_override_input_sec = 0;
                uint64_t local_start_time_ms = 0;
                bool has_target_override = false;
                bool has_target_bitfield_override = false;
                PlaylistTargetType target_type_override = TARGET_TYPE_COUNT;
                uint8_t target_bitfield_override = 0;

                if (payload != NULL) {
                    cJSON *j = cJSON_GetObjectItem(payload, "playlist_id");
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "Playlist_ID");
                    }
                    if (j == NULL) {
                        j = cJSON_GetObjectItem(payload, "PlaylistID");
                    }
                    if (cJSON_IsNumber(j)) {
                        playlist_id = j->valueint;
                    }

//                    printf(" PlaylistID = %d \n", playlist_id);

                    j = cJSON_GetObjectItem(payload, "brightness_override");
                    if (j != NULL && cJSON_IsNumber(j)) {
                        brightness_override = (float)cJSON_GetNumberValue(j);
                    }

//                    printf(" brightness_override = %f \n", brightness_override);

                    j = cJSON_GetObjectItem(payload, "total_duration_override");
                    if (j != NULL && cJSON_IsNumber(j)) {
                        total_duration_override_input_sec = j->valueint;
                    }

                    if (total_duration_override_input_sec <= 0) {
                        /* 0 or negative means no timeout (run forever). */
                        total_duration_override_sec = 0;
                    } else {
                        total_duration_override_sec = (uint32_t)total_duration_override_input_sec;
                    }

//                    printf(" total_duration_override_sec = %ld \n", total_duration_override_sec);

                    j = cJSON_GetObjectItem(payload, "local_start_time_ms");
                    if (j != NULL && cJSON_IsNumber(j)) {
                        local_start_time_ms = (uint64_t)cJSON_GetNumberValue(j);
                    }

//                    printf(" local_start_time_ms = %lld \n", local_start_time_ms);

                    j = cJSON_GetObjectItem(payload, "target_type_override");
                    if (j != NULL) {
                        if (cJSON_IsNumber(j) && j->valueint >= 0 && j->valueint < TARGET_TYPE_COUNT) {
                            target_type_override = (PlaylistTargetType)j->valueint;
//                            printf("target_type_override = %d \n", target_type_override);
                            if(target_type_override == 0)
                            {
                            	target_type_override = TARGET_TYPE_COUNT;
                            }
                            else
                            {
                            	has_target_override = true;
                            }
                        } else if (cJSON_IsString(j) && j->valuestring != NULL) {
                            target_type_override = parse_target_type_override(j->valuestring);
                            has_target_override = true;
                        }
                    }
                    else
                    {
                    	target_type_override = 0;
                    }

//                    printf(" target_type_override = %d \n", target_type_override);

                    j = cJSON_GetObjectItem(payload, "target_bitfield_override");
                    if (j != NULL) {
                        if (cJSON_IsNumber(j)) {
                            target_bitfield_override = (uint8_t)(j->valueint & 0xFF);

//                            printf(" target_bitfield_override = %d \n", target_bitfield_override);
							if(target_bitfield_override == 0)
							{

							}
							else
							{
#if defined(B542)
								target_bitfield_override = 15;
#endif
	                            has_target_bitfield_override = true;
	                            has_target_override = true;
							}
                        }
                    }
                    else
                    {
                    	target_bitfield_override = 0;
                    }

//                    printf(" target_bitfield_override = %d \n", target_bitfield_override);
                }

//                if (target_type_override == TARGET_ALL_CHANNELS && !has_target_bitfield_override) {
                if (target_type_override == TARGET_ALL_CHANNELS) {
                    target_bitfield_override = (uint8_t)(((1u << NUMBER_OF_CHANNELS) - 1) & 0xFF);
                    has_target_bitfield_override = true;
                }

                if (playlist_id >= 1 && playlist_id <= MAX_ACTIVE_PLAYLISTS) {
                    int resp_exp = executePlaylist(playlist_id, brightness_override, total_duration_override_sec, local_start_time_ms, has_target_override, target_type_override, target_bitfield_override, has_target_bitfield_override);
                    if(resp_exp == 0)
                    {
                    	Add_Response_msg("EXECUTE_PLAYLIST will be start.", s_Message_Rx, payLoadData);
                    }
                    else
                    {
                    	Add_Response_msg("Error in playlist running.", s_Message_Rx, payLoadData);
                    }

                } else {
                    Add_Response_msg("EXECUTE_PLAYLIST requires playlist_id 1-25.", s_Message_Rx, payLoadData);
                }
                if (payload != NULL) {
                    cJSON_Delete(payload);
                }
            }
            else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "READ_VIRTUAL_TABLE"))
            {
                if (vVirtual_Table_Que == NULL) {
                    vVirtual_Table_Que = xQueueCreateStatic(READ_SQL_TABLE_V_QUEUE_DEPTH, READ_SQL_TABLE_QUEUE_ENTRY_SIZE, ReadVirtual_pucQueueStorage, &ReadVirtual_pxQueueBuffer);
                    if (vVirtual_Table_Que == NULL) {
#ifdef ENABLE_PRINT_MSG
                        printf("ERROR(Virtual table queue is not created)\n ");
#endif
                    }
                }

                if(ReadVirtualTable_Handle == NULL)
                {
                    ReadVirtualTable_Handle = xTaskCreateStaticPinnedToCore(
                            Read_Virtual_Table,
                            "READ_VIRTUAL_TABLE",
                            READ_VIRTUAL_TABLE_TASK_STACK_DEPTH,
                            s_Message_Rx,
                            READ_VIRTUAL_TABLE_TASK_PRIORITY,
                            xReadVirtualTableTaskStack,
                            &xReadVirtualTableTaskBuffer,
                            0
                    );

                    if (ReadVirtualTable_Handle == NULL)
                    {
#ifdef ENABLE_PRINT_MSG
                        printf("Failed to create READ_VIRTUAL_TABLE task\n");
#endif
                    }
                }
                else
                {
                    Add_Response_msg("READ_VIRTUAL_TABLE task already created.", s_Message_Rx, payLoadData);
                }
            }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "LIGHT_GMT_DST"))
			 {
				set_gmt_dst(s_Message_Rx);
			 }
			else
			{
				uint8_t u8SrcLength = strlen((const char *)s_Message_Rx->src_Actor_a8);
				uint8_t u8DstLength = strlen((const char *)s_Message_Rx->dest_Actor_a8);
				//TRIM error message: invalid method

				if(u8SrcLength != 0 && u8DstLength != 0)
				Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}

IRAM_ATTR int CalculateDataBuffers(int u8ChannelNumber, uint64_t u64CurrentTime) {
    int offset = 0;
    float offset_float = 0.0f;
    int i = u8ChannelNumber;

    uint64_t u64Ramp_DwellTime = 0;

    if(playlist_started_flag == true)
    {
		/* Apply pending playlist command first (fill arrays/flags) so PrepareDataWithModeSetting below runs with correct data */
		if (i >= 0 && i < NUMBER_OF_CHANNELS && playlist_apply_pending[i] && playlist_pending_command[i] != NULL) {
			apply_playlist_command_to_channel(i, playlist_pending_command[i], playlist_pending_brightness[i]);
			playlist_apply_pending[i] = false;
			playlist_pending_command[i] = NULL;
		}
    }

    if (duration_time[i] != 0) {

        int time_diff_period = (u64CurrentTime - duration_Start_time[i]); // In milliseconds

        if (time_diff_period > duration_time[i]) {
            duration_time[i] = 0;
            pendingRestoreLastCommand[i] = 1;
        }
    }

    if (rampData[i].RampStartTime != 0) {

        int time_diff_period = (u64CurrentTime - rampData[i].RampStartTime); // In milliseconds
        u64Ramp_DwellTime = rampData[i].RampTimeSceneVal + rampData[i].DwellTimeSceneVal;

        if (time_diff_period > u64Ramp_DwellTime) {
        	rampData[i].RampStartTime = 0;
        	rampData[i].RampTimeSceneVal = 0;
        	rampData[i].DwellTimeSceneVal = 0;
        	ExecuteSceneRampFlag[i] = 0;

            pendingRestoreLastCommand[i] = 1;
        }
    }

    if (ExecuteSceneRampFlag[i] != 0) {
    	RampAndDwellFunction(i, u64CurrentTime);
    }

    if(playlist_started_flag == true)
    {
    	executePlaylistFunc(i, u64CurrentTime);
    }

    if (SparkleStartFlag[i] != 0) {
        decayledProc(i, u64CurrentTime, 0);
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if (MultiColorSparkleStartFlag[i] != 0) {
    	MultiColordecayledProc(i, u64CurrentTime, 0);
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if ((PatternStartFlag_start[i] != 0) && (ExecuteSceneRampFlag[i] == 0))
    {
        patternExecuteProc(i, u64CurrentTime, 0);
		offset = 0;
		PrepareDataWithModeSetting(offset, i, 1);
    }

    if (HueSatStartFlag[i] != 0) {
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if (setColorStartFlag[i] != 0) {
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if (colorIndexStartFlag[i] != 0) {
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if ((One_LED_time[i] != 0) && (TapeMeasureStartFlag[i] == 1))
    {
    	offset_float = Moving_Tap_Offset(i, u64CurrentTime, 0);
#ifdef ENABLE_PRINT_MSG
    	printf("offset_float = %f \n", offset_float);
#endif
        if(TapeMeasureStartFlag_offset[i]!=(int)offset_float)
        {
        	TapeMeasureStartFlag_offset[i]=(int)offset_float;
        }
        else
        {

        }

        Execute_PrepareDataWithModeSetting(offset_float, i, 1, 0);

    }
    else if(TapeMeasureStartFlag[i] == 1)
    {
    	if(TapeMeasureStartFlag_offset[i] == 1)
    	{
    		TapeMeasureStartFlag_offset[i] = 0;
    		Execute_PrepareDataWithModeSetting(0, i, 1, 0);

    	}
    	else
    	{
    		Execute_PrepareDataWithModeSetting(0, i, 1, 0);

    	}
    }

    if (RacingStartFlag[i] != 0) {
    	Execute_RacingEffect(i, u64CurrentTime);
    }

//    executePlaylistFunc(i, u64CurrentTime);

    if ((One_LED_time[i] != 0) && (ExecuteCustomStartFlag[i] == 1))
    {
    	offset_float = Moving_Tap_Offset(i, u64CurrentTime, 0);
#ifdef ENABLE_PRINT_MSG
    	if(i == 0)
    	{
    		printf("offset_float = %f \n", offset_float);
    	}
#endif
        if(ExecuteCustomStartFlag_offset[i]!=(int)offset_float)
        {
        	ExecuteCustomStartFlag_offset[i]=(int)offset_float;
        }
        else
        {

        }

        Execute_PrepareDataWithModeSetting(offset_float, i, 1, 0);

    }
    else if(ExecuteCustomStartFlag[i] == 1)
    {
#ifdef ENABLE_PRINT_MSG
    	printf(" channel  = %d, ExecuteCustomStartFlag_offset1 = %d, ExecuteCustomStartFlag = %d \n", i, ExecuteCustomStartFlag_offset1[i], ExecuteCustomStartFlag[i]);
#endif

    	if(ExecuteCustomStartFlag_offset1[i] == 1)
    	{
    		ExecuteCustomStartFlag_offset1[i] = 0;

    		Execute_PrepareDataWithModeSetting(0, i, 1, 0);
    	}
    	else
    	{
    		if (enableMirror_uint8[i] == 1)
    		{
    			Execute_PrepareDataWithModeSetting(0, i, 1, 0);
    		}
    		else
    		{
    			Execute_PrepareDataWithModeSetting(0, i, 1, 0);
    		}
    	}
    }

    if ((One_LED_time[i] != 0) && (MarqueeExecuteCustomStartFlag[i] == 1))
    {
    	offset_float = Marquee_Moving_Tap_Offset(i, u64CurrentTime, 0);
#ifdef ENABLE_PRINT_MSG
    	if(i == 0)
    	{
    		printf("offset_float = %f \n", offset_float);
    	}
#endif
        if(MarqueeCustomStartFlag_offset[i]!=(int)offset_float)
        {
        	MarqueeCustomStartFlag_offset[i]=(int)offset_float;
        }
        else
        {

        }

        ExecuteMarquee_PrepareDataWithModeSetting(offset_float, i, 1, u64CurrentTime, 0);
    }
    else if(MarqueeExecuteCustomStartFlag[i] == 1)
    {
#ifdef ENABLE_PRINT_MSG
    	printf(" channel  = %d, MarqueeCustomStartFlag_offset1 = %d, MarqueeExecuteCustomStartFlag = %d \n", i, MarqueeCustomStartFlag_offset1[i], MarqueeExecuteCustomStartFlag[i]);
#endif

    	if(MarqueeCustomStartFlag_offset1[i] == 1)
    	{
    		MarqueeCustomStartFlag_offset1[i] = 0;

    		ExecuteMarquee_PrepareDataWithModeSetting(0, i, 1, u64CurrentTime, 0);
    	}
    	else
    	{
    		if (enableMirror_uint8[i] == 1)
    		{
    			ExecuteMarquee_PrepareDataWithModeSetting(0, i, 1, u64CurrentTime, 0);
    		}
    		else
    		{
    			ExecuteMarquee_PrepareDataWithModeSetting(0, i, 1, u64CurrentTime, 0);
    		}
    	}
    }

    if (RippleStartFlag[i]) {
        RippleContinious(i, u64CurrentTime, 0);
        offset = 0;
        PrepareDataWithModeSetting(offset, i, 1);
    }

    if((ExecuteCustomStartFlag[i] == 1) || (MarqueeExecuteCustomStartFlag[i] == 1))
    {
		if(enableMirror_uint8[i] == 1)
		{
			MirrorData(MirrorLedNum[i], i);
		}
    }
    return 1;
}

#if defined(B480) || defined(B553) || defined (B543)
static void LEdRunningmonitor(void *pvParameters __attribute__((unused)))
{
    int i = (int)pvParameters;
    uint64_t u64CurrentTime = 0;

	int Number_of_LED_int1 = EXAMPLE_LED_NUMBERS; // 800; //1024; //EXAMPLE_LED_NUMBERS;
    rmt_tx_event_callbacks_t cbs = {
        .on_trans_done = rmt_tx_done_callback,
    };
	for(int i = 0; i < 4; i++)
	{
        tx_contexts[i].channel_index = i;
        ESP_ERROR_CHECK(rmt_tx_register_event_callbacks(tx_channels[i], &cbs, &tx_contexts[i]));
	}
    while (1)
    {
    	if(flag_direct_array_testing == 1)
    	{
            u64CurrentTime = get_current_time_ms();
        	if(flag_not_rmt == 0)
        	{
				for(i=0; i<4; i++)
				{
					send_rmt_data(tx_channels[i], led_encoders[i],
					(use_ping_buffer[i]) ? data_channels_ping[i] : data_channels[i],
							Number_of_LED_int1 * 3);
				}
        	}

			uint8_t num1 = 0;

			for(i=0; i<4; i++)
			{
				if( channel_equal(i) == 0)  //channel_equal(i) 1- same, 0 - different
				{
					num1 = 1;
				}
			}

			if(num1 == 0)
			{
				if(delay_same_array != 0x7FFFFFFF)
				{
					delay_same_array = (delay_same_array << 1) | 1 ;
				}
				if(delay_same_array == 0x7FFFFFFF)
				{
					flag_not_rmt = 1;
				}
			}
			else
			{
				delay_same_array = 1;
				flag_not_rmt = 0;
			}

            update_led_refresh_timer_state(u64CurrentTime);
    	}
    	else
    	{
        bool all_idle =
            !tx_busy_flags[0] &&
            !tx_busy_flags[1] &&
            !tx_busy_flags[2] &&
            !tx_busy_flags[3];

        if (all_idle) {
            rmt_sync_reset(synchro);
        }
        else
        {
        	vTaskDelay(1 / portTICK_PERIOD_MS); // Delay between transmissions
        	continue;
        }
    	/* Swap only when RMT actually runs; swapping without a transmit desyncs ping/pong from
    	 * the last frame clocked to the strip and causes visible jumps on each LED_REFRESH_TIMER resend. */
    	const uint8_t sent_rmt_this_cycle = (flag_not_rmt == 0) ? 1u : 0u;
    	if(sent_rmt_this_cycle)
    	{
			for(i=0; i<4; i++)
			{
				tx_busy_flags[i] = true;
				send_rmt_data(tx_channels[i], led_encoders[i],
				(use_ping_buffer[i]) ? data_channels_ping[i] : data_channels[i],
						Number_of_LED_int1 * 3);
			}
		}
		uint8_t num1 = 0;
	    u64CurrentTime = get_current_time_ms();
		for(i=0; i<4; i++)
		{
			if (sent_rmt_this_cycle) {
				swap_buffers(i);
			}
			CalculateDataBuffers(i, u64CurrentTime);

			if (!SparkleStartFlag[i])
			{
				if( channel_equal(i) == 0)  //channel_equal(i) 1- same, 0 - different
				{
					num1 = 1;
				}
			}
			else
			{
				num1 = 1;
			}
		}
		
		if(num1 == 0)
		{
			if(delay_same_array != 0x7FFFFFFF)
			{
				delay_same_array = (delay_same_array << 1) | 1 ;
			}
			if(delay_same_array == 0x7FFFFFFF)
			{
				flag_not_rmt = 1;
			}
		}
		else
		{
			delay_same_array = 1;
			flag_not_rmt = 0;
		}

        update_led_refresh_timer_state(u64CurrentTime);
	}

	   vTaskDelay(LED_TASK_RATE / portTICK_PERIOD_MS); // Delay between transmissions
    }
}

#elif defined(B542)
#define NUM_STRIPS       4
static void LEdRunningmonitor(void *pvParameters __attribute__((unused)))
{
    // 0) PWM ← instead of RMT setup
   // pwm_init_timer_and_channels();

    // 1) track your “all same?” logic exactly as before:
    uint32_t delay_same_array = 1;
    uint8_t  flag_not_rmt     = 0;
    while (1) {
        uint64_t now = get_current_time_ms();

        // 2) decide if everything’s “same” yet or not
        uint8_t any_change = 0;
        for (int i = 0; i < NUM_STRIPS; ++i) {
            // pick current RGB from whichever buffer
            uint16_t r, g, b;
            if (use_ping_buffer[i]) {
                r = data_channels_ping[i][2];
                g = data_channels_ping[i][1];
                b = data_channels_ping[i][0];
            } else {
                r = data_channels[i][2];
                g = data_channels[i][1];
                b = data_channels[i][0];
            }

            // 3) drive this strip via PWM
            apply_rgb_pwm_to_channel((uint8_t)i, r, g, b);

            // 4) update your data buffers
            swap_buffers(i);

            CalculateDataBuffers(i, now);

            // 5) detect if this channel changed
            if (!SparkleStartFlag[i] && channel_equal(i) == 0) {
                any_change = 1;
            } else if (SparkleStartFlag[i]) {
                any_change = 1;
            }
        }

        // 6) replicate your “delay_same_array / flag_not_rmt” FSM
        if (any_change == 0) {
            if (delay_same_array != 0x7FFFFFFF) {
                delay_same_array = (delay_same_array << 1) | 1;
            }
            if (delay_same_array == 0x7FFFFFFF) {
                flag_not_rmt = 1;
            }
        } else {
            delay_same_array = 1;
            flag_not_rmt     = 0;
        }
        
        update_led_refresh_timer_state(now);
        
        // 7) wait exactly your task‐rate,
        //    vTaskDelay is OK here since LED_TASK_RATE >> RTOS tick
        vTaskDelay(LED_TASK_RATE / portTICK_PERIOD_MS);
    }
}
#endif

#if defined(B480) || defined(B553) || defined (B542) || defined (B543)
static void LEdRunningmonitor4(void *pvParameters __attribute__((unused)))
{
	int i = 0;

	while (1)
	{
		uint64_t u64CurrentTime = get_current_time_ms();
		for(i=0; i<NUMBER_OF_CHANNELS; i++)
		{
			if (xSemaphoreTake(xPowerUpMutex[i], portMAX_DELAY) == pdTRUE)
			{
				uint64_t time_diff_monitor1 = 0;
				if(u64CurrentTime >=  powerUpCommandTime[i])
				{
					time_diff_monitor1 = u64CurrentTime - powerUpCommandTime[i];
				}
				else
				{
					time_diff_monitor1 = powerUpCommandTime[i] - u64CurrentTime;
				}
				if (powerUpCommandTime[i] != 0 && (time_diff_monitor1 >= 3500)) // 10 seconds in milliseconds
				{
					Last_CommandFlag[i] = 1;
					Power_Cycle[i] = 1;
					OnPower_setLastC(i);
					Last_CommandFlag[i] = 2;

					powerUpCommandTime[i] = 0;
				}
				xSemaphoreGive(xPowerUpMutex[i]);
			}

			if (xSemaphoreTake(xLightCommandMutex[i], portMAX_DELAY) == pdTRUE)
			{
				uint64_t time_diff_monitor2 = 0;
				if(u64CurrentTime >= light_LastCommandPara[i].lastCommandTime)
				{
					time_diff_monitor2 = u64CurrentTime - light_LastCommandPara[i].lastCommandTime;
				}
				else
				{
					time_diff_monitor2 = light_LastCommandPara[i].lastCommandTime - u64CurrentTime;
				}

				if (light_LastCommandPara[i].lastCommandTime != 0 && (time_diff_monitor2 >= 10000)) // 10 seconds in milliseconds
				{
					UBaseType_t pending_rx = (light_Rx_Queue != NULL) ? uxQueueMessagesWaiting(light_Rx_Queue) : 0;
					if (pending_rx <= 5U) {
						Power_up_counter_d2c = 0;

						Set_Command_Property(i + 1);
						light_LastCommandPara[i].lastCommandTime=0;
						Last_CommandFlag[i] = 2; // Reset the flag after saving
					}
				}
                xSemaphoreGive(xLightCommandMutex[i]);
            }
		}

		for (i = 0; i < NUMBER_OF_CHANNELS; i++) {
			if (pendingRestoreLastCommand[i] != 0U) {
				UBaseType_t pending_rx = (light_Rx_Queue != NULL) ? uxQueueMessagesWaiting(light_Rx_Queue) : 0;
				if (pending_rx <= 5U) {
					pendingRestoreLastCommand[i] = 0U;
					setLastCommand(i, 2);
					break;
				}
			}
		}

		uint64_t time_diff_monitor3 = 0;
		if(u64CurrentTime >= initCommandTime)
		{
			time_diff_monitor3 = u64CurrentTime - initCommandTime;
		}
		else
		{
			time_diff_monitor3= initCommandTime - u64CurrentTime;
		}

		if (initCommandTime != 0 && (time_diff_monitor3 >= 17000)) // 17 seconds in milliseconds
		{
			initCommandTime = 0;
#ifdef ENABLE_PRINT_MSG
			printf("initCommandTime = %lld \n", initCommandTime);
#endif
			if (!strcmp((char*)&light_para.chan1LastCommand, ""))
			{
#ifdef ENABLE_PRINT_MSG
				printf("blank command \n");
#endif
				char line2[200] = {0};

				strcpy(line2, source); // off command
#ifdef ENABLE_PRINT_MSG
				printf("line2 = %s \n", line2);
#endif
				int ret;

				esp_err_t err = esp_console_run_Custom(line2, &ret, THIS_ACTOR);

				if (err == ESP_ERR_NOT_FOUND)
				{
		#ifdef ENABLE_PRINT_MSG
					printf("Unrecognized command\n");
		#endif
				}
				else if (err == ESP_ERR_INVALID_ARG)
				{
					// command was empty
				} else if (err == ESP_OK && ret != ESP_OK)
				{
		#ifdef ENABLE_PRINT_MSG
					printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
					esp_err_to_name(ret));
		#endif
				}
				else if (err != ESP_OK)
				{
		#ifdef ENABLE_PRINT_MSG
					printf("Internal error: %s\n", esp_err_to_name(err));
		#endif
				}
			}
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}
#endif

void LIGHT_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstlightEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(light_Rx_Queue, s_Message, QUE_DELAY);
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<LIGHTING.ERROR(LIGHTING RX Queue is full)\n");
		}
		else
		{
			printf("<LIGHTING.ERROR(LIGHTING RX Queue send unsuccessful)\n");
		}
	}

}//	LIGHT_ConsolWriteToActor

static void get_actor_properties(AMessage_st* s_Message_Rx)
{
	char typeString[20] = {0};

	int no_of_elements = sizeof(prop) / sizeof(struct property);

	// Create JSON arrays
	cJSON *jsonArrayName = cJSON_CreateArray();
	cJSON *jsonArrayType = cJSON_CreateArray();
	cJSON *jsonArrayValue = cJSON_CreateArray();
	cJSON *jsonArrayAccess = cJSON_CreateArray();
	cJSON *jsonArrayHelpString = cJSON_CreateArray();

	for (int i = 0; i < no_of_elements; i++)
	{
		cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(prop[i].str_name));
		// Convert DataType enum to string representation for property type
		// Add value based on data type for property name
		switch (prop[i].type)
		{
			case U_INT8:
				strcpy(typeString, "U_INT8");
				sprintf(actor_prop_val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				strcpy(typeString, "U_INT16");
				sprintf(actor_prop_val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				strcpy(typeString, "U_INT32");
				sprintf(actor_prop_val_a8, "%ld", *(uint32_t*) prop[i].name);
				break;

			case U_INT64:
				sprintf(actor_prop_val_a8, "%016llu", *(uint64_t*) prop[i].name);
				break;

			case INT:
				strcpy(typeString, "INT");
				sprintf(actor_prop_val_a8, "%d", *(int*) prop[i].name);
				break;

			case FLOAT:
				strcpy(typeString, "FLOAT");
				sprintf(actor_prop_val_a8, "%f", *(float*) prop[i].name);
				break;

			case STRING:
				strcpy(typeString, "STRING");
				strcpy(actor_prop_val_a8, prop[i].name);
				break;

			default:
				break;
		}
		cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
		cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(actor_prop_val_a8));
		cJSON_AddItemToArray(jsonArrayAccess, cJSON_CreateString(prop[i].access));
		cJSON_AddItemToArray(jsonArrayHelpString, cJSON_CreateString(prop[i].HelpString));
	}
	// Create a JSON object and add the array to it
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Name", jsonArrayName);
	cJSON_AddItemToObject(jsonObject, "Type", jsonArrayType);
	cJSON_AddItemToObject(jsonObject, "Value", jsonArrayValue);
	cJSON_AddItemToObject(jsonObject, "Access", jsonArrayAccess);
	cJSON_AddItemToObject(jsonObject, "Help String", jsonArrayHelpString);

	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(jsonObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the LIGHTING actor.");
	cJSON_AddStringToObject(responseObject, "SET(string REVDIRCH1)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET_RGB_VALUE(arrayINT CH, int POSITION)", "Get the RGB value of the property table.");
	cJSON_AddStringToObject(responseObject, "STATUS(string FLAG)", "Get the Status of the all channel, FLAG = (true or false).");
	cJSON_AddStringToObject(responseObject, "SET_LED_STRIP(arrayINT CH, int POSITIONS, int RED, int GREEN, int BLUE)", "Send LED strip data(SET_LED_STRIP).");
	cJSON_AddStringToObject(responseObject, "ON(arrayINT CH, float BRIGHTNESS, int BRIGHTNESSINDEX, float DURATION, string FUNCTION,Config:{(float HUE, float SAT, float VAL)})", "Send LED strip ON command for 1-HUESAT PARAMETR: float HUE, float SAT, float VAL 2-EXECUTECUSTOM PARAMETR: arrayFLOAT COLORSELECTIONS, arrayFLOAT BGCOLOR, float COLORLENGTH, float PADDINGLENGTH, string TRANSITIONTYPE, float MOVINGSPEED, int ENABLEMIRROR, float MIRRORPOSITION, float OSCAMP, float OSCPERIOD, int spacingOverride 3-SPARKLE PARAMETR: arrayFLOAT STARTCOLOR, arrayFLOAT ENDCOLOR, float INTENSITY, float WIDTH, float DECAYTIME 4-RIPPLE PARAMETR: arrayFLOAT STARTCOLOR, arrayFLOAT PEAKCOLOR, arrayFLOAT VALLEYCOLOR, float AMP1, float WAVE1, float SPEED1, float AMP2, float WAVE2, float SPEED2, float AMP3, float WAVE3, float SPEED3 5-PATTERN PARAMETR: string TYPE, float PERIOD, 6-PWM PARAMETR: float RED, float GREEN, float BLUE, 7-TAPEMEASURE PARAMETR: float Spacing, float Speed 8-colorIndex PARAMETR: int Index,  10-MARQUEE PARAMETR: arrayFLOAT COLORS, string TRANSITIONTYPE, float SPEED, int ENABLEMIRROR, float MIRRORPOSITION, float OSCAMP, float OSCPERIOD, float brightnessWavelength, float brightnessAmplitude, float brightnessSpeed, int spacingOverride 11-MultiColorSparkle PARAMETR: arrayFLOAT COLORS, arrayFLOAT ENDCOLOR, float INTENSITY, float WIDTH, float DECAYTIME 12-Racing PARAMETR: int enable_collision_avoidance, int override_pitch_in, float pitch_in_inches, int fixed_cars, int max_cars_cap, float min_len_in, float max_len_in, float min_start_spacing_in, float reentry_gap_in, float min_speed_in_s, float max_speed_in_s, int retarget_min_ms, int retarget_jitter_ms, float max_accel_in_s2, float min_collision_gap_in, int spawn_mode, float max_dt_s.");	//

	cJSON_AddStringToObject(responseObject, "OFF(arrayINT CH)", "Send LIGHTING off command.");
	cJSON_AddStringToObject(responseObject, "EXECUTE_COMMAND(INT Command_ID, INT Channel, float brightness_override)", "Run one Command_Table entry directly for testing. Channel is a bitmask: 1=CH1, 2=CH2, 4=CH3, 8=CH4, combinations 1-15, or -1 for all channels.");
	cJSON_AddStringToObject(responseObject, "EXECUTE_PLAYLIST(INT Playlist_ID, float brightness_override, INT total_duration_override, U_INT64 local_start_time_ms, INT target_type_override, INT target_bitfield_override)", "Send LIGHTING EXECUTE_PLAYLIST command.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "READ_COLOR_TABLE()", "Read Color Table from database");
	cJSON_AddStringToObject(responseObject, "READ_PLAYLIST_TABLE()", "Read Playlist Table from database");
	cJSON_AddStringToObject(responseObject, "READ_COMMAND_TABLE()", "Read Command Table from database");
	cJSON_AddStringToObject(responseObject, "READ_VIRTUAL_TABLE()", "Read Virtual Group Table from database");
	cJSON_AddStringToObject(responseObject, "STOP_PLAYLIST()", "STOP_PLAYLIST");
	
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
	get_actor_properties(s_Message_Rx);
}//	help

static void Get_Property(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	char val_p8[256] = {0};
	int Array_size = 0;

	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"console get property Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	out_JSON 	= cJSON_CreateObject();
	head_JSON = cJSON_GetObjectItem(in_JSON, "Property_Names");
	Array_size = cJSON_GetArraySize(head_JSON);
	if(Array_size > 0)
	{
		for(int i=0; i<Array_size; i++)
		{
			cJSON *element = cJSON_GetArrayItem(head_JSON, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL))
			{
				if(strlen(element->valuestring) == 0)
					continue;
				memset(val_p8, 0, sizeof (val_p8));
				get(element->valuestring, val_p8);
				cJSON_AddStringToObject(out_JSON, element->valuestring, (char*) val_p8);
			}
		}
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
	{
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx, payLoadData);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

#ifndef B542
static void configure_rmt_channel(gpio_num_t gpio_num, rmt_channel_handle_t *channel)
{
    rmt_tx_channel_config_t tx_chan_config =
    {
        .clk_src = RMT_CLK_SRC_DEFAULT, // select source clock
        .gpio_num = gpio_num,
        .mem_block_symbols = 48, // increase the block size can make the LED less flickering
        .resolution_hz = RMT_LED_STRIP_RESOLUTION_HZ,
        .trans_queue_depth = 4, // set the number of transactions that can be pending in the background
		.flags.invert_out = (sAddressableStipArr[IC_Type_Var].u8FlagToggleOP),
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_chan_config, channel));
}

static void configure_rmt_channel_encoder(rmt_encoder_handle_t *encoder, rmt_channel_handle_t *channel)
{
	led_strip_encoder_config_t encoder_config =
	{
		.resolution = RMT_LED_STRIP_RESOLUTION_HZ,
	};
	ESP_ERROR_CHECK(rmt_new_led_strip_encoder(&encoder_config, encoder));
}


// Function to configure and enable RMT channels
static void configure_and_enable_rmt_channels(int i)
{
    {
    	{
    		configure_rmt_channel(tx_gpio_number[i], &tx_channels[i]);
    		 configure_rmt_channel_encoder(&led_encoders[i],&tx_channels[i]);

    		        ESP_ERROR_CHECK(rmt_enable(tx_channels[i]));
#ifdef ENABLE_PRINT_MSG
    		      //  printf("\n\n RMT channel %d enabled\n\n", i);
#endif
    	}
    }

    if(i == 3)
    {
		rmt_sync_manager_config_t synchro_config =
		{
			.tx_channel_array = tx_channels,
			.array_size = sizeof(tx_channels) / sizeof(tx_channels[0]),
		};
    	ESP_ERROR_CHECK(rmt_new_sync_manager(&synchro_config, &synchro));
    }
#ifdef ENABLE_PRINT_MSG

#endif
}

IRAM_ATTR static esp_err_t send_rmt_data(rmt_channel_handle_t channel, rmt_encoder_handle_t encoder, uint16_t *data, size_t size)
{
    esp_err_t err = ESP_OK;

    // Check if any of the input parameters are NULL
    if (channel == NULL) {
        printf("Channel handle is NULL \n");
        return ESP_ERR_INVALID_ARG;
    }

    if (encoder == NULL) {
        printf("Encoder handle is NULL \n");
        return ESP_ERR_INVALID_ARG;
    }

    if (data == NULL || size == 0) {
        printf("Invalid data or size: data = %p, size = %d \n", data, (int)size);
        return ESP_ERR_INVALID_ARG;
    }

    // Prepare transmit configuration
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,  // no transfer loop
    };

    // Attempt to transmit data and check for errors
    err = rmt_transmit(channel, encoder, data, size, &tx_config);
    if (err != ESP_OK) {

        printf("Failed to transmit data, error code: %d \n", err);
        return err;
    }

    // Successful transmission
    return ESP_OK;
}
#endif

static inline uint16_t swap_endianness(uint16_t value) {
    return (value >> 8) | (value << 8);
}

static void set_led_color(uint8_t  channels, uint16_t position,uint16_t red,uint16_t green,uint16_t blue)  // Set LED color
{
	int ModeSetting = 0;
	ModeSetting = 0;//sAddressableStipArr[IC_Type_Var].u8ModeSetting;
	uint16_t red1,green1,blue1;
	// Define the union

	red1=swap_endianness(red);
	green1=swap_endianness(green);
	blue1=swap_endianness(blue);

	if((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1)
	{


		switch(channels-1)
		{
		case 0:
			data_channels1_1[ModeSetting+((position-1)*3)]=red1;
				data_channels1_1[ModeSetting+((position-1)*3)+1]=green1;
				data_channels1_1[ModeSetting+((position-1)*3)+2]=blue1;

			break;
		case 1:
			data_channels1_2[ModeSetting+((position-1)*3)]=red1;
							data_channels1_2[ModeSetting+((position-1)*3)+1]=green1;
							data_channels1_2[ModeSetting+((position-1)*3)+2]=blue1;
			break;
		case 2:
			data_channels1_3[ModeSetting+((position-1)*3)]=red1;
							data_channels1_3[ModeSetting+((position-1)*3)+1]=green1;
							data_channels1_3[ModeSetting+((position-1)*3)+2]=blue1;
			break;
		case 3:
			data_channels1_4[ModeSetting+((position-1)*3)]=red1;
							data_channels1_4[ModeSetting+((position-1)*3)+1]=green1;
							data_channels1_4[ModeSetting+((position-1)*3)+2]=blue1;
			break;

		}
	}
}

cJSON *createJsonFromLastCommand(LastCommand_t *lastCommand, int Chan) {
    cJSON *json = cJSON_CreateObject();
    int Chan11 = Chan+1;

    if(lastCommand->state == 1)
    {
    // Parse the "ch" part of the payload to extract the channel information
    cJSON *ch_json = cJSON_Parse(lastCommand->payload);
    cJSON *ch_item = cJSON_GetObjectItem(ch_json, "ch");

    // Add the "CH" key directly to the main JSON object
    cJSON_AddItemToObject(json, "CH", cJSON_Duplicate(ch_item, 1));
    cJSON_Delete(ch_json); // Clean up temporary JSON object


    cJSON_AddNumberToObject(json, "BRIGHTNESS", lastCommand->brightness);
    cJSON_AddNumberToObject(json, "DURATION", lastCommand->duration);
    cJSON_AddStringToObject(json, "SOURCE", lastCommand->source);
    cJSON_AddStringToObject(json, "FUNCTION", lastCommand->function);
    cJSON_AddItemToObject(json, "CONFIG", cJSON_Duplicate(lastCommand->config, 1));
    }
    else if(lastCommand->state == 0)
    {
		cJSON *channelArray = cJSON_CreateArray();
		cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(Chan11));
		cJSON_AddItemToObject(json, "CH", channelArray);

	    cJSON_AddNumberToObject(json, "BRIGHTNESS", lastCommand->brightness);
	    cJSON_AddStringToObject(json, "SOURCE", lastCommand->source);
	    cJSON_AddStringToObject(json, "FUNCTION", lastCommand->function);
	    cJSON_AddItemToObject(json, "CONFIG", cJSON_Duplicate(lastCommand->config, 1));
    }

    cJSON_AddNumberToObject(json, "STATE", lastCommand->state);

    return json;
}

static void OnPower_setLastC( int Chan)
{

	Power_up_counter_d2c++;
#ifdef ENABLE_PRINT_MSG
//	printf("Power_up_counter_d2c P = %d \n", Power_up_counter_d2c);
#endif
	switch(Chan)
	{
	case 0:
		memcpy(line_Set_Cmd, (char*)&light_para.chan1LastCommand, sizeof(light_para.chan1LastCommand));
#ifdef ENABLE_PRINT_MSG
//		printf("line_Set_Cmd = %s \n", line_Set_Cmd);
#endif
		break;

	case 1:
		memcpy(line_Set_Cmd, (char*)&light_para.chan2LastCommand, sizeof(light_para.chan2LastCommand));
		break;

	case 2:
		memcpy(line_Set_Cmd, (char*)&light_para.chan3LastCommand, sizeof(light_para.chan3LastCommand));
		break;

	case 3:
		memcpy(line_Set_Cmd, (char*)&light_para.chan4LastCommand, sizeof(light_para.chan4LastCommand));
		break;
	}
#ifdef ENABLE_PRINT_MSG
	//printf("Final command power on = %s \n", line);
#endif

	 int ret;
	 if(line_Set_Cmd[0]=='<')
	 {
		esp_err_t err = esp_console_run_Custom(line_Set_Cmd, &ret, THIS_ACTOR);

		if (err == ESP_ERR_NOT_FOUND)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Unrecognized command\n");
#endif
		}
		else if (err == ESP_ERR_INVALID_ARG)
		{
			// command was empty
		} else if (err == ESP_OK && ret != ESP_OK)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
			esp_err_to_name(ret));
#endif
		}
		else if (err != ESP_OK)
		{
#ifdef ENABLE_PRINT_MSG
			printf("Internal error: %s\n", esp_err_to_name(err));
#endif
		}
			vTaskDelay(300/portTICK_PERIOD_MS);
	}
}

//static void setLastCommand( int Chan)

static void setLastCommand( int Chan, int BsaveFlag)
{
	if((Last_CommandFlag[Chan] != 0))
	{
		Last_CommandFlag[Chan] = 1;

		char line2[10] = {0};

		if(light_LastCommandPara[Chan].state == 1)
		{
			strcpy(line_setLastCommand, "<LIGHTING.ON(");
		}
		else if(light_LastCommandPara[Chan].state == 0)
		{
			strcpy(line_setLastCommand, "<LIGHTING.OFF(");
		}

		 // Create the root JSON object
#ifdef ENABLE_PRINT_MSG
//		printf("line1= %s \n", lastCommand.payload);
#endif
		 cJSON *json = createJsonFromLastCommand(&light_LastCommandPara[Chan], Chan);

		    // Free allocated memory
		
		 payLoadData_SETCMD[0] = '\0';
	cJSON_PrintPreallocated(json, payLoadData_SETCMD, sizeof(payLoadData_SETCMD), false);
#ifdef ENABLE_PRINT_MSG
		 printf("line1= %s \n", payLoadData);
#endif
		{
			size_t prefix_len = strlen(line_setLastCommand);
			size_t room = (prefix_len < COMMAND_LEN) ? (COMMAND_LEN - prefix_len - 2U) : 0U;
			if (room > 0U) {
				strncat(line_setLastCommand, payLoadData_SETCMD, room);
			}
			line_setLastCommand[COMMAND_LEN - 1U] = '\0';
		}
		strcpy(line2, ")");
		strncat(line_setLastCommand, line2, COMMAND_LEN - strlen(line_setLastCommand) - 1U);

#ifdef ENABLE_PRINT_MSG
//		 int size2 = strlen(line);
//		 printf("size2 = %d \n", size2);
#endif

#ifdef ENABLE_PRINT_MSG
		 printf("Final command= %s \n", line);
#endif
		 cJSON_Delete(json);
#ifdef ENABLE_PRINT_MSG
		printf("line= %s \n", line);
		printf("line= %s \n", line);
#endif

		 if(BsaveFlag == 1) // save command
		 {
			if(light_LastCommandPara[Chan].state == 1)
			{
#ifdef ENABLE_PRINT_MSG
//				printf("line1= %s \n", payLoadData);
#endif
				strcpy(light_LastCommandPara[Chan].payload, payLoadData_SETCMD);
#ifdef ENABLE_PRINT_MSG
//				printf("line1 2= %s \n", light_LastCommandPara[Chan].payload);
#endif
			}

			 {
				switch(Chan)
				{
					case 0:
						if (xSemaphoreTake(xLightCommandMutex[0], portMAX_DELAY) == pdTRUE)
						{
							memcpy((char*)&light_para.chan1LastCommand, line_setLastCommand, sizeof(line_setLastCommand));
							light_LastCommandPara[Chan].lastCommandTime = get_current_time_ms(); // Update last command time in milliseconds
							xSemaphoreGive(xLightCommandMutex[0]);
						}
						break;

					case 1:
						if (xSemaphoreTake(xLightCommandMutex[1], portMAX_DELAY) == pdTRUE)
						{
							memcpy((char*)&light_para.chan2LastCommand, line_setLastCommand, sizeof(line_setLastCommand));
							light_LastCommandPara[Chan].lastCommandTime = get_current_time_ms(); // Update last command time in milliseconds
							xSemaphoreGive(xLightCommandMutex[1]);
						}
						break;

					case 2:
						if (xSemaphoreTake(xLightCommandMutex[2], portMAX_DELAY) == pdTRUE)
						{
						memcpy((char*)&light_para.chan3LastCommand, line_setLastCommand, sizeof(line_setLastCommand));
						light_LastCommandPara[Chan].lastCommandTime = get_current_time_ms(); // Update last command time in milliseconds
						xSemaphoreGive(xLightCommandMutex[2]);
					}
						break;

					case 3:
						if (xSemaphoreTake(xLightCommandMutex[3], portMAX_DELAY) == pdTRUE)
						{
						memcpy((char*)&light_para.chan4LastCommand, line_setLastCommand, sizeof(line_setLastCommand));
						light_LastCommandPara[Chan].lastCommandTime = get_current_time_ms(); // Update last command time in milliseconds
						xSemaphoreGive(xLightCommandMutex[3]);
					}
						break;
				 }
			 }
		 }
		 else if(BsaveFlag == 2) // save command
		 {
			 //execute command

			 int ret;

			 if(line_setLastCommand[0]=='<')
			 {
				esp_err_t err = esp_console_run_Custom(line_setLastCommand, &ret, THIS_ACTOR);

				if (err == ESP_ERR_NOT_FOUND)
				{
	#ifdef ENABLE_PRINT_MSG
					printf("Unrecognized command\n");
	#endif
				}
				else if (err == ESP_ERR_INVALID_ARG)
				{
					// command was empty
				} else if (err == ESP_OK && ret != ESP_OK)
				{
	#ifdef ENABLE_PRINT_MSG
					printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
					esp_err_to_name(ret));
	#endif
				}
				else if (err != ESP_OK)
				{
	#ifdef ENABLE_PRINT_MSG
					printf("Internal error: %s\n", esp_err_to_name(err));
	#endif
				}
			}
		 }
	}
	else
	{
#ifdef ENABLE_PRINT_MSG
		printf("In strip channel off \n");
#endif
		StripChanOFF(Chan+1);	//Off strip
	}

#ifdef ENABLE_PRINT_MSG
	printf("In end \n");
#endif
	Last_CommandFlag[Chan] = 2;
}

static void Set_Command_Property(uint8_t Chan)
{
	char line2[10] = {0};

	strcpy(line_Set_Cmd, "<LIGHTING.SET(");

//------------------
	command_run_at_power_up = 0;

    cJSON *json_Command = cJSON_CreateObject();
    if (json_Command == NULL) {
        fprintf(stderr, "Error creating cJSON object\n");
    }

    if(Chan == 1)
    {
    	cJSON_AddStringToObject(json_Command, "CHAN1LASTCOMMAND", (char*)&light_para.chan1LastCommand);
    }
    if(Chan == 2)
	{
		cJSON_AddStringToObject(json_Command, "CHAN2LASTCOMMAND", (char*)&light_para.chan2LastCommand);
	}
    if(Chan == 3)
	{
		cJSON_AddStringToObject(json_Command, "CHAN3LASTCOMMAND", (char*)&light_para.chan3LastCommand);
	}
    if(Chan == 4)
	{
		cJSON_AddStringToObject(json_Command, "CHAN4LASTCOMMAND", (char*)&light_para.chan4LastCommand);
	}

    // Output the JSON string
   
 payLoadData_Running[0]='\0';
	cJSON_PrintPreallocated(json_Command, payLoadData_Running, sizeof(payLoadData_Running), false);
#ifdef ENABLE_PRINT_MSG
    printf("json_string_Command = %s\n", payLoadData_Running);
#endif

    strncat(line_Set_Cmd, payLoadData_Running, COMMAND_LEN - strlen(line_Set_Cmd) - 1U);
	strcpy(line2, ")");
	strncat(line_Set_Cmd, line2, COMMAND_LEN - strlen(line_Set_Cmd) - 1U);

#ifdef ENABLE_PRINT_MSG
	printf("line_Set_Cmd= %s \n", line_Set_Cmd);
	printf("line_Set_Cmd= %s \n", line_Set_Cmd);
#endif
	int ret;
	if(line_Set_Cmd[0]=='<')
	{
		esp_err_t err = esp_console_run_Custom(line_Set_Cmd, &ret, THIS_ACTOR);
		if (err == ESP_ERR_NOT_FOUND)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Unrecognized command\n");
	#endif
		}
		else if (err == ESP_ERR_INVALID_ARG)
		{
			// command was empty
		}
		else if (err == ESP_OK && ret != ESP_OK)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
			esp_err_to_name(ret));
	#endif
		}
		else if (err != ESP_OK)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Internal error: %s\n", esp_err_to_name(err));
	#endif
		}
//		vTaskDelay(300/portTICK_PERIOD_MS);
	}
    cJSON_Delete(json_Command);
}

typedef struct { bool valid; float brightness; float hue, sat, val; } HueSatTemplate;
typedef struct { bool valid; float brightness; float red, green, blue; } SetColorTemplate;
typedef struct { bool valid; float brightness; int index; } ColorIndexTemplate;
typedef struct { bool valid; float brightness; float spacing, speed; } TapeMeasureTemplate;
typedef struct { bool valid; float brightness; uint8_t typeColor; float period_ms; } PatternTemplate;

typedef struct { bool valid; float brightness; float start[3], end[3], intensity, width, decay; } SparkleTemplate;
typedef struct { bool valid; float brightness; int numColors; uint16_t colors[MAX_MultiColorSparkle_COLORS][3]; uint16_t endColor[3]; float intensity, width, decay; } MultiColorSparkleTemplate;
typedef struct { bool valid; float brightness; float start[3], peak[3], valley[3]; float amp[3], wave[3], speed[3]; } RippleTemplate;
typedef struct { bool valid; float brightness; customImage img; } CustomTemplate;
typedef struct { bool valid; float brightness; RacingParams params; } RacingTemplate;
typedef struct {
    bool valid;
    float brightness;
    int numColors;
    MarqueeColor_t colors[MAX_MARQUEE_COLORS];
    int transitionType;
    int enableMirror;
    float mirrorPosition;
    float oscAmp;
    float oscPeriod;
    float movingSpeed;
    float brightnessWavelength;
    float brightnessAmplitude;
    float brightnessSpeed;
    uint8_t spacingOverride;
} MarqueeTemplate;

enum {
    PARSED_EXEC_NONE = 0,
    PARSED_EXEC_PATTERN,
    PARSED_EXEC_HUESAT,
    PARSED_EXEC_SPARKLE,
    PARSED_EXEC_MULTICOLORSPARKLE,
    PARSED_EXEC_RIPPLE,
    PARSED_EXEC_CUSTOM,
    PARSED_EXEC_RACING,
    PARSED_EXEC_SETCOLOR,
    PARSED_EXEC_TAPEMEASURE,
    PARSED_EXEC_COLORINDEX,
    PARSED_EXEC_MARQUEE
};

static const char *parsed_exec_kind_name(int kind)
{
    switch (kind) {
        case PARSED_EXEC_PATTERN: return "PATTERN";
        case PARSED_EXEC_HUESAT: return "HUESAT";
        case PARSED_EXEC_SPARKLE: return "SPARKLE";
        case PARSED_EXEC_MULTICOLORSPARKLE: return "MULTICOLORSPARKLE";
        case PARSED_EXEC_RIPPLE: return "RIPPLE";
        case PARSED_EXEC_CUSTOM: return "CUSTOM";
        case PARSED_EXEC_RACING: return "RACING";
        case PARSED_EXEC_SETCOLOR: return "SETCOLOR";
        case PARSED_EXEC_TAPEMEASURE: return "TAPEMEASURE";
        case PARSED_EXEC_COLORINDEX: return "COLORINDEX";
        case PARSED_EXEC_MARQUEE: return "MARQUEE";
        default: return "NONE";
    }
}

static HueSatTemplate g_template_huesat = {0};
static SetColorTemplate g_template_setcolor = {0};
static ColorIndexTemplate g_template_colorindex = {0};
static TapeMeasureTemplate g_template_tapemeasure = {0};
static PatternTemplate g_template_pattern = {0};
static SparkleTemplate g_template_sparkle = {0};
static MultiColorSparkleTemplate g_template_multicolorsparkle = {0};
static RippleTemplate g_template_ripple = {0};
static CustomTemplate g_template_executecustom = {0};
static RacingTemplate g_template_executeracing = {0};
static MarqueeTemplate g_template_executemarquee = {0};

static float apply_runtime_brightness(int channel, float brightness)
{
    float factor = light_para.contrMaxB_float * 0.01f;
    if (channel == 1) factor *= light_para.chan1MaxB_float * 0.01f;
    else if (channel == 2) factor *= light_para.chan2MaxB_float * 0.01f;
    else if (channel == 3) factor *= light_para.chan3MaxB_float * 0.01f;
    else factor *= light_para.chan4MaxB_float * 0.01f;
    if (functionNullFlag == 1) brightness = brightness_RunTimeChan[channel - 1];
    else brightness_RunTimeChan[channel - 1] = brightness;
    if (brightness != 0.0f) factor *= (brightness * 0.01f);
    return factor;
}

static cJSON *cfg_find(const cJSON *obj, const char *const keys[], size_t key_count)
{
    return find_json_field_case_insensitive(obj, keys, key_count);
}

static bool save_parsed_template_snapshot(CommandEntry *entry, int kind, const void *src, size_t size)
{
    if (entry == NULL || src == NULL || size == 0 || size > 65535u) {
        return false;
    }
    if (entry->parsed_exec_blob != NULL) {
        heap_caps_free(entry->parsed_exec_blob);
        entry->parsed_exec_blob = NULL;
    }
    entry->parsed_exec_blob = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (entry->parsed_exec_blob == NULL) {
        entry->parsed_exec_blob_size = 0;
        entry->parsed_exec_kind = PARSED_EXEC_NONE;
        return false;
    }
    memcpy(entry->parsed_exec_blob, src, size);
    entry->parsed_exec_blob_size = (uint16_t)size;
    entry->parsed_exec_kind = (uint8_t)kind;
    return true;
}

static int execute_parsed_command_snapshot(AMessage_st *msg, const CommandEntry *command, int channel, float brightness_override)
{
    if (command == NULL || command->parsed_exec_blob == NULL || command->parsed_exec_blob_size == 0) {
        return -1;
    }
    switch (command->parsed_exec_kind) {
        case PARSED_EXEC_PATTERN: {
            g_template_pattern = *(const PatternTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_pattern.brightness = brightness_override;
            return execute_pattern_template(msg, channel);
        }
        case PARSED_EXEC_HUESAT: {
            g_template_huesat = *(const HueSatTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_huesat.brightness = brightness_override;
            return execute_huesat_template(msg, channel);
        }
        case PARSED_EXEC_SPARKLE: {
            g_template_sparkle = *(const SparkleTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_sparkle.brightness = brightness_override;
            return execute_sparkle_template(msg, channel);
        }
        case PARSED_EXEC_MULTICOLORSPARKLE: {
            g_template_multicolorsparkle = *(const MultiColorSparkleTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_multicolorsparkle.brightness = brightness_override;
            return execute_multicolorsparkle_template(msg, channel);
        }
        case PARSED_EXEC_RIPPLE: {
            g_template_ripple = *(const RippleTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_ripple.brightness = brightness_override;
            return execute_ripple_template(msg, channel);
        }
        case PARSED_EXEC_CUSTOM: {
            g_template_executecustom = *(const CustomTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_executecustom.brightness = brightness_override;
            return execute_executecustom_template(msg, channel);
        }
        case PARSED_EXEC_RACING: {
            g_template_executeracing = *(const RacingTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_executeracing.brightness = brightness_override;
            return execute_executeracing_template(msg, channel);
        }
        case PARSED_EXEC_SETCOLOR: {
            g_template_setcolor = *(const SetColorTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_setcolor.brightness = brightness_override;
            return execute_setcolor_template(msg, channel);
        }
        case PARSED_EXEC_TAPEMEASURE: {
            g_template_tapemeasure = *(const TapeMeasureTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_tapemeasure.brightness = brightness_override;
            return execute_tapemeasure_template(msg, channel);
        }
        case PARSED_EXEC_COLORINDEX: {
            g_template_colorindex = *(const ColorIndexTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_colorindex.brightness = brightness_override;
            return execute_colorindex_template(msg, channel);
        }
        case PARSED_EXEC_MARQUEE: {
            g_template_executemarquee = *(const MarqueeTemplate *)command->parsed_exec_blob;
            if (brightness_override >= 0.0f) g_template_executemarquee.brightness = brightness_override;
            return execute_executemarquee_template(msg, channel);
        }
        default:
            return -1;
    }
}

static int parse_huesat_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    cJSON *h = cJSON_GetObjectItem((cJSON *)configItem, "HUE");
    cJSON *s = cJSON_GetObjectItem((cJSON *)configItem, "SAT");
    cJSON *v = cJSON_GetObjectItem((cJSON *)configItem, "VAL");
    if (!cJSON_IsNumber(h) || !cJSON_IsNumber(s) || !cJSON_IsNumber(v)) return -1;
    g_template_huesat.valid = true;
    g_template_huesat.brightness = brightness;
    g_template_huesat.hue = (float)h->valuedouble;
    g_template_huesat.sat = (float)s->valuedouble;
    g_template_huesat.val = (float)v->valuedouble;
    return 1;
}

static int execute_huesat_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_huesat.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    bytes_per_channel[channel - 1] = 200;
    float bfactor = apply_runtime_brightness(channel, g_template_huesat.brightness);
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            rampData[channel-1].hue_end[i] = g_template_huesat.hue;
            rampData[channel-1].sat_end[i] = g_template_huesat.sat;
            rampData[channel-1].val_end[i] = g_template_huesat.val;
        }
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        uint16_t red = 0, green = 0, blue = 0;
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            rampData[channel-1].hue_start[i] = g_template_huesat.hue;
            rampData[channel-1].sat_start[i] = g_template_huesat.sat;
            rampData[channel-1].val_start[i] = g_template_huesat.val;
        }
        hsv_to_rgb_16bit(g_template_huesat.hue, g_template_huesat.sat, 100, &red, &green, &blue);
        restrict_and_scale_RGB(&red, &green, &blue, g_template_huesat.val * bfactor);
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) set_led_color((uint8_t)channel, (uint16_t)(i + 1), red, green, blue);
        PrepareDataWithModeSetting(0, channel - 1, 1);
        HueSatStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_setcolor_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    cJSON *r = cJSON_GetObjectItem((cJSON *)configItem, "RED");
    cJSON *g = cJSON_GetObjectItem((cJSON *)configItem, "GREEN");
    cJSON *b = cJSON_GetObjectItem((cJSON *)configItem, "BLUE");
    if (!cJSON_IsNumber(r) || !cJSON_IsNumber(g) || !cJSON_IsNumber(b)) return -1;
    g_template_setcolor.valid = true;
    g_template_setcolor.brightness = brightness;
    g_template_setcolor.red = (float)r->valuedouble;
    g_template_setcolor.green = (float)g->valuedouble;
    g_template_setcolor.blue = (float)b->valuedouble;
    return 1;
}

static int execute_setcolor_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_setcolor.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    uint16_t red = 0, green = 0, blue = 0;
    float hue = 0, sat = 0, val = 0;
    float red1 = ((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1) ? (g_template_setcolor.red * 65535.0f * 0.01f) : (g_template_setcolor.red * 255.0f * 0.01f);
    float green1 = ((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1) ? (g_template_setcolor.green * 65535.0f * 0.01f) : (g_template_setcolor.green * 255.0f * 0.01f);
    float blue1 = ((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1) ? (g_template_setcolor.blue * 65535.0f * 0.01f) : (g_template_setcolor.blue * 255.0f * 0.01f);
    TurnFlagsOff(channel);
    bytes_per_channel[channel - 1] = 200;
    float bfactor = apply_runtime_brightness(channel, g_template_setcolor.brightness);
    red = (uint16_t)(red1 * bfactor);
    green = (uint16_t)(green1 * bfactor);
    blue = (uint16_t)(blue1 * bfactor);
    rgb16_to_hsv(red, green, blue, &hue, &sat, &val);
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            rampData[channel-1].hue_end[i] = hue;
            rampData[channel-1].sat_end[i] = sat;
            rampData[channel-1].val_end[i] = val;
        }
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
            rampData[channel-1].hue_start[i] = hue;
            rampData[channel-1].sat_start[i] = sat;
            rampData[channel-1].val_start[i] = val;
        }
        restrict_and_scale_RGB(&red, &green, &blue, 100);
        for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) set_led_color((uint8_t)channel, (uint16_t)(i + 1), red, green, blue);
        PrepareDataWithModeSetting(0, channel - 1, 1);
        setColorStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_colorindex_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    cJSON *idx = cJSON_GetObjectItem((cJSON *)configItem, "Index");
    if (!cJSON_IsNumber(idx)) return -1;
    g_template_colorindex.valid = true;
    g_template_colorindex.brightness = brightness;
    g_template_colorindex.index = idx->valueint;
    return 1;
}

static int execute_colorindex_template(AMessage_st* s_Message_Rx, int channel)
{
    if (!g_template_colorindex.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    float hue = 0, sat = 0, val = 0;
    int found = 0;
    for (int i = 0; i < 60; i++) {
        if (Color_table[i].ColorIndex == g_template_colorindex.index) {
            hue = Color_table[i].Hue;
            sat = Color_table[i].Saturation;
            val = Color_table[i].Value;
            found = 1;
            break;
        }
    }
    if (!found) return -1;
    g_template_huesat.valid = true;
    g_template_huesat.brightness = g_template_colorindex.brightness;
    g_template_huesat.hue = hue;
    g_template_huesat.sat = sat;
    g_template_huesat.val = val;
    if (execute_huesat_template(s_Message_Rx, channel) == 1) {
        colorIndexStartFlag[channel-1] = 1;
        return 1;
    }
    return -1;
}

static int parse_pattern_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const type_keys[] = {"TYPE", "Type"};
    static const char *const period_keys[] = {"PERIOD", "Period"};
    cJSON *typeItem = cfg_find(configItem, type_keys, sizeof(type_keys) / sizeof(type_keys[0]));
    cJSON *periodItem = cfg_find(configItem, period_keys, sizeof(period_keys) / sizeof(period_keys[0]));
    if (!cJSON_IsString(typeItem)) return -1;
    uint8_t t = 0;
    if (!strcmp(typeItem->valuestring, "All Colors")) t = 1;
    else if (!strcmp(typeItem->valuestring, "Warm Colors")) t = 2;
    else if (!strcmp(typeItem->valuestring, "Cool Colors")) t = 3;
    else return -1;
    g_template_pattern.valid = true;
    g_template_pattern.brightness = brightness;
    g_template_pattern.typeColor = t;
    g_template_pattern.period_ms = (cJSON_IsNumber(periodItem) && periodItem->valuedouble != 0.0) ? (float)(periodItem->valuedouble * 1000.0) : 500.0f;
    return 1;
}

static int execute_pattern_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_pattern.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    uint64_t now = get_current_time_ms();
    float p = g_template_pattern.period_ms / 2.0f;
    uint16_t *arr = NULL;
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        periodPattern_float_end[channel-1] = p;
        PatternStartFlag_end[channel-1] = 1;
        pattern_Start_time_end[channel-1] = now;
        arr = NewPattern_u16Hue_end[channel-1];
        rampData[channel-1].RampStartTime = now;
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        periodPattern_float_start[channel-1] = p;
        PatternStartFlag_start[channel-1] = 1;
        pattern_Start_time_start[channel-1] = now;
        arr = NewPattern_u16Hue_start[channel-1];
    }
    if (g_template_pattern.typeColor == 1) { arr[0] = 0; arr[1] = 180; arr[2] = 360; }
    else if (g_template_pattern.typeColor == 2) { arr[0] = 0; arr[1] = 60; arr[2] = 0; }
    else { arr[0] = 248; arr[1] = 129; arr[2] = 248; }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_tapemeasure_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const spacing_keys[] = {"Spacing", "SPACING"};
    static const char *const speed_keys[] = {"Speed", "SPEED"};
    cJSON *spacing = cfg_find(configItem, spacing_keys, sizeof(spacing_keys) / sizeof(spacing_keys[0]));
    cJSON *speed = cfg_find(configItem, speed_keys, sizeof(speed_keys) / sizeof(speed_keys[0]));
    if (!cJSON_IsNumber(spacing) || !cJSON_IsNumber(speed)) return -1;
    g_template_tapemeasure.valid = true;
    g_template_tapemeasure.brightness = brightness;
    g_template_tapemeasure.spacing = (float)spacing->valuedouble;
    g_template_tapemeasure.speed = (float)speed->valuedouble;
    return 1;
}

static int execute_tapemeasure_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_tapemeasure.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    int ramp = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0));
    customImage *img = ramp ? &ImageConfig_end[channel-1] : &ImageConfig_start[channel-1];
    float led_in = floor(ChannelParamObject[channel-1].LEDspacingCh_float) / 25.4f;
    float spacing = g_template_tapemeasure.spacing < led_in ? led_in : g_template_tapemeasure.spacing;
    float pad = spacing - led_in;
    if (pad < 0) pad = 0;
    img->spacingOverride = 0;
    img->numColors = 4;
    img->colorLength = led_in;
    img->paddingLength = pad;
    img->transitionType = 0;
    img->mirror = 0;
    img->mirrorPosition = 0;
    img->oscAmplitude = 0;
    img->oscPeriod = 0;
    img->movingSpeed = -g_template_tapemeasure.speed;
    img->colorSelections[0].hue = 0; img->colorSelections[0].saturation = 100; img->colorSelections[0].brightness = 100;
    img->colorSelections[1].hue = 120; img->colorSelections[1].saturation = 100; img->colorSelections[1].brightness = 100;
    img->colorSelections[2].hue = 240; img->colorSelections[2].saturation = 100; img->colorSelections[2].brightness = 100;
    img->colorSelections[3].hue = 0; img->colorSelections[3].saturation = 0; img->colorSelections[3].brightness = 100;
    img->paddingColor.hue = 0; img->paddingColor.saturation = 0; img->paddingColor.brightness = 0;
    ImageSize_forMode[channel-1] = img->numColors * (img->colorLength + img->paddingLength);
    if ((EXAMPLE_LED_NUMBERS * 3) < ImageSize_forMode[channel-1]) ImageSize_forMode[channel-1] = (EXAMPLE_LED_NUMBERS * 3);
    bytes_per_channel[channel-1] = ImageSize_forMode[channel-1];
    (void)apply_runtime_brightness(channel, g_template_tapemeasure.brightness);
    if (g_template_tapemeasure.speed == 0) {
        One_LED_time[channel-1] = 0;
        TapeMeasureStartFlag_offset[channel-1] = 1;
    } else {
        float n = (led_in > 0.0f) ? (1.0f / led_in) : 1.0f;
        One_LED_time[channel-1] = (int)(1000.0f / (n * g_template_tapemeasure.speed));
    }
    if (ramp) {
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        Execute_PrepareDataWithModeSetting(0, channel-1, 1, 0);
        TapeMeasureStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_sparkle_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const s_keys[] = {"STARTCOLOR", "startColor"};
    static const char *const e_keys[] = {"ENDCOLOR", "endColor"};
    static const char *const i_keys[] = {"INTENSITY", "intensity"};
    static const char *const w_keys[] = {"WIDTH", "width"};
    static const char *const d_keys[] = {"DECAYTIME", "decayTime"};
    cJSON *s = cfg_find(configItem, s_keys, sizeof(s_keys) / sizeof(s_keys[0]));
    cJSON *e = cfg_find(configItem, e_keys, sizeof(e_keys) / sizeof(e_keys[0]));
    cJSON *i = cfg_find(configItem, i_keys, sizeof(i_keys) / sizeof(i_keys[0]));
    cJSON *w = cfg_find(configItem, w_keys, sizeof(w_keys) / sizeof(w_keys[0]));
    cJSON *d = cfg_find(configItem, d_keys, sizeof(d_keys) / sizeof(d_keys[0]));
    if (!cJSON_IsArray(s) || !cJSON_IsArray(e) || !cJSON_IsNumber(i) || !cJSON_IsNumber(w) || !cJSON_IsNumber(d)) return -1;
    for (int k = 0; k < 3; k++) {
        g_template_sparkle.start[k] = (float)cJSON_GetArrayItem(s, k)->valuedouble;
        g_template_sparkle.end[k] = (float)cJSON_GetArrayItem(e, k)->valuedouble;
    }
    g_template_sparkle.intensity = (float)i->valuedouble;
    g_template_sparkle.width = (float)w->valuedouble;
    g_template_sparkle.decay = (float)d->valuedouble;
    if (g_template_sparkle.decay > 0 && g_template_sparkle.decay < 100) g_template_sparkle.decay = 100;
    g_template_sparkle.brightness = brightness;
    g_template_sparkle.valid = true;
    return 1;
}

static int execute_sparkle_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_sparkle.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    bytes_per_channel[channel-1] = 200;
    (void)apply_runtime_brightness(channel, g_template_sparkle.brightness);
    SparkleParameters *sp = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0))
        ? &SparkleParamObject_end[channel-1] : &SparkleParamObject_start[channel-1];
    sp->Width = g_template_sparkle.width;
    sp->Intensity = g_template_sparkle.intensity;
    sp->Decaytime = g_template_sparkle.decay;
    for (int k = 0; k < 3; k++) {
        sp->StartColor_float[k] = g_template_sparkle.start[k];
        sp->EndColor_float[k] = g_template_sparkle.end[k];
    }
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        SparkleStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_multicolorsparkle_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const colors_keys[] = {"Colors", "COLORS"};
    static const char *const end_keys[] = {"ENDCOLOR", "endColor"};
    static const char *const i_keys[] = {"INTENSITY", "intensity"};
    static const char *const w_keys[] = {"WIDTH", "width"};
    static const char *const d_keys[] = {"DECAYTIME", "decayTime"};
    cJSON *colors = cfg_find(configItem, colors_keys, sizeof(colors_keys) / sizeof(colors_keys[0]));
    cJSON *end = cfg_find(configItem, end_keys, sizeof(end_keys) / sizeof(end_keys[0]));
    cJSON *i = cfg_find(configItem, i_keys, sizeof(i_keys) / sizeof(i_keys[0]));
    cJSON *w = cfg_find(configItem, w_keys, sizeof(w_keys) / sizeof(w_keys[0]));
    cJSON *d = cfg_find(configItem, d_keys, sizeof(d_keys) / sizeof(d_keys[0]));
    if (!cJSON_IsArray(colors) || !cJSON_IsArray(end) || !cJSON_IsNumber(i) || !cJSON_IsNumber(w) || !cJSON_IsNumber(d)) return -1;
    int n = cJSON_GetArraySize(colors);
    if (n > MAX_MultiColorSparkle_COLORS) n = MAX_MultiColorSparkle_COLORS;
    g_template_multicolorsparkle.numColors = n;
    memset(g_template_multicolorsparkle.colors, 0, sizeof(g_template_multicolorsparkle.colors));
    for (int x = 0; x < n; x++) {
        cJSON *obj = cJSON_GetArrayItem(colors, x);
        static const char *const color_keys[] = {"color", "Color"};
        cJSON *arr = cfg_find(obj, color_keys, sizeof(color_keys) / sizeof(color_keys[0]));
        if (cJSON_IsArray(arr) && cJSON_GetArraySize(arr) == 3) {
            g_template_multicolorsparkle.colors[x][0] = (uint16_t)cJSON_GetArrayItem(arr, 0)->valueint;
            g_template_multicolorsparkle.colors[x][1] = (uint16_t)cJSON_GetArrayItem(arr, 1)->valueint;
            g_template_multicolorsparkle.colors[x][2] = (uint16_t)cJSON_GetArrayItem(arr, 2)->valueint;
        }
    }
    for (int k = 0; k < 3; k++) g_template_multicolorsparkle.endColor[k] = (uint16_t)cJSON_GetArrayItem(end, k)->valueint;
    g_template_multicolorsparkle.intensity = (float)i->valuedouble;
    g_template_multicolorsparkle.width = (float)w->valuedouble;
    g_template_multicolorsparkle.decay = (float)d->valuedouble;
    if (g_template_multicolorsparkle.decay > 0 && g_template_multicolorsparkle.decay < 100) g_template_multicolorsparkle.decay = 100;
    g_template_multicolorsparkle.brightness = brightness;
    g_template_multicolorsparkle.valid = true;
    return 1;
}

static int execute_multicolorsparkle_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_multicolorsparkle.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    (void)apply_runtime_brightness(channel, g_template_multicolorsparkle.brightness);
    MultiColorSparkleParameters *mp = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0))
        ? &MultiColorSparkleParamObject_end[channel-1] : &MultiColorSparkleParamObject_start[channel-1];
    mp->Width = g_template_multicolorsparkle.width;
    mp->Intensity = g_template_multicolorsparkle.intensity;
    mp->Decaytime = g_template_multicolorsparkle.decay;
    mp->numColors = g_template_multicolorsparkle.numColors;
    memcpy(mp->MultiColor1_uint16, g_template_multicolorsparkle.colors[0], sizeof(mp->MultiColor1_uint16));
    memcpy(mp->MultiColor2_uint16, g_template_multicolorsparkle.colors[1], sizeof(mp->MultiColor2_uint16));
    memcpy(mp->MultiColor3_uint16, g_template_multicolorsparkle.colors[2], sizeof(mp->MultiColor3_uint16));
    memcpy(mp->MultiColor4_uint16, g_template_multicolorsparkle.colors[3], sizeof(mp->MultiColor4_uint16));
    memcpy(mp->MultiColor5_uint16, g_template_multicolorsparkle.colors[4], sizeof(mp->MultiColor5_uint16));
    memcpy(mp->MultiColor6_uint16, g_template_multicolorsparkle.colors[5], sizeof(mp->MultiColor6_uint16));
    memcpy(mp->MultiColor7_uint16, g_template_multicolorsparkle.colors[6], sizeof(mp->MultiColor7_uint16));
    memcpy(mp->MultiColor8_uint16, g_template_multicolorsparkle.colors[7], sizeof(mp->MultiColor8_uint16));
    memcpy(mp->EndColor_uint16, g_template_multicolorsparkle.endColor, sizeof(mp->EndColor_uint16));
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        MultiColorSparkleStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_ripple_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const s_keys[] = {"STARTCOLOR", "startColor"};
    static const char *const p_keys[] = {"PEAKCOLOR", "peakColor"};
    static const char *const v_keys[] = {"VALLEYCOLOR", "valleyColor"};
    cJSON *s = cfg_find(configItem, s_keys, sizeof(s_keys) / sizeof(s_keys[0]));
    cJSON *p = cfg_find(configItem, p_keys, sizeof(p_keys) / sizeof(p_keys[0]));
    cJSON *v = cfg_find(configItem, v_keys, sizeof(v_keys) / sizeof(v_keys[0]));
    if (!cJSON_IsArray(s) || !cJSON_IsArray(p) || !cJSON_IsArray(v)) return -1;
    for (int k = 0; k < 3; k++) {
        g_template_ripple.start[k] = (float)cJSON_GetArrayItem(s, k)->valuedouble;
        g_template_ripple.peak[k] = (float)cJSON_GetArrayItem(p, k)->valuedouble;
        g_template_ripple.valley[k] = (float)cJSON_GetArrayItem(v, k)->valuedouble;
    }
    const char *ak[] = {"AMP1","AMP2","AMP3"};
    const char *wk[] = {"WAVE1","WAVE2","WAVE3"};
    const char *sk[] = {"SPEED1","SPEED2","SPEED3"};
    const char *ak_l[] = {"amp1","amp2","amp3"};
    const char *wk_l[] = {"wave1","wave2","wave3"};
    const char *sk_l[] = {"speed1","speed2","speed3"};
    for (int i = 0; i < 3; i++) {
        cJSON *a = cJSON_GetObjectItem((cJSON *)configItem, ak[i]);
        cJSON *w = cJSON_GetObjectItem((cJSON *)configItem, wk[i]);
        cJSON *sp = cJSON_GetObjectItem((cJSON *)configItem, sk[i]);
        if (!cJSON_IsNumber(a)) a = cJSON_GetObjectItem((cJSON *)configItem, ak_l[i]);
        if (!cJSON_IsNumber(w)) w = cJSON_GetObjectItem((cJSON *)configItem, wk_l[i]);
        if (!cJSON_IsNumber(sp)) sp = cJSON_GetObjectItem((cJSON *)configItem, sk_l[i]);
        if (!cJSON_IsNumber(a) || !cJSON_IsNumber(w) || !cJSON_IsNumber(sp)) return -1;
        g_template_ripple.amp[i] = (float)a->valuedouble;
        g_template_ripple.wave[i] = (float)w->valuedouble;
        g_template_ripple.speed[i] = (float)sp->valuedouble;
    }
    g_template_ripple.brightness = brightness;
    g_template_ripple.valid = true;
    return 1;
}

static int execute_ripple_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_ripple.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    bytes_per_channel[channel-1] = 200;
    float bfactor = apply_runtime_brightness(channel, g_template_ripple.brightness);
    Color *sc = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) ? &startColor_end[channel-1] : &startColor_start[channel-1];
    Color *pc = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) ? &peakColor_end[channel-1] : &peakColor_start[channel-1];
    Color *vc = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) ? &valleyColor_end[channel-1] : &valleyColor_start[channel-1];
    SineWave *w = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) ? waves_end[channel-1] : waves_start[channel-1];
    SineWave *w1 = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) ? waves1_end[channel-1] : waves1_start[channel-1];
    sc->hue = g_template_ripple.start[0]; sc->saturation = g_template_ripple.start[1] * 0.01f; sc->brightness = (g_template_ripple.start[2] * bfactor) * 0.01f;
    pc->hue = g_template_ripple.peak[0]; pc->saturation = g_template_ripple.peak[1] * 0.01f; pc->brightness = (g_template_ripple.peak[2] * bfactor) * 0.01f;
    vc->hue = g_template_ripple.valley[0]; vc->saturation = g_template_ripple.valley[1] * 0.01f; vc->brightness = (g_template_ripple.valley[2] * bfactor) * 0.01f;
    for (int i = 0; i < 3; i++) {
        w[i].amplitude = g_template_ripple.amp[i];
        w[i].wavelength = g_template_ripple.wave[i];
        if (w[i].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4f)) w[i].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4f) + 0.01f;
        w[i].speed = -g_template_ripple.speed[i];
        w1[i].wavelength = (w[i].wavelength * 25.4f) / ChannelParamObject[channel-1].LEDspacingCh_float;
        w1[i].speed = w[i].speed;
        w1[i].amplitude = w[i].amplitude;
    }
    if ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0)) {
        rampData[channel-1].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[channel-1] = 1;
    } else {
        RippleStartFlag[channel-1] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_executecustom_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    customImage *img = &g_template_executecustom.img;
    memset(img, 0, sizeof(*img));
    cJSON *arr = cJSON_GetObjectItemCaseSensitive((cJSON *)configItem, "colorSelections");
    if (!cJSON_IsArray(arr)) return -1;
    int n = cJSON_GetArraySize(arr);
    if (n <= 0) return -1;
    if (n > MAX_COLORS) n = MAX_COLORS;
    img->numColors = n;
    for (int i = 0; i < n; i++) {
        cJSON *item = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsString(item)) sscanf(item->valuestring, "%f , %f , %f", &img->colorSelections[i].hue, &img->colorSelections[i].saturation, &img->colorSelections[i].brightness);
    }
    cJSON *bg = cJSON_GetObjectItem((cJSON *)configItem, "bgColor");
    if (cJSON_IsArray(bg) && cJSON_GetArraySize(bg) >= 3) {
        img->paddingColor.hue = (float)cJSON_GetArrayItem(bg, 0)->valuedouble;
        img->paddingColor.saturation = (float)cJSON_GetArrayItem(bg, 1)->valuedouble;
        img->paddingColor.brightness = (float)cJSON_GetArrayItem(bg, 2)->valuedouble;
    }
    cJSON *v = cJSON_GetObjectItem((cJSON *)configItem, "colorLength"); if (cJSON_IsNumber(v)) img->colorLength = (float)v->valuedouble;
    v = cJSON_GetObjectItem((cJSON *)configItem, "paddingLength"); if (cJSON_IsNumber(v)) img->paddingLength = (float)v->valuedouble;
    cJSON *tt = cJSON_GetObjectItem((cJSON *)configItem, "transitionType"); if (cJSON_IsString(tt)) img->transitionType = (!strcmp(tt->valuestring, "None")) ? 0 : 1;
    v = cJSON_GetObjectItem((cJSON *)configItem, "enableMirror"); if (cJSON_IsNumber(v)) img->mirror = (uint8_t)v->valueint;
    v = cJSON_GetObjectItem((cJSON *)configItem, "mirrorPosition"); if (cJSON_IsNumber(v)) img->mirrorPosition = (float)v->valuedouble;
    v = cJSON_GetObjectItem((cJSON *)configItem, "oscAmp"); if (cJSON_IsNumber(v)) img->oscAmplitude = (float)v->valuedouble;
    v = cJSON_GetObjectItem((cJSON *)configItem, "oscPeriod"); if (cJSON_IsNumber(v)) img->oscPeriod = (float)v->valuedouble;
    v = cJSON_GetObjectItem((cJSON *)configItem, "movingSpeed"); if (cJSON_IsNumber(v)) img->movingSpeed = (float)v->valuedouble;
    v = cJSON_GetObjectItem((cJSON *)configItem, "spacingOverride"); if (cJSON_IsNumber(v)) img->spacingOverride = (uint8_t)v->valueint;
    g_template_executecustom.brightness = brightness;
    g_template_executecustom.valid = true;
    return 1;
}

static int execute_executecustom_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_executecustom.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    int ramp = ((rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0));
    customImage *img = ramp ? &ImageConfig_end[channel-1] : &ImageConfig_start[channel-1];
    *img = g_template_executecustom.img;
    if (img->spacingOverride == 1) {
        float pitch = (ChannelParamObject[channel-1].LEDspacingCh_float)/25.4f;
        img->colorLength *= pitch;
        img->paddingLength *= pitch;
        img->movingSpeed *= pitch;
    }
    img->movingSpeed = -img->movingSpeed;
    ImageSize_forMode[channel-1] = img->numColors * (img->colorLength + img->paddingLength);
    if ((EXAMPLE_LED_NUMBERS * 3) < ImageSize_forMode[channel-1]) ImageSize_forMode[channel-1] = (EXAMPLE_LED_NUMBERS * 3);
    bytes_per_channel[channel-1] = ImageSize_forMode[channel-1];
    (void)apply_runtime_brightness(channel, g_template_executecustom.brightness);
    if (!ramp) Execute_PrepareDataWithModeSetting(0, channel-1, 1, 0);
    float sp = img->movingSpeed;
    if (sp == 0) { ExecuteCustomStartFlag_offset1[channel-1] = 1; One_LED_time[channel-1] = 0; }
    else if (sp < 0) { ChannelParamObject[channel-1].speedrevDirCh_u8 = 1; One_LED_time[channel-1] = (int)((1 * 1000) / (-sp)); }
    else { ChannelParamObject[channel-1].speedrevDirCh_u8 = 0; One_LED_time[channel-1] = (int)((1 * 1000) / sp); }
    oscP_Flag[channel-1] = ((img->oscAmplitude != 0) && (img->oscPeriod != 0)) ? 1 : 0;
    enableMirror_uint8[channel-1] = img->mirror;
    MirrorLedNum[channel-1] = (int)(((img->mirrorPosition) * 25.4f) / ChannelParamObject[channel-1].LEDspacingCh_float);
    if (!ramp) ExecuteCustomStartFlag[channel-1] = 1;
    else { rampData[channel-1].RampStartTime = get_current_time_ms(); ExecuteSceneRampFlag[channel-1] = 1; }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_executeracing_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    Racing_DefaultParams(&g_template_executeracing.params);
    if (cJSON_IsObject(configItem)) {
        cJSON *v = cJSON_GetObjectItem((cJSON *)configItem, "enable_collision_avoidance"); if (cJSON_IsBool(v) || cJSON_IsNumber(v)) g_template_executeracing.params.enable_collision_avoidance = cJSON_IsTrue(v) ? 1 : (v ? (v->valueint ? 1 : 0) : g_template_executeracing.params.enable_collision_avoidance);
        v = cJSON_GetObjectItem((cJSON *)configItem, "override_pitch_in"); if (cJSON_IsBool(v) || cJSON_IsNumber(v)) g_template_executeracing.params.override_pitch_in = cJSON_IsTrue(v) ? 1 : (v ? (v->valueint ? 1 : 0) : g_template_executeracing.params.override_pitch_in);
        v = cJSON_GetObjectItem((cJSON *)configItem, "fixed_cars"); if (cJSON_IsNumber(v)) g_template_executeracing.params.fixed_cars = v->valueint;
        v = cJSON_GetObjectItem((cJSON *)configItem, "max_cars_cap"); if (cJSON_IsNumber(v)) g_template_executeracing.params.max_cars_cap = v->valueint;
        v = cJSON_GetObjectItem((cJSON *)configItem, "min_len_in"); if (cJSON_IsNumber(v)) g_template_executeracing.params.min_len_in = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "max_len_in"); if (cJSON_IsNumber(v)) g_template_executeracing.params.max_len_in = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "min_start_spacing_in"); if (cJSON_IsNumber(v)) g_template_executeracing.params.min_start_spacing_in = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "reentry_gap_in"); if (cJSON_IsNumber(v)) g_template_executeracing.params.reentry_gap_in = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "min_speed_in_s"); if (cJSON_IsNumber(v)) g_template_executeracing.params.min_speed_in_s = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "max_speed_in_s"); if (cJSON_IsNumber(v)) g_template_executeracing.params.max_speed_in_s = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "retarget_min_ms"); if (cJSON_IsNumber(v)) g_template_executeracing.params.retarget_min_ms = (uint32_t)v->valueint;
        v = cJSON_GetObjectItem((cJSON *)configItem, "retarget_jitter_ms"); if (cJSON_IsNumber(v)) g_template_executeracing.params.retarget_jitter_ms = (uint32_t)v->valueint;
        v = cJSON_GetObjectItem((cJSON *)configItem, "max_accel_in_s2"); if (cJSON_IsNumber(v)) g_template_executeracing.params.max_accel_in_s2 = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "min_collision_gap_in"); if (cJSON_IsNumber(v)) g_template_executeracing.params.min_collision_gap_in = (float)v->valuedouble;
        v = cJSON_GetObjectItem((cJSON *)configItem, "spawn_mode"); if (cJSON_IsNumber(v)) g_template_executeracing.params.spawn_mode = (uint8_t)v->valueint;
        v = cJSON_GetObjectItem((cJSON *)configItem, "max_dt_s"); if (cJSON_IsNumber(v)) g_template_executeracing.params.max_dt_s = (float)v->valuedouble;
        cJSON *colors = cJSON_GetObjectItem((cJSON *)configItem, "COLORS");
        if (cJSON_IsArray(colors)) {
            int n = cJSON_GetArraySize(colors); if (n > DEF_MAX_CARS_CAP) n = DEF_MAX_CARS_CAP;
            g_template_executeracing.params.num_colors = n;
            for (int i = 0; i < n; i++) {
                cJSON *rgb = cJSON_GetArrayItem(colors, i);
                if (cJSON_IsArray(rgb) && cJSON_GetArraySize(rgb) == 3) {
                    g_template_executeracing.params.colors[i][0] = (uint16_t)cJSON_GetArrayItem(rgb,0)->valuedouble;
                    g_template_executeracing.params.colors[i][1] = (uint16_t)cJSON_GetArrayItem(rgb,1)->valuedouble;
                    g_template_executeracing.params.colors[i][2] = (uint16_t)cJSON_GetArrayItem(rgb,2)->valuedouble;
                }
            }
        } else {
            g_template_executeracing.params.num_colors = 0;
        }
    }
    g_template_executeracing.brightness = brightness;
    g_template_executeracing.valid = true;
    return 1;
}

static int execute_executeracing_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_executeracing.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    g_racing_state[channel-1].params = g_template_executeracing.params;
    (void)apply_runtime_brightness(channel, g_template_executeracing.brightness);
    Racing_InitChannel(channel-1);
    RacingStartFlag[channel-1] = 1;
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

static int parse_executemarquee_template(AMessage_st* s_Message_Rx, int optional_channel, float brightness, const cJSON *configItem)
{
    (void)s_Message_Rx;
    (void)optional_channel;
    if (!cJSON_IsObject(configItem)) return -1;
    static const char *const colors_keys[] = {"Colors", "COLORS"};
    cJSON *colors = cfg_find(configItem, colors_keys, sizeof(colors_keys) / sizeof(colors_keys[0]));
    if (!cJSON_IsArray(colors)) return -1;
    int n = cJSON_GetArraySize(colors);
    if (n <= 0) return -1;
    if (n > MAX_MARQUEE_COLORS) n = MAX_MARQUEE_COLORS;
    g_template_executemarquee.numColors = n;
    g_template_executemarquee.brightness = brightness;
    for (int i = 0; i < n; i++) {
        cJSON *obj = cJSON_GetArrayItem(colors, i);
        static const char *const color_keys[] = {"color", "Color"};
        static const char *const len_keys[] = {"length", "Length"};
        cJSON *carr = cfg_find(obj, color_keys, sizeof(color_keys) / sizeof(color_keys[0]));
        cJSON *len = cfg_find(obj, len_keys, sizeof(len_keys) / sizeof(len_keys[0]));
        g_template_executemarquee.colors[i].hue = (cJSON_IsArray(carr) && cJSON_GetArraySize(carr) == 3) ? (float)cJSON_GetArrayItem(carr,0)->valuedouble : 0.0f;
        g_template_executemarquee.colors[i].saturation = (cJSON_IsArray(carr) && cJSON_GetArraySize(carr) == 3) ? (float)(cJSON_GetArrayItem(carr,1)->valuedouble * 100.0) : 0.0f;
        g_template_executemarquee.colors[i].brightness = (cJSON_IsArray(carr) && cJSON_GetArraySize(carr) == 3) ? (float)(cJSON_GetArrayItem(carr,2)->valuedouble * 100.0) : 0.0f;
        g_template_executemarquee.colors[i].lengthInches = cJSON_IsNumber(len) ? (float)len->valuedouble : 0.0f;
    }
    static const char *const spacing_keys[] = {"spacingOverride", "SpacingOverride"};
    static const char *const trans_keys[] = {"transitionType", "TransitionType"};
    static const char *const mirror_en_keys[] = {"mirrorEnable", "MirrorEnable"};
    static const char *const mirror_pos_keys[] = {"mirrorPosition", "MirrorPosition"};
    static const char *const osc_amp_keys[] = {"oscAmp", "OscAmp"};
    static const char *const osc_period_keys[] = {"oscPeriod", "OscPeriod"};
    static const char *const moving_speed_keys[] = {"movingSpeed"};
    static const char *const speed_keys[] = {"Speed", "speed"};
    static const char *const bw_keys[] = {"brightnessWavelength", "BrightnessWavelength"};
    static const char *const ba_keys[] = {"brightnessAmplitude", "BrightnessAmplitude"};
    static const char *const bs_keys[] = {"brightnessSpeed", "BrightnessSpeed"};
    cJSON *v = cfg_find(configItem, spacing_keys, sizeof(spacing_keys) / sizeof(spacing_keys[0])); g_template_executemarquee.spacingOverride = cJSON_IsNumber(v) ? (uint8_t)v->valueint : 0;
    cJSON *tt = cfg_find(configItem, trans_keys, sizeof(trans_keys) / sizeof(trans_keys[0])); g_template_executemarquee.transitionType = (cJSON_IsString(tt) && strcmp(tt->valuestring, "None")) ? 1 : 0;
    v = cfg_find(configItem, mirror_en_keys, sizeof(mirror_en_keys) / sizeof(mirror_en_keys[0])); g_template_executemarquee.enableMirror = (cJSON_IsBool(v) ? (cJSON_IsTrue(v) ? 1 : 0) : (cJSON_IsNumber(v) ? v->valueint : 0));
    v = cfg_find(configItem, mirror_pos_keys, sizeof(mirror_pos_keys) / sizeof(mirror_pos_keys[0])); g_template_executemarquee.mirrorPosition = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, osc_amp_keys, sizeof(osc_amp_keys) / sizeof(osc_amp_keys[0])); g_template_executemarquee.oscAmp = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, osc_period_keys, sizeof(osc_period_keys) / sizeof(osc_period_keys[0])); g_template_executemarquee.oscPeriod = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, moving_speed_keys, sizeof(moving_speed_keys) / sizeof(moving_speed_keys[0]));
    if (!cJSON_IsNumber(v)) v = cfg_find(configItem, speed_keys, sizeof(speed_keys) / sizeof(speed_keys[0]));
    g_template_executemarquee.movingSpeed = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, bw_keys, sizeof(bw_keys) / sizeof(bw_keys[0])); g_template_executemarquee.brightnessWavelength = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, ba_keys, sizeof(ba_keys) / sizeof(ba_keys[0])); g_template_executemarquee.brightnessAmplitude = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    v = cfg_find(configItem, bs_keys, sizeof(bs_keys) / sizeof(bs_keys[0])); g_template_executemarquee.brightnessSpeed = cJSON_IsNumber(v) ? (float)v->valuedouble : 0.0f;
    g_template_executemarquee.valid = true;
    return 1;
}

static int execute_executemarquee_template(AMessage_st* s_Message_Rx, int channel)
{
    (void)s_Message_Rx;
    if (!g_template_executemarquee.valid || channel < 1 || channel > NUMBER_OF_CHANNELS) return -1;
    TurnFlagsOff(channel);
    int ch = channel - 1;
    int ramp = ((rampData[ch].DwellTimeSceneVal != 0) || (rampData[ch].RampTimeSceneVal != 0));
    marqueeImage_t *m = ramp ? &marqueeImage_end[ch] : &marqueeImage_start[ch];
    memset(m, 0, sizeof(*m));
    m->numColors = g_template_executemarquee.numColors;
    m->spacingOverride = g_template_executemarquee.spacingOverride;
    m->transitionType = g_template_executemarquee.transitionType;
    m->enableMirror = g_template_executemarquee.enableMirror;
    m->mirrorPosition = g_template_executemarquee.mirrorPosition;
    m->oscAmp = g_template_executemarquee.oscAmp;
    m->oscPeriod = g_template_executemarquee.oscPeriod;
    m->movingSpeed = -g_template_executemarquee.movingSpeed;
    m->brightnessWavelength = g_template_executemarquee.brightnessWavelength;
    m->brightnessAmplitude = g_template_executemarquee.brightnessAmplitude;
    m->brightnessSpeed = g_template_executemarquee.brightnessSpeed;
    for (int i = 0; i < m->numColors; i++) {
        m->colors[i] = g_template_executemarquee.colors[i];
        if (m->spacingOverride == 1) m->colors[i].lengthInches *= (ChannelParamObject[ch].LEDspacingCh_float / 25.4f);
        if (m->colors[i].lengthInches <= 0.0f) m->colors[i].lengthInches = (ChannelParamObject[ch].LEDspacingCh_float / 25.4f);
        m->totalLengthInches += m->colors[i].lengthInches;
    }
    if (m->spacingOverride == 1) m->movingSpeed *= (ChannelParamObject[ch].LEDspacingCh_float / 25.4f);
    {
        float moving_speed = m->movingSpeed;
        float moving_speed_abs = 0.0f;
        float temp_cal = 0.0f;

        MarqueeCustomStartFlag_offset[ch] = 0;
        MarqueeCustomStartFlag_offset1[ch] = 0;
        One_LED_time_back[ch] = 0;

        if (fabsf(moving_speed) < EPS1) {
            MarqueeCustomStartFlag_offset1[ch] = 1;
            One_LED_time[ch] = 0;
        } else if (moving_speed < 0.0f) {
            moving_speed_abs = -moving_speed;
            ChannelParamObject[ch].speedrevDirCh_u8 = 1;
            temp_cal = 1000.0f / moving_speed_abs;
            if (temp_cal < 1.0f) temp_cal = 1.0f;
            One_LED_time[ch] = (int)temp_cal;
        } else {
            moving_speed_abs = moving_speed;
            ChannelParamObject[ch].speedrevDirCh_u8 = 0;
            temp_cal = 1000.0f / moving_speed_abs;
            if (temp_cal < 1.0f) temp_cal = 1.0f;
            One_LED_time[ch] = (int)temp_cal;
        }

        if ((m->oscAmp != 0.0f) && (m->oscPeriod != 0.0f)) {
            oscP_Flag[ch] = 1;
            oscStart_time[ch] = get_current_time_ms();
        } else {
            oscP_Flag[ch] = 0;
        }

        enableMirror_uint8[ch] = m->enableMirror;
        if (ChannelParamObject[ch].LEDspacingCh_float > 0.0f) {
            temp_cal = (m->mirrorPosition * 25.4f) / ChannelParamObject[ch].LEDspacingCh_float;
            MirrorLedNum[ch] = (int)temp_cal;
        } else {
            MirrorLedNum[ch] = 0;
        }

        oscOffset[ch] = 0;
        oscOffset_forward[ch] = 0;
        oscOffset_back[ch] = 0;
    }
    if (m->totalLengthInches < (ChannelParamObject[ch].LEDspacingCh_float / 25.4f)) m->totalLengthInches = (ChannelParamObject[ch].LEDspacingCh_float / 25.4f);
    ImageSize_forMode[ch] = m->totalLengthInches;
    if ((EXAMPLE_LED_NUMBERS * 3) < ImageSize_forMode[ch]) ImageSize_forMode[ch] = (EXAMPLE_LED_NUMBERS * 3);
    bytes_per_channel[ch] = ImageSize_forMode[ch];
    (void)apply_runtime_brightness(channel, g_template_executemarquee.brightness);
    if (!ramp) {
        uint64_t now = get_current_time_ms();
        ExecuteMarquee_PrepareDataWithModeSetting(0.0f, ch, 1, now, 0);
        MarqueeExecuteCustomStartFlag[ch] = 1;
    } else {
        rampData[ch].RampStartTime = get_current_time_ms();
        ExecuteSceneRampFlag[ch] = 1;
    }
    if (functionNullFlag == 1) functionNullFlag = 0;
    return 1;
}

//static int hueSat(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//    cJSON *json = cJSON_CreateObject();
//    if (json == NULL) {
//        Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//    if (channelArray == NULL) {
//        cJSON_Delete(json);
//        Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//    // Extract values from JSON
//    uint16_t red=0, green=0, blue=0;
//    float hue = 0, sat = 0, val = 0;
//    float brightness = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//	    cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//		if (cJSON_IsObject(configItem))
//		{
//			// Extract "HUE"
//			cJSON *hueItem = cJSON_GetObjectItem(configItem, "HUE");
//			hue = hueItem->valuedouble;
//
//			// Extract "SAT"
//			cJSON *satItem = cJSON_GetObjectItem(configItem, "SAT");
//			sat = satItem->valuedouble;
//
//			// Extract "VAL"
//			cJSON *valItem = cJSON_GetObjectItem(configItem, "VAL");
//			val = valItem->valuedouble;
//		}
//
//		int Number_of_LED_int =0;
//
//		int useAllPositions = 0;
//		int offset = 0;
//		if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//		{
//			bytes_per_channel[channel-1] = 200;
//
//			TurnFlagsOff(channel);
//			Number_of_LED_int = EXAMPLE_LED_NUMBERS;
//
//		    // Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//
//			brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//							   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//							   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//												light_para.chan4MaxB_float * 0.01;
//
//			if(functionNullFlag == 1)
//			{
//				brightness = brightness_RunTimeChan[channel-1];
//#ifdef ENABLE_PRINT_MSG
//				printf("brightness = %f \n", brightness);
//#endif
//			}
//
//			if(brightness != 0)
//			{
////				brightness_factor = (brightness_factor*brightness)/100;
//				brightness_factor = (brightness_factor*brightness) * 0.01;
//			}
//#ifdef ENABLE_PRINT_MSG
//			printf("brightness_factor = %f \n", brightness_factor);
//#endif
//
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				for (int i = 0; i < Number_of_LED_int; i++)
//				{
//					rampData[channel-1].hue_end[i] = hue;
//					rampData[channel-1].sat_end[i] = sat;
//					rampData[channel-1].val_end[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//					printf("hue e = %f, sat e = %f, val e = %f \n", rampData[channel-1].hue_end[i], rampData[channel-1].sat_end[i], rampData[channel-1].val_end[i] );
//#endif
//				}
//				rampData[channel-1].RampStartTime = get_current_time_ms();
//				ExecuteSceneRampFlag[channel-1] = 1;
//
//				goto EndLoop;
//			}
//			else
//			{
//				for (int i = 0; i < Number_of_LED_int; i++)
//				{
//					rampData[channel-1].hue_start[i] = hue;
//					rampData[channel-1].sat_start[i] = sat;
//					rampData[channel-1].val_start[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//					printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_start[i], rampData[channel-1].sat_start[i], rampData[channel-1].val_start[i] );
//#endif
//				}
//			}
//			hsv_to_rgb_16bit(hue, sat, 100, &red, &green, &blue);
//#ifdef ENABLE_PRINT_MSG
//			printf("Test Case : Before Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
//#endif
//			// Call the restrict and scale function
//			restrict_and_scale_RGB(&red, &green, &blue, (val*brightness_factor));
//#ifdef ENABLE_PRINT_MSG
//			printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
//#endif
//
//			// Iterate through positions
//			for (int i = 0; i < Number_of_LED_int; i++)
//			{
//				int pos = (useAllPositions + i + 1);
//
//				if (pos >= 1 && pos <= Number_of_LED_int)
//				{
//					// Set LED color for each combination of channel and position
//					set_led_color((uint8_t )channel,(uint16_t ) pos, (uint16_t )red,(uint16_t ) green,(uint16_t ) blue);
//				}
//				else
//				{
//					// Handle invalid position (out of range)
//					Add_Response_msg("Error: Invalid position (out of range).", s_Message_Rx, payLoadData);
//					// You may print an error message or take appropriate action.
//					cJSON_Delete(json);
//					return -1;
//				}
//			}
//			offset = 0;
//
//			PrepareDataWithModeSetting(offset, channel-1, 1);
//			HueSatStartFlag[channel-1] = 1;
//
//EndLoop:
//		}
//		else
//		{
//					// Handle invalid channel (out of range)
//					Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
//					// You may print an error message or take appropriate action.
//					cJSON_Delete(json);
//					return -1;
//		}
//	}
//	else
//	{
//		Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//		cJSON_Delete(json);
//		return -1;
//	}
//
//	if(functionNullFlag == 1)
//	{
//		functionNullFlag = 0;
//	}
//    cJSON_Delete(json);
//    return 1;
//}

//static int setColor(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//    cJSON *json = cJSON_CreateObject();
//    if (json == NULL) {
//        Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//    if (channelArray == NULL) {
//        cJSON_Delete(json);
//        Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//    // Extract values from JSON
//    uint16_t red=0, green=0, blue=0;
//    float red1 = 0, green1 = 0, blue1 = 0;
//    float hue = 0, sat = 0, val = 0;
//
//    // Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//
//		float brightness = 0;
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		// Get RGB values
//		cJSON *redItem = cJSON_GetObjectItem(configItem, "RED");
//		cJSON *greenItem = cJSON_GetObjectItem(configItem, "GREEN");
//		cJSON *blueItem = cJSON_GetObjectItem(configItem, "BLUE");
//
//		if (redItem == NULL || greenItem == NULL || blueItem == NULL)
//		{
//			Add_Response_msg("Error: Missing RGB values in JSON.", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		if((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1)
//		{
//			red1 = (redItem->valuedouble * 65535) * 0.01;;
//			green1 = (greenItem->valuedouble * 65535) * 0.01;;
//			blue1 = (blueItem->valuedouble * 65535) * 0.01;;
//		}
//		else
//		{
//			red1 = (redItem->valuedouble * 255) * 0.01;;
//			green1 = (greenItem->valuedouble * 255) * 0.01;;
//			blue1 = (blueItem->valuedouble * 255) * 0.01;;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("red1 = %f \n", red1);
//		printf("green1 = %f \n", green1);
//		printf("blue1 = %f \n", blue1);
//#endif
//
//		int Number_of_LED_int =0;
//		//Number_of_LED_int = light_para.SetLEDstripalCh1_u16;
//
//		int useAllPositions = 0;
//		int offset = 0;
//
//		TurnFlagsOff(channel);
//
//	    // Calculate brightness factor
//		float brightness_factor = light_para.contrMaxB_float * 0.01;
//
//		if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//		{
//			bytes_per_channel[channel-1] = 200;
//
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//
//			if(functionNullFlag == 1)
//			{
//				brightness = brightness_RunTimeChan[channel-1];
//#ifdef ENABLE_PRINT_MSG
////				printf("brightness = %f \n", brightness);
//#endif
//			}
//
//			if(brightness != 0)
//			{
//				brightness_factor = (brightness_factor*brightness) * 0.01;;
//			}
//
//			red = red1*brightness_factor;
//			green = green1*brightness_factor;
//			blue = blue1*brightness_factor;
//#ifdef ENABLE_PRINT_MSG
//			printf("red = %d, green = %d, blue = %d \n", red, green, blue);
//#endif
//
////			Number_of_LED_int = ChannelParamObject[channel-1].SetLEDstripal_u16;
//			Number_of_LED_int = EXAMPLE_LED_NUMBERS;
//
//			rgb16_to_hsv(red, green, blue, &hue, &sat, &val);
//
//#ifdef ENABLE_PRINT_MSG
//			printf("hue = %f, sat = %f, val = %f \n", hue, sat, val);
//#endif
//
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				for (int i = 0; i < Number_of_LED_int; i++)
//				{
//					rampData[channel-1].hue_end[i] = hue;
//					rampData[channel-1].sat_end[i] = sat;
//					rampData[channel-1].val_end[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//					printf("hue e = %f, sat e = %f, val e = %f \n", rampData[channel-1].hue_end[i], rampData[channel-1].sat_end[i], rampData[channel-1].val_end[i] );
//#endif
//				}
//				rampData[channel-1].RampStartTime = get_current_time_ms();
//				ExecuteSceneRampFlag[channel-1] = 1;
//
//				goto EndLoop1;
//			}
//			else
//			{
//				for (int i = 0; i < Number_of_LED_int; i++)
//				{
//					rampData[channel-1].hue_start[i] = hue;
//					rampData[channel-1].sat_start[i] = sat;
//					rampData[channel-1].val_start[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//					printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_start[i], rampData[channel-1].sat_start[i], rampData[channel-1].val_start[i] );
//#endif
//				}
//			}
//
//			// Call the restrict and scale function
//			restrict_and_scale_RGB(&red, &green, &blue, 100);
//#ifdef ENABLE_PRINT_MSG
////			        printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
//#endif
//
//			// Iterate through positions
//			//for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++) {
//			for (int i = 0; i < Number_of_LED_int; i++)
//			{
//				int pos = (useAllPositions + i + 1);
//
//				if (pos >= 1 && pos <= Number_of_LED_int)
//				{
//					// Set LED color for each combination of channel and position
//					set_led_color((uint8_t )channel,(uint16_t ) pos, (uint16_t )red,(uint16_t ) green,(uint16_t ) blue);
//				}
//				else
//				{
//					// Handle invalid position (out of range)
//					Add_Response_msg("Error: Invalid position (out of range).", s_Message_Rx, payLoadData);
//					// You may print an error message or take appropriate action.
//					cJSON_Delete(json);
//					return -1;
//				}
//			}
//
//			offset = 0;
//			PrepareDataWithModeSetting(offset, channel-1, 1);
//			setColorStartFlag[channel-1] = 1;
//
//EndLoop1:
//		}
//		else
//		{
//			// Handle invalid channel (out of range)
//			Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
//			// You may print an error message or take appropriate action.
//			cJSON_Delete(json);
//			return -1;
//		}
//	}
//	else
//	{
//		// Handle invalid JSON structure
//		Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//		cJSON_Delete(json);
//		return -1;
//	}
//
//    cJSON_Delete(json);
//
//	return 1;
//}

//static int colorIndex(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//
//	cJSON *json = cJSON_CreateObject();
//	char str[100]={0};
//
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	// Extract values from JSON
//	int color_index_val = 0;
//    uint16_t red, green, blue;
//    float hue = 0, sat = 0, val = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel.", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		float brightness = 0;
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		if (cJSON_IsObject(configItem))
//		{
//
//			// Get RGB values
//			cJSON *indexItem = cJSON_GetObjectItem(configItem, "Index");
//
//			if (indexItem == NULL)
//			{
//				// Handle missing RGB values in JSON
//
//				Add_Response_msg("Error: Missing indexItem values in JSON.", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			int Number_of_LED_int =0;
//			int useAllPositions = 0;
//			int offset = 0;
//
//			int found = 0;
//
//		    // Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//			{
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//
//				if(functionNullFlag == 1)
//				{
//					brightness = brightness_RunTimeChan[channel-1];
//				}
//				else
//				{
//					brightness_RunTimeChan[channel-1] = brightness;
//				}
//
//				if(brightness != 0)
//				{
//					brightness_factor = (brightness_factor*brightness) * 0.01;;
//				}
//
//				color_index_val = indexItem->valueint;
//				found = 0;
//
//				//for(color_index = 0; color_index < End_Record; color_index++)
//				for(int color_index = 0; color_index < 60; color_index++)
//				{
//#ifdef ENABLE_PRINT_MSG
//					printf("Color_table[color_index] = %d \n", Color_table[color_index].ColorIndex);
//#endif
//					if(Color_table[color_index].ColorIndex == color_index_val)
//					{
//#ifdef ENABLE_PRINT_MSG
////						printf("color_index yes = %d \n", color_index_val);
//#endif
//						hue = Color_table[color_index].Hue;
//						sat = Color_table[color_index].Saturation;
//						val = Color_table[color_index].Value;
//						found = 1;
//					}
//				}
//
//				if(found == 0)
//				{
//					Add_Response_msg("Error: color_index Not found.", s_Message_Rx, payLoadData);
//					cJSON_Delete(json);
//					return -1;
//				}
//
//				TurnFlagsOff(channel);
//				bytes_per_channel[channel-1] = 200;
//
//				Number_of_LED_int = EXAMPLE_LED_NUMBERS;
//
//				if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//				{
//					for (int i = 0; i < Number_of_LED_int; i++)
//					{
//						rampData[channel-1].hue_end[i] = hue;
//						rampData[channel-1].sat_end[i] = sat;
//						rampData[channel-1].val_end[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//						printf("hue e = %f, sat e = %f, val e = %f \n", rampData[channel-1].hue_end[i], rampData[channel-1].sat_end[i], rampData[channel-1].val_end[i] );
//#endif
//					}
//					rampData[channel-1].RampStartTime = get_current_time_ms();
//					ExecuteSceneRampFlag[channel-1] = 1;
//
//					goto EndLoop2;
//				}
//				else
//				{
//					for (int i = 0; i < Number_of_LED_int; i++)
//					{
//						rampData[channel-1].hue_start[i] = hue;
//						rampData[channel-1].sat_start[i] = sat;
//						rampData[channel-1].val_start[i] = val;
//
//#ifdef ENABLE_PRINT_MSG
//						printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_start[i], rampData[channel-1].sat_start[i], rampData[channel-1].val_start[i] );
//#endif
//					}
//				}
//
//				hsv_to_rgb_16bit(hue, sat, 100, &red, &green, &blue);
//
//				// Call the restrict and scale function
//				restrict_and_scale_RGB(&red, &green, &blue, (val*brightness_factor));
//#ifdef ENABLE_PRINT_MSG
////			        printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
//#endif
//
//				// Iterate through positions
//				for (int i = 0; i < Number_of_LED_int; i++)
//				{
//					int pos = (useAllPositions + i + 1);
//
//					if (pos >= 1 && pos <= Number_of_LED_int)
//					{
//						// Set LED color for each combination of channel and position
//						set_led_color((uint8_t )channel,(uint16_t ) pos, (uint16_t )red,(uint16_t ) green,(uint16_t ) blue);
//					}
//					else
//					{
//						// Handle invalid position (out of range)
//						Add_Response_msg("Error: Invalid position (out of range).", s_Message_Rx, payLoadData);
//						// You may print an error message or take appropriate action.
//						cJSON_Delete(json);
//						return -1;
//					}
//				}
//
//				offset = 0;
//				PrepareDataWithModeSetting(offset, channel-1, 1);
//				colorIndexStartFlag[channel-1] = 1;
//EndLoop2:
//			}
//			else
//			{
//				// Handle invalid channel (out of range)
//				Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
//				// You may print an error message or take appropriate action.
//				cJSON_Delete(json);
//				return -1;
//			}
//		}
//	}
//	else
//	{
//		// Handle invalid JSON structure
//		Add_Response_msg(str,s_Message_Rx, payLoadData);
//		//Add_Response_msg("Invalid channel ", s_Message_Rx);
//	}
//	cJSON_Delete(json);
//
//	return 1;
//}

//static int tapeMeasure(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//	cJSON *json = cJSON_CreateObject();
//
//	char str[100]={0};
//
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	// Extract values from JSON
//	float NumLEDInOneInch = 0;
//
//	TurnFlagsOff(channel);
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel.", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		float brightness = 0;
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		}
//
//		if (cJSON_IsObject(configItem))
//		{
//			int temp_ramp = 0;
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				temp_ramp = 1;
//			}
//			else
//			{
//			}
//
//			customImage *imgConfig = (temp_ramp) ? &ImageConfig_end[channel-1] : &ImageConfig_start[channel-1];
//
//			uint8_t spacingOverrideV = 0;
//			imgConfig->spacingOverride = spacingOverrideV;
//
//			cJSON *inchBetweenMarkersItem = cJSON_GetObjectItem(configItem, "Spacing");
//
//			if (inchBetweenMarkersItem == NULL)
//			{
//				// Handle missing RGB values in JSON
//
//				Add_Response_msg("Error: Missing Spacing values in JSON.", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			float check_LED_Size_value = 0;
//			float inchBetweenMarkersItem_local = 0;
//
//			inchBetweenMarkersItem_local = inchBetweenMarkersItem->valuedouble;
//
//			check_LED_Size_value = floor(ChannelParamObject[channel-1].LEDspacingCh_float)/25.4;
//
//			if(inchBetweenMarkersItem_local< check_LED_Size_value)
//			{
//				inchBetweenMarkersItem_local = check_LED_Size_value;
//			}
//
//			float padding_l = 0;
//
//			padding_l = inchBetweenMarkersItem_local - check_LED_Size_value;
//
//			if(padding_l < 0)
//			{
//				padding_l = 0;
//			}
//
//			imgConfig->numColors = 4;
//			imgConfig->colorLength = check_LED_Size_value;
//			imgConfig->paddingLength = padding_l;
//			imgConfig->transitionType = 0;
//			imgConfig->mirror = 0;
//			imgConfig->mirrorPosition = 0;
//			imgConfig->oscAmplitude = 0;
//			imgConfig->oscPeriod = 0;
//			imgConfig->movingSpeed = cJSON_GetObjectItem(configItem, "Speed")->valuedouble;
//			imgConfig->movingSpeed = -1*imgConfig->movingSpeed;
//
//			ImageSize_forMode[channel-1]= (imgConfig->numColors)*(imgConfig->colorLength+imgConfig->paddingLength);
//
//			if((EXAMPLE_LED_NUMBERS*3) < ImageSize_forMode[channel-1])
//			{
//				ImageSize_forMode[channel-1] = (EXAMPLE_LED_NUMBERS*3);
//			}
//
//			bytes_per_channel[channel-1] = ImageSize_forMode[channel-1];
//
//			imgConfig->colorSelections[0].hue = 0;
//			imgConfig->colorSelections[0].saturation = 100;
//			imgConfig->colorSelections[0].brightness = 100;
//
//			imgConfig->colorSelections[1].hue = 120;
//			imgConfig->colorSelections[1].saturation = 100;
//			imgConfig->colorSelections[1].brightness = 100;
//
//			imgConfig->colorSelections[2].hue = 240;
//			imgConfig->colorSelections[2].saturation = 100;
//			imgConfig->colorSelections[2].brightness = 100;
//
//			imgConfig->colorSelections[3].hue = 0;
//			imgConfig->colorSelections[3].saturation = 0;
//			imgConfig->colorSelections[3].brightness = 100;
//
//			// Extract background color
//			imgConfig->paddingColor.hue = 0;
//			imgConfig->paddingColor.saturation = 0;
//			imgConfig->paddingColor.brightness = 0;
//
//			cJSON *inchPerSecItem = cJSON_GetObjectItem(configItem, "Speed");
//
//			float inchPerSecItem_local = 0;
//
//		    // Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//			{
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//				if(functionNullFlag == 1)
//				{
//					brightness = brightness_RunTimeChan[channel-1];
//				}
//				else
//				{
//					brightness_RunTimeChan[channel-1] = brightness;
//				}
//
//				if(brightness != 0)
//				{
//					brightness_factor = (brightness_factor*brightness) * 0.01;;
//				}
//
//				 inchPerSecItem_local = inchPerSecItem->valuedouble;
//
//				 if(inchPerSecItem_local == 0)
//				 {
//					 One_LED_time[channel-1] = 0;
//					 TapeMeasureStartFlag_offset[channel-1]=1;
//				 }
//				 else
//				 {
//					One_LED_time[channel-1] = 1000 / ( NumLEDInOneInch * inchPerSecItem_local );
//				 }
//
//				if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//				{
//					rampData[channel-1].RampStartTime = get_current_time_ms();
//					ExecuteSceneRampFlag[channel-1] = 1;
//				}
//				else
//				{
//					Execute_PrepareDataWithModeSetting(0, channel-1, 1, 0);
//					TapeMeasureStartFlag[channel-1]=1;
//				}
//			}
//			else
//			{
//				// Handle invalid channel (out of range)
//				Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
//				// You may print an error message or take appropriate action.
//				cJSON_Delete(json);
//				return -1;
//			}
//		}
//	}
//	else
//	{
//		// Handle invalid JSON structure
//		Add_Response_msg(str,s_Message_Rx, payLoadData);
//	}
//	cJSON_Delete(json);
//
//	return 1;
//}

static void StripChanOFF(uint8_t Chan)
{
	uint8_t channel =  Chan-1;

	memset(data_channels[channel],0,(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING));
	memset(data_channels_ping[channel],0,(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING));
	if(flag_direct_array_testing == 0)
	{
	if(channel == 0)
	{
		memset(data_channels1_1,0,sizeof(data_channels1_1));
	}
	else if(channel == 1)
	{
		memset(data_channels1_2,0,sizeof(data_channels1_2));
	}
	else if(channel == 2)
	{
		memset(data_channels1_3,0,sizeof(data_channels1_3));
	}
	else if(channel == 3)
	{
		memset(data_channels1_4,0,sizeof(data_channels1_4));
	}
}
}

static int trimLightOFF(AMessage_st* s_Message_Rx)
{
	// Parse JSON payload
	cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);

	uint16_t red = 0, green = 0, blue = 0;
	float hue = 0, sat = 0, val = 0;

	int eventIdV = 0;
	int DEFEReventIdV = 0;
	int optionalChangeID = 0;

    float brightness = 0;
    char *source = NULL;
    char *function = NULL;

	if (json == NULL)
	{
		// Handle JSON parsing error
		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
		return -1;
	}

	uint64_t timechange = 0;
	char *changeReason = NULL;

	cJSON *channelArray = cJSON_GetObjectItem(json, "CH");

    cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
    cJSON *sourceItem = cJSON_GetObjectItem(json, "SOURCE");
    cJSON *functionItem = cJSON_GetObjectItem(json, "FUNCTION");
    cJSON *configItem = cJSON_GetObjectItem(json, "CONFIG");

	cJSON *RampTimeSceneItem = cJSON_GetObjectItem(json, "RampTime");

	cJSON *DwellTimeSceneItem = cJSON_GetObjectItem(json, "Duration");

	if (channelArray == NULL || !cJSON_IsArray(channelArray))
    {
        Add_Response_msg("Error: Invalid or missing 'CH' array in JSON 5.", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

	cJSON *DEFEReventIdItem = cJSON_GetObjectItem(json, "DEFER_eventId");
	if (DEFEReventIdItem == NULL)
	{
		DEFEReventIdV = 0;
	}
	else
	{
		DEFEReventIdV = DEFEReventIdItem->valueint;
	}

	cJSON *eventIdItem = cJSON_GetObjectItem(json, "eventId");
	if (eventIdItem == NULL)
	{
		eventIdV = 0;
	}
	else
	{
		eventIdV = eventIdItem->valueint;
	}

	timechange = get_current_time_ms();

    if (brightnessItem != NULL && cJSON_IsNumber(brightnessItem)) {
        brightness = brightnessItem->valuedouble;
#ifdef ENABLE_PRINT_MSG
        printf("Brightness extracted: %f\n", brightness);
#endif
    } else {
        brightnessItem = NULL;
    }

    if (sourceItem != NULL && cJSON_IsString(sourceItem) && sourceItem->valuestring != NULL) {
    	source = sourceItem->valuestring;
#ifdef ENABLE_PRINT_MSG
        printf("Source extracted: %s\n", source);
#endif
    } else {
    	sourceItem = NULL;
    }

    if (functionItem != NULL && cJSON_IsString(functionItem) && functionItem->valuestring != NULL) {
        function = functionItem->valuestring;
#ifdef ENABLE_PRINT_MSG
        printf("Function extracted: %s\n", function);
#endif
    } else {
        functionItem = NULL;
    }

    if (configItem != NULL && !cJSON_IsObject(configItem)) {
#ifdef ENABLE_PRINT_MSG
        printf("Config item is not a valid JSON object.\n");
#endif
        configItem = NULL;
    }

	if(DEFEReventIdV != 0)
	{
		changeReason = "DEFER_eventId";
		optionalChangeID = DEFEReventIdV;
	}
	else if(eventIdV != 0)
	{
		changeReason = "eventId";
		optionalChangeID = eventIdV;
	}
	else
	{
		changeReason = (char*)s_Message_Rx->src_Actor_a8;
	}

	// Validate JSON structure
	if ((channelArray != NULL) &&
		(channelArray->type == cJSON_Array))
	{
		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
		if(check_ch_zero == 1)
		{
			Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
			cJSON_Delete(json);
			return -1;
		}

		int useAllChannels = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (-1);

		// Iterate through channels
		for (int j = 0; j < (useAllChannels ? NUMBER_OF_CHANNELS : cJSON_GetArraySize(channelArray)); j++)
		{
			int channel = useAllChannels ? j + 1 : cJSON_GetArrayItem(channelArray, j)->valueint;

			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
			{
				bytes_per_channel[channel-1] = 200;
				LastCommand_t *lastCommand = &light_LastCommandPara[channel - 1];

				if(brightness != 0)
				{
					lastCommand->brightness = brightness;
				}
				if(source != NULL)
				{
					lastCommand->source = source ? strdup(source) : NULL;
				}
				if(function != NULL)
				{
					lastCommand->function = function ? strdup(function) : NULL;
				}

				if(configItem != NULL)
				{
				    if (lastCommand->config) {
				        cJSON_Delete(lastCommand->config);  // Free previous memory
				    }
					lastCommand->config = (cJSON_Duplicate(configItem, 1));// ? cJSON_Duplicate(configItem, 1) : NULL;
				}

		        if (RampTimeSceneItem == NULL)
	        	{
	        		rampData[channel-1].RampTimeSceneVal = 0;
	        	}
	        	else
	        	{
	        		rampData[channel-1].RampTimeSceneVal = RampTimeSceneItem->valueint;					//In second
	        		rampData[channel-1].RampTimeSceneVal = (rampData[channel-1].RampTimeSceneVal)*1000; //In mili Second
	        	}

	        	if( (DwellTimeSceneItem == NULL) && (rampData[channel-1].RampTimeSceneVal == 0) )
	        	{
	        		rampData[channel-1].DwellTimeSceneVal = 0;
	        		strcpy(rampData[channel-1].function_start, "OFF");

	            	rampData[channel-1].RampStartTime = 0;
	            	ExecuteSceneRampFlag[channel-1] = 0;
	        	}
	        	else
	        	{
	        		if(DwellTimeSceneItem == NULL)
	        		{
	        			rampData[channel-1].DwellTimeSceneVal = 0;
	        		}
	        		else
	        		{
						rampData[channel-1].DwellTimeSceneVal = DwellTimeSceneItem->valueint; 					//In second
						rampData[channel-1].DwellTimeSceneVal = (rampData[channel-1].DwellTimeSceneVal)*1000; 	//In mili Second
	        		}
					strcpy(rampData[channel-1].function_end, "OFF");
	        	}

#ifdef ENABLE_PRINT_MSG
	        	printf("RampTimeSceneV = %ld, DwellTimeSceneV = %ld, function_start = %s, function_end = %s \n", rampData[channel-1].RampTimeSceneVal, rampData[channel-1].DwellTimeSceneVal, rampData[channel-1].function_start, rampData[channel-1].function_end );
#endif

				if(DEFEReventIdV != 0)
				{
					LastCommand_t *lastCommand = &light_LastCommandPara[channel - 1];
					strncpy(lastCommand->changeReason, (char*)changeReason, MAX_CMD_LEN);
					lastCommand->optionalChangeID = optionalChangeID;
				}
				else
				{
					TapeMeasureStartFlag[channel-1]=0;
					ExecuteCustomStartFlag[channel-1]=0;
					RacingStartFlag[channel-1]=0;
					MarqueeExecuteCustomStartFlag[channel-1]=0;
					RippleStartFlag[channel-1]=0;
					SparkleStartFlag[channel - 1]=0;
					MultiColorSparkleStartFlag[channel - 1]=0;
					One_LED_time[channel-1] = 0;
					One_LED_time_back[channel-1] = 0;
					SparkleParamObject_start[channel -1].Width = 0;
					SparkleParamObject_end[channel -1].Width = 0;
					MultiColorSparkleParamObject_start[channel -1].Width = 0;
					MultiColorSparkleParamObject_end[channel -1].Width = 0;
					enableMirror_uint8[channel-1] = 0;
					oscP_Flag[channel-1] = 0;
					oscStart_time[channel-1] = 0;
					PatternStartFlag_start[channel-1] = 0;
					ExecuteCustomStartFlag_offset1[channel-1] = 0;
					MarqueeCustomStartFlag_offset1[channel-1] = 0;
					HueSatStartFlag[channel-1] = 0;
					setColorStartFlag[channel-1] = 0;
					colorIndexStartFlag[channel-1] = 0;

					int pos =  0;

					for(pos =1; pos<=(EXAMPLE_LED_NUMBERS); pos++)
					{
						if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
						{
								rampData[channel-1].hue_end[pos-1] = hue;
								rampData[channel-1].sat_end[pos-1] = sat;
								rampData[channel-1].val_end[pos-1] = val;

			#ifdef ENABLE_PRINT_MSG
								printf("hue e = %f, sat e = %f, val e = %f \n", rampData[channel-1].hue_end[i], rampData[channel-1].sat_end[i], rampData[channel-1].val_end[i] );
			#endif

							if(pos == EXAMPLE_LED_NUMBERS)
							{
								rampData[channel-1].RampStartTime = get_current_time_ms();
								ExecuteSceneRampFlag[channel-1] = 1;
#ifdef ENABLE_PRINT_MSG
					printf("RampStartTime = %lld \n", rampData[channel-1].RampStartTime );
#endif
					flag_not_rmt = 0;
					delay_same_array = 1;
					goto EndLoop11;
							}
						}
						else
						{
								rampData[channel-1].hue_start[pos-1] = hue;
								rampData[channel-1].sat_start[pos-1] = sat;
								rampData[channel-1].val_start[pos-1] = val;

			#ifdef ENABLE_PRINT_MSG
								printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_start[i], rampData[channel-1].sat_start[i], rampData[channel-1].val_start[i] );
			#endif

							red = 0;
							green = 0;
							blue = 0;

							// Call the restrict and scale function
							restrict_and_scale_RGB(&red, &green, &blue, 100);
	#ifdef ENABLE_PRINT_MSG
					        printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
	#endif
							set_led_color((uint8_t )channel, (uint16_t ) pos, (uint16_t )red,(uint16_t ) green,(uint16_t ) blue);
						}

					}
#if defined(B527)
				if(channel == 1)
					gpio_set_level(LIGHT1, 0);  // turn LIGHT1 OFF
				else if(channel == 2)
					gpio_set_level(LIGHT2,0);  // turn LIGHT1 OFF
#endif
					memset(data_channels[channel-1],0,(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING));
					memset(data_channels_ping[channel-1],0,(EXAMPLE_LED_NUMBERS * 3 + MODE_SETTING));
					vTaskDelay(10 / portTICK_PERIOD_MS);
					flag_not_rmt = 0;
					delay_same_array = 1;
EndLoop11:
					//------------------------------------------------
					char state1 = 0;

					LastCommand_t *lastCommand = &light_LastCommandPara[channel - 1];
#ifdef ENABLE_PRINT_MSG
					printf(" Power_Cycle[channel-1] = %d \n",Power_Cycle[channel-1]);
#endif
					if(Power_Cycle[channel-1] == 1)
					{
						strncpy(lastCommand->changeReason, "power Cycle", MAX_CMD_LEN);
						lastCommand->timeChanged = get_current_time_ms();
						Power_Cycle[channel-1] = 0;
					}
					else
					{
						strncpy(lastCommand->changeReason, (char*)changeReason, MAX_CMD_LEN);

						if(eventIdV != 0)
						{
							lastCommand->optionalChangeID = optionalChangeID;
						}
						else
						{
							if((!strcmp(lastCommand->changeReason, "EVENT_ACTOR")))
							{
								strcpy(lastCommand->changeReason,"executeScene");
							}

							if((!strcmp(lastCommand->changeReason, "IHUB")))
							{
								strcpy(lastCommand->changeReason,"executeCommand");
							}
						}

						lastCommand->timeChanged = timechange;
					}

					if( (rampData[channel-1].DwellTimeSceneVal == 0) && (rampData[channel-1].RampTimeSceneVal == 0) )
					{
						lastCommand->state =  state1;

						Last_CommandFlag[channel - 1] = 2;

						{
							setLastCommand((channel-1), 1);
						}
					}
				}
//				//---------------------------------------------
			}
			else
			{
				// Handle invalid channel (out of range)
				Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
				// You may print an error message or take appropriate action.
				cJSON_Delete(json);
				return -1;
			}
		}

		if((Power_up_counter_d2c == 0) || (Power_up_counter_d2c >= 4))
		{
			Power_up_counter_d2c = 0;
			{
				Send_D2C();		//Send D2C command
			}
		}
	}
	else
	{
		// Handle invalid JSON structure
		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
		cJSON_Delete(json);
		return -1;
	}

	cJSON_Delete(json);

	return 1;
}

//static int ripple(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//	cJSON *json = cJSON_CreateObject();
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	TurnFlagsOff(channel);
//
//    float brightness = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//		else
//		{
//
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness = *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		if (cJSON_IsObject(configItem))
//		{
//			cJSON *startColorArray = cJSON_GetObjectItem(configItem, "STARTCOLOR");
//
//			if ((startColorArray != NULL) && (cJSON_IsArray(startColorArray)))
//			{
//				// Validate JSON structure
//				if ((startColorArray != NULL) &&
//					(startColorArray->type == cJSON_Array))
//				{
//					cJSON *peakColorArray = cJSON_GetObjectItem(configItem, "PEAKCOLOR");
//
//					if ((peakColorArray != NULL) && (cJSON_IsArray(peakColorArray)))
//					{
//						// Validate JSON structure
//						if ((peakColorArray != NULL) &&
//							(peakColorArray->type == cJSON_Array))
//						{
//							cJSON *valleyColorArray = cJSON_GetObjectItem(configItem, "VALLEYCOLOR");
//
//							if ((valleyColorArray != NULL) && (cJSON_IsArray(valleyColorArray)))
//							{
//								// Validate JSON structure
//								if ((valleyColorArray != NULL) &&
//									(valleyColorArray->type == cJSON_Array))
//								{
//									cJSON *amp1Item = cJSON_GetObjectItem(configItem, "AMP1");
//									cJSON *wave1Item = cJSON_GetObjectItem(configItem, "WAVE1");
//									cJSON *speed1Item = cJSON_GetObjectItem(configItem, "SPEED1");
//
//									cJSON *amp2Item = cJSON_GetObjectItem(configItem, "AMP2");
//									cJSON *wave2Item = cJSON_GetObjectItem(configItem, "WAVE2");
//									cJSON *speed2Item = cJSON_GetObjectItem(configItem, "SPEED2");
//
//									cJSON *amp3Item = cJSON_GetObjectItem(configItem, "AMP3");
//									cJSON *wave3Item = cJSON_GetObjectItem(configItem, "WAVE3");
//									cJSON *speed3Item = cJSON_GetObjectItem(configItem, "SPEED3");
//
//									if (amp1Item == NULL || wave1Item == NULL || speed1Item == NULL || amp2Item == NULL || wave2Item == NULL || speed2Item == NULL || amp3Item == NULL || wave3Item == NULL || speed3Item == NULL)
//									{
//										Add_Response_msg("Error: Missing ripple parameters in JSON.", s_Message_Rx, payLoadData);
//										cJSON_Delete(json);
//										return -1;
//									}
//
//									// Convert start and end colors from HSV to RGB
//									float startColorHSV[3], peakColorHSV[3], valleyColorHSV[3];
//									for (int i = 0; i < 3; i++)
//									{
//										startColorHSV[i] = cJSON_GetArrayItem(startColorArray, i)->valuedouble * (i == 0 ? 1.0 : 1.0);
//										peakColorHSV[i] = cJSON_GetArrayItem(peakColorArray, i)->valuedouble * (i == 0 ? 1.0 : 1.0);
//										valleyColorHSV[i] = cJSON_GetArrayItem(valleyColorArray, i)->valuedouble * (i == 0 ? 1.0 : 1.0);
//									}
//
//								    // Calculate brightness factor
//									float brightness_factor = light_para.contrMaxB_float * 0.01;
//									if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//									{
//										bytes_per_channel[channel-1] = 200;
//
//										brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//														   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//														   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//																			light_para.chan4MaxB_float * 0.01;
//
//										if(functionNullFlag == 1)
//										{
//											brightness = brightness_RunTimeChan[channel-1];
//										}
//										else
//										{
//											brightness_RunTimeChan[channel-1] = brightness;
//										}
//
//										if(brightness != 0)
//										{
//											brightness_factor = (brightness_factor*brightness) * 0.01;;
//										}
//
//										if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//										{
//											startColor_end[channel-1].hue = startColorHSV[0];
//											startColor_end[channel-1].saturation = startColorHSV[1] * 0.01;
//											startColor_end[channel-1].brightness = (startColorHSV[2]*brightness_factor) * 0.01;
//
//											peakColor_end[channel-1].hue = peakColorHSV[0];
//											peakColor_end[channel-1].saturation = peakColorHSV[1] * 0.01;
//											peakColor_end[channel-1].brightness = (peakColorHSV[2]*brightness_factor) * 0.01;
//
//											valleyColor_end[channel-1].hue = valleyColorHSV[0];
//											valleyColor_end[channel-1].saturation = valleyColorHSV[1] * 0.01;
//											valleyColor_end[channel-1].brightness = (valleyColorHSV[2]*brightness_factor) * 0.01;
//
//											// Retrieve and print the amp1, wave1, and speed1
//											waves_end[channel-1][0].amplitude = amp1Item->valuedouble;
//											waves_end[channel-1][0].wavelength = wave1Item->valuedouble;
//	#ifdef ENABLE_PRINT_MSG
//											//printf("waves_end[channel-1][0].wavelength = %f \n", waves_end[channel-1][0].wavelength);
//	#endif
//											if(waves_end[channel-1][0].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_end[channel-1][0].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//	#ifdef ENABLE_PRINT_MSG
//											//printf("waves_end[channel-1][0].wavelength after = %f \n", waves_end[channel-1][0].wavelength);
//	#endif
//											waves_end[channel-1][0].speed  = -1*speed1Item->valuedouble;
//
//											// Retrieve and print the amp2, wave2, and speed2
//											waves_end[channel-1][1].amplitude = amp2Item->valuedouble;
//											waves_end[channel-1][1].wavelength = wave2Item->valuedouble;
//
//											if(waves_end[channel-1][1].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_end[channel-1][1].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//
//											waves_end[channel-1][1].speed  = -1*speed2Item->valuedouble;
//
//											// Retrieve and print the amp3, wave3, and speed3
//											waves_end[channel-1][2].amplitude = amp3Item->valuedouble;
//											waves_end[channel-1][2].wavelength = wave3Item->valuedouble;
//
//											if(waves_end[channel-1][2].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_end[channel-1][2].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//
//											waves_end[channel-1][2].speed  = -1*speed3Item->valuedouble;
//
//											waves1_end[channel-1][0].wavelength = ((waves_end[channel-1][0].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//											waves1_end[channel-1][1].wavelength = ((waves_end[channel-1][1].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//											waves1_end[channel-1][2].wavelength = ((waves_end[channel-1][2].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//
//											waves1_end[channel-1][0].speed = waves_end[channel-1][0].speed;
//											waves1_end[channel-1][1].speed = waves_end[channel-1][1].speed;
//											waves1_end[channel-1][2].speed = waves_end[channel-1][2].speed;
//
//											waves1_end[channel-1][0].amplitude = waves_end[channel-1][0].amplitude;
//											waves1_end[channel-1][1].amplitude = waves_end[channel-1][1].amplitude;
//											waves1_end[channel-1][2].amplitude = waves_end[channel-1][2].amplitude;
//
//											rampData[channel-1].RampStartTime = get_current_time_ms();
//											ExecuteSceneRampFlag[channel-1] = 1;
//										}
//										else
//										{
//											startColor_start[channel-1].hue = startColorHSV[0];
//											startColor_start[channel-1].saturation = startColorHSV[1] * 0.01;
//											startColor_start[channel-1].brightness = (startColorHSV[2]*brightness_factor) * 0.01;
//
//											peakColor_start[channel-1].hue = peakColorHSV[0];
//											peakColor_start[channel-1].saturation = peakColorHSV[1] * 0.01;
//											peakColor_start[channel-1].brightness = (peakColorHSV[2]*brightness_factor) * 0.01;
//
//											valleyColor_start[channel-1].hue = valleyColorHSV[0];
//											valleyColor_start[channel-1].saturation = valleyColorHSV[1] * 0.01;
//											valleyColor_start[channel-1].brightness = (valleyColorHSV[2]*brightness_factor) * 0.01;
//
//											// Retrieve and print the amp1, wave1, and speed1
//											waves_start[channel-1][0].amplitude = amp1Item->valuedouble;
//											waves_start[channel-1][0].wavelength = wave1Item->valuedouble;
//	#ifdef ENABLE_PRINT_MSG
//											//printf("waves_start[channel-1][0].wavelength = %f \n", waves_start[channel-1][0].wavelength);
//	#endif
//											if(waves_start[channel-1][0].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_start[channel-1][0].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//	#ifdef ENABLE_PRINT_MSG
//											//printf("waves_start[channel-1][0].wavelength after = %f \n", waves_start[channel-1][0].wavelength);
//	#endif
//											waves_start[channel-1][0].speed  = -1*speed1Item->valuedouble;
//
//											// Retrieve and print the amp2, wave2, and speed2
//											waves_start[channel-1][1].amplitude = amp2Item->valuedouble;
//											waves_start[channel-1][1].wavelength = wave2Item->valuedouble;
//
//											if(waves_start[channel-1][1].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_start[channel-1][1].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//
//											waves_start[channel-1][1].speed  = -1*speed2Item->valuedouble;
//
//											// Retrieve and print the amp3, wave3, and speed3
//											waves_start[channel-1][2].amplitude = amp3Item->valuedouble;
//											waves_start[channel-1][2].wavelength = wave3Item->valuedouble;
//
//											if(waves_start[channel-1][2].wavelength < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//											{
//												waves_start[channel-1][2].wavelength = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4)+0.01;
//											}
//
//											waves_start[channel-1][2].speed  = -1*speed3Item->valuedouble;
//
//											waves1_start[channel-1][0].wavelength = ((waves_start[channel-1][0].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//											waves1_start[channel-1][1].wavelength = ((waves_start[channel-1][1].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//											waves1_start[channel-1][2].wavelength = ((waves_start[channel-1][2].wavelength)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;	//From feet to  mm
//
//											waves1_start[channel-1][0].speed = waves_start[channel-1][0].speed;
//											waves1_start[channel-1][1].speed = waves_start[channel-1][1].speed;
//											waves1_start[channel-1][2].speed = waves_start[channel-1][2].speed;
//
//											waves1_start[channel-1][0].amplitude = waves_start[channel-1][0].amplitude;
//											waves1_start[channel-1][1].amplitude = waves_start[channel-1][1].amplitude;
//											waves1_start[channel-1][2].amplitude = waves_start[channel-1][2].amplitude;
//
//											RippleStartFlag[channel-1]=1;
//										}
//									}
//									else
//									{
//										// Handle invalid channel (out of range)
//										Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
//										cJSON_Delete(json);
//										return -1;
//										// You may print an error message or take appropriate action.
//									}
//								}
//								else
//								{
//									// Handle invalid JSON structure
//									Add_Response_msg("Error: Invalid valleyColorArray array in JSON. ", s_Message_Rx, payLoadData);
//
//									cJSON_Delete(json);
//									return -1;
//								}
//							}
//							else
//							{
//								Add_Response_msg("Error: Invalid valleyColorArray. ", s_Message_Rx, payLoadData);
//								cJSON_Delete(json);
//								return -1;
//							}
//						}
//						else
//						{
//							// Handle invalid JSON structure
//							Add_Response_msg("Error: Invalid peakColorArray array in JSON. ", s_Message_Rx, payLoadData);
//
//							cJSON_Delete(json);
//							return -1;
//						}
//					}
//					else
//					{
//						Add_Response_msg("Error: Invalid peakColorArray. ", s_Message_Rx, payLoadData);
//
//						cJSON_Delete(json);
//						return -1;
//					}
//				}
//				else
//				{
//					// Handle invalid JSON structure
//					Add_Response_msg("Error: Invalid startColorArray array in JSON. ", s_Message_Rx, payLoadData);
//					cJSON_Delete(json);
//					return -1;
//				}
//			}
//			else
//			{
//				Add_Response_msg("Error: Invalid startColorArray ", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//		}
//	}
//	else
//	{
//		// Handle invalid JSON structure
//		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
//		cJSON_Delete(json);
//		return -1;
//	}
//
//	if(functionNullFlag == 1)
//	{
//		functionNullFlag = 0;
//	}
//	cJSON_Delete(json);
//
//	return 1;
//}

void RippleContinious(int Chan, uint64_t currentTimeMs, int fill_data) {
    uint16_t red = 0, green = 0, blue = 0;
    int channel = Chan + 1;

    // Convert the current time from milliseconds to seconds
    float currentTime = (float)(currentTimeMs % 100000000) * 0.001f;

    // Precompute phase factors and time adjustments
    float wavePhaseFactor[3];
    float timePhase[3];
    float AmplitudeAddition = 0.0f;

    float position_time = 0.0f;

    if( (fill_data == 0) || (fill_data == 1) )
	{
		// Precompute values outside the loop
		for (int j = 0; j < 3; j++) {
			float scale = fabsf(ChannelParamObject[Chan].scaleCh_float);
			float wavelength = waves1_start[Chan][j].wavelength * scale;
			wavePhaseFactor[j] = (wavelength != 0) ? (2.0f * PI / wavelength) : 0.0000001f;

			float speed_temp = waves1_start[Chan][j].speed;

			timePhase[j] = speed_temp * currentTime;
			AmplitudeAddition += waves1_start[Chan][j].amplitude;
	#ifdef ENABLE_PRINT_MSG
	//        if(Chan == 0)
	//        printf("wave = %f offset_temp = %f, wavelength = %f, wavePhaseFactor = %f, speed_temp = %f, timePhase = %f, AmplitudeAddition =%f \n",waves1_start[Chan][j].wavelength, offset_temp, wavelength, wavePhaseFactor[j], speed_temp, timePhase[j], AmplitudeAddition);
	#endif
		}

		float invAmplitudeAddition = (AmplitudeAddition != 0) ? (1.0f / AmplitudeAddition) : 1.0f;

		// Precompute scale factor once
		float scale_factor = fabsf(ChannelParamObject[Chan].scaleCh_float);
	#ifdef ENABLE_PRINT_MSG
	//    if(Chan == 0)
	//    printf("invAmplitudeAddition = %f, scale_factor = %f \n", invAmplitudeAddition, scale_factor);
	#endif
		// Loop through each LED position

		int Number_of_LED_int_R1 =0;

		Number_of_LED_int_R1 = EXAMPLE_LED_NUMBERS;

		for (int pos = 1; pos <= Number_of_LED_int_R1; pos++)
		{
			float combinedSine = 0.0f;

			// Calculate the combined sine wave value for the current LED position
			for (int j = 0; j < 3; j++) {
				if (waves1_start[Chan][j].amplitude != 0.0f) {
					int groupIndex = pos % (int)(waves1_start[Chan][j].wavelength * scale_factor);
					float positionPhase = groupIndex * wavePhaseFactor[j];
	#ifdef ENABLE_PRINT_MSG
	//				if(Chan == 0)
	//				{
	//					printf("positionPhase = %f, timePhase = %f \n", positionPhase, timePhase[j]);
	//					printf("positionPhase = %f, timePhase = %f \n", positionPhase, timePhase[j]);
	//				}
	#endif

					if (!isnan(positionPhase) && !isinf(positionPhase) &&
						!isnan(timePhase[j]) && !isinf(timePhase[j])) {
						position_time = positionPhase + timePhase[j];
						combinedSine += waves1_start[Chan][j].amplitude * fast_sinf(position_time);
					}

	#ifdef ENABLE_PRINT_MSG
	//                if(Chan == 0)
	//                printf("groupIndex = %d, positionPhase = %f, combinedSine = %f \n", groupIndex, positionPhase, combinedSine);
	#endif
				}
			}

			combinedSine *= invAmplitudeAddition;
			if ((!isnan(combinedSine)) && (!isinf(combinedSine)))
			{
				combinedSine = (combinedSine >  1.0f) ?  1.0f
				              : (combinedSine < -1.0f) ? -1.0f
				              : combinedSine;
			}

			Color currentColor1;
			if (combinedSine > 0.0f) {
				interpolate_color(&currentColor1, &startColor_start[Chan], &peakColor_start[Chan], combinedSine);
			} else {
				interpolate_color(&currentColor1, &startColor_start[Chan], &valleyColor_start[Chan], -combinedSine);
			}

			currentColor1.brightness *= 100.0f;
			currentColor1.saturation *= 100.0f;

			if(fill_data == 1)
			{
				rampData[channel-1].hue_start[pos-1] = currentColor1.hue;
				rampData[channel-1].sat_start[pos-1] = currentColor1.saturation;
				rampData[channel-1].val_start[pos-1] = currentColor1.brightness;
			}

			if(fill_data == 0)
			{
				hsv_to_rgb_16bit(currentColor1.hue, currentColor1.saturation, 100, &red, &green, &blue);

				// Call the restrict and scale function
				restrict_and_scale_RGB(&red, &green, &blue, currentColor1.brightness);
		#ifdef ENABLE_PRINT_MSG
		//        printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
		#endif

				const bool rev =
				    (Chan == 0 && light_para.revDirCh1_u8 == 1) ||
				    (Chan == 1 && light_para.revDirCh2_u8 == 1) ||
				    (Chan == 2 && light_para.revDirCh3_u8 == 1) ||
				    (Chan == 3 && light_para.revDirCh4_u8 == 1);

			    int led_index = pos;

			    if(rev)
			    {
			    	int num_led = 0;
			    	if(Chan == 0)
			    	{
			    		num_led = light_para.SetLEDstripalCh1_u16;
			    	}
			    	else if(Chan == 1)
			    	{
			    		num_led = light_para.SetLEDstripalCh2_u16;
			    	}
			    	else if(Chan == 2)
			    	{
			    		num_led = light_para.SetLEDstripalCh3_u16;
			    	}
			    	else if(Chan == 3)
			    	{
			    		num_led = light_para.SetLEDstripalCh4_u16;
			    	}

					if ((pos-1) < num_led) {
						led_index = (num_led) - (pos-1);
					} else {
						led_index = (EXAMPLE_LED_NUMBERS) - ((pos-1) - num_led);
					}

			    }

				set_led_color((uint8_t)channel, (uint16_t)(led_index), (uint16_t)red, (uint16_t)green, (uint16_t)blue);
			}
		}
	}

    if(fill_data == 2)
	{
		// Precompute values outside the loop
		for (int j = 0; j < 3; j++) {
			float scale = fabsf(ChannelParamObject[Chan].scaleCh_float);
			float wavelength = waves1_end[Chan][j].wavelength * scale;
			wavePhaseFactor[j] = (wavelength != 0) ? (2.0f * PI / wavelength) : 0.0000001f;

			float speed_temp = waves1_end[Chan][j].speed;

			if ((Chan == 0 && light_para.revDirCh1_u8 == 1) ||
				(Chan == 1 && light_para.revDirCh2_u8 == 1) ||
				(Chan == 2 && light_para.revDirCh3_u8 == 1) ||
				(Chan == 3 && light_para.revDirCh4_u8 == 1)) {
				speed_temp = -speed_temp;
			}

			timePhase[j] = speed_temp * currentTime;
			AmplitudeAddition += waves1_end[Chan][j].amplitude;
	#ifdef ENABLE_PRINT_MSG
	//        if(Chan == 0)
	//        printf("wave = %f offset_temp = %f, wavelength = %f, wavePhaseFactor = %f, speed_temp = %f, timePhase = %f, AmplitudeAddition =%f \n",waves1_end[Chan][j].wavelength, offset_temp, wavelength, wavePhaseFactor[j], speed_temp, timePhase[j], AmplitudeAddition);
	#endif
		}

		float invAmplitudeAddition = (AmplitudeAddition != 0) ? (1.0f / AmplitudeAddition) : 1.0f;

		// Precompute scale factor once
		float scale_factor = fabsf(ChannelParamObject[Chan].scaleCh_float);
	#ifdef ENABLE_PRINT_MSG
	//    if(Chan == 0)
	//    printf("invAmplitudeAddition = %f, scale_factor = %f \n", invAmplitudeAddition, scale_factor);
	#endif
		// Loop through each LED position

		int Number_of_LED_int_R1 =0;

		Number_of_LED_int_R1 = EXAMPLE_LED_NUMBERS;

		for (int pos = 1; pos <= Number_of_LED_int_R1; pos++)
		{
			float combinedSine = 0.0f;

			// Calculate the combined sine wave value for the current LED position
			for (int j = 0; j < 3; j++) {
				if (waves1_end[Chan][j].amplitude != 0.0f) {
					int groupIndex = pos % (int)(waves1_end[Chan][j].wavelength * scale_factor);
					float positionPhase = groupIndex * wavePhaseFactor[j];
	#ifdef ENABLE_PRINT_MSG
	//				if(Chan == 0)
	//				{
	//					printf("positionPhase = %f, timePhase = %f \n", positionPhase, timePhase[j]);
	//					printf("positionPhase = %f, timePhase = %f \n", positionPhase, timePhase[j]);
	//				}
	#endif

					if (!isnan(positionPhase) && !isinf(positionPhase) &&
						!isnan(timePhase[j]) && !isinf(timePhase[j])) {
						position_time = positionPhase + timePhase[j];
						combinedSine += waves1_end[Chan][j].amplitude * fast_sinf(position_time);
					}

	#ifdef ENABLE_PRINT_MSG
	//                if(Chan == 0)
	//                printf("groupIndex = %d, positionPhase = %f, combinedSine = %f \n", groupIndex, positionPhase, combinedSine);
	#endif
				}
			}

			combinedSine *= invAmplitudeAddition;
			if ((!isnan(combinedSine)) && (!isinf(combinedSine)))
			{
				combinedSine = (combinedSine >  1.0f) ?  1.0f
				              : (combinedSine < -1.0f) ? -1.0f
				              : combinedSine;
			}

			Color currentColor1;
			if (combinedSine > 0.0f) {
				interpolate_color(&currentColor1, &startColor_end[Chan], &peakColor_end[Chan], combinedSine);
			} else {
				interpolate_color(&currentColor1, &startColor_end[Chan], &valleyColor_end[Chan], -combinedSine);
			}

			currentColor1.brightness *= 100.0f;
			currentColor1.saturation *= 100.0f;

			rampData[channel-1].hue_end[pos-1] = currentColor1.hue;
			rampData[channel-1].sat_end[pos-1] = currentColor1.saturation;
			rampData[channel-1].val_end[pos-1] = currentColor1.brightness;

		}
	}
}

//static int multicolorsparkle(AMessage_st* s_Message_Rx,int channel, float *brightness_1, cJSON *configItem) {
//
//    cJSON *json = cJSON_CreateObject();
//    if (json == NULL) {
//        Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//    if (channelArray == NULL) {
//        cJSON_Delete(json);
//        Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//#ifdef ENABLE_PRINT_MSG
//    printf("JSON parsed successfully.\n");
//#endif
//
//    TurnFlagsOff(channel);
//
//#ifdef ENABLE_PRINT_MSG
//    printf("'CH' array extracted successfully.\n");
//#endif
//
//    // Extract brightness and config items
//
//    float brightness = 0;
//
//    cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//    cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//    cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//    configItem = (configItem1 != NULL) ? configItem1 : configItem;
//
//    if (brightnessItem != NULL && cJSON_IsNumber(brightnessItem)) {
//        brightness = brightnessItem->valuedouble;
//    } else {
//        brightness = *brightness_1;
//    }
//
//    if (brightnessIndexItem != NULL && cJSON_IsNumber(brightnessIndexItem)) {
//        if (brightness == 0) {
//            brightness = brightnessIndexItem->valuedouble;
//            brightness = fminf(fmaxf(brightness, 0), 10) * 10;
//        }
//    }
//
//#ifdef ENABLE_PRINT_MSG
//    printf("Brightness extracted and calculated: %f\n", brightness);
//#endif
//
//    if (cJSON_IsObject(configItem)) {
//        // Extract and validate start and end colors
//    	  // Extract the "Colors" array from the CONFIG object ---
//    	   cJSON *colorsArray = cJSON_GetObjectItem(configItem, "Colors");
//    	   if (!colorsArray || !cJSON_IsArray(colorsArray)) {
//    	       Add_Response_msg("Missing or invalid 'Colors' array in CONFIG.", s_Message_Rx, payLoadData);
//    	       cJSON_Delete(json);
//    	       return -1;
//    	   }
//    	   int numColors = cJSON_GetArraySize(colorsArray);
//    	   if (numColors <= 0) {
//    	       Add_Response_msg("No colors found in 'Colors' array.", s_Message_Rx, payLoadData);
//    	       cJSON_Delete(json);
//    	       return -1;
//    	   }
//
//    	   if (numColors > MAX_MultiColorSparkle_COLORS) {
//    	       numColors = MAX_MultiColorSparkle_COLORS; // Limit to MAX_MultiColorSparkle_COLORS
//    	   }
//
//           // Convert start and end colors from HSV to RGB
//    	   uint16_t ColorsRGB[MAX_MultiColorSparkle_COLORS][3], endColorRGB[3];
//
//    	      // Loop through each color segment in the "Colors" array.
//    	   for (int i = 0; i < numColors; i++) {
//    	       cJSON *colorObj = cJSON_GetArrayItem(colorsArray, i);
//    	       if (!colorObj || !cJSON_IsObject(colorObj)) {
//    	           // Skip any invalid entries.
//    	           continue;
//    	       }
//    	       // Extract the "color" array and "length" value from each segment.
//    	       cJSON *colorArr = cJSON_GetObjectItem(colorObj, "color");
//
//    	       uint16_t red = 0, green = 0, blue = 0;
//    	       if (colorArr && cJSON_IsArray(colorArr) && cJSON_GetArraySize(colorArr) == 3) {
//    	    	   red = cJSON_GetArrayItem(colorArr, 0)->valueint;
//    	    	   green = cJSON_GetArrayItem(colorArr, 1)->valueint;
//    	    	   blue = cJSON_GetArrayItem(colorArr, 2)->valueint;
//    	       }
//
//    	       // Store the extracted color and length.
//    	       ColorsRGB[i][0] = red;
//    	       ColorsRGB[i][1] = green;
//    	       ColorsRGB[i][2] = blue;
//    		}
//
//        cJSON *endColorArray = cJSON_GetObjectItem(configItem, "ENDCOLOR");
//
//        if (endColorArray == NULL || !cJSON_IsArray(endColorArray)) {
//            Add_Response_msg("Error: Invalid or missing color arrays in JSON.", s_Message_Rx, payLoadData);
//            cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//            printf("Invalid or missing color arrays.\n");
//#endif
//            return -1;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Color arrays extracted successfully.\n");
//#endif
//
//        // Extract other effect parameters
//        cJSON *intensityItem = cJSON_GetObjectItem(configItem, "INTENSITY");
//        cJSON *widthItem = cJSON_GetObjectItem(configItem, "WIDTH");
//        cJSON *decaytimeItem = cJSON_GetObjectItem(configItem, "DECAYTIME");
//
//        if (intensityItem == NULL || widthItem == NULL || decaytimeItem == NULL ||
//            !cJSON_IsNumber(intensityItem) || !cJSON_IsNumber(widthItem) || !cJSON_IsNumber(decaytimeItem)) {
//            Add_Response_msg("Error: Missing or invalid effect parameters in JSON.", s_Message_Rx, payLoadData);
//            cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//            printf("Missing or invalid effect parameters.\n");
//#endif
//            return -1;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Effect parameters extracted: Intensity = %f, Width = %f, DecayTime = %f\n", intensityItem->valuedouble, widthItem->valuedouble, decaytimeItem->valuedouble);
//#endif
//
//        // Retrieve the intensity, width, and decay time
//        float intensity = intensityItem->valuedouble;
//        float width = widthItem->valuedouble;
//        float decayTime = decaytimeItem->valuedouble;
//
//        if( (decayTime > 0) && (decayTime < 100) )
//        {
//        	decayTime = 100;	//mSec
//        }
//
//        for (int i = 0; i < 3; i++) {
//            cJSON *endColorItem = cJSON_GetArrayItem(endColorArray, i);
//
//            if ( endColorItem == NULL || !cJSON_IsNumber(endColorItem)) {
//                Add_Response_msg("Error: Invalid start or end color values in JSON.", s_Message_Rx, payLoadData);
//                cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//                printf("Invalid start or end color values.\n");
//#endif
//                return -1;
//            }
//
//            endColorRGB[i] = endColorItem->valueint;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Start and end colors converted from HSV to RGB.\n");
//#endif
//
//        // Placeholder variables for the RGB values
//        float brightness_factor = light_para.contrMaxB_float * 0.01;
//
//        // Calculate brightness factor for each channel
//		switch (channel) {
//			case 1: brightness_factor *= light_para.chan1MaxB_float * 0.01; break;
//			case 2: brightness_factor *= light_para.chan2MaxB_float * 0.01; break;
//			case 3: brightness_factor *= light_para.chan3MaxB_float * 0.01; break;
//			case 4: brightness_factor *= light_para.chan4MaxB_float * 0.01; break;
//			default:
//
//				Add_Response_msg("Error: Invalid channel in switch case.", s_Message_Rx, payLoadData);
//
//				cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//				printf("Invalid channel in switch case: %d\n", channel);
//#endif
//				return -1;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Channel %d brightness factor: %f\n", channel, brightness_factor);
//#endif
//
//		if (functionNullFlag == 1) {
//			brightness = brightness_RunTimeChan[channel - 1];
//		} else {
//			brightness_RunTimeChan[channel - 1] = brightness;
//		}
//
//		if (brightness != 0) {
//			brightness_factor = (brightness_factor * brightness) * 0.01;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Brightness factor adjusted for channel %d: %f\n", channel, brightness_factor);
//#endif
//
//		// Convert HSV to RGB
//#ifdef ENABLE_PRINT_MSG
//		printf("Converted start and end colors to RGB for channel %d.\n", channel);
//#endif
//
//		// Reset parameters for the channel
//		vTaskDelay(2 / portTICK_PERIOD_MS);
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Reset parameters for channel %d.\n", channel);
//#endif
//
//		// Calculate the number of LEDs and the spacing for the sparkle effect
//		uint32_t u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[channel - 1].LEDspacingCh_float / 25.4));
//
//		float offset_temp = (((ChannelParamObject[channel - 1].offsetCh_float) + (light_para.locationOffset_float)) * 25.4) / ChannelParamObject[channel - 1].LEDspacingCh_float;
//
//		u32StartPosition = fmaxf(1, fminf(u32StartPosition + offset_temp, SPARKLE_DISTANCE_IN_INCH));
//
//		uint32_t u32NumberOfLedBurst = generate_random(1, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[channel - 1].LEDspacingCh_float / 25.4)) * ChannelParamObject[channel - 1].scaleCh_float;
//
//		if (u32NumberOfLedBurst == 0)
//		{
//			u32NumberOfLedBurst = 1;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Calculated sparkle effect parameters for channel %d.\n", channel);
//#endif
//
//		if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//		{
//			// Initialize the parameters for this specific channel's sparkle effect
//			MultiColorSparkleParamObject_end[channel - 1].Width = width;
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor1_uint16[0] = ColorsRGB[0][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor1_uint16[1] = ColorsRGB[0][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor1_uint16[2] = ColorsRGB[0][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor2_uint16[0] = ColorsRGB[1][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor2_uint16[1] = ColorsRGB[1][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor2_uint16[2] = ColorsRGB[1][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor3_uint16[0] = ColorsRGB[2][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor3_uint16[1] = ColorsRGB[2][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor3_uint16[2] = ColorsRGB[2][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor4_uint16[0] = ColorsRGB[3][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor4_uint16[1] = ColorsRGB[3][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor4_uint16[2] = ColorsRGB[3][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor5_uint16[0] = ColorsRGB[4][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor5_uint16[1] = ColorsRGB[4][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor5_uint16[2] = ColorsRGB[4][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor6_uint16[0] = ColorsRGB[5][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor6_uint16[1] = ColorsRGB[5][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor6_uint16[2] = ColorsRGB[5][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor7_uint16[0] = ColorsRGB[6][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor7_uint16[1] = ColorsRGB[6][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor7_uint16[2] = ColorsRGB[6][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor8_uint16[0] = ColorsRGB[7][0];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor8_uint16[1] = ColorsRGB[7][1];
//			MultiColorSparkleParamObject_end[channel - 1].MultiColor8_uint16[2] = ColorsRGB[7][2];
//
//			MultiColorSparkleParamObject_end[channel - 1].EndColor_uint16[0] = endColorRGB[0];
//			MultiColorSparkleParamObject_end[channel - 1].EndColor_uint16[1] = endColorRGB[1];
//			MultiColorSparkleParamObject_end[channel - 1].EndColor_uint16[2] = endColorRGB[2];
//			MultiColorSparkleParamObject_end[channel - 1].Intensity = intensity;
//
//			MultiColorSparkleParamObject_end[channel - 1].Decaytime = decayTime;
//
//			MultiColorSparkleParamObject_end[channel - 1].numColors = numColors;
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Applied sparkle effect to LED strip for channel %d.\n", channel);
//		#endif
//
//			// Prepare data with the current mode settings
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Prepared data with mode settings for channel %d.\n", channel);
//		#endif
//
//			rampData[channel-1].RampStartTime = get_current_time_ms();
//			ExecuteSceneRampFlag[channel-1] = 1;
//		}
//		else
//		{
//			// Initialize the parameters for this specific channel's sparkle effect
//			MultiColorSparkleParamObject_start[channel - 1].Width = width;
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[0] = ColorsRGB[0][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[1] = ColorsRGB[0][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[2] = ColorsRGB[0][2];
//#ifdef ENABLE_PRINT_MSG
//			printf("MultiColorSparkleParamObject_start_R = %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[0]);
//			printf("MultiColorSparkleParamObject_start_G = %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[1]);
//			printf("MultiColorSparkleParamObject_start_B = %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor1_uint16[2]);
//#endif
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor2_uint16[0] = ColorsRGB[1][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor2_uint16[1] = ColorsRGB[1][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor2_uint16[2] = ColorsRGB[1][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor3_uint16[0] = ColorsRGB[2][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor3_uint16[1] = ColorsRGB[2][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor3_uint16[2] = ColorsRGB[2][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor4_uint16[0] = ColorsRGB[3][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor4_uint16[1] = ColorsRGB[3][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor4_uint16[2] = ColorsRGB[3][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor5_uint16[0] = ColorsRGB[4][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor5_uint16[1] = ColorsRGB[4][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor5_uint16[2] = ColorsRGB[4][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor6_uint16[0] = ColorsRGB[5][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor6_uint16[1] = ColorsRGB[5][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor6_uint16[2] = ColorsRGB[5][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor7_uint16[0] = ColorsRGB[6][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor7_uint16[1] = ColorsRGB[6][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor7_uint16[2] = ColorsRGB[6][2];
//
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[0] = ColorsRGB[7][0];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[1] = ColorsRGB[7][1];
//			MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[2] = ColorsRGB[7][2];
//#ifdef ENABLE_PRINT_MSG
//			printf("MultiColorSparkleParamObject_start_R 8= %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[0]);
//			printf("MultiColorSparkleParamObject_start_G 8= %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[1]);
//			printf("MultiColorSparkleParamObject_start_B 8= %d \n", MultiColorSparkleParamObject_start[channel - 1].MultiColor8_uint16[2]);
//#endif
//			MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[0] = endColorRGB[0];
//			MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[1] = endColorRGB[1];
//			MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[2] = endColorRGB[2];
//#ifdef ENABLE_PRINT_MSG
//			printf("MultiColorSparkleParamObject_start_R END = %d \n", MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[0]);
//			printf("MultiColorSparkleParamObject_start_G END = %d \n", MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[1]);
//			printf("MultiColorSparkleParamObject_start_B END = %d \n", MultiColorSparkleParamObject_start[channel - 1].EndColor_uint16[2]);
//#endif
//			MultiColorSparkleParamObject_start[channel - 1].Intensity = intensity;
//
//			MultiColorSparkleParamObject_start[channel - 1].Decaytime = decayTime;
//			MultiColorSparkleParamObject_start[channel - 1].numColors = numColors;
//
//#ifdef ENABLE_PRINT_MSG
//			printf("numColors = %d \n", MultiColorSparkleParamObject_start[channel - 1].numColors);
//#endif
//		#ifdef ENABLE_PRINT_MSG
//			printf("Applied sparkle effect to LED strip for channel %d.\n", channel);
//		#endif
//
//			// Prepare data with the current mode settings
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Prepared data with mode settings for channel %d.\n", channel);
//		#endif
//			MultiColorSparkleStartFlag[channel - 1]=1;
//		}
//    }
//
//    if (functionNullFlag == 1) {
//        functionNullFlag = 0;
//    }
//
//    // Clean up and delete the JSON object
//
//    cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//    printf("Cleaned up and deleted JSON object.\n");
//#endif
//
//    return 1;
//}

//static int sparkle(AMessage_st* s_Message_Rx,int channel, float *brightness_1, cJSON *configItem) {
//
//    cJSON *json = cJSON_CreateObject();
//    if (json == NULL) {
//        Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//    if (channelArray == NULL) {
//        cJSON_Delete(json);
//        Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//        return -1;
//    }
//    cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//#ifdef ENABLE_PRINT_MSG
//    printf("JSON parsed successfully.\n");
//#endif
//
//    TurnFlagsOff(channel);
//    bytes_per_channel[channel-1] = 200;
//
//#ifdef ENABLE_PRINT_MSG
//    printf("'CH' array extracted successfully.\n");
//#endif
//
//    // Extract brightness and config items
//
//    float brightness = 0;
//
//    cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//    cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//    cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//    configItem = (configItem1 != NULL) ? configItem1 : configItem;
//
//    if (brightnessItem != NULL && cJSON_IsNumber(brightnessItem)) {
//        brightness = brightnessItem->valuedouble;
//    } else {
//        brightness = *brightness_1;
//    }
//
//    if (brightnessIndexItem != NULL && cJSON_IsNumber(brightnessIndexItem)) {
//        if (brightness == 0) {
//            brightness = brightnessIndexItem->valuedouble;
//            brightness = fminf(fmaxf(brightness, 0), 10) * 10;
//        }
//    }
//
//#ifdef ENABLE_PRINT_MSG
//    printf("Brightness extracted and calculated: %f\n", brightness);
//#endif
//
//    if (cJSON_IsObject(configItem)) {
//        // Extract and validate start and end colors
//
//        cJSON *startColorArray = cJSON_GetObjectItem(configItem, "STARTCOLOR");
//        cJSON *endColorArray = cJSON_GetObjectItem(configItem, "ENDCOLOR");
//
//        if (startColorArray == NULL || !cJSON_IsArray(startColorArray) ||
//            endColorArray == NULL || !cJSON_IsArray(endColorArray)) {
//            Add_Response_msg("Error: Invalid or missing color arrays in JSON.", s_Message_Rx, payLoadData);
//            cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//            printf("Invalid or missing color arrays.\n");
//#endif
//            return -1;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Color arrays extracted successfully.\n");
//#endif
//
//        // Extract other effect parameters
//        cJSON *intensityItem = cJSON_GetObjectItem(configItem, "INTENSITY");
//        cJSON *widthItem = cJSON_GetObjectItem(configItem, "WIDTH");
//        cJSON *decaytimeItem = cJSON_GetObjectItem(configItem, "DECAYTIME");
//
//        if (intensityItem == NULL || widthItem == NULL || decaytimeItem == NULL ||
//            !cJSON_IsNumber(intensityItem) || !cJSON_IsNumber(widthItem) || !cJSON_IsNumber(decaytimeItem)) {
//            Add_Response_msg("Error: Missing or invalid effect parameters in JSON.", s_Message_Rx, payLoadData);
//            cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//            printf("Missing or invalid effect parameters.\n");
//#endif
//            return -1;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Effect parameters extracted: Intensity = %f, Width = %f, DecayTime = %f\n", intensityItem->valuedouble, widthItem->valuedouble, decaytimeItem->valuedouble);
//#endif
//
//        // Retrieve the intensity, width, and decay time
//        float intensity = intensityItem->valuedouble;
//        float width = widthItem->valuedouble;
//        float decayTime = decaytimeItem->valuedouble;
//
//        if( (decayTime > 0) && (decayTime < 100) )
//        {
//        	decayTime = 100;	//mSec
//        }
//
//        // Convert start and end colors from HSV to RGB
//        float startColorHSV[3], endColorHSV[3];
//
//        for (int i = 0; i < 3; i++) {
//            cJSON *startColorItem = cJSON_GetArrayItem(startColorArray, i);
//            cJSON *endColorItem = cJSON_GetArrayItem(endColorArray, i);
//
//            if (startColorItem == NULL || endColorItem == NULL || !cJSON_IsNumber(startColorItem) || !cJSON_IsNumber(endColorItem)) {
//                Add_Response_msg("Error: Invalid start or end color values in JSON.", s_Message_Rx, payLoadData);
//                cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//                printf("Invalid start or end color values.\n");
//#endif
//                return -1;
//            }
//
//            startColorHSV[i] = startColorItem->valuedouble;
//            endColorHSV[i] = endColorItem->valuedouble;
//        }
//
//#ifdef ENABLE_PRINT_MSG
//        printf("Start and end colors converted from HSV to RGB.\n");
//#endif
//
//        // Placeholder variables for the RGB values
//        uint16_t red1 = 0, green1 = 0, blue1 = 0;
//        uint16_t red2 = 0, green2 = 0, blue2 = 0;
//        float brightness_factor = light_para.contrMaxB_float * 0.01;
//
//        // Calculate brightness factor for each channel
//		switch (channel) {
//			case 1: brightness_factor *= light_para.chan1MaxB_float * 0.01; break;
//			case 2: brightness_factor *= light_para.chan2MaxB_float * 0.01; break;
//			case 3: brightness_factor *= light_para.chan3MaxB_float * 0.01; break;
//			case 4: brightness_factor *= light_para.chan4MaxB_float * 0.01; break;
//			default:
//
//				Add_Response_msg("Error: Invalid channel in switch case.", s_Message_Rx, payLoadData);
//
//				cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//				printf("Invalid channel in switch case: %d\n", channel);
//#endif
//				return -1;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Channel %d brightness factor: %f\n", channel, brightness_factor);
//#endif
//
//		if (functionNullFlag == 1) {
//			brightness = brightness_RunTimeChan[channel - 1];
//		} else {
//			brightness_RunTimeChan[channel - 1] = brightness;
//		}
//
//		if (brightness != 0) {
//			brightness_factor = (brightness_factor * brightness) * 0.01;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Brightness factor adjusted for channel %d: %f\n", channel, brightness_factor);
//#endif
//
//		// Convert HSV to RGB
//		hsv_to_rgb_16bit(startColorHSV[0], startColorHSV[1], startColorHSV[2] * brightness_factor, &red1, &green1, &blue1);
//		hsv_to_rgb_16bit(endColorHSV[0], endColorHSV[1], endColorHSV[2] * brightness_factor, &red2, &green2, &blue2);
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Converted start and end colors to RGB for channel %d.\n", channel);
//#endif
//
//		// Reset parameters for the channel
//		vTaskDelay(2 / portTICK_PERIOD_MS);
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Reset parameters for channel %d.\n", channel);
//#endif
//
//		// Calculate the number of LEDs and the spacing for the sparkle effect
//		uint32_t u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[channel - 1].LEDspacingCh_float / 25.4));
//
//		float offset_temp = (((ChannelParamObject[channel - 1].offsetCh_float) + (light_para.locationOffset_float)) * 25.4) / ChannelParamObject[channel - 1].LEDspacingCh_float;
//
//		u32StartPosition = fmaxf(1, fminf(u32StartPosition + offset_temp, SPARKLE_DISTANCE_IN_INCH));
//
//		uint32_t u32NumberOfLedBurst = generate_random(1, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[channel - 1].LEDspacingCh_float / 25.4)) * ChannelParamObject[channel - 1].scaleCh_float;
//
//		if (u32NumberOfLedBurst == 0)
//		{
//			u32NumberOfLedBurst = 1;
//		}
//
//#ifdef ENABLE_PRINT_MSG
//		printf("Calculated sparkle effect parameters for channel %d.\n", channel);
//#endif
//
//		if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//		{
//			// Initialize the parameters for this specific channel's sparkle effect
//			SparkleParamObject_end[channel - 1].Width = width;
//			SparkleParamObject_end[channel - 1].StartColor_float[0] = startColorHSV[0];
//			SparkleParamObject_end[channel - 1].StartColor_float[1] = startColorHSV[1];
//			SparkleParamObject_end[channel - 1].StartColor_float[2] = startColorHSV[2];
//			SparkleParamObject_end[channel - 1].EndColor_float[0] = endColorHSV[0];
//			SparkleParamObject_end[channel - 1].EndColor_float[1] = endColorHSV[1];
//			SparkleParamObject_end[channel - 1].EndColor_float[2] = endColorHSV[2];
//			SparkleParamObject_end[channel - 1].Intensity = intensity;
//
//			SparkleParamObject_end[channel - 1].Decaytime = decayTime;
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Applied sparkle effect to LED strip for channel %d.\n", channel);
//		#endif
//
//			// Prepare data with the current mode settings
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Prepared data with mode settings for channel %d.\n", channel);
//		#endif
//			rampData[channel-1].RampStartTime = get_current_time_ms();
//			ExecuteSceneRampFlag[channel-1] = 1;
//		}
//		else
//		{
//			// Initialize the parameters for this specific channel's sparkle effect
//			SparkleParamObject_start[channel - 1].Width = width;
//			SparkleParamObject_start[channel - 1].StartColor_float[0] = startColorHSV[0];
//			SparkleParamObject_start[channel - 1].StartColor_float[1] = startColorHSV[1];
//			SparkleParamObject_start[channel - 1].StartColor_float[2] = startColorHSV[2];
//			SparkleParamObject_start[channel - 1].EndColor_float[0] = endColorHSV[0];
//			SparkleParamObject_start[channel - 1].EndColor_float[1] = endColorHSV[1];
//			SparkleParamObject_start[channel - 1].EndColor_float[2] = endColorHSV[2];
//			SparkleParamObject_start[channel - 1].Intensity = intensity;
//			SparkleParamObject_start[channel - 1].Decaytime = decayTime;
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Applied sparkle effect to LED strip for channel %d.\n", channel);
//		#endif
//
//			// Prepare data with the current mode settings
//
//		#ifdef ENABLE_PRINT_MSG
//			printf("Prepared data with mode settings for channel %d.\n", channel);
//		#endif
//			SparkleStartFlag[channel - 1]=1;
//		}
//    }
//
//    if (functionNullFlag == 1) {
//        functionNullFlag = 0;
//    }
//
//    // Clean up and delete the JSON object
//
//    cJSON_Delete(json);
//
//#ifdef ENABLE_PRINT_MSG
//    printf("Cleaned up and deleted JSON object.\n");
//#endif
//
//    return 1;
//}

//static int pattern(AMessage_st* s_Message_Rx, int channel, float *brightness_1,cJSON *configItem)
//{
//	cJSON *json = cJSON_CreateObject();
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	// Extract values from JSON
//	TurnFlagsOff(channel);
//
//	float brightness = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness =*brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		if (cJSON_IsObject(configItem))
//		{
//			cJSON *typeItem = cJSON_GetObjectItem(configItem, "TYPE");
//
//			if (typeItem == NULL)
//			{
//				// Handle missing transitionTypeItem values in JSON
//				Add_Response_msg("Error: Missing type values in JSON. ", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			cJSON *periodItem = cJSON_GetObjectItem(configItem, "PERIOD");
//
//			if (periodItem == NULL)
//			{
//				// Handle missing movingSpeedItem values in JSON
//				Add_Response_msg("Error: Missing periodI values in JSON. ", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			uint8_t typeColor_u8 = 0;
//			if(!strcmp(typeItem->valuestring, "All Colors"))
//			{
//				typeColor_u8 = 1;
//			}
//			else if(!strcmp(typeItem->valuestring, "Warm Colors"))
//			{
//				typeColor_u8 = 2;
//			}
//			else if(!strcmp(typeItem->valuestring, "Cool Colors"))
//			{
//				typeColor_u8 = 3;
//			}
//			else
//			{
//				// Handle invalid JSON string
//				Add_Response_msg("Error: Invalid type in JSON. ", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			float periodPattern_float_temp = 0;
//
//			uint64_t u64CurrentTime = 0;
//
//			if(periodItem->valuedouble == 0)
//			{
//				periodPattern_float_temp = 500;							// mSec
//			}
//			else
//			{
//				periodPattern_float_temp = (periodItem->valuedouble)*1000; 	//In mili seconds
//			}
//
//		    // Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//			{
//				bytes_per_channel[channel-1] = 200;
//
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//			}
//
//			if(functionNullFlag == 1)
//			{
//				brightness = brightness_RunTimeChan[channel-1];
//			}
//			else
//			{
//				brightness_RunTimeChan[channel-1] = brightness;
//			}
//
//			if(brightness != 0)
//			{
//				brightness_factor = (brightness_factor*brightness) * 0.01;;
//			}
//
//			u64CurrentTime = get_current_time_ms();		// In millisecond
//
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				periodPattern_float_end[channel-1] = periodPattern_float_temp/2;
//
//				PatternStartFlag_end[channel-1] = 1;
//
//				pattern_Start_time_end[channel-1] = u64CurrentTime;
//
//				if(typeColor_u8 == 1)
//				{
//					NewPattern_u16Hue_end[channel-1][0] = 0;
//					NewPattern_u16Hue_end[channel-1][1] = 180;
//					NewPattern_u16Hue_end[channel-1][2] = 360;
//				}
//				else if(typeColor_u8 == 2)
//				{
//					NewPattern_u16Hue_end[channel-1][0] = 0;
//					NewPattern_u16Hue_end[channel-1][1] = 60;
//					NewPattern_u16Hue_end[channel-1][2] = 0;
//				}
//				else if(typeColor_u8 == 3)
//				{
//					NewPattern_u16Hue_end[channel-1][0] = 248;
//					NewPattern_u16Hue_end[channel-1][1] = 129;
//					NewPattern_u16Hue_end[channel-1][2] = 248;
//				}
//
//				rampData[channel-1].RampStartTime = u64CurrentTime;
//				ExecuteSceneRampFlag[channel-1] = 1;
//			}
//			else
//			{
//				periodPattern_float_start[channel-1] = periodPattern_float_temp/2;
//
//				PatternStartFlag_start[channel-1] = 1;
//
//				pattern_Start_time_start[channel-1] = u64CurrentTime;
//
//				if(typeColor_u8 == 1)
//				{
//					NewPattern_u16Hue_start[channel-1][0] = 0;
//					NewPattern_u16Hue_start[channel-1][1] = 180;
//					NewPattern_u16Hue_start[channel-1][2] = 360;
//				}
//				else if(typeColor_u8 == 2)
//				{
//					NewPattern_u16Hue_start[channel-1][0] = 0;
//					NewPattern_u16Hue_start[channel-1][1] = 60;
//					NewPattern_u16Hue_start[channel-1][2] = 0;
//				}
//				else if(typeColor_u8 == 3)
//				{
//					NewPattern_u16Hue_start[channel-1][0] = 248;
//					NewPattern_u16Hue_start[channel-1][1] = 129;
//					NewPattern_u16Hue_start[channel-1][2] = 248;
//				}
//			}
//		}
//	}
//	else
//	{
//		// Handle invalid JSON structure
//		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
//	}
//
//	if(functionNullFlag == 1)
//	{
//		functionNullFlag = 0;
//	}
//	cJSON_Delete(json);
//	return 1;
//}

/**
 * ExecuteMarquee
 *
 * Parses the JSON input for the marquee effect and fills in the marqueeImage_t
 * configuration structure for the specified channel.
 *
 * Expected JSON format:
 * {
 *   "CH": [2],
 *   "BRIGHTNESS": 50,
 *   "CONFIG": {
 *     "Colors": [
 *         { "color": [12, 100, 100], "length": 3.0 },
 *         { "color": [77, 100, 50],  "length": 6.0 },
 *         { "color": [99, 80, 80],   "length": 9.9 }
 *     ],
 *     "transitionType": "None",
 *     "enableMirror": 0,
 *     "mirrorPosition": 0,
 *     "oscAmp": 0,
 *     "oscPeriod": 0,
 *     "movingSpeed": 0
 *   }
 * }
 *
 * @param s_Message_Rx   Pointer to the incoming message structure.
 * @param channel        The channel number (1-indexed).
 * @param brightness_1   Pointer to a stored brightness value.
 * @param configItem     (Optional) JSON config item pointer.
 *
 * @return 1 on success, -1 on error.
 */
//static int ExecuteMarquee(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//   cJSON *json = cJSON_CreateObject();
//   if (json == NULL) {
//       Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//       return -1;
//   }
//   cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//   if (channelArray == NULL) {
//       cJSON_Delete(json);
//       Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//       return -1;
//   }
//   cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//   // For example, if CH contains only a 0 value, treat it as invalid.
//   if (cJSON_GetArraySize(channelArray) == 1 &&
//       cJSON_GetArrayItem(channelArray, 0)->valueint == 0)
//   {
//       Add_Response_msg("Invalid channel.", s_Message_Rx, payLoadData);
//       cJSON_Delete(json);
//       return -1;
//   }
//
//   // --- 3. Extract brightness ---
//   float brightness = 0.0f;
//   cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//   cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//   if (brightnessItem) {
//       brightness = brightnessItem->valuedouble;
//   } else {
//       // Use the stored brightness if not provided explicitly
//       brightness = *brightness_1;
//   }
//   // Optionally adjust brightness based on BRIGHTNESSINDEX if brightness is zero
//   if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && (brightness == 0)) {
//       float tmp = brightnessIndexItem->valuedouble * 10.0f;
//       if (tmp < 0)   tmp = 0;
//       else if (tmp > 100) tmp = 100;
//       brightness = tmp;
//   }
//
//   // --- 4. Extract the "CONFIG" object from JSON ---
//
//   cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//	if (configItem1 != NULL )
//	{
//		configItem = configItem1;
//	}
//
//   uint8_t spacingOverrideV = 0;
//   cJSON *spacingOverrideItem = cJSON_GetObjectItem(configItem, "spacingOverride");
//   if (spacingOverrideItem)
//   {
//	   spacingOverrideV = spacingOverrideItem->valueint;
//   }
//
//   // --- 5. Extract the "Colors" array from the CONFIG object ---
//   cJSON *colorsArray = cJSON_GetObjectItem(configItem, "Colors");
//   if (!colorsArray || !cJSON_IsArray(colorsArray)) {
//       Add_Response_msg("Missing or invalid 'Colors' array in CONFIG.", s_Message_Rx, payLoadData);
//       cJSON_Delete(json);
//       return -1;
//   }
//   int numColors = cJSON_GetArraySize(colorsArray);
//   if (numColors <= 0) {
//       Add_Response_msg("No colors found in 'Colors' array.", s_Message_Rx, payLoadData);
//       cJSON_Delete(json);
//       return -1;
//   }
//
//   TurnFlagsOff(channel);
//
//   if (numColors > MAX_MARQUEE_COLORS) {
//       numColors = MAX_MARQUEE_COLORS; // Limit to MAX_MARQUEE_COLORS
//   }
//
//   // --- 6. Select the proper marquee configuration structure ---
//   // If a ramp (scene transition) is active, use the "end" state.
//   int chanIndex = channel - 1;  // Convert to 0-index
//   int isRampActive = ((rampData[chanIndex].DwellTimeSceneVal != 0) ||
//                       (rampData[chanIndex].RampTimeSceneVal  != 0));
//   marqueeImage_t *mImg = (isRampActive)
//                          ? &marqueeImage_end[chanIndex]
//                          : &marqueeImage_start[chanIndex];
//
////   uint8_t spacingOverrideV = 0;
//	mImg->spacingOverride = spacingOverrideV;
//// --- 7. Populate the marqueeImage_t structure ---
//   mImg->numColors = numColors;
//   mImg->totalLengthInches = 0.0f;  // Initialize total length
//
//   // Loop through each color segment in the "Colors" array.
//   for (int i = 0; i < numColors; i++) {
//       cJSON *colorObj = cJSON_GetArrayItem(colorsArray, i);
//       if (!colorObj || !cJSON_IsObject(colorObj)) {
//           // Skip any invalid entries.
//           continue;
//       }
//       // Extract the "color" array and "length" value from each segment.
//       cJSON *colorArr = cJSON_GetObjectItem(colorObj, "color");
//       cJSON *lengthItem = cJSON_GetObjectItem(colorObj, "length");
//
//       float hue = 0.0f, sat = 0.0f, val = 0.0f, lengthInch = 0.0f;
//       if (colorArr && cJSON_IsArray(colorArr) && cJSON_GetArraySize(colorArr) == 3) {
//           hue = cJSON_GetArrayItem(colorArr, 0)->valuedouble;
//           sat = cJSON_GetArrayItem(colorArr, 1)->valuedouble;
//           sat = sat*100;
//           val = cJSON_GetArrayItem(colorArr, 2)->valuedouble;
//           val = val*100;
//       }
//       if (lengthItem && cJSON_IsNumber(lengthItem)) {
//           lengthInch = (float)lengthItem->valuedouble;
//    	   if(spacingOverrideV == 1)	//Override color length if spacingOverrideV = 1
//    	   {
//    		   lengthInch = lengthInch*((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4);
//    	   }
//       }
//
//       // Store the extracted color and length.
//       mImg->colors[i].hue = hue;
//       mImg->colors[i].saturation = sat;
//       mImg->colors[i].brightness = val;
//
//       if(lengthInch <= 0.0f)
//       {
//    	   lengthInch = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4);
//       }
//       mImg->colors[i].lengthInches = lengthInch;
//
////       printf("h= %f, s= %f, v= %f, l= %f,   \n", mImg->colors[i].hue, mImg->colors[i].saturation, mImg->colors[i].brightness, mImg->colors[i].lengthInches);
//       // Accumulate the total length of the marquee pattern.
//       mImg->totalLengthInches += lengthInch;
//   }
//
//   if(mImg->totalLengthInches < ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4))
//   {
//	   mImg->totalLengthInches = ((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4);
//   }
//
//   ImageSize_forMode[chanIndex]= mImg->totalLengthInches;
//
//	if((EXAMPLE_LED_NUMBERS*3) < ImageSize_forMode[chanIndex])
//	{
//		ImageSize_forMode[chanIndex] = (EXAMPLE_LED_NUMBERS*3);
//	}
//
//	bytes_per_channel[chanIndex] = ImageSize_forMode[chanIndex];
//
//   // --- 8. Extract additional configuration parameters ---
//   // Transition type: if "None", set to 0; otherwise, set to 1 for gradient transitions.
//   cJSON *transitionTypeItem = cJSON_GetObjectItem(configItem, "transitionType");
//   if (transitionTypeItem != NULL && cJSON_IsString(transitionTypeItem)) {
//       mImg->transitionType = (strcmp(transitionTypeItem->valuestring, "None") == 0) ? 0 : 1;
//   } else {
//       mImg->transitionType = 0;  // Default to "None"
//   }
//   // Mirror enable flag.
////   cJSON *enableMirrorItem = cJSON_GetObjectItem(configObj, "enableMirror");
//   cJSON *enableMirrorItem = cJSON_GetObjectItem(configItem, "mirrorEnable");
//   mImg->enableMirror = (enableMirrorItem != NULL) ? enableMirrorItem->valueint : 0;
//   // Mirror position (in inches).
//   cJSON *mirrorPositionItem = cJSON_GetObjectItem(configItem, "mirrorPosition");
//   mImg->mirrorPosition = (mirrorPositionItem != NULL) ? mirrorPositionItem->valuedouble : 0;
//
////   light_para.locationMidpoint_float = mImg->mirrorPosition;		//Is this to be done? //Sanjeev
//
//   // Oscillation amplitude.
//   cJSON *oscAmpItem = cJSON_GetObjectItem(configItem, "oscAmp");
//   mImg->oscAmp = (oscAmpItem != NULL) ? oscAmpItem->valuedouble : 0;
//   // Oscillation period.
//   cJSON *oscPeriodItem = cJSON_GetObjectItem(configItem, "oscPeriod");
//   mImg->oscPeriod = (oscPeriodItem != NULL) ? oscPeriodItem->valuedouble : 0;
//   // Moving speed (if used in further logic).
////   cJSON *movingSpeedItem = cJSON_GetObjectItem(configObj, "movingSpeed");
//   cJSON *movingSpeedItem = cJSON_GetObjectItem(configItem, "Speed");
//   mImg->movingSpeed = (movingSpeedItem != NULL) ? movingSpeedItem->valuedouble : 0;
//
//
//   cJSON *brightnessWavelengthItem = cJSON_GetObjectItem(configItem, "brightnessWavelength");
//   mImg->brightnessWavelength = (brightnessWavelengthItem != NULL) ? brightnessWavelengthItem->valuedouble : 0;
//
//   cJSON *brightnessAmplitudeItem = cJSON_GetObjectItem(configItem, "brightnessAmplitude");
//   mImg->brightnessAmplitude = (brightnessAmplitudeItem != NULL) ? brightnessAmplitudeItem->valuedouble : 0;
//
//   cJSON *brightnessSpeedItem = cJSON_GetObjectItem(configItem, "brightnessSpeed");
//   mImg->brightnessSpeed = (brightnessSpeedItem != NULL) ? brightnessSpeedItem->valuedouble : 0;
//
////   printf("enableMirror = %d, mirrorPosition = %f,  oscAmp = %f, oscPeriod = %f, movingSpeed = %f \n", mImg->enableMirror, mImg->mirrorPosition, mImg->oscAmp, mImg->oscPeriod, mImg->movingSpeed  );
//
//   mImg->movingSpeed = -1*mImg->movingSpeed;
//
//   if (mImg->spacingOverride == 1) {
//       float pitch_in = (ChannelParamObject[channel-1].LEDspacingCh_float) * (1.0f/25.4f);
//       mImg->movingSpeed *= pitch_in; // pixels/sec -> inches/sec
//   }
//   float movingSpeed_float = 0;
//   float movingSpeed_float1 = 0;
//   float temp_cal = 0;
//
//   movingSpeed_float = mImg->movingSpeed;
//
//   if(movingSpeed_float == 0)
//   {
//	   MarqueeCustomStartFlag_offset1[channel-1] = 1;
//	   One_LED_time[channel-1] = 0;
//   }
//   else if(movingSpeed_float < 0)
//   {//Negative direction
//	   movingSpeed_float1 = -movingSpeed_float;
//	   ChannelParamObject[channel-1].speedrevDirCh_u8 = 1;
//	   temp_cal = (1*1000)/ (movingSpeed_float1);			//One inch in milisecond
//	   One_LED_time[channel-1] = temp_cal;
//   }
//   else
//   {//Positve direction
//	   ChannelParamObject[channel-1].speedrevDirCh_u8 = 0;
//
//	   temp_cal = (1*1000)/ (movingSpeed_float);			//One inch in milisecond
//	   One_LED_time[channel-1] = temp_cal;
//   }
//
//
//	if(( mImg->oscAmp != 0 ) && ( mImg->oscPeriod != 0 ))
//	{
//		oscP_Flag[channel-1] = 1;
//	}
//	else
//	{
//		oscP_Flag[channel-1] = 0;
//	}
//
//	enableMirror_uint8[channel-1] = mImg->enableMirror;
//	temp_cal = ((mImg->mirrorPosition)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;
//	MirrorLedNum[channel-1] = 1*temp_cal;
//
//	float temp_cal1 = 0;
//
//	if(oscP_Flag[channel-1] != 0)
//	{
//		temp_cal1 = 0;
//
//		temp_cal = (mImg->oscAmp)/(mImg->oscPeriod);
//
//		if(ChannelParamObject[channel-1].speedrevDirCh_u8 == 0)
//		{
//			temp_cal1 = (movingSpeed_float);
//
//			oscOffsetMax[channel-1] = 1*((mImg->oscPeriod)*temp_cal);
//			oscOffsetMin[channel-1] = 1*(temp_cal1+((mImg->oscPeriod)*temp_cal));
//
//			temp_cal1 = temp_cal1+(temp_cal*2);
//
//			One_LED_time[channel-1] = 1000/(temp_cal*2);
//
//			One_LED_time_back[channel-1] = (1000/temp_cal1);
//
//		}
//		else
//		{
//			movingSpeed_float1 = -movingSpeed_float;
//			temp_cal1 = (movingSpeed_float1);
//
//			oscOffsetMax[channel-1] = 1*(temp_cal1+((mImg->oscPeriod)*temp_cal));
//			oscOffsetMin[channel-1] = 1*((mImg->oscPeriod)*temp_cal);
//
//			temp_cal1 = temp_cal1+(temp_cal*2);
//
//			One_LED_time[channel-1] = (1000/temp_cal1);
//			One_LED_time_back[channel-1] = 1000/(temp_cal*2);
//		}
//
//		oscOffset[channel-1] = 0;
//		oscOffset_forward[channel-1] = 0;
//		oscOffset_back[channel-1] = 0;
//	}
//
//   if (mImg->enableMirror == 1)
//   {
//       // Mirror effect data arrangement (if necessary)
//   }
//   else
//   {
//       if (mImg->oscAmp != 0 && mImg->oscPeriod != 0)
//       {
//           uint64_t u64CurrentTime = get_current_time_ms();
//           oscStart_time[channel-1] = u64CurrentTime;
//       }
//   }
//
//   // --- 9. Compute brightness factor (global and channel-specific) ---
//   float brightness_factor = (light_para.contrMaxB_float * 0.01f);
//   switch (channel) {
//       case 1: brightness_factor *= (light_para.chan1MaxB_float * 0.01f); break;
//       case 2: brightness_factor *= (light_para.chan2MaxB_float * 0.01f); break;
//       case 3: brightness_factor *= (light_para.chan3MaxB_float * 0.01f); break;
//       case 4: brightness_factor *= (light_para.chan4MaxB_float * 0.01f); break;
//       default:
//           Add_Response_msg("Invalid channel number.", s_Message_Rx, payLoadData);
//           cJSON_Delete(json);
//           return -1;
//   }
//   // Use the provided brightness if available.
//   if (functionNullFlag == 1) {
//       brightness = brightness_RunTimeChan[chanIndex];
//       functionNullFlag = 0;
//   } else {
//       brightness_RunTimeChan[chanIndex] = brightness;
//   }
//   if (brightness != 0) {
//       brightness_factor *= (brightness * 0.01f);
//   }
//
//   // --- 10. Render the marquee effect immediately or set up for a ramp ---
//   if (!isRampActive) {
//	   uint64_t u64CurrentTime = get_current_time_ms();
//
//       // fill_data==0 means we are writing the final LED output immediately.
//       ExecuteMarquee_PrepareDataWithModeSetting(0.0f, chanIndex, 1, u64CurrentTime, 0);
//
//       MarqueeExecuteCustomStartFlag[chanIndex] = 1;
//   } else {
//       // If ramping is active, set ramp flags and record the start time.
//       rampData[chanIndex].RampStartTime = get_current_time_ms();
//       ExecuteSceneRampFlag[chanIndex] = 1;
//   }
//
//   // --- 11. Clean up ---
//   cJSON_Delete(json);
//   return 1;
//}

void ExecuteMarquee_PrepareDataWithModeSetting(float offset1, int Chan, int Start_Offset_Flag, uint64_t currentTimeMs, int fill_data)
{
    // --- 1. Select the proper marquee configuration ---
    int isRamp = (fill_data == 2) ? 1 : 0;
    marqueeImage_t *mImg = isRamp ? &marqueeImage_end[Chan] : &marqueeImage_start[Chan];

    // --- 2. Set up time and channel parameters ---
    // currentTime in seconds (modulo some large value to prevent overflow)
//    float currentTime = (float)(currentTimeMs % 100000000) * 0.001f;
//    currentTime = ((currentTime*25.4)/ChannelParamObject[Chan].LEDspacingCh_float);
    float currentTime = (float)(currentTimeMs % 100000000) * 0.0001f;
//    float currentTime = (float)(currentTimeMs % 1000000) * 0.001f;

    ChannelParameters *chParam = &ChannelParamObject[Chan];

    // Guard against division by zero in LED spacing.
    if(chParam->LEDspacingCh_float == 0.0f) {
#ifdef ENABLE_PRINT_MSG
        printf("ERROR: LEDspacingCh_float is zero for channel %d\n", Chan);
#endif
        return;
    }

    // Guard against zero total length to avoid infinite loops.
    if(mImg->totalLengthInches <= 0.0f) {
#ifdef ENABLE_PRINT_MSG
        printf("ERROR: totalLengthInches is non-positive for channel %d\n", Chan);
#endif
        return;
    }

    // --- 3. Convert and precompute spacing values ---
    float scale = chParam->scaleCh_float;
    // Convert LED spacing from mm to inches (1 inch = 25.4 mm)
    float LEDspacingInches = chParam->LEDspacingCh_float / 25.4f;
    // scaleSpacing is the effective spacing adjusted by the scaling factor
    float scaleSpacing = scale * LEDspacingInches;

    // Compute effective starting offset.
    float offset = 0.0f;
    if(mImg->spacingOverride == 0)
    {
    	offset = chParam->offsetCh_float + offset1 + light_para.locationOffset_float;
    }
    else
    {
    	offset = (((chParam->offsetCh_float + light_para.locationOffset_float)* chParam->LEDspacingCh_float)/25.4) + offset1;
    }
    // Wrap offset to [0, totalLength)
    float totalLength = mImg->totalLengthInches;
    // Instead of loops we could also use fmodf, but ensure offset is positive.
    if (offset < 0)
        offset = totalLength - fmodf(-offset, totalLength);
    else
        offset = fmodf(offset, totalLength);

    // --- 4. Compute the brightness global factor ---
    float brightness_factor = (light_para.contrMaxB_float * 0.01f);
    switch (Chan + 1) {
        case 1: brightness_factor *= (light_para.chan1MaxB_float * 0.01f); break;
        case 2: brightness_factor *= (light_para.chan2MaxB_float * 0.01f); break;
        case 3: brightness_factor *= (light_para.chan3MaxB_float * 0.01f); break;
        case 4: brightness_factor *= (light_para.chan4MaxB_float * 0.01f); break;
        default:
#ifdef ENABLE_PRINT_MSG
            printf("ERROR: Invalid channel: %d\n", Chan);
#endif
            return;
    }
    if (brightness_RunTimeChan[Chan] != 0) {
        brightness_factor *= (brightness_RunTimeChan[Chan] * 0.01f);
    }

    // --- 5. Determine the output LED buffer pointer ---
    uint16_t *data_channel = (use_ping_buffer[Chan])
                             ? data_channels_ping[Chan]
                             : data_channels[Chan];

    int forwardDirection = (chParam->revDirCh_u8 == 0);

	float offset_temp123 = 0;

    if(mImg->spacingOverride == 0)
    {
    	offset_temp123 = (((ChannelParamObject[Chan].offsetCh_float) + (light_para.locationOffset_float)) *25.4)/ChannelParamObject[Chan].LEDspacingCh_float;
    }
    else
    {
    	offset_temp123 = (((ChannelParamObject[Chan].offsetCh_float) + (light_para.locationOffset_float))); // In terms of LED
    }

	offset_temp123 *= ChannelParamObject[Chan].scaleCh_float;

    // --- 6. Precompute constants for brightness effect if using sine modulation ---
    // Only apply the sine modulation if the brightnessWavelength is at least as large as one LED spacing.
    bool useSineModulation = (mImg->brightnessWavelength >= LEDspacingInches);
    // Precompute phase factors and time adjustments
    float wavePhaseFactor;
    float timePhase;
    float AmplitudeAddition = 0.0f;

	// Precompute values outside the loop
	{
		float scale = fabsf(ChannelParamObject[Chan].scaleCh_float);
		float wavelength = ((mImg->brightnessWavelength * scale)/LEDspacingInches);
		wavePhaseFactor = (wavelength != 0) ? (2.0f * PI / wavelength) : 0.0000001f;

//		float speed_temp = mImg->brightnessSpeed;
		float speed_temp = -mImg->brightnessSpeed;

		if ((Chan == 0 && light_para.revDirCh1_u8 == 1) ||
			(Chan == 1 && light_para.revDirCh2_u8 == 1) ||
			(Chan == 2 && light_para.revDirCh3_u8 == 1) ||
			(Chan == 3 && light_para.revDirCh4_u8 == 1)) {
			speed_temp = -speed_temp;
		}

		timePhase = speed_temp * currentTime;
		AmplitudeAddition = mImg->brightnessAmplitude;
#ifdef ENABLE_PRINT_MSG
//        if(Chan == 0)
//        printf("wave = %f offset_temp = %f, wavelength = %f, wavePhaseFactor = %f, speed_temp = %f, timePhase = %f, AmplitudeAddition =%f \n",waves1_start[Chan][j].wavelength, offset_temp, wavelength, wavePhaseFactor[j], speed_temp, timePhase[j], AmplitudeAddition);
#endif
	}

	float invAmplitudeAddition = (AmplitudeAddition != 0) ? (1.0f / AmplitudeAddition) : 1.0f;

	// Precompute scale factor once
	float scale_factor = fabsf(ChannelParamObject[Chan].scaleCh_float);


    // --- 7. Loop over each LED in the channel ---
    for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
    {
    	int pos = i+1+offset_temp123;
        // --- 7a. Compute the effective LED index ---
        int effectiveIndex = i;
        if (effectiveIndex < 0 || effectiveIndex >= EXAMPLE_LED_NUMBERS) {
#ifdef ENABLE_PRINT_MSG
            printf("ERROR: effectiveIndex out of bounds for i = %d: %d\n", i, effectiveIndex);
#endif
            continue;
        }

        // --- 7b. Compute the absolute position (in inches) along the LED strip ---
        float ledPos = offset + (effectiveIndex * scaleSpacing);
        // Wrap ledPos within [0, totalLength) using fmodf for speed.
        ledPos = fmodf(ledPos, totalLength);

        // --- 7c. Determine which color segment the current position falls into ---
        if (mImg->numColors <= 0) {
#ifdef ENABLE_PRINT_MSG
            printf("ERROR: No color segments defined for channel %d\n", Chan);
#endif
            continue;
        }
        // Round ledPos to 3 decimal places.
        ledPos = roundf(ledPos * 1000.0f) / 1000.0f;
        float cumulative = 0.0f;
        int segIndex = 0;
        for (int j = 0; j < mImg->numColors; j++) {
            float nextCumulative = cumulative + mImg->colors[j].lengthInches;
            if (ledPos >= cumulative && ledPos < nextCumulative) {
                segIndex = j;
                break;
            }
            cumulative = nextCumulative;
        }
        // Retrieve HSV color from the segment.
        float hue = mImg->colors[segIndex].hue;
        float sat = mImg->colors[segIndex].saturation;
        float val = mImg->colors[segIndex].brightness;

        // --- 7d. Check if we are only filling ramp data ---
        if (fill_data == 1) {
            rampData[Chan].hue_start[i] = hue;
            rampData[Chan].sat_start[i] = sat;
            rampData[Chan].val_start[i] = val;
            continue;  // Skip LED output
        } else if (fill_data == 2) {
            rampData[Chan].hue_end[i] = hue;
            rampData[Chan].sat_end[i] = sat;
            rampData[Chan].val_end[i] = val;
            continue;  // Skip LED output
        }

        // --- 7e. Apply brightness modulation via a sine wave (if enabled) ---

		float combinedSine = 0.0f;

        if (useSineModulation)
        {
			// Calculate the combined sine wave value for the current LED position
			{
				if (mImg->brightnessAmplitude != 0.0f) {
					int groupIndex = pos % (int)((mImg->brightnessWavelength * scale_factor)/LEDspacingInches);
					float positionPhase = groupIndex * wavePhaseFactor;


					if (!isnan(positionPhase) && !isinf(positionPhase) &&
						!isnan(timePhase) && !isinf(timePhase)) {
						float position_time = positionPhase + timePhase;
						combinedSine = (mImg->brightnessAmplitude) * fast_sinf(position_time);
//						brightness_mod = 1.0f - (((fast_sinf(sineFactor * i) + 1.0f) * 0.5f) * mImg->brightnessAmplitude);
					}
				}
			}

			combinedSine *= invAmplitudeAddition;
			if ((!isnan(combinedSine)) && (!isinf(combinedSine)))
			{
//				combinedSine = fmaxf(fminf(combinedSine, 1.0f), -1.0f);
				combinedSine = (combinedSine >  1.0f) ?  1.0f
				              : (combinedSine < -1.0f) ? -1.0f
				              : combinedSine;
//				combinedSine = fmaxf(fminf(combinedSine, mImg->brightnessAmplitude), -mImg->brightnessAmplitude);
			}

			combinedSine = ((combinedSine + 1.0f) * 0.5f);

			combinedSine = (1- mImg->brightnessAmplitude) + (combinedSine * mImg->brightnessAmplitude);
			val *= combinedSine;
        }

        // --- 7f. Convert HSV to RGB (using your helper function) ---
        uint16_t red16 = 0, green16 = 0, blue16 = 0;
        // For now, use transitionType==0 path (both calls are identical here).
        hsv_to_rgb_16bit(hue, sat, 100, &red16, &green16, &blue16);
        // Scale the final RGB values with the brightness factor.
        restrict_and_scale_RGB(&red16, &green16, &blue16, val);

        // --- 7g. Determine the final LED index based on drawing direction ---
        int led_index;
        if (forwardDirection) {
            led_index = i;
        } else {
            if (i < chParam->SetLEDstripal_u16) {
                led_index = (chParam->SetLEDstripal_u16 - 1) - i;
            } else {
                led_index = (EXAMPLE_LED_NUMBERS - 1) - (i - chParam->SetLEDstripal_u16);
            }
        }
        if (led_index < 0 || led_index >= EXAMPLE_LED_NUMBERS) {
#ifdef ENABLE_PRINT_MSG
            printf("ERROR: Computed LED index %d is out of bounds (expected 0 to %d) for Chan %d\n",
                   led_index, EXAMPLE_LED_NUMBERS - 1, Chan);
#endif
            continue;
        }

        // --- 7h. Write the computed color into the internal LED buffer ---
        set_led_color((uint8_t)(Chan + 1), (uint16_t)(led_index + 1), red16, green16, blue16);
    }

    // --- 8. DMA Copy to LED data buffer (if not filling ramp data) ---
    if (fill_data == 0) {
        switch (Chan) {
            case 0:
                memcpy(&data_channel[0], &data_channels1_1[0], EXAMPLE_LED_NUMBERS * 3 * 2);
                break;
            case 1:
                memcpy(&data_channel[0], &data_channels1_2[0], EXAMPLE_LED_NUMBERS * 3 * 2);
                break;
            case 2:
                memcpy(&data_channel[0], &data_channels1_3[0], EXAMPLE_LED_NUMBERS * 3 * 2);
                break;
            case 3:
                memcpy(&data_channel[0], &data_channels1_4[0], EXAMPLE_LED_NUMBERS * 3 * 2);
                break;
            default:
#ifdef ENABLE_PRINT_MSG
    	        printf("ERROR: Invalid channel in DMA copy: %d\n", Chan);
#endif
                break;
        }
    }
}

static inline int jget_int_def(const cJSON *obj, const char *key, int defv) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(n) ? (int)n->valuedouble : defv;
}
static inline float jget_float_def(const cJSON *obj, const char *key, float defv) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(n) ? (float)n->valuedouble : defv;
}
static inline int jget_bool_def(const cJSON *obj, const char *key, int defv) {
    const cJSON *n = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(n)) return cJSON_IsTrue(n) ? 1 : 0;
    if (cJSON_IsNumber(n)) return ((int)n->valuedouble) ? 1 : 0;
    return defv;
}

//static int executeRacing(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//	cJSON *json = cJSON_CreateObject();
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	float brightness = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		if (cJSON_IsObject(configItem))
//		{
//			TurnFlagsOff(channel);
//
//			RacingState *st = &g_racing_state[channel-1];
//			RacingParams *RacingConfig = &st->params;
//
//			/* Always start from defaults */
//			Racing_DefaultParams(RacingConfig);
//
//			/* If we have a proper object, override defaults with provided values */
//			if (cJSON_IsObject(configItem))
//			{
//			    RacingConfig->enable_collision_avoidance = jget_bool_def (configItem, "enable_collision_avoidance", RacingConfig->enable_collision_avoidance);
//			    RacingConfig->override_pitch_in         = jget_bool_def (configItem, "override_pitch_in",         RacingConfig->override_pitch_in);
//			    RacingConfig->fixed_cars                = jget_int_def  (configItem, "fixed_cars",                RacingConfig->fixed_cars);
//			    RacingConfig->max_cars_cap              = jget_int_def  (configItem, "max_cars_cap",              RacingConfig->max_cars_cap);
//
//			    RacingConfig->min_len_in                = jget_float_def(configItem, "min_len_in",                RacingConfig->min_len_in);
//			    RacingConfig->max_len_in                = jget_float_def(configItem, "max_len_in",                RacingConfig->max_len_in);
//			    RacingConfig->min_start_spacing_in      = jget_float_def(configItem, "min_start_spacing_in",      RacingConfig->min_start_spacing_in);
//
//			    RacingConfig->reentry_gap_in            = jget_float_def(configItem, "reentry_gap_in",            RacingConfig->reentry_gap_in);
//			    RacingConfig->min_speed_in_s            = jget_float_def(configItem, "min_speed_in_s",            RacingConfig->min_speed_in_s);
//			    RacingConfig->max_speed_in_s            = jget_float_def(configItem, "max_speed_in_s",            RacingConfig->max_speed_in_s);
//
//			    RacingConfig->retarget_min_ms           = jget_int_def  (configItem, "retarget_min_ms",           RacingConfig->retarget_min_ms);
//			    RacingConfig->retarget_jitter_ms        = jget_int_def  (configItem, "retarget_jitter_ms",        RacingConfig->retarget_jitter_ms);
//			    RacingConfig->max_accel_in_s2           = jget_float_def(configItem, "max_accel_in_s2",           RacingConfig->max_accel_in_s2);
//
//			    RacingConfig->min_collision_gap_in      = jget_float_def(configItem, "min_collision_gap_in",      RacingConfig->min_collision_gap_in);
//			    RacingConfig->spawn_mode                = jget_int_def  (configItem, "spawn_mode",                RacingConfig->spawn_mode);
//			    RacingConfig->max_dt_s                  = jget_float_def(configItem, "max_dt_s",                  RacingConfig->max_dt_s);
//
//			    cJSON *colorsItem = cJSON_GetObjectItem(configItem, "COLORS");
//			    if (colorsItem && cJSON_IsArray(colorsItem)) {
//			        int count = cJSON_GetArraySize(colorsItem);
//			        if (count > DEF_MAX_CARS_CAP) count = DEF_MAX_CARS_CAP;
//
//			        RacingConfig->num_colors = count;
//
//			        for (int i = 0; i < count; i++) {
//			            cJSON *rgb = cJSON_GetArrayItem(colorsItem, i);
//			            if (cJSON_IsArray(rgb) && cJSON_GetArraySize(rgb) == 3) {
//			                RacingConfig->colors[i][0] = (uint16_t)cJSON_GetArrayItem(rgb, 0)->valuedouble;
//			                RacingConfig->colors[i][1] = (uint16_t)cJSON_GetArrayItem(rgb, 1)->valuedouble;
//			                RacingConfig->colors[i][2] = (uint16_t)cJSON_GetArrayItem(rgb, 2)->valuedouble;
//			            }
//			        }
//			    } else {
//			        RacingConfig->num_colors = 0; // fallback → auto color
//			    }
//			}
//
//#ifdef ENABLE_PRINT_MSG
//			/* Optional: logs (unchanged) */
//			printf("enable_collision_avoidance=%d override_pitch_in=%d fixed_cars=%d max_cars_cap=%d\n",
//			       RacingConfig->enable_collision_avoidance, RacingConfig->override_pitch_in,
//			       RacingConfig->fixed_cars, RacingConfig->max_cars_cap);
//			printf("min_len_in=%f max_len_in=%f min_start_spacing_in=%f\n",
//			       RacingConfig->min_len_in, RacingConfig->max_len_in, RacingConfig->min_start_spacing_in);
//			printf("reentry_gap_in=%f min_speed_in_s=%f max_speed_in_s=%f\n",
//			       RacingConfig->reentry_gap_in, RacingConfig->min_speed_in_s, RacingConfig->max_speed_in_s);
//			printf("retarget_min_ms=%ld retarget_jitter_ms=%ld max_accel_in_s2=%f\n",
//			       RacingConfig->retarget_min_ms, RacingConfig->retarget_jitter_ms, RacingConfig->max_accel_in_s2);
//			printf("min_collision_gap_in=%f spawn_mode=%d max_dt_s=%f\n",
//			       RacingConfig->min_collision_gap_in, RacingConfig->spawn_mode, RacingConfig->max_dt_s);
//
////#endif
//			if(RacingConfig->num_colors > 0)
//			{
//				for(int idx=0; idx<(RacingConfig->num_colors); idx++)
//				{
//				    printf("R = %d \t", RacingConfig->colors[idx][0]);
//				    printf("G = %d \t", RacingConfig->colors[idx][1]);
//				    printf("B = %d \n", RacingConfig->colors[idx][2]);
//				}
//			}
//
//			printf("num_colors = %d \n", RacingConfig->num_colors);
//
//#endif
//
//
//			// Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//			{
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//			}
//
//			if(functionNullFlag == 1)
//			{
//				brightness = brightness_RunTimeChan[channel-1];
//			}
//			else
//			{
//				brightness_RunTimeChan[channel-1] = brightness;
//			}
//
//			if(brightness != 0)
//			{
//				brightness_factor = (brightness_factor*brightness) * 0.01;;
//			}
//
//			Racing_InitChannel(channel-1);
//
//			RacingStartFlag[channel-1]=1;
//        }
//    }
//    else
//    {
//		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
//		cJSON_Delete(json);
//		return -1;
//	}
//
//	if(functionNullFlag == 1)
//	{
//		functionNullFlag = 0;
//	}
//	cJSON_Delete(json);
//
//	return 1;
//}

//static int executeCustom(AMessage_st* s_Message_Rx, int channel, float *brightness_1, cJSON *configItem)
//{
//	cJSON *json = cJSON_CreateObject();
//	if (json == NULL) {
//		Add_Response_msg("Internal error: failed to create JSON context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON *channelArray = cJSON_AddArrayToObject(json, "CH");
//	if (channelArray == NULL) {
//		cJSON_Delete(json);
//		Add_Response_msg("Internal error: failed to create channel context.", s_Message_Rx, payLoadData);
//		return -1;
//	}
//	cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(channel));
//
//	float brightness = 0;
//
//	// Validate JSON structure
//	if ((channelArray != NULL) &&
//		(channelArray->type == cJSON_Array))
//	{
//		int check_ch_zero = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == (0);
//		if(check_ch_zero == 1)
//		{
//			Add_Response_msg("Invalid channel ", s_Message_Rx, payLoadData);
//			cJSON_Delete(json);
//			return -1;
//		}
//
//		// Get values
//		cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
//		cJSON *configItem1 = cJSON_GetObjectItem(json, "CONFIG");
//		cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");
//
//		if (configItem1 != NULL )
//		{
//			configItem = configItem1;
//		}
//
//		if (brightnessItem != NULL )
//		{
//			brightness = brightnessItem->valuedouble;
//		}
//		else
//		{
//			brightness= *brightness_1;
//		}
//
//		if (brightnessIndexItem && cJSON_IsNumber(brightnessIndexItem) && brightness == 0) {
//		        brightness = fminf(fmaxf(brightnessIndexItem->valuedouble * 10, 0), 100);
//		    }
//
//		if (cJSON_IsObject(configItem))
//		{
//
//			// Extract colorSelections array
//			//cJSON *colorSelections = cJSON_GetObjectItemCaseSensitive(json, "COLORSELECTIONS");
//			cJSON *colorSelections = cJSON_GetObjectItemCaseSensitive(configItem, "colorSelections");
//			if (!cJSON_IsArray(colorSelections))
//			{
//				Add_Response_msg("colorSelections is not an array.", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//
//			// Count the number of elements in colorSelections array
//			int num_colorSelections = cJSON_GetArraySize(colorSelections);
//			if(num_colorSelections == 0)
//			{
//				Add_Response_msg("Colour Selection Empty array received.", s_Message_Rx, payLoadData);
//				cJSON_Delete(json);
//				return -1;
//			}
//			TurnFlagsOff(channel);
//
//			 // Ensure number of colors does not exceed MAX_COLORS
//			if (num_colorSelections > MAX_COLORS) num_colorSelections = MAX_COLORS;
//
//			// Initialize customImage structure for the channel
//			int temp_ramp = 0;
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
////				customImage *imgConfig = &ImageConfig_end[channel-1];
//				temp_ramp = 1;
//			}
//			else
//			{
////				customImage *imgConfig = &ImageConfig_start[channel-1];
//			}
//
//			uint8_t spacingOverrideV = 0;
//			cJSON *spacingOverrideItem = cJSON_GetObjectItem(configItem, "spacingOverride");
//			if (spacingOverrideItem)
//			{
//			   spacingOverrideV = spacingOverrideItem->valueint;
//			}
//
//			customImage *imgConfig = (temp_ramp) ? &ImageConfig_end[channel-1] : &ImageConfig_start[channel-1];
//
//
//			imgConfig->spacingOverride = spacingOverrideV;
//			imgConfig->numColors = num_colorSelections;
//			imgConfig->colorLength = cJSON_GetObjectItem(configItem, "colorLength")->valuedouble;
//			imgConfig->paddingLength = cJSON_GetObjectItem(configItem, "paddingLength")->valuedouble;
//
//			float lengthInch = 0.0f;
//
//			if(spacingOverrideV == 1)	//Override color length if spacingOverrideV = 1
//			{
//			   lengthInch = imgConfig->colorLength*((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4);
//
//			   imgConfig->colorLength = lengthInch;
//
//			   lengthInch = imgConfig->paddingLength*((ChannelParamObject[channel-1].LEDspacingCh_float)/25.4);
//
//			   imgConfig->paddingLength = lengthInch;
//			}
//
//			imgConfig->transitionType = !strcmp(cJSON_GetObjectItem(configItem, "transitionType")->valuestring, "None") ? 0 : 1;
//			imgConfig->mirror = cJSON_GetObjectItem(configItem, "enableMirror")->valueint;
//			imgConfig->mirrorPosition = cJSON_GetObjectItem(configItem, "mirrorPosition")->valuedouble;
//
////			light_para.locationMidpoint_float = imgConfig->mirrorPosition;		//Is this to be done? //Sanjeev
//
//			imgConfig->oscAmplitude = cJSON_GetObjectItem(configItem, "oscAmp")->valuedouble;
//			imgConfig->oscPeriod = cJSON_GetObjectItem(configItem, "oscPeriod")->valuedouble;
//			imgConfig->movingSpeed = cJSON_GetObjectItem(configItem, "movingSpeed")->valuedouble;
//
//			imgConfig->movingSpeed = -1*imgConfig->movingSpeed;
//
//
//			if (imgConfig->spacingOverride == 1) {
//				float pitch_in = (ChannelParamObject[channel-1].LEDspacingCh_float) * (1.0f/25.4f);
//				imgConfig->movingSpeed *= pitch_in; // pixels/sec -> inches/sec
//			}
//
//			ImageSize_forMode[channel-1]= (imgConfig->numColors)*(imgConfig->colorLength+imgConfig->paddingLength);
//
//			if((EXAMPLE_LED_NUMBERS*3) < ImageSize_forMode[channel-1])
//			{
//				ImageSize_forMode[channel-1] = (EXAMPLE_LED_NUMBERS*3);
//			}
//
//			bytes_per_channel[channel-1] = ImageSize_forMode[channel-1];
//
//			// Extract color selections
//			for (int i = 0; i < num_colorSelections; ++i)
//			{
//				cJSON *colorSelection = cJSON_GetArrayItem(colorSelections, i);
//				if (cJSON_IsString(colorSelection))
//				{
//					const char *colorString = cJSON_GetStringValue(colorSelection);
//					sscanf(colorString, "%f , %f , %f", &imgConfig->colorSelections[i].hue, &imgConfig->colorSelections[i].saturation, &imgConfig->colorSelections[i].brightness);
//				}
//			}
//
//			// Extract background color
//			cJSON *bgColorArray = cJSON_GetObjectItem(configItem, "bgColor");
//			for (int j = 0; j < 3; j++)
//			{
//				imgConfig->paddingColor.hue = cJSON_GetArrayItem(bgColorArray, 0)->valuedouble;
//				imgConfig->paddingColor.saturation = cJSON_GetArrayItem(bgColorArray, 1)->valuedouble;
//				imgConfig->paddingColor.brightness = cJSON_GetArrayItem(bgColorArray, 2)->valuedouble;
//			}
//
//		    // Calculate brightness factor
//			float brightness_factor = light_para.contrMaxB_float * 0.01;
//			if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
//			{
//				brightness_factor *= (channel == 1) ? light_para.chan1MaxB_float * 0.01 :
//								   (channel == 2) ? light_para.chan2MaxB_float * 0.01 :
//								   (channel == 3) ? light_para.chan3MaxB_float * 0.01 :
//													light_para.chan4MaxB_float * 0.01;
//			}
//
//			if(functionNullFlag == 1)
//			{
//				brightness = brightness_RunTimeChan[channel-1];
//			}
//			else
//			{
//				brightness_RunTimeChan[channel-1] = brightness;
//			}
//
//			if(brightness != 0)
//			{
//				brightness_factor = (brightness_factor*brightness) * 0.01;;
//			}
//
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				//nothing
//			}
//			else
//			{
//				Execute_PrepareDataWithModeSetting(0, channel-1, 1, 0);
//			}
//
//            float movingSpeed_float = 0;
//            float movingSpeed_float1 = 0;
//            float temp_cal = 0;
//
//            movingSpeed_float = imgConfig->movingSpeed;
//
//			if(movingSpeed_float == 0)
//			{
//				ExecuteCustomStartFlag_offset1[channel-1] = 1;
//				One_LED_time[channel-1] = 0;
//			}
//			else if(movingSpeed_float < 0)
//			{//Negative direction
//				movingSpeed_float1 = -movingSpeed_float;
//				ChannelParamObject[channel-1].speedrevDirCh_u8 = 1;
//				temp_cal = (1*1000)/ (movingSpeed_float1);			//One inch in milisecond
//				One_LED_time[channel-1] = temp_cal;
//			}
//			else
//			{//Positve direction
//				ChannelParamObject[channel-1].speedrevDirCh_u8 = 0;
//
//				temp_cal = (1*1000)/ (movingSpeed_float);			//One inch in milisecond
//				One_LED_time[channel-1] = temp_cal;
//			}
//
//			if(( imgConfig->oscAmplitude != 0 ) && ( imgConfig->oscPeriod != 0 ))
//			{
//				oscP_Flag[channel-1] = 1;
//			}
//			else
//			{
//				oscP_Flag[channel-1] = 0;
//			}
//
//			enableMirror_uint8[channel-1] = imgConfig->mirror;
//			temp_cal = ((imgConfig->mirrorPosition)*25.4)/ChannelParamObject[channel-1].LEDspacingCh_float;
//			MirrorLedNum[channel-1] = 1*temp_cal;
//
//			float temp_cal1 = 0;
//
//			if(oscP_Flag[channel-1] != 0)
//			{
//				temp_cal1 = 0;
//
//				temp_cal = (imgConfig->oscAmplitude)/(imgConfig->oscPeriod);
//
//				if(ChannelParamObject[channel-1].speedrevDirCh_u8 == 0)
//				{
//					temp_cal1 = (movingSpeed_float);
//
//					oscOffsetMax[channel-1] = 1*((imgConfig->oscPeriod)*temp_cal);
//					oscOffsetMin[channel-1] = 1*(temp_cal1+((imgConfig->oscPeriod)*temp_cal));
//
//					temp_cal1 = temp_cal1+(temp_cal*2);
//
//					One_LED_time[channel-1] = 1000/(temp_cal*2);
//
//					One_LED_time_back[channel-1] = (1000/temp_cal1);
//
//				}
//				else
//				{
//					movingSpeed_float1 = -movingSpeed_float;
//					temp_cal1 = (movingSpeed_float1);
//
//					oscOffsetMax[channel-1] = 1*(temp_cal1+((imgConfig->oscPeriod)*temp_cal));
//					oscOffsetMin[channel-1] = 1*((imgConfig->oscPeriod)*temp_cal);
//
//					temp_cal1 = temp_cal1+(temp_cal*2);
//
//					One_LED_time[channel-1] = (1000/temp_cal1);
//					One_LED_time_back[channel-1] = 1000/(temp_cal*2);
//				}
//
//				oscOffset[channel-1] = 0;
//				oscOffset_forward[channel-1] = 0;
//				oscOffset_back[channel-1] = 0;
//			}
//
//            if (imgConfig->mirror == 1)
//            {
//                // Mirror effect data arrangement (if necessary)
//            }
//            else
//            {
//                if (imgConfig->oscAmplitude != 0 && imgConfig->oscPeriod != 0)
//                {
//                    uint64_t u64CurrentTime = get_current_time_ms();
//                    oscStart_time[channel-1] = u64CurrentTime;
//                }
//            }
//
////			ExecuteCustomStartFlag[channel-1]=1;
//
//			if( (rampData[channel-1].DwellTimeSceneVal != 0) || (rampData[channel-1].RampTimeSceneVal != 0) )
//			{
//				rampData[channel-1].RampStartTime = get_current_time_ms();
//				ExecuteSceneRampFlag[channel-1] = 1;
//			}
//			else
//			{
//				ExecuteCustomStartFlag[channel-1]=1;
//			}
//        }
//    }
//    else
//    {
//		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
//		cJSON_Delete(json);
//		return -1;
//	}
//
//	if(functionNullFlag == 1)
//	{
//		functionNullFlag = 0;
//	}
//	cJSON_Delete(json);
//
//	return 1;
//}

// Function to generate a random number between min and max (inclusive)
static inline int generate_random(int min, int max) {
    return (esp_random() % (max - min + 1)) + min;
}

static void patternExecuteProc(int Chan, uint64_t u64CurrentTime, int fill_data) {

	uint64_t u64CurrentTime1 = 0;
	u64CurrentTime1 = u64CurrentTime;

	u64CurrentTime=u64CurrentTime * 0.000000001;
	u64CurrentTime=u64CurrentTime*1000000000;

	u64CurrentTime1 = u64CurrentTime1 - u64CurrentTime;

	u64CurrentTime = u64CurrentTime1 & 0xFFFFFFFF;

#ifdef ENABLE_PRINT_MSG
	printf("u64CurrentTime2 = %lld \n", u64CurrentTime);
#endif

	// Initialize color start and end points based on the pattern flag
	Color start, end, result;
	float sat_pattern = 1.0;  // Saturation percentage
	float val_pattern = 1.0;  // Brightness percentage

	// Calculate the brightness factor based on global and channel-specific settings
	float brightness_factor = light_para.contrMaxB_float * 0.01;
	float channelMaxB_factors[] = {
		light_para.chan1MaxB_float * 0.01,
		light_para.chan2MaxB_float * 0.01,
		light_para.chan3MaxB_float * 0.01,
		light_para.chan4MaxB_float * 0.01
	};
	brightness_factor *= channelMaxB_factors[Chan];
	if (brightness_RunTimeChan[Chan] != 0) {
		brightness_factor *= brightness_RunTimeChan[Chan] * 0.01;
	}

    if((fill_data == 0) || (fill_data == 1))
    {
		 float period = periodPattern_float_start[Chan];
		 float remaining_ramp_ms = period - (fmod(u64CurrentTime, period));

		 if (fmod(u64CurrentTime, period * 2) < period) {
			  PatternStartFlag_start[Chan] = 1;
		  } else {
			  PatternStartFlag_start[Chan] = 2;
		  }
		 float output = remaining_ramp_ms / period;

		output = 1 - output;  // Reverse the output for interpolation

#ifdef ENABLE_PRINT_MSG
		printf("output  = %f \n", output);
#endif

		if (PatternStartFlag_start[Chan] == 1) {
			start.hue = NewPattern_u16Hue_start[Chan][0];
			start.saturation = sat_pattern;
			start.brightness = val_pattern * brightness_factor;
			end.hue = NewPattern_u16Hue_start[Chan][1];
			end.saturation = sat_pattern;
			end.brightness = val_pattern * brightness_factor;
		} else {
			start.hue = NewPattern_u16Hue_start[Chan][1];
			start.saturation = sat_pattern;
			start.brightness = val_pattern * brightness_factor;
			end.hue = NewPattern_u16Hue_start[Chan][2];
			end.saturation = sat_pattern;
			end.brightness = val_pattern * brightness_factor;
		}

		// Interpolate color based on the output factor
		interpolate_color(&result, &start, &end, output);
		result.brightness *= 100;
		result.saturation *= 100;

		if(fill_data == 1)
		{
//			for (int i = 0; i < Number_of_LED_int; i++)
			for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
			{
				rampData[Chan].hue_start[i] = result.hue;	//hue;
				rampData[Chan].sat_start[i] = result.saturation;	//sat;
				rampData[Chan].val_start[i] = result.brightness;	//val;

#ifdef ENABLE_PRINT_MSG
				printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_start[i], rampData[channel-1].sat_start[i], rampData[channel-1].val_start[i] );
#endif
			}
		}
    }

    if(fill_data == 2)
    {
		 float period = periodPattern_float_end[Chan];
		 float remaining_ramp_ms = period - (fmod(u64CurrentTime, period));

		 if (fmod(u64CurrentTime, period * 2) < period) {
			  PatternStartFlag_end[Chan] = 1;
		  } else {
			  PatternStartFlag_end[Chan] = 2;
		  }
		 float output = remaining_ramp_ms / period;

		output = 1 - output;  // Reverse the output for interpolation

		if (PatternStartFlag_end[Chan] == 1) {
			start.hue = NewPattern_u16Hue_end[Chan][0];
			start.saturation = sat_pattern;
			start.brightness = val_pattern * brightness_factor;
			end.hue = NewPattern_u16Hue_end[Chan][1];
			end.saturation = sat_pattern;
			end.brightness = val_pattern * brightness_factor;
		} else {
			start.hue = NewPattern_u16Hue_end[Chan][1];
			start.saturation = sat_pattern;
			start.brightness = val_pattern * brightness_factor;
			end.hue = NewPattern_u16Hue_end[Chan][2];
			end.saturation = sat_pattern;
			end.brightness = val_pattern * brightness_factor;
		}

		// Interpolate color based on the output factor
		interpolate_color(&result, &start, &end, output);
		result.brightness *= 100;
		result.saturation *= 100;

		for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
		{
			rampData[Chan].hue_end[i] = result.hue;	//hue;
			rampData[Chan].sat_end[i] = result.saturation;	//sat;
			rampData[Chan].val_end[i] = result.brightness;	//val;

#ifdef ENABLE_PRINT_MSG
			printf("hue = %f, sat = %f, val = %f, \n", rampData[channel-1].hue_end[i], rampData[channel-1].sat_end[i], rampData[channel-1].val_end[i] );
#endif
		}
    }

    if(fill_data == 0)
    {
		// Convert interpolated HSV values to RGB
		uint16_t red=0, green=0, blue=0;
		hsv_to_rgb_16bit(result.hue, result.saturation, 100, &red, &green, &blue);

		// Call the restrict and scale function
		restrict_and_scale_RGB(&red, &green, &blue, result.brightness);

	#ifdef ENABLE_PRINT_MSG
		printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
	#endif

		for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++) {
			set_led_color((uint8_t)(Chan + 1), (uint16_t)pos, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
		}
    }
}

/* ---------- utils ---------- */
static inline uint16_t clamp_u16(uint32_t v) {
    return (uint16_t)(v > 65535u ? 65535u : v);
}

/* ---------- 1) float t in [0..1] – fast linear blend in sRGB ---------- */
static inline uint16_t lerp_u16(float t, uint16_t a, uint16_t b) {
    if (t <= 0.0f)
    {
    	return a;
    }
    if (t >= 1.0f)
    {
    	return b;
    }
    float v = (1.0f - t) * a + t * b;
    if (v < 0.0f)
    {
    	v = 0.0f;
    }
    if (v > 65535.0f)
    {
    	v = 65535.0f;
    }
    return (uint16_t)(v + 0.5f);
}

static inline void lerp_rgb16(float t,
                              uint16_t r0, uint16_t g0, uint16_t b0,
                              uint16_t r1, uint16_t g1, uint16_t b1,
                              uint16_t *r, uint16_t *g, uint16_t *b)
{
    *r = lerp_u16(t, r0, r1);
    *g = lerp_u16(t, g0, g1);
    *b = lerp_u16(t, b0, b1);
}

static void MultiColordecayledProc(int Chan, uint64_t u64CurrentTime, int fill_data)
{
    // Check for valid channel number
    if (Chan < 0 || Chan >= NUMBER_OF_CHANNELS) {
        printf("Invalid channel number: %d\n", Chan);
        return;
    }

    // Calculate the total width of the LED strip in LEDs
    int temp_width = (int)(SPARKLE_DISTANCE_IN_INCH * 25.4 / ChannelParamObject[Chan].LEDspacingCh_float);

    // Fetch the current and previous times to measure elapsed time
    uint64_t u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();  // Get current time in milliseconds

    float random_number = (float)generate_random(500, 1500);  // Generate a random number for dynamic timing

    uint32_t u32StartPosition = 0;
    uint32_t u32NumberOfLedBurst = 0;

    uint16_t red = 0, green = 0, blue = 0;

    uint8_t com_color = 1;
    if(fill_data == 2)
    {
    	com_color = MultiColorSparkleParamObject_end[Chan].numColors;
    }
    else
    {
    	com_color = MultiColorSparkleParamObject_start[Chan].numColors;
    }

    uint8_t u8ColorNum1 = generate_random(1, com_color);
//    printf("u8ColorNum1 1=%d\n", u8ColorNum1);

    float brightness_factor = light_para.contrMaxB_float * 0.01;
    switch (Chan + 1) {
        case 1:
            brightness_factor *= light_para.chan1MaxB_float * 0.01;
            break;
        case 2:
            brightness_factor *= light_para.chan2MaxB_float * 0.01;
            break;
        case 3:
            brightness_factor *= light_para.chan3MaxB_float * 0.01;
            break;
        case 4:
            brightness_factor *= light_para.chan4MaxB_float * 0.01;
            break;
        default:
            printf("Invalid channel number: %d\n", Chan);
            return;
    }

    if (brightness_RunTimeChan[Chan] != 0) {
        brightness_factor = (brightness_factor * brightness_RunTimeChan[Chan]) * 0.01;
    }

//    Color start, start_2, end, result;
    Color16 start, start_2, end, result;

    if((fill_data == 0) || (fill_data == 1))
    {
		// Initialize variables to store the decay parameters
		float intensity = MultiColorSparkleParamObject_start[Chan].Intensity;

	    // Calculate time threshold for LED update
	    float LEDspacingInch_temp = (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
	    if( (LEDspacingInch_temp > 3) && (intensity > 250) )
	    {
	    	LEDspacingInch_temp = 3.0;
	    }
	    float checkTimeElapse = (24 * LEDspacingInch_temp) / 12.0 / 100 * intensity;

#ifdef ENABLE_PRINT_MSG
		if(Chan == 0)
		{
			printf("checkTimeElapse = %f\n", checkTimeElapse);
		}
#endif
		uint64_t u64PreviousTime = MultiColorSparkleParamObject_start[Chan].u64RandomGenTime;  // Last time this function was run
		uint64_t elapsedTime = u64CurrentTime1 - u64PreviousTime;  // Calculate elapsed time

		checkTimeElapse = random_number / checkTimeElapse;  // Adjust time threshold with the random number

#ifdef ENABLE_PRINT_MSG
		if(Chan == 0)
		{
			printf("checkTimeElapse 2 = %f\n", checkTimeElapse);
		}
#endif
		if(u8ColorNum1 == 1)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[2];
		}
		else if(u8ColorNum1 == 2)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[2];
		}
		else if(u8ColorNum1 == 3)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[2];
		}
		else if(u8ColorNum1 == 4)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[2];
		}
		else if(u8ColorNum1 == 5)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[2];
		}
		else if(u8ColorNum1 == 6)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[2];
		}
		else if(u8ColorNum1 == 7)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[2];
		}
		else if(u8ColorNum1 == 8)
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[2];
		}
		else
		{
			start.r = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[0];
			start.g = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[1];
			start.b = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[2];
		}

//		printf("start h = %f, s = %f, v = %f \n", start.hue, start.saturation, start.brightness);

		end.r = MultiColorSparkleParamObject_start[Chan].EndColor_uint16[0];
		end.g = MultiColorSparkleParamObject_start[Chan].EndColor_uint16[1];
		end.b = MultiColorSparkleParamObject_start[Chan].EndColor_uint16[2];

		// If elapsed time is greater than the threshold, update LEDs
		if (elapsedTime >= checkTimeElapse) {
			// Generate random start position and number of LEDs to burst
			u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4));
			if (u32StartPosition < 1) {
				u32StartPosition = 1;
			}

			if (u32StartPosition > SPARKLE_DISTANCE_IN_INCH) {
				u32StartPosition = u32StartPosition - SPARKLE_DISTANCE_IN_INCH;
			}

			int random1 = MultiColorSparkleParamObject_start[Chan].Width / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
			if (random1 == 0) {
				random1 = 1;
			}
			u32NumberOfLedBurst = generate_random(1, random1);
			u32NumberOfLedBurst = u32NumberOfLedBurst * (ChannelParamObject[Chan].scaleCh_float);
			if (u32NumberOfLedBurst == 0) {
				u32NumberOfLedBurst = 1;
			}

			// Reset the timer for this function
			MultiColorSparkleParamObject_start[Chan].u64RandomGenTime = u64CurrentTime;	//get_current_time_ms();
			// Get current time again for accuracy in decay calculations
			u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();

			if(fill_data == 1)
			{
				int ledNumber = 0;
				// Loop through each group
				for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
					// Calculate the starting LED for the burst in the current group
					for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
						// Calculate actual LED number considering wrap-around within the group
						ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

						MultiColorSparkleParamObject_start[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
						MultiColorSparkleParamObject_start[Chan].u8ColorNum[ledNumber-1] = u8ColorNum1;

//						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);

						rampData[Chan].r_start[ledNumber-1] = start.r;
						rampData[Chan].g_start[ledNumber-1] = start.g;
						rampData[Chan].b_start[ledNumber-1] = start.b;
					}
					vTaskDelay(1 / portTICK_PERIOD_MS);
				}
			}

			if(fill_data == 0)
			{
//				hsv_to_rgb_16bit(start.hue, (start.saturation) * 100, 100, &red, &green, &blue);

				red 	= start.r;
				green 	= start.g;
				blue	= start.b;
				// Call the restrict and scale function
//				restrict_and_scale_RGB(&red, &green, &blue, (start.brightness) * 100);

		#ifdef ENABLE_PRINT_MSG
				printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
		#endif
				int ledNumber = 0;
				// Loop through each group
				for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
					// Calculate the starting LED for the burst in the current group
					for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
						// Calculate actual LED number considering wrap-around within the group
						ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

						MultiColorSparkleParamObject_start[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
						MultiColorSparkleParamObject_start[Chan].u8ColorNum[ledNumber-1] = u8ColorNum1;
						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
					}
					vTaskDelay(1 / portTICK_PERIOD_MS);
				}
			}
		} else {
			u64CurrentTime1 = u64CurrentTime;
			// If not updating, loop through all LEDs to apply decay based on time
			for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++) {
				float time = (u64CurrentTime1 - MultiColorSparkleParamObject_start[Chan].u64CurrentTime[pos-1]);  // Calculate time since last update
				uint8_t u8ColorNum1_2 =  MultiColorSparkleParamObject_start[Chan].u8ColorNum[pos-1];
				float rate = 1.0f;
				if(MultiColorSparkleParamObject_start[Chan].Decaytime > 0)
				{
					rate = 1.0f / (MultiColorSparkleParamObject_start[Chan].Decaytime);  // Compute decay rate
				}
				float output = decay_function(time, rate);  // Calculate output based on decay function

				if(u8ColorNum1_2 == 1)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[2];
				}
				else if(u8ColorNum1_2 == 2)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor2_uint16[2];
				}
				else if(u8ColorNum1_2 == 3)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor3_uint16[2];
				}
				else if(u8ColorNum1_2 == 4)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor4_uint16[2];
				}
				else if(u8ColorNum1_2 == 5)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor5_uint16[2];
				}
				else if(u8ColorNum1_2 == 6)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor6_uint16[2];
				}
				else if(u8ColorNum1_2 == 7)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor7_uint16[2];
				}
				else if(u8ColorNum1_2 == 8)
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor8_uint16[2];
				}
				else
				{
					start_2.r = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[0];
					start_2.g = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[1];
					start_2.b = MultiColorSparkleParamObject_start[Chan].MultiColor1_uint16[2];
				}

//				interpolate_color(&result, &start_2, &end, output);  // Interpolate color based on decay
//				result.brightness *= 100;
//				result.saturation *= 100;

//				lerp_rgb16(output, r0, g0, b0, r1, g1, b1, *r, *g, *b)
				lerp_rgb16(output, start_2.r, start_2.g, start_2.b, end.r, end.g, end.b, &result.r, &result.g, &result.b);

				if(fill_data == 1)
				{
					rampData[Chan].r_start[pos-1] = result.r;
					rampData[Chan].g_start[pos-1] = result.g;
					rampData[Chan].b_start[pos-1] = result.b;
				}

				if(fill_data == 0)
				{
//					hsv_to_rgb_16bit(result.hue, result.saturation, 100, &red, &green, &blue);
					red 	= result.r;
					green 	= result.g;
					blue	= result.b;

					// Call the restrict and scale function
//					restrict_and_scale_RGB(&red, &green, &blue, result.brightness);
		#ifdef ENABLE_PRINT_MSG
					printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
		#endif
					// Set the RGB values for the LED
					set_led_color((uint8_t)Chan + 1, (uint16_t)pos, (uint16_t)red, (uint16_t)green, (uint16_t)blue);

					// Yield to other tasks
					if (pos % 50 == 0) {
						vTaskDelay(1 / portTICK_PERIOD_MS);
					}
				}
			}
		}
    }

    if(fill_data == 2)
    {
		// Initialize variables to store the decay parameters
		float intensity = MultiColorSparkleParamObject_end[Chan].Intensity;

		// Calculate time threshold for LED update
	    float LEDspacingInch_temp = (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
	    if( (LEDspacingInch_temp > 3) && (intensity > 250) )
	    {
	    	LEDspacingInch_temp = 3.0;
	    }
		float checkTimeElapse = (24 * LEDspacingInch_temp) / 12.0 / 100 * intensity;

		uint64_t u64PreviousTime = MultiColorSparkleParamObject_end[Chan].u64RandomGenTime;  // Last time this function was run
		uint64_t elapsedTime = u64CurrentTime1 - u64PreviousTime;  // Calculate elapsed time

		checkTimeElapse = random_number / checkTimeElapse;  // Adjust time threshold with the random number

		if(u8ColorNum1 == 1)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[2];
		}
		else if(u8ColorNum1 == 2)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[2];
		}
		else if(u8ColorNum1 == 3)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[2];
		}
		else if(u8ColorNum1 == 4)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[2];
		}
		else if(u8ColorNum1 == 5)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[2];
		}
		else if(u8ColorNum1 == 6)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[2];
		}
		else if(u8ColorNum1 == 7)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[2];
		}
		else if(u8ColorNum1 == 8)
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[2];
		}
		else
		{
			start.r = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[0];
			start.g = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[1];
			start.b = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[2];
		}

		end.r = MultiColorSparkleParamObject_end[Chan].EndColor_uint16[0];
		end.g = MultiColorSparkleParamObject_end[Chan].EndColor_uint16[1];
		end.b = MultiColorSparkleParamObject_end[Chan].EndColor_uint16[2];

		// If elapsed time is greater than the threshold, update LEDs
		if (elapsedTime >= checkTimeElapse) {
			// Generate random start position and number of LEDs to burst
			u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4));
			if (u32StartPosition < 1) {
				u32StartPosition = 1;
			}

			if (u32StartPosition > SPARKLE_DISTANCE_IN_INCH) {
				u32StartPosition = u32StartPosition - SPARKLE_DISTANCE_IN_INCH;
			}

			int random1 = MultiColorSparkleParamObject_end[Chan].Width / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
			if (random1 == 0) {
				random1 = 1;
			}
			u32NumberOfLedBurst = generate_random(1, random1);
			u32NumberOfLedBurst = u32NumberOfLedBurst * (ChannelParamObject[Chan].scaleCh_float);
			if (u32NumberOfLedBurst == 0) {
				u32NumberOfLedBurst = 1;
			}

			// Reset the timer for this function
			MultiColorSparkleParamObject_end[Chan].u64RandomGenTime = u64CurrentTime;	//get_current_time_ms();
			// Get current time again for accuracy in decay calculations
			u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();

			int ledNumber = 0;
			// Loop through each group
			for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
				// Calculate the starting LED for the burst in the current group
				for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
					// Calculate actual LED number considering wrap-around within the group
					ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

					MultiColorSparkleParamObject_end[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
//						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
					MultiColorSparkleParamObject_end[Chan].u8ColorNum[ledNumber-1] = u8ColorNum1;

					rampData[Chan].r_end[ledNumber-1] = start.r;
					rampData[Chan].g_end[ledNumber-1] = start.g;
					rampData[Chan].b_end[ledNumber-1] = start.b;
				}
				vTaskDelay(1 / portTICK_PERIOD_MS);
			}

		} else {
			u64CurrentTime1 = u64CurrentTime;
			// If not updating, loop through all LEDs to apply decay based on time
			for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++) {
				float time = (u64CurrentTime1 - MultiColorSparkleParamObject_end[Chan].u64CurrentTime[pos-1]);  // Calculate time since last update
				uint8_t u8ColorNum1_2 =  MultiColorSparkleParamObject_end[Chan].u8ColorNum[pos-1];
				float rate = 1.0f;
				if(MultiColorSparkleParamObject_end[Chan].Decaytime > 0)
				{
					rate = 1.0f / (MultiColorSparkleParamObject_end[Chan].Decaytime);  // Compute decay rate
				}
				float output = decay_function(time, rate);  // Calculate output based on decay function


				if(u8ColorNum1_2 == 1)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[2];
				}
				else if(u8ColorNum1_2 == 2)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor2_uint16[2];
				}
				else if(u8ColorNum1_2 == 3)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor3_uint16[2];
				}
				else if(u8ColorNum1_2 == 4)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor4_uint16[2];
				}
				else if(u8ColorNum1_2 == 5)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor5_uint16[2];
				}
				else if(u8ColorNum1_2 == 6)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor6_uint16[2];
				}
				else if(u8ColorNum1_2 == 7)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor7_uint16[2];
				}
				else if(u8ColorNum1_2 == 8)
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor8_uint16[2];
				}
				else
				{
					start_2.r = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[0];
					start_2.g = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[1];
					start_2.b = MultiColorSparkleParamObject_end[Chan].MultiColor1_uint16[2];
				}

				lerp_rgb16(output, start_2.r, start_2.g, start_2.b, end.r, end.g, end.b, &result.r, &result.g, &result.b);

				rampData[Chan].r_end[pos-1] = result.r;
				rampData[Chan].r_end[pos-1] = result.g;
				rampData[Chan].r_end[pos-1] = result.b;
			}
		}
	}

}

static void decayledProc(int Chan, uint64_t u64CurrentTime, int fill_data)
{
    // Check for valid channel number
    if (Chan < 0 || Chan >= NUMBER_OF_CHANNELS) {
        printf("Invalid channel number: %d\n", Chan);
        return;
    }

    // Calculate the total width of the LED strip in LEDs
    int temp_width = (int)(SPARKLE_DISTANCE_IN_INCH * 25.4 / ChannelParamObject[Chan].LEDspacingCh_float);

    // Fetch the current and previous times to measure elapsed time
    uint64_t u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();  // Get current time in milliseconds

    float random_number = (float)generate_random(500, 1500);  // Generate a random number for dynamic timing

    uint32_t u32StartPosition = 0;
    uint32_t u32NumberOfLedBurst = 0;

    uint16_t red = 0, green = 0, blue = 0;

    float brightness_factor = light_para.contrMaxB_float * 0.01;
    switch (Chan + 1) {
        case 1:
            brightness_factor *= light_para.chan1MaxB_float * 0.01;
            break;
        case 2:
            brightness_factor *= light_para.chan2MaxB_float * 0.01;
            break;
        case 3:
            brightness_factor *= light_para.chan3MaxB_float * 0.01;
            break;
        case 4:
            brightness_factor *= light_para.chan4MaxB_float * 0.01;
            break;
        default:
            printf("Invalid channel number: %d\n", Chan);
            return;
    }

    if (brightness_RunTimeChan[Chan] != 0) {
        brightness_factor = (brightness_factor * brightness_RunTimeChan[Chan]) * 0.01;
    }

    Color start, end, result;

    if((fill_data == 0) || (fill_data == 1))
    {
		// Initialize variables to store the decay parameters
		float intensity = SparkleParamObject_start[Chan].Intensity;

	    // Calculate time threshold for LED update
	    float LEDspacingInch_temp = (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
	    if( (LEDspacingInch_temp > 3) && (intensity > 250) )
	    {
	    	LEDspacingInch_temp = 3.0;
	    }
	    float checkTimeElapse = (24 * LEDspacingInch_temp) / 12.0 / 100 * intensity;

#ifdef ENABLE_PRINT_MSG
		if(Chan == 0)
		{
			printf("checkTimeElapse = %f\n", checkTimeElapse);
		}
#endif
		uint64_t u64PreviousTime = SparkleParamObject_start[Chan].u64RandomGenTime;  // Last time this function was run
		uint64_t elapsedTime = u64CurrentTime1 - u64PreviousTime;  // Calculate elapsed time

		checkTimeElapse = random_number / checkTimeElapse;  // Adjust time threshold with the random number

#ifdef ENABLE_PRINT_MSG
		if(Chan == 0)
		{
			printf("checkTimeElapse 2 = %f\n", checkTimeElapse);
		}
#endif
		start.hue = SparkleParamObject_start[Chan].StartColor_float[0];
		start.saturation = SparkleParamObject_start[Chan].StartColor_float[1] * 0.01;
		start.brightness = ((SparkleParamObject_start[Chan].StartColor_float[2]) * brightness_factor) * 0.01;
		end.hue = SparkleParamObject_start[Chan].EndColor_float[0];
		end.saturation = SparkleParamObject_start[Chan].EndColor_float[1] * 0.01;
		end.brightness = ((SparkleParamObject_start[Chan].EndColor_float[2]) * brightness_factor) * 0.01;

		// If elapsed time is greater than the threshold, update LEDs
		if (elapsedTime >= checkTimeElapse) {
			// Generate random start position and number of LEDs to burst
			u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4));
			if (u32StartPosition < 1) {
				u32StartPosition = 1;
			}

			if (u32StartPosition > SPARKLE_DISTANCE_IN_INCH) {
				u32StartPosition = u32StartPosition - SPARKLE_DISTANCE_IN_INCH;
			}

			int random1 = SparkleParamObject_start[Chan].Width / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
			if (random1 == 0) {
				random1 = 1;
			}
			u32NumberOfLedBurst = generate_random(1, random1);
			u32NumberOfLedBurst = u32NumberOfLedBurst * (ChannelParamObject[Chan].scaleCh_float);
			if (u32NumberOfLedBurst == 0) {
				u32NumberOfLedBurst = 1;
			}

			// Reset the timer for this function
			SparkleParamObject_start[Chan].u64RandomGenTime = u64CurrentTime;	//get_current_time_ms();
			// Get current time again for accuracy in decay calculations
			u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();

			if(fill_data == 1)
			{
				int ledNumber = 0;
				// Loop through each group
				for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
					// Calculate the starting LED for the burst in the current group
					for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
						// Calculate actual LED number considering wrap-around within the group
						ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

						SparkleParamObject_start[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
//						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);

						rampData[Chan].hue_start[ledNumber-1] = start.hue;
						rampData[Chan].sat_start[ledNumber-1] = start.saturation;
						rampData[Chan].val_start[ledNumber-1] = start.brightness;
					}
					vTaskDelay(1 / portTICK_PERIOD_MS);
				}
			}

			if(fill_data == 0)
			{
				hsv_to_rgb_16bit(start.hue, (start.saturation) * 100, 100, &red, &green, &blue);

				// Call the restrict and scale function
				restrict_and_scale_RGB(&red, &green, &blue, (start.brightness) * 100);
		#ifdef ENABLE_PRINT_MSG
				printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
		#endif
				int ledNumber = 0;
				// Loop through each group
				for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
					// Calculate the starting LED for the burst in the current group
					for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
						// Calculate actual LED number considering wrap-around within the group
						ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

						SparkleParamObject_start[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
					}
					vTaskDelay(1 / portTICK_PERIOD_MS);
				}
			}
		} else {
			u64CurrentTime1 = u64CurrentTime;
			// If not updating, loop through all LEDs to apply decay based on time
			for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++) {
				float time = (u64CurrentTime1 - SparkleParamObject_start[Chan].u64CurrentTime[pos-1]);  // Calculate time since last update
				float rate = 1.0f;
				if(SparkleParamObject_start[Chan].Decaytime > 0)
				{
					rate = 1.0f / (SparkleParamObject_start[Chan].Decaytime);  // Compute decay rate
				}
				float output = decay_function(time, rate);  // Calculate output based on decay function

				interpolate_color(&result, &start, &end, output);  // Interpolate color based on decay
				result.brightness *= 100;
				result.saturation *= 100;

				if(fill_data == 1)
				{
					rampData[Chan].hue_start[pos-1] = result.hue;
					rampData[Chan].sat_start[pos-1] = result.saturation;
					rampData[Chan].val_start[pos-1] = result.brightness;
				}

				if(fill_data == 0)
				{
					hsv_to_rgb_16bit(result.hue, result.saturation, 100, &red, &green, &blue);

					// Call the restrict and scale function
					restrict_and_scale_RGB(&red, &green, &blue, result.brightness);
		#ifdef ENABLE_PRINT_MSG
					printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
		#endif
					// Set the RGB values for the LED
					set_led_color((uint8_t)Chan + 1, (uint16_t)pos, (uint16_t)red, (uint16_t)green, (uint16_t)blue);

					// Yield to other tasks
					if (pos % 50 == 0) {
						vTaskDelay(1 / portTICK_PERIOD_MS);
					}
				}
			}
		}
    }

    if(fill_data == 2)
    {
		// Initialize variables to store the decay parameters
		float intensity = SparkleParamObject_end[Chan].Intensity;

		// Calculate time threshold for LED update
	    float LEDspacingInch_temp = (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
	    if( (LEDspacingInch_temp > 3) && (intensity > 250) )
	    {
	    	LEDspacingInch_temp = 3.0;
	    }
		float checkTimeElapse = (24 * LEDspacingInch_temp) / 12.0 / 100 * intensity;

		uint64_t u64PreviousTime = SparkleParamObject_end[Chan].u64RandomGenTime;  // Last time this function was run
		uint64_t elapsedTime = u64CurrentTime1 - u64PreviousTime;  // Calculate elapsed time

		checkTimeElapse = random_number / checkTimeElapse;  // Adjust time threshold with the random number

		start.hue = SparkleParamObject_end[Chan].StartColor_float[0];
		start.saturation = SparkleParamObject_end[Chan].StartColor_float[1] * 0.01;
		start.brightness = ((SparkleParamObject_end[Chan].StartColor_float[2]) * brightness_factor) * 0.01;
		end.hue = SparkleParamObject_end[Chan].EndColor_float[0];
		end.saturation = SparkleParamObject_end[Chan].EndColor_float[1] * 0.01;
		end.brightness = ((SparkleParamObject_end[Chan].EndColor_float[2]) * brightness_factor) * 0.01;

		// If elapsed time is greater than the threshold, update LEDs
		if (elapsedTime >= checkTimeElapse) {
			// Generate random start position and number of LEDs to burst
			u32StartPosition = generate_random(0, SPARKLE_DISTANCE_IN_INCH / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4));
			if (u32StartPosition < 1) {
				u32StartPosition = 1;
			}

			if (u32StartPosition > SPARKLE_DISTANCE_IN_INCH) {
				u32StartPosition = u32StartPosition - SPARKLE_DISTANCE_IN_INCH;
			}

			int random1 = SparkleParamObject_end[Chan].Width / (ChannelParamObject[Chan].LEDspacingCh_float / 25.4);
			if (random1 == 0) {
				random1 = 1;
			}
			u32NumberOfLedBurst = generate_random(1, random1);
			u32NumberOfLedBurst = u32NumberOfLedBurst * (ChannelParamObject[Chan].scaleCh_float);
			if (u32NumberOfLedBurst == 0) {
				u32NumberOfLedBurst = 1;
			}

			// Reset the timer for this function
			SparkleParamObject_end[Chan].u64RandomGenTime = u64CurrentTime;	//get_current_time_ms();
			// Get current time again for accuracy in decay calculations
			u64CurrentTime1 = u64CurrentTime;	//get_current_time_ms();

			int ledNumber = 0;
			// Loop through each group
			for (int group = 0; group < EXAMPLE_LED_NUMBERS / temp_width; ++group) {
				// Calculate the starting LED for the burst in the current group
				for (int burstIndex = 0; burstIndex < u32NumberOfLedBurst; ++burstIndex) {
					// Calculate actual LED number considering wrap-around within the group
					ledNumber = 1 + group * temp_width + (u32StartPosition + burstIndex - 1) % temp_width;

					SparkleParamObject_end[Chan].u64CurrentTime[ledNumber-1] = u64CurrentTime1;
//						set_led_color((uint8_t)(Chan + 1), (uint16_t)ledNumber, (uint16_t)red, (uint16_t)green, (uint16_t)blue);

					rampData[Chan].hue_end[ledNumber-1] = start.hue;
					rampData[Chan].sat_end[ledNumber-1] = start.saturation;
					rampData[Chan].val_end[ledNumber-1] = start.brightness;
				}
				vTaskDelay(1 / portTICK_PERIOD_MS);
			}

		} else {
			u64CurrentTime1 = u64CurrentTime;
			// If not updating, loop through all LEDs to apply decay based on time
			for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++) {
				float time = (u64CurrentTime1 - SparkleParamObject_end[Chan].u64CurrentTime[pos-1]);  // Calculate time since last update
				
				float rate = 1.0f;
				if(SparkleParamObject_end[Chan].Decaytime > 0)
				{
					rate = 1.0f / (SparkleParamObject_end[Chan].Decaytime);  // Compute decay rate
				}
				float output = decay_function(time, rate);  // Calculate output based on decay function

				interpolate_color(&result, &start, &end, output);  // Interpolate color based on decay
				result.brightness *= 100;
				result.saturation *= 100;

				rampData[Chan].hue_end[pos-1] = result.hue;
				rampData[Chan].sat_end[pos-1] = result.saturation;
				rampData[Chan].val_end[pos-1] = result.brightness;
			}
		}
	}
}

//====================  ==================
static float Moving_Tap_Offset(int Chan, uint64_t u64CurrentTime, int fill_data) {
    float offset_mov = 0;
    float oscAmplitude, oscPeriod, movingSpeed;
//    float timeInSeconds = u64CurrentTime / 1000.0; // Convert time to seconds
//    float timeInSeconds = (float)(u64CurrentTime % 100000000) / 1000.0f;
    float timeInSeconds = (float)(u64CurrentTime % 100000000) * 0.001f;
    float oscillationPosition, movingPosition;
    float Mod_Val_Inch_11 = 0;

//    customImage *imgConfig = &ImageConfig_start[Chan];

	int temp_ramp = 0;
	if(fill_data == 2)
	{
//				customImage *imgConfig = &ImageConfig_end[Chan];
		temp_ramp = 1;
	}
	else
	{
//				customImage *imgConfig = &ImageConfig_start[Chan];
	}

	customImage *imgConfig = (temp_ramp) ? &ImageConfig_end[Chan] : &ImageConfig_start[Chan];

    // Retrieve the parameters
    oscAmplitude = imgConfig->oscAmplitude; // Amplitude in inches
    oscPeriod = imgConfig->oscPeriod;       // Period in seconds
    movingSpeed = imgConfig->movingSpeed;   // Speed in inches/second

    // Calculate the sinusoidal oscillation position
	if(oscPeriod!=0.0f)
	{
		float angularFrequency = PIX2 / oscPeriod; // Angular frequency in radians/second
		oscillationPosition = oscAmplitude * sinf(angularFrequency * timeInSeconds);
	}
	else
	{
		oscillationPosition=0;
	}

    // Calculate the moving position based on the moving speed
    movingPosition = movingSpeed * timeInSeconds;

    // Combine both motions
    float combinedPosition = oscillationPosition + movingPosition;

    offset_mov = combinedPosition;

    Mod_Val_Inch_11 = (ImageSize_forMode[Chan])/(ChannelParamObject[Chan].scaleCh_float);

	offset_mov=fmod(offset_mov, Mod_Val_Inch_11);

    return offset_mov;
}

static float Marquee_Moving_Tap_Offset(int Chan, uint64_t u64CurrentTime, int fill_data) {
    float offset_mov = 0;
    float oscAmplitude, oscPeriod, movingSpeed;
//    float timeInSeconds = u64CurrentTime / 1000.0; // Convert time to seconds
//    float timeInSeconds = (float)(u64CurrentTime % 100000000) / 1000.0f;
    float timeInSeconds = (float)(u64CurrentTime % 100000000) * 0.001f;
    float oscillationPosition, movingPosition;
    float Mod_Val_Inch_11 = 0;

	int temp_ramp = 0;
	if(fill_data == 2)
	{
		temp_ramp = 1;
	}
	else
	{

	}

	marqueeImage_t *mImg = (temp_ramp)
		                          ? &marqueeImage_end[Chan]
		                          : &marqueeImage_start[Chan];

    // Retrieve the parameters
    oscAmplitude = mImg->oscAmp; // Amplitude in inches
    oscPeriod = mImg->oscPeriod;       // Period in seconds
    movingSpeed = mImg->movingSpeed;   // Speed in inches/second

    // Calculate the sinusoidal oscillation position
	if(oscPeriod!=0.0f)
	{
		float angularFrequency = PIX2 / oscPeriod; // Angular frequency in radians/second
		oscillationPosition = oscAmplitude * sinf(angularFrequency * timeInSeconds);
	}
	else
	{
		oscillationPosition=0;
	}

    // Calculate the moving position based on the moving speed
    movingPosition = movingSpeed * timeInSeconds;

    // Combine both motions
    float combinedPosition = oscillationPosition + movingPosition;

    offset_mov = combinedPosition;

    Mod_Val_Inch_11 = (ImageSize_forMode[Chan])/(ChannelParamObject[Chan].scaleCh_float);

	offset_mov=fmod(offset_mov, Mod_Val_Inch_11);

    return offset_mov;
}

void init_data_channels()
{
	ESP_LOGI(TAG,"\r\n Initialise of LED strip channels ");
#ifdef ENABLE_PRINT_MSG
	printf("\n Initialise of LED strip channels ");
#endif

//	int ModeSetting = 0;
//	ModeSetting = sAddressableStipArr[IC_Type_Var].u8ModeSetting;
	int offset = 0;
    for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
    {
        for (int j = 0; j < (EXAMPLE_LED_NUMBERS*2); j++)
        {
        	if((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1)
        	{
        		switch(i)
				{
				case 0:
					data_channels1_1[j * 3] = default_data_16[0];
					data_channels1_1[j * 3 + 1] = default_data_16[1];
					data_channels1_1[j * 3 + 2] = default_data_16[2];
					break;
				case 1:
					data_channels1_2[j * 3] = default_data_16[0];
					data_channels1_2[j * 3 + 1] = default_data_16[1];
					data_channels1_2[j * 3 + 2] = default_data_16[2];
					break;
				case 2:
					data_channels1_3[j * 3] = default_data_16[0];
					data_channels1_3[j * 3 + 1] = default_data_16[1];
					data_channels1_3[j * 3 + 2] = default_data_16[2];
					break;
				case 3:
					data_channels1_4[j * 3] = default_data_16[0];
					data_channels1_4[j * 3 + 1] = default_data_16[1];
					data_channels1_4[j * 3 + 2] = default_data_16[2];
					break;
				}
        	}
        }

		offset = 0;
		PrepareDataWithModeSetting(offset, i, 0);
    }
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  		= (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}

	strcpy((char*)newpointer, response);
	s_Message_Tx_new.payload_p8 	=  newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
}

static void queue_sql_response(QueueHandle_t queue, char *buffer, size_t buffer_size, cJSON *response)
{
	if (queue == NULL || response == NULL) {
		return;
	}
	memset(buffer, 0, buffer_size);
	cJSON_PrintPreallocated(response, buffer, buffer_size, false);
	xQueueSend(queue, buffer, QUE_DELAY);
}

static void log_playlist_memory_usage(const char *phase)
{
    size_t parsed_blob_total = 0;
    for (size_t i = 0; i < command_table_size; ++i) {
        parsed_blob_total += command_table[i].parsed_exec_blob_size;
    }
    size_t dynamic_step_bytes = 0;
    for (size_t i = 0; i < playlist_sequence_cache_count; ++i) {
        dynamic_step_bytes += (size_t)playlist_sequence_cache[i].sequence.capacity * sizeof(PlaylistStep);
    }

    const size_t cmd_static = command_table_capacity * sizeof(CommandEntry);
    const size_t playlist_static = playlist_record_capacity * sizeof(PlaylistEntryRecord);
    const size_t slot_static = sizeof(playlist_slots);
    const size_t virtual_static = sizeof(virtual_groups);
    const size_t total_static = cmd_static + playlist_static + slot_static + virtual_static;

    size_t spiram_free = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    size_t spiram_largest = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    size_t internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

    ESP_LOGI(TAG,
             "PlaylistMem[%s]: static_psram=%uB cmd=%uB playlist=%uB slots=%uB virtual=%uB parsed_blobs=%uB dynamic_steps=%uB spiram_free=%uB spiram_largest=%uB internal_free=%uB",
             (phase != NULL) ? phase : "unknown",
             (unsigned)total_static,
             (unsigned)cmd_static,
             (unsigned)playlist_static,
             (unsigned)slot_static,
             (unsigned)virtual_static,
             (unsigned)parsed_blob_total,
             (unsigned)dynamic_step_bytes,
             (unsigned)spiram_free,
             (unsigned)spiram_largest,
             (unsigned)internal_free);
}

static void free_command_entry(CommandEntry *entry)
{
    if (entry == NULL) {
        return;
    }
    if (entry->parsed_exec_blob != NULL) {
        heap_caps_free(entry->parsed_exec_blob);
        entry->parsed_exec_blob = NULL;
    }
    entry->parsed_exec_blob_size = 0;
    entry->parsed_exec_kind = 0;
    entry->nested_playlist_id = 0;
}

static void clear_command_table(void)
{
    clear_playlist_sequence_cache();
    if (command_table != NULL) {
        for (size_t i = 0; i < command_table_size; ++i) {
            free_command_entry(&command_table[i]);
        }
        heap_caps_free(command_table);
        command_table = NULL;
    }
    command_table_size = 0;
    command_table_capacity = 0;
    rebuild_command_index_table();
}

static void clear_virtual_groups(void)
{
    memset(virtual_groups, 0, sizeof(virtual_groups));
    virtual_group_count = 0;
}

static void clear_playlist_records(void)
{
    clear_playlist_sequence_cache();
    if (playlist_records != NULL) {
        heap_caps_free(playlist_records);
        playlist_records = NULL;
    }
    playlist_record_count = 0;
    playlist_record_capacity = 0;
}

static void load_default_virtual_groups(void)
{
    clear_virtual_groups();
    size_t default_count = sizeof(default_virtual_group_defs) / sizeof(default_virtual_group_defs[0]);
    for (size_t i = 0; i < default_count && virtual_group_count < MAX_VIRTUAL_GROUPS; ++i) {
        virtual_groups[virtual_group_count].id = default_virtual_group_defs[i].id;
        strncpy(virtual_groups[virtual_group_count].name, default_virtual_group_defs[i].name, sizeof(virtual_groups[virtual_group_count].name) - 1);
        virtual_groups[virtual_group_count].name[sizeof(virtual_groups[virtual_group_count].name) - 1] = '\\0';
        virtual_groups[virtual_group_count].channel_mask = default_virtual_group_defs[i].channel_mask;
        ++virtual_group_count;
    }
}

static const VirtualGroup *find_virtual_group_by_id(int id)
{
    for (size_t i = 0; i < virtual_group_count; ++i) {
        if (virtual_groups[i].id == id) {
            return &virtual_groups[i];
        }
    }
    return NULL;
}

static inline uint32_t command_index_hash(uint16_t command_id)
{
    return ((uint32_t)command_id * 2654435761u) & (COMMAND_INDEX_TABLE_SIZE - 1u);
}

static void rebuild_command_index_table(void)
{
    for (size_t i = 0; i < COMMAND_INDEX_TABLE_SIZE; ++i) {
        command_index_table[i] = -1;
    }
    for (size_t i = 0; i < command_table_size; ++i) {
        uint16_t id = command_table[i].command_id;
        uint32_t pos = command_index_hash(id);
        for (size_t probe = 0; probe < COMMAND_INDEX_TABLE_SIZE; ++probe) {
            if (command_index_table[pos] < 0) {
                command_index_table[pos] = (int16_t)i;
                break;
            }
            pos = (pos + 1u) & (COMMAND_INDEX_TABLE_SIZE - 1u);
        }
    }
}

static const CommandEntry *find_command_entry(int command_ID)
{
    if (command_table_size == 0) {
        return NULL;
    }
    if (command_ID <= 0 || command_ID > 65535) {
        return NULL;
    }
    uint16_t id = (uint16_t)command_ID;
    uint32_t pos = command_index_hash(id);
    for (size_t probe = 0; probe < COMMAND_INDEX_TABLE_SIZE; ++probe) {
        int16_t idx = command_index_table[pos];
        if (idx < 0) {
            return NULL;
        }
        if ((size_t)idx < command_table_size && command_table[idx].command_id == id) {
            return &command_table[idx];
        }
        pos = (pos + 1u) & (COMMAND_INDEX_TABLE_SIZE - 1u);
    }
    return NULL;
}

static void playlist_sequence_reset(PlaylistSequence *sequence)
{
    if (sequence == NULL) {
        return;
    }
    if (sequence->steps != NULL) {
        heap_caps_free(sequence->steps);
        sequence->steps = NULL;
    }
    sequence->count = 0;
    sequence->capacity = 0;
    sequence->total_duration_ms = 0;
}

static bool playlist_sequence_append(PlaylistSequence *sequence, const PlaylistStep *step)
{
    if (sequence == NULL || step == NULL) {
        return false;
    }
    if (sequence->count >= MAX_PLAYLIST_STEPS) {
        return false;
    }

    if (sequence->count == sequence->capacity) {
        uint16_t new_capacity = (sequence->capacity == 0) ? 16u : (uint16_t)(sequence->capacity * 2u);
        if (new_capacity > MAX_PLAYLIST_STEPS) {
            new_capacity = MAX_PLAYLIST_STEPS;
        }
        if (new_capacity <= sequence->capacity) {
            return false;
        }

        size_t new_size = (size_t)new_capacity * sizeof(PlaylistStep);
        PlaylistStep *new_steps = (PlaylistStep *)heap_caps_realloc(
            sequence->steps,
            new_size,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (new_steps == NULL) {
            return false;
        }
        sequence->steps = new_steps;
        sequence->capacity = new_capacity;
    }

    sequence->steps[sequence->count] = *step;
    sequence->count++;
    if (step->duration_ms > sequence->total_duration_ms) {
        sequence->total_duration_ms = step->duration_ms;
    }
    return true;
}

static PlaylistSequenceCacheEntry *find_playlist_cache_entry(uint16_t playlist_id)
{
    for (size_t i = 0; i < playlist_sequence_cache_count; ++i) {
        if (playlist_sequence_cache[i].playlist_id == playlist_id) {
            return &playlist_sequence_cache[i];
        }
    }
    return NULL;
}

static PlaylistSequenceCacheEntry *create_playlist_cache_entry(uint16_t playlist_id)
{
    if (playlist_sequence_cache_count < MAX_PLAYLIST_CACHE) {
        PlaylistSequenceCacheEntry *entry = &playlist_sequence_cache[playlist_sequence_cache_count++];
        memset(entry, 0, sizeof(*entry));
        entry->playlist_id = playlist_id;
        return entry;
    }

    for (size_t i = 0; i < playlist_sequence_cache_count; ++i) {
        bool in_use = false;
        for (int s = 0; s < MAX_ACTIVE_PLAYLISTS; ++s) {
            if (playlist_slots[s].active && playlist_slots[s].sequence_ref == &playlist_sequence_cache[i].sequence) {
                in_use = true;
                break;
            }
        }
        if (!in_use) {
            playlist_sequence_reset(&playlist_sequence_cache[i].sequence);
            memset(&playlist_sequence_cache[i], 0, sizeof(playlist_sequence_cache[i]));
            playlist_sequence_cache[i].playlist_id = playlist_id;
            return &playlist_sequence_cache[i];
        }
    }
    return NULL;
}

static void clear_playlist_sequence_cache(void)
{
    for (int i = 0; i < MAX_ACTIVE_PLAYLISTS; ++i) {
        playlist_slots[i].active = 0;
        playlist_slots[i].playlist_id = 0;
        playlist_slots[i].sequence_ref = NULL;
        playlist_slots[i].current_step_index = 0;
    }

    for (size_t i = 0; i < playlist_sequence_cache_count; ++i) {
        playlist_sequence_reset(&playlist_sequence_cache[i].sequence);
    }
    memset(playlist_sequence_cache, 0, sizeof(playlist_sequence_cache));
    playlist_sequence_cache_count = 0;
}

static const PlaylistSequence *get_compiled_playlist_sequence(uint16_t playlist_id)
{
    if (playlist_id == 0) {
        return NULL;
    }
    PlaylistSequenceCacheEntry *entry = find_playlist_cache_entry(playlist_id);
    if (entry == NULL) {
        entry = create_playlist_cache_entry(playlist_id);
        if (entry == NULL) {
            return NULL;
        }
    }
    if (!entry->valid) {
        playlist_sequence_reset(&entry->sequence);
        if (!build_playlist_sequence_by_id((int)playlist_id, &entry->sequence, 0)) {
            playlist_sequence_reset(&entry->sequence);
            return NULL;
        }
        entry->valid = 1u;
    }
    return &entry->sequence;
}

static uint32_t resolve_target_channel_mask(PlaylistTargetType target_type, uint32_t bitfield)
{
    const uint32_t channel_limit_mask = ((1u << NUMBER_OF_CHANNELS) - 1);
    switch (target_type) {
        case TARGET_ALL_CHANNELS:
            return channel_limit_mask;
        case TARGET_SELECTED_CHANNELS:
            return bitfield & channel_limit_mask;
        case TARGET_VIRTUAL_GROUPS: {
            if (bitfield != 0) {
                const VirtualGroup *vg = find_virtual_group_by_id((int)bitfield);
                if (vg != NULL) {
                    return ((uint32_t)vg->channel_mask) & channel_limit_mask;
                }
                else
                {
                	printf("Virtual ID not found \n");
                }
            }
            uint32_t mask = 0;
            for (int id = 1; id <= MAX_VIRTUAL_GROUPS; ++id) {
                if ((bitfield & (1u << (id - 1))) == 0) {
                    continue;
                }
                const VirtualGroup *vg = find_virtual_group_by_id(id);
                if (vg != NULL) {
                    mask |= (uint32_t)vg->channel_mask;
                }
            }

#ifdef ENABLE_PRINT_MSG
            printf("mask = %ld \n", mask);
#endif

            return mask & channel_limit_mask;
        }
        default:
            return channel_limit_mask;
    }
}

static uint32_t resolve_playlist_entry_target_mask(uint8_t entry_target_type, uint8_t entry_target_bitfield, bool has_target_override, PlaylistTargetType target_type_override, bool has_target_bitfield_override, uint8_t target_bitfield_override)
{
    uint32_t entry_mask = resolve_target_channel_mask((PlaylistTargetType)entry_target_type, (uint32_t)entry_target_bitfield);
    entry_mask &= ((1u << NUMBER_OF_CHANNELS) - 1u);

    if (!has_target_override && !has_target_bitfield_override) {
        return entry_mask;
    }

    PlaylistTargetType override_type = target_type_override;
    if (override_type == TARGET_TYPE_COUNT) {
        override_type = TARGET_ALL_CHANNELS;
    }

    uint32_t override_bitfield = has_target_bitfield_override
        ? (uint32_t)target_bitfield_override
        : ((override_type == TARGET_ALL_CHANNELS) ? ((1u << NUMBER_OF_CHANNELS) - 1u) : 0u);

    uint32_t override_mask = resolve_target_channel_mask(override_type, override_bitfield);
    override_mask &= ((1u << NUMBER_OF_CHANNELS) - 1u);

    return entry_mask & override_mask;
}

static void executeCommand(int command_ID, int Chan, const PlaylistRequest *request)
{
    if (Chan < 0 || Chan >= NUMBER_OF_CHANNELS) {
        return;
    }

    const CommandEntry *command = find_command_entry(command_ID);
    if (command == NULL) {
		printf("playlist executeCommand: channel=%d command_id=%d not found \n", Chan + 1, command_ID);
        return;
    }
    /* Brightness: use request override if >= 0, else Command_Table brightness per Command_ID */
    float brightness = (request != NULL && request->brightness_override > 0.0f)
        ? request->brightness_override
        : command->brightness;
    if (brightness > 100.0f) {
        brightness = 100.0f;
    }
    if (brightness < 0.0f) {
        brightness = 0.0f;
    }
    playlist_pending_brightness[Chan] = brightness;

#ifdef ENABLE_PRINT_MSG
    printf("playlist executeCommand: playlist=%u channel=%d command_id=%d type=%d brightness=%.1f \n",
                       (request != NULL) ? request->playlist_id : 0u,
                       Chan + 1,
                       command_ID,
                       command->type,
                       brightness);
#endif

    switch (command->type) {
        case COMMAND_TYPE_ON:
            setColorStartFlag[Chan] = 1;
            break;
        case COMMAND_TYPE_OFF:
//        	printf(" command type 11= %d \n", command->type);
        	flag_not_rmt = 0;
        	delay_same_array = 1;
			flag_direct_array_testing = 0;
			flag_direct_array_testing_2 = 0;
            TurnFlagsOff(Chan + 1);
            StripChanOFF(Chan + 1);
            break;
        case COMMAND_TYPE_COLOR:
        case COMMAND_TYPE_PATTERN:
        case COMMAND_TYPE_EFFECT:
        case COMMAND_TYPE_SCENE:
        case COMMAND_TYPE_LIGHT_SHOW:
            /* Defer apply to task: task will fill arrays via apply_playlist_command_to_channel, then call PrepareDataWithModeSetting */

//        	printf(" command type 12= %d \n", command->type);

        	playlist_pending_command[Chan] = command;
            playlist_apply_pending[Chan] = true;
            break;
        case COMMAND_TYPE_PLAYLIST:
            /* Should not appear in steps when recursion inlined; no-op if present */
            break;
        default:
            break;
    }
}

/* Run all active playlists: absolute-time scheduling, rollover, stop after total_duration_override. */
static void notify_playlist_complete(int playlist_id)
{
    if (playlist_id < 1 || playlist_id > MAX_ACTIVE_PLAYLISTS) {
        return;
    }

#ifdef ENABLE_PRINT_MSG
    printf("playlist complete: playlist_id=%d \n", playlist_id);
#endif

    AMessage_st message;
    char payload[128];

    memset(&message, 0, sizeof(message));
    snprintf(payload, sizeof(payload),
             "{\"RESP\":\"Playlist %d is complete.\",\"Playlist_ID\":%d}",
             playlist_id, playlist_id);

    message.payload_p8 = (uint8_t*)payload;
    strcpy((char*)message.cmdFun_a8, "PLAYLIST_COMPLETE");
    strcpy((char*)message.src_Actor_a8, THIS_ACTOR);
    strcpy((char*)message.dest_Actor_a8, "CONSOLE");
    console_send_responce_to_console_xface(&message);
}

static void executePlaylistFunc(int Chan, uint64_t u64CurrentTime)
{
static int diff_cnt = 0;
    (void)Chan; /* one call per tick; we iterate all slots */
    uint64_t now = u64CurrentTime;

    for (int slot_id = 0; slot_id < MAX_ACTIVE_PLAYLISTS; slot_id++) {
        PlaylistSlot *slot = &playlist_slots[slot_id];
        if (!slot->active || slot->sequence_ref == NULL || slot->sequence_ref->count == 0) {
            continue;
        }

        uint64_t elapsed_total_ms = (now >= slot->initial_start_ms) ? (now - slot->initial_start_ms) : 0;
        uint64_t total_limit_ms = (uint64_t)slot->total_duration_override_sec * 1000u;

//        if((now < slot->initial_start_ms) && (total_limit_ms > 0))
        if(now < slot->initial_start_ms)
        {
        	uint64_t remaining_time = (slot->initial_start_ms - now)/1000;
        	if(diff_cnt != remaining_time)
        	{
        		printf("remaining time to start playlist in Second = %llu \n", (unsigned long long)remaining_time);

        		diff_cnt = remaining_time;
        	}

        }

        if (total_limit_ms > 0 && elapsed_total_ms >= total_limit_ms) {
            slot->active = false;
            notify_playlist_complete(slot->playlist_id);
            slot->playlist_id = 0;
            slot->sequence_ref = NULL;
            continue;
        }

        const PlaylistSequence *seq = slot->sequence_ref;
        if (seq->count == 0) {
            continue;
        }

        size_t step_index = slot->current_step_index;

        while (slot->active) {
            if (step_index >= seq->count) {
                uint64_t next_cycle_start_ms = slot->cycle_start_ms + slot->cycle_duration_ms;
                uint64_t first_step_offset_ms = (seq->count > 0) ? (uint64_t)seq->steps[0].duration_ms : 0;
                slot->next_step_start_ms = next_cycle_start_ms + first_step_offset_ms;

                if (slot->cycle_duration_ms == 0 || now < next_cycle_start_ms) {
                    break;
                }

                slot->cycle_start_ms = next_cycle_start_ms;
                step_index = 0;
                slot->current_step_index = 0;
            }

            uint64_t step_start_ms = slot->cycle_start_ms + (uint64_t)seq->steps[step_index].duration_ms;
            slot->next_step_start_ms = step_start_ms;
            if (now < step_start_ms) {
                break;
            }

            const PlaylistStep *step = &seq->steps[step_index];

            uint32_t target_mask = resolve_playlist_entry_target_mask(
                step->target_type,
                step->target_bitfield,
                slot->has_target_override != 0u,
                (PlaylistTargetType)slot->target_type_override,
                slot->has_target_bitfield_override != 0u,
                slot->target_bitfield_override
            );

#if defined(B542)
            target_mask &= 15u;
#endif

#ifdef ENABLE_PRINT_MSG
            printf("playlist step: playlist=%u slot=%d step=%u command=%d start_ms=%llu now_ms=%llu duration_ms=%lu target_type=%d target_mask=0x%02lx \n",
                               slot->playlist_id,
                               slot_id + 1,
                               (unsigned)step_index,
                               step->command_id,
                               (unsigned long long)step_start_ms,
                               (unsigned long long)now,
                               (unsigned long)step->duration_ms,
                               (int)step->target_type,
                               (unsigned long)target_mask);
#endif

            for (int c = 0; c < NUMBER_OF_CHANNELS; c++) {
                if ((target_mask & (1u << c)) == 0) {
                    continue;
                }
                executeCommand(step->command_id, c, &slot->request);
            }

            step_index++;
            slot->current_step_index = (uint16_t)step_index;
            if (step_index < seq->count) {
                slot->next_step_start_ms = slot->cycle_start_ms + (uint64_t)seq->steps[step_index].duration_ms;
            } else {
                uint64_t next_cycle_start_ms = slot->cycle_start_ms + slot->cycle_duration_ms;
                uint64_t first_step_offset_ms = (seq->count > 0) ? (uint64_t)seq->steps[0].duration_ms : 0;
                slot->next_step_start_ms = next_cycle_start_ms + first_step_offset_ms;
            }
        }
    }
}

static uint64_t convert_local_epoch_ms_to_system_epoch_ms(uint64_t local_epoch_ms)
{
    if (local_epoch_ms == 0) {
        return 0;
    }

    int64_t offset_ms = (int64_t)gmt_val * 60LL * 1000LL;
    if (dst_val == 1) {
        offset_ms += 3600LL * 1000LL;
    }

    int64_t system_epoch_ms = (int64_t)local_epoch_ms - offset_ms;
    if (system_epoch_ms < 0) {
        return 0;
    }
    return (uint64_t)system_epoch_ms;
}

static void stopPlaylist()
{
	int playlist_id = 1;
    int slot_id = playlist_id - 1;

	for(playlist_id = 1; playlist_id <= MAX_ACTIVE_PLAYLISTS; playlist_id++ )
	{
		slot_id = playlist_id - 1;
	    PlaylistSlot *slot = &playlist_slots[slot_id];

#ifdef ENABLE_PRINT_MSG
        if (slot->active) {
        	printf("playlist stop: playlist_id=%u slot=%d current_step=%u next_step_start_ms=%llu \n",
                               slot->playlist_id,
                               slot_id + 1,
                               (unsigned)slot->current_step_index,
                               (unsigned long long)slot->next_step_start_ms);
        }
#endif

	    slot->active = false;

	    slot->playlist_id = 0;
	    slot->sequence_ref = NULL;
	}

    playlist_started_flag = false;

    AMessage_st message;
    char payload[128];

    memset(&message, 0, sizeof(message));
    snprintf(payload, sizeof(payload),
             "{\"RESP\":\"All Playlist ID Stop.\"}");

    message.payload_p8 = (uint8_t*)payload;
    strcpy((char*)message.cmdFun_a8, "PLAYLIST_STOP");
    strcpy((char*)message.src_Actor_a8, THIS_ACTOR);
    strcpy((char*)message.dest_Actor_a8, "CONSOLE");
    console_send_responce_to_console_xface(&message);
}

static int executePlaylist(int playlist_id, float brightness_override, uint32_t total_duration_override_sec, uint64_t local_start_time_ms, bool has_target_override, PlaylistTargetType target_type_override, uint8_t target_bitfield_override, bool has_target_bitfield_override)
{
    if (playlist_id < 1 || playlist_id > MAX_ACTIVE_PLAYLISTS) {
        return -1;
    }
    uint64_t start_ms = get_current_time_ms();
    if (local_start_time_ms != 0) {
//        uint64_t converted_start_ms = convert_local_epoch_ms_to_system_epoch_ms(local_start_time_ms);
    	uint64_t converted_start_ms = local_start_time_ms;
        if (converted_start_ms != 0) {
            start_ms = converted_start_ms;
        }
    }
    int slot_id = playlist_id - 1;
    PlaylistSlot *slot = &playlist_slots[slot_id];

    const PlaylistSequence *compiled = get_compiled_playlist_sequence((uint16_t)playlist_id);
    if (compiled == NULL || compiled->count == 0) {
        return -1;
    }
    log_playlist_memory_usage("execute_playlist_start");

    slot->active = true;
    slot->playlist_id = (uint16_t)playlist_id;
    slot->initial_start_ms = start_ms;
    slot->total_duration_override_sec = total_duration_override_sec;
    slot->brightness_override = brightness_override;
    slot->has_target_override = has_target_override ? 1u : 0u;
    slot->target_type_override = (uint8_t)target_type_override;
    slot->target_bitfield_override = target_bitfield_override;
    slot->has_target_bitfield_override = has_target_bitfield_override ? 1u : 0u;
    slot->sequence_ref = compiled;
    slot->current_step_index = 0;
    slot->cycle_start_ms = start_ms;
    slot->cycle_duration_ms = (compiled->total_duration_ms > 0)
        ? compiled->total_duration_ms
        : 1;
    slot->next_step_start_ms = start_ms;

    slot->request.playlist_id = (uint16_t)playlist_id;
    slot->request.epocStartTime = start_ms;
    slot->request.durationMsec = total_duration_override_sec * 1000u;
    slot->request.brightness_override = brightness_override;
    slot->request.target_type_override = (uint8_t)target_type_override;
    slot->request.target_bitfield_override = target_bitfield_override;
    slot->request.has_target_bitfield_override = has_target_bitfield_override ? 1u : 0u;
    if (compiled->count > 0) {
        slot->next_step_start_ms = start_ms + (uint64_t)compiled->steps[0].duration_ms;
    }

    playlist_started_flag = true;

#ifdef ENABLE_PRINT_MSG
    printf("playlist start: playlist_id=%d start_ms=%llu steps=%u cycle_ms=%llu brightness=%.1f total_duration_sec=%lu target_type=%d target_bitfield=0x%02x target_override=%d bitfield_override=%d \n",
                       playlist_id,
                       (unsigned long long)start_ms,
                       (unsigned)compiled->count,
                       (unsigned long long)slot->cycle_duration_ms,
                       brightness_override,
                       (unsigned long)total_duration_override_sec,
                       (int)target_type_override,
                       (unsigned int)target_bitfield_override,
                       has_target_override ? 1 : 0,
                       has_target_bitfield_override ? 1 : 0);
#endif
    return 0;
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char keyValue[100] = {0};
	uint8_t	value=0;

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);

	if (in_JSON == NULL)
	{
		printf("\n TRIM s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		Add_Response_msg("Invalid JSON input in trimlight actor.", s_Message_Rx, payLoadData);
		return;
	}
	else
	{
			// Obtain the COMMAND and RESPONSE keys
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

			if (commandKey != NULL && responseKey != NULL)
			{
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);

			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"WRITE_FILE")==0))
		{
			name_JSON 		= cJSON_GetObjectItem(in_JSON, "RESPONSE");
			name_JSON = name_JSON->child;
			if (cJSON_IsNumber(name_JSON) && (!(strcmp(name_JSON->string, "JFS_Resp"))))
			{
				value	= (uint8_t)cJSON_GetNumberValue(name_JSON);
				JFS_Response= value;
			}
		}
		else if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_FILE_SIZE")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				name_JSON 		= cJSON_GetObjectItem(root, "FILE_NAME");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					if(strcmp(name_JSON->valuestring,"Database/Color.db")==0)
					{
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(vColor_Table_Que != NULL)
							xQueueSend(vColor_Table_Que, payLoadData, QUE_DELAY);
					}
					else if(strcmp(name_JSON->valuestring,"Database/Command_Table.db")==0)
					{
						//printf(" In analyse_r 2\n");
//						queue_sql_response(vCommand_Table_Que, payLoadData_Command_table, sizeof(payLoadData_Command_table), root);
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(vCommand_Table_Que != NULL)
							xQueueSend(vCommand_Table_Que, payLoadData, QUE_DELAY);
					}
					else if(strcmp(name_JSON->valuestring,"Database/Playlist_Table.db")==0)
					{
					//	printf(" In analyse_r 3\n");
						memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(vPlaylist_Table_Que != NULL)
							xQueueSend(vPlaylist_Table_Que, payLoadData, QUE_DELAY);
					}
					else if((strcmp(name_JSON->valuestring,"Database/Virtual_Table.db")==0) ||
							(strcmp(name_JSON->valuestring,"Database/Virtual_Group_Table.db")==0))
					{
					//	printf(" In analyse_r virtual table\n");
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
						if(vVirtual_Table_Que != NULL)
							xQueueSend(vVirtual_Table_Que, payLoadData, QUE_DELAY);
					}
				}
			}
	    }
		else if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				// Iterate over the keys
				cJSON *currentItem = root->child;
				if(currentItem != NULL)
				{
					if(currentItem->valuestring != NULL)
					{
						if(!strcasecmp(currentItem->valuestring, "System/Light_Command.json"))
						{
							currentItem = currentItem->next;
							while (currentItem != NULL)
							{
								if (cJSON_IsString(currentItem))   // Check the type of the value
								{
									command_run_at_power_up = 1;
									set(currentItem->string, currentItem->valuestring,s_Message_Rx);
									command_run_at_power_up = 0;
								}
								currentItem = currentItem->next;    // Move to the next key-value pair
							}
						}
						else
						{
							if(!strcasecmp(currentItem->valuestring, "System/LIGHTING.json"))
							{
								currentItem = currentItem->next;
								while (currentItem != NULL)
								{
									if (cJSON_IsString(currentItem))   // Check the type of the value
									{
										if ((strcmp(currentItem->string, "CHAN1LASTCOMMAND") != 0) && (strcmp(currentItem->string, "CHAN2LASTCOMMAND") != 0) && (strcmp(currentItem->string, "CHAN3LASTCOMMAND") != 0) && (strcmp(currentItem->string, "CHAN4LASTCOMMAND") != 0))
										{
											command_run_at_power_up = 1;
											set(currentItem->string, currentItem->valuestring,s_Message_Rx);
											command_run_at_power_up = 0;
										}
									}
									else if (cJSON_IsNumber(currentItem))
									{
										sprintf(keyValue, "%d", currentItem->valueint);
										set(currentItem->string, keyValue,s_Message_Rx);
									}
									currentItem = currentItem->next;    // Move to the next key-value pair
								}
							}
							else
							{
								cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
								if (responseKey != NULL && cJSON_IsString(responseKey->child))
								{
									cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
									if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
									{
										if(strcmp(name_JSON->valuestring, "System/LIGHTING.json file is not present in JFS.") == 0)
										{
											char val [300]=  {0};
											getAll(NULL,val,NULL);
										}
									}
								}
							}
						}
					}
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SQL")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"DB_EXECUTE")==0))
		{

			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsObject(responseKey))
			{
				//printf(" In analyse_r 6\n");

				bool handled = false;
				cJSON *name_field = cJSON_GetObjectItemCaseSensitive(responseKey, "Name");
				if (name_field != NULL && strcmp(name_field->string, "Name") == 0)
				{
					queue_sql_response(vColor_Table_Que, payLoadData, sizeof(payLoadData), responseKey);
					handled = true;
				}

				if (!handled)
				{
					cJSON *col_min = cJSON_GetObjectItemCaseSensitive(responseKey, "COL_MIN_ID");
					if (col_min != NULL && strcmp(col_min->string, "COL_MIN_ID") == 0)
					{
//						queue_sql_response(vColor_Table_Que, payLoadData, sizeof(payLoadData), responseKey);
//						handled = true;

					   	memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(vColor_Table_Que != NULL)
							xQueueSend(vColor_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
					//printf(" In analyse_r 7\n");

					cJSON *command_min = cJSON_GetObjectItemCaseSensitive(responseKey, "COMMAND_MIN_ID");
					if (command_min != NULL)
					{
					   	memset(payLoadData,0,sizeof(payLoadData));//\0';
						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);

//						printf("payLoadData 1 = %s \n", payLoadData);

						if(vCommand_Table_Que != NULL)
							xQueueSend(vCommand_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
//					printf(" In analyse_r 8\n");

					cJSON *command_type = cJSON_GetObjectItemCaseSensitive(responseKey, "Command_Type");
					if (command_type == NULL)
					{
						command_type = cJSON_GetObjectItemCaseSensitive(responseKey, "CommandType");
					}
					if (command_type == NULL)
					{
						command_type = cJSON_GetObjectItemCaseSensitive(responseKey, "command_type");
					}
					if (command_type == NULL)
					{
						command_type = cJSON_GetObjectItemCaseSensitive(responseKey, "commandtype");
					}
					if (command_type != NULL)
					{

					   	memset(payLoadData,0,sizeof(payLoadData));//\0';

					//   	printf("payLoadData 2 = %s \n", payLoadData);

						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(vCommand_Table_Que != NULL)
							xQueueSend(vCommand_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
			//		printf(" In analyse_r 9\n");

					cJSON *playlist_min = cJSON_GetObjectItemCaseSensitive(responseKey, "PLAY_MIN_ID");
					if (playlist_min != NULL)
					{

					   	memset(payLoadData,0,sizeof(payLoadData));//\0';

					  // 	printf("payLoadData 3 = %s \n", payLoadData);

						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(vPlaylist_Table_Que != NULL)
							xQueueSend(vPlaylist_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
				//	printf(" In analyse_r 10\n");

					const char *const playlist_row_keys[] = {"PlaylistEntryID", "Playlist_Entry_ID", "Playlist_ID", "playlist_id", "Target_Type", "TargetType"};
					cJSON *playlist_row = NULL;
					for (size_t i = 0; i < sizeof(playlist_row_keys) / sizeof(playlist_row_keys[0]); ++i)
					{
						playlist_row = cJSON_GetObjectItemCaseSensitive(responseKey, playlist_row_keys[i]);
						if (playlist_row != NULL)
						{
							break;
						}
					}
					if (playlist_row != NULL)
					{
					   	memset(payLoadData,0,sizeof(payLoadData));//\0';

					//   	printf("payLoadData 4 = %s \n", payLoadData);

						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						if(vPlaylist_Table_Que != NULL)
							xQueueSend(vPlaylist_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
					cJSON *virtual_min = cJSON_GetObjectItemCaseSensitive(responseKey, "VIRTUAL_GROUP_MIN_ID");
					if (virtual_min != NULL)
					{
					   	memset(payLoadData,0,sizeof(payLoadData));//\0';

						cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);

					//	printf("payLoadData 51 = %s \n", payLoadData);

						if(vVirtual_Table_Que != NULL)
							xQueueSend(vVirtual_Table_Que, payLoadData, QUE_DELAY);
					}
				}

				if (!handled)
				{
					const size_t virtual_row_key_count = sizeof(virtual_group_row_keys) / sizeof(virtual_group_row_keys[0]);
					for (size_t idx = 0; idx < virtual_row_key_count; ++idx)
					{
						if (cJSON_GetObjectItemCaseSensitive(responseKey, virtual_group_row_keys[idx]) != NULL)
						{
						   	memset(payLoadData,0,sizeof(payLoadData));//\0';

						  // 	printf("payLoadData 52 = %s \n", payLoadData);

							cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
							if(vVirtual_Table_Que != NULL)
								xQueueSend(vVirtual_Table_Que, payLoadData, QUE_DELAY);
							break;
						}
					}
				}
		 }
		 }

		cJSON_Delete(in_JSON);
		return;
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"EVENT_ACTOR")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "GMT");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					gmt_val = atoi(name_JSON->valuestring);
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

void MirrorData(int position, int channel) {
    int ModeSetting = sAddressableStipArr[IC_Type_Var].u8ModeSetting;

    if(light_para.locationMidpoint_float != 0)
    {
    	if(channel == 0)
    	{
    		position = light_para.SetLEDstripalCh1_u16/2;
    	}
    	else if(channel == 1)
    	{
    		position = light_para.SetLEDstripalCh2_u16/2;
    	}
    	else if(channel == 2)
    	{
    		position = light_para.SetLEDstripalCh3_u16/2;
    	}
    	else if(channel == 3)
    	{
    		position = light_para.SetLEDstripalCh4_u16/2;
    	}
    }

    int half_strip = EXAMPLE_LED_NUMBERS / 2;
    uint16_t* data_channel = (use_ping_buffer[channel]) ? data_channels_ping[channel] : data_channels[channel];

    int offset_temp = 0;

    if(MarqueeExecuteCustomStartFlag[channel] == 1)
    {
		marqueeImage_t *mImg = &marqueeImage_start[channel];

		if(mImg->spacingOverride == 0)
		{
			offset_temp = (((ChannelParamObject[channel].offsetCh_float) + (light_para.locationOffset_float)) *25.4) /ChannelParamObject[channel].LEDspacingCh_float;
		}
		else
		{
			offset_temp = (((ChannelParamObject[channel].offsetCh_float) + (light_para.locationOffset_float))); // Number of led
		}
    }
    else if(ExecuteCustomStartFlag[channel] == 1)
    {
    	customImage *imgConfig = &ImageConfig_start[channel];

		if(imgConfig->spacingOverride == 0)
		{
			offset_temp = (((ChannelParamObject[channel].offsetCh_float) + (light_para.locationOffset_float)) *25.4) /ChannelParamObject[channel].LEDspacingCh_float;
		}
		else
		{
			offset_temp = (((ChannelParamObject[channel].offsetCh_float) + (light_para.locationOffset_float))); // Number of led
		}
    }

    position = position - offset_temp;
//    int offset_temp1 = offset_temp;
//    if(ChannelParamObject[channel].revDirCh_u8 != 0)
//    {
//    	position = (ChannelParamObject[channel].SetLEDstripal_u16) - position;
//    }

    int length = position * 2;


    offset_temp = 0;

    length = length - offset_temp;

    if (sAddressableStipArr[IC_Type_Var].u8NumberOfbits == 1) {
    	if(ChannelParamObject[channel].revDirCh_u8 == 0)
    	{
			if (half_strip < position) {
				// Iterate through the first half of the strip
				for (int k = 0; k < length / 2; k++) {
					data_channel[ModeSetting + (length - k - 1) * 3] = data_channel[ModeSetting + (k + offset_temp) * 3];
					data_channel[ModeSetting + (length - k - 1) * 3 + 1] = data_channel[ModeSetting + (k + offset_temp) * 3 + 1];
					data_channel[ModeSetting + (length - k - 1) * 3 + 2] = data_channel[ModeSetting + (k + offset_temp) * 3 + 2];
				}
			} else {
				// Iterate through the first half of the strip
				for (int k = 0; k < length / 2; k++) {
					data_channel[ModeSetting + (k + offset_temp) * 3] = data_channel[ModeSetting + (length - k - 1) * 3];
					data_channel[ModeSetting + (k + offset_temp) * 3 + 1] = data_channel[ModeSetting + (length - k - 1) * 3 + 1];
					data_channel[ModeSetting + (k + offset_temp) * 3 + 2] = data_channel[ModeSetting + (length - k - 1) * 3 + 2];
				}
			}
    	}
    	else
    	{
//    		position = position + offset_temp1;

    		int temp_100 = ChannelParamObject[channel].SetLEDstripal_u16;

    		int position_1 = position;

////    		printf("temp_100 = %d \n", temp_100);

//    		int length_rev = (position) * 2;
    		if(position<=(temp_100))
    		{
    			position = temp_100 - position;

				length = (position) * 2;
				if (half_strip < position) {
					// Iterate through the first half of the strip
	//				for (int k = 0; k < length_rev / 2; k++) {
					for (int k = 0; k < length / 2; k++) {
						data_channel[ModeSetting + (k + offset_temp) * 3] = data_channel[ModeSetting + (length - k - 1) * 3];
						data_channel[ModeSetting + (k + offset_temp) * 3 + 1] = data_channel[ModeSetting + (length - k - 1) * 3 + 1];
						data_channel[ModeSetting + (k + offset_temp) * 3 + 2] = data_channel[ModeSetting + (length - k - 1) * 3 + 2];
					}
					if(position_1 > position)
					{
	//					printf("In next loop \n");
						int rem_len = position_1 - position;
						for (int k = 0; k < rem_len; k++) {
	//						printf("In next 2 loop \n");
							data_channel[ModeSetting + (length + 1 + k + offset_temp) * 3] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3];
							data_channel[ModeSetting + (length + 1 + k + offset_temp) * 3 + 1] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3 + 1];
							data_channel[ModeSetting + (length + 1 + k + offset_temp) * 3 + 2] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3 + 2];
						}

					}

				} else {

					// Iterate through the first half of the strip
					for (int k = 0; k < length / 2; k++) {
						data_channel[ModeSetting + (length - k - 1) * 3] = data_channel[ModeSetting + (k + offset_temp) * 3];
						data_channel[ModeSetting + (length - k - 1) * 3 + 1] = data_channel[ModeSetting + (k + offset_temp) * 3 + 1];
						data_channel[ModeSetting + (length - k - 1) * 3 + 2] = data_channel[ModeSetting + (k + offset_temp) * 3 + 2];
					}

					if(position_1 > position)
					{
	//					printf("In next loop \n");
						int rem_len = position_1 - position;
						for (int k = 0; k < rem_len; k++) {
	//						printf("In next 2 loop \n");
							data_channel[ModeSetting + (length + k + offset_temp) * 3] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3];
							data_channel[ModeSetting + (length + k + offset_temp) * 3 + 1] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3 + 1];
							data_channel[ModeSetting + (length + k + offset_temp) * 3 + 2] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - k - 1) * 3 + 2];
						}

					}
				}

			}
			else
			{
//				printf("position = %d \n", position);

				int offset = position - temp_100;
				offset = offset*2;

				for (int k = 0; k < temp_100; k++) {
//						printf("In next 2 loop \n");
					data_channel[ModeSetting + ( k + offset_temp) * 3] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - offset - k - 1) * 3];
					data_channel[ModeSetting + ( k + offset_temp) * 3 + 1] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - offset - k - 1) * 3 + 1];
					data_channel[ModeSetting + ( k + offset_temp) * 3 + 2] = data_channel[ModeSetting + (EXAMPLE_LED_NUMBERS - offset - k - 1) * 3 + 2];
				}
			}
    	}
    }
}

void Execute_PrepareDataWithModeSetting(float offset1, int Chan, int Start_Offset_Flag, int fill_data)
{
    // Predeclare / localize frequently used pointers & variables
//    customImage       *imgConfig   = &ImageConfig_start[Chan];

	int temp_ramp = 0;
	if(fill_data == 2)
	{
//				customImage *imgConfig = &ImageConfig_end[Chan];
		temp_ramp = 1;
	}
	else
	{
//				customImage *imgConfig = &ImageConfig_start[Chan];
	}

	customImage *imgConfig = (temp_ramp) ? &ImageConfig_end[Chan] : &ImageConfig_start[Chan];

    ChannelParameters      *chParam     = &ChannelParamObject[Chan];
    float             *pImageSize  = &ImageSize_forMode[Chan];

    // 1) Precompute often-used values outside the loop
    float scale                = chParam->scaleCh_float;
    float ledspacingFactor     = chParam->LEDspacingCh_float * (1.0f / 25.4f);  // multiply instead of dividing every time
    float scaleSpacing         = scale * ledspacingFactor;                      // used in the loop
    float modValInch           = (*pImageSize) / scale;                         // was: (ImageSize_forMode[Chan])/(chParam->scaleCh_float)
    float segmentLength        = imgConfig->colorLength + imgConfig->paddingLength;
    float invSegmentLength = 1.0f / (segmentLength - 0.0001f);

    // 2) Prepare offset once (modulus to keep it in [0, modValInch))

    float offset = 0.0f;


    if(imgConfig->spacingOverride == 0)
    {
    	offset = chParam->offsetCh_float + offset1 + modValInch + (light_para.locationOffset_float);
    }
    else
    {
    	offset = (((chParam->offsetCh_float + light_para.locationOffset_float) * chParam->LEDspacingCh_float)/25.4) + offset1 +  modValInch;
    }

    while (offset < 0.0f) {
        offset += modValInch;
    }

    offset=fmod(offset, modValInch);

    // 3) Compute brightness_factor once
    float brightness_factor = (light_para.contrMaxB_float * 0.01f);  // dividing by 100 => multiply by 0.01
    switch (Chan + 1) {
        case 1:
            brightness_factor *= (light_para.chan1MaxB_float * 0.01f);
            break;
        case 2:
            brightness_factor *= (light_para.chan2MaxB_float * 0.01f);
            break;
        case 3:
            brightness_factor *= (light_para.chan3MaxB_float * 0.01f);
            break;
        case 4:
            brightness_factor *= (light_para.chan4MaxB_float * 0.01f);
            break;
        default:
            printf("Invalid channel number: %d\n", Chan);
            return;
    }
    if (brightness_RunTimeChan[Chan] != 0) {
        brightness_factor *= (brightness_RunTimeChan[Chan] * 0.01f);
    }

    // 4) Prepare pointer for final data copy
    uint16_t *data_channel =
        (use_ping_buffer[Chan]) ? data_channels_ping[Chan] : data_channels[Chan];

    // 5) Clear or prefetch local arrays for usage in the loop (if needed).
    //    The set_led_color() likely writes into data_channels1_x.

    // For readability, store the references to color arrays
    const Color *colorSelections = imgConfig->colorSelections;
    uint8_t   numColors              = imgConfig->numColors;
    uint8_t   transitionType         = imgConfig->transitionType;

    // 6) Prepare to handle direction outside the loop if possible
    int forwardDirection = (chParam->revDirCh_u8 == 0);

    // 7) The main loop
    //    If you need maximum speed, consider restricting these to integer arithmetic
    //    or combining steps carefully.
    for (int i = 0; i < EXAMPLE_LED_NUMBERS; i++)
    {
        // Instead of doing `ceil(offset + i*scaleSpacing)` with a math-lib call:
        float fPos = offset + (i * scaleSpacing);
        // Replicate `ceil()` via an integer cast trick.  E.g.
        // ic_position = (int)(fPos + 0.999999f) if we know fPos >= 0.
        // Or do it more precisely:
        int ic_position = (int)fPos;
        if (fPos - ic_position > 0.0f) {
            ic_position++;  // emulate ceil
        }

        float fImageIndex       = ic_position * invSegmentLength;
        int   floorImageIndex   = (int)fImageIndex;     // floorf() is free with cast if fImageIndex >= 0
        float fractional        = fImageIndex - floorImageIndex; // should be in [0,1)
        int   TempImageIndex    = floorImageIndex % numColors;

        int segmentPosition     = (int)(fractional * segmentLength);

        // 8) Determine color based on transition type
        uint16_t red2   = 0;
        uint16_t green2 = 0;
        uint16_t blue2  = 0;
        float    brightness_temp1 = 0.0f;

        if (transitionType == 0) {
            // No transition - just discrete color or padding
            if (segmentPosition < imgConfig->colorLength) {
            	if(fill_data == 0)
            	{
					hsv_to_rgb_16bit(colorSelections[TempImageIndex].hue,
									 colorSelections[TempImageIndex].saturation,
									 100, &red2, &green2, &blue2);
					brightness_temp1 = colorSelections[TempImageIndex].brightness * brightness_factor;
            	}
            	else
            	if(fill_data == 1)
            	{
					rampData[Chan].hue_start[i] = colorSelections[TempImageIndex].hue;
					rampData[Chan].sat_start[i] = colorSelections[TempImageIndex].saturation;
					rampData[Chan].val_start[i] = colorSelections[TempImageIndex].brightness;
            	}
            	else
            	if(fill_data == 2)
            	{
					rampData[Chan].hue_end[i] = colorSelections[TempImageIndex].hue;
					rampData[Chan].sat_end[i] = colorSelections[TempImageIndex].saturation;
					rampData[Chan].val_end[i] = colorSelections[TempImageIndex].brightness;
            	}
            } else {
            	if(fill_data == 0)
            	{
					hsv_to_rgb_16bit(imgConfig->paddingColor.hue,
									 imgConfig->paddingColor.saturation,
									 100, &red2, &green2, &blue2);
					brightness_temp1 = imgConfig->paddingColor.brightness * brightness_factor;
            	}
            	else
            	if(fill_data == 1)
            	{
					rampData[Chan].hue_start[i] = imgConfig->paddingColor.hue;
					rampData[Chan].sat_start[i] = imgConfig->paddingColor.saturation;
					rampData[Chan].val_start[i] = imgConfig->paddingColor.brightness;
            	}
            	else
            	if(fill_data == 2)
            	{
					rampData[Chan].hue_end[i] = imgConfig->paddingColor.hue;
					rampData[Chan].sat_end[i] = imgConfig->paddingColor.saturation;
					rampData[Chan].val_end[i] = imgConfig->paddingColor.brightness;
            	}
            }
        } else {
            // transitionType != 0 => do the gradient
            // fractional index within the segment:
            // originally: ModeimageIndex = fmod(imageIndex,1)
            // but we already have 'fractional' == fractional part of imageIndex
            float output = fractional * (segmentLength / (segmentLength - ledspacingFactor));

            if (output > 1.0f) {
                output = 1.0f;
            }

            Color start, end, result;
            // start color
            start.hue        = colorSelections[TempImageIndex].hue;
            start.saturation = colorSelections[TempImageIndex].saturation * 0.01f;
            start.brightness = (colorSelections[TempImageIndex].brightness * brightness_factor) * 0.01f;
            // end color
            end.hue        = imgConfig->paddingColor.hue;
            end.saturation = imgConfig->paddingColor.saturation * 0.01f;
            end.brightness = (imgConfig->paddingColor.brightness * brightness_factor) * 0.01f;

            // interpolate
            interpolate_color(&result, &start, &end, output);

            // convert result back to "0..100" range for hsv_to_rgb_16bit
            float brightnessScaled = result.brightness * 100.0f;
            float saturationScaled = result.saturation * 100.0f;

        	if(fill_data == 0)
        	{
				hsv_to_rgb_16bit(result.hue, saturationScaled, 100, &red2, &green2, &blue2);
				brightness_temp1 = brightnessScaled;
        	}
        	else
        	if(fill_data == 1)
        	{
				rampData[Chan].hue_start[i] = result.hue;
				rampData[Chan].sat_start[i] = saturationScaled;
				rampData[Chan].val_start[i] = brightnessScaled;
        	}
        	else
        	if(fill_data == 2)
        	{
				rampData[Chan].hue_end[i] = result.hue;
				rampData[Chan].sat_end[i] = saturationScaled;
				rampData[Chan].val_end[i] = brightnessScaled;
        	}
        }

    	if(fill_data == 0)
    	{

			// 9) Final brightness scaling
			restrict_and_scale_RGB(&red2, &green2, &blue2, brightness_temp1);

			// 10) Determine the actual LED index if reversed
			int led_index;
			if (forwardDirection) {
				led_index = i;
			} else {
				// If the total number of relevant LEDs is Number_of_LED_int,
				// and the physical buffer is EXAMPLE_LED_NUMBERS,
				// replicate your original logic:
				if (i < chParam->SetLEDstripal_u16) {
					led_index = (chParam->SetLEDstripal_u16 - 1) - i;
				} else {
					led_index = (EXAMPLE_LED_NUMBERS - 1) - (i - chParam->SetLEDstripal_u16);
				}
			}

			// 11) Write the color into the "data_channels1_X" buffer inside set_led_color
			//     (Wherever your final LED data actually goes.)
			set_led_color((uint8_t)(Chan + 1),
						  (uint16_t)(led_index + 1),
						  red2, green2, blue2);
    	}
    }

	if(fill_data == 0)
	{
		// 12) Do one memcpy at the end
		switch(Chan) {
			case 0:
				memcpy(&data_channel[0], &data_channels1_1[0], EXAMPLE_LED_NUMBERS * 3 * 2);
				break;
			case 1:
				memcpy(&data_channel[0], &data_channels1_2[0], EXAMPLE_LED_NUMBERS * 3 * 2);
				break;
			case 2:
				memcpy(&data_channel[0], &data_channels1_3[0], EXAMPLE_LED_NUMBERS * 3 * 2);
				break;
			case 3:
				memcpy(&data_channel[0], &data_channels1_4[0], EXAMPLE_LED_NUMBERS * 3 * 2);
				break;
			default:
				break;
		}
	}
}

void PrepareDataWithModeSetting(int offset, int Chan, int Start_Offset_Flag)
{
    int ModeSetting = sAddressableStipArr[IC_Type_Var].u8ModeSetting;
//    int Number_of_LED_int = ChannelParamObject[Chan].SetLEDstripal_u16;
    int Number_of_LED_int = EXAMPLE_LED_NUMBERS;

    int i = Chan;
    uint16_t* data_channel = (use_ping_buffer[Chan]) ? data_channels_ping[Chan] : data_channels[Chan];

    {
        if((sAddressableStipArr[IC_Type_Var].u8NumberOfbits) == 1)
        {
            if(ModeSetting != 0)
            {
            	if(i == 0)
            	{
					// Mode setting bytes
            		data_channel[0] = 0xFFFF;
            		data_channel[1] = 0xFFFF;
            		data_channel[2] = 0xFFFF;
            		data_channel[3] = 0x0000;
            		data_channel[4] = 0x0000;
            		data_channel[5] = 0x0000;
            	}
            	else if(i == 1)
            	{
					// Mode setting bytes
            		data_channel[0] = 0xFFFF;
            		data_channel[1] = 0xFFFF;
            		data_channel[2] = 0xFFFF;
            		data_channel[3] = 0x0000;
            		data_channel[4] = 0x0000;
            		data_channel[5] = 0x0000;
            	}
            	else if(i == 2)
            	{
					// Mode setting bytes
            		data_channel[0] = 0xFFFF;
            		data_channel[1] = 0xFFFF;
            		data_channel[2] = 0xFFFF;
            		data_channel[3] = 0x0000;
            		data_channel[4] = 0x0000;
            		data_channel[5] = 0x0000;
            	}
            	else if(i == 3)
            	{
					// Mode setting bytes
            		data_channel[0] = 0xFFFF;
            		data_channel[1] = 0xFFFF;
            		data_channel[2] = 0xFFFF;
            		data_channel[3] = 0x0000;
            		data_channel[4] = 0x0000;
            		data_channel[5] = 0x0000;
            	}
            }

        	float offset_temp123 = 0;
        	int offset_temp1 = 0;

        	if(Start_Offset_Flag == 1)
        	{
				offset_temp123 = (((ChannelParamObject[Chan].offsetCh_float) + (light_para.locationOffset_float)) *25.4)/ChannelParamObject[Chan].LEDspacingCh_float;
				offset_temp123 *= ChannelParamObject[Chan].scaleCh_float;

				offset_temp1 =(int) offset_temp123*3;

		        // If the channel runs in reverse, flip the spatial offset direction
				if (RippleStartFlag[Chan])
				{
			        bool rev =
			            (Chan == 0 && light_para.revDirCh1_u8 == 1) ||
			            (Chan == 1 && light_para.revDirCh2_u8 == 1) ||
			            (Chan == 2 && light_para.revDirCh3_u8 == 1) ||
			            (Chan == 3 && light_para.revDirCh4_u8 == 1);

			        if (rev) offset_temp1 = -offset_temp1;
				}
        	}

            int buffer_size = (Number_of_LED_int * 3 + MODE_SETTING); // Assuming all channels have the same buffer size

            // Calculate start and end position considering the negative offset
            int start_pos = ( ModeSetting + (offset * 3) + offset_temp1 + buffer_size*2) % buffer_size;

            if ( start_pos  == 0)
            {
                // No wrap around, copy directly
            	if(i==0)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_1[start_pos], Number_of_LED_int * 3*2);
            	}
            	else if(i==1)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_2[start_pos], Number_of_LED_int * 3*2);
            	}
            	else if(i==2)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_3[start_pos], Number_of_LED_int * 3*2);
            	}
            	else if(i==3)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_4[start_pos], Number_of_LED_int * 3*2);
            	}
            }
            else
            {
                // Handle wrap around by copying in two parts
                int first_part_size = buffer_size - start_pos;

                if(i==0)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_1[start_pos], first_part_size*2);
            	}
            	else if(i==1)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_2[start_pos], first_part_size*2);
            	}
            	else if(i==2)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_3[start_pos], first_part_size*2);
            	}
            	else if(i==3)
            	{
            		memcpy(&data_channel[ ModeSetting], &data_channels1_4[start_pos], first_part_size*2);
            	}

                int second_part_size = (Number_of_LED_int * 3) - first_part_size;

            	if(i==0)
            	{
            		memcpy(&data_channel[ ModeSetting + (first_part_size )], &data_channels1_1[0], second_part_size*2);
            	}
            	else if(i==1)
            	{
            		memcpy(&data_channel[ ModeSetting + (first_part_size )], &data_channels1_2[0], second_part_size*2);
            	}
            	else if(i==2)
            	{
            		memcpy(&data_channel[ ModeSetting + (first_part_size )], &data_channels1_3[0], second_part_size*2);
            	}
            	else if(i==3)
            	{
            		memcpy(&data_channel[ ModeSetting + (first_part_size )], &data_channels1_4[0], second_part_size*2);
            	}
            }
        }
    }
}

inline void hsv_to_rgb_16bit(float h, float s, float v, uint16_t *r, uint16_t *g, uint16_t *b)
{
    // Pre-scale S and V just once
    float s_f = s * 0.01f;     // Instead of s/100
    float v_f = v * 0.01f;     // Instead of v/100

    // If saturation is near zero => gray
    if (s_f <= 0.0f) {
        uint16_t gray = (uint16_t)(v_f * 65535.0f);
        *r = *g = *b = gray;
        return;
    }

    // Ensure hue is in [0..360)
    if (h >= 360.0f) {
        // If you want to wrap around, do: h = fmodf(h, 360.0f);
        h = 0.0f;
    }

    // Convert hue to sector 0..5
    float sector = h * (1.0f / 60.0f);  // = h / 60
    long  i      = (long)sector;
    float f      = sector - i;         // fractional part

    float p = v_f * (1.0f - s_f);
    float q = v_f * (1.0f - (s_f * f));
    float t = v_f * (1.0f - (s_f * (1.0f - f)));

    // Multiply once at the end, or multiply now. 
    // We'll do it now to match the original code:
    float scale = 65535.0f;

    switch (i) {
        case 0:
            *r = (uint16_t)(v_f * scale);
            *g = (uint16_t)(t   * scale);
            *b = (uint16_t)(p   * scale);
            break;
        case 1:
            *r = (uint16_t)(q   * scale);
            *g = (uint16_t)(v_f * scale);
            *b = (uint16_t)(p   * scale);
            break;
        case 2:
            *r = (uint16_t)(p   * scale);
            *g = (uint16_t)(v_f * scale);
            *b = (uint16_t)(t   * scale);
            break;
        case 3:
            *r = (uint16_t)(p   * scale);
            *g = (uint16_t)(q   * scale);
            *b = (uint16_t)(v_f * scale);
            break;
        case 4:
            *r = (uint16_t)(t   * scale);
            *g = (uint16_t)(p   * scale);
            *b = (uint16_t)(v_f * scale);
            break;
        default:  // case 5:
            *r = (uint16_t)(v_f * scale);
            *g = (uint16_t)(p   * scale);
            *b = (uint16_t)(q   * scale);
            break;
    }
}

// Helper function to interpolate between two float values
static inline float interpolate(float start, float end, float t) {
    return start + (end - start) * t;
}

// Function to handle hue interpolation specifically, taking the shortest path on the hue circle
static inline float interpolate_hue(float h1, float h2, float t)
{
    // Calculate the difference between the two hues
    float diff = h2 - h1;

    // Check if the absolute difference is greater than 180 degrees
    if (fabs(diff) > 180)
    {
        // Determine the direction to interpolate
        if (h2 > h1)
        {
            // If h2 is greater, adjust h1 upwards by 360 degrees to ensure
            // the interpolation takes the shorter path counter-clockwise
            h1 += 360;
        }
        else
        {
            // If h1 is greater, adjust h2 upwards by 360 degrees to ensure
            // the interpolation takes the shorter path clockwise
            h2 += 360;
        }
    }

    // Interpolate the adjusted hue values
    float hue = interpolate(h1, h2, t);

    // Ensure the interpolated hue value wraps around at 360 degrees,
    // keeping it within the standard hue range (0-360 degrees)
    // Use modulus to correct for any overshoot beyond 360 degrees
    hue = fmod(hue, 360);

    // Return the calculated hue, which is the shortest path interpolation result
    return hue;
}

// Main function to interpolate colors, optimized for precision with floating points
void interpolate_color(Color *result, const Color *start, const Color *end, float t)
{
//    if (start->brightness < 0.04)
    if (start->brightness <= 0.0)
    //if (start->brightness < 0.15)
    {
        // Start is black
        result->hue = end->hue;
        result->saturation = end->saturation;
        result->brightness = interpolate(0, end->brightness, t);
    }
    else if (end->brightness  <= 0.00)
    //else if (end->brightness  < 0.15)
    {
        // End is black
        result->hue = start->hue;
        result->saturation = start->saturation;
        result->brightness = interpolate(start->brightness, 0, t);
    }
    else if (start->saturation  <= 0.00 )
    //else if (start->saturation  < 0.15 )
    {
        // Start is white
        result->hue = end->hue;
        result->saturation = interpolate(0, end->saturation, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
    else if (end->saturation  <= 0.00)
    //else if (end->saturation  < 0.15)
    {
        // End is white
        result->hue = start->hue;
        result->saturation = interpolate(start->saturation, 0, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
    else
    {
        // Normal case: interpolate each component
        result->hue = interpolate_hue(start->hue, end->hue, t);
        result->saturation = interpolate(start->saturation, end->saturation, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
}

void initializeMultiColorSparkleParameters(int numChannels)
{

	MultiColorSparkleParamObject_start = (MultiColorSparkleParameters*)heap_caps_malloc(numChannels * sizeof(MultiColorSparkleParameters), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (MultiColorSparkleParamObject_start == NULL)
    {
//    	printf("MultiColorSparkleParamObject_start 1\n");
        fprintf(stderr, "Failed to allocate memory for MultiColorSparkleParamObject_start array\n");
        exit(1);
    }

    // Initialize each channel's sub-arrays
    for (int i = 0; i < numChannels; i++)
    {
    	MultiColorSparkleParamObject_start[i].u64CurrentTime = (uint64_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (MultiColorSparkleParamObject_start[i].u64CurrentTime == NULL)
        {
//        	printf("MultiColorSparkleParamObject_start 2\n");
            fprintf(stderr, "Failed to allocate MultiColor memory 1 for channel %d\n", i);
            exit(1);
        }

    	MultiColorSparkleParamObject_start[i].u8ColorNum = (uint8_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (MultiColorSparkleParamObject_start[i].u8ColorNum == NULL)
        {
//        	printf("MultiColorSparkleParamObject_start 3\n");
            fprintf(stderr, "Failed to allocate MultiColor memory 2 for channel %d\n", i);
            exit(1);
        }

        // Initialize values, setting `factor` to zero and `u64CurrentTime` to zero
        for (int j = 0; j < EXAMPLE_LED_NUMBERS; j++)
        {
        	MultiColorSparkleParamObject_start[i].u64CurrentTime[j] = 0;
        	MultiColorSparkleParamObject_start[i].u8ColorNum[j] = 0;
        }

        // Initialize other parameters to default values
        for (int j = 0; j < 3; j++)
        {
        	MultiColorSparkleParamObject_start[i].MultiColor1_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor2_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor3_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor4_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor5_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor6_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor7_uint16[j] = 0;
        	MultiColorSparkleParamObject_start[i].MultiColor8_uint16[j] = 0;

        	MultiColorSparkleParamObject_start[i].EndColor_uint16[j] = 0;
        }
        MultiColorSparkleParamObject_start[i].Intensity = 0.0f;
        MultiColorSparkleParamObject_start[i].Width = 0.0f;
        MultiColorSparkleParamObject_start[i].Decaytime = 0.0f;
        MultiColorSparkleParamObject_start[i].u64RandomGenTime = 0;
        MultiColorSparkleParamObject_start[i].numColors = 0;
    }

    // For second
    MultiColorSparkleParamObject_end = (MultiColorSparkleParameters*)heap_caps_malloc(numChannels * sizeof(MultiColorSparkleParameters), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (MultiColorSparkleParamObject_end == NULL)
    {
//    	printf("MultiColorSparkleParamObject_start 4\n");
        fprintf(stderr, "Failed to allocate memory for MultiColorSparkleParamObject_end array\n");
        exit(1);
    }

    // Initialize each channel's sub-arrays
    for (int i = 0; i < numChannels; i++)
    {
    	MultiColorSparkleParamObject_end[i].u64CurrentTime = (uint64_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (MultiColorSparkleParamObject_end[i].u64CurrentTime == NULL)
        {
//        	printf("MultiColorSparkleParamObject_start 5\n");
            fprintf(stderr, "Failed to allocate MultiColor memory 11 for channel %d\n", i);
            exit(1);
        }

    	MultiColorSparkleParamObject_end[i].u8ColorNum = (uint8_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint8_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (MultiColorSparkleParamObject_end[i].u8ColorNum == NULL)
        {
//        	printf("MultiColorSparkleParamObject_start 6\n");
            fprintf(stderr, "Failed to allocate MultiColor memory 12 for channel %d\n", i);
            exit(1);
        }

        // Initialize values, setting `factor` to zero and `u64CurrentTime` to zero
        for (int j = 0; j < EXAMPLE_LED_NUMBERS; j++)
        {
        	MultiColorSparkleParamObject_end[i].u64CurrentTime[j] = 0;
        	MultiColorSparkleParamObject_end[i].u8ColorNum[j] = 0;
        }

        // Initialize other parameters to default values
        for (int j = 0; j < 3; j++)
        {
        	MultiColorSparkleParamObject_end[i].MultiColor1_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor2_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor3_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor4_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor5_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor6_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor7_uint16[j] = 0;
        	MultiColorSparkleParamObject_end[i].MultiColor8_uint16[j] = 0;

        	MultiColorSparkleParamObject_end[i].EndColor_uint16[j] = 0;
        }
        MultiColorSparkleParamObject_end[i].Intensity = 0.0f;
        MultiColorSparkleParamObject_end[i].Width = 0.0f;
        MultiColorSparkleParamObject_end[i].Decaytime = 0.0f;
        MultiColorSparkleParamObject_end[i].u64RandomGenTime = 0;
        MultiColorSparkleParamObject_end[i].numColors = 0;
    }
}

// Free allocated memory to prevent memory leaks
void freeMultiColorSparkleParameters()
{
    if (MultiColorSparkleParamObject_start != NULL)
    {
        for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
        {
        	if(MultiColorSparkleParamObject_start[i].u64CurrentTime!=NULL)
        	{
        		heap_caps_free(MultiColorSparkleParamObject_start[i].u64CurrentTime);
        		MultiColorSparkleParamObject_start[i].u64CurrentTime = NULL;
        	}

        	if(MultiColorSparkleParamObject_start[i].u8ColorNum!=NULL)
        	{
        		heap_caps_free(MultiColorSparkleParamObject_start[i].u8ColorNum);
        		MultiColorSparkleParamObject_start[i].u8ColorNum = NULL;
        	}
        }
		if(MultiColorSparkleParamObject_start!=NULL)
		{
			heap_caps_free(MultiColorSparkleParamObject_start);
			MultiColorSparkleParamObject_start = NULL;
		}
    }

    //For second
    if (MultiColorSparkleParamObject_end != NULL)
    {
        for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
        {
        	if(MultiColorSparkleParamObject_end[i].u64CurrentTime!=NULL)
        	{
        		heap_caps_free(MultiColorSparkleParamObject_end[i].u64CurrentTime);
        		MultiColorSparkleParamObject_end[i].u64CurrentTime = NULL;
        	}

        	if(MultiColorSparkleParamObject_end[i].u8ColorNum!=NULL)
        	{
        		heap_caps_free(MultiColorSparkleParamObject_end[i].u8ColorNum);
        		MultiColorSparkleParamObject_end[i].u8ColorNum = NULL;
        	}
        }
		if(MultiColorSparkleParamObject_end!=NULL)
		{
			heap_caps_free(MultiColorSparkleParamObject_end);
			MultiColorSparkleParamObject_end = NULL;
		}
    }
}

//void PSRAM_PLACE initializeSparkleParameters(int numChannels)
void  initializeSparkleParameters(int numChannels)
{
    SparkleParamObject_start = (SparkleParameters*)heap_caps_malloc(numChannels * sizeof(SparkleParameters), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (SparkleParamObject_start == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for SparkleParamObject_start array\n");
        exit(1);
    }

    // Initialize each channel's sub-arrays
    for (int i = 0; i < numChannels; i++)
    {
        SparkleParamObject_start[i].u64CurrentTime = (uint64_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (SparkleParamObject_start[i].u64CurrentTime == NULL)
        {
            fprintf(stderr, "Failed to allocate memory for channel %d\n", i);
            exit(1);
        }

        // Initialize values, setting `factor` to zero and `u64CurrentTime` to zero
        for (int j = 0; j < EXAMPLE_LED_NUMBERS; j++)
        {
            SparkleParamObject_start[i].u64CurrentTime[j] = 0;
        }

        // Initialize other parameters to default values
        for (int j = 0; j < 3; j++)
        {
            SparkleParamObject_start[i].StartColor_float[j] = 0.0f;
            SparkleParamObject_start[i].EndColor_float[j] = 0.0f;
        }
        SparkleParamObject_start[i].Intensity = 0.0f;
        SparkleParamObject_start[i].Width = 0.0f;
        SparkleParamObject_start[i].Decaytime = 0.0f;
        SparkleParamObject_start[i].u64RandomGenTime = 0;
    }

    // For second
    SparkleParamObject_end = (SparkleParameters*)heap_caps_malloc(numChannels * sizeof(SparkleParameters), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (SparkleParamObject_end == NULL)
    {
        fprintf(stderr, "Failed to allocate memory for SparkleParamObject_end array\n");
        exit(1);
    }

    // Initialize each channel's sub-arrays
    for (int i = 0; i < numChannels; i++)
    {
        SparkleParamObject_end[i].u64CurrentTime = (uint64_t *)heap_caps_malloc(EXAMPLE_LED_NUMBERS * sizeof(uint64_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (SparkleParamObject_end[i].u64CurrentTime == NULL)
        {
            fprintf(stderr, "Failed to allocate memory for channel %d\n", i);
            exit(1);
        }

        // Initialize values, setting `factor` to zero and `u64CurrentTime` to zero
        for (int j = 0; j < EXAMPLE_LED_NUMBERS; j++)
        {
            SparkleParamObject_end[i].u64CurrentTime[j] = 0;
        }

        // Initialize other parameters to default values
        for (int j = 0; j < 3; j++)
        {
            SparkleParamObject_end[i].StartColor_float[j] = 0.0f;
            SparkleParamObject_end[i].EndColor_float[j] = 0.0f;
        }
        SparkleParamObject_end[i].Intensity = 0.0f;
        SparkleParamObject_end[i].Width = 0.0f;
        SparkleParamObject_end[i].Decaytime = 0.0f;
        SparkleParamObject_end[i].u64RandomGenTime = 0;
    }
}

// Free allocated memory to prevent memory leaks
void freeSparkleParameters()
{
    if (SparkleParamObject_start != NULL)
    {
        for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
        {
        	if(SparkleParamObject_start[i].u64CurrentTime!=NULL)
        	{
        		heap_caps_free(SparkleParamObject_start[i].u64CurrentTime);
        		SparkleParamObject_start[i].u64CurrentTime = NULL;
        	}
        }
		if(SparkleParamObject_start!=NULL)
		{
			heap_caps_free(SparkleParamObject_start);
			SparkleParamObject_start = NULL;
		}
    }

    //For second
    if (SparkleParamObject_end != NULL)
    {
        for (int i = 0; i < NUMBER_OF_CHANNELS; i++)
        {
        	if(SparkleParamObject_end[i].u64CurrentTime!=NULL)
        	{
        		heap_caps_free(SparkleParamObject_end[i].u64CurrentTime);
        		SparkleParamObject_end[i].u64CurrentTime = NULL;
        	}
        }
		if(SparkleParamObject_end!=NULL)
		{
			heap_caps_free(SparkleParamObject_end);
			SparkleParamObject_end = NULL;
		}
    }
}

static inline float getLastCommandBrightness(int channel)
{
    #ifdef ENABLE_PRINT_MSG
    printf("Getting last command brightness for channel: %d\n", channel);
    #endif
    return light_LastCommandPara[channel - 1].brightness;
}

static inline float getLastCommandDuration(int channel)
{
    #ifdef ENABLE_PRINT_MSG
    printf("Getting last command duration for channel: %d\n", channel);
    #endif
    return light_LastCommandPara[channel - 1].duration;
}

static inline char* getLastCommandSource(int channel)
{
    #ifdef ENABLE_PRINT_MSG
    printf("Getting last command source for channel: %d\n", channel);
    #endif
    return light_LastCommandPara[channel - 1].source;
}

static inline char* getLastCommandFunction(int channel)
{
    #ifdef ENABLE_PRINT_MSG
    printf("Getting last command function for channel: %d\n", channel);
    #endif
    return light_LastCommandPara[channel - 1].function;
}

static inline cJSON* getLastCommandConfig(int channel)
{
    #ifdef ENABLE_PRINT_MSG
    printf("Getting last command CONFIG for channel: %d\n", channel);
    #endif
    return light_LastCommandPara[channel - 1].config ? cJSON_Duplicate(light_LastCommandPara[channel - 1].config, 1) : NULL;
}

static void loadFromLastCommand(int channel, float *brightness, float *duration, char **source, char **function, cJSON **configItem) {
    #ifdef ENABLE_PRINT_MSG
    printf("Loading from the last command for channel: %d\n", channel);
    #endif
    if (*brightness == 0) {
        *brightness = getLastCommandBrightness(channel);
        #ifdef ENABLE_PRINT_MSG
        printf("Loaded brightness: %f\n", *brightness);
        #endif
    }
    if (*duration == 0) {
        *duration = getLastCommandDuration(channel);
        #ifdef ENABLE_PRINT_MSG
        printf("Loaded duration: %f\n", *duration);
        #endif
    }

    if (*source == NULL) {
        *source = getLastCommandSource(channel);
        #ifdef ENABLE_PRINT_MSG
//        printf("Loaded source: %s\n", *source);
//        printf("Loaded source: \n");
        #endif
    }

    if (*function == NULL) {
        *function = getLastCommandFunction(channel);
        #ifdef ENABLE_PRINT_MSG
//        printf("Loaded function: %s\n", *function);
//        printf("Loaded function: \n");
        #endif
    }
    if (*configItem == NULL) {
        *configItem = getLastCommandConfig(channel);
        #ifdef ENABLE_PRINT_MSG
//        printf("Loaded CONFIG: %s\n", cJSON_Print(*configItem));
//        printf("Loaded CONFIG: \n");
        #endif
    }
}

static void saveLastCommand(AMessage_st* s_Message_Rx, int channel, float brightness, float duration, char *source, char *function, cJSON *configItem, char *changeReason, uint64_t timechange, char state1, int optionalChangeID) {
    #ifdef ENABLE_PRINT_MSG
    printf("Saving the last command for channel: %d\n", channel);
    #endif

    LastCommand_t *lastCommand = &light_LastCommandPara[channel - 1];

    if (lastCommand->payload == NULL) {
        lastCommand->payload = (char *)heap_caps_malloc(MAX_PAYLOAD_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!lastCommand->payload) {
            #ifdef ENABLE_PRINT_MSG
            printf("Memory allocation failed for payload.\n");
            #endif
            return;
        }
    }

    cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);
    if (json == NULL) {
        #ifdef ENABLE_PRINT_MSG
        printf("Invalid JSON input.\n");
        #endif
        return;
    }

    cJSON *chArray = cJSON_CreateArray();
    cJSON_AddItemToArray(chArray, cJSON_CreateNumber(channel));
    cJSON_ReplaceItemInObject(json, "ch", chArray);

	memset(payLoadData,0,sizeof(payLoadData));//\0';
	
	cJSON_PrintPreallocated(json, payLoadData, sizeof(payLoadData), false);
    strncpy(lastCommand->payload, payLoadData, MAX_PAYLOAD_SIZE);
    lastCommand->payload[MAX_PAYLOAD_SIZE - 1] = '\0'; // Ensure null-terminated string

    cJSON_Delete(json);

    #ifdef ENABLE_PRINT_MSG
    printf("Payload saved: %s\n", lastCommand->payload);
    #endif

    lastCommand->brightness = brightness;
    lastCommand->duration = duration;
    lastCommand->source = source ? strdup(source) : NULL;
    lastCommand->function = function ? strdup(function) : NULL;

    if (lastCommand->config) {
        cJSON_Delete(lastCommand->config);  // Free previous memory
    }

    lastCommand->config = (cJSON_Duplicate(configItem, 1));// ? cJSON_Duplicate(configItem, 1) : NULL;

    if(Power_Cycle[channel-1] == 1)
    {
    	strncpy(lastCommand->changeReason, "power Cycle", MAX_CMD_LEN);
    	lastCommand->timeChanged = get_current_time_ms();
    	Power_Cycle[channel-1] = 0;
    }
    else
    {
    	strncpy(lastCommand->changeReason, (char*)changeReason, MAX_CMD_LEN);

    	if(optionalChangeID != 0)
    	{
    		lastCommand->optionalChangeID = optionalChangeID;
    	}
    	else
    	{
			if((!strcmp(lastCommand->changeReason, "EVENT_ACTOR")))
			{
				strcpy(lastCommand->changeReason,"executeScene");
			}

			if((!strcmp(lastCommand->changeReason, "IHUB")))
			{
				strcpy(lastCommand->changeReason,"executeCommand");
			}
    	}
    	lastCommand->timeChanged = timechange;
    }
    lastCommand->state =  state1;

//    char *changeReason, uint64_t timechange,
#ifdef ENABLE_PRINT_MSG
    printf("source = %s \n", (char*)lastCommand->changeReason);
    printf("timeChanged = %llu \n", lastCommand->timeChanged);
#endif

    #ifdef ENABLE_PRINT_MSG
//    printf("Brightness saved: %f, Duration saved: %f, Function saved: %s, CONFIG saved: %s\n",
//           brightness, duration, lastCommand->function, cJSON_Print(lastCommand->config));
    #endif

    Last_CommandFlag[channel - 1] = 2;
    setLastCommand((channel-1), 1);
}

static int trim_On(AMessage_st* s_Message_Rx)
{
#ifdef ENABLE_PRINT_MSG
    printf("Parsing JSON payload...\n");
#endif

    cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);

    if (json == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Invalid JSON input.\n");
#endif
        Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
        return -1;
    }

#ifdef ENABLE_PRINT_MSG
    printf("Extracting 'CH' array from JSON...\n");
#endif

    cJSON *channelArray = cJSON_GetObjectItem(json, "CH");

    if (channelArray == NULL || !cJSON_IsArray(channelArray) || cJSON_GetArraySize(channelArray) == 0) {
#ifdef ENABLE_PRINT_MSG
        printf("Error: Invalid or missing 'CH' array in JSON 13.\n");
#endif
        Add_Response_msg("Error: Invalid or missing 'CH' array in JSON 13.", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

    if (cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == 0) {
#ifdef ENABLE_PRINT_MSG
        printf("Invalid channel.\n");
#endif
        Add_Response_msg("Invalid channel", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

    float brightness = 0;
    float duration = 0;
    char *source = NULL;
    char *function = NULL;
    uint64_t timechange = 0;
    char *changeReason = NULL;
    char state1 = 1;

	int eventIdV = 0;
	int DEFEReventIdV = 0;
	int optionalChangeID = 0;

#ifdef ENABLE_PRINT_MSG
    printf("Extracting optional parameters from JSON...\n");
#endif

    cJSON *brightnessItem = cJSON_GetObjectItem(json, "BRIGHTNESS");
    cJSON *durationItem = cJSON_GetObjectItem(json, "DURATION");
    cJSON *sourceItem = cJSON_GetObjectItem(json, "SOURCE");
    cJSON *functionItem = cJSON_GetObjectItem(json, "FUNCTION");
    cJSON *configItem = cJSON_GetObjectItem(json, "CONFIG");

    cJSON *brightnessIndexItem = cJSON_GetObjectItem(json, "BRIGHTNESSINDEX");

	cJSON *DEFEReventIdItem = cJSON_GetObjectItem(json, "DEFER_eventId");
	if (DEFEReventIdItem == NULL)
	{
		DEFEReventIdV = 0;
	}
	else
	{
		DEFEReventIdV = DEFEReventIdItem->valueint;
	}

	cJSON *eventIdItem = cJSON_GetObjectItem(json, "eventId");
	if (eventIdItem == NULL)
	{
		eventIdV = 0;
	}
	else
	{
		eventIdV = eventIdItem->valueint;
	}

	cJSON *RampTimeSceneItem = cJSON_GetObjectItem(json, "RampTime");

//	cJSON *DwellTimeSceneItem = cJSON_GetObjectItem(json, "DwellTimeScene");	//Duration

    timechange = get_current_time_ms();

	if(DEFEReventIdV != 0)
	{
		changeReason = "DEFER_eventId";
		optionalChangeID = DEFEReventIdV;
	}
	else if(eventIdV != 0)
	{
		changeReason = "eventId";
		optionalChangeID = eventIdV;
	}
    else
    {
    	changeReason = (char*)s_Message_Rx->src_Actor_a8;
    }

    if (brightnessItem != NULL && cJSON_IsNumber(brightnessItem)) {
        brightness = brightnessItem->valuedouble;
#ifdef ENABLE_PRINT_MSG
        printf("Brightness extracted: %f\n", brightness);
#endif
    } else {
        brightnessItem = NULL;
    }

    if (brightnessIndexItem != NULL && cJSON_IsNumber(brightnessIndexItem))
    {
    	if(brightness == 0)
    	{
    		brightness = brightnessIndexItem->valuedouble;

			if(brightness > 10)
			{
				brightness = 10;
			}
			else if(brightness < 0)
			{
				brightness = 0;
			}

    		brightness = brightness*10;
    	}

#ifdef ENABLE_PRINT_MSG
        printf("Brightness extracted: %f\n", brightness);
#endif
    } else {
    	brightnessIndexItem = NULL;
    }

    if (durationItem != NULL && cJSON_IsNumber(durationItem)) {
        duration = durationItem->valuedouble;
#ifdef ENABLE_PRINT_MSG
        printf("Duration extracted: %f\n", duration);
#endif
    } else {
        durationItem = NULL;
    }

    if (sourceItem != NULL && cJSON_IsString(sourceItem) && sourceItem->valuestring != NULL) {
    	source = sourceItem->valuestring;
#ifdef ENABLE_PRINT_MSG
        printf("Source extracted: %s\n", source);
#endif
    } else {
    	sourceItem = NULL;
    }

    if (functionItem != NULL && cJSON_IsString(functionItem) && functionItem->valuestring != NULL) {
        function = functionItem->valuestring;
#ifdef ENABLE_PRINT_MSG
        printf("Function extracted: %s\n", function);
#endif
    } else {
        functionItem = NULL;
    }

    if (configItem != NULL && !cJSON_IsObject(configItem)) {
#ifdef ENABLE_PRINT_MSG
        printf("Config item is not a valid JSON object.\n");
#endif
        configItem = NULL;
    }

    int useAllChannels = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == -1;
#ifdef ENABLE_PRINT_MSG
    printf("Use all channels: %d\n", useAllChannels);
#endif

    for (int j = 0; j < (useAllChannels ? NUMBER_OF_CHANNELS : cJSON_GetArraySize(channelArray)); j++) {
        int channel = useAllChannels ? j + 1 : cJSON_GetArrayItem(channelArray, j)->valueint;
#ifdef ENABLE_PRINT_MSG
        printf("Processing channel: %d\n", channel);
#endif

        if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
        {
        	if (RampTimeSceneItem == NULL)
        	{
        		rampData[channel-1].RampTimeSceneVal = 0;
        	}
        	else
        	{
        		rampData[channel-1].RampTimeSceneVal = RampTimeSceneItem->valueint;					//In second
        		rampData[channel-1].RampTimeSceneVal = (rampData[channel-1].RampTimeSceneVal)*1000; //In mili Second
        	}

        	if( (duration == 0) && (rampData[channel-1].RampTimeSceneVal == 0) )
        	{
        		rampData[channel-1].DwellTimeSceneVal = 0;

        		if (functionItem != NULL && cJSON_IsString(functionItem) && functionItem->valuestring != NULL) {
        		strcpy(rampData[channel-1].function_start, function);
        		}
            	rampData[channel-1].RampStartTime = 0;
            	ExecuteSceneRampFlag[channel-1] = 0;
        	}
        	else if(rampData[channel-1].RampTimeSceneVal != 0)
        	{
        		rampData[channel-1].DwellTimeSceneVal = duration;					//In second
        		rampData[channel-1].DwellTimeSceneVal = (rampData[channel-1].DwellTimeSceneVal)*1000; 	//In mili Second
        		if (functionItem != NULL && cJSON_IsString(functionItem) && functionItem->valuestring != NULL) {
				strcpy(rampData[channel-1].function_end, function);
        		}
        	}

#ifdef ENABLE_PRINT_MSG
        	printf("RampTimeSceneV = %ld, DwellTimeSceneV = %ld, function_start = %s, function_end = %s \n", rampData[channel-1].RampTimeSceneVal, rampData[channel-1].DwellTimeSceneVal, rampData[channel-1].function_start, rampData[channel-1].function_end );
#endif

			if(DEFEReventIdV != 0)
			{
				LastCommand_t *lastCommand = &light_LastCommandPara[channel - 1];
				strncpy(lastCommand->changeReason, (char*)changeReason, MAX_CMD_LEN);
				lastCommand->optionalChangeID = optionalChangeID;
			}
			else
			{
				if(processChannel(s_Message_Rx, channel, brightness, duration, source, function, configItem, changeReason, timechange,  brightnessItem, brightnessIndexItem, durationItem, sourceItem, functionItem, state1, optionalChangeID) != -1)
				{
					//return 1;
					flag_not_rmt = 0;
					delay_same_array = 1;
				}
				else
				{
					return -1;
				}
			}

        } else {
#ifdef ENABLE_PRINT_MSG
            printf("Error: Invalid channel (out of range).\n");
#endif
            Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
            cJSON_Delete(json);
            return -1;
        }
    }

	if((Power_up_counter_d2c == 0) || (Power_up_counter_d2c >= 4))
	{
#ifdef ENABLE_PRINT_MSG
		printf("Power_up_counter_d2c = %d \n", Power_up_counter_d2c);
#endif
		Power_up_counter_d2c = 0;
		{
			Send_D2C();		//Send D2C command
		}
	}

    cJSON_Delete(json);
    return 1;
}

static int processChannel(AMessage_st* s_Message_Rx, int channel, float brightness, float duration, char *source, char *function, cJSON *configItem, char *changeReason, uint64_t timechange, cJSON *brightnessItem, cJSON *brightnessIndexItem, cJSON *durationItem, cJSON *sourceItem, cJSON *functionItem, char state1, int optionalChangeID) {
#ifdef ENABLE_PRINT_MSG
    printf("Processing channel: %d\n", channel);
#endif

    if (((brightnessItem == NULL) && (brightnessIndexItem == NULL)) || durationItem == NULL || sourceItem == NULL || functionItem == NULL || configItem == NULL) {
#ifdef ENABLE_PRINT_MSG
       printf("Loading last command parameters\n");
#endif
        loadFromLastCommand(channel, &brightness, &duration, &source, &function, &configItem);
    }

#if defined(B527)
    if(channel == 1)
    {
    	gpio_set_level(LIGHT1, 1);  // turn LIGHT1 ON
    }

    else if(channel == 2)
    {
    	gpio_set_level(LIGHT2, 1);  // turn LIGHT1 ON
    }
#endif

    if (function != NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing function: %s\n", function);
#endif
        if(executeFunction(s_Message_Rx, channel, function, &brightness, configItem) == -1)
        {
        	return -1;
        }
    }

    if ((function != NULL) && (rampData[channel-1].RampTimeSceneVal == 0)) {
#ifdef ENABLE_PRINT_MSG
        printf("Saving the last command...\n");
#endif
        saveLastCommand(s_Message_Rx, channel, brightness, duration, source, function, configItem, changeReason, timechange, state1, optionalChangeID);
        Last_CommandFlag[channel - 1] = 2;
    } else if (function == NULL) {
        brightness_RunTimeChan[channel - 1] = brightness;
#ifdef ENABLE_PRINT_MSG
        printf("Updated runtime brightness: %f\n", brightness);
#endif
    }

    if( (duration != 0) && (rampData[channel-1].RampTimeSceneVal == 0) )
    {
   		duration_time[channel-1] = duration * 1000; // In milliseconds
        uint64_t u64CurrentTime = get_current_time_ms(); // Get current time in milliseconds
        duration_Start_time[channel-1] = u64CurrentTime;
#ifdef ENABLE_PRINT_MSG
        printf("Updated duration: %f, start time: %llu\n", duration, u64CurrentTime);
#endif
    }

    return 1;
}

static int executeFunction(AMessage_st* s_Message_Rx, int channel, char *function, float *brightness, cJSON *configItem) {
#ifdef ENABLE_PRINT_MSG
    printf("Executing function: %s\n", function);
#endif

    if ((!strcmp(function, "PATTERN")) || (!strcmp(function, "Pattern"))) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing PATTERN function...\n");
#endif
        if(parse_pattern_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_pattern_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }

    } else if ((!strcmp(function, "HUESAT")) || (!strcmp(function, "HSV"))) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing HUESAT function...\n");
#endif
        if(parse_huesat_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_huesat_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }

    }
#ifndef B542
    else if ((!strcmp(function, "SPARKLE")) || (!strcmp(function, "Sparkle"))) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing SPARKLE function...\n");
#endif
        if(parse_sparkle_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_sparkle_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
    else if ((!strcasecmp(function, "MULTICOLORSPARKLE"))) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing MultiColor SPARKLE function...\n");
#endif
        if(parse_multicolorsparkle_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_multicolorsparkle_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
    else if ((!strcasecmp(function, "RIPPLE")) || (!strcasecmp(function, "Wave")) ) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing RIPPLE function...\n");
#endif

        if(parse_ripple_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_ripple_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }

#endif
    else if ((!strcmp(function, "EXECUTECUSTOM")) || (!strcmp(function, "Custom")) || (!strcmp(function, "Cascade")) ) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing EXECUTECUSTOM function...\n");
#endif

        if(parse_executecustom_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_executecustom_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
    else if ((!strcasecmp(function, "RACING"))) {
#ifdef ENABLE_PRINT_MSG
        printf("Executing EXECUTECUSTOM function...\n");
#endif

        if(parse_executeracing_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_executeracing_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
    else if ((!strcasecmp(function, "SET_COLOR")) || (!strcasecmp(function, "PWM"))) {
#ifdef ENABLE_PRINT_MSG
        printf("In PWM \n");
#endif

        if(parse_setcolor_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_setcolor_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
#ifndef B542
    else if ((!strcmp(function, "TAPEMEASURE")) || (!strcmp(function, "tapeMeasure"))) {
#ifdef ENABLE_PRINT_MSG
        printf("In TAPEMEASURE \n");
#endif

        if(parse_tapemeasure_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_tapemeasure_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }

#endif
    else if ((!strcmp(function, "COLORINDEX")) || (!strcmp(function, "colorIndex"))) {
#ifdef ENABLE_PRINT_MSG
        printf("In colorIndex \n");
#endif

        if(parse_colorindex_template(s_Message_Rx, -1, *brightness, configItem) == -1)
        {
            return -1;
        }
        if(execute_colorindex_template(s_Message_Rx, channel) != -1)
        {
        	return 1;
        }
        else
        {
        	return -1;
        }
    }
#ifndef B542
	else if ((!strcmp(function, "MARQUEE")) || (!strcmp(function, "Marquee"))) {
	#ifdef ENABLE_PRINT_MSG
			printf("Executing MARQUEE function...\n");
	#endif

            if(parse_executemarquee_template(s_Message_Rx, -1, *brightness, configItem) == -1)
            {
                return -1;
            }
			if(execute_executemarquee_template(s_Message_Rx, channel) != -1)
			{
				return 1;
			}
			else
			{
				return -1;
			}
		}

#endif
    else {
#ifdef ENABLE_PRINT_MSG
        printf("Unknown function: %s\n", function);
#endif
    }
    return 1;
}

/* Fill channel arrays from command payload so the task can then call PrepareDataWithModeSetting. Called from CalculateDataBuffers. */
static void apply_playlist_command_to_channel(int Chan, const CommandEntry *command, float brightness_override)
{
    if (Chan < 0 || Chan >= NUMBER_OF_CHANNELS || command == NULL) {
        return;
    }
    if (command->parsed_exec_blob == NULL || command->parsed_exec_blob_size == 0) {
#ifdef ENABLE_PRINT_MSG
    	printf("playlist apply: channel=%d command_id=%d has no parsed payload, using start flag \n",
                           Chan + 1,
                           command->command_id);
#endif
        setColorStartFlag[Chan] = 1;
        return;
    }

    float effective_brightness = (brightness_override > 0.0f && brightness_override <= 100.0f)
        ? brightness_override
        : command->brightness;

    rampData[Chan].RampTimeSceneVal = 0;
    rampData[Chan].DwellTimeSceneVal = 0;
    rampData[Chan].RampStartTime = 0;

    int ch1 = Chan + 1;
    static AMessage_st playlist_dummy_msg;
    memset(&playlist_dummy_msg, 0, sizeof(playlist_dummy_msg));
#ifdef ENABLE_PRINT_MSG
    printf("playlist apply: channel=%d command_id=%d effective_brightness=%.1f blob_size=%u \n",
                       ch1,
                       command->command_id,
                       effective_brightness,
                       (unsigned)command->parsed_exec_blob_size);
#endif
    if (execute_parsed_command_snapshot(&playlist_dummy_msg, command, ch1, effective_brightness) == -1) {
#ifdef ENABLE_PRINT_MSG
    	printf("playlist apply failed: channel=%d command_id=%d \n", ch1, command->command_id);
#endif
        setColorStartFlag[Chan] = 1;
        return;
    }
}

static void Read_Color_Table(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx_data = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx1;
	AMessage_st *s_Message_Rx = &s_Message_Rx1;
	char data_buffer[512];
	memset(data_buffer,0,sizeof(data_buffer));
	memcpy(s_Message_Rx, s_Message_Rx_data, sizeof(AMessage_st));
	s_Message_Rx->payload_p8 = (uint8_t*)data_buffer;

    cJSON *root_new = NULL; // Pointer to store parsed JSON data
    cJSON *name_new_JSON = NULL; // Pointer for extracting JSON items
    cJSON *name_new_JSON1 = NULL; // Pointer for extracting additional JSON items
    int End_Record = 0, Start_Record = 0; // Variables for tracking start and end of records
    char out[100] = {0}; // Temporary buffer for sending commands
    int32_t file_size = 0; // Variable to store the file size of the database

    printf("****************************************Color Table****************************************\r\n");

    // Step 1: Request the file size of the color database to check if it exists
    strcpy(out, "{\"FILE_NAME\":\"A:/Database/Color.db\"}"); // Prepare JSON request for the file size
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", out, strlen(out), "GET_FILE_SIZE"); // Send request to file system actor

    // Step 2: Wait for the response from the file system actor
    if (pdTRUE == xQueueReceive(vColor_Table_Que, (void*)Read_Color_Table_buffer, 5000)) {
        // Parse the received JSON response
        root_new = cJSON_Parse((char*)Read_Color_Table_buffer);
        if (root_new == NULL) {
            // Handle JSON parsing error
        	sprintf(Read_Color_Table_str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
            Add_Response_msg(Read_Color_Table_str, s_Message_Rx, payLoadData_Color_table); // Add error message to the response
            goto exit; // Exit in case of error
        }

        // Extract the file size from the JSON response
        name_new_JSON = cJSON_GetObjectItem(root_new, "FILE_SIZE");
        if ((name_new_JSON != NULL) && cJSON_IsNumber(name_new_JSON)) {
            file_size = name_new_JSON->valueint; // Get the file size if valid
        }

        // Clean up the JSON object
        cJSON_Delete(root_new);

        // Step 3: If the file size is <= 0, the database does not exist, so we create it
        if (file_size <= 0) {
            ESP_LOGI("DB", "Database does not exist. Creating a new one.");

            // Step 4: Create the ColorTable if it doesn't exist
            sprintf(Read_Color_Table_str, "CREATE TABLE IF NOT EXISTS ColorTable("
                         "Id INT, "
                         "Name TEXT, "
                         "ColorIndex INT, "
                         "Hue INT, "
                         "Saturation INT, "
                         "Value INT);"); // SQL query to create the table

            // Prepare the JSON command to send to the SQLite actor
            cJSON *create_table_JSON = cJSON_CreateObject(); // Create JSON object
            cJSON_AddStringToObject(create_table_JSON, "FILE_NAME", "A:/DATABASE/Color.db"); // Add file name
            cJSON_AddStringToObject(create_table_JSON, "QUERY", Read_Color_Table_str); // Add SQL query
          
			payLoadData_Color_table[0]='\0';
			cJSON_PrintPreallocated(create_table_JSON, payLoadData_Color_table, sizeof(payLoadData_Color_table), false);
            Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Color_table, strlen(payLoadData_Color_table), "DB_EXECUTE"); // Send to SQLite actor
            cJSON_Delete(create_table_JSON); // Clean up JSON object

            // Step 5: Populate the table with default values
            const char *insert_sql_template = "INSERT INTO ColorTable VALUES(%d, '%s', %d, %d, %d, %d);"; // SQL template for inserting data

            // Loop through the default colors and insert them into the table
            for (int i = 0; i < sizeof(default_colors) / sizeof(color_t); i++) {
                // Prepare SQL query to insert each color
                snprintf(Read_Color_Table_str, sizeof(Read_Color_Table_str), insert_sql_template,
                         default_colors[i].id,        // Color ID
                         default_colors[i].name,      // Color name
                         default_colors[i].colorIndex,// Color index
                         default_colors[i].hue,       // Hue
                         default_colors[i].saturation,// Saturation
                         default_colors[i].value);    // Value (brightness)

                // Prepare the JSON command to send to the SQLite actor
                cJSON *insert_data_JSON = cJSON_CreateObject(); // Create JSON object
                cJSON_AddStringToObject(insert_data_JSON, "FILE_NAME", "A:/DATABASE/Color.db"); // Add file name
                cJSON_AddStringToObject(insert_data_JSON, "QUERY", Read_Color_Table_str); // Add SQL query
                
				payLoadData_Color_table[0]='\0';
				cJSON_PrintPreallocated(insert_data_JSON, payLoadData_Color_table, sizeof(payLoadData_Color_table), false);
                Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Color_table, strlen(payLoadData_Color_table), "DB_EXECUTE"); // Send to SQLite actor
                cJSON_Delete(insert_data_JSON); // Clean up JSON object
           
                //store default values in the color structure
                strcpy(Color_table[i].Name, default_colors[i].name);
                Color_table[i].ColorIndex = default_colors[i].colorIndex;
                Color_table[i].Hue = default_colors[i].hue;
                Color_table[i].Saturation = default_colors[i].saturation;
                Color_table[i].Value = default_colors[i].value;


//                vTaskDelay(50 / portTICK_PERIOD_MS);   //Speed-up
                // Wait for the response for each insertion
            }
            Add_Response_msg("Color table populated with default values.", s_Message_Rx, payLoadData_Color_table);
         }

        // Step 6: If the database already exists, proceed with reading the color table
        if (file_size > 0) {
            // Initialize the color table array with zeros
            memset(Color_table, 0, sizeof(Color_table));

            // Step 7: Fetch the minimum and maximum row IDs from the ColorTable
            sprintf(Read_Color_Table_str, "SELECT MIN(rowid) AS COL_MIN_ID, MAX(rowid) AS COL_MAX_ID FROM ColorTable;"); // SQL query to fetch row ID range
            cJSON *min_max_JSON = cJSON_CreateObject(); // Create JSON object
            cJSON_AddStringToObject(min_max_JSON, "FILE_NAME", "A:/DATABASE/Color.db"); // Add file name
            cJSON_AddStringToObject(min_max_JSON, "QUERY", Read_Color_Table_str); // Add SQL query

			payLoadData_Color_table[0]='\0';
			cJSON_PrintPreallocated(min_max_JSON, payLoadData_Color_table, sizeof(payLoadData_Color_table), false);
			Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Color_table, strlen(payLoadData_Color_table), "DB_EXECUTE"); // Send to SQLite actor
            cJSON_Delete(min_max_JSON); // Clean up JSON object

            // Wait for the response containing the min and max row IDs
            if (pdTRUE == xQueueReceive(vColor_Table_Que, (void*)Read_Color_Table_buffer, 5000)) {
                root_new = cJSON_Parse((char*)Read_Color_Table_buffer); // Parse the received JSON response
                if (root_new == NULL) {
                	sprintf(Read_Color_Table_str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__); // Handle JSON parsing error
                    Add_Response_msg(Read_Color_Table_str, s_Message_Rx, payLoadData_Color_table); // Add error message to response
                    goto exit; // Exit in case of error
                }

                // Extract the maximum row ID
                name_new_JSON = cJSON_GetObjectItem(root_new, "COL_MAX_ID");
                if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL)) {
                    End_Record = atoi(name_new_JSON->valuestring); // Convert string to integer
                }

                // Extract the minimum row ID
                name_new_JSON = cJSON_GetObjectItem(root_new, "COL_MIN_ID");
                if ((name_new_JSON != NULL) && (name_new_JSON->valuestring != NULL)) {
                    Start_Record = atoi(name_new_JSON->valuestring); // Convert string to integer
                }

                // Clean up the JSON object
                cJSON_Delete(root_new);
            } else {
                goto exit; // Exit if no response is received
            }

            // Step 8: Loop to fetch and process records from the ColorTable in batches
            int batch_size = 61; // Define batch size

            while (Start_Record <= End_Record) {
                // Calculate the number of records to fetch in the current batch
                int records_to_fetch = (Start_Record + batch_size - 1 <= End_Record) ? batch_size : (End_Record - Start_Record + 1);

                // Step 9: Fetch the next batch of records starting from Start_Record
                sprintf(Read_Color_Table_str, "SELECT * FROM ColorTable WHERE rowid >= %d LIMIT %d;", Start_Record, records_to_fetch); // SQL query to fetch records
                cJSON *my_JSON1 = cJSON_CreateObject(); // Create JSON object
                cJSON_AddStringToObject(my_JSON1, "FILE_NAME", "A:/DATABASE/Color.db"); // Add file name
                cJSON_AddStringToObject(my_JSON1, "QUERY", Read_Color_Table_str); // Add SQL query
                payLoadData_Color_table[0]='\0';
				cJSON_PrintPreallocated(my_JSON1, payLoadData_Color_table, sizeof(payLoadData_Color_table), false);
				Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Color_table, strlen(payLoadData_Color_table), "DB_EXECUTE"); // Send to SQLite actor
                cJSON_Delete(my_JSON1); // Clean up JSON object

                // Step 10: Process each record received from the SQLite actor
                int records_processed = 0;
                for (int i = 0; i < records_to_fetch; i++) {
                    if (pdTRUE == xQueueReceive(vColor_Table_Que, (void*)Read_Color_Table_buffer, 5000)) {
                        root_new = cJSON_Parse((char*)Read_Color_Table_buffer); // Parse the received JSON response
                        if (root_new == NULL) {
                        	sprintf(Read_Color_Table_str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__); // Handle JSON parsing error
                            Add_Response_msg(Read_Color_Table_str, s_Message_Rx, payLoadData_Color_table); // Add error message to response
                            goto exit; // Exit in case of error
                        }

                        // Ensure the array bounds are not exceeded
                        if (Start_Record + records_processed < MAX_COLOR_ORDER) {
                            // Extract and store color Name from the JSON response
                            name_new_JSON = cJSON_GetObjectItem(root_new, "Name");
                            if (name_new_JSON != NULL && name_new_JSON->valuestring != NULL) {
                                strcpy(Color_table[Start_Record + records_processed].Name, name_new_JSON->valuestring);
                            }

                            // Extract and store ColorIndex from the JSON response
                            name_new_JSON1 = cJSON_GetObjectItem(root_new, "ColorIndex");
                            if (name_new_JSON1 != NULL && name_new_JSON1->valuestring != NULL) {
                                Color_table[Start_Record + records_processed].ColorIndex = atoi(name_new_JSON1->valuestring);
                            }

                            // Extract and store Hue from the JSON response
                            name_new_JSON = cJSON_GetObjectItem(root_new, "Hue");
                            if (name_new_JSON != NULL && name_new_JSON->valuestring != NULL) {
                                Color_table[Start_Record + records_processed].Hue = atoi(name_new_JSON->valuestring);
                            }

                            // Extract and store Saturation from the JSON response
                            name_new_JSON = cJSON_GetObjectItem(root_new, "Saturation");
                            if (name_new_JSON != NULL && name_new_JSON->valuestring != NULL) {
                                Color_table[Start_Record + records_processed].Saturation = atoi(name_new_JSON->valuestring);
                            }

                            // Extract and store Value from the JSON response
                            name_new_JSON = cJSON_GetObjectItem(root_new, "Value");
                            if (name_new_JSON != NULL && name_new_JSON->valuestring != NULL) {
                                Color_table[Start_Record + records_processed].Value = atoi(name_new_JSON->valuestring);
                            }
                        } else {
                            // Log and handle array bounds exceeded error
                            sprintf(Read_Color_Table_str, "Array bounds exceeded: Start_Record = %d, records_processed = %d", Start_Record, records_processed);
                            Add_Response_msg(Read_Color_Table_str, s_Message_Rx, payLoadData_Color_table); // Add error message to response
                            goto exit; // Exit in case of error
                        }

                        // Clean up the parsed JSON object
                        cJSON_Delete(root_new);

                        // Increment the number of processed records
                        records_processed++;
                    } else {
                        goto exit; // Exit if no response is received
                    }
                }

                // Update Start_Record to move to the next batch
                Start_Record += records_processed;
            }
        }
    }

exit:
    ReadColorTable_Handle = NULL;
    vTaskDelete(ReadColorTable_Handle); // Delete the task
}

static void Read_Command_Table(void *pvParameters __attribute__((unused)))
{
//	printf("Init READ_COMMAND_TABLE \n");

    const char *db_path = "A:/Database/Command_Table.db";
    cJSON *root = NULL;
    int32_t file_size = 0;
    int32_t start_record = 0;
    int32_t end_record = 0;

    if (vCommand_Table_Que == NULL) {
        ESP_LOGE(TAG, "Command table queue missing");
        goto exit;
    }

    sprintf(Read_Command_Table_str, "{\"FILE_NAME\":\"%s\"}", db_path);
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", Read_Command_Table_str, strlen(Read_Command_Table_str), "GET_FILE_SIZE");
    if (pdTRUE != xQueueReceive(vCommand_Table_Que, (void *)Read_Command_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for command file size");
        goto exit;
    }

    root = cJSON_Parse((char *)Read_Command_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Command file size JSON invalid");
        goto exit;
    }
    cJSON *size_item = cJSON_GetObjectItemCaseSensitive(root, "FILE_SIZE");
    if (size_item != NULL && cJSON_IsNumber(size_item)) {
        file_size = size_item->valueint;
//        printf(" file_size = %ld \n", file_size);
    }
    cJSON_Delete(root);
    if (file_size <= 0) {
        ESP_LOGW(TAG, "Command database is missing or empty");
        goto exit;
    }

    clear_command_table();

    sprintf(Read_Command_Table_str, "SELECT MIN(rowid) AS COMMAND_MIN_ID, MAX(rowid) AS COMMAND_MAX_ID FROM Command_table;");
    cJSON *min_max_json = cJSON_CreateObject();
    if (min_max_json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate command min/max JSON");
        goto exit;
    }

//    printf("Command table 1 \n");

    cJSON_AddStringToObject(min_max_json, "FILE_NAME", "A:/DATABASE/Command_Table.db");
    cJSON_AddStringToObject(min_max_json, "QUERY", Read_Command_Table_str);
    payLoadData_Command_table[0] = '\0';
    cJSON_PrintPreallocated(min_max_json, payLoadData_Command_table, sizeof(payLoadData_Command_table), false);
    Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Command_table, strlen(payLoadData_Command_table), "DB_EXECUTE");
    cJSON_Delete(min_max_json);

    if (pdTRUE != xQueueReceive(vCommand_Table_Que, (void *)Read_Command_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for command min/max");
        goto exit;
    }

//    printf("Command table 2 \n");

    root = cJSON_Parse((char *)Read_Command_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Command min/max response invalid");
        goto exit;
    }
    cJSON *min_item = cJSON_GetObjectItemCaseSensitive(root, "COMMAND_MIN_ID");
    cJSON *max_item = cJSON_GetObjectItemCaseSensitive(root, "COMMAND_MAX_ID");

//    printf("Command table 3 \n");

//    if (min_item != NULL && cJSON_IsNumber(min_item)) {
    if ((min_item != NULL) && (min_item->valuestring != NULL)) {
//        start_record = min_item->valueint;
    	start_record = atoi(min_item->valuestring); // Convert string to integer
//        printf(" start_record = %ld \n", start_record);
    }
//    if (max_item != NULL && cJSON_IsNumber(max_item)) {
    if ((max_item != NULL) && (max_item->valuestring != NULL)) {
//        end_record = max_item->valueint;
    	end_record = atoi(max_item->valuestring); // Convert string to integer
//        printf(" end_record = %ld \n", end_record);
    }
    cJSON_Delete(root);
    if (start_record < 1 || end_record < start_record) {
        ESP_LOGE(TAG, "Invalid command record range %ld-%ld", start_record, end_record);
        goto exit;
    }

    command_table_size = 0;
    command_table_capacity = (size_t)(end_record - start_record + 1);
    if (command_table_capacity == 0) {
        ESP_LOGW(TAG, "Command table has no rows");
        goto exit;
    }
    command_table = (CommandEntry *)heap_caps_calloc(
        command_table_capacity,
        sizeof(CommandEntry),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (command_table == NULL) {
        ESP_LOGE(TAG, "Failed to allocate command_table for %u rows", (unsigned)command_table_capacity);
        command_table_capacity = 0;
        goto exit;
    }

//    printf("Command table 4 \n");

    const int batch_size = 50;
    while (start_record <= end_record && command_table_size < command_table_capacity)
    {
//    	printf("Command table 5 \n");

        int remaining = end_record - start_record + 1;
        int records_to_fetch = (remaining < batch_size) ? remaining : batch_size;
        sprintf(Read_Command_Table_str, "SELECT * FROM Command_table WHERE rowid >= %ld LIMIT %d;", start_record, records_to_fetch);
        cJSON *query_json = cJSON_CreateObject();
        if (query_json == NULL) {
            ESP_LOGE(TAG, "Failed to build command query");
            break;
        }
        cJSON_AddStringToObject(query_json, "FILE_NAME", "A:/DATABASE/Command_Table.db");
        cJSON_AddStringToObject(query_json, "QUERY", Read_Command_Table_str);
        payLoadData_Command_table[0] = '\0';
        cJSON_PrintPreallocated(query_json, payLoadData_Command_table, sizeof(payLoadData_Command_table), false);
        Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Command_table, strlen(payLoadData_Command_table), "DB_EXECUTE");
        cJSON_Delete(query_json);

        int records_processed = 0;
        for (int i = 0; i < records_to_fetch && command_table_size < command_table_capacity; ++i)
        {
//        	printf("Command table 6 \n");

            if (pdTRUE != xQueueReceive(vCommand_Table_Que, (void *)Read_Command_Table_buffer, 5000)) {
                ESP_LOGE(TAG, "Timeout reading command rows");
                break;
            }

//            printf("Read_Command_Table_buffer 11 = %s \n", Read_Command_Table_buffer);

            cJSON *row = cJSON_Parse((char *)Read_Command_Table_buffer);
            if (row == NULL) {
                ESP_LOGE(TAG, "Invalid command row JSON");
                continue;
            }
            CommandEntry entry = {0};
            if (parse_command_row(row, &entry)) {
                command_table[command_table_size++] = entry;
            }
            cJSON_Delete(row);
            ++records_processed;
        }
        if (records_processed == 0) {
            break;
        }
        start_record += records_processed;
    }

    if (command_table_size < command_table_capacity) {
        size_t shrink_size = command_table_size * sizeof(CommandEntry);
        CommandEntry *shrunk = (CommandEntry *)heap_caps_realloc(
            command_table,
            shrink_size,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (shrunk != NULL || command_table_size == 0) {
            command_table = shrunk;
            command_table_capacity = command_table_size;
        }
    }

    rebuild_command_index_table();
    ESP_LOGI(TAG, "Loaded %zu command entries", command_table_size);
    for (size_t i = 0; i < command_table_size; ++i) {
        const CommandEntry *e = &command_table[i];
        bool ready = (e->type == COMMAND_TYPE_PLAYLIST)
            ? (e->nested_playlist_id > 0)
            : (e->parsed_exec_kind != PARSED_EXEC_NONE && e->parsed_exec_blob != NULL && e->parsed_exec_blob_size > 0);
        ESP_LOGI(TAG, "Command validate: id=%d type=%d parse=%s ready=%s nested_playlist_id=%d",
                 e->command_id, (int)e->type, parsed_exec_kind_name(e->parsed_exec_kind),
                 ready ? "YES" : "NO", e->nested_playlist_id);
    }
    log_playlist_memory_usage("after_command_table_load");

exit:
    ReadCommandTable_Handle = NULL;
    vTaskDelete(NULL);
}

static void Read_Playlist_Table(void *pvParameters __attribute__((unused)))
{
//	printf("Init READ_PLAYLIST_TABLE \n");

    const char *db_path = "A:/Database/Playlist_Table.db";
    cJSON *root = NULL;
    int32_t file_size = 0;
    int32_t start_record = 0;
    int32_t end_record = 0;

    if (vPlaylist_Table_Que == NULL) {
        ESP_LOGE(TAG, "Playlist table queue missing");
        goto exit;
    }

    sprintf(Read_Playlist_Table_str, "{\"FILE_NAME\":\"%s\"}", db_path);
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", Read_Playlist_Table_str, strlen(Read_Playlist_Table_str), "GET_FILE_SIZE");
    if (pdTRUE != xQueueReceive(vPlaylist_Table_Que, (void *)Read_Playlist_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for playlist file size");
        goto exit;
    }
    root = cJSON_Parse((char *)Read_Playlist_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Playlist file size JSON invalid");
        goto exit;
    }
    cJSON *size_item = cJSON_GetObjectItemCaseSensitive(root, "FILE_SIZE");
    if (size_item != NULL && cJSON_IsNumber(size_item)) {
        file_size = size_item->valueint;
    }
    cJSON_Delete(root);

//    printf("Playlist table 1 \n");

    if (file_size <= 0) {
        ESP_LOGW(TAG, "Playlist database is missing or empty");
        goto exit;
    }

    sprintf(Read_Playlist_Table_str, "SELECT MIN(rowid) AS PLAY_MIN_ID, MAX(rowid) AS PLAY_MAX_ID FROM Playlist_table;");
    cJSON *min_max_json = cJSON_CreateObject();
    if (min_max_json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate playlist min/max JSON");
        goto exit;
    }
    cJSON_AddStringToObject(min_max_json, "FILE_NAME", "A:/DATABASE/Playlist_Table.db");
    cJSON_AddStringToObject(min_max_json, "QUERY", Read_Playlist_Table_str);
    payLoadData_Playlist_table[0] = '\0';
    cJSON_PrintPreallocated(min_max_json, payLoadData_Playlist_table, sizeof(payLoadData_Playlist_table), false);
    Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Playlist_table, strlen(payLoadData_Playlist_table), "DB_EXECUTE");
    cJSON_Delete(min_max_json);

    if (pdTRUE != xQueueReceive(vPlaylist_Table_Que, (void *)Read_Playlist_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for playlist min/max");
        goto exit;
    }
    root = cJSON_Parse((char *)Read_Playlist_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Playlist min/max response invalid");
        goto exit;
    }

//    printf("Playlist table 2 \n");

    cJSON *min_item = cJSON_GetObjectItemCaseSensitive(root, "PLAY_MIN_ID");
    cJSON *max_item = cJSON_GetObjectItemCaseSensitive(root, "PLAY_MAX_ID");

    //    if (min_item != NULL && cJSON_IsNumber(min_item)) {
        if ((min_item != NULL) && (min_item->valuestring != NULL)) {
    //        start_record = min_item->valueint;
        	start_record = atoi(min_item->valuestring); // Convert string to integer
//            printf(" start_record = %ld \n", start_record);
        }
    //    if (max_item != NULL && cJSON_IsNumber(max_item)) {
        if ((max_item != NULL) && (max_item->valuestring != NULL)) {
    //        end_record = max_item->valueint;
        	end_record = atoi(max_item->valuestring); // Convert string to integer
//            printf(" end_record = %ld \n", end_record);
        }

    if (min_item != NULL && (min_item->valuestring != NULL)) {
//        start_record = min_item->valueint;
    	start_record = atoi(min_item->valuestring); // Convert string to integer
//    	printf(" start_record = %ld \n", start_record);
    }
    if (max_item != NULL && (max_item->valuestring != NULL)) {
//        end_record = max_item->valueint;
    	end_record = atoi(max_item->valuestring); // Convert string to integer
//        printf(" end_record = %ld \n", end_record);
    }
    cJSON_Delete(root);
    if (start_record < 1 || end_record < start_record) {
        ESP_LOGE(TAG, "Invalid playlist record range %ld-%ld", start_record, end_record);
        goto exit;
    }

    clear_playlist_records();
    playlist_record_capacity = (size_t)(end_record - start_record + 1);
    if (playlist_record_capacity == 0) {
        ESP_LOGW(TAG, "Playlist table has no rows");
        goto exit;
    }
    playlist_records = (PlaylistEntryRecord *)heap_caps_calloc(
        playlist_record_capacity,
        sizeof(PlaylistEntryRecord),
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (playlist_records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate playlist_records for %u rows", (unsigned)playlist_record_capacity);
        playlist_record_capacity = 0;
        goto exit;
    }
    playlist_record_count = 0;

    const int batch_size = 50;
    while (start_record <= end_record && playlist_record_count < playlist_record_capacity) {
//    	printf("Playlist table 3 \n");

        int remaining = end_record - start_record + 1;
        int records_to_fetch = (remaining < batch_size) ? remaining : batch_size;
        sprintf(Read_Playlist_Table_str, "SELECT * FROM Playlist_table WHERE rowid >= %ld LIMIT %d;", start_record, records_to_fetch);
        cJSON *query_json = cJSON_CreateObject();
        if (query_json == NULL) {
            ESP_LOGE(TAG, "Failed to build playlist query");
            break;
        }
        cJSON_AddStringToObject(query_json, "FILE_NAME", "A:/DATABASE/Playlist_Table.db");
        cJSON_AddStringToObject(query_json, "QUERY", Read_Playlist_Table_str);
        payLoadData_Playlist_table[0] = '\0';
        cJSON_PrintPreallocated(query_json, payLoadData_Playlist_table, sizeof(payLoadData_Playlist_table), false);
        Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Playlist_table, strlen(payLoadData_Playlist_table), "DB_EXECUTE");
        cJSON_Delete(query_json);

        int records_processed = 0;
        for (int i = 0; i < records_to_fetch && playlist_record_count < playlist_record_capacity; ++i) {
//        	printf("Playlist table 4 \n");
            if (pdTRUE != xQueueReceive(vPlaylist_Table_Que, (void *)Read_Playlist_Table_buffer, 5000)) {
                ESP_LOGE(TAG, "Timeout reading playlist rows");
                break;
            }

//            printf("Read_Playlist_Table_buffer 11 = %s \n", Read_Playlist_Table_buffer);

            cJSON *row = cJSON_Parse((char *)Read_Playlist_Table_buffer);
            if (row == NULL) {
                ESP_LOGE(TAG, "Invalid playlist row JSON");
                continue;
            }

//            printf("Playlist table 5 \n");

            PlaylistEntryRecord record = {0};
            if (parse_playlist_row(row, &record)) {
                playlist_records[playlist_record_count++] = record;
            }
            cJSON_Delete(row);
            ++records_processed;
        }
        if (records_processed == 0) {
            break;
        }
        start_record += records_processed;
    }

    if (playlist_record_count > 1) {
        qsort(playlist_records, playlist_record_count, sizeof(PlaylistEntryRecord), playlist_entry_compare);
    }
    if (playlist_record_count < playlist_record_capacity) {
        size_t shrink_size = playlist_record_count * sizeof(PlaylistEntryRecord);
        PlaylistEntryRecord *shrunk = (PlaylistEntryRecord *)heap_caps_realloc(
            playlist_records,
            shrink_size,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );
        if (shrunk != NULL || playlist_record_count == 0) {
            playlist_records = shrunk;
            playlist_record_capacity = playlist_record_count;
        }
    }
    clear_playlist_sequence_cache();
    ESP_LOGI(TAG, "Loaded %zu playlist entries", playlist_record_count);
    log_playlist_memory_usage("after_playlist_table_load");

exit:
    ReadPlaylistTable_Handle = NULL;
    vTaskDelete(NULL);
}

static void Read_Virtual_Table(void *pvParameters __attribute__((unused)))
{
    const char *db_path = "A:/Database/Virtual_Table.db";
    cJSON *root = NULL;
    int32_t file_size = 0;
    int32_t start_record = 0;
    int32_t end_record = -1;
    bool loaded_from_db = false;

    if (vVirtual_Table_Que == NULL) {
        ESP_LOGE(TAG, "Virtual table queue missing");
        goto exit;
    }

    clear_virtual_groups();

    sprintf(Read_Virtual_Table_str, "{\"FILE_NAME\":\"%s\"}", db_path);
    Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", Read_Virtual_Table_str, strlen(Read_Virtual_Table_str), "GET_FILE_SIZE");
    if (pdTRUE != xQueueReceive(vVirtual_Table_Que, (void *)Read_Virtual_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for virtual file size");
        goto exit;
    }

    root = cJSON_Parse((char *)Read_Virtual_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Virtual file size JSON invalid");
        goto exit;
    }

    cJSON *size_item = cJSON_GetObjectItem(root, "FILE_SIZE");
    if (size_item != NULL && cJSON_IsNumber(size_item)) {
        file_size = size_item->valueint;
    }
    cJSON_Delete(root);

//    printf("Virtual table 1 \n");

    if (file_size <= 0) {
        ESP_LOGW(TAG, "Virtual table database missing or empty, loading defaults");
        load_default_virtual_groups();
        loaded_from_db = true;
        goto exit;
    }

    sprintf(Read_Virtual_Table_str, "SELECT MIN(rowid) AS VIRTUAL_GROUP_MIN_ID, MAX(rowid) AS VIRTUAL_GROUP_MAX_ID FROM Virtual_table;");
    cJSON *min_max_json = cJSON_CreateObject();
    if (min_max_json == NULL) {
        ESP_LOGE(TAG, "Failed to allocate virtual min/max JSON");
        goto exit;
    }
    cJSON_AddStringToObject(min_max_json, "FILE_NAME", "A:/DATABASE/Virtual_Table.db");
    cJSON_AddStringToObject(min_max_json, "QUERY", Read_Virtual_Table_str);
    payLoadData_Virtual_table[0] = '\0';
    cJSON_PrintPreallocated(min_max_json, payLoadData_Virtual_table, sizeof(payLoadData_Virtual_table), false);
    Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Virtual_table, strlen(payLoadData_Virtual_table), "DB_EXECUTE");
    cJSON_Delete(min_max_json);

    if (pdTRUE != xQueueReceive(vVirtual_Table_Que, (void *)Read_Virtual_Table_buffer, 5000)) {
        ESP_LOGE(TAG, "Timeout waiting for virtual min/max");
        goto exit;
    }

    root = cJSON_Parse((char *)Read_Virtual_Table_buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Virtual min/max response invalid");
        goto exit;
    }

//    printf("Virtual table 2 \n");

    cJSON *min_item = cJSON_GetObjectItemCaseSensitive(root, "VIRTUAL_GROUP_MIN_ID");
    cJSON *max_item = cJSON_GetObjectItemCaseSensitive(root, "VIRTUAL_GROUP_MAX_ID");
    if (min_item != NULL && min_item->valuestring != NULL) {
        start_record = atoi(min_item->valuestring);
//        printf("start_record = %ld\n", start_record);
    }
    if (max_item != NULL && max_item->valuestring != NULL) {
        end_record = atoi(max_item->valuestring);
//        printf("end_record = %ld\n", end_record);
    }
    cJSON_Delete(root);

    if (end_record < start_record) {
        ESP_LOGE(TAG, "Virtual table range invalid %ld-%ld", start_record, end_record);
        goto exit;
    }

    const int batch_size = 32;
    while (start_record <= end_record && virtual_group_count < MAX_VIRTUAL_GROUPS)
    {
//    	printf("Virtual table 3 \n");

        int remaining = end_record - start_record + 1;
        int records_to_fetch = (remaining < batch_size) ? remaining : batch_size;
        sprintf(Read_Virtual_Table_str, "SELECT * FROM Virtual_table WHERE rowid >= %ld LIMIT %d;", start_record, records_to_fetch);
        cJSON *query_json = cJSON_CreateObject();
        if (query_json == NULL) {
            ESP_LOGE(TAG, "Failed to build virtual query");
            break;
        }
        cJSON_AddStringToObject(query_json, "FILE_NAME", "A:/DATABASE/Virtual_Table.db");
        cJSON_AddStringToObject(query_json, "QUERY", Read_Virtual_Table_str);
        payLoadData_Virtual_table[0] = '\0';
        cJSON_PrintPreallocated(query_json, payLoadData_Virtual_table, sizeof(payLoadData_Virtual_table), false);
        Send_CMD_To_Other_Actor(SQL, "SQL", payLoadData_Virtual_table, strlen(payLoadData_Virtual_table), "DB_EXECUTE");
        cJSON_Delete(query_json);

        int records_processed = 0;
        for (int i = 0; i < records_to_fetch && virtual_group_count < MAX_VIRTUAL_GROUPS; ++i) {
            if (pdTRUE != xQueueReceive(vVirtual_Table_Que, (void *)Read_Virtual_Table_buffer, 5000)) {
                ESP_LOGE(TAG, "Timeout reading virtual rows");
                break;
            }

//            printf("Read_Virtual_Table_buffer 11 = %s \n", Read_Virtual_Table_buffer);

            cJSON *row = cJSON_Parse((char *)Read_Virtual_Table_buffer);
            if (row == NULL) {
                ESP_LOGE(TAG, "Invalid virtual row JSON");
                continue;
            }

            VirtualGroup entry = {0};
            if (parse_virtual_group_row(row, &entry)) {
                virtual_groups[virtual_group_count++] = entry;
            }
            cJSON_Delete(row);
            ++records_processed;
        }
        if (records_processed == 0) {
            break;
        }
        start_record += records_processed;
    }

    if (virtual_group_count > 0) {
        loaded_from_db = true;
    }

exit:
    if (!loaded_from_db && virtual_group_count == 0) {
        load_default_virtual_groups();
    }
    ReadVirtualTable_Handle = NULL;
    vTaskDelete(NULL);
}

static cJSON *find_json_field_case_insensitive(const cJSON *object, const char *const keys[], size_t key_count)
{
    if (object == NULL) {
        return NULL;
    }
    cJSON *current = (cJSON *)object->child;
    while (current != NULL) {
        if (current->string != NULL) {
            for (size_t i = 0; i < key_count; ++i) {
                if (strcasecmp(current->string, keys[i]) == 0) {
                    return current;
                }
            }
        }
        current = current->next;
    }
    return NULL;
}

static int json_get_int_value(const cJSON *object, const char *const keys[], size_t key_count, int default_value)
{
    cJSON *item = find_json_field_case_insensitive(object, keys, key_count);
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            return item->valueint;
        }
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            return atoi(item->valuestring);
        }
    }
    return default_value;
}

static uint8_t json_get_u8_value(const cJSON *object, const char *const keys[], size_t key_count, uint8_t default_value)
{
    int val = json_get_int_value(object, keys, key_count, -1);
    if (val < 0) {
        return default_value;
    }
    return (uint8_t)val;
}

static const char *json_get_string_value(const cJSON *object, const char *const keys[], size_t key_count)
{
    cJSON *item = find_json_field_case_insensitive(object, keys, key_count);
    if (item != NULL && cJSON_IsString(item)) {
        return item->valuestring;
    }
    return NULL;
}

static CommandType command_type_from_value(const cJSON *value)
{
    if (value == NULL) {
        return COMMAND_TYPE_COUNT;
    }
    if (cJSON_IsNumber(value)) {
        int index = value->valueint;
        if (index >= 0 && index < COMMAND_TYPE_COUNT) {
            return (CommandType)index;
        }
    }
    if (cJSON_IsString(value) && value->valuestring != NULL) {
        const char *str = value->valuestring;
        if (strcasecmp(str, "ON") == 0) {
            return COMMAND_TYPE_ON;
        }
        if (strcasecmp(str, "OFF") == 0) {
            return COMMAND_TYPE_OFF;
        }
        if (strcasecmp(str, "COLOR") == 0) {
            return COMMAND_TYPE_COLOR;
        }
        if (strcasecmp(str, "PATTERN") == 0) {
            return COMMAND_TYPE_PATTERN;
        }
        if (strcasecmp(str, "EFFECT") == 0) {
            return COMMAND_TYPE_EFFECT;
        }
        if (strcasecmp(str, "SCENE") == 0) {
            return COMMAND_TYPE_SCENE;
        }
        if (strcasecmp(str, "LIGHT_SHOW") == 0 || strcasecmp(str, "LIGHTSHOW") == 0) {
            return COMMAND_TYPE_LIGHT_SHOW;
        }
        if (strcasecmp(str, "PLAYLIST") == 0) {
            return COMMAND_TYPE_PLAYLIST;
        }
    }
    return COMMAND_TYPE_COUNT;
}

static PlaylistTargetType parse_target_type_override(const char *value)
{
    if (value == NULL)
    {
        return TARGET_TYPE_COUNT;
    }

    if (strcasecmp(value, "ALL_CHANNELS") == 0)
    {
        return TARGET_ALL_CHANNELS;
    }
    if (strcasecmp(value, "SELECTED_CHANNELS") == 0)
    {
        return TARGET_SELECTED_CHANNELS;
    }
    if (strcasecmp(value, "VIRTUAL_GROUPS") == 0)
    {
        return TARGET_VIRTUAL_GROUPS;
    }
    return TARGET_TYPE_COUNT;
}

static PlaylistTargetType playlist_target_type_from_row(const cJSON *row)
{
    cJSON *item = find_json_field_case_insensitive(row, playlist_target_type_keys, sizeof(playlist_target_type_keys) / sizeof(playlist_target_type_keys[0]));
    if (item != NULL) {
        if (cJSON_IsNumber(item)) {
            int index = item->valueint;
            if (index >= 0 && index < TARGET_TYPE_COUNT) {
                return (PlaylistTargetType)index;
            }
        }
        if (cJSON_IsString(item) && item->valuestring != NULL) {
            PlaylistTargetType parsed = parse_target_type_override(item->valuestring);
            if (parsed != TARGET_TYPE_COUNT) {
                return parsed;
            }
        }
    }
    return TARGET_ALL_CHANNELS;
}

static bool parse_command_row(cJSON *row, CommandEntry *entry)
{
//	printf("parse_command_row 1 \n");

    if (row == NULL || entry == NULL) {
        return false;
    }
    int command_id = json_get_int_value(row, command_id_keys, sizeof(command_id_keys) / sizeof(command_id_keys[0]), -1);
    if (command_id <= 0 || command_id > 65535) {
        return false;
    }

//    printf("parse_command_row 2 , command_id = %d \n", command_id);

    entry->command_id = (uint16_t)command_id;

    cJSON *type_field = find_json_field_case_insensitive(row, command_type_keys, sizeof(command_type_keys) / sizeof(command_type_keys[0]));
    CommandType parsed_type = command_type_from_value(type_field);
    if (parsed_type == COMMAND_TYPE_COUNT) {
        return false;
    }
    entry->type = (uint8_t)parsed_type;

//    printf("parse_command_row 3 , entry->type = %d \n", entry->type);

    entry->brightness = 100.0f;
    cJSON *brightness_item = find_json_field_case_insensitive(row, brightness_keys, sizeof(brightness_keys) / sizeof(brightness_keys[0]));
    if (brightness_item != NULL && cJSON_IsNumber(brightness_item)) {
        entry->brightness = (float)cJSON_GetNumberValue(brightness_item);
        if (entry->brightness < 0.0f) {
            entry->brightness = 0.0f;
        } else if (entry->brightness > 100.0f) {
            entry->brightness = 100.0f;
        }
    }

//    printf("parse_command_row 4 , entry->brightness = %d \n", entry->brightness);

    entry->parsed_exec_kind = PARSED_EXEC_NONE;
    entry->parsed_exec_blob = NULL;
    entry->parsed_exec_blob_size = 0;
    entry->nested_playlist_id = 0;
    const char *payload = json_get_string_value(row, command_payload_keys, sizeof(command_payload_keys) / sizeof(command_payload_keys[0]));
    if (entry->type == COMMAND_TYPE_OFF) {
        g_template_setcolor.valid = true;
        g_template_setcolor.brightness = 100.0f;
        g_template_setcolor.red = 0.0f;
        g_template_setcolor.green = 0.0f;
        g_template_setcolor.blue = 0.0f;
        if (!save_parsed_template_snapshot(entry, PARSED_EXEC_SETCOLOR, &g_template_setcolor, sizeof(g_template_setcolor))) {
            return false;
        }
        return true;
    } else if (payload != NULL) {
        if (entry->type == COMMAND_TYPE_PLAYLIST) {
            int nested_playlist = atoi(payload);
            if (nested_playlist <= 0 || nested_playlist > 65535) {
                return false;
            }
            entry->nested_playlist_id = (uint16_t)nested_playlist;
        } else {
            cJSON *root = cJSON_Parse(payload);
            if (root == NULL) {
                return false;
            }
            cJSON *configItem = cJSON_GetObjectItem(root, "CONFIG");
            if (configItem == NULL) {
                configItem = root;
            }

            const char *function = NULL;
            cJSON *funcItem = cJSON_GetObjectItem(root, "FUNCTION");
            if (funcItem != NULL && cJSON_IsString(funcItem) && funcItem->valuestring != NULL) {
                function = funcItem->valuestring;
            }
            if (function == NULL) {
                static const char *const type_names[] = { "ON", "OFF", "COLOR", "PATTERN", "EFFECT", "SCENE", "LIGHT_SHOW", "PLAYLIST" };
                if (entry->type < COMMAND_TYPE_COUNT) {
                    function = type_names[entry->type];
                }
                if (function == NULL || strcmp(function, "ON") == 0 || strcmp(function, "OFF") == 0 || strcmp(function, "PLAYLIST") == 0) {
                    function = "PATTERN";
                }
            }

            float brightness = entry->brightness;
            cJSON *brightItem = cJSON_GetObjectItem(root, "BRIGHTNESS");
            if (brightItem != NULL && cJSON_IsNumber(brightItem)) {
                brightness = (float)cJSON_GetNumberValue(brightItem);
                if (brightness < 0.0f) {
                    brightness = 0.0f;
                } else if (brightness > 100.0f) {
                    brightness = 100.0f;
                }
            }

            static AMessage_st dummy_msg;
            memset(&dummy_msg, 0, sizeof(dummy_msg));
            bool ok = false;

            if ((!strcmp(function, "PATTERN")) || (!strcmp(function, "Pattern"))) {
                ok = (parse_pattern_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_PATTERN, &g_template_pattern, sizeof(g_template_pattern));
            } else if ((!strcmp(function, "HUESAT")) || (!strcmp(function, "HSV"))) {
                ok = (parse_huesat_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_HUESAT, &g_template_huesat, sizeof(g_template_huesat));
            }
#ifndef B542
            else if ((!strcmp(function, "SPARKLE")) || (!strcmp(function, "Sparkle"))) {
                ok = (parse_sparkle_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_SPARKLE, &g_template_sparkle, sizeof(g_template_sparkle));
            } else if ((!strcasecmp(function, "MULTICOLORSPARKLE"))) {
                ok = (parse_multicolorsparkle_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_MULTICOLORSPARKLE, &g_template_multicolorsparkle, sizeof(g_template_multicolorsparkle));
            } else if ((!strcasecmp(function, "RIPPLE")) || (!strcasecmp(function, "Wave"))) {
                ok = (parse_ripple_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_RIPPLE, &g_template_ripple, sizeof(g_template_ripple));
            }
#endif
            else if ((!strcmp(function, "EXECUTECUSTOM")) || (!strcmp(function, "Custom")) || (!strcmp(function, "Cascade"))) {
                ok = (parse_executecustom_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_CUSTOM, &g_template_executecustom, sizeof(g_template_executecustom));
            } else if ((!strcasecmp(function, "RACING"))) {
                ok = (parse_executeracing_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_RACING, &g_template_executeracing, sizeof(g_template_executeracing));
            } else if ((!strcasecmp(function, "SET_COLOR")) || (!strcasecmp(function, "PWM"))) {
                ok = (parse_setcolor_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_SETCOLOR, &g_template_setcolor, sizeof(g_template_setcolor));
            }
#ifndef B542
            else if ((!strcmp(function, "TAPEMEASURE")) || (!strcmp(function, "tapeMeasure"))) {
                ok = (parse_tapemeasure_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_TAPEMEASURE, &g_template_tapemeasure, sizeof(g_template_tapemeasure));
            }
#endif
            else if ((!strcmp(function, "COLORINDEX")) || (!strcmp(function, "colorIndex"))) {
                ok = (parse_colorindex_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_COLORINDEX, &g_template_colorindex, sizeof(g_template_colorindex));
            }
#ifndef B542
            else if ((!strcmp(function, "MARQUEE")) || (!strcmp(function, "Marquee"))) {
                ok = (parse_executemarquee_template(&dummy_msg, -1, brightness, configItem) != -1) &&
                     save_parsed_template_snapshot(entry, PARSED_EXEC_MARQUEE, &g_template_executemarquee, sizeof(g_template_executemarquee));
            }
#endif
            else {
                ok = false;
            }
            cJSON_Delete(root);
            if (!ok) {
#ifdef ENABLE_PRINT_MSG
                printf("parse_command_row: command_id=%d parse failed (function=%s)\n", entry->command_id, function ? function : "NULL");
#endif
                free_command_entry(entry);
                return false;
            }
#ifdef ENABLE_PRINT_MSG
            printf("parse_command_row: command_id=%d parsed_exec_kind=%d function=%s\n", entry->command_id, entry->parsed_exec_kind, function ? function : "NULL");
#endif
        }
    }
    else {
        if (entry->type != COMMAND_TYPE_PLAYLIST) {
            return false;
        }
    }
    return true;
}

static int playlist_entry_compare(const void *a, const void *b)
{
    const PlaylistEntryRecord *lhs = (const PlaylistEntryRecord *)a;
    const PlaylistEntryRecord *rhs = (const PlaylistEntryRecord *)b;
    if (lhs->playlist_id != rhs->playlist_id) {
        return (int)lhs->playlist_id - (int)rhs->playlist_id;
    }
    if (lhs->duration_ms != rhs->duration_ms) {
        return (lhs->duration_ms < rhs->duration_ms) ? -1 : 1;
    }
    return (int)lhs->playlist_entry_id - (int)rhs->playlist_entry_id;
}

static bool parse_playlist_row(cJSON *row, PlaylistEntryRecord *record)
{
//	printf("parse_playlist_row 1 \n");

    if (row == NULL || record == NULL) {
        return false;
    }
    int playlist_entry_id = json_get_int_value(row, playlist_entry_id_keys, sizeof(playlist_entry_id_keys) / sizeof(playlist_entry_id_keys[0]), -1);

//    printf("parse_playlist_row 2 , record->playlist_entry_id = %d \n", record->playlist_entry_id);

    int playlist_id = json_get_int_value(row, playlist_id_keys, sizeof(playlist_id_keys) / sizeof(playlist_id_keys[0]), -1);

//    printf("parse_playlist_row 3 , record->playlist_id = %d \n", record->playlist_id);

    int command_id = json_get_int_value(row, command_id_keys, sizeof(command_id_keys) / sizeof(command_id_keys[0]), -1);

//    printf("parse_playlist_row 4 , record->command_id = %d \n", record->command_id);

    int duration_ms = json_get_int_value(row, playlist_duration_keys, sizeof(playlist_duration_keys) / sizeof(playlist_duration_keys[0]), 0);

//    printf("parse_playlist_row 4 , record->duration_ms = %ld \n", record->duration_ms);

    if (duration_ms <= 0) {
        duration_ms = 1;
    }
    if (playlist_entry_id < 0 || playlist_entry_id > 65535 ||
        playlist_id < 0 || playlist_id > 65535 ||
        command_id < 0 || command_id > 65535) {
        return false;
    }
    record->playlist_entry_id = (uint16_t)playlist_entry_id;
    record->playlist_id = (uint16_t)playlist_id;
    record->command_id = (uint16_t)command_id;
    record->duration_ms = (uint32_t)duration_ms;
    record->target_type = (uint8_t)playlist_target_type_from_row(row);

//    printf("parse_playlist_row 5 , record->target_type = %d \n", record->target_type);


    record->target_bitfield = json_get_u8_value(row, playlist_target_bitfield_keys, sizeof(playlist_target_bitfield_keys) / sizeof(playlist_target_bitfield_keys[0]), 0u);

//    printf("parse_playlist_row 6 , record->target_bitfield = %d \n", record->target_bitfield);

    return true;
}

static bool build_playlist_sequence(int channel, const PlaylistRequest *request, PlaylistSequence *sequence, int recursion_depth)
{
    if (request == NULL || sequence == NULL || request->playlist_id == 0) {
    	printf("No build request 1 \n");
        return false;
    }
    if (recursion_depth == 0) {
        playlist_sequence_reset(sequence);
    }

    const uint32_t channel_bit = (channel >= 0 && channel < NUMBER_OF_CHANNELS) ? (1u << channel) : 0;
//    printf("channel_bit = %ld \n", channel_bit);

    if (channel_bit == 0) {
        return false;
    }

    for (size_t i = 0; i < playlist_record_count && sequence->count < MAX_PLAYLIST_STEPS; ++i) {
        const PlaylistEntryRecord *rec = &playlist_records[i];
        if (rec->playlist_id != request->playlist_id) {
            continue;
        }

        uint32_t mask = resolve_playlist_entry_target_mask(
            rec->target_type,
            rec->target_bitfield,
            request->target_type_override != TARGET_TYPE_COUNT,
            (PlaylistTargetType)request->target_type_override,
            request->has_target_bitfield_override != 0u,
            request->target_bitfield_override
        );
        if ((mask & channel_bit) == 0) {
            continue;
        }

        const CommandEntry *command = find_command_entry(rec->command_id);

//        printf("command id= %d \n", command->command_id);
        if (command == NULL) {
            continue;
        }

//        printf("command type = %d \n", command->type);

        if (command->type == COMMAND_TYPE_PLAYLIST) {
            if (recursion_depth >= 1) {
                continue;
            }
            uint16_t child_id = command->nested_playlist_id;
            if (child_id <= 0) {
                continue;
            }
            PlaylistRequest child_req = *request;
            child_req.playlist_id = child_id;
            build_playlist_sequence(channel, &child_req, sequence, 1);
            continue;
        }

        PlaylistStep step = {0};
        step.command_id = rec->command_id;
        step.duration_ms = rec->duration_ms;
        step.command = command;
        step.target_type = rec->target_type;
        step.target_bitfield = rec->target_bitfield;
        if (!playlist_sequence_append(sequence, &step)) {
            break;
        }
    }
    return sequence->count > 0;
}

/* Build one sequence for playlist_id (all entries); Duration is absolute offset from cycle start. */
static bool build_playlist_sequence_by_id(int playlist_id, PlaylistSequence *sequence, int recursion_depth)
{
    if (sequence == NULL || playlist_id <= 0) {
        return false;
    }
    if (recursion_depth == 0) {
        playlist_sequence_reset(sequence);
    }

    for (size_t i = 0; i < playlist_record_count && sequence->count < MAX_PLAYLIST_STEPS; ++i) {
        const PlaylistEntryRecord *rec = &playlist_records[i];
        if (rec->playlist_id != playlist_id) {
            continue;
        }

        const CommandEntry *command = find_command_entry((int)rec->command_id);
        if (command == NULL) {
            continue;
        }

        if (command->type == COMMAND_TYPE_PLAYLIST) {
            if (recursion_depth >= 1) {
                continue;
            }
            uint16_t child_id = command->nested_playlist_id;
            if (child_id <= 0) {
                continue;
            }
            build_playlist_sequence_by_id((int)child_id, sequence, 1);
            continue;
        }

        PlaylistStep step = {0};
        step.command_id = rec->command_id;
        step.duration_ms = rec->duration_ms;
        step.command = command;
        step.target_type = rec->target_type;
        step.target_bitfield = rec->target_bitfield;
        if (!playlist_sequence_append(sequence, &step)) {
            break;
        }
    }
    return sequence->count > 0;
}

static bool parse_virtual_group_row(cJSON *row, VirtualGroup *group)
{
    if (row == NULL || group == NULL) {
        return false;
    }

    int id = json_get_int_value(row, virtual_group_id_keys, sizeof(virtual_group_id_keys) / sizeof(virtual_group_id_keys[0]), -1);
    if (id < 0) {
        return false;
    }

    group->id = id;
    const char *name = json_get_string_value(row, virtual_group_name_keys, sizeof(virtual_group_name_keys) / sizeof(virtual_group_name_keys[0]));
    if (name != NULL) {
        strncpy(group->name, name, sizeof(group->name) - 1);
        group->name[sizeof(group->name) - 1] = '\\0';
    } else {
        group->name[0] = '\\0';
    }

    uint32_t mask_value = 0;
    size_t mask_key_count = sizeof(virtual_group_mask_keys) / sizeof(virtual_group_mask_keys[0]);
    cJSON *mask_item = find_json_field_case_insensitive(row, virtual_group_mask_keys, mask_key_count);
    if (mask_item != NULL) {
        if (cJSON_IsNumber(mask_item)) {
            mask_value = (uint32_t)mask_item->valueint;
        } else if (cJSON_IsString(mask_item) && mask_item->valuestring != NULL) {
            char *endptr = NULL;
            mask_value = (uint32_t)strtoul(mask_item->valuestring, &endptr, 0);
            if (endptr == mask_item->valuestring) {
                mask_value = 0;
            }
        }
    }

    group->channel_mask = (uint8_t)(mask_value & 0xFF);
    return true;
}

static void TurnFlagsOff(int channel)
{

	TapeMeasureStartFlag[channel-1]=0;
	ExecuteCustomStartFlag[channel-1]=0;
	RacingStartFlag[channel-1]=0;
	MarqueeExecuteCustomStartFlag[channel-1]=0;
	RippleStartFlag[channel-1]=0;
	SparkleStartFlag[channel - 1]=0;
	MultiColorSparkleStartFlag[channel - 1]=0;
	SparkleParamObject_start[channel -1].Width = 0;
	SparkleParamObject_end[channel -1].Width = 0;
	MultiColorSparkleParamObject_start[channel -1].Width = 0;
	MultiColorSparkleParamObject_end[channel -1].Width = 0;
	ExecuteCustomStartFlag_offset1[channel-1] = 0;
	MarqueeCustomStartFlag_offset1[channel-1] = 0;
	enableMirror_uint8[channel-1] = 0;
	oscP_Flag[channel-1] = 0;
	oscStart_time[channel-1] = 0;
	One_LED_time[channel-1] = 0;
	One_LED_time_back[channel-1] = 0;
	PatternStartFlag_start[channel-1] = 0;
	HueSatStartFlag[channel-1] = 0;
	setColorStartFlag[channel-1] = 0;
	colorIndexStartFlag[channel-1] = 0;

	if( (rampData[channel-1].DwellTimeSceneVal == 0) && (rampData[channel-1].RampTimeSceneVal == 0) && (flag_direct_array_testing_2 == 0) )
	{
		StripChanOFF( channel);
		StripChanOFF( channel);
	}
}

float fast_sinf(float x) {
    // 0) Guard bad inputs
    if (!isfinite(x)) {
        return 0.0f;
    }

    // 1) Range-reduce: k = round(x / TWO_PI)
    int k = (int)(x * INV_TWO_PI + (x >= 0.0f ? 0.5f : -0.5f));
    x -= (float)k * TWO_PI;

    // 2) Now x ? [�p�+p]; shift into [0�2p)
    if (x < 0.0f) {
        x += TWO_PI;
    }

    // 3) Map into [0�LUT_SIZE)
    float pos  = x * INV_TWO_PI * (float)LUT_SIZE;
    int   idx  = (int)pos;           // base index
    float frac = pos - (float)idx;   // interp fraction

    // 4) Clamp & wrap indices
    if (idx < 0)         idx = 0;
    else if (idx >= LUT_SIZE) idx = LUT_SIZE - 1;
    int idx_n = (idx + 1 == LUT_SIZE) ? 0 : idx + 1;

    // 5) Linear interpolate
    float y0 = sin_lut[idx];
    float y1 = sin_lut[idx_n];
    return y0 + frac * (y1 - y0);
}

static inline void swap_buffers(int i)
{
    use_ping_buffer[i] = !use_ping_buffer[i];
}

/**
 * Function to restrict and scale RGB values to a maximum while maintaining the ratio between them.
 * If any of the individual R, G, B values or the total exceeds their respective maximum values,
 * all values are scaled down proportionally to ensure no limits are exceeded.
 */
static inline void restrict_and_scale_RGB(uint16_t *r, uint16_t *g, uint16_t *b, float v)
{
	float v1 = v*0.01f;

    float max_R 	= light_para.max_R_float;
    float max_G 	= light_para.max_G_float;
    float max_B 	= light_para.max_B_float;
    float max_Total = light_para.max_Total_float;

    float per_R = 0.0f;
    float per_G = 0.0f;
    float per_B = 0.0f;

    per_R = (float)*r*inv_max_Value;
    per_G = (float)*g*inv_max_Value;
    per_B = (float)*b*inv_max_Value;
//    printf("Test percent  -> R: %f, G: %f, B: %f\n", per_R, per_G, per_B);

    float cur_R = 0.0f;
    float cur_G = 0.0f;
    float cur_B = 0.0f;
    float cur_RGB = 0.0f;

    float cur_R1 = 0.0f;
    float cur_G1 = 0.0f;
    float cur_B1 = 0.0f;

    float cur_R2 = 0.0f;
    float cur_G2 = 0.0f;
    float cur_B2 = 0.0f;

    float cur_R3 = 0.0f;
    float cur_G3 = 0.0f;
    float cur_B3 = 0.0f;

    float scale_R = 1.0f;
    float scale_G = 1.0f;
    float scale_B = 1.0f;
    float scale_RGB = 1.0f;

    cur_R = slope_R * per_R;
    cur_G = slope_G * per_G;
	cur_B = slope_B * per_B;

	// --- Stage 1: clamp by max_R ---
	if (cur_R > max_R && fabsf(cur_R) > EPS1) {
		scale_R = max_R / cur_R;
	}

    cur_R1 = scale_R * cur_R;
    cur_G1 = scale_R * cur_G;
	cur_B1 = scale_R * cur_B;


    // --- Stage 2: clamp by max_G ---
    if (cur_G1 > max_G && fabsf(cur_G1) > EPS1) {
        scale_G = max_G / cur_G1;
    }

    cur_R2 = scale_G * cur_R1;
    cur_G2 = scale_G * cur_G1;
	cur_B2 = scale_G * cur_B1;

    // --- Stage 3: clamp by max_B ---
    if (cur_B2 > max_B && fabsf(cur_B2) > EPS1) {
        scale_B = max_B / cur_B2;
    }

    cur_R3 = scale_B * cur_R2;
    cur_G3 = scale_B * cur_G2;
	cur_B3 = scale_B * cur_B2;

	cur_RGB = cur_R3 + cur_G3 + cur_B3;


    if (cur_RGB > max_Total && fabsf(cur_RGB) > EPS1) {
        scale_RGB = max_Total / cur_RGB;
    }

	cur_R = cur_R3*scale_RGB;
	cur_G = cur_G3*scale_RGB;
	cur_B = cur_B3*scale_RGB;

    per_R = cur_R*v1*inv_slope_R;
    per_G = cur_G*v1*inv_slope_G;
    per_B = cur_B*v1*inv_slope_B;

    // Clamp to [0..1] before scaling to avoid overflow
    if (per_R < 0) {per_R = 0;} if (per_R > 1.0f) {per_R = 1.0f;}
    if (per_G < 0) {per_G = 0;} if (per_G > 1.0f) {per_G = 1.0f;}
    if (per_B < 0) {per_B = 0;} if (per_B > 1.0f) {per_B = 1.0f;}

    *r = (uint16_t)(per_R * max_Value);
    *g = (uint16_t)(per_G * max_Value);
    *b = (uint16_t)(per_B * max_Value);
}

static int put_array(AMessage_st* s_Message_Rx)
{
//	string FLAG
#ifdef ENABLE_PRINT_MSG
	printf("Parsing JSON payload...\n");
#endif

	cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);

	if (json == NULL) {
#ifdef ENABLE_PRINT_MSG
		printf("Invalid JSON input.\n");
#endif
		Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
		return -1;
	}

#ifdef ENABLE_PRINT_MSG
    Color16 out_colors[MAX_COLORS_RGB_ARRAY];
#endif

    unsigned char decoded[3 * 2 * MAX_COLORS_RGB_ARRAY];  // 600 bytes max
    size_t decoded_len = 0;

//    char b64_str[COMMAND_LEN];
	cJSON *colors = cJSON_GetObjectItem(json, "colors_b64");
	if ((colors != NULL) && (colors->valuestring != NULL))  // && (smtp_Response == 0)
	{
		memset(direct_array_testing_str,0,sizeof(direct_array_testing_str));
		strcpy(direct_array_testing_str, colors->valuestring); // = cJSON_GetStringValue(file_data_value);  // after using this we get some garbage data at the end
#ifdef ENABLE_PRINT_MSG
		printf("direct_array_testing_str = %s \n", direct_array_testing_str);
#endif
	}
	int input_size = strlen(direct_array_testing_str);

	input_size = input_size*3/4;


        // Decode Base64
        if (mbedtls_base64_decode(decoded, sizeof(decoded), &decoded_len,
                                  (const unsigned char *)direct_array_testing_str, strlen(direct_array_testing_str)) != 0) {
        	printf("Base64 decode failed!\n");
            return -1;
        }

#ifdef ENABLE_PRINT_MSG
        int count = decoded_len / 6;  // 6 bytes per color (R16 + G16 + B16)
        if (count > MAX_COLORS_RGB_ARRAY) count = MAX_COLORS_RGB_ARRAY;


        for (int i = 0; i < count; i++) {
            out_colors[i].r = (decoded[i*6] << 8) | decoded[i*6 + 1];
            out_colors[i].g = (decoded[i*6 + 2] << 8) | decoded[i*6 + 3];
            out_colors[i].b = (decoded[i*6 + 4] << 8) | decoded[i*6 + 5];
            printf("Color[%d] = R:%u G:%u B:%u\n", i,
                   out_colors[i].r, out_colors[i].g, out_colors[i].b);
        }
#endif

	    flag_direct_array_testing_2 = 1;

		TurnFlagsOff(1);
		TurnFlagsOff(2);
		TurnFlagsOff(3);
		TurnFlagsOff(4);

	    int Chan = 0;
	    for(int k=0; k<4; k++)
	    {
	    	Chan = k;

	    	swap_buffers(Chan);

			uint16_t *data_channel =
				(use_ping_buffer[Chan]) ? data_channels_ping[Chan] : data_channels[Chan];


			memcpy(&data_channel[0], &decoded[0], input_size);

			// Again for other buffer
	    	swap_buffers(Chan);

			uint16_t *data_channel_p =
				(use_ping_buffer[Chan]) ? data_channels_ping[Chan] : data_channels[Chan];


			memcpy(&data_channel_p[0], &decoded[0], input_size);

//			memcpy(&data_channel[0], &decoded[0], MAX_COLORS_RGB_ARRAY * 3 * 2);
	    }

	    flag_direct_array_testing = 1;

		delay_same_array = 1;
		flag_not_rmt = 0;

	    cJSON_Delete(json);
	    return 1;

}
static int get_channel_status(AMessage_st* s_Message_Rx)
{
//	string FLAG
#ifdef ENABLE_PRINT_MSG
    printf("Parsing JSON payload...\n");
#endif

    cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);

    if (json == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Invalid JSON input.\n");
#endif
        Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
        return -1;
    }

	char Off_com[256] = {0};
	char flag[10] = {0};

    char channelStr[10] = {0};               // Buffer to hold the string representation of the integer
//    char ON_com[2048] = {0};

    cJSON *flagItem = cJSON_GetObjectItem(json, "FLAG");
    strcpy(flag, flagItem->valuestring);

    // Create the root JSON object
    cJSON *root = cJSON_CreateObject();

    // Create the channelStatus array
    cJSON *channelStatusArray = cJSON_CreateArray();

    int d2csend_or_not = 0;
    // Create the 4 channel objects
    for (int channel = 1; channel <= 4; channel++) {

    	if(light_LastCommandPara[channel - 1].timeChanged == 0)
    	{
    		d2csend_or_not++;
    	}

        cJSON *channelObj = cJSON_CreateObject();  // Create an object for each channel

		if(light_LastCommandPara[channel - 1].state == 1)
		{
			strcpy(ON_com, "<LIGHTING.ON(");

		    // Parse the JSON string
		    cJSON *json1 = cJSON_Parse(light_LastCommandPara[channel - 1].payload);
		    if (json1 == NULL) {
		        printf("Error parsing JSON1\n");
		    }

		    // Get the "CH" array
		    cJSON *ch_array = cJSON_GetObjectItem(json1, "CH");
		    if (cJSON_IsArray(ch_array) && cJSON_GetArraySize(ch_array) == 1) {
		        // Get the first element of the array
		        cJSON *ch_value = cJSON_GetArrayItem(ch_array, 0);
		        if (cJSON_IsNumber(ch_value)) {
		            // Replace the "CH" array with the integer value
		            int ch_int = ch_value->valueint;
		            cJSON_ReplaceItemInObject(json1, "CH", cJSON_CreateNumber(ch_int));
		        }
		    }

		    // Convert the modified JSON object to a string
		    char *modified_json_str = cJSON_PrintUnformatted(json1);

		    strcat(ON_com, modified_json_str);

		    if (modified_json_str != NULL) {
#ifdef ENABLE_PRINT_MSG
//		        printf("Modified JSON:\n%s\n", modified_json_str);
#endif
		        free(modified_json_str); // Free the allocated memory
		    }

		    // Clean up
		    cJSON_Delete(json1);

			strcat(ON_com,")");

#ifdef ENABLE_PRINT_MSG
//			printf(" ON_com = %s \n", ON_com);
#endif
			cJSON_AddStringToObject(channelObj, "status", ON_com);  // Add status object

			cJSON_AddNumberToObject(channelObj, "BRIGHTNESS", light_LastCommandPara[channel - 1].brightness);

			cJSON_AddStringToObject(channelObj, "SOURCE", light_LastCommandPara[channel - 1].source);
		}
		else if(light_LastCommandPara[channel - 1].state == 0)
		{
			strcpy(Off_com, "<LIGHTING.OFF({\"CH\":");

		    // Convert the integer to a string
		    snprintf(channelStr, sizeof(channelStr), "%d", channel);
			strcat(Off_com, channelStr);
			strcat(Off_com, "})");

#ifdef ENABLE_PRINT_MSG
//			printf("Off_com = %s", Off_com);
#endif
			cJSON_AddStringToObject(channelObj, "status", Off_com);  // Add status object

			cJSON_AddStringToObject(channelObj, "SOURCE", light_LastCommandPara[channel - 1].source);
		}

		{
			{
				cJSON_AddStringToObject(channelObj, "changeReason", light_LastCommandPara[channel - 1].changeReason);
				
				if ((!strcmp(light_LastCommandPara[channel - 1].changeReason, "eventId")) || (!strcmp(light_LastCommandPara[channel - 1].changeReason, "DEFER_eventId")))
    			{
    				cJSON_AddNumberToObject(channelObj, "optionalChangeID", light_LastCommandPara[channel - 1].optionalChangeID);	
    			}
			}
		}
#ifdef ENABLE_PRINT_MSG
//		printf("light_LastCommandPara[channel - 1].timeChanged = %llu, gmt_val = %d \n", light_LastCommandPara[channel - 1].timeChanged, gmt_val);
#endif
		uint64_t current_epos_sec11 = (uint64_t)(light_LastCommandPara[channel - 1].timeChanged + (gmt_val*60*1000));	// Added GMT for local time
#ifdef ENABLE_PRINT_MSG
//		printf("current_epos_sec11 1= %llu \n", current_epos_sec11 );
#endif
//		current_epos_sec11 =(current_epos_sec11/1000)-EPOSCH_TO_30_YEAR;
		current_epos_sec11 =(current_epos_sec11 * 0.001)-EPOSCH_TO_30_YEAR;

#ifdef ENABLE_PRINT_MSG
//		printf("current_epos_sec11 2= %llu \n", current_epos_sec11 );
#endif

		date_time_t       sdate_tim;
		char str[100] ={0};

		if(dst_val == 1)
		{
			current_epos_sec11 += 3600;
		}
		// Convert to local time
		epoch_to_date_time(&sdate_tim,current_epos_sec11);

		sprintf(str,"%02d-%02d-%02d %02d:%02d:%02d", sdate_tim.year, sdate_tim.month, sdate_tim.date, sdate_tim.hour, sdate_tim.minute, sdate_tim.second); // Years since 1900

    // Add timeChanged
		cJSON_AddStringToObject(channelObj, "timeChanged", str);

        // Add the channel object to the channelStatus array
        cJSON_AddItemToArray(channelStatusArray, channelObj);
    }


	{
        {
        	uint64_t current_epos_sec11 = get_current_time_ms();
        	current_epos_sec11 = (uint64_t)(current_epos_sec11 + (gmt_val*60*1000));	// Added GMT for local time
#ifdef ENABLE_PRINT_MSG
	//		printf("current_epos_sec11 1= %llu \n", current_epos_sec11 );
#endif
//			current_epos_sec11 =(current_epos_sec11/1000)-EPOSCH_TO_30_YEAR;
        	current_epos_sec11 =(current_epos_sec11 * 0.001)-EPOSCH_TO_30_YEAR;
#ifdef ENABLE_PRINT_MSG
	//		printf("current_epos_sec11 2= %llu \n", current_epos_sec11 );
#endif
			date_time_t       sdate_tim1;
			char str1[100] ={0};

			if(dst_val == 1)
			{
				current_epos_sec11 += 3600;
			}
			// Convert to local time
			epoch_to_date_time(&sdate_tim1,current_epos_sec11);

			sprintf(str1,"%02d-%02d-%02d %02d:%02d:%02d", sdate_tim1.year, sdate_tim1.month, sdate_tim1.date, sdate_tim1.hour, sdate_tim1.minute, sdate_tim1.second); // Years since 1900

        	cJSON_AddStringToObject(root, "currentLocalTime", str1);
        }
		// Add the channelStatus array to the root object
		cJSON_AddItemToObject(root, "channelStatus", channelStatusArray);
	}

    // Print the JSON to a preallocated buffer

    payLoadDataEvtExe[0]='\0';

    if (cJSON_PrintPreallocated(root, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false)) {
#ifdef ENABLE_PRINT_MSG
        printf("Generated JSON:\n%s\n", payLoadDataEvtExe);
#endif
        strcpy(((char*)s_Message_Rx->payload_p8),payLoadDataEvtExe);
    } else {
#ifdef ENABLE_PRINT_MSG
        printf("Error: Big size data.\n");
#endif
        Add_Response_msg("Error: Big size data.", s_Message_Rx, payLoadDataEvtExe);
        cJSON_Delete(root);
        root = NULL;
        cJSON_Delete(json);
        return -1;
    }

    if(d2csend_or_not >= 4)	//Dont send d2c command, as no command received yet
    {
#ifdef ENABLE_PRINT_MSG
    	//printf("Not send D2C \n");
#endif
    	strcpy(flag, "FALSE");
    }

    // Send D2C_MESSAGE
    if ((!strcmp(flag, "TRUE")) || (!strcmp(flag, "true")))
    {
#ifdef ENABLE_PRINT_MSG
//    	printf("flag1 %s \n", flag);
#endif
		cJSON *D2CPayload = cJSON_CreateObject();
		if(D2CPayload != NULL)
		{
			cJSON_AddItemToObject(D2CPayload, "PAYLOAD", root);

			payLoadDataEvtExe[0]='\0';
			cJSON_PrintPreallocated(D2CPayload, payLoadDataEvtExe, sizeof(payLoadDataEvtExe), false);
			Send_CMD_To_Other_Actor(IHUB,"IHUB", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "D2C_MESSAGE");
	
		    {
		    	strcat(payLoadDataEvtExe, "\n");
		    	Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadDataEvtExe, strlen(payLoadDataEvtExe), "SAVE_EVENT_LOG");
		    }

			if(D2CPayload != NULL)
			{
				cJSON_Delete(D2CPayload);
				D2CPayload = NULL;
				root = NULL;
			}
		}
    }
    if(root != NULL)
    {
        // Clean up the JSON object
        cJSON_Delete(root);
        root = NULL;
    }
    cJSON_Delete(json);

    return 1;
}

static int get_rgb_value(AMessage_st* s_Message_Rx)
{

#ifdef ENABLE_PRINT_MSG
    printf("Parsing JSON payload...\n");
#endif

    cJSON *json = cJSON_Parse((char*)s_Message_Rx->payload_p8);

    if (json == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Invalid JSON input.\n");
#endif
        Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
        return -1;
    }

#ifdef ENABLE_PRINT_MSG
    printf("Extracting 'CH' array from JSON...\n");
#endif

    uint16_t red=0, green=0, blue=0, position = 0, red1=0, green1=0, blue1=0;
    char json_buffer[128];  // Create a buffer to hold the JSON string

    cJSON *channelArray = cJSON_GetObjectItem(json, "CH");
    cJSON *positionItem = cJSON_GetObjectItem(json, "POSITION");
    position = positionItem->valueint;

    if (channelArray == NULL || !cJSON_IsArray(channelArray) || cJSON_GetArraySize(channelArray) == 0) {
#ifdef ENABLE_PRINT_MSG
        printf("Error: Invalid or missing 'CH' array in JSON 14.\n");
#endif
        Add_Response_msg("Error: Invalid or missing 'CH' array in JSON 14.", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

    if (cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == 0) {
#ifdef ENABLE_PRINT_MSG
        printf("Invalid channel.\n");
#endif
        Add_Response_msg("Invalid channel", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

    // Validate JSON structure
    if ((position == 0) ||
        ( position > EXAMPLE_LED_NUMBERS)) {

        Add_Response_msg("Invalid position", s_Message_Rx, payLoadData);
        cJSON_Delete(json);
        return -1;
    }

    int useAllChannels = cJSON_GetArraySize(channelArray) == 1 && cJSON_GetArrayItem(channelArray, 0)->valueint == -1;
#ifdef ENABLE_PRINT_MSG
    printf("Use all channels: %d\n", useAllChannels);
#endif

    for (int j = 0; j < (useAllChannels ? NUMBER_OF_CHANNELS : cJSON_GetArraySize(channelArray)); j++) {
        int channel = useAllChannels ? j + 1 : cJSON_GetArrayItem(channelArray, j)->valueint;
#ifdef ENABLE_PRINT_MSG
        printf("Processing channel: %d\n", channel);
#endif

        if (channel >= 1 && channel <= NUMBER_OF_CHANNELS)
        {
        	uint16_t* data_channel = (use_ping_buffer[channel-1]) ? data_channels_ping[channel-1] : data_channels[channel-1];

        	red = data_channel[(position-1) * 3];
        	green = data_channel[ (position-1) * 3 + 1];
        	blue = data_channel[ (position-1) * 3 + 2];

        	red1=swap_endianness(red);
        	green1=swap_endianness(green);
        	blue1=swap_endianness(blue);

#ifdef ENABLE_PRINT_MSG
//        	printf("Test RGB Value  -> R: %u, G: %u, B: %u\n", red1, green1, blue1);
#endif

        	snprintf(json_buffer, sizeof(json_buffer), "{\"CH\":[%d], \"POSITION\": %d, \"RED\": %d, \"GREEN\": %d, \"BLUE\": %d}",
        			channel, position, red1, green1, blue1);

        	strcpy(((char*)s_Message_Rx->payload_p8),json_buffer);

        } else {
#ifdef ENABLE_PRINT_MSG
            printf("Error: Invalid channel (out of range).\n");
#endif
            Add_Response_msg("Error: Invalid channel (out of range).", s_Message_Rx, payLoadData);
            cJSON_Delete(json);
            return -1;
        }
    }

    cJSON_Delete(json);
    return 1;
}

static void epoch_to_date_time(date_time_t* date_time,unsigned long epoch)
{
   date_time->second = epoch%60; epoch /= 60;
   date_time->minute = epoch%60; epoch /= 60;
   date_time->hour   = epoch%24; epoch /= 24;
   unsigned long years = epoch/(365*4+1)*4; epoch %= 365*4+1;
   unsigned long year;
   for (year=3; year>0; year--)
   {
       if (epoch >= days_1[year][0])
           break;
   }
   unsigned long month;
   for (month=11; month>0; month--)
   {
       if (epoch >= days_1[year][month])
           break;
   }
   date_time->year  = years+year;
   date_time->month = month+1;
   date_time->date   = epoch-days_1[year][month]+1;
}

static void Send_D2C(void)
{
	char line2[10] = {0};

	strcpy(line_Cmd, "<LIGHTING.STATUS(");

//------------------

    cJSON *json_Command = cJSON_CreateObject();
    if (json_Command == NULL) {
        fprintf(stderr, "Error creating cJSON object\n");
    }

    	cJSON_AddStringToObject(json_Command, "FLAG", "true");

    // Output the JSON string

    payLoadData_Running[0]='\0';
	cJSON_PrintPreallocated(json_Command, payLoadData_Running, sizeof(payLoadData_Running), false);
#ifdef ENABLE_PRINT_MSG
    printf("json_string_Command = %s\n", payLoadData_Running);
#endif

    cJSON_Delete(json_Command);

    strcat(line_Cmd, payLoadData_Running);
	strcpy(line2, ")");
	strcat(line_Cmd, line2);

#ifdef ENABLE_PRINT_MSG
	printf("line= %s \n", line);
	printf("line= %s \n", line);
#endif
	int ret;
	if(line_Cmd[0]=='<')
	{
		esp_err_t err = esp_console_run_Custom(line_Cmd, &ret, THIS_ACTOR);
		if (err == ESP_ERR_NOT_FOUND)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Unrecognized command\n");
	#endif
		}
		else if (err == ESP_ERR_INVALID_ARG)
		{
			// command was empty
		}
		else if (err == ESP_OK && ret != ESP_OK)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
			esp_err_to_name(ret));
	#endif
		}
		else if (err != ESP_OK)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("Internal error: %s\n", esp_err_to_name(err));
	#endif
		}
	}
}

static void set_gmt_dst(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;

	char str[100] = {0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
	}
	else
	{
		name_JSON = cJSON_GetObjectItem(in_JSON, "GMT_VAL");
		if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
		{
			gmt_val = name_JSON->valueint;
		}
		name_JSON = cJSON_GetObjectItem(in_JSON, "DST_VAL");
		if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
		{
			dst_val = name_JSON->valueint;
		}
		cJSON_Delete(in_JSON);
#ifdef ENABLE_PRINT_MSG
//		printf("gmt_value1 = %d, dst_value1 = %d, \n",gmt_val, dst_val);
#endif
	}
}

static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value)
{

	char out_val[200] = {0};
	cJSON   *my_JSON  	= cJSON_CreateObject();
	switch(data_type)
	{
		case U_INT8  :	sprintf((char*)out_val,	"%d",	*(uint8_t *) 	value);	break;
		case U_INT16 :	sprintf((char*)out_val,	"%d",	*(uint16_t *) 	value);	break;
		case INT	 :	sprintf((char*)out_val,	"%d", 	*(int *) 		value);	break;
//		case FLOAT   :  sprintf((char*)out_val,	"%f", 	(float) 	value); break;
		case STRING  :	sprintf((char*)out_val,	"%s",	(char*) 	value);	break;
		default 	 : 	break;
	}
	cJSON_AddStringToObject(my_JSON, parameter, (char*)out_val);

	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);

	uint8_t *newpointer = (uint8_t*) heap_caps_calloc(strlen((char*) payLoadData) + 1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		cJSON_Delete(my_JSON);
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payLoadData);
	s_Message_Tx.payload_size = strlen((char*)payLoadData);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor

void RampAndDwellFunction(int Chan, uint64_t u64CurrentTime)
{
	uint64_t u64Ramp_DiffTime = 0, time_remain = 0;
    uint16_t red=0, green=0, blue=0;
    float offset_float = 0.0f;

    flag_not_rmt = 0;

    u64Ramp_DiffTime = u64CurrentTime - rampData[Chan].RampStartTime;

    time_remain = rampData[Chan].RampTimeSceneVal - u64Ramp_DiffTime;

    float output = 0.0f;
    if (rampData[Chan].RampTimeSceneVal > 0)
    {
        output = (float)time_remain / rampData[Chan].RampTimeSceneVal;
    }

	if(output > 1)
	{
		output = 1;
	}
	output = 1 - output;  // Reverse the output for interpolation

#ifdef ENABLE_PRINT_MSG
	printf("output  = %f \n", output);
#endif

	if(!strcmp(rampData[Chan].function_start, "Pattern"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
//    		vTaskDelay(50 / portTICK_PERIOD_MS);
    		StripChanOFF( Chan+1);
    	}
		patternExecuteProc(Chan, u64CurrentTime, 1);

    	rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if(!strcmp(rampData[Chan].function_end, "Pattern"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		patternExecuteProc(Chan, u64CurrentTime, 2);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}

	if(!strcmp(rampData[Chan].function_start, "Sparkle"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		decayledProc(Chan, u64CurrentTime, 1);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if(!strcmp(rampData[Chan].function_end, "Sparkle"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		decayledProc(Chan, u64CurrentTime, 2);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}

	if(!strcmp(rampData[Chan].function_start, "MultiColorSparkle"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
    	MultiColordecayledProc(Chan, u64CurrentTime, 1);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if(!strcmp(rampData[Chan].function_end, "MultiColorSparkle"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
    	MultiColordecayledProc(Chan, u64CurrentTime, 2);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}


	if((!strcmp(rampData[Chan].function_start, "Ripple")) || (!strcmp(rampData[Chan].function_start, "Wave")))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		RippleContinious(Chan, u64CurrentTime, 1);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if((!strcmp(rampData[Chan].function_end, "Ripple"))  || (!strcmp(rampData[Chan].function_start, "Wave")))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		RippleContinious(Chan, u64CurrentTime, 2);
		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}

	if(!strcmp(rampData[Chan].function_start, "Marquee"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		offset_float = Marquee_Moving_Tap_Offset(Chan, u64CurrentTime, 1);

		ExecuteMarquee_PrepareDataWithModeSetting(offset_float, Chan, 1, u64CurrentTime, 1);

		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if(!strcmp(rampData[Chan].function_end, "Marquee"))
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		offset_float = Marquee_Moving_Tap_Offset(Chan, u64CurrentTime, 2);

		ExecuteMarquee_PrepareDataWithModeSetting(offset_float, Chan, 1, u64CurrentTime, 2);

		rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}

	if( (!strcmp(rampData[Chan].function_start, "tapeMeasure")) || (!strcmp(rampData[Chan].function_start, "Custom")) || (!strcmp(rampData[Chan].function_start, "Cascade")) )
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		offset_float = Moving_Tap_Offset(Chan, u64CurrentTime, 1);
	    Execute_PrepareDataWithModeSetting(offset_float, Chan, 1, 1);
	    rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
	if( (!strcmp(rampData[Chan].function_end, "tapeMeasure")) || (!strcmp(rampData[Chan].function_end, "Custom")) || (!strcmp(rampData[Chan].function_start, "Cascade")) )
	{
    	if(rampData[Chan].RampTimeSceneVal != 0)
    	{
    		StripChanOFF( Chan+1);
    		StripChanOFF( Chan+1);
    	}
		offset_float = Moving_Tap_Offset(Chan, u64CurrentTime, 2);

	    Execute_PrepareDataWithModeSetting(offset_float, Chan, 1, 2);
	    rampData[Chan].RampTimeSceneVal = 0;		//No ramp in case of effects
	}
    // Calculate the brightness factor based on global and channel-specific settings
    float brightness_factor = light_para.contrMaxB_float * 0.01;
    float channelMaxB_factors[] =
    {
        light_para.chan1MaxB_float * 0.01,
        light_para.chan2MaxB_float * 0.01,
        light_para.chan3MaxB_float * 0.01,
        light_para.chan4MaxB_float * 0.01
    };
    brightness_factor *= channelMaxB_factors[Chan];
    if (brightness_RunTimeChan[Chan] != 0)
    {
        brightness_factor *= brightness_RunTimeChan[Chan] * 0.01;
    }

    int i = 0;

    for (int pos = 1; pos <= EXAMPLE_LED_NUMBERS; pos++)
    {
    	i = pos-1;
    	red=0, green=0, blue=0;

		if(u64Ramp_DiffTime >= rampData[Chan].RampTimeSceneVal)
		{
			hsv_to_rgb_16bit(rampData[Chan].hue_end[i], rampData[Chan].sat_end[i], 100, &red, &green, &blue);
			restrict_and_scale_RGB(&red, &green, &blue, (rampData[Chan].val_end[i]*brightness_factor));
		}
		else
		{
			// Initialize color start and end points based on the pattern flag
			Color start, end, result;
			if ( (!strcmp(rampData[Chan].function_start, "HSV")) || (!strcmp(rampData[Chan].function_start, "PWM")) || (!strcmp(rampData[Chan].function_start, "colorIndex")) || (!strcmp(rampData[Chan].function_start, "Pattern")) || (!strcmp(rampData[Chan].function_start, "Sparkle")) || (!strcmp(rampData[Chan].function_start, "MultiColorSparkle")) || (!strcmp(rampData[Chan].function_start, "Ripple")) || (!strcmp(rampData[Chan].function_start, "Wave")) || (!strcmp(rampData[Chan].function_start, "tapeMeasure")) || (!strcmp(rampData[Chan].function_start, "Custom")) || (!strcmp(rampData[Chan].function_start, "Cascade")) || (!strcmp(rampData[Chan].function_start, "OFF")) || (!strcmp(rampData[Chan].function_start, "Marquee")) )
			{
				start.hue = rampData[Chan].hue_start[i];
				start.saturation = rampData[Chan].sat_start[i]  * 0.01;
				start.brightness = (rampData[Chan].val_start[i] * brightness_factor)  * 0.01;
#ifdef ENABLE_PRINT_MSG
				printf("saturation = %f, brightness =  %f \n", start.saturation, start.brightness);
#endif
			}
			if ( (!strcmp(rampData[Chan].function_end, "HSV")) || (!strcmp(rampData[Chan].function_end, "PWM")) || (!strcmp(rampData[Chan].function_end, "colorIndex")) || (!strcmp(rampData[Chan].function_end, "Pattern")) || (!strcmp(rampData[Chan].function_end, "Sparkle")) || (!strcmp(rampData[Chan].function_end, "MultiColorSparkle")) || (!strcmp(rampData[Chan].function_end, "Ripple")) || (!strcmp(rampData[Chan].function_end, "Wave"))  || (!strcmp(rampData[Chan].function_end, "tapeMeasure")) || (!strcmp(rampData[Chan].function_end, "Custom")) || (!strcmp(rampData[Chan].function_end, "Cascade")) || (!strcmp(rampData[Chan].function_end, "OFF")) || (!strcmp(rampData[Chan].function_end, "Marquee")) )
			{
				end.hue = rampData[Chan].hue_end[i];
				end.saturation = rampData[Chan].sat_end[i]  * 0.01;
				end.brightness = (rampData[Chan].val_end[i] * brightness_factor)  * 0.01;
			}
			// Interpolate color based on the output factor
			interpolate_color(&result, &start, &end, output);
			result.brightness *= 100;
			result.saturation *= 100;

			// Convert interpolated HSV values to RGB
			hsv_to_rgb_16bit(result.hue, result.saturation, 100, &red, &green, &blue);
			// Call the restrict and scale function
			restrict_and_scale_RGB(&red, &green, &blue, result.brightness);
		}

#ifdef ENABLE_PRINT_MSG
    printf("Test Case : After Scaling  -> R: %u, G: %u, B: %u\n", red, green, blue);
#endif

        set_led_color((uint8_t)(Chan + 1), (uint16_t)pos, (uint16_t)red, (uint16_t)green, (uint16_t)blue);
    }

	int offset = 0;
	PrepareDataWithModeSetting(offset, Chan, 1);
}

// Function to convert 16-bit RGB (0-65535) to HSV (Hue: 0-360, Saturation & Brightness: 0-100)
void rgb16_to_hsv(uint16_t R, uint16_t G, uint16_t B, float *H, float *S, float *V)
{
    // Normalize RGB to range [0,1]
    float r = R / 65535.0;
    float g = G / 65535.0;
    float b = B / 65535.0;

    float C_max = fmaxf(r, fmaxf(g, b));  // Max color component
    float C_min = fminf(r, fminf(g, b));  // Min color component
    float delta = C_max - C_min;          // Difference

    // Compute Hue (0 - 360)
    if (delta == 0)
    {
        *H = 0;
    }
    else if (C_max == r)
    {
        *H = 60 * fmodf((g - b) / delta, 6);
    }
    else if (C_max == g)
    {
        *H = 60 * ((b - r) / delta + 2);
    }
    else if (C_max == b)
    {
        *H = 60 * ((r - g) / delta + 4);
    }

    if (*H < 0)
    {
        *H += 360;
    }

    // Compute Saturation (0 - 100)
    *S = (C_max == 0) ? 0 : (delta / C_max) * 100;

    // Compute Brightness (0 - 100)
    *V = C_max * 100;
}

bool channel_equal(int ch)
{
    return memcmp(
        data_channels[ch],
        data_channels_ping[ch],
        bytes_per_channel[ch]
    )==0;
}


//========================================================================================================================================================
#if defined(B542)

#define PTO_DEFAULT_TIMEBASE_RESOLUTION_HZ  (1000000)   // Default 1MHz, 1us per tick
#define PTO_DEFAULT_FREQUENCY_HZ            (20000)     // Default 20000 ticks, 20ms
#define PTO_DEFAULT_DUTY_CYCLE_PERCENT      (50)        // Default 50% duty cycle
#define T5_PTO_DRIVER_MAX_CHANNELS          (3)


static const uint8_t output_mapping[] =
{
    GPIO_NUM_15, // 0 -> GPIO 18  RED
    GPIO_NUM_16, // 1 -> GPIO 21  GREEN
    GPIO_NUM_17, // 2 -> GPIO 16  BLUE
};

static mcpwm_timer_handle_t timer[T5_PTO_DRIVER_MAX_CHANNELS] = {NULL};
static mcpwm_oper_handle_t oper[T5_PTO_DRIVER_MAX_CHANNELS] = {NULL};
static mcpwm_cmpr_handle_t comparator[T5_PTO_DRIVER_MAX_CHANNELS] = {NULL};
static mcpwm_gen_handle_t generator[T5_PTO_DRIVER_MAX_CHANNELS] = {NULL};

#define PTO_Polarity_Active_High 0
#define PTO_Polarity_Active_Low 1


// Apply 16-bit RGB color to PWM channel (scaled to 13-bit LEDC duty)
static void apply_rgb_pwm_to_channel(int ch, uint16_t r16, uint16_t g16, uint16_t b16)
{

	pto_set_duty(0, PWM_FREQ_HZ, r16, PTO_Polarity_Active_Low); // R = 100%
	pto_set_duty(1, PWM_FREQ_HZ, g16,   PTO_Polarity_Active_Low); // G = 0%
	pto_set_duty(2, PWM_FREQ_HZ, b16,   PTO_Polarity_Active_Low); // B = 0%

}

void config_pto(uint8_t point, uint32_t frequency, uint8_t duty, uint8_t polarity, uint8_t enable)
{
    {
        {

            gpio_config_t io_conf; // Structure to hold GPIO configuration
            // Configure output GPIOs
            io_conf.intr_type = GPIO_INTR_DISABLE; // Disable interrupts for output pins
            io_conf.mode = GPIO_MODE_OUTPUT;       // Set mode to output
            io_conf.pin_bit_mask = 0;              // Initialize pin mask for outputs
            io_conf.pin_bit_mask |= (1ULL << output_mapping[point]); // Add GPIO to pin mask
            io_conf.pull_down_en = 0; // Disable pull-down resistors
            io_conf.pull_up_en = 0;   // Disable pull-up resistors
            gpio_config(&io_conf);    // Apply configuration for output pins

            mcpwm_timer_config_t timer_config = {
                .group_id = 0,
                .clk_src = MCPWM_TIMER_CLK_SRC_DEFAULT,
                .resolution_hz = PTO_DEFAULT_TIMEBASE_RESOLUTION_HZ,
                .period_ticks = (PTO_DEFAULT_TIMEBASE_RESOLUTION_HZ / frequency),
                .count_mode = MCPWM_TIMER_COUNT_MODE_UP,
            };
            ESP_ERROR_CHECK(mcpwm_new_timer(&timer_config, &timer[point]));

            mcpwm_operator_config_t operator_config =
            {
                .group_id = 0, // operator must be in the same group to the timer
            };
            ESP_ERROR_CHECK(mcpwm_new_operator(&operator_config, &oper[point]));

            // printf("Connecting timer and operator\n");
            ESP_ERROR_CHECK(mcpwm_operator_connect_timer(oper[point], timer[point]));

            // printf("Creating comparator and generator from the operator\n");
            mcpwm_comparator_config_t comparator_config =
            {
                .flags.update_cmp_on_tez = true,
            };
            ESP_ERROR_CHECK(mcpwm_new_comparator(oper[point], &comparator_config, &comparator[point]));

            mcpwm_generator_config_t generator_config =
            {
                .gen_gpio_num = output_mapping[point],
            };
            ESP_ERROR_CHECK(mcpwm_new_generator(oper[point], &generator_config, &generator[point]));

            uint32_t newCompartorValue = (((PTO_DEFAULT_TIMEBASE_RESOLUTION_HZ / frequency) * duty) / 100);
            // set the initial compare value, so that the servo will spin to the center position
            ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[point], newCompartorValue));

            // printf("Set generator action on timer and compare event\n");

            if(PTO_Polarity_Active_High == polarity)
            {
                // go high on counter empty
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator[point],
                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_EMPTY, MCPWM_GEN_ACTION_HIGH)));
                // go low on compare threshold
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator[point],
                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[point], MCPWM_GEN_ACTION_LOW)));
            }
            else
            {
                // go high on counter full
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_timer_event(generator[point],
                                MCPWM_GEN_TIMER_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, MCPWM_TIMER_EVENT_FULL, MCPWM_GEN_ACTION_HIGH)));
                // go low on compare threshold
                ESP_ERROR_CHECK(mcpwm_generator_set_action_on_compare_event(generator[point],
                                MCPWM_GEN_COMPARE_EVENT_ACTION(MCPWM_TIMER_DIRECTION_UP, comparator[point], MCPWM_GEN_ACTION_LOW)));
            }

            // printf("Enable and start timer\n");
            ESP_ERROR_CHECK(mcpwm_timer_enable(timer[point]));
            ESP_ERROR_CHECK(mcpwm_timer_start_stop(timer[point], MCPWM_TIMER_START_NO_STOP));
        }
    }
}

// Helper: update duty without reinitializing the channel
static inline void pto_set_duty(uint8_t point, uint32_t frequency, uint8_t duty_percent, uint8_t polarity)
{
    if (duty_percent > 100)
    	duty_percent = 100;
    uint32_t period = PTO_DEFAULT_TIMEBASE_RESOLUTION_HZ / frequency;

    // For active-low (common-anode), invert the duty so more "on" = more low time
    uint32_t effective = (polarity == PTO_Polarity_Active_Low) ? (100 - duty_percent) : duty_percent;

    uint32_t cmp = (period * effective) / 100;
    ESP_ERROR_CHECK(mcpwm_comparator_set_compare_value(comparator[point], cmp));
}
#endif

static void Racing_ApplyParams(int Chan, const RacingParams *p)
{
    g_racing_state[Chan].params = *p;                                         // store per channel
}

/*** ----------- INTERNAL HELPERS ----------- ***/
static inline float pick_length(const RacingParams *p)
{
	return randf_range(p->min_len_in,  p->max_len_in);
}
static inline float pick_speed (const RacingParams *p)
{
	return randf_range(p->min_speed_in_s, p->max_speed_in_s);
}

/*** ----------- INIT/RESET PER CHANNEL ----------- ***/
static void Racing_InitChannel(int Chan)
{
    RacingState *st = &g_racing_state[Chan];                                  // get channel state
    ChannelParameters *ch = &ChannelParamObject[Chan];                        // get channel params

    const RacingParams *p = &st->params;

    // resolve pitch (inches) either from override or from ChannelParam in mm
    if (p->override_pitch_in)
    {                                               // if override is enabled
        st->pitch_in = (p->pitch_in_inches > 0.0f) ? p->pitch_in_inches : 3.0f; // safe fallback
    } else {                                                                  // else use ChannelParam mm
        float mm = ch->LEDspacingCh_float;                                    // spacing in mm from firmware
        if (mm <= 0.0f) mm = 76.2f;                                           // fallback to 3.0 inches in mm
        st->pitch_in = mm * (1.0f / 25.4f);                                   // convert mm -> inches
    }

    st->inv_pitch    = (st->pitch_in > 0.0f) ? (1.0f / st->pitch_in) : (1.0f / 3.0f); // precompute inverse
    st->active_leds  = ch->SetLEDstripal_u16;                                  // cache active LED count
    st->strip_len_in = st->active_leds * st->pitch_in;                         // compute total inches
    st->forward_map  = (ch->revDirCh_u8 == 0);                                 // forward if revDir is 0

    // clamp desired number of cars to storage cap
    int want = p->fixed_cars;                                                  // desired cars
    if (want < 1) want = 1;                                                    // ensure at least 1
    if (want > p->max_cars_cap) want = p->max_cars_cap;                        // clamp to per-channel cap
    if (want > (int)(sizeof(st->cars) / sizeof(st->cars[0])))                  // clamp to static storage
        want = (int)(sizeof(st->cars) / sizeof(st->cars[0]));
#ifdef ENABLE_PRINT_MSG
    printf("want = %d \n", want);
#endif
    if (p->num_colors > 0)
    {
    	st->numCars = p->num_colors;                                                        // store actual count
    }
    else
    {
        st->numCars = want;                                                        // store actual count
    }

    uint64_t now = get_current_time_ms();                                      // current time

    // initialize all cars
    for (int i = 0; i < st->numCars; i++)
    {                                    // loop cars
        RacingCar *c = &st->cars[i];                                           // alias car pointer
        c->length_in         = pick_length(p);                                 // random length
        c->speed_in_s        = pick_speed(p);                                  // random speed
        c->target_speed_in_s = c->speed_in_s;                                  // initial target = current

        // spawn policy
        if (p->spawn_mode == 0 || i < (st->numCars / 2))
        {                     												   // if on-screen spawn
            int attempts = 0;                                                  // rejection attempts
            for (;;)
            {                                                         		   // loop until fits
                attempts++;                                                    // bump attempts
                c->tail_in = randf_range(0.0f, fmaxf(0.0f, st->strip_len_in - c->length_in)); // random tail
                int ok = 1;                                                    // assume placement ok
                for (int j = 0; j < i; j++)
                {                                  // test spacing vs earlier cars
                    float L0 = c->tail_in - p->min_start_spacing_in;           // left padded
                    float R0 = c->tail_in + c->length_in + p->min_start_spacing_in; // right padded
                    float L1 = st->cars[j].tail_in - p->min_start_spacing_in;  // earlier car left padded
                    float R1 = st->cars[j].tail_in + st->cars[j].length_in + p->min_start_spacing_in; // right padded
                    if (!(R0 < L1 || R1 < L0)) { ok = 0; break; }              // overlap => not ok
                }
                if (ok || attempts > 80) break;                                // accept after tries
            }
        }
        else
        {                                                                // off-screen spawn
            float len = c->length_in;                                          // car length
            c->tail_in = randf_range(-(len + p->reentry_gap_in), -p->reentry_gap_in); // start negative
        }

        c->next_retarget_ms = now + p->retarget_min_ms + (fx_rand32() % (p->retarget_jitter_ms + 1)); // next retarget

        if(p->num_colors == 0)
        {
			float hue = (360.0f * (float)i) / (float)st->numCars;                  // evenly spaced hues
			c->color.hue        = hue;                                             // set hue
			c->color.saturation = 100.0f;                                          // full saturation
			c->color.brightness = 100.0f;                                          // full brightness

			hsv_to_rgb_16bit(c->color.hue, c->color.saturation, 100.0f,            // precompute car RGB (full V)
							 &c->rgb_r, &c->rgb_g, &c->rgb_b);                     // store RGB once
        }
        else
        {
        	if(i < p->num_colors)
        	{
        		c->color.brightness = 100.0f;                                          // full brightness
    			// Pick from supplied array
    	//        int idx = car_index % c->num_colors;
        		c->rgb_r = p->colors[i][0];
        		c->rgb_g = p->colors[i][1];
        		c->rgb_b = p->colors[i][2];
        	}
        }
#ifdef ENABLE_PRINT_MSG
        printf("r = %d, g = %d, b = %d \n", c->rgb_r, c->rgb_g, c->rgb_b);
#endif
    }

    st->last_update_ms = now;                                                  // store update time
    st->initialized    = 1;                                                    // mark ready
}

static void Racing_Respawn(RacingState *st, int idx)
{
    const RacingParams *p = &st->params;                                       // alias params
    RacingCar *c = &st->cars[idx];                                             // alias car
    c->length_in         = pick_length(p);                                     // new length
    c->speed_in_s        = pick_speed(p);                                      // new speed
    c->target_speed_in_s = c->speed_in_s;                                      // reset target to current

    float len = c->length_in;                                                  // local length
    c->tail_in = randf_range(-(len + p->reentry_gap_in), -p->reentry_gap_in);  // re-enter from negative

    c->next_retarget_ms = get_current_time_ms()                                // schedule next retarget
                        + p->retarget_min_ms
                        + (fx_rand32() % (p->retarget_jitter_ms + 1));

    if (p->num_colors == 0)
    {
        // fallback: keep your old random generator
        hsv_to_rgb_16bit(c->color.hue, c->color.saturation, 100.0f,                // refresh cached RGB (same color)
                         &c->rgb_r, &c->rgb_g, &c->rgb_b);                         // store
    }

}

/*** ----------- BRIGHTNESS FACTOR (your style) ----------- ***/
static float get_brightness_factor_for_channel(int Chan)
{
    float f = (light_para.contrMaxB_float * 0.01f);                            // apply global cap
    switch (Chan + 1)
    {                                                        // apply per-channel cap
        case 1: f *= (light_para.chan1MaxB_float * 0.01f); break;
        case 2: f *= (light_para.chan2MaxB_float * 0.01f); break;
        case 3: f *= (light_para.chan3MaxB_float * 0.01f); break;
        case 4: f *= (light_para.chan4MaxB_float * 0.01f); break;
        default: break;
    }
    if (brightness_RunTimeChan[Chan] != 0.0f)
    {                                // if runtime brightness set
        f *= (brightness_RunTimeChan[Chan] * 0.01f);                           // scale by runtime %
    }
    return f;                                                                   // return factor
}

/*** ----------- ADVANCE SIMULATION ----------- ***/
static void Racing_Advance(int Chan, uint64_t now_ms)
{
    RacingState *st = &g_racing_state[Chan];                                   // channel state
    if (!st->initialized)
    {                                                    // if not ready
        Racing_InitChannel(Chan);                                              // init now
    }

    const RacingParams *p = &st->params;                                       // alias params

    float dt_s = (float)(now_ms - st->last_update_ms) * 0.001f;                // compute dt in seconds
    if (dt_s < 0.0f) dt_s = 0.0f;                                              // guard negative
    if (dt_s > p->max_dt_s) dt_s = p->max_dt_s;                                // clamp long pauses
    st->last_update_ms = now_ms;                                               // store time

    // 1) Retarget & accelerate toward target (bounded accel) for each car
    for (int i = 0; i < st->numCars; i++)
    {                                    // loop cars
        RacingCar *c = &st->cars[i];                                           // alias car
        if (now_ms >= c->next_retarget_ms)
        {                                   // time to pick new target?
            c->target_speed_in_s = pick_speed(p);                               // pick new target speed
            c->next_retarget_ms  = now_ms + p->retarget_min_ms                   // schedule next retarget
                                  + (fx_rand32() % (p->retarget_jitter_ms + 1));
        }
        float dv     = c->target_speed_in_s - c->speed_in_s;                   // delta velocity
        float max_dv = p->max_accel_in_s2 * dt_s;                              // max change this frame
        if (dv >  max_dv) dv =  max_dv;                                        // clamp up
        if (dv < -max_dv) dv = -max_dv;                                        // clamp down
        c->speed_in_s += dv;                                                   // apply velocity change
    }

    // 2) Proposed motion (no collisions yet)
    //    Keep this small array on stack; size bounded by max_cars_cap.
    float proposed_tail[DEF_MAX_CARS_CAP];                                     // proposed new tails
    for (int i = 0; i < st->numCars; i++)
    {                                    // loop cars
        proposed_tail[i] = st->cars[i].tail_in + st->cars[i].speed_in_s * dt_s;// tail + v*dt
    }

    // 3) Optional collision avoidance (single strip head→tail)
    if (p->enable_collision_avoidance)
    {                                       // if enabled at runtime
        int order[DEF_MAX_CARS_CAP];                                           // index order by position
        for (int i = 0; i < st->numCars; i++) order[i] = i;                    // fill indices

        // insertion sort by current tail position (ascending)
        for (int a = 1; a < st->numCars; a++)
        {                                // sort loop
            int key = order[a];                                                // key index
            int b = a - 1;                                                     // scan left
            while (b >= 0 && st->cars[order[b]].tail_in > st->cars[key].tail_in)
            {
                order[b + 1] = order[b];                                       // shift right
                b--;                                                           // continue
            }
            order[b + 1] = key;                                                // insert
        }

        // enforce gap for follower relative to immediate leader
        for (int pidx = 0; pidx < st->numCars - 1; pidx++)
        {                   // for all follower/leader pairs
            int i = order[pidx];                                               // follower index
            int j = order[pidx + 1];                                           // leader index

            RacingCar *F = &st->cars[i];                                       // follower car
            RacingCar *L = &st->cars[j];                                       // leader car

            float allowed_head = L->tail_in - st->params.min_collision_gap_in; // max head position
            float follower_head_new = proposed_tail[i] + F->length_in;         // follower head after move

            if (follower_head_new > allowed_head)
            {                            // would violate gap?
                float clamped_tail = allowed_head - F->length_in;              // push tail back
                if (clamped_tail < F->tail_in) clamped_tail = F->tail_in;      // no backward slide
                float disp = clamped_tail - F->tail_in;                        // displacement
                if (dt_s > 1e-6f) F->speed_in_s = disp / dt_s;                 // adjust speed this frame
                proposed_tail[i] = clamped_tail;                               // commit clamped tail
            }
        }
    }

    // 4) Commit motion & respawn when a car finishes the strip
    for (int i = 0; i < st->numCars; i++)
    {                                    // loop cars
        RacingCar *c = &st->cars[i];                                           // alias car
        float new_tail = proposed_tail[i];                                     // proposed tail
        if (new_tail >= st->strip_len_in)
        {                                    // finished entire segment?
            Racing_Respawn(st, i);                                             // respawn off-screen
        }
        else
        {                                                               // otherwise
            c->tail_in = new_tail;                                             // commit tail
        }
    }
}

/*** ----------- RENDER (optimized inner loop) ----------- ***/
static float get_channel_brightness_factor(int Chan)
{                         // small wrapper
    return get_brightness_factor_for_channel(Chan);                            // reuse your math
}

void Execute_RacingEffect(int Chan, uint64_t now_ms)
{
    RacingState *st = &g_racing_state[Chan];                                   // channel state

    Racing_Advance(Chan, now_ms);                                              // step simulation

    uint16_t *data_channel = (use_ping_buffer[Chan]) ? data_channels_ping[Chan] // pick live buffer
                                                     : data_channels[Chan];

    // Clear staging buffer for this channel (memset once)
    switch (Chan)
    {
        case 0: memset(&data_channels1_1[0], 0, EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 1: memset(&data_channels1_2[0], 0, EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 2: memset(&data_channels1_3[0], 0, EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 3: memset(&data_channels1_4[0], 0, EXAMPLE_LED_NUMBERS * 3 * 2); break;
        default: break;
    }

    const int   N        = EXAMPLE_LED_NUMBERS;                                // total physical buffer
    const int   activeN  = st->active_leds;                                    // logical active LEDs
    const int   forward  = st->forward_map;                                    // forward mapping?
    const float invPitch = st->inv_pitch;                                      // 1/pitch_in (precomputed)

    // Compute one brightness factor for the whole channel once per frame
    const float bf = get_channel_brightness_factor(Chan);                      // channel brightness factor

    // Draw each car
    for (int k = 0; k < st->numCars; k++)
    {                                    // loop cars
        RacingCar *c = &st->cars[k];                                           // alias car

        float tail = c->tail_in;                                               // car tail
        float head = c->tail_in + c->length_in;                                // car head

        float visL = (tail > 0.0f) ? tail : 0.0f;                              // left clip
        float visR = (head < st->strip_len_in) ? head : st->strip_len_in;      // right clip
        if (visR <= visL) continue;                                            // fully off-screen => skip

        // Convert inches -> LED index using LED centers at (i + 0.5) * pitch
        // We approximate by using floor((x / pitch) - 0.5) == floor(x * invPitch - 0.5) to avoid divides
        int first_i = (int)floorf(visL * invPitch - 0.5f);                     // first LED index
        int last_i  = (int)floorf(visR * invPitch - 0.5f);                     // last LED index
        if (first_i < 0) first_i = 0;                                          // clamp low
        if (last_i >= activeN) last_i = activeN - 1;                           // clamp high
        if (last_i < first_i) continue;                                        // empty span => skip

        // Scale brightness **once** per car; same for every LED in the segment
        float v_scaled = c->color.brightness * bf;                              // (0..100) * factor
#ifdef ENABLE_PRINT_MSG
        printf("v_scaled = %f \n", v_scaled);
#endif
        // Make local copies of base RGB so we can scale in place once
        uint16_t r = c->rgb_r;                                                 // base R (16-bit)
        uint16_t g = c->rgb_g;                                                 // base G (16-bit)
        uint16_t b = c->rgb_b;                                                 // base B (16-bit)
        restrict_and_scale_RGB(&r, &g, &b, v_scaled);                          // apply brightness scaling once

        // Emit to LEDs
        for (int i = first_i; i <= last_i; i++)
        {                              // loop LEDs in span
            int led_index;                                                     // mapped LED index
            if (forward)
            {                                                     // forward mapping
                led_index = i;                                                 // same index
            }
            else
            {                                                           // reverse mapping
                if (i < activeN)
                	led_index = (activeN - 1) - i;                // reverse within active range
                else
                	led_index = (N - 1)      - (i - activeN);     // outside active range (safety)
            }
            set_led_color((uint8_t)(Chan + 1), (uint16_t)(led_index + 1), r, g, b); // set LED color
        }
    }

    // Final memcpy from staging into live channel buffer (your pipeline)
    switch (Chan)
    {
        case 0: memcpy(&data_channel[0], &data_channels1_1[0], EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 1: memcpy(&data_channel[0], &data_channels1_2[0], EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 2: memcpy(&data_channel[0], &data_channels1_3[0], EXAMPLE_LED_NUMBERS * 3 * 2); break;
        case 3: memcpy(&data_channel[0], &data_channels1_4[0], EXAMPLE_LED_NUMBERS * 3 * 2); break;
        default: break;
    }
}

/*** ----------- PUBLIC HELPERS ----------- ***/
void Racing_ResetChannel(int Chan)
{
    memset(&g_racing_state[Chan], 0, sizeof(RacingState));                     // zero state
}

void Racing_SetParams(int Chan, const RacingParams *p)
{
    Racing_ApplyParams(Chan, p);                                               // copy params
}

void Racing_InitWithParams(int Chan, const RacingParams *p)
{
    Racing_ApplyParams(Chan, p);                                               // copy params
    Racing_InitChannel(Chan);                                                  // init state
}
