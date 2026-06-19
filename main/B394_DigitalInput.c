/*
 * B394_DigitalInput.c
 * B394-only: 8 digital inputs (DI0..DI7), debounce, combine window,
 * single/combination mode, slot/bank/action_code, rising-edge D2C Input Event.
 * Implemented as a full actor: queue, monitor task, INIT, PROPERTIES, HELP.
 * When B394 is not defined, this file provides a no-op init only.
 */

#if defined(B394)

#include "B394_DigitalInput.h"
#include "actor.h"
#include "Console_Actor.h"
#include "Config.h"
#include "cJSON.h"
#include "driver/gpio.h"
#include "esp_mac.h"
#include "esp_app_desc.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "sys/time.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

static const char * THIS_ACTOR = "B394_DI";

/* --- GPIO mapping (B394 / ESP32-S3-WROOM-1) --- */
/* DI0..DI5 = slot bits (INP_1..INP_6); DI6, DI7 = bank select */
//#define DI0_GPIO   GPIO_NUM_4   /* INP_1 */
//#define DI1_GPIO   GPIO_NUM_5   /* INP_2 */
//#define DI2_GPIO   GPIO_NUM_7   /* INP_3 */
//#define DI3_GPIO   GPIO_NUM_15  /* INP_4 */
//#define DI4_GPIO   GPIO_NUM_16  /* INP_5 */
//#define DI5_GPIO   GPIO_NUM_8   /* INP_6 */
//#define DI6_GPIO   GPIO_NUM_2   /* bank bit 0 (assign per hardware) */
//#define DI7_GPIO   GPIO_NUM_3   /* bank bit 1 (assign per hardware) */
#if defined(B394)
/* B394: shorter timeout so D2C (e.g. digital input events) is sent within ~2–3 s when connected */
#define sampleazureiotPROCESS_LOOP_TIMEOUT_MS                 ( 2000U )
#else
#define sampleazureiotPROCESS_LOOP_TIMEOUT_MS                 ( 60000U )
#endif

#define DI0_GPIO   GPIO_NUM_15   /* INP_1 */
#define DI1_GPIO   GPIO_NUM_16   /* INP_2 */
#define DI2_GPIO   GPIO_NUM_17   /* INP_3 */
#define DI3_GPIO   GPIO_NUM_18   /* INP_4 */
//#define DI4_GPIO   GPIO_NUM_7   /* INP_5 */
#define DI4_GPIO   GPIO_NUM_21   /* INP_5 */
#define DI5_GPIO   GPIO_NUM_39   /* INP_6 */
#define DI6_GPIO   GPIO_NUM_47   /* bank bit 0 (assign per hardware) */
#define DI7_GPIO   GPIO_NUM_48   /* bank bit 1 (assign per hardware) */

#define EXT_WDI_KICK   GPIO_NUM_8

#define DEBOUNCE_MS_DEFAULT       1000
#define POLL_PERIOD_MS            10
#define B394_DI_TASK_STACK        8192
#define B394_DI_TASK_PRIORITY     5
#define B394_DI_RX_QUEUE_LEN      16
#define B394_DI_MONITOR_STACK    4096
#define B394_DI_MONITOR_PRIORITY  4
#define B394_DI_JSON_FILE        "A:/System/B394_DI.json"
//#define B394_HTTP_TRIGGER_PATH   "/api/AutomationLinkManagement/triggerAutomationLink"
//#define B394_HTTP_TRIGGER_PATH   "/api/Api/triggerAutomationLink"
#define B394_HTTP_TRIGGER_PATH   "/DeviceApi/triggerAutomationLink"

#define B394_HTTP_URL_MAX_LEN    384
#define B394_HTTP_TIMEOUT_MS_DEFAULT 1500
#define B394_ROUTE_D2C_ONLY      0
#define B394_ROUTE_HTTP_ONLY     1
#define B394_ROUTE_HTTP_AND_D2C  2

static const gpio_num_t s_di_gpios[] = {
	DI0_GPIO, DI1_GPIO, DI2_GPIO, DI3_GPIO,
	DI4_GPIO, DI5_GPIO, DI6_GPIO, DI7_GPIO
};

static char direct_http_api_key[100];
static char automation_url[256];

/* Property table for GET/SET and file persistence (same pattern as WIFI actor) */
PSRAM_ATTR_BSS static struct b394_di_parameter {
	uint32_t debounce_ms;
	uint32_t event_route_mode;
	uint32_t direct_http_timeout_ms;
} s_B394_Para;

PSRAM_ATTR static struct property s_B394_prop[] = {
	{ &s_B394_Para.debounce_ms, "DEBOUNCE_MS", U_INT32, "RW", "Edge debounce time in ms" },
	{ &s_B394_Para.event_route_mode, "EVENT_ROUTE_MODE", U_INT32, "RW", "0=D2C only, 1=HTTP PUT only, 2=HTTP PUT + D2C" },
	{ &s_B394_Para.direct_http_timeout_ms, "DIRECT_HTTP_TIMEOUT_MS", U_INT32, "RW", "HTTP PUT timeout in ms" }

};
#define B394_PROP_COUNT (sizeof(s_B394_prop) / sizeof(s_B394_prop[0]))

static uint8_t b394_di_set(char *property, char *value, AMessage_st *s_Message_Rx);
static void b394_di_get(char *prop, char *val_a8, size_t val_len);
static void b394_di_Get_Property(AMessage_st *s_Message_Rx);
static void b394_di_get_actor_properties(AMessage_st *s_Message_Rx);
static void b394_di_help(AMessage_st *s_Message_Rx);
static void b394_di_Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void b394_di_Analyse_Response(AMessage_st *s_Message_Rx);
static void b394_di_task(void *pvParameters);

/* Actor queue and state */
static uint8_t s_B394_DI_Rx_QueueStorage[B394_DI_RX_QUEUE_LEN * sizeof(AMessage_st)];
static StaticQueue_t s_B394_DI_Rx_QueueBuffer;
static QueueHandle_t s_B394_DI_Rx_Queue = NULL;
static TaskHandle_t s_B394_DI_MonitorHandle = NULL;
static TaskHandle_t s_B394_DI_PollHandle = NULL;
static bool s_B394_DI_FirstEntry = false;
static bool s_B394_DI_GPIO_inited = false;
static volatile bool s_B394_Waiting_System_ApiKey = false;

PSRAM_ATTR_BSS static char s_B394_Rx_buffer[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static char s_B394_Response_buffer[MAX_JSON_PAYLOAD_BYTES];
/* Buffer for EVENT to CONSOLE (debug UART + D2C); console overwrites with COMMAND+RESPONSE. */
PSRAM_ATTR_BSS static char s_B394_EventToConsole_buf[MAX_JSON_PAYLOAD_BYTES + COMMAND_LEN];
//PSRAM_ATTR_BSS static char s_B394_d2c_http_buf[MAX_JSON_PAYLOAD_BYTES + COMMAND_LEN];
/* HTTP buffer for array format: [ {...} ] */
PSRAM_ATTR_BSS static char s_B394_HttpArrayPayload_buf[MAX_JSON_PAYLOAD_BYTES + COMMAND_LEN + 4];

static void b394_di_monitor(void *pvParameters);
static void b394_di_init_actor(void);
static void b394_di_add_response(const char *buffer, AMessage_st *s_Message_Rx);
static bool b394_di_is_absolute_url(const char *url);
static void b394_di_refresh_direct_http_api_key(void);
static void b394_di_send_direct_http_post(const char *payload, const char *device_id);
static void b394_di_send_d2c_via_console(const char *payload);

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES];

#define POPCOUNT(m) ((m)&1)+(((m)>>1)&1)+(((m)>>2)&1)+(((m)>>3)&1)+(((m)>>4)&1)+(((m)>>5)&1)+(((m)>>6)&1)+(((m)>>7)&1)

static uint8_t read_raw_mask(void)
{
	uint8_t m = 0;
	for (int i = 0; i < 8; i++) {
		if (gpio_get_level(s_di_gpios[i]))
			m |= (1u << i);
	}
	return m;
}

static bool b394_di_is_absolute_url(const char *url)
{
	return (url != NULL) &&
		((strncmp(url, "https://", 8) == 0) || (strncmp(url, "http://", 7) == 0));
}

static void b394_di_send_direct_http_post(const char *payload, const char *device_id)
{
	if (!payload || payload[0] == '\0')
		return;

	b394_di_refresh_direct_http_api_key();

	char url[B394_HTTP_URL_MAX_LEN] = {0};
	strcpy(url, automation_url);

	uint32_t timeout_ms = s_B394_Para.direct_http_timeout_ms;
	if (timeout_ms == 0)
		timeout_ms = B394_HTTP_TIMEOUT_MS_DEFAULT;

	esp_http_client_config_t config = {
		.url = url,
		.method = HTTP_METHOD_POST,
		.timeout_ms = (int)timeout_ms,
		.disable_auto_redirect = true,
		.transport_type = HTTP_TRANSPORT_OVER_TCP,
	};

	if (strncmp(url, "https://", 8) == 0) {
		config.transport_type = HTTP_TRANSPORT_OVER_SSL;
		config.crt_bundle_attach = esp_crt_bundle_attach;
	}

	esp_http_client_handle_t client = esp_http_client_init(&config);
	if (client == NULL) {
		printf("<B394_DI.ERROR(HTTP client init failed)\n");
		return;
	}

	esp_http_client_set_post_field(client, payload, (int)strlen(payload));
	esp_http_client_set_header(client, "Content-Type", "application/json");
	esp_http_client_set_header(client, "Connection", "keep-alive");
	if (device_id && device_id[0] != '\0')
		esp_http_client_set_header(client, "Device", device_id);
	if (direct_http_api_key[0] != '\0')
		esp_http_client_set_header(client, "x-api-key", direct_http_api_key);

	esp_err_t err = esp_http_client_perform(client);
	int status = esp_http_client_get_status_code(client);

    // Read response body
    char response_buf[4096];
    int read_len = esp_http_client_read(client, response_buf, sizeof(response_buf) - 1);
    if (read_len >= 0)
    {
        response_buf[read_len] = '\0';
        printf("response_buf = %s \n", response_buf);
    }
    else
    {
        printf("  Failed to read response: %s\n", esp_err_to_name(read_len));
    }

	printf("<B394_DI.HTTP_POST(status=%d, err=%s, url=%s, api-key= %s, device_id= %s, payload= %s)\n", status, esp_err_to_name(err), url, direct_http_api_key, device_id, payload);
	esp_http_client_close(client);
	esp_http_client_cleanup(client);
}

static void b394_di_refresh_direct_http_api_key(void)
{
	char request_payload[128] = {0};
	cJSON *name_array = cJSON_CreateArray();
	cJSON *request = cJSON_CreateObject();

	if (!name_array || !request) {
		if (name_array) cJSON_Delete(name_array);
		if (request) cJSON_Delete(request);
		return;
	}

	cJSON_AddItemToArray(name_array, cJSON_CreateString("API_KEY"));
	cJSON_AddItemToArray(name_array, cJSON_CreateString("AUTOMATION_URL"));
	cJSON_AddItemToObject(request, "Property_Names", name_array);
	if (!cJSON_PrintPreallocated(request, request_payload, sizeof(request_payload), false)) {
		cJSON_Delete(request);
		return;
	}
	cJSON_Delete(request);

	s_B394_Waiting_System_ApiKey = true;
	b394_di_Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", request_payload, (int16_t)strlen(request_payload), "GET");

	/* Give monitor task time to consume SYSTEM RESPONSE and update local key. */
	for (int i = 0; i < 10; i++) {
		if (!s_B394_Waiting_System_ApiKey)
			break;
		vTaskDelay(pdMS_TO_TICKS(10));
	}
	s_B394_Waiting_System_ApiKey = false;
}

static void b394_di_send_d2c_via_console(const char *payload)
{
	if (!payload || payload[0] == '\0')
		return;

	AMessage_st msg = { 0 };
	msg.Dest_ID_a8 = B394_DI;
	msg.Src_ID_u8 = CONSOLE;
	msg.payload_p8 = (uint8_t *)payload;
	msg.payload_size = (uint16_t)strlen(payload);
	strcpy((char *)msg.src_Actor_a8, "CONSOLE");
	strcpy((char *)msg.dest_Actor_a8, "B394_DI");
	strcpy((char *)msg.cmdFun_a8, "EVENT");
	console_send_responce_to_console_xface(&msg);
}

/* Send input event from B394_DI to CONSOLE: debug UART shows >B394_DI.EVENT(...), console forwards to IHUB as D2C. */
static void send_d2c_input_event(uint8_t raw_mask, uint8_t bank_id, uint8_t slot_index,
	uint8_t action_code, const char *input_mode, uint64_t event_ts,
	const char *device_id, const char *device_mac, const char *fw_version)
{
	cJSON *payload_obj = cJSON_CreateObject();
	if (!payload_obj) return;

	cJSON_AddStringToObject(payload_obj, "DeviceId", device_id);
//	cJSON_AddStringToObject(payload_obj, "device_mac", device_mac);
	cJSON_AddNumberToObject(payload_obj, "TimeStamp", (double)event_ts);
	cJSON_AddStringToObject(payload_obj, "Type", input_mode);
	cJSON_AddNumberToObject(payload_obj, "Input", raw_mask);
//	cJSON_AddNumberToObject(payload_obj, "bank_id", bank_id);
//	cJSON_AddNumberToObject(payload_obj, "slot_index", slot_index);
//	cJSON_AddNumberToObject(payload_obj, "action_code", action_code);
//	cJSON_AddStringToObject(payload_obj, "fw_version", fw_version);

	memset(s_B394_EventToConsole_buf, 0, sizeof(s_B394_EventToConsole_buf));
//	memset(s_B394_d2c_http_buf, 0, sizeof(s_B394_d2c_http_buf));

//	if (!cJSON_PrintPreallocated(payload_obj, s_B394_d2c_http_buf, sizeof(s_B394_d2c_http_buf), false)) {
//
//	}

	if (!cJSON_PrintPreallocated(payload_obj, s_B394_EventToConsole_buf, sizeof(s_B394_EventToConsole_buf), false)) {
		cJSON_Delete(payload_obj);
		return;
	}
	cJSON_Delete(payload_obj);

	uint32_t mode = s_B394_Para.event_route_mode;

	int n = snprintf(s_B394_HttpArrayPayload_buf, sizeof(s_B394_HttpArrayPayload_buf), "[%s]", s_B394_EventToConsole_buf);
//	printf("s_B394_HttpArrayPayload_buf = %s \n", s_B394_HttpArrayPayload_buf);

	if (mode == B394_ROUTE_D2C_ONLY || mode == B394_ROUTE_HTTP_AND_D2C)
		b394_di_send_d2c_via_console(s_B394_EventToConsole_buf);
	if (mode == B394_ROUTE_HTTP_ONLY || mode == B394_ROUTE_HTTP_AND_D2C) {
		if (n > 0 && (size_t)n < sizeof(s_B394_HttpArrayPayload_buf))
			b394_di_send_direct_http_post(s_B394_HttpArrayPayload_buf, device_id);
		else
			printf("<B394_DI.ERROR(HTTP array payload build failed)\n");
	}
}

static void b394_di_add_response(const char *buffer, AMessage_st *s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();
	if (responseObject) {
		cJSON_AddStringToObject(responseObject, "RESP", buffer);
		memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
		if (cJSON_PrintPreallocated(responseObject, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false)) {
			s_Message_Rx->payload_p8 = (uint8_t *)s_B394_Response_buffer;
			s_Message_Rx->payload_size = (uint16_t)strlen(s_B394_Response_buffer);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
		cJSON_Delete(responseObject);
	}
}

static uint8_t b394_di_set(char *property, char *value, AMessage_st *s_Message_Rx)
{
	(void)s_Message_Rx;
	for (int i = 0; i < (int)B394_PROP_COUNT; i++) {
		if (strcmp(property, s_B394_prop[i].str_name) != 0)
			continue;
		if (strcmp(s_B394_prop[i].access, "RW") != 0)
			return 2;
		if (s_B394_prop[i].type == U_INT32)
			*(uint32_t *)s_B394_prop[i].name = (uint32_t)atoi(value);
		else if (s_B394_prop[i].type == STRING) {
			char *dst = (char *)s_B394_prop[i].name;

		}
		return 1;
	}
	return 0;
}

static void b394_di_get(char *prop, char *val_a8, size_t val_len)
{
	if (!val_a8 || val_len == 0)
		return;
	for (int i = 0; i < (int)B394_PROP_COUNT; i++) {
		if (strcmp(prop, s_B394_prop[i].str_name) != 0)
			continue;
		if (s_B394_prop[i].type == U_INT32)
			snprintf(val_a8, val_len, "%lu", (unsigned long)*(uint32_t *)s_B394_prop[i].name);
		else if (s_B394_prop[i].type == STRING)
			snprintf(val_a8, val_len, "%s", (char *)s_B394_prop[i].name);
		return;
	}
	val_a8[0] = '\0';
}

static void b394_di_Get_Property(AMessage_st *s_Message_Rx)
{
	cJSON *in = cJSON_Parse(s_B394_Rx_buffer);
	if (!in) {
		b394_di_add_response("Invalid JSON for GET.", s_Message_Rx);
		return;
	}
	cJSON *names = cJSON_GetObjectItem(in, "Property_Names");
	cJSON_Delete(in);
	if (!cJSON_IsArray(names) || cJSON_GetArraySize(names) == 0) {
		b394_di_add_response("'Property_Names' array is NULL or empty.", s_Message_Rx);
		return;
	}
	cJSON *out = cJSON_CreateObject();
	if (!out) return;
	for (int i = 0; i < cJSON_GetArraySize(names); i++) {
		cJSON *el = cJSON_GetArrayItem(names, i);
		if (!cJSON_IsString(el) || !el->valuestring) continue;
		char val[300] = {0};
		b394_di_get(el->valuestring, val, sizeof(val));
		cJSON_AddStringToObject(out, el->valuestring, val);
	}
	memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
	if (cJSON_PrintPreallocated(out, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false)) {
		s_Message_Rx->payload_p8 = (uint8_t *)s_B394_Response_buffer;
		s_Message_Rx->payload_size = (uint16_t)strlen(s_B394_Response_buffer);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	cJSON_Delete(out);
}

//static void get_actor_properties(AMessage_st* s_Message_Rx){
static void b394_di_get_actor_properties(AMessage_st *s_Message_Rx){
	char val_a8[300] = {0};
	char typeString[20] = {0};

	int no_of_elements = sizeof(s_B394_prop) / sizeof(struct property);

	    // Create JSON arrays
	    cJSON *jsonArrayName = cJSON_CreateArray();
	    cJSON *jsonArrayType = cJSON_CreateArray();
	    cJSON *jsonArrayValue = cJSON_CreateArray();
	    cJSON *jsonArrayAccess = cJSON_CreateArray();
	    cJSON *jsonArrayHelpString = cJSON_CreateArray();

	    for (int i = 0; i < no_of_elements; i++) {
			cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(s_B394_prop[i].str_name));
			// Convert DataType enum to string representation for property type
			// Add value based on data type for property name
			switch (s_B394_prop[i].type) {
				case U_INT8:
					strcpy(typeString, "U_INT8");
					sprintf(val_a8, "%d", *(uint8_t*) s_B394_prop[i].name);
					break;

				case U_INT16:
					strcpy(typeString, "U_INT16");
					sprintf(val_a8, "%d", *(uint16_t*) s_B394_prop[i].name);
					break;

				case U_INT32:
					strcpy(typeString, "U_INT32");
					sprintf(val_a8, "%ld", *(uint32_t*) s_B394_prop[i].name);
					break;

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) s_B394_prop[i].name);
					break;

				case INT:
					strcpy(typeString, "INT");
					sprintf(val_a8, "%d", *(int*) s_B394_prop[i].name);
					break;

				case INT16:
					strcpy(typeString, "INT16");
					sprintf(val_a8, "%d", *(int16_t*) s_B394_prop[i].name);
					break;

				case FLOAT:
					strcpy(typeString, "FLOAT");
					sprintf(val_a8, "%f", *(float*) s_B394_prop[i].name);
					break;

				case DOUBLE:
					strcpy(typeString, "DOUBLE");
					sprintf(val_a8, "%lf", *(double*) s_B394_prop[i].name);
					break;

				case STRING:
					strcpy(typeString, "STRING");
					snprintf(val_a8, sizeof(val_a8), "%s", (char *)s_B394_prop[i].name);
					break;

				default:
					break;
				}
			cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
			cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(val_a8));
			cJSON_AddItemToArray(jsonArrayAccess, cJSON_CreateString(s_B394_prop[i].access));
			cJSON_AddItemToArray(jsonArrayHelpString, cJSON_CreateString(s_B394_prop[i].HelpString));
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
		cJSON_Delete(jsonObject);
		{
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
}	//	get_actor_properties

//static void b394_di_get_actor_properties(AMessage_st *s_Message_Rx)
//{
//	char val_a8[32] = {0};
//	cJSON *root = cJSON_CreateObject();
//	if (!root) return;
//	cJSON *arr_name = cJSON_CreateArray();
//	cJSON *arr_type = cJSON_CreateArray();
//	cJSON *arr_value = cJSON_CreateArray();
//	cJSON *arr_access = cJSON_CreateArray();
//	cJSON *arr_help = cJSON_CreateArray();
//	if (!arr_name || !arr_type || !arr_value || !arr_access || !arr_help) {
//		cJSON_Delete(root);
//		return;
//	}
//	for (int i = 0; i < (int)B394_PROP_COUNT; i++) {
//		cJSON_AddItemToArray(arr_name, cJSON_CreateString(s_B394_prop[i].str_name));
//		cJSON_AddItemToArray(arr_type, cJSON_CreateString("U_INT32"));
//		val_a8[0] = '\0';
//		b394_di_get(s_B394_prop[i].str_name, val_a8);
//		cJSON_AddItemToArray(arr_value, cJSON_CreateString(val_a8));
//		cJSON_AddItemToArray(arr_access, cJSON_CreateString(s_B394_prop[i].access));
//		cJSON_AddItemToArray(arr_help, cJSON_CreateString(s_B394_prop[i].HelpString));
//	}
//	cJSON_AddItemToObject(root, "Property_Names", arr_name);
//	cJSON_AddItemToObject(root, "Property_Types", arr_type);
//	cJSON_AddItemToObject(root, "Property_Values", arr_value);
//	cJSON_AddItemToObject(root, "Property_Access", arr_access);
//	cJSON_AddItemToObject(root, "Property_HelpString", arr_help);
//	memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
//	if (cJSON_PrintPreallocated(root, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false)) {
//		s_Message_Rx->payload_p8 = (uint8_t *)s_B394_Response_buffer;
//		s_Message_Rx->payload_size = (uint16_t)strlen(s_B394_Response_buffer);
//		console_send_responce_to_console_xface(s_Message_Rx);
//	}
//	cJSON_Delete(root);
//}

static void b394_di_help(AMessage_st *s_Message_Rx)
{
	cJSON *root = cJSON_CreateObject();
	if (!root) return;
	cJSON_AddStringToObject(root, "INIT()", "Initialize B394_DI actor and start input polling. Loads properties from file.");
	cJSON_AddStringToObject(root, "SET(json ...)", "Set DEBOUNCE_MS, EVENT_ROUTE_MODE, DIRECT_HTTP_TIMEOUT_MS");
	cJSON_AddStringToObject(root, "GET(json Property_Names)", "Get parameters by name.");
	cJSON_AddStringToObject(root, "PROPERTIES()", "Display property table (values, access, help).");
	cJSON_AddStringToObject(root, "HELP()", "Display this help.");
	memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
	if (cJSON_PrintPreallocated(root, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false)) {
		s_Message_Rx->payload_p8 = (uint8_t *)s_B394_Response_buffer;
		s_Message_Rx->payload_size = (uint16_t)strlen(s_B394_Response_buffer);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	cJSON_Delete(root);
}

static void b394_di_Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	memset(&s_Message_Tx_new, 0, sizeof(s_Message_Tx_new));
	uint8_t *newpointer = (uint8_t *)heap_caps_calloc((size_t)size + 1, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (!newpointer) return;
	memcpy(newpointer, response, (size_t)size);
	s_Message_Tx_new.payload_p8 = newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = (uint16_t)size;
	strcpy((char *)s_Message_Tx_new.src_Actor_a8, THIS_ACTOR);
	strcpy((char *)s_Message_Tx_new.dest_Actor_a8, DestActor);
	strcpy((char *)s_Message_Tx_new.cmdFun_a8, function);
	console_ActorWriteToConsole_xface(&s_Message_Tx_new);
}

static void b394_di_Analyse_Response(AMessage_st *s_Message_Rx)
{
	if (!s_Message_Rx->payload_p8 || strlen((char *)s_Message_Rx->payload_p8) == 0)
		return;

	if (strcmp((char *)s_Message_Rx->src_Actor_a8, "SYSTEM") == 0) {
		cJSON *system_json = cJSON_Parse((char *)s_Message_Rx->payload_p8);
		if (system_json) {
			cJSON *response_obj = cJSON_GetObjectItem(system_json, "RESPONSE");
			cJSON *key_source = cJSON_IsObject(response_obj) ? response_obj : system_json;
			cJSON *api_key = cJSON_GetObjectItem(key_source, "API_KEY");
			if (cJSON_IsString(api_key) && api_key->valuestring) {
				strncpy(direct_http_api_key, api_key->valuestring, sizeof(direct_http_api_key) - 1);
				direct_http_api_key[sizeof(direct_http_api_key) - 1] = '\0';

			}

			cJSON *atomation_url_1 = cJSON_GetObjectItem(key_source, "AUTOMATION_URL");

			if (cJSON_IsString(atomation_url_1) && atomation_url_1->valuestring) {
				strncpy(automation_url, atomation_url_1->valuestring, sizeof(automation_url) - 1);
			}

			s_B394_Waiting_System_ApiKey = false;
			cJSON_Delete(system_json);
		}

		return;
	}

	if (strcmp((char *)s_Message_Rx->src_Actor_a8, "FILE_SYSTEM") != 0)
		return;
	cJSON *in = cJSON_Parse((char *)s_Message_Rx->payload_p8);
	if (!in) return;
	cJSON *cmd = cJSON_GetObjectItem(in, "COMMAND");
	if (!cJSON_IsString(cmd) || strcmp(cmd->valuestring, "READ") != 0) {
		cJSON_Delete(in);
		return;
	}
	cJSON *resp = cJSON_GetObjectItem(in, "RESPONSE");
//	cJSON_Delete(in);
	if (!cJSON_IsObject(resp))
	{
		cJSON_Delete(in);
		return;
	}
	/* First child may be FILE_NAME; rest are property key-values */
	cJSON *item = resp->child;
	while (item) {
		if (item->string && item->valuestring && strcmp(item->string, "FILE_NAME") != 0) {
			b394_di_set(item->string, item->valuestring, s_Message_Rx);
		} else if (item->string && cJSON_IsNumber(item)) {
			char val[32];
			snprintf(val, sizeof(val), "%d", item->valueint);
			b394_di_set(item->string, val, s_Message_Rx);
		}
		item = item->next;
	}
	cJSON_Delete(in);
}

static void b394_di_init_actor(void)
{
	if (s_B394_DI_FirstEntry)
		return;
	s_B394_DI_FirstEntry = true;

	if (s_B394_DI_Rx_Queue == NULL) {
		s_B394_DI_Rx_Queue = xQueueCreateStatic(B394_DI_RX_QUEUE_LEN, sizeof(AMessage_st),
			s_B394_DI_Rx_QueueStorage, &s_B394_DI_Rx_QueueBuffer);
	}
	if (s_B394_DI_Rx_Queue == NULL)
		return;

	if (!s_B394_DI_GPIO_inited) {
		gpio_config_t io = {
			.pin_bit_mask = 0,
			.mode = GPIO_MODE_INPUT,
			.pull_up_en = GPIO_PULLUP_DISABLE,
			.pull_down_en = GPIO_PULLDOWN_DISABLE,
			.intr_type = GPIO_INTR_DISABLE,
		};
		for (int i = 0; i < 8; i++) {
			io.pin_bit_mask = (1ULL << s_di_gpios[i]);
			gpio_config(&io);
		}
		s_B394_DI_GPIO_inited = true;
	}

	s_B394_Para.debounce_ms = DEBOUNCE_MS_DEFAULT;
	s_B394_Para.event_route_mode = 1; // B394_ROUTE_HTTP_AND_D2C
	s_B394_Para.direct_http_timeout_ms = B394_HTTP_TIMEOUT_MS_DEFAULT;

	gpio_set_direction(EXT_WDI_KICK, GPIO_MODE_OUTPUT);
	gpio_set_level(EXT_WDI_KICK, 0);   // Set HIGH

	if (s_B394_DI_MonitorHandle == NULL) {
		xTaskCreate(b394_di_monitor, "B394_DI_Mon", B394_DI_MONITOR_STACK, NULL, B394_DI_MONITOR_PRIORITY, &s_B394_DI_MonitorHandle);
	}
}

static void b394_di_monitor(void *pvParameters)
{
	(void)pvParameters;
	AMessage_st s_Message_Rx_data;
	AMessage_st *s_Message_Rx = &s_Message_Rx_data;

	while (1) {
		memset(&s_Message_Rx_data, 0, sizeof(AMessage_st));
		if (xQueueReceive(s_B394_DI_Rx_Queue, s_Message_Rx, portMAX_DELAY) != pdTRUE)
			continue;

		/* Copy payload to local buffer and release console's payload */
		memset(s_B394_Rx_buffer, 0, sizeof(s_B394_Rx_buffer));
		if (s_Message_Rx->payload_p8 != NULL && s_Message_Rx->payload_size > 0) {
			size_t copy_len = s_Message_Rx->payload_size;
			if (copy_len >= sizeof(s_B394_Rx_buffer))
				copy_len = sizeof(s_B394_Rx_buffer) - 1;
			memcpy(s_B394_Rx_buffer, s_Message_Rx->payload_p8, copy_len);
			console_MessageRelease_xface((char *)s_Message_Rx->payload_p8);
		}
		s_Message_Rx->payload_p8 = (uint8_t *)s_B394_Rx_buffer;
		s_Message_Rx->payload_size = (uint16_t)strlen(s_B394_Rx_buffer);

		if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "INIT")) {
			if (s_B394_DI_PollHandle == NULL) {
				xTaskCreate(b394_di_task, "B394_DI", B394_DI_TASK_STACK, NULL, B394_DI_TASK_PRIORITY, &s_B394_DI_PollHandle);
				/* Load properties from file (response handled in RESPONSE) */
				cJSON *req = cJSON_CreateObject();
				if (req) {
					cJSON_AddStringToObject(req, "FILE_NAME", B394_DI_JSON_FILE);
					memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
					if (cJSON_PrintPreallocated(req, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false))
						b394_di_Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", s_B394_Response_buffer, (int16_t)strlen(s_B394_Response_buffer), "READ");
					cJSON_Delete(req);
				}
				b394_di_add_response("B394_DI actor initialized; input polling started. Properties loaded from file.", s_Message_Rx);
			} else {
				b394_di_add_response("B394_DI already initialized.", s_Message_Rx);
			}
		} else if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "GET")) {
			b394_di_Get_Property(s_Message_Rx);
		} else if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "SET")) {
			cJSON *name_JSON = cJSON_Parse(s_B394_Rx_buffer);
			if (!name_JSON) {
				b394_di_add_response("Invalid JSON for SET.", s_Message_Rx);
			} else {
				cJSON *head = name_JSON->child;
				cJSON *root_JSON = cJSON_CreateObject();
				if (root_JSON) cJSON_AddStringToObject(root_JSON, "FILE_NAME", B394_DI_JSON_FILE);
				uint8_t u8Result = 0;
				while (head) {
					if (cJSON_IsString(head)) {
						uint8_t r = b394_di_set(head->string, head->valuestring, s_Message_Rx);
						if (r == 1 && root_JSON) {
							cJSON_AddStringToObject(root_JSON, head->string, head->valuestring);
							u8Result = 1;
						} else if (r == 2) {
							char str[80];
							snprintf(str, sizeof(str), "'%s' is read-only.", head->string);
							b394_di_add_response(str, s_Message_Rx);
						}
					} else if (cJSON_IsNumber(head)) {
						char val[32];
						snprintf(val, sizeof(val), "%d", head->valueint);
						uint8_t r = b394_di_set(head->string, val, s_Message_Rx);
						if (r == 1 && root_JSON) {
							cJSON_AddNumberToObject(root_JSON, head->string, head->valueint);
							u8Result = 1;
						} else if (r == 2) {
							char str[80];
							snprintf(str, sizeof(str), "'%s' is read-only.", head->string);
							b394_di_add_response(str, s_Message_Rx);
						}
					}
					head = head->next;
				}
				cJSON_Delete(name_JSON);
				if (u8Result && root_JSON) {
					memset(s_B394_Response_buffer, 0, sizeof(s_B394_Response_buffer));
					if (cJSON_PrintPreallocated(root_JSON, s_B394_Response_buffer, sizeof(s_B394_Response_buffer), false))
						b394_di_Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", s_B394_Response_buffer, (int16_t)strlen(s_B394_Response_buffer), "WRITE_VAR");
					cJSON_Delete(root_JSON);
					b394_di_add_response("Properties updated and saved to file.", s_Message_Rx);
				} else if (root_JSON) {
					cJSON_Delete(root_JSON);
				}
			}
		} else if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "RESPONSE")) {
			b394_di_Analyse_Response(s_Message_Rx);
		} else if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "PROPERTIES")) {
			b394_di_get_actor_properties(s_Message_Rx);
		} else if (!strcmp((char *)s_Message_Rx->cmdFun_a8, "HELP")) {
			b394_di_help(s_Message_Rx);
		} else {
			b394_di_add_response("invalid method", s_Message_Rx);
		}
	}
}

void B394_DI_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message = (AMessage_st *)msg;
	if (!s_B394_DI_FirstEntry)
		b394_di_init_actor();
	if (s_B394_DI_Rx_Queue == NULL) {
		if (s_Message->payload_p8 != NULL)
			console_MessageRelease_xface((char *)s_Message->payload_p8);
		return;
	}
	BaseType_t state = xQueueSend(s_B394_DI_Rx_Queue, s_Message, QUE_DELAY);
	if (state != pdTRUE) {
		if (s_Message->payload_p8 != NULL)
			console_MessageRelease_xface((char *)s_Message->payload_p8);
	}
}

static void b394_di_task(void *pvParameters)
{
	(void)pvParameters;

	uint8_t raw_current;
	uint8_t raw_stable = read_raw_mask();
	uint64_t last_edge_time_ms = 0;
	bool pending_send = false;

	char device_id[16];
	char device_mac[24];
	uint8_t mac[6];
	if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
		snprintf(device_id, sizeof(device_id), "000000000000");
		snprintf(device_mac, sizeof(device_mac), "00:00:00:00:00:00");
	} else {
		snprintf(device_id, sizeof(device_id), "%02X%02X%02X%02X%02X%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		snprintf(device_mac, sizeof(device_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
			mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	}

	const esp_app_desc_t *app = esp_app_get_description();
	const char *fw_version = app ? app->version : "0";

	struct timeval tv;
	uint64_t now_ms;
	static uint64_t save_ext_time = 0;

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(POLL_PERIOD_MS));

//		EXT_WDI_KICK

		raw_current = read_raw_mask();
		_gettimeofday_r(NULL, &tv, NULL);
		now_ms = (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);

		if(save_ext_time == 0)
		{
			save_ext_time = now_ms;
		}

		if((now_ms - save_ext_time) >= 1000)
		{
			gpio_set_level(EXT_WDI_KICK, 1);
			save_ext_time = now_ms;
		}
		else
		{
			gpio_set_level(EXT_WDI_KICK, 0);

		}

		if (raw_current != raw_stable) {
			raw_stable = raw_current;
			last_edge_time_ms = now_ms;
			pending_send = true;
			continue;
		}

		if (!pending_send)
			continue;

		uint32_t debounce_ms = s_B394_Para.debounce_ms;
		if (debounce_ms == 0)
			debounce_ms = DEBOUNCE_MS_DEFAULT;

		if (now_ms - last_edge_time_ms < debounce_ms)
			continue;

		pending_send = false;

		if (raw_stable == 0)
			continue;

		/* Resolve mode and validity */
		int pop = POPCOUNT(raw_stable);
		uint8_t bank_id;
		uint8_t slot_index;
		const char *input_mode;
		int valid = 0;

		if (pop == 1) {
			input_mode = "automationLink";  // "Binary";
			bank_id = 0;
			for (slot_index = 0; slot_index < 8; slot_index++) {
				if (raw_stable == (1u << slot_index))
					break;
			}
			valid = 1;
		} else {
			input_mode = "automationLink";  //"Binary";
			slot_index = raw_stable & 0x3Fu;
			bank_id = (uint8_t)((raw_stable & 0x80 ? 2 : 0) + (raw_stable & 0x40 ? 1 : 0));
			valid = 1;
//			if (bank_id == 0) {
//				valid = 1; /* allow bank 0 combinations of multiple inputs */
//			} else {
//				valid = 1; /* Banks 1, 2: slot 0..63 */
//			}
		}

		if (!valid)
			continue;

		uint8_t action_code = (uint8_t)(bank_id * 64 + slot_index);
		send_d2c_input_event(raw_stable, bank_id, slot_index, action_code,
			input_mode, now_ms, device_id, device_mac, fw_version);
	}
}

void B394_DigitalInput_Init(void)
{
	/* Actor init: create queue and start monitor so actor can receive messages.
	 * GPIO is initialized on first message; input poll task starts on INIT command. */
	b394_di_init_actor();
}

#else

void B394_DigitalInput_Init(void)
{
	/* No-op when not B394 */
}

#endif /* B394 */
