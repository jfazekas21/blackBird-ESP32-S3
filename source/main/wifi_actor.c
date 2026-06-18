/*
 * wifi_actor.c
 *
 *  Created on: May 12, 2022
 *      Author: shyam and Amruta
 */

#include "actor.h"
#include "Config.h"
#include "wifi_Actor.h"
#include "console_Actor.h"
#include "esp_mac.h"
#include "esp_wifi.h"
//#include "cmd_Line.h"
#include "esp_log.h"
#include <stddef.h>
#include <esp_netif.h>
#include "esp_attr.h"
#include "esp_heap_caps.h"


//----------------------------- Actor Tags ---------------------------------------//
PSRAM_ATTR static char stored_ssid[50] = {0};   // Store SSID for reconnect attempts
PSRAM_ATTR static char stored_pass[50] = {0};   // Store Password for reconnect attempts
static const char * THIS_ACTOR = "WIFI";
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
#define PREFIX "Haven "

#define PREFIX_LEN 6

#define CATEGORY_POS 6

#define HYPHEN_POS 7

#define MIN_SERIES_LEN 3

#define MAX_SERIES_LEN 4

#define MAC_SUFFIX_LEN 4
#define RX_QUE_COUNT 20


//------------------------ WIFI Actor Resources -------------------------------------------------//

typedef struct
{
   /* data */
   char *ssid;
   char *pass;
   int  timeout;
   int  mode;
}wifi_user_config_st;

//static bool wifi_status_bool		= false;
bool wifi_status 					= false;

static uint16_t ap_count_u16 		    = 0;
static const uint16_t CONNECTED_BIT_u16 = BIT0;
PSRAM_ATTR_BSS static wifi_ap_record_t app_info, ap_info[DEFAULT_SCAN_LIST_SIZE];
//static char 			Update_parametes	= 	0;
static wifi_user_config_st   	s_config_wifi;
static EventGroupHandle_t 		wifi_event_group;
esp_netif_t *sta_netif;
bool web_server_started=0, wifi_ap_started =0;

// Global variables to track WiFi connection attempts
static int current_ssid_attempt = 0;   // Tracks the current SSID attempt number
static int total_ssid_attempts = 0;    // Total number of SSIDs to attempt
static bool wifi_connected = false;    // Tracks if the device is currently connected
static char No_Network_Retry = 0;
// Global array pointer
static uint8_t *g_ScanListArray = NULL;

static StaticQueue_t Monitor_pxQueueBuffer , NetScan_pxQueueBuffer ;
PSRAM_ATTR_BSS static StackType_t xTaskStack[WIFI_TASK_STACK_DEPTH];
extern esp_netif_t* get_ethernet_netif_handle();
static uint16_t  wifi_disconnect(AMessage_st* s_Message_Rx);
static void wifi_setup(void *a, void *b);
static void wifi_scan();
static void wifi_actor_Initialise(void);
static void wifi_Init_as_STA(void);
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static bool wifi_join(const char *ssid, const char *pass, const uint8_t *bssid, int timeout_ms) ;
static void Get_Property(AMessage_st* s_Message_Rx);
static void Get_WIFI_Mode(uint8_t Send_to_console_flag,AMessage_st* s_Message_Rx);
static void Get_AP_INFO(AMessage_st* s_Message_Rx);
static void Get_WIFI_MAC_ADDR(AMessage_st* s_Message_Rx);
static void Get_WIFI_CUSTOM_MAC_ADDR(AMessage_st* s_Message_Rx);
esp_err_t esp_netif_dhcpc_stop(esp_netif_t *esp_netif);
esp_err_t esp_netif_dhcps_start(esp_netif_t *esp_netif);
void web_server(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
void pingServer(AMessage_st* s_Message_Rx);
static void get_WifiScanList_RAM(AMessage_st* s_Message_Rx);
static void Network_Scan (void *pvParameters __attribute__((unused)));
static void Wifi_connect_with_SSID(AMessage_st* s_Message_Rx);
static char* log_wifi_disconn_reason(uint16_t reason);
static void wifi_scan_task(void *pvParameters __attribute__((unused)));
static void Wifi_Scan_Init();
static void WIFI_Scan_Return (AMessage_st* s_Message_Rx);
static void Get_Connectivity_Info_For_update_Twin(AMessage_st* s_Message_Rx);
static void update_ip_change_datetime(char *dest, size_t dest_len);
static int should_skip_ssid(const char *ssid);
static uint16_t wifiDisconnectCount;
static uint8_t wifiConnDisconnFlag;


//-------------------------- Actor Parameters ------------------------------//
static esp_netif_t *current_default_netif = NULL;

PSRAM_ATTR_BSS static struct actor_parameter {
	int status_u8;
	bool auto_connect_u8;
	uint8_t Mode_u8[10];
	uint8_t SSID_a8 [SSID_LEN];
	uint8_t SSID1_a8 [SSID_LEN];
	uint8_t SSID2_a8 [SSID_LEN];
	uint8_t SSID3_a8 [SSID_LEN];
	uint8_t Connected_ssid [SSID_LEN];
	uint8_t Pass_a8	 [PASS_LEN];
	uint8_t Pass1_a8 [PASS_LEN];
	uint8_t Pass2_a8 [PASS_LEN];
	uint8_t Pass3_a8 [PASS_LEN];
	uint8_t Connected_pass [PASS_LEN];
	uint8_t MAC_ID_a8[MAC_LEN ];
	uint8_t Module_ID_a8[MAC_LEN ];
	uint8_t SSID_AP_a8 [SSID_LEN];
	uint8_t Pass_AP_a8 [PASS_LEN];
	uint8_t AP_IP_ADDR_a8 [30];
	uint8_t AP_GW_ADDR_a8 [30];
	uint8_t AP_SM_ADDR_a8 [30];
	uint8_t STA_IP_ADDR_a8 [30];
	uint8_t STA_GW_ADDR_a8 [30];
	uint8_t STA_SM_ADDR_a8 [30];
	uint8_t STA_IP_UPDATED_DATETIME_a8[32];
	uint8_t DHCP_u8;
	uint32_t SCAN_DELAY_u32;
	uint16_t wifi_disconnect_reason;
	uint8_t WIFI_Scan_Mode_u8;
	uint8_t WIFI_GATEWAY_MAC_ADDR_a8[18];
} s_Para;

PSRAM_ATTR static   struct  property  prop[] = {							//Actor Property
		{ &s_Para.status_u8	,				"STATUS"	,			U_INT8 	,"R",  "Current Wi-Fi connection status"},
		{ &s_Para.auto_connect_u8	,		"AUTO_CONNECT"	,		U_INT8 	,"RW",  "Use for auto connect"},
		{ &s_Para.Mode_u8	, 				"MODE"		,			STRING 	,"R" , " Wi-Fi operating mode"},
		{ &s_Para.SSID_a8	,				"RESCUE_SSID",			STRING 	,"RW", "Additional SSIDs for Wi-Fi connection"},
		{ &s_Para.SSID1_a8	,				"SSID1"		,			STRING 	,"RW", "Additional SSIDs for Wi-Fi connection"},
		{ &s_Para.SSID2_a8	,				"SSID2"		,			STRING 	,"RW", "Additional SSIDs for Wi-Fi connection"},
		{ &s_Para.SSID3_a8	,				"SSID3"		,			STRING 	,"RW", "Additional SSIDs for Wi-Fi connection"},
		{ &s_Para.Connected_ssid	,		"CONNECTED_SSID"	   ,STRING 	,"RW", "Device connected to this SSID"},
		{ &s_Para.Pass_a8	,				"RESCUE_PASS"		,	STRING 	,"RW", "Password for rescue Wi-Fi connection"},
		{ &s_Para.Pass1_a8	,				"PASS1"		,			STRING 	,"RW", "Additional passwords for Wi-Fi connection"},
		{ &s_Para.Pass2_a8	,				"PASS2"		,			STRING 	,"RW", "Additional passwords for Wi-Fi connection"},
		{ &s_Para.Pass3_a8	,				"PASS3"		,			STRING 	,"RW", "Additional passwords for Wi-Fi connection"},
		{ &s_Para.Connected_pass	,		"CONNECTED_PASS"	   ,STRING 	,"RW", "Device connected to this PASSWORD"},
		{ &s_Para.SSID_AP_a8	,			"SSID_AP"		,		STRING 	,"RW", "SSID for Wi-Fi access point"},
		{ &s_Para.Pass_AP_a8	,			"PASS_AP"		,		STRING 	,"RW", "Password for Wi-Fi access point"},
		{ &s_Para.MAC_ID_a8	, 				"MAC_ADD"	,			STRING 	,"R", "MAC address of the Wi-Fi module"},
		{ &s_Para.Module_ID_a8, 			"MODULE_ID"	,			STRING 	,"R", "Module ID of the Wi-Fi module"},
		{ &s_Para.AP_IP_ADDR_a8	,			"AP_IP_ADDR"	,		STRING 	,"R", "IP address of the Wi-Fi access point"},
		{ &s_Para.AP_GW_ADDR_a8	, 			"AP_GW_ADDR"	,		STRING 	,"RW", "Gateway IP address for the Wi-Fi access point"},
		{ &s_Para.AP_SM_ADDR_a8, 			"AP_SM_ADDR"	,		STRING 	,"RW", "Subnet mask for the Wi-Fi access point"},
		{ &s_Para.STA_IP_ADDR_a8	,		"STA_IP_ADDR"	,		STRING 	,"RW", "IP address of the Wi-Fi station"},
		{ &s_Para.STA_GW_ADDR_a8	, 		"STA_GW_ADDR"	,		STRING 	,"RW", "Gateway IP address for the Wi-Fi station"},
		{ &s_Para.STA_SM_ADDR_a8, 			"STA_SM_ADDR"	,		STRING 	,"RW", "Subnet mask for the Wi-Fi station"},
		{ &s_Para.STA_IP_UPDATED_DATETIME_a8, "STA_IP_UPDATED_DATETIME", STRING, "R", "UTC dateTime when the Wi-Fi station IP address was assigned or changed"},
	    { &s_Para.DHCP_u8,                  "DHCP"   ,              U_INT8 ,"RW", "Enable/disable DHCP"},
		{ &s_Para.SCAN_DELAY_u32,          "SCAN_DELAY"   ,        U_INT32,"RW", "Scan delay for Wi-Fi scanning"},
		{ &s_Para.wifi_disconnect_reason,   "WIFI_DISCON_REASON"   ,U_INT8,"R", "Wi-Fi disconnect reason"},
	    { &s_Para.WIFI_Scan_Mode_u8,        "WIFI_SCAN_MODE"   ,    U_INT8 ,"RW", "WIFI Scan mode, 0- Active, 1- Passive"},
		{ &s_Para.WIFI_GATEWAY_MAC_ADDR_a8, "WIFI_GATEWAY_MAC_ADDR", STRING, "R", "MAC address of the WiFi gateway (AP BSSID)"},
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Event[1024];
PSRAM_ATTR_BSS static char payLoadData_Scan[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS StackType_t xTaskStack_NETSCAN[WIFI_NETWORKSCAN_TASK_STACK_DEPTH],	 xScanTaskStack [WIFI_SCANTASK_STACK_DEPTH];
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [RX_QUE_COUNT * sizeof(AMessage_st)], NetScan_pucQueueStorage[WIFI_NETWORKSCAN_QUEUE_LENGTH * WIFI_NETWORKSCAN_QUEUE_ITEMSIZE];
PSRAM_ATTR_BSS static char Net_Scan_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char data_buffer[2048];
PSRAM_ATTR_BSS static char WIFI_NW_buffer[4*1024];
PSRAM_ATTR_BSS static char payLoadData_WIFI_NW[4*1024];

//-------------------------- Common Actor Resources ------------------------------//
static bool 			FirstEntry_bool = false;
static TaskHandle_t  	 Task_Handle		= NULL, Network_Scan_Handle = NULL,WifiScanHandle=NULL; 	// WIFI Task Handler,ScanHandle		= NULL,
static QueueHandle_t 	msg_Rx_Queue	= NULL,  Network_Scan_Que = NULL; 	// WIFI Rx Queue
static StaticTask_t xWIFITaskBuffer, xWIFISCANTaskBuffer,xWIFI_NETSCAN_TaskBuffer;  //// Declare a static task control block
static void init(AMessage_st* s_Message_Rx); 							// Init actor
static uint8_t set			(char *property, char *value, AMessage_st* s_Message_Rx);					// Set or Change a parameter
static void get			(char *prop, char *val_a8);						// Get or Read a Parameter
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor		(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Deinit_Actor(AMessage_st* s_Message_Rx);
void init_wifi_connection_attempts(int ssid_attempts);
void start_wifi_connection_process();
void trigger_next_wifi_attempt();

//----------------------- Commen Actor Methods ------------------------------------------------------------------//

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
	int64_t mac_value=0;
	uint8_t mac[6] = {0};
	int no_of_elements = sizeof(prop) / sizeof(struct property);

	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(property, prop[i].str_name)) {

			if (!strcmp(prop[i].access, "RW")) {

			parameter_found = 1; // Set flag to indicate actor is found
			switch (prop[i].type) {

			case U_INT8:
				*(uint8_t*) prop[i].name = atoi(value);
				break;

			case U_INT16:
				*(uint16_t*) prop[i].name = atoi(value);
				break;

			case U_INT32:
				*(uint32_t*) prop[i].name = atoi(value);
				break;

			case INT:
				*(int*) prop[i].name = atoi(value);
				break;
			case FLOAT:
				*(float*) prop[i].name = atof(value);
				break;

			case STRING:
				strcpy((char*) prop[i].name, value);
				break;

			default:
				break;
			}
			if(strcmp(property, "MAC_ADD") == 0)
			{
				mac_value = strtoull(value, NULL, 16);;
				for(int i=5;i>(-1);i--)
				{
					mac[i] = (uint8_t)mac_value & 0x00FF;
					mac_value = mac_value >> 8;
				}
				ESP_ERROR_CHECK(esp_base_mac_addr_set(mac));
				sprintf((char*)s_Para.MAC_ID_a8,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
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

static void get(char *str_prop, char *val_a8) {
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(str_prop, prop[i].str_name)) {
			switch (prop[i].type) {

			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
				break;

			case U_INT32:
				sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
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
			if(!(strcmp(prop[i].str_name, "WIFI_SCAN_MODE")))
			{
				switch (*(char*)prop[i].name)
				{
				case WIFI_SCAN_TYPE_ACTIVE:
					strcpy(val_a8, "ACTIVE");
					break;

				case WIFI_SCAN_TYPE_PASSIVE:
					strcpy(val_a8, "PASSIVE");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_wifi_ZERO_STATE:
					strcpy(val_a8, "WIFI IDLE STATE");
					break;

				case E_wifi_NOT_CONNECTED:
					strcpy(val_a8, "WIFI NOT CONNECTED");
					break;

				case E_wifi_CONNECTED_LOW:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_HIGH:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_AP:
					strcpy(val_a8, "WIFI CONNECTED IN AP MODE");
					break;

				default:
					break;
				}
			}
			if (!(strcmp(prop[i].str_name, "WIFI_DISCON_REASON"))) {
			switch (*(char*)prop[i].name) {
			case 0:
				strcpy(val_a8, "");
				break;
			 case WIFI_REASON_UNSPECIFIED:
				 sprintf(val_a8, "Disconnected: Unspecified reason");
				 break;
			 case WIFI_REASON_AUTH_EXPIRE:
				 sprintf(val_a8, "Disconnected: Authentication expired");
				 break;
			 case WIFI_REASON_AUTH_LEAVE:
				 sprintf(val_a8, "Disconnected: Authentication leave");
				 break;
			 case WIFI_REASON_ASSOC_EXPIRE:
				 sprintf(val_a8, "Disconnected: Association expired");
				 break;
			 case WIFI_REASON_ASSOC_TOOMANY:
				 sprintf(val_a8, "Disconnected: Too many associations");
				 break;
			 case WIFI_REASON_NOT_AUTHED:
				 sprintf(val_a8, "Disconnected: Not authenticated");
				 break;
			 case WIFI_REASON_NOT_ASSOCED:
				 sprintf(val_a8, "Disconnected: Not associated");
				 break;
			 case WIFI_REASON_ASSOC_LEAVE:
				 sprintf(val_a8, "Disconnected: Association leave");
				 break;
			 case WIFI_REASON_ASSOC_NOT_AUTHED:
				 sprintf(val_a8, "Disconnected: Association but not authenticated");
				 break;
			 case WIFI_REASON_DISASSOC_PWRCAP_BAD:
				 sprintf(val_a8, "Disconnected: Disassociated due to bad power capability");
				 break;
			 case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
				 sprintf(val_a8, "Disconnected: Disassociated due to unsupported channel");
				 break;
			 case WIFI_REASON_BSS_TRANSITION_DISASSOC:
				 sprintf(val_a8, "Disconnected: BSS transition disassociation");
				 break;
			 case WIFI_REASON_IE_INVALID:
				 sprintf(val_a8, "Disconnected: Invalid IE");
				 break;
			 case WIFI_REASON_MIC_FAILURE:
				 sprintf(val_a8, "Disconnected: MIC failure");
				 break;
			 case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
				 sprintf(val_a8, "Disconnected: 4-way handshake timeout");
				 break;
			 case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
				 sprintf(val_a8, "Disconnected: Group key update timeout");
				 break;
			 case WIFI_REASON_IE_IN_4WAY_DIFFERS:
				 sprintf(val_a8, "Disconnected: IE in 4-way differs");
				 break;
			 case WIFI_REASON_GROUP_CIPHER_INVALID:
				 sprintf(val_a8, "Disconnected: Group cipher invalid");
				 break;
			 case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
				 sprintf(val_a8, "Disconnected: Pairwise cipher invalid");
				 break;
			 case WIFI_REASON_AKMP_INVALID:
				 sprintf(val_a8, "Disconnected: AKMP invalid");
				 break;
			 case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
				 sprintf(val_a8, "Disconnected: Unsupported RSN IE version");
				 break;
			 case WIFI_REASON_INVALID_RSN_IE_CAP:
				 sprintf(val_a8, "Disconnected: Invalid RSN IE capability");
				 break;
			 case WIFI_REASON_802_1X_AUTH_FAILED:
				 sprintf(val_a8, "Disconnected: 802.1X authentication failed");
				 break;
			 case WIFI_REASON_CIPHER_SUITE_REJECTED:
				 sprintf(val_a8, "Disconnected: Cipher suite rejected");
				 break;
			 case WIFI_REASON_TDLS_PEER_UNREACHABLE:
				 sprintf(val_a8, "Disconnected: TDLS peer unreachable");
				 break;
			 case WIFI_REASON_TDLS_UNSPECIFIED:
				 sprintf(val_a8, "Disconnected: TDLS unspecified");
				 break;
			 case WIFI_REASON_SSP_REQUESTED_DISASSOC:
				 sprintf(val_a8, "Disconnected: SSP requested disassociation");
				 break;
			 case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
				 sprintf(val_a8, "Disconnected: No SSP roaming agreement");
				 break;
			 case WIFI_REASON_BAD_CIPHER_OR_AKM:
				 sprintf(val_a8, "Disconnected: Bad cipher or AKM");
				 break;
			 case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
				 sprintf(val_a8, "Disconnected: Not authorized at this location");
				 break;
			 case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
				 sprintf(val_a8, "Disconnected: Service change precludes TS");
				 break;
			 case WIFI_REASON_UNSPECIFIED_QOS:
				 sprintf(val_a8, "Disconnected: Unspecified QoS");
				 break;
			 case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
				 sprintf(val_a8, "Disconnected: Not enough bandwidth");
				 break;
			 case WIFI_REASON_MISSING_ACKS:
				 sprintf(val_a8, "Disconnected: Missing ACKs");
				 break;
			 case WIFI_REASON_EXCEEDED_TXOP:
				 sprintf(val_a8, "Disconnected: Exceeded TXOP");
				 break;
			 case WIFI_REASON_STA_LEAVING:
				 sprintf(val_a8, "Disconnected: STA leaving");
				 break;
			 case WIFI_REASON_END_BA:
				 sprintf(val_a8, "Disconnected: End BA");
				 break;
			 case WIFI_REASON_UNKNOWN_BA:
				 sprintf(val_a8, "Disconnected: Unknown BA");
				 break;
			 case WIFI_REASON_TIMEOUT:
				 sprintf(val_a8, "Disconnected: Timeout");
				 break;
			 case WIFI_REASON_PEER_INITIATED:
				 sprintf(val_a8, "Disconnected: Peer initiated");
				 break;
			 case WIFI_REASON_AP_INITIATED:
				 sprintf(val_a8, "Disconnected: AP initiated");
				 break;
			 case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
				 sprintf(val_a8, "Disconnected: Invalid FT action frame count");
				 break;
			 case WIFI_REASON_INVALID_PMKID:
				 sprintf(val_a8, "Disconnected: Invalid PMKID");
				 break;
			 case WIFI_REASON_INVALID_MDE:
				 sprintf(val_a8, "Disconnected: Invalid MDE");
				 break;
			 case WIFI_REASON_INVALID_FTE:
				 sprintf(val_a8, "Disconnected: Invalid FTE");
				 break;
			 case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
				 sprintf(val_a8, "Disconnected: Transmission link establishment failed");
				 break;
			 case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
				 sprintf(val_a8, "Disconnected: Alternative channel occupied");
				 break;
			 case WIFI_REASON_BEACON_TIMEOUT:
				 sprintf(val_a8, "Disconnected: Beacon timeout");
				 break;
			 case WIFI_REASON_NO_AP_FOUND:
				 sprintf(val_a8, "Disconnected: No AP found");
				 break;
			 case WIFI_REASON_AUTH_FAIL:
				 sprintf(val_a8, "Disconnected: Authentication failed");
				 break;
			 case WIFI_REASON_ASSOC_FAIL:
				 sprintf(val_a8, "Disconnected: Association failed");
				 break;
			 case WIFI_REASON_HANDSHAKE_TIMEOUT:
				 sprintf(val_a8, "Disconnected: Handshake timeout");
				 break;
			 case WIFI_REASON_CONNECTION_FAIL:
				 sprintf(val_a8, "Disconnected: Connection failed");
				 break;
			 case WIFI_REASON_AP_TSF_RESET:
				 sprintf(val_a8, "Disconnected: AP TSF reset");
				 break;
			 case WIFI_REASON_ROAMING:
				 sprintf(val_a8, "Disconnected: Roaming");
				 break;
			 case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
				 sprintf(val_a8, "Disconnected: Association comeback time too long");
				 break;
			 case WIFI_REASON_SA_QUERY_TIMEOUT:
				 sprintf(val_a8, "Disconnected: SA query timeout");
				 break;
			 default:
				 sprintf(val_a8, "Disconnected: Unknown reason");
				 break;
			 }
		   }
		}
	}
}//	get


static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[100] = {0};
	char typeString[20] = {0};

	int no_of_elements = sizeof(prop) / sizeof(struct property);

	    // Create JSON arrays
	    cJSON *jsonArrayName = cJSON_CreateArray();
	    cJSON *jsonArrayType = cJSON_CreateArray();
	    cJSON *jsonArrayValue = cJSON_CreateArray();
	    cJSON *jsonArrayAccess = cJSON_CreateArray();
	    cJSON *jsonArrayHelpString = cJSON_CreateArray();

	    for (int i = 0; i < no_of_elements; i++) {
			cJSON_AddItemToArray(jsonArrayName, cJSON_CreateString(prop[i].str_name));
			// Convert DataType enum to string representation for property type
			// Add value based on data type for property name
			switch (prop[i].type) {
				case U_INT8:
					strcpy(typeString, "U_INT8");
					sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
					break;

				case U_INT16:
					strcpy(typeString, "U_INT16");
					sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
					break;

				case U_INT32:
					strcpy(typeString, "U_INT32");
					sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
					break;

				case INT:
					strcpy(typeString, "INT");
					sprintf(val_a8, "%d", *(int*) prop[i].name);
					break;

				case FLOAT:
					strcpy(typeString, "FLOAT");
					sprintf(val_a8, "%f", *(float*) prop[i].name);
					break;

				case STRING:
					strcpy(typeString, "STRING");
					strcpy(val_a8, prop[i].name);
					break;

				default:
					break;
				}
			if(!(strcmp(prop[i].str_name, "WIFI_SCAN_MODE")))
			{
				switch (*(char*)prop[i].name)
				{
				case WIFI_SCAN_TYPE_ACTIVE:
					strcpy(val_a8, "ACTIVE");
					break;

				case WIFI_SCAN_TYPE_PASSIVE:
					strcpy(val_a8, "PASSIVE");
					break;

				default:
					break;
				}
			}
			if (!(strcmp(prop[i].str_name, "WIFI_DISCON_REASON"))) {
				 switch (*(char*)prop[i].name) {
				case 0:
					strcpy(val_a8, "");
					break;
				 case WIFI_REASON_UNSPECIFIED:
					 sprintf(val_a8, "Disconnected: Unspecified reason");
					 break;
				 case WIFI_REASON_AUTH_EXPIRE:
					 sprintf(val_a8, "Disconnected: Authentication expired");
					 break;
				 case WIFI_REASON_AUTH_LEAVE:
					 sprintf(val_a8, "Disconnected: Authentication leave");
					 break;
				 case WIFI_REASON_ASSOC_EXPIRE:
					 sprintf(val_a8, "Disconnected: Association expired");
					 break;
				 case WIFI_REASON_ASSOC_TOOMANY:
					 sprintf(val_a8, "Disconnected: Too many associations");
					 break;
				 case WIFI_REASON_NOT_AUTHED:
					 sprintf(val_a8, "Disconnected: Not authenticated");
					 break;
				 case WIFI_REASON_NOT_ASSOCED:
					 sprintf(val_a8, "Disconnected: Not associated");
					 break;
				 case WIFI_REASON_ASSOC_LEAVE:
					 sprintf(val_a8, "Disconnected: Association leave");
					 break;
				 case WIFI_REASON_ASSOC_NOT_AUTHED:
					 sprintf(val_a8, "Disconnected: Association but not authenticated");
					 break;
				 case WIFI_REASON_DISASSOC_PWRCAP_BAD:
					 sprintf(val_a8, "Disconnected: Disassociated due to bad power capability");
					 break;
				 case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
					 sprintf(val_a8, "Disconnected: Disassociated due to unsupported channel");
					 break;
				 case WIFI_REASON_BSS_TRANSITION_DISASSOC:
					 sprintf(val_a8, "Disconnected: BSS transition disassociation");
					 break;
				 case WIFI_REASON_IE_INVALID:
					 sprintf(val_a8, "Disconnected: Invalid IE");
					 break;
				 case WIFI_REASON_MIC_FAILURE:
					 sprintf(val_a8, "Disconnected: MIC failure");
					 break;
				 case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
					 sprintf(val_a8, "Disconnected: 4-way handshake timeout");
					 break;
				 case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
					 sprintf(val_a8, "Disconnected: Group key update timeout");
					 break;
				 case WIFI_REASON_IE_IN_4WAY_DIFFERS:
					 sprintf(val_a8, "Disconnected: IE in 4-way differs");
					 break;
				 case WIFI_REASON_GROUP_CIPHER_INVALID:
					 sprintf(val_a8, "Disconnected: Group cipher invalid");
					 break;
				 case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
					 sprintf(val_a8, "Disconnected: Pairwise cipher invalid");
					 break;
				 case WIFI_REASON_AKMP_INVALID:
					 sprintf(val_a8, "Disconnected: AKMP invalid");
					 break;
				 case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
					 sprintf(val_a8, "Disconnected: Unsupported RSN IE version");
					 break;
				 case WIFI_REASON_INVALID_RSN_IE_CAP:
					 sprintf(val_a8, "Disconnected: Invalid RSN IE capability");
					 break;
				 case WIFI_REASON_802_1X_AUTH_FAILED:
					 sprintf(val_a8, "Disconnected: 802.1X authentication failed");
					 break;
				 case WIFI_REASON_CIPHER_SUITE_REJECTED:
					 sprintf(val_a8, "Disconnected: Cipher suite rejected");
					 break;
				 case WIFI_REASON_TDLS_PEER_UNREACHABLE:
					 sprintf(val_a8, "Disconnected: TDLS peer unreachable");
					 break;
				 case WIFI_REASON_TDLS_UNSPECIFIED:
					 sprintf(val_a8, "Disconnected: TDLS unspecified");
					 break;
				 case WIFI_REASON_SSP_REQUESTED_DISASSOC:
					 sprintf(val_a8, "Disconnected: SSP requested disassociation");
					 break;
				 case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
					 sprintf(val_a8, "Disconnected: No SSP roaming agreement");
					 break;
				 case WIFI_REASON_BAD_CIPHER_OR_AKM:
					 sprintf(val_a8, "Disconnected: Bad cipher or AKM");
					 break;
				 case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
					 sprintf(val_a8, "Disconnected: Not authorized at this location");
					 break;
				 case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
					 sprintf(val_a8, "Disconnected: Service change precludes TS");
					 break;
				 case WIFI_REASON_UNSPECIFIED_QOS:
					 sprintf(val_a8, "Disconnected: Unspecified QoS");
					 break;
				 case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
					 sprintf(val_a8, "Disconnected: Not enough bandwidth");
					 break;
				 case WIFI_REASON_MISSING_ACKS:
					 sprintf(val_a8, "Disconnected: Missing ACKs");
					 break;
				 case WIFI_REASON_EXCEEDED_TXOP:
					 sprintf(val_a8, "Disconnected: Exceeded TXOP");
					 break;
				 case WIFI_REASON_STA_LEAVING:
					 sprintf(val_a8, "Disconnected: STA leaving");
					 break;
				 case WIFI_REASON_END_BA:
					 sprintf(val_a8, "Disconnected: End BA");
					 break;
				 case WIFI_REASON_UNKNOWN_BA:
					 sprintf(val_a8, "Disconnected: Unknown BA");
					 break;
				 case WIFI_REASON_TIMEOUT:
					 sprintf(val_a8, "Disconnected: Timeout");
					 break;
				 case WIFI_REASON_PEER_INITIATED:
					 sprintf(val_a8, "Disconnected: Peer initiated");
					 break;
				 case WIFI_REASON_AP_INITIATED:
					 sprintf(val_a8, "Disconnected: AP initiated");
					 break;
				 case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
					 sprintf(val_a8, "Disconnected: Invalid FT action frame count");
					 break;
				 case WIFI_REASON_INVALID_PMKID:
					 sprintf(val_a8, "Disconnected: Invalid PMKID");
					 break;
				 case WIFI_REASON_INVALID_MDE:
					 sprintf(val_a8, "Disconnected: Invalid MDE");
					 break;
				 case WIFI_REASON_INVALID_FTE:
					 sprintf(val_a8, "Disconnected: Invalid FTE");
					 break;
				 case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
					 sprintf(val_a8, "Disconnected: Transmission link establishment failed");
					 break;
				 case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
					 sprintf(val_a8, "Disconnected: Alternative channel occupied");
					 break;
				 case WIFI_REASON_BEACON_TIMEOUT:
					 sprintf(val_a8, "Disconnected: Beacon timeout");
					 break;
				 case WIFI_REASON_NO_AP_FOUND:
					 sprintf(val_a8, "Disconnected: No AP found");
					 break;
				 case WIFI_REASON_AUTH_FAIL:
					 sprintf(val_a8, "Disconnected: Authentication failed");
					 break;
				 case WIFI_REASON_ASSOC_FAIL:
					 sprintf(val_a8, "Disconnected: Association failed");
					 break;
				 case WIFI_REASON_HANDSHAKE_TIMEOUT:
					 sprintf(val_a8, "Disconnected: Handshake timeout");
					 break;
				 case WIFI_REASON_CONNECTION_FAIL:
					 sprintf(val_a8, "Disconnected: Connection failed");
					 break;
				 case WIFI_REASON_AP_TSF_RESET:
					 sprintf(val_a8, "Disconnected: AP TSF reset");
					 break;
				 case WIFI_REASON_ROAMING:
					 sprintf(val_a8, "Disconnected: Roaming");
					 break;
				 case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
					 sprintf(val_a8, "Disconnected: Association comeback time too long");
					 break;
				 case WIFI_REASON_SA_QUERY_TIMEOUT:
					 sprintf(val_a8, "Disconnected: SA query timeout");
					 break;
				 default:
					 sprintf(val_a8, "Disconnected: Unknown reason");
					 break;
				 }
			}
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				switch (*(char*)prop[i].name) {
				case E_wifi_ZERO_STATE:
					strcpy(val_a8, "WIFI IDLE STATE");
					break;

				case E_wifi_NOT_CONNECTED:
					strcpy(val_a8, "WIFI NOT CONNECTED");
					break;

				case E_wifi_CONNECTED_LOW:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_HIGH:
					strcpy(val_a8, "WIFI CONNECTED IN STA MODE");
					break;

				case E_wifi_CONNECTED_AP:
					strcpy(val_a8, "WIFI CONNECTED IN AP MODE");
					break;

				default:strcpy(val_a8, "WIFI default STATE");
					break;
				}
			}
			cJSON_AddItemToArray(jsonArrayType, cJSON_CreateString(typeString));
			cJSON_AddItemToArray(jsonArrayValue, cJSON_CreateString(val_a8));
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
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		
		// Cleanup
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the WIFI actor.");
	cJSON_AddStringToObject(responseObject, "SET(string RESCUE_SSID, string RESCUE_PASS)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "DISCONNECT()", "Disconnect from WIFI.");
	cJSON_AddStringToObject(responseObject, "WEB_SERVER(INT NUMBER_OF_MINUTES)", "Connect to WIFI in AP mode and then perform FOTA from web server (link: 2.2.2.1)");
	cJSON_AddStringToObject(responseObject, "SCAN()", "Display upto 10 available APs.");
	cJSON_AddStringToObject(responseObject, "AP_INFO()", "Read AP's information.");
	cJSON_AddStringToObject(responseObject, "MODE()", "Provide WIFI operating mode.");
	cJSON_AddStringToObject(responseObject, "MAC_ADDR()", "Provide MAC address of ESP module.");
	cJSON_AddStringToObject(responseObject, "CUS_MAC_ADDR()", "Provide custom MAC address of ESP module.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "PING(string IP_ADDRESS, INT TIMEOUT)", "Ping to the server.");
	cJSON_AddStringToObject(responseObject, "GET_SCANLIST_RAM()", "Display the list of scanned SSID and RSSI values. ");
	cJSON_AddStringToObject(responseObject, "NETWORK_SCAN()", "Scan available networks required for server connect.");
	cJSON_AddStringToObject(responseObject, "CONNECT_SSID(string SSID, string PASSWORD)", "Connect to WIFI with given SSID and Password.");
	cJSON_AddStringToObject(responseObject, "DEINIT()", "Deinit the actor. It delete the monitor task and its queue.");
	cJSON_AddStringToObject(responseObject, "SWITCH_AP_MODE()", "Switch WIFI mode to AP.");
	cJSON_AddStringToObject(responseObject, "SCAN_RETURN(json INDEX)", "Retrun SSID list as per the provided index numbers.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void init(AMessage_st* s_Message_Rx){
		if(FirstEntry_bool)
		{
			return;
		}

		FirstEntry_bool = true;
		uint8_t mac[6]; //, temp;
		char temp1[10]={0};
		uint64_t chipmacid = 0LL;
		char* Ptr;
		int8_t temp4;
		if(msg_Rx_Queue == NULL)
		{
			msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
		}
		if(msg_Rx_Queue == NULL)
		{
			printf("WIFI RX Queue is not created. \n");
		}

		strcpy((char*)s_Para.SSID_a8, "");
		strcpy((char*)s_Para.Pass_a8, "");
		strcpy((char*)s_Para.SSID1_a8, "");
		strcpy((char*)s_Para.Pass1_a8, "");
		strcpy((char*)s_Para.SSID2_a8, "");
		strcpy((char*)s_Para.Pass2_a8, "");
		strcpy((char*)s_Para.SSID3_a8, "");
		strcpy((char*)s_Para.Pass3_a8, "");
		strcpy((char*)s_Para.AP_IP_ADDR_a8, "192.168.2.4");  	// default ip address for AP mode
		strcpy((char*)s_Para.AP_GW_ADDR_a8, "192.168.2.1");		// default getway address for AP mode
		strcpy((char*)s_Para.AP_SM_ADDR_a8, "255.255.255.0");	// default subnet mask for AP mode
		strcpy((char*)s_Para.STA_IP_UPDATED_DATETIME_a8, "");
		s_Para.WIFI_Scan_Mode_u8 = WIFI_SCAN_TYPE_ACTIVE;
		wifi_setup(0, 0);
		s_Para.SCAN_DELAY_u32 = 60;     //Default value for scan delay is 1 min

		esp_read_mac(mac, ESP_MAC_WIFI_STA);
		sprintf((char*)s_Para.MAC_ID_a8,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
		esp_efuse_mac_get_default((uint8_t*) (&chipmacid));
		sprintf((char*)s_Para.Module_ID_a8, "%012llX", chipmacid);
		s_Para.status_u8  = E_wifi_ZERO_STATE;
		sprintf((char*)s_Para.SSID_AP_a8, "Haven X-Series:%02X%02X",mac[4], mac[5]);
		Ptr = temp1;
		esp_wifi_get_country_code(Ptr);
		esp_wifi_get_max_tx_power(&temp4);  //Get maximum transmitting power after WiFi start.
		s_Para.auto_connect_u8 = 0;  //Enable WIFI auto connect after reboot
		Task_Handle = xTaskCreateStaticPinnedToCore(
						monitor,                 // Task function
						"WIFI_Monitor",            // Task name
						WIFI_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						WIFI_TASK_PRIORITY,                       // Task priority
						xTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xWIFITaskBuffer,             // Pointer to task control block
						1
		);

		if (Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create WIFI task\n");
#endif
				// Handle error
			}
		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/WIFI.json");
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		cJSON_Delete(responseObject);
		Get_WIFI_Mode(0,s_Message_Rx);
		Wifi_Scan_Init();
}// init

static void monitor(void *pvParameters __attribute__((unused))) {

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
		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))
		{
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("WIFI DT = %s \n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;

			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				init(s_Message_Rx);
					Add_Response_msg(" WIFI initialization is done.", s_Message_Rx, payLoadData);
//				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) { // Get Actor Properties
				Get_Property(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SET")) // Set Actor Properties
			{
				u8Result =0;
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				    }
				else{
				head_JSON = name_JSON->child;
				cJSON *root_JSON  = cJSON_CreateObject();
				cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/WIFI.json");
			   // Loop through each key-value pair
				do {
					// Check if the value string is not NULL
					if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
					{
						// Set the key-value pair
						u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
//						if(strcmp(head_JSON->string, "STATUS") !=0)
//						{
						if(u8Result==1)
						{
						cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
						}
						else if(u8Result==2){
							sprintf(str,"'%s' is a read only property", head_JSON->string);
							 Add_Response_msg(str, s_Message_Rx, payLoadData);
						}
						else{
						cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
						}
					} else {
						// Handle the case where value string is NULL (e.g., log an error or take appropriate action)
						sprintf(str, "Invalid parameter '%s'", head_JSON->string);
						Add_Response_msg(str,s_Message_Rx, payLoadData);
						// Handle the error as per your application's requirements
					}
					head_JSON = head_JSON->next;
				} while (head_JSON != 0);

				if(u8Result==1){
				//  save parameters to JFS
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
				cJSON_Delete(root_JSON);
				console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
				// Free the parsed JSON
				cJSON_Delete(name_JSON);
			}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}


			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DISCONNECT")) {
					wifi_disconnect(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "MODE"))
			{
				Get_WIFI_Mode(1,s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "MAC_ADDR"))
			{
				Get_WIFI_MAC_ADDR(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "CUS_MAC_ADDR"))
			{
				Get_WIFI_CUSTOM_MAC_ADDR(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "AP_INFO"))
			{
				Get_AP_INFO(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HEARTBEAT"))
			{
				cJSON *responseObject = cJSON_CreateObject();// Create a RESPONSE object and add it to the RESPONSE array
				cJSON_AddNumberToObject(responseObject, "RSSI", app_info.rssi);

				cJSON *D2CPayload = cJSON_CreateObject();

				if(D2CPayload != NULL)
				{
					cJSON_AddItemToObject(D2CPayload, "PAYLOAD", responseObject);

					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(D2CPayload, payLoadData, sizeof(payLoadData), false);

					Send_CMD_To_Other_Actor(IHUB,"IHUB", payLoadData, strlen(payLoadData), "D2C_MESSAGE");

					if(D2CPayload != NULL)
					{
						cJSON_Delete(D2CPayload);
						D2CPayload = NULL;
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "SCAN" )){
				web_server_started=0;
				wifi_scan();
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "WEB_SERVER"))
			{
				if(web_server_started==0)
				{
					web_server(s_Message_Rx);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
             }
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PING"))
			 {
				pingServer(s_Message_Rx);
             }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET_SCANLIST_RAM"))
			{
				get_WifiScanList_RAM(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "NETWORK_SCAN"))
			{
				if(Network_Scan_Que == NULL)
				{
					Network_Scan_Que = xQueueCreateStatic(WIFI_NETWORKSCAN_QUEUE_LENGTH, WIFI_NETWORKSCAN_QUEUE_ITEMSIZE, NetScan_pucQueueStorage, &NetScan_pxQueueBuffer);
				}
				if (Network_Scan_Que == NULL){
#ifdef ENABLE_PRINT_MSG
					printf("ERROR(Network_Scan_Que is not created)\n");
#endif
				}
				if(Network_Scan_Handle == NULL)
				{
					AMessage_st s_Message_Rx_data;
					memset(Net_Scan_buffer,0,sizeof(Net_Scan_buffer));
					memcpy(&s_Message_Rx_data, s_Message_Rx, sizeof(AMessage_st));
					if((char*)s_Message_Rx_data.payload_p8 != NULL)
					{
						strncpy(Net_Scan_buffer, (char*)s_Message_Rx_data.payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
					}
					s_Message_Rx_data.payload_p8 = (uint8_t*)Net_Scan_buffer;
					Network_Scan_Handle = xTaskCreateStaticPinnedToCore(
									Network_Scan,                 // Task function
									"Network Scan",            // Task name
									WIFI_NETWORKSCAN_TASK_STACK_DEPTH,        // Stack size in words
									&s_Message_Rx_data,                    // Task parameters (not used here)
									2,                       // Task priority
									xTaskStack_NETSCAN,              // Pointer to task stack (allocated in PSRAM)
									&xWIFI_NETSCAN_TaskBuffer,             // Pointer to task control block
									1
					);

					if (Network_Scan_Handle == NULL) {
						    printf("Failed to create task\n");
						    // Handle error
						}
				}
				// get ethernet status
				cJSON *my_JSON1  	= cJSON_CreateArray();
				cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("ETH_STATUS"));
				cJSON *jsonObject = cJSON_CreateObject();
				cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
			    cJSON_Delete(jsonObject);
			    Send_CMD_To_Other_Actor(CONSOLE, "CONSOLE", payLoadData, strlen(payLoadData), "GET");
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "CONNECT_SSID"))
			{
				Wifi_connect_with_SSID(s_Message_Rx);

			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DEINIT"))
			{
				Add_Response_msg("WIFI DEINIT method is received", s_Message_Rx, payLoadData);
				Deinit_Actor(s_Message_Rx);
			}
//	       else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SWITCH_AP_MODE"))
//			{
//				Switch_AP_Mode(s_Message_Rx);
//			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "SCAN_RETURN"))
			{
				WIFI_Scan_Return(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET_CONN_INFO"))
			{
				Get_Connectivity_Info_For_update_Twin(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESETDISCONCOUNT"))
			{
				wifiDisconnectCount = 0;
			}
			else
			{
				//wifi error message: invalid method
				Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}// monitor


void wifi_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstEntry_bool == false)
	{
		if(strcmp((char*)s_Message->cmdFun_a8, "DEINIT") == 0)
			return;
		//memset(&xWIFITaskBuffer, 0, sizeof(xWIFITaskBuffer));
		init(s_Message);
	}

	uint8_t state = xQueueSend(msg_Rx_Queue, s_Message, QUE_DELAY);
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<WIFI.ERROR(WIFI RX Queue is full). \n");
		}
		else
		{
			printf("<WIFI.ERROR(WIFI RX Queue send unsuccessful).");
		}
	}
}//	wifi_ConsolWriteToActor

static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {

	uint8_t out_val[128]  	= {0}; //(uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char payload[100] = {0};
	cJSON 	*my_JSON  	= cJSON_CreateObject();
	AMessage_st s_Message_Tx;
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
	if(!(strcmp(parameter, "WIFI_STATUS")))
	{
		switch (*(uint8_t *)value)
		{
			case E_wifi_ZERO_STATE:
				cJSON_AddStringToObject(my_JSON, "STATUS", "WIFI IDLE STATE");
				break;

			case E_wifi_NOT_CONNECTED:
				cJSON_AddStringToObject(my_JSON, "STATUS", "WIFI NOT CONNECTED");
				break;

			case E_wifi_CONNECTED_LOW:
				cJSON_AddStringToObject(my_JSON, "STATUS", "WIFI CONNECTED");
				break;

			case E_wifi_CONNECTED_HIGH:
				cJSON_AddStringToObject(my_JSON, "STATUS", "WIFI CONNECTED IN STA MODE");
				break;

			case E_wifi_CONNECTED_AP:
				cJSON_AddStringToObject(my_JSON, "STATUS", "WIFI CONNECTED IN AP MODE");
				break;

			default:
				break;
		}
	}

	s_Message_Tx.payload_size = strlen((char*)out_val);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);

	cJSON_PrintPreallocated(my_JSON, payload, sizeof(payload), false);

	uint8_t *newpointer = (uint8_t*) heap_caps_calloc(strlen((char*) payload) + 1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*)s_Message_Tx.payload_p8, payload);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor


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

static void update_ip_change_datetime(char *dest, size_t dest_len)
{
	time_t now;
	struct tm timeinfo = {0};

	time(&now);
	gmtime_r(&now, &timeinfo);
	strftime(dest, dest_len, "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
}

//------------------------ WIFI Actor Methods Starts ------------------------------------------------------------------------//

static void wifi_setup(void *a, void *b) {
	s_Para.status_u8 = E_wifi_ZERO_STATE;
	set_to_other_actor("CONSOLE",U_INT8, "WIFI_STATUS", &s_Para.status_u8);
	wifi_actor_Initialise();
	wifi_Init_as_STA();

    // ? Ensure WiFi is actually started before any esp_wifi_get_* calls elsewhere
    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) != ESP_OK) {
        printf("WiFi not initialized properly\n");
    }
}

/**
 * @brief Initializes the WiFi connection attempt tracking variables.
 *
 * This function sets up the global variables used to track the current and total
 * number of connection attempts. It is called before starting the connection process.
 *
 * @param ssid_attempts The total number of SSIDs to attempt.
 */
void init_wifi_connection_attempts(int ssid_attempts) {
    current_ssid_attempt = 0;
    total_ssid_attempts = ssid_attempts;
    wifi_connected = false;
}


/**
 * @brief Starts the WiFi connection process.
 *
 * This function initializes the connection attempt variables and begins the
 * process of connecting to the first SSID in the list. It parses the list of
 * available SSIDs and manages the connection attempts.
 */
void start_wifi_connection_process(const char *ssid, const char *pass, AMessage_st* s_Message_Rx) {
	 if (!g_ScanListArray) return;

	// Parse g_ScanListArray to count SSIDs with the same name
    cJSON *root = cJSON_Parse((char*)g_ScanListArray);

    if (!root) return;

    cJSON *networks = cJSON_GetObjectItem(root, "WIFI_NETWORKS");
    total_ssid_attempts = cJSON_GetArraySize(networks);  // Count the number of available networks
    // Initialize connection attempt tracking
    init_wifi_connection_attempts(total_ssid_attempts);
    // Start with the first SSID
    trigger_next_wifi_attempt(ssid, pass,s_Message_Rx);  // Pass SSID and Password to trigger_next_wifi_attempt
    cJSON_Delete(root);  // Clean up JSON object
}



/**
 * @brief Triggers the next WiFi connection attempt.
 *
 * This function retrieves the next SSID and BSSID from the list and attempts
 * to connect to that network using the provided password. It is called
 * by the event handler when a disconnection occurs.
 */
void trigger_next_wifi_attempt(const char *stored_ssid, const char *stored_pass, AMessage_st* s_Message_Rx) {
    cJSON *network = NULL;
    cJSON *json_ssid = NULL;
    cJSON *json_bssid = NULL;
    cJSON *json_RSSI = NULL;
    uint8_t bssid[6];
    char AP_mac[32] = {0};
    int RSSI = 0;

    if (!g_ScanListArray) return;

	cJSON *root = cJSON_Parse((char*)g_ScanListArray);

	if (!root) return;

    cJSON *networks = cJSON_GetObjectItem(root, "WIFI_NETWORKS");
    if (!cJSON_IsArray(networks)) {
        cJSON_Delete(root);
        return;
    }

#ifdef ENABLE_PRINT_MSG
    // Print the full list of available SSIDs for debugging
    printf("Available SSIDs:\n");
    for (int i = 0; i < cJSON_GetArraySize(networks); i++) {
        cJSON *network = cJSON_GetArrayItem(networks, i);
        cJSON *json_ssid = cJSON_GetObjectItem(network, "SSID");
        if (json_ssid && cJSON_IsString(json_ssid)) {
            printf("Index %d: SSID = %s (Length: %d)\n", i, json_ssid->valuestring, (int)strlen(json_ssid->valuestring));
        } else {
            printf("Index %d: Invalid SSID\n", i);
        }
    }
#endif

//    bool connection_attempted = false;
    int max_retries = 3;  // Retry a maximum of 3 times
    int timeout_ms = 10000;  // Set timeout to 10 seconds (10000 ms)

    while (current_ssid_attempt < total_ssid_attempts) {
        network = cJSON_GetArrayItem(networks, current_ssid_attempt);

        if (!network) break;

        json_ssid = cJSON_GetObjectItem(network, "SSID");
        json_bssid = cJSON_GetObjectItem(network, "AP_MAC");
        json_RSSI = cJSON_GetObjectItem(network, "RSSI");
#ifdef ENABLE_PRINT_MSG
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(network, payLoadData, sizeof(payLoadData), false);	
        printf("Network Object: %s,item no:%d,total_ssid_attempts:%d\n", payLoadData,current_ssid_attempt,total_ssid_attempts);
#endif

//        if (json_ssid && cJSON_IsString(json_ssid)) {
        if (!json_ssid || !cJSON_IsString(json_ssid) ||
            !json_bssid || !cJSON_IsString(json_bssid)) {
            current_ssid_attempt++;
            continue;
        }
#ifdef ENABLE_PRINT_MSG
            // Print each character and its ASCII value
            printf("Comparing SSID at index %d: '%s' (Length: %d) vs Stored SSID: '%s' (Length: %d)\n",
                   current_ssid_attempt, json_ssid->valuestring, (int)strlen(json_ssid->valuestring),
                   stored_ssid, (int)strlen(stored_ssid));
#endif

            if (strcmp(stored_ssid, json_ssid->valuestring) == 0) {
                // SSID matches, proceed with connection attempt
//                connection_attempted = true;
                int retry_count = 0;

                // Convert BSSID from string to byte array
                memset(bssid,0,sizeof(bssid));
                sscanf(cJSON_GetStringValue(json_bssid), "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
                       &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
                if(cJSON_IsNumber(json_RSSI))
                	RSSI = json_RSSI->valueint;
                memset(AP_mac,0,sizeof(AP_mac));
    			sprintf((char*)AP_mac,"%02X%02X%02X%02X%02X%02X", bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);


#ifdef ENABLE_PRINT_MSG
                printf("Matching SSID found at index %d: %s. Attempting to connect with BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",
                       current_ssid_attempt, json_ssid->valuestring,
                       bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
#endif

                // Retry logic
                bool connected = false;
                while (retry_count < max_retries && !connected) {
#ifdef ENABLE_PRINT_MSG
                    printf("Attempt #%d to join SSID: %s with BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",
                           retry_count + 1, json_ssid->valuestring, bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
#endif
                    // Attempt to connect to this AP using wifi_join
                    connected = wifi_join(stored_ssid, stored_pass, bssid, timeout_ms);

                   if(s_Para.wifi_disconnect_reason == 15) // if disconnect due to WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT then don't re-attempt.
                    {
                    	max_retries = 1;
                    }

                    if (connected) {
#ifdef ENABLE_PRINT_MSG
                        printf("Successfully connected to SSID: %s at index %d\n", json_ssid->valuestring, current_ssid_attempt);
#endif
               	    if(wifiConnDisconnFlag == 0)
                        {
                        	wifiConnDisconnFlag = 1;
                        }

                        strcpy((char*)s_Message_Rx->cmdFun_a8, "EVENT");
                        strcpy((char*)s_Message_Rx->src_Actor_a8, "SYSTEM");  

                        s_Para.wifi_disconnect_reason  = 0;
                        cJSON_Delete(root);  // Clean up JSON object
                        return;  // Exit function after successful connection
                    } else {
                        retry_count++;
#ifdef ENABLE_PRINT_MSG
                        printf("Failed to connect to SSID: %s at index %d, retry count: %d\n", json_ssid->valuestring, current_ssid_attempt, retry_count);
#endif
                    }
                }
                // If we have exhausted the retries
                if (!connected) {
#ifdef ENABLE_PRINT_MSG
                    printf("All retries failed for SSID: %s at index %d. Stopping WiFi, delaying, and trying next SSID.\n", json_ssid->valuestring, current_ssid_attempt);
#endif
                    // ? safer than blindly calling esp_wifi_start()
                    esp_wifi_stop();
                    // Stop WiFi, delay for 100 ms, then start WiFi again
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                    esp_wifi_start();
                }
            } else {
#ifdef ENABLE_PRINT_MSG
                printf("SSID '%s' at index %d does not match the desired SSID '%s'. Skipping this AP.\n",
                       json_ssid->valuestring, current_ssid_attempt, stored_ssid);
#endif
            }
//        } else {
//#ifdef ENABLE_PRINT_MSG
//            printf("Invalid SSID in scan result at index %d. Skipping this AP.\n", current_ssid_attempt);
//#endif
//        }

        // Increment the current attempt counter to try the next SSID
        current_ssid_attempt++;
//        printf("current_ssid_attempt %d.\n",current_ssid_attempt);
    }
    s_Para.WIFI_Scan_Mode_u8 = WIFI_SCAN_TYPE_ACTIVE;
    cJSON *root_response = cJSON_CreateObject();
    cJSON_AddStringToObject(root_response, "WIFI_CONNECTION_STATUS", "DISCONNECTED");
    cJSON_AddStringToObject(root_response, "REASON", log_wifi_disconn_reason(s_Para.wifi_disconnect_reason));
    cJSON_AddStringToObject(root_response, "SSID", stored_ssid);
    cJSON_AddStringToObject(root_response, "PASSWORD", stored_pass);
    cJSON_AddStringToObject(root_response, "AP_MAC", (char*)AP_mac);
    cJSON_AddStringToObject(root_response, "IP_ADDR", (char*)s_Para.STA_IP_ADDR_a8);
    cJSON_AddNumberToObject(root_response, "RSSI", RSSI);
    if(wifiConnDisconnFlag == 1)
    {
    	wifiDisconnectCount++;
    	wifiConnDisconnFlag = 0;
    }

    strcpy((char*)s_Message_Rx->cmdFun_a8, "EVENT");
    strcpy((char*)s_Message_Rx->src_Actor_a8, "SYSTEM");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(root_response, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	s_Message_Rx->payload_size = strlen((char*)s_Message_Rx->payload_p8);
    cJSON_Delete(root_response);
    cJSON_Delete(root);  // Clean up JSON object
    console_send_responce_to_console_xface(s_Message_Rx);
    Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",payLoadData,strlen(payLoadData),"SAVE_WIFI_AUDIT_LOG");
	char str2 [100]= {0};
	sprintf(str2,"WIFI DISCONNECT COUNT = %d", wifiDisconnectCount);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str2,strlen(str2),"SAVE_WIFI_AUDIT_LOG");
}





/**
 * @brief Initiates the WiFi connection process.
 *
 * This function configures the network settings (DHCP or static IP) and starts
 * the WiFi connection process. The actual connection attempts, including retries
 * across multiple SSIDs with the same name, are handled asynchronously by the event handler.
 *
 * @param ssid The SSID to connect to.
 * @param pass The password for the WiFi network.
 * @param s_Message_Rx A pointer to the message structure for response handling.
 * @return uint16_t 0 on success, 1 on error.
 */

static uint16_t  wifi_disconnect(AMessage_st* s_Message_Rx) {
	char str1 [100]= {0};
	char str2 [100]= {0};

    wifi_mode_t mode;
    if (esp_wifi_get_mode(&mode) == ESP_OK) {
        esp_wifi_disconnect();
    }

	wifi_status = false;
	s_Para.status_u8 =  E_wifi_NOT_CONNECTED;
	int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT_u16, pdFALSE,pdTRUE, 2000 / portTICK_PERIOD_MS);
	strcpy(str1,"WIFI is disconnected because \"DISCONNECT\" method is received.");

    if(wifiConnDisconnFlag == 1)
    {
    	wifiDisconnectCount++;
    	wifiConnDisconnFlag = 0;
    }
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str1,strlen(str1),"SAVE_WIFI_AUDIT_LOG");

	sprintf(str2,"WIFI DISCONNECT COUNT = %d", wifiDisconnectCount);

	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str2,strlen(str2),"SAVE_WIFI_AUDIT_LOG");
	vTaskDelay(100 / portTICK_PERIOD_MS);
	return (bits & CONNECTED_BIT_u16) != 0;
}//wifi_disconnect

extern bool esp_event_loop_created;

/**
 * @brief Handles WiFi and IP events.
 *
 * This function processes WiFi events such as connection, disconnection, and IP acquisition.
 * It manages connection retries across multiple SSIDs and only triggers a disconnect message
 * after all connection attempts have failed.
 *
 * @param arg User-defined argument.
 * @param event_base The base ID of the event.
 * @param event_id The ID of the event.
 * @param event_data Additional event data.
 * @param s_Message_Rx A pointer to the message structure for response handling.
 */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    char str1[100] = {0};
    AMessage_st s_Message_Rx_New;

    // Initialize message structure for sending responses
    s_Message_Rx_New.Dest_ID_a8 = WIFI;
    s_Message_Rx_New.Src_ID_u8 = SYSTEM;
    strcpy((char*)s_Message_Rx_New.dest_Actor_a8, "WIFI");
    strcpy((char*)s_Message_Rx_New.src_Actor_a8, "SYSTEM");
    strcpy((char*)s_Message_Rx_New.cmdFun_a8, "EVENT");
    memset(data_buffer, 0 ,sizeof(data_buffer));
    s_Message_Rx_New.payload_p8 = (uint8_t*) data_buffer;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event_reason = (wifi_event_sta_disconnected_t *)event_data;

#ifdef ENABLE_PRINT_MSG
        printf("WiFi disconnected with reason code: %d\n", event_reason->reason);
#endif
        s_Para.wifi_disconnect_reason = event_reason->reason;

        char safe_ssid[33] = {0};   // max SSID length = 32
        memcpy(safe_ssid, event_reason->ssid, sizeof(event_reason->ssid));

        if (wifi_connected) {
            wifi_connected = false;  // Reset the connected status
            sprintf((char*)s_Para.STA_IP_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
            sprintf((char*)s_Para.STA_GW_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
            sprintf((char*)s_Para.STA_SM_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
            strcpy((char*)s_Para.WIFI_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00");
            s_Para.status_u8 = E_wifi_NOT_CONNECTED;
            set_to_other_actor("CONSOLE", U_INT8, "WIFI_STATUS", &s_Para.status_u8);
            // Send disconnection message
            s_Para.WIFI_Scan_Mode_u8 = WIFI_SCAN_TYPE_ACTIVE;
            cJSON *root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "WIFI_CONNECTION_STATUS", "DISCONNECTED");

            if(wifiConnDisconnFlag == 1)
            {
            	wifiDisconnectCount++;
            	wifiConnDisconnFlag = 0;
            }

            if(s_Para.wifi_disconnect_reason == 0)
            {
//            	if(strlen((char*)event_reason->ssid) != 0)


            	if (safe_ssid[0] != '\0')
            	{
					snprintf(str1, sizeof(str1), "SSID '%s' not found", safe_ssid);
					cJSON_AddStringToObject(root, "REASON", str1);
					cJSON_AddStringToObject(root, "PASSWORD", stored_pass);
            	}
            	else
            	{
            		cJSON_AddStringToObject(root, "REASON", "AP not found..");
            	}
            }
            else
            {
            	cJSON_AddStringToObject(root, "REASON", log_wifi_disconn_reason(s_Para.wifi_disconnect_reason));
            	cJSON_AddStringToObject(root, "PASSWORD", stored_pass);
            }
            cJSON_AddNumberToObject(root, "RSSI", event_reason->rssi);
            snprintf(str1, sizeof(str1), "%02x:%02x:%02x:%02x:%02x:%02x", event_reason->bssid[0],event_reason->bssid[1],event_reason->bssid[2],event_reason->bssid[3],event_reason->bssid[4],event_reason->bssid[5]);
            cJSON_AddStringToObject(root, "AP_MAC", str1);
    		cJSON_AddStringToObject(root, "IP ADDR", (char*)s_Para.STA_IP_ADDR_a8);
			memset(payLoadData_Event,0,sizeof(payLoadData_Event));
			cJSON_PrintPreallocated(root, payLoadData_Event, sizeof(payLoadData_Event), false);
			strcpy((char*)s_Message_Rx_New.payload_p8, payLoadData_Event);
			s_Message_Rx_New.payload_size = strlen((char*)s_Message_Rx_New.payload_p8);
            cJSON_Delete(root);
            console_send_responce_to_console_xface(&s_Message_Rx_New);
            Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",payLoadData_Event,strlen(payLoadData_Event),"SAVE_WIFI_AUDIT_LOG");
            xEventGroupClearBits(wifi_event_group, CONNECTED_BIT_u16);

        	char str2 [100]= {0};
        	sprintf(str2,"WIFI DISCONNECT COUNT = %d", wifiDisconnectCount);

        	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str2,strlen(str2),"SAVE_WIFI_AUDIT_LOG");
        } else {
             //current_ssid_attempt++;
            if (current_ssid_attempt < total_ssid_attempts) {
            } else {
                sprintf((char*)s_Para.STA_IP_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
                sprintf((char*)s_Para.STA_GW_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
                sprintf((char*)s_Para.STA_SM_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
                strcpy((char*)s_Para.WIFI_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00");
                s_Para.status_u8 = E_wifi_NOT_CONNECTED;
                set_to_other_actor("CONSOLE", U_INT8, "WIFI_STATUS", &s_Para.status_u8);

                s_Para.WIFI_Scan_Mode_u8 = WIFI_SCAN_TYPE_ACTIVE;
                // Send disconnection message
                cJSON *root = cJSON_CreateObject();
                cJSON_AddStringToObject(root, "WIFI_CONNECTION_STATUS", "DISCONNECTED");
                if(wifiConnDisconnFlag == 1)
                {
                	wifiDisconnectCount++;
                	wifiConnDisconnFlag = 0;
                }

                cJSON_AddStringToObject(root, "REASON", log_wifi_disconn_reason(s_Para.wifi_disconnect_reason));
//                cJSON_AddStringToObject(root, "SSID", (char*)event_reason->ssid);
                cJSON_AddStringToObject(root, "SSID", safe_ssid);
                cJSON_AddStringToObject(root, "PASSWORD", stored_pass);
				cJSON_AddNumberToObject(root, "RSSI", event_reason->rssi);
				snprintf(str1, sizeof(str1), "%02x:%02x:%02x:%02x:%02x:%02x", event_reason->bssid[0],event_reason->bssid[1],event_reason->bssid[2],event_reason->bssid[3],event_reason->bssid[4],event_reason->bssid[5]);
				cJSON_AddStringToObject(root, "AP_MAC", str1);
				cJSON_AddStringToObject(root, "IP_ADDR", (char*)s_Para.STA_IP_ADDR_a8);

				memset(payLoadData_Event,0,sizeof(payLoadData_Event));
				cJSON_PrintPreallocated(root, payLoadData_Event, sizeof(payLoadData_Event), false);
				strcpy((char*)s_Message_Rx_New.payload_p8, payLoadData_Event);
				s_Message_Rx_New.payload_size = strlen((char*)s_Message_Rx_New.payload_p8);
                cJSON_Delete(root);
                console_send_responce_to_console_xface(&s_Message_Rx_New);
                Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",payLoadData_Event,strlen(payLoadData_Event),"SAVE_WIFI_AUDIT_LOG");

            	char str2 [100]= {0};
            	sprintf(str2,"WIFI DISCONNECT COUNT = %d", wifiDisconnectCount);

            	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str2,strlen(str2),"SAVE_WIFI_AUDIT_LOG");

                xEventGroupClearBits(wifi_event_group, CONNECTED_BIT_u16);
            }
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT_u16);
        wifi_connected = true;  // Set connected status to true
        current_ssid_attempt = 0;
        total_ssid_attempts = 0;
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        char previous_ip[sizeof(s_Para.STA_IP_ADDR_a8)] = {0};
        strncpy(previous_ip, (char*)s_Para.STA_IP_ADDR_a8, sizeof(previous_ip) - 1);
        sprintf((char*)s_Para.STA_IP_ADDR_a8, IPSTR, IP2STR(&event->ip_info.ip));
        sprintf((char*)s_Para.STA_GW_ADDR_a8, IPSTR, IP2STR(&event->ip_info.gw));
        sprintf((char*)s_Para.STA_SM_ADDR_a8, IPSTR, IP2STR(&event->ip_info.netmask));
        if ((strcmp(previous_ip, (char*)s_Para.STA_IP_ADDR_a8) != 0) &&
            (strcmp((char*)s_Para.STA_IP_ADDR_a8, "0.0.0.0") != 0))
        {
        	update_ip_change_datetime((char*)s_Para.STA_IP_UPDATED_DATETIME_a8, sizeof(s_Para.STA_IP_UPDATED_DATETIME_a8));
        }

        s_Para.status_u8 = E_wifi_CONNECTED_LOW;  // Save wifi status to console
        set_to_other_actor("CONSOLE",U_INT8, "WIFI_STATUS", &s_Para.status_u8);

        esp_wifi_sta_get_ap_info(&app_info);
        s_Para.WIFI_Scan_Mode_u8 = WIFI_SCAN_TYPE_PASSIVE;
        cJSON *root_response = cJSON_CreateObject();
        cJSON_AddStringToObject(root_response, "WIFI_CONNECTION_STATUS", "CONNECTED");
        cJSON_AddStringToObject(root_response, "SSID", (char*)app_info.ssid);
        cJSON_AddStringToObject(root_response, "PASSWORD", stored_pass);
    	sprintf(str1, "%02x:%02x:%02x:%02x:%02x:%02x", app_info.bssid[0],app_info.bssid[1],app_info.bssid[2],app_info.bssid[3],app_info.bssid[4],app_info.bssid[5]);
        sprintf((char*)s_Para.WIFI_GATEWAY_MAC_ADDR_a8,
                "%02x:%02x:%02x:%02x:%02x:%02x",
                app_info.bssid[0], app_info.bssid[1], app_info.bssid[2],
                app_info.bssid[3], app_info.bssid[4], app_info.bssid[5]);
        cJSON_AddStringToObject(root_response, "AP_MAC", str1);
        cJSON_AddNumberToObject(root_response, "RSSI", app_info.rssi);
        cJSON_AddStringToObject(root_response, "IP ADDR", (char*)s_Para.STA_IP_ADDR_a8);
    	memset(payLoadData_Event,0,sizeof(payLoadData_Event));
    	cJSON_PrintPreallocated(root_response, payLoadData_Event, sizeof(payLoadData_Event), false);
    	strcpy((char*)s_Message_Rx_New.payload_p8, payLoadData_Event);
    	s_Message_Rx_New.payload_size = strlen((char*)s_Message_Rx_New.payload_p8);
        cJSON_Delete(root_response);
        console_send_responce_to_console_xface(&s_Message_Rx_New);

#ifdef ENABLE_PRINT_MSG
        printf("IP address acquired, connection successful.\n");
#endif
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_CONNECTED) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT_u16);
        strcpy(str1, "WIFI is connected in station mode.");
        if(wifiConnDisconnFlag == 0)
        {
        	wifiConnDisconnFlag = 1;
        }

        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str1, strlen(str1), "SAVE_WIFI_AUDIT_LOG");
        wifi_connected = true;  // Set connected status to true

#ifdef ENABLE_PRINT_MSG
        printf("WiFi connection established.\n");
#endif
    }
}



static void wifi_Init_as_STA(void) {
//	int ha;
	esp_log_level_set("wifi",ESP_LOG_NONE); //ESP_LOG_WARN
	static bool initialized = false;
	if (initialized) {
		return;
	}
	ESP_ERROR_CHECK(esp_netif_init());
	wifi_event_group = xEventGroupCreate();

	if (!esp_event_loop_created) {
		esp_event_loop_created = true;
		// Create default event loop that running in background
		ESP_ERROR_CHECK(esp_event_loop_create_default());
	}

	sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_CONNECTED, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_NULL));

	memset(ap_info, 0, sizeof(ap_info));
	s_config_wifi.mode = WIFI_MODE_STA;
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	 wifi_pmf_config_t pmf_cfg = {
	        .capable = true,    // Enable PMF capable
	        .required = false,  // Enable PMF required if needed
	    };
	wifi_config_t wifi_config = {
	        .sta = {
	            .pmf_cfg = pmf_cfg,
	        },
	    };
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));

	esp_wifi_set_ps(WIFI_PS_NONE);    // set wifi power saving to none
	esp_wifi_set_inactive_time(WIFI_IF_STA, 30);   // set STA inactive time to 30 sec
	esp_wifi_set_bandwidth(WIFI_IF_STA, WIFI_BW_HT20);
	ESP_ERROR_CHECK(esp_wifi_start());
	initialized = true;
}

/**
 * @brief Attempts to join a WiFi network.
 *
 * This function attempts to connect to a specific SSID and BSSID (AP MAC address).
 * It handles setting the SSID, BSSID, and password, and then initiates the connection.
 *
 * @param ssid The SSID to connect to.
 * @param pass The password for the WiFi network.
 * @param bssid The MAC address of the specific access point to connect to.
 * @param timeout_ms The timeout for the connection attempt in milliseconds.
 * @return bool True if connected, false otherwise.
 */
static bool wifi_join(const char *ssid, const char *pass, const uint8_t *bssid, int timeout_ms) {
    char str1[200] = {0}; // heap_caps_calloc(200,sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);  // Allocate more space for detailed logging
    char str2 [100]= {0};
    wifi_config_t wifi_config = { 0 };

    // Always clear event bit before attempting new connection
    xEventGroupClearBits(wifi_event_group, CONNECTED_BIT_u16);

    // Always disconnect before reconfiguring
    esp_wifi_disconnect();
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Copy SSID into WiFi configuration
    strlcpy((char*) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));

#ifdef ENABLE_PRINT_MSG
    printf("Attempting to join SSID: %s\n", ssid);
#endif

    // Set the BSSID (MAC address of the AP) to connect to a specific AP
    if (bssid) {
        memcpy(wifi_config.sta.bssid, bssid, sizeof(wifi_config.sta.bssid));
        wifi_config.sta.bssid_set = true; // Enable BSSID setting
#ifdef ENABLE_PRINT_MSG
        printf("Setting BSSID: %02x:%02x:%02x:%02x:%02x:%02x\n",
               bssid[0], bssid[1], bssid[2], bssid[3], bssid[4], bssid[5]);
#endif
    } else {
#ifdef ENABLE_PRINT_MSG
        printf("No BSSID specified, connecting to any AP with matching SSID.\n");
#endif
        wifi_config.sta.bssid_set = false; // Connect to any AP with the SSID
    }

    // Check and set the WiFi password
    if (pass && strlen(pass) > 0) {
        strlcpy((char*) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
#ifdef ENABLE_PRINT_MSG
        printf("Password provided for SSID: %s, Password: %s\n", ssid, pass);
#endif
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;  // Open network if no password
#ifdef ENABLE_PRINT_MSG
        printf("No password provided, assuming open network.\n");
#endif
    }

    // Configure PMF (Protected Management Frames)
    wifi_pmf_config_t pmf_cfg = {
        .capable = true,    // Enable PMF capable
        .required = false,  // PMF not required
    };
    wifi_config.sta.pmf_cfg = pmf_cfg;
    wifi_config.sta.threshold.rssi = -90;  // set minimum RSSI threshold value to -90. If AP's RSSI is better than -90 dBm then ESP will connect to that AP otherwise not.
    // Set WiFi mode to STA (Station)
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Set WiFi configuration
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

#ifdef ENABLE_PRINT_MSG
    printf("WiFi configuration set, attempting to connect...\n");
#endif

    // Attempt to connect to the WiFi network
    esp_err_t err = esp_wifi_connect();
	s_Para.wifi_disconnect_reason = err;
    if (err != ESP_OK) {
        sprintf(str1, "Error! Cannot connect to WIFI due to %s", esp_err_to_name(err));
        if(wifiConnDisconnFlag == 1)
        {
        	wifiDisconnectCount++;
        	wifiConnDisconnFlag = 0;
        }
        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str1, strlen(str1), "SAVE_WIFI_AUDIT_LOG");

    	sprintf(str2,"WIFI DISCONNECT COUNT = %d", wifiDisconnectCount);

        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str2, strlen(str2), "SAVE_WIFI_AUDIT_LOG");
#ifdef ENABLE_PRINT_MSG
        printf("Failed to initiate WiFi connection: %s\n", esp_err_to_name(err));
#endif
//        free(str1);
        return false;
    }

   // Store connected SSID and password (if provided)
    strcpy((char*)s_Para.Connected_ssid, (char*) wifi_config.sta.ssid);
    if (pass && strlen(pass) > 0) {
        strcpy((char*)s_Para.Connected_pass, (char*) wifi_config.sta.password);
    }
    else
    	memset(s_Para.Connected_pass, 0, PASS_LEN);


    // Wait for connection event
    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT_u16, pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);

    if (bits & CONNECTED_BIT_u16) {
        // Connection successful
#ifdef ENABLE_PRINT_MSG
        printf("Successfully connected to SSID: %s\n", ssid);
#endif
//        free(str1);
        return true;
    } else {
        // Connection failed or timed out
#ifdef ENABLE_PRINT_MSG
        printf("Failed to connect within the timeout period.\n");
#endif
        wifi_connected = false;
        sprintf((char*)s_Para.STA_IP_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
		sprintf((char*)s_Para.STA_GW_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
		sprintf((char*)s_Para.STA_SM_ADDR_a8, "%d.%d.%d.%d", 0, 0, 0, 0);
		strcpy((char*)s_Para.WIFI_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00");
		s_Para.status_u8 = E_wifi_NOT_CONNECTED;
		set_to_other_actor("CONSOLE", U_INT8, "WIFI_STATUS", &s_Para.status_u8);
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT_u16);
        return false;
    }
}


static void wifi_scan()
{
    uint16_t number_u16 = DEFAULT_SCAN_LIST_SIZE;
    esp_err_t err = -1;
    uint8_t mac[32] ={0};
	wifi_scan_config_t scan_config = {
		.ssid = NULL,
		.bssid = NULL,
		.channel = 0,
		.show_hidden = false,
		.scan_type = s_Para.WIFI_Scan_Mode_u8,
	};
    char retry = 0;
	cJSON *apObject[DEFAULT_SCAN_LIST_SIZE];

	while(retry <3)
	{
		scan_config.scan_time.active.min = 100;
		scan_config.scan_time.active.max = 150;
		err = esp_wifi_scan_start(&scan_config, true);
		if (err != ESP_OK) {
		#ifdef ENABLE_PRINT_MSG
			printf("Error in esp_wifi_scan_start - %s\n", esp_err_to_name(err));
		#endif
			esp_wifi_scan_stop();  // Cleanup
			//esp_wifi_disconnect();
			esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
			retry++;
			vTaskDelay(500 / portTICK_PERIOD_MS);
		continue;
		}

		// Get scan results
		err = esp_wifi_scan_get_ap_records(&number_u16, ap_info);
		if (err != ESP_OK) {
		#ifdef ENABLE_PRINT_MSG
			printf("Error in esp_wifi_scan_get_ap_records - %s\n", esp_err_to_name(err));
		#endif
			retry++;
			esp_wifi_scan_stop();  // Cleanup
			esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
			vTaskDelay(500  / portTICK_PERIOD_MS);
			continue;
		}

		err = esp_wifi_scan_get_ap_num(&ap_count_u16);
		if (err != ESP_OK) {
		#ifdef ENABLE_PRINT_MSG
			printf("Error in esp_wifi_scan_get_ap_num - %s\n", esp_err_to_name(err));
		#endif
			retry++;
			esp_wifi_scan_stop();  // Cleanup
			esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
			vTaskDelay(500  / portTICK_PERIOD_MS);
			continue;
		}

		cJSON *jsonArray = cJSON_CreateArray();
		if (!jsonArray) {
		#ifdef ENABLE_PRINT_MSG
			printf("Error creating JSON array\n");
		#endif
			esp_wifi_scan_stop();  // Cleanup
			esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
			return;
		}
		if(ap_count_u16 == 0)
		{
			#ifdef ENABLE_PRINT_MSG
			printf("Error! AP count is 0 \n");
			#endif
			retry++;
			cJSON_Delete(jsonArray);
			esp_wifi_scan_stop();  // Cleanup
			esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
			vTaskDelay(500 / portTICK_PERIOD_MS);
			continue;
		}
		for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count_u16); i++)
		{
			apObject[i] = cJSON_CreateObject();
			if (!apObject[i]) {
			#ifdef ENABLE_PRINT_MSG
				printf("Error creating JSON object for AP\n");
			#endif
				for (int j = 0; j < i; j++) { // Free previously created objects
					cJSON_Delete(apObject[j]);
				}
				cJSON_Delete(jsonArray);
//				esp_wifi_scan_stop();  // Cleanup
				esp_wifi_clear_ap_list();   // free the memory allocated by esp_wifi_scan_start
				return;
			}
//			if(strlen((char*)ap_info[i].ssid) == 0)
			char safe_ssid[33] = {0};
			memcpy(safe_ssid, ap_info[i].ssid, sizeof(ap_info[i].ssid));

			if (strlen(safe_ssid) == 0)
			{
				#ifdef ENABLE_PRINT_MSG
				printf("SSID is empty, retry count = %d \n", retry);
				#endif
				retry++;
				if(apObject[i] != NULL)
				{
					cJSON_Delete(apObject[i]);
					apObject[i] = NULL;
				}
				vTaskDelay(500/ portTICK_PERIOD_MS);
				continue;
			}
			if(should_skip_ssid((const char *)ap_info[i].ssid))
			{
				if(apObject[i] != NULL)
				{
					cJSON_Delete(apObject[i]);
					apObject[i] = NULL;
				}
				continue;
			}
//			cJSON_AddStringToObject(apObject[i], "SSID", (char*) ap_info[i].ssid);
			cJSON_AddStringToObject(apObject[i], "SSID", safe_ssid);
			cJSON_AddNumberToObject(apObject[i], "RSSI", ap_info[i].rssi);


			switch (ap_info[i].authmode)
			{
				case WIFI_AUTH_OPEN:
					cJSON_AddStringToObject(apObject[i], "Security", "open");
					break;
				case WIFI_AUTH_WEP:
					cJSON_AddStringToObject(apObject[i], "Security", "wep");
					break;
				case WIFI_AUTH_WPA_PSK:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa_psk");
					break;
				case WIFI_AUTH_WPA2_PSK:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa2_psk");
					break;
				case WIFI_AUTH_WPA_WPA2_PSK:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa_wpa2_psk");
					break;
				case WIFI_AUTH_WPA2_ENTERPRISE:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa2_enterprise");
					break;
				case WIFI_AUTH_WPA3_PSK:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa3_psk");
					break;
				case WIFI_AUTH_WPA2_WPA3_PSK:
					cJSON_AddStringToObject(apObject[i], "Security", "wpa2_wpa3_psk");
					break;
				default:
					cJSON_AddStringToObject(apObject[i], "Security", "unknown");
					break;
			}
			sprintf((char*)mac,"%02X%02X%02X%02X%02X%02X", ap_info[i].bssid[0], ap_info[i].bssid[1], ap_info[i].bssid[2], ap_info[i].bssid[3], ap_info[i].bssid[4], ap_info[i].bssid[5]);
			cJSON_AddStringToObject(apObject[i], "AP_MAC", (char*)mac);
			cJSON_AddItemToArray(jsonArray, apObject[i]);	
		}

		if((jsonArray != NULL) && cJSON_IsObject(jsonArray->child) && (No_Network_Retry < 2))
		{
			No_Network_Retry = 0;    // networks are available
			cJSON *root_new = cJSON_CreateObject();
			cJSON_AddItemToObject(root_new, "WIFI_NETWORKS", jsonArray);
			memset(payLoadData_Scan,0,sizeof(payLoadData_Scan));
			cJSON_PrintPreallocated(root_new, payLoadData_Scan, sizeof(payLoadData_Scan), false);
			g_ScanListArray = (uint8_t *)payLoadData_Scan;
			cJSON_Delete(root_new);
		}
		else
		{
			No_Network_Retry++;
			if(No_Network_Retry >= 3)    //No network available
			{
				No_Network_Retry = 0;
				cJSON *root_new = cJSON_CreateObject();
				cJSON_AddItemToObject(root_new, "WIFI_NETWORKS", jsonArray);
				memset(payLoadData_Scan,0,sizeof(payLoadData_Scan));
				cJSON_PrintPreallocated(root_new, payLoadData_Scan, sizeof(payLoadData_Scan), false);
				g_ScanListArray = (uint8_t *)payLoadData_Scan;
				cJSON_Delete(root_new);
			}
		}
		esp_wifi_clear_ap_list();  // // free the memory allocated by esp_wifi_scan_start
		esp_wifi_scan_stop();  // Cleanup
		break;
	} // end of while
}


static void wifi_actor_Initialise(void) {
	s_config_wifi.ssid 	  = (char*)s_Para.SSID_a8;
	s_config_wifi.pass 	  = (char*)s_Para.Pass_a8;
	s_config_wifi.timeout = DEFAULT_TIMEOUT_MS;
	s_config_wifi.mode 	  = WIFI_MODE_NULL;
}

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

static void Get_WIFI_Mode(uint8_t Send_to_console_flag,AMessage_st* s_Message_Rx)
{
	wifi_mode_t mode;
	esp_err_t err;
	err = esp_wifi_get_mode(&mode);
	if(err != ESP_OK)
	{
		Add_Response_msg("WiFi is not initialized by esp_wifi_init", s_Message_Rx, payLoadData);
		return;
	}

	else
	{
		switch(mode)
		{
			case WIFI_MODE_NULL:	sprintf((char*)s_Para.Mode_u8,"%s", "NULL");  	/**< null mode */
									break;
			case WIFI_MODE_STA:		sprintf((char*)s_Para.Mode_u8,"%s", "STA");		/**< WiFi station mode */
									break;
			case WIFI_MODE_AP:		sprintf((char*)s_Para.Mode_u8,"%s", "AP");		/**< WiFi soft-AP mode */
									break;
			case WIFI_MODE_APSTA:	sprintf((char*)s_Para.Mode_u8,"%s", "APSTA");		 /**< WiFi station + soft-AP mode */
									break;
			case WIFI_MODE_MAX:		sprintf((char*)s_Para.Mode_u8,"%s","MAX");
									break;
			case WIFI_MODE_NAN:		sprintf((char*)s_Para.Mode_u8,"%s","NAN");
												break;
		}
	}
	if(Send_to_console_flag  == 1)
	{
		Add_Response_msg((char*)s_Para.Mode_u8, s_Message_Rx, payLoadData);
	}
}

static void Get_AP_INFO(AMessage_st* s_Message_Rx)
{
	esp_err_t err;
	char str[100]={0};
	err = esp_wifi_sta_get_ap_info(&app_info);

	if(err == ESP_OK)
	{
		sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",app_info.bssid[0],app_info.bssid[1],app_info.bssid[2],app_info.bssid[3],
						app_info.bssid[4],app_info.bssid[5]);
		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "SSID", (char*)app_info.ssid);
		cJSON_AddStringToObject(responseObject, "BSSID", str);
		cJSON_AddNumberToObject(responseObject, "signal strength", app_info.rssi);
		switch (app_info.authmode)
		{
		case WIFI_AUTH_OPEN:  			cJSON_AddStringToObject(responseObject, "AP_authmode", "wifi_AUTH_OPEN");  			break;
		case WIFI_AUTH_WEP:				cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WEP");  			break;
		case WIFI_AUTH_WPA_PSK:			cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA_PSK");  		break;
		case WIFI_AUTH_WPA2_PSK:		cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA2_PSK");  		break;
		case WIFI_AUTH_WPA_WPA2_PSK:	cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA_WPA2_PSK");  	break;
		case WIFI_AUTH_WPA2_ENTERPRISE:	cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA2_ENTERPRISE"); break;
		case WIFI_AUTH_WPA3_PSK:		cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA3_PSK");  		break;
		case WIFI_AUTH_WPA2_WPA3_PSK:	cJSON_AddStringToObject(responseObject, "AP_authmode", "WIFI_AUTH_WPA2_WPA3_PSK");  break;
		default:						cJSON_AddStringToObject(responseObject, "AP_authmode", "wifi_AUTH_UNKNOWN");  		break;
		}
		
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(responseObject);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
	{
		if(err == ESP_ERR_WIFI_CONN)
		{

			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "ERROR","The station interface don't initialized");
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}

		if(err == ESP_ERR_WIFI_NOT_CONNECT)
		{
			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "ERROR","The station interface don't initialized.");
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
	}
}

static void Get_WIFI_MAC_ADDR(AMessage_st* s_Message_Rx)
{
	uint8_t mac[6]={0};
//	esp_base_mac_addr_get(mac);
    esp_err_t err = esp_base_mac_addr_get(mac);  // Get base MAC

    if (err != ESP_OK) {
//        send_json_error(s_Message_Rx, "Failed to read base MAC address");
        return;
    }
	sprintf((char*)s_Para.MAC_ID_a8,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
    if (!responseObject) {
//        send_json_error(s_Message_Rx, "JSON creation failed");
        return;
    }

	cJSON_AddStringToObject(responseObject, "MAC_ID", (char*) s_Para.MAC_ID_a8);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	s_Message_Rx->payload_p8[sizeof(s_Message_Rx->payload_p8) - 1] = '\0';
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Get_WIFI_CUSTOM_MAC_ADDR(AMessage_st* s_Message_Rx)
{
	uint8_t mac[6]={0}; //, str[50]={0};
//	esp_efuse_mac_get_custom(mac);
    esp_err_t err = esp_efuse_mac_get_custom(mac);
    if (err != ESP_OK) {
//        send_json_error(s_Message_Rx, "Failed to read custom MAC address");
        return;
    }

	sprintf((char*)s_Para.MAC_ID_a8,"%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array

    if (!responseObject) {
//        send_json_error(s_Message_Rx, "JSON creation failed");
        return;
    }

	cJSON_AddStringToObject(responseObject, "MAC_ID", (char*) s_Para.MAC_ID_a8);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

//static void wifi_join_AP(wifi_mode_t value)
//{
//	//esp_netif_ip_info_t ip_info;
//	esp_netif_ip_info_t ipInfo;
//	uint16_t address[4];
//
//	//wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//		  //  cfg.nvs_enable = false;
//
//		wifi_config_t w_config = {
//			.ap.ssid_len = 0,
//			.ap.channel = 1,
//			.ap.authmode = WIFI_AUTH_WPA2_PSK,
//			.ap.ssid_hidden = false,
//			.ap.max_connection = 4,
//			.ap.beacon_interval = 100,
//		};
//
//		strcpy((char*)w_config.ap.ssid, (char*)s_Para.SSID_AP_a8);
//		strcpy((char*)w_config.ap.password, (char*)s_Para.Pass_AP_a8);
//
//		esp_netif_t* wifiAP = esp_netif_create_default_wifi_ap();
//
//		str2array((char*)s_Para.AP_IP_ADDR_a8, address);  // IP address
//		IP4_ADDR(&ipInfo.ip, address[0],address[1],address[2],address[3]);
//
//		str2array((char*)s_Para.AP_GW_ADDR_a8, address);  //getway
//		IP4_ADDR(&ipInfo.gw, address[0],address[1],address[2],address[3]);
//
//		str2array((char*)s_Para.AP_SM_ADDR_a8, address);  //subnet mask
//		IP4_ADDR(&ipInfo.netmask, address[0],address[1],address[2],address[3]);
//
//
////		IP4_ADDR(&ipInfo.ip, 192,168,2,4);
////		IP4_ADDR(&ipInfo.gw, 192,168,2,1);
////		IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
//		esp_netif_dhcps_stop(wifiAP);
//		esp_netif_set_ip_info(wifiAP, &ipInfo);
//		esp_netif_dhcps_start(wifiAP);
//
//		ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
//		ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &w_config));
//		ESP_ERROR_CHECK(esp_wifi_start());
//}

static void wifi_init_softap(void)
{
	esp_err_t err;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();

//    esp_wifi_init(&cfg);

    // Initialize Wi-Fi driver
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
//        ESP_LOGE(TAG, "Wi-Fi init failed: %d", err);
        return;
    }

    wifi_config_t wifi_config = {
				.ap.ssid_len = strlen((char*)s_Para.SSID_AP_a8),//strlen(EXAMPLE_ESP_WIFI_SSID),
				.ap.max_connection = 4,//EXAMPLE_MAX_STA_CONN,
				.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK,
    };

	strcpy((char*)wifi_config.ap.ssid,(char*)s_Para.SSID_AP_a8);
	wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';

	strcpy((char*)wifi_config.ap.password,(char*)s_Para.Pass_AP_a8);
	wifi_config.ap.password[sizeof(wifi_config.ap.password) - 1] = '\0';

    if (strlen((char*)s_Para.Pass_AP_a8) == 0) {

        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
}

void web_server(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	uint16_t u16numberOfMinutes;
	char buffer1[100] = {0};  //heap_caps_calloc (100,sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char str[100] = {0};

	in_JSON 		    = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	name_JSON = cJSON_GetObjectItem(in_JSON, "NUMBER_OF_MINUTES");

	if (!name_JSON) {
	    Add_Response_msg("NUMBER_OF_MINUTES missing", s_Message_Rx, payLoadData);
	    cJSON_Delete(in_JSON);
	    return;
	}

	u16numberOfMinutes = name_JSON->valueint;
	cJSON_Delete(in_JSON);
	{
		vTaskDelete(WifiScanHandle);			// delete wifi scan task
		cJSON *my_JSON = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
		cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 0);		
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);	
		Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "SETSERVCONNFLAG");
		cJSON_Delete(my_JSON); // Free the cJSON object
		

		cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
		cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 0);		
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);	
		Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "SETEVTTASKFG");
		cJSON_Delete(my_JSON1); // Free the cJSON object
		

		Send_CMD_To_Other_Actor(IHUB,"IHUB", "\0", 0, "DISCONNECT");
		uint8_t val = 0;
		set_to_other_actor("CONSOLE", U_INT8, "JFS_DBG",&val);
		set_to_other_actor("CONSOLE", U_INT8, "U0_DBG", &val);

		Send_CMD_To_Other_Actor(IDLE, "IDLE", "\0", 0, "TEST_STOP");
		vTaskDelay(10000/portTICK_PERIOD_MS);
	  }

	wifi_init_softap();
	wifi_init_config_t cfg =WIFI_INIT_CONFIG_DEFAULT();

	esp_netif_t* wifiAP= esp_netif_create_default_wifi_ap();
	if(wifiAP!=NULL)
	 {
	 esp_netif_ip_info_t ipInfo;
	 IP4_ADDR(&ipInfo.ip, 10,10,100,254);
	 IP4_ADDR(&ipInfo.gw, 10,10,100,254);
	 IP4_ADDR(&ipInfo.netmask, 255,255,255,0);
	 esp_netif_dhcps_stop(wifiAP);
	 esp_netif_set_ip_info(wifiAP, &ipInfo);
	 esp_netif_dhcps_start(wifiAP);
	 }
	  wifi_config_t w_config =
	  {
					  .ap.ssid_len = 0,
					  .ap.channel = 1,
					  .ap.authmode = 0,
					  .ap.ssid_hidden = false,
					  .ap.max_connection = 4,
					  .ap.beacon_interval = 100,

	  };
	    strcpy((char*)w_config.ap.ssid,(char*)s_Para.SSID_AP_a8);
	    strcpy((char*)w_config.ap.password,(char*)s_Para.Pass_AP_a8);
	    if ((s_Para.Pass_AP_a8[0] == 0)||(s_Para.Pass_AP_a8[0] == 0x20))
	    {
	    	  w_config.ap.authmode =0;
	    }

		else
		{
			  w_config.ap.authmode =WIFI_AUTH_WPA_PSK;
			  strcpy((char*)w_config.ap.password,(char*)s_Para.Pass_AP_a8);
		}
	    strcpy((char*)w_config.ap.ssid,(char*)s_Para.SSID_AP_a8);
	    esp_wifi_init(&cfg);

		esp_wifi_set_mode(WIFI_MODE_AP);
		esp_wifi_set_config(WIFI_IF_AP, &w_config);

//		vTaskDelay(10000/portTICK_PERIOD_MS);

		s_Para.status_u8 =  E_wifi_CONNECTED_AP;
		set_to_other_actor("CONSOLE",U_INT8, "WIFI_STATUS", &s_Para.status_u8);
		cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
		cJSON_AddStringToObject(responseObject, "AP_SSID", (char*)s_Para.SSID_AP_a8);
		cJSON_AddStringToObject(responseObject, "AP_PASSWORD", (char*)w_config.ap.password);
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(responseObject);
		console_send_responce_to_console_xface(s_Message_Rx);

		sprintf(buffer1,"{\"Minutes\":%d}",u16numberOfMinutes);
		Send_CMD_To_Other_Actor(WEB_SERVER,"WEB_SERVER",buffer1, strlen(buffer1), "START");
		 web_server_started=1;
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char keyValue[50]={0};
	char str[200]={0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

if(s_Message_Rx->payload_p8 != NULL)
{
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n wifi s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	else
	{
			// Obtain the COMMAND and RESPONSE keys
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

			if (commandKey != NULL && responseKey != NULL)
			{				
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
		}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
	{
		static bool copy_files_flag = true;
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
	    {
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				// Iterate over the keys
				cJSON *currentItem = root->child;
				if(currentItem->valuestring != NULL)
				{
					if(!strcasecmp(currentItem->valuestring, "System/WIFI.json"))
					{
						currentItem = currentItem->next;
						while (currentItem != NULL)
						{
							if (cJSON_IsString(currentItem))   // Check the type of the value
							{
								set(currentItem->string, currentItem->valuestring,s_Message_Rx);
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
						if(copy_files_flag == true)
						{
							copy_files_flag = false;
							cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
							if (responseKey != NULL && cJSON_IsString(responseKey->child))
							{
								cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "RESP");
								if((name_JSON != NULL) && (name_JSON->valuestring != NULL))
								{
									if(strcasecmp(name_JSON->valuestring, "System/WIFI.json file is not present in JFS.") == 0)
									{
										Send_CMD_To_Other_Actor(SYS_FILES,"SYS_FILES", "\0", 0, "COPY_SPIFFS_JFS");
									}
									vTaskDelay(7000 / portTICK_PERIOD_MS);
									cJSON *responseObject = cJSON_CreateObject();
									cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/WIFI.json");
									memset(payLoadData,0,sizeof(payLoadData));
									cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
									Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
									cJSON_Delete(responseObject);
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

	 if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
	 {
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL)
		{
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
		if (responseKey != NULL && cJSON_IsString(responseKey->child))
		{
			cJSON* name_JSON 		= cJSON_GetObjectItem(responseKey, "ETH_STATUS");
			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
			{
				if(Network_Scan_Que != NULL)
				{					
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
		
					uint8_t state =  xQueueSend(Network_Scan_Que, payLoadData, QUE_DELAY);
					if (state != pdTRUE)
					{
						if (state == errQUEUE_FULL)
						{
#ifdef ENABLE_PRINT_MSG
							printf("<WIFI.ERROR (Network_Scan_Que is full)<CR>\n");
#endif
						}
						else
						{
#ifdef ENABLE_PRINT_MSG
							printf("<WIFI.ERROR(Network_Scan_Que send unsuccessful)<CR>");
#endif
						}
					}
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	return;
  }
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

void pingServer(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	char str[100] = {0};
	char IP_addr[50] = {0};
	int timeout = 0;
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		return;
	}
	else
	{
		cJSON *IP_address = cJSON_GetObjectItem(in_JSON, "IP_ADDRESS");
		if ((IP_address != NULL) && (IP_address->valuestring != NULL))
		{
			strcpy(IP_addr, IP_address->valuestring);
		}

		cJSON *Time_out = cJSON_GetObjectItem(in_JSON, "TIMEOUT");
		if ((Time_out != NULL))
		{
			timeout = Time_out->valuedouble;
		}
		if (initialize_ping(timeout, 2, IP_addr) == ESP_OK)
		{
			Add_Response_msg("Ping to the server is successful.",s_Message_Rx, payLoadData);
			strcpy(str, "Ping to the server is successful.");
		}
	 	else
	 	{
	 		Add_Response_msg("Ping to the server is failed.",s_Message_Rx, payLoadData);
	 		strcpy(str, "Ping to the server is failed.");
	 	}
	}
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM",str,strlen(str),"SAVE_WIFI_AUDIT_LOG");
	cJSON_Delete(in_JSON);
}//	pingServer

static void get_WifiScanList_RAM(AMessage_st* s_Message_Rx){
		if(g_ScanListArray != NULL)
		{
			char string [2048]= {0}; //heap_caps_calloc(strlen((const char*)g_ScanListArray)+1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
			{
				strcpy(string, (const char*)g_ScanListArray);
				s_Message_Rx->payload_p8 = (uint8_t*)string;
				console_send_responce_to_console_xface(s_Message_Rx);
			}
		}
		else
			Add_Response_msg("Kindly execute WIFI.SCAN method at first.",s_Message_Rx, payLoadData);
}
void set_ethernet_priority(AMessage_st* s_Message_Rx) {
    esp_netif_t *eth_netif = get_ethernet_netif_handle();
    char str[200] = {0}, payLoadData_NScan[200] = {0};
    if (eth_netif) {
        if (current_default_netif != eth_netif) {
            esp_err_t ret = esp_netif_set_default_netif(eth_netif);
            if (ret != ESP_OK) {
                 sprintf(str, "Failed to set Ethernet as default netif: %s\n", esp_err_to_name(ret));
                Add_Response_msg(str,s_Message_Rx, payLoadData_NScan);
            } else {
            	Add_Response_msg("Ethernet set as default network interface",s_Message_Rx, payLoadData_NScan);
                current_default_netif = eth_netif;
                Send_CMD_To_Other_Actor(ETH, "ETH", "", strlen(""), "REFRESH");
            }
        } else {
        	Add_Response_msg("Ethernet is already the default network interface",s_Message_Rx, payLoadData);
         }
    } else {
    	Add_Response_msg("Ethernet netif handle not found",s_Message_Rx, payLoadData_NScan);
    }
}

void set_wifi_priority(AMessage_st* s_Message_Rx) {
    esp_netif_t *wifi_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    char str[200] = {0}, payLoadData_NScan[200]={0};
    if (wifi_netif) {
        if (current_default_netif != wifi_netif) {
            esp_err_t ret = esp_netif_set_default_netif(wifi_netif);
            if (ret != ESP_OK) {
                sprintf(str,"Failed to set Wi-Fi as default netif: %s\n", esp_err_to_name(ret));
                Add_Response_msg(str,s_Message_Rx, payLoadData_NScan);
            } else {
                Add_Response_msg("Wi-Fi set as default network interface",s_Message_Rx, payLoadData_NScan);
                current_default_netif = wifi_netif;
            }
        } else {
        	//Add_Response_msg("Wi-Fi is already the default network interface",s_Message_Rx, payLoadData);
//            printf("Wi-Fi is already the default network interface\n");
        }
    } else {
    	Add_Response_msg("Wi-Fi netif handle not found",s_Message_Rx, payLoadData_NScan);
    }
}


static void Network_Scan(void *pvParameters __attribute__((unused))) {
    char buffer[200] = {0}, payLoadData_NScan[200] = {0};
    AMessage_st s_Message_Rx;
    strcpy((char*)s_Message_Rx.cmdFun_a8, "NETWORK_SCAN");
    strcpy((char*)s_Message_Rx.src_Actor_a8, "SYSTEM");
    strcpy((char*)s_Message_Rx.dest_Actor_a8, "WIFI");
    s_Message_Rx.payload_p8 = (uint8_t*)Net_Scan_buffer;
    cJSON* In_JSON = NULL;
    char str[200] = {0};
    cJSON *root = NULL;

    while(1)
    {
    if (pdTRUE == xQueueReceive(Network_Scan_Que, (void*)buffer, portMAX_DELAY)) {
        int eth_status = 0;
        int Final_RSSI = -150, arraySize = 0;

        root = cJSON_CreateObject();
        char Final_SSID [50]= {0}, Final_Pass [50]= {0}; //heap_caps_calloc(50,sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        cJSON *New_JSON = cJSON_Parse(buffer);
        if (New_JSON == NULL) {
        	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
        	Add_Response_msg(str, &s_Message_Rx, payLoadData_NScan);
            goto next;
        }

        cJSON *eth_status_json = cJSON_GetObjectItemCaseSensitive(New_JSON, "ETH_STATUS");

        if (cJSON_IsString(eth_status_json)) {
            eth_status = atoi(eth_status_json->valuestring);
        } else {
#ifdef ENABLE_PRINT_MSG
            printf("Invalid or missing ETH_STATUS\n");
#endif
        }

        cJSON_Delete(New_JSON);

        if(g_ScanListArray != NULL)
		{
			cJSON *ScanList = cJSON_Parse((char*)g_ScanListArray);
			if (ScanList == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str, &s_Message_Rx, payLoadData_NScan);
				goto exit;
			}

			In_JSON = cJSON_GetObjectItemCaseSensitive(ScanList, "WIFI_NETWORKS");
			if (In_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str, &s_Message_Rx, payLoadData_NScan);
				goto exit;
			}

			arraySize = cJSON_GetArraySize(In_JSON);

			// Check for highest priority Rescue SSID
			for (int i = 0; i < arraySize; i++) {
				cJSON *element = cJSON_GetArrayItem(In_JSON, i);
				if (element != NULL) {
					cJSON *SSID = cJSON_GetObjectItem(element, "SSID");
					if (cJSON_IsString(SSID) && (SSID->valuestring != NULL) &&
						(strcmp(SSID->valuestring, "") != 0)) {
						if ((strcmp(SSID->valuestring, (char*)&s_Para.SSID_a8) == 0) &&
							(strcmp((char*)&s_Para.SSID_a8, "") != 0)) {
							strcpy(Final_SSID, SSID->valuestring);
							strcpy(Final_Pass, (char*)&s_Para.Pass_a8);
							cJSON_AddStringToObject(root, "SSID", (char*)Final_SSID);
							cJSON_AddStringToObject(root, "PASSWORD", (char*)Final_Pass);
							// Set Wi-Fi as the primary interface
							set_wifi_priority(&s_Message_Rx);
							goto exit_new;
						}
					}
				}
			}
		}
        // Check for Ethernet availability
        if (eth_status == E_ETH_CONNECTED) {
            cJSON_AddStringToObject(root, "ETHERNET", "AVAILABLE");
            // Set Ethernet as the primary interface
            set_ethernet_priority(&s_Message_Rx);
            goto exit_new;
        }

next:
        for (int i = 0; i < arraySize; i++) {
            cJSON *element = cJSON_GetArrayItem(In_JSON, i);
            if (element != NULL) {
                cJSON *SSID = cJSON_GetObjectItem(element, "SSID");
                if (cJSON_IsString(SSID) && (SSID->valuestring != NULL) &&
                    (strcmp(SSID->valuestring, "") != 0)) {
                    if ((strcmp(SSID->valuestring, (char*)&s_Para.SSID1_a8) == 0) &&
                        (strcmp((char*)&s_Para.SSID1_a8, "") != 0)) {
                        cJSON *RSSI = cJSON_GetObjectItem(element, "RSSI");
                        if (cJSON_IsNumber(RSSI)) {
                            Final_RSSI = RSSI->valuedouble;
                        }
                        strcpy(Final_SSID, SSID->valuestring);
                        strcpy(Final_Pass, (char*)&s_Para.Pass1_a8);
                    }
                }
            }
        }
        for (int i = 0; i < arraySize; i++) {
            cJSON *element = cJSON_GetArrayItem(In_JSON, i);
            if (element != NULL) {
                cJSON *SSID = cJSON_GetObjectItem(element, "SSID");
                if (cJSON_IsString(SSID) && (SSID->valuestring != NULL) &&
                    (strcmp(SSID->valuestring, "") != 0)) {
                    if ((strcmp(SSID->valuestring, (char*)&s_Para.SSID2_a8) == 0) &&
                        (strcmp((char*)&s_Para.SSID2_a8, "") != 0)) {
                        cJSON *RSSI = cJSON_GetObjectItem(element, "RSSI");
                        if (cJSON_IsNumber(RSSI)) {
                            if (RSSI->valuedouble > Final_RSSI) {
                                strcpy(Final_SSID, SSID->valuestring);
                                strcpy(Final_Pass, (char*)&s_Para.Pass2_a8);
                                Final_RSSI = RSSI->valuedouble;
                            }
                        }
                    }
                }
            }
        }
        for (int i = 0; i < arraySize; i++) {
            cJSON *element = cJSON_GetArrayItem(In_JSON, i);
            if (element != NULL) {
                cJSON *SSID = cJSON_GetObjectItem(element, "SSID");
                if (cJSON_IsString(SSID) && (SSID->valuestring != NULL) &&
                    (strcmp(SSID->valuestring, "") != 0)) {
                    if ((strcmp(SSID->valuestring, (char*)&s_Para.SSID3_a8) == 0) &&
                        (strcmp((char*)&s_Para.SSID3_a8, "") != 0)) {
                        cJSON *RSSI = cJSON_GetObjectItem(element, "RSSI");
                        if (cJSON_IsNumber(RSSI)) {
                            if (RSSI->valuedouble > Final_RSSI) {
                                strcpy(Final_SSID, SSID->valuestring);
                                strcpy(Final_Pass, (char*)&s_Para.Pass3_a8);
                                Final_RSSI = RSSI->valuedouble;
                            }
                        }
                    }
                }
            }
        }
        if (strlen(Final_SSID) != 0) {
            cJSON_AddStringToObject(root, "SSID", (char*)Final_SSID);
            cJSON_AddStringToObject(root, "PASSWORD", (char*)Final_Pass);
            // Set Wi-Fi as the primary interface
            set_wifi_priority(&s_Message_Rx);
        } else {
            cJSON_AddStringToObject(root, "NETWORK", "NOT AVAILABLE");
        }

exit_new:
        cJSON_Delete(In_JSON);
    } // end of if que receive

exit:
    if (root != NULL) {
		memset(payLoadData_NScan,0,sizeof(payLoadData_NScan));
		cJSON_PrintPreallocated(root, payLoadData_NScan, sizeof(payLoadData_NScan), false);
		strcpy((char*)s_Message_Rx.payload_p8, payLoadData_NScan);

        cJSON_Delete(root);
        console_send_responce_to_console_xface(&s_Message_Rx);
    }
  } // end of while(1)
    Network_Scan_Handle = NULL;
    vTaskDelete(Network_Scan_Handle);  // Delete the task
}


static void Wifi_connect_with_SSID(AMessage_st* s_Message_Rx) {
    char SSID[50] = {0}, Pass[50] = {0}, str[200] = {0};
    cJSON *New_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
    if (New_JSON == NULL) {
    	sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
    	Add_Response_msg(str, s_Message_Rx, payLoadData);
        return;
    }

    // Get the value associated with "SSID" key
    cJSON *JSON_SSID = cJSON_GetObjectItemCaseSensitive(New_JSON, "SSID");
    if (cJSON_IsString(JSON_SSID) && (JSON_SSID->valuestring != NULL)) {
        strcpy(SSID, JSON_SSID->valuestring);
    } else {
#ifdef ENABLE_PRINT_MSG
        printf("No valid SSID provided in the JSON payload.\n");
#endif
        cJSON_Delete(New_JSON);
        return;
    }

    // Get the value associated with "PASSWORD" key
    cJSON *JSON_PASS = cJSON_GetObjectItemCaseSensitive(New_JSON, "PASSWORD");
    if (cJSON_IsString(JSON_PASS) && (JSON_PASS->valuestring != NULL)) {
        strcpy(Pass, JSON_PASS->valuestring);
    } else {
#ifdef ENABLE_PRINT_MSG
        printf("No valid password provided in the JSON payload. Assuming open network.\n");
#endif
        strcpy(Pass, ""); // Assuming open network if no password is provided
    }

    // Store the SSID and Password for use in reconnection attempts
    strcpy(stored_ssid, SSID);
    strcpy(stored_pass, Pass);

#ifdef ENABLE_PRINT_MSG
    printf("Initiating connection to SSID: %s with Password: %s\n", SSID, Pass);
#endif

    // Call trigger_next_wifi_attempt to start the connection attempts
//    trigger_next_wifi_attempt(stored_ssid, stored_pass, s_Message_Rx);  // Pass the message structure
    start_wifi_connection_process(stored_ssid, stored_pass,s_Message_Rx);
    cJSON_Delete(New_JSON);
}


static char* log_wifi_disconn_reason(uint16_t reason) {
    switch (reason) {
    case WIFI_REASON_UNSPECIFIED:
        return "Unspecified reason";
        break;
    case WIFI_REASON_AUTH_EXPIRE:
    	return "Authentication expired";
        break;
    case WIFI_REASON_AUTH_LEAVE:
    	return "Authentication leave";
        break;
    case WIFI_REASON_ASSOC_EXPIRE:
    	return "Association expired";
        break;
    case WIFI_REASON_ASSOC_TOOMANY:
    	return "Too many associations";
        break;
    case WIFI_REASON_NOT_AUTHED:
    	return "Not authenticated. Value";
        break;
    case WIFI_REASON_NOT_ASSOCED:
    	return "Not associated. Value";
        break;
    case WIFI_REASON_ASSOC_LEAVE:
    	return "Association leave";
        break;
    case WIFI_REASON_ASSOC_NOT_AUTHED:
    	return "Association but not authenticated";
        break;
    case WIFI_REASON_DISASSOC_PWRCAP_BAD:
    	return "Disassociated due to bad power capability";
        break;
    case WIFI_REASON_DISASSOC_SUPCHAN_BAD:
    	return "Disassociated due to unsupported channel";
        break;
    case WIFI_REASON_BSS_TRANSITION_DISASSOC:
    	return "BSS transition disassociation";
        break;
    case WIFI_REASON_IE_INVALID:
    	return "Invalid IE";
        break;
    case WIFI_REASON_MIC_FAILURE:
    	return "MIC failure";
        break;
    case WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT:
    	return "4-way handshake timeout";
        break;
    case WIFI_REASON_GROUP_KEY_UPDATE_TIMEOUT:
    	return "Group key update timeout";
        break;
    case WIFI_REASON_IE_IN_4WAY_DIFFERS:
    	return "IE in 4-way differs";
        break;
    case WIFI_REASON_GROUP_CIPHER_INVALID:
    	return "Group cipher invalid";
        break;
    case WIFI_REASON_PAIRWISE_CIPHER_INVALID:
    	return "Pairwise cipher invalid";
        break;
    case WIFI_REASON_AKMP_INVALID:
    	return "AKMP invalid";
        break;
    case WIFI_REASON_UNSUPP_RSN_IE_VERSION:
    	return "Unsupported RSN IE version";
        break;
    case WIFI_REASON_INVALID_RSN_IE_CAP:
    	return "Invalid RSN IE capability";
        break;
    case WIFI_REASON_802_1X_AUTH_FAILED:
    	return "802.1X authentication failed";
        break;
    case WIFI_REASON_CIPHER_SUITE_REJECTED:
    	return "Cipher suite rejected";
        break;
    case WIFI_REASON_TDLS_PEER_UNREACHABLE:
    	return "TDLS peer unreachable";
        break;
    case WIFI_REASON_TDLS_UNSPECIFIED:
    	return "TDLS unspecified";
        break;
    case WIFI_REASON_SSP_REQUESTED_DISASSOC:
    	return "SSP requested disassociation";
        break;
    case WIFI_REASON_NO_SSP_ROAMING_AGREEMENT:
    	return "No SSP roaming agreement";
        break;
    case WIFI_REASON_BAD_CIPHER_OR_AKM:
    	return "Bad cipher or AKM";
        break;
    case WIFI_REASON_NOT_AUTHORIZED_THIS_LOCATION:
    	return "Not authorized at this location";
        break;
    case WIFI_REASON_SERVICE_CHANGE_PERCLUDES_TS:
    	return "Service change precludes TS";
        break;
    case WIFI_REASON_UNSPECIFIED_QOS:
    	return "Unspecified QoS";
        break;
    case WIFI_REASON_NOT_ENOUGH_BANDWIDTH:
    	return "Not enough bandwidth";
        break;
    case WIFI_REASON_MISSING_ACKS:
    	return "Missing ACKs";
        break;
    case WIFI_REASON_EXCEEDED_TXOP:
    	return "Disconnected: Exceeded TXOP";
        break;
    case WIFI_REASON_STA_LEAVING:
    	return "STA leaving";
        break;
    case WIFI_REASON_END_BA:
    	return "End BA";
        break;
    case WIFI_REASON_UNKNOWN_BA:
    	return "Unknown BA";
        break;
    case WIFI_REASON_TIMEOUT:
    	return "Timeout";
        break;
    case WIFI_REASON_PEER_INITIATED:
    	return "Peer initiated";
        break;
    case WIFI_REASON_AP_INITIATED:
    	return "AP initiated";
        break;
    case WIFI_REASON_INVALID_FT_ACTION_FRAME_COUNT:
    	return "Invalid FT action frame count";
        break;
    case WIFI_REASON_INVALID_PMKID:
    	return "Invalid PMKID";
        break;
    case WIFI_REASON_INVALID_MDE:
    	return "Invalid MDE";
        break;
    case WIFI_REASON_INVALID_FTE:
    	return "Invalid FTE";
        break;
    case WIFI_REASON_TRANSMISSION_LINK_ESTABLISH_FAILED:
    	return "Transmission link establishment failed";
        break;
    case WIFI_REASON_ALTERATIVE_CHANNEL_OCCUPIED:
    	return "Alternative channel occupied";
        break;
    case WIFI_REASON_BEACON_TIMEOUT:
    	return "Beacon timeout";
        break;
    case WIFI_REASON_NO_AP_FOUND:
    	return "No AP found";
        break;
    case WIFI_REASON_AUTH_FAIL:
    	return "Authentication failed";
        break;
    case WIFI_REASON_ASSOC_FAIL:
    	return "Association failed";
        break;
    case WIFI_REASON_HANDSHAKE_TIMEOUT:
    	return "Handshake timeout";
        break;
    case WIFI_REASON_CONNECTION_FAIL:
    	return "Connection failed";
        break;
    case WIFI_REASON_AP_TSF_RESET:
    	return "AP TSF reset";
        break;
    case WIFI_REASON_ROAMING:
    	return "Roaming";
        break;
    case WIFI_REASON_ASSOC_COMEBACK_TIME_TOO_LONG:
    	return "Association comeback time too long";
        break;
    case WIFI_REASON_SA_QUERY_TIMEOUT:
    	return "SA query timeout";
        break;

    case ESP_ERR_WIFI_NOT_INIT		:    return "WiFi driver was not installed by esp_wifi_init";		break;
    case ESP_ERR_WIFI_NOT_STARTED 	:    return "WiFi driver was not started by esp_wifi_start";		break;
    case ESP_ERR_WIFI_NOT_STOPPED  	:    return "WiFi driver was not stopped by esp_wifi_stop";			break;
    case ESP_ERR_WIFI_IF           	:    return "WiFi interface error";									break;
    case ESP_ERR_WIFI_MODE 			:    return "WiFi mode error";										break;
    case ESP_ERR_WIFI_STATE         :    return "WiFi internal state error";							break;
    case ESP_ERR_WIFI_CONN          :    return "WiFi internal control block of station or soft-AP error";		break;
    case ESP_ERR_WIFI_NVS           :    return "WiFi internal NVS module error";						break;
    case ESP_ERR_WIFI_MAC           :    return "MAC address is invalid";								break;
    case ESP_ERR_WIFI_SSID          :    return "SSID is invalid";										break;
    case ESP_ERR_WIFI_PASSWORD      :    return "Password is invalid";									break;
    case ESP_ERR_WIFI_TIMEOUT       :    return "Timeout error";										break;
    case ESP_ERR_WIFI_WAKE_FAIL     :    return "WiFi is in sleep state(RF closed) and wakeup fail";	break;
    case ESP_ERR_WIFI_WOULD_BLOCK   :    return "The caller would block";								break;
    case ESP_ERR_WIFI_NOT_CONNECT   :    return "Station still in disconnect status";					break;

    case ESP_ERR_WIFI_POST          :    return "Failed to post the event to WiFi task";				break;
    case ESP_ERR_WIFI_INIT_STATE    :    return "Invalid WiFi state when init/deinit is called ";		break;
    case ESP_ERR_WIFI_STOP_STATE    :    return "Returned when WiFi is stopping";						break;
    case ESP_ERR_WIFI_NOT_ASSOC     :    return "The WiFi connection is not associated";				break;
    case ESP_ERR_WIFI_TX_DISALLOW   :    return "The WiFi TX is disallowed";							break;

    case ESP_ERR_WIFI_TWT_FULL         : return "no available flow id";									break;
    case ESP_ERR_WIFI_TWT_SETUP_TIMEOUT: return "Timeout of receiving twt setup response frame, timeout times can be set during twt setup";		break;
    case ESP_ERR_WIFI_TWT_SETUP_TXFAIL : return "TWT setup frame tx failed";							break;
    case ESP_ERR_WIFI_TWT_SETUP_REJECT : return "The twt setup request was rejected by the AP";			break;
    case ESP_ERR_WIFI_DISCARD          : return "Discard frame";										break;

    default:
    	return "Unknown reason";
        break;
    }
}

static void Deinit_Actor(AMessage_st* s_Message_Rx)
{

	if(FirstEntry_bool)
	{
		FirstEntry_bool = false;

		esp_err_t ret = -1;
		if(Task_Handle != NULL)
		{
			vTaskDelete(Task_Handle);
			Task_Handle = NULL;
		}

		// Stop Wi-Fi if it's running
		ret = esp_wifi_stop();
		if (ret != ESP_OK) {
			Add_Response_msg("WIFI Failed to stop ", s_Message_Rx, payLoadData);
		   // return;
		}

		// Deinitialize Wi-Fi
		ret = esp_wifi_deinit();
		if (ret != ESP_OK) {
			Add_Response_msg("Failed to deinitialize Wifi", s_Message_Rx, payLoadData);
		   // return;
		}
	}
	else
		Add_Response_msg("WIFI actor is already de-initialized.",s_Message_Rx, payLoadData);
}

static void Wifi_Scan_Init()
{
//	taskENTER_CRITICAL(); // Prevent race condition

	if(WifiScanHandle == NULL)
	{
		WifiScanHandle = xTaskCreateStaticPinnedToCore(wifi_scan_task, (const char*) "WIFI_SCAN_TASK",WIFI_SCANTASK_STACK_DEPTH, NULL, WIFI_SCANTASK_PRIORITY, xScanTaskStack, &xWIFISCANTaskBuffer,1); // add Rx_buffer as a parameter
	}
	else
	{
		#ifdef ENABLE_PRINT_MSG
			printf("WIFI Scan Task already created\n");
		#endif
	}
//	taskEXIT_CRITICAL();
}
static void wifi_scan_task(void *pvParameters __attribute__((unused))) {
	AMessage_st s_Message_Rx_data;
	strcpy((char*)s_Message_Rx_data.dest_Actor_a8, "WIFI");
	strcpy((char*)s_Message_Rx_data.src_Actor_a8, "CONSOLE");
	strcpy((char*)s_Message_Rx_data.cmdFun_a8, "EVENT");
	s_Message_Rx_data.payload_p8 = (uint8_t*)WIFI_NW_buffer;
	while(1)
	{
		if (!wifi_connected)
		{
			wifi_scan();
			if((char*)g_ScanListArray != NULL)
			{
				if(strlen((char*)g_ScanListArray) != 0)
				{
					cJSON *root = cJSON_Parse((char*)g_ScanListArray);
					memset(payLoadData_WIFI_NW,0,sizeof(payLoadData_WIFI_NW));
					cJSON_PrintPreallocated(root, payLoadData_WIFI_NW, sizeof(payLoadData_WIFI_NW), false);
					strcpy((char*)s_Message_Rx_data.payload_p8, payLoadData_WIFI_NW);
					cJSON_Delete(root);
					console_send_responce_to_console_xface(&s_Message_Rx_data);
				}
			}
		}
		vTaskDelay((s_Para.SCAN_DELAY_u32*1000)/ portTICK_PERIOD_MS); // Scan every minute
	}
}

///*
// Function to check if an SSID is unique in the current request

bool isSSIDUniqueInRequest(const char* ssid, char uniqueSSIDs[][100], int uniqueSSIDCount) {
    for (int i = 0; i < uniqueSSIDCount; i++) {
        if (strcmp(uniqueSSIDs[i], ssid) == 0) {
            return false;  // SSID is not unique in this request
        }
    }
    return true;  // SSID is unique in this request
}

// Function to add an SSID to the current request's unique SSID list

// Function to add an SSID to the current request's unique SSID list
void addSSIDToRequest(const char* ssid, char uniqueSSIDs[][100], int* uniqueSSIDCount) {
    strncpy(uniqueSSIDs[*uniqueSSIDCount], ssid, 100);  // Copy SSID to the local unique list
    (*uniqueSSIDCount)++;  // Increment the count
}

//*/
// /*
static void WIFI_Scan_Return (AMessage_st* s_Message_Rx)
{
	if (!s_Message_Rx || !s_Message_Rx->payload_p8) return;

    // Declare JSON objects for parsing and building JSON structures
    cJSON *In_JSON = NULL;
    cJSON *RAM_SSID = NULL;
    cJSON *root = NULL;
    cJSON *root_unique = NULL;
    // Buffer to hold error or debug messages
    char str[500] = {0};
    uint8_t temp_element = 0;

    // Parse the incoming message payload into JSON
    In_JSON = cJSON_Parse((char*)s_Message_Rx->payload_p8);
    // Check if parsing the incoming JSON failed

    if (In_JSON == NULL) {

        // Log an error and return early
        sprintf(str,"Invalid Json input received in WIFI scan return method: %s", cJSON_GetErrorPtr());
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        return;
    }

    // Check if the global scan list array is empty
    if (g_ScanListArray == NULL) {
        // Log an error indicating the scan list is empty
        Add_Response_msg("Error! WIFI network list is empty", s_Message_Rx, payLoadData);
        return;
    }

    // Parse the scan list stored in the global variable `g_ScanListArray`
    RAM_SSID = cJSON_Parse((char*)g_ScanListArray);
    // Check if parsing the scan list JSON failed
    if (RAM_SSID == NULL) {
        // Log an error and free the input JSON before returning
        sprintf(str, "Invalid Json input received in WIFI scan return method: %s", cJSON_GetErrorPtr());
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        cJSON_Delete(In_JSON);
        return;
    }

    // Get the array of available WiFi networks from the parsed scan list
    cJSON *RAM_Array = cJSON_GetObjectItemCaseSensitive(RAM_SSID, "WIFI_NETWORKS");
    // Create a new array to store the response data
    root_unique = cJSON_CreateArray();
    // Check if creating the array failed
    if (root_unique == NULL) {
        // Log an error and clean up resources
       sprintf(str, "Error creating JSON array\n");
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        cJSON_Delete(In_JSON);
        cJSON_Delete(RAM_SSID);
        return;
    }

    root = cJSON_CreateArray();
    // Check if creating the array failed
    if (root == NULL) {
        // Log an error and clean up resources
       sprintf(str, "Error creating JSON array\n");
        Add_Response_msg(str, s_Message_Rx, payLoadData);
        cJSON_Delete(In_JSON);
        cJSON_Delete(RAM_SSID);
        return;
    }

    // Local array to track unique SSIDs within this specific request

    char uniqueSSIDs[30][100];  // Can store up to 200 SSIDs with a max length of 100 characters each

//    memset(uniqueSSIDs, 0 , sizeof(uniqueSSIDs));
    static int uniqueSSIDCount = 0;     // Counter to track unique SSIDs within the request

    // Retrieve the "INDEX" object from the incoming JSON request

    cJSON *INDEX_JSON = cJSON_GetObjectItem(In_JSON, "INDEX");

    // Check if the INDEX array is present and valid

    if ((INDEX_JSON != NULL) && (cJSON_IsArray(INDEX_JSON))) {

        // Get the size of the INDEX array

        int arraySize = cJSON_GetArraySize(INDEX_JSON);

        // Iterate over each index in the array

        for (int i = 0; i < arraySize; i++) {

            // Get the current index from the INDEX array

            cJSON *index = cJSON_GetArrayItem(INDEX_JSON, i);

            // Check if the current index is a valid number

            if (cJSON_IsNumber(index)) {

                // If the index is -1, it indicates that the whole list should be sent

                if (index->valueint == -1) {
                	memset(uniqueSSIDs, 0 , sizeof(uniqueSSIDs));
                	uniqueSSIDCount = 0;

                    // Iterate through all the networks in the scan list

                    for (int j = 0; j < cJSON_GetArraySize(RAM_Array); j++) {

                        // Get the network at index `j`

                        cJSON *item = cJSON_GetArrayItem(RAM_Array, j);

                        // Extract the SSID field from the current network item

                        cJSON *SSID = cJSON_GetObjectItem(item, "SSID");

                        // Check if the SSID is unique in this request

                        if (SSID && isSSIDUniqueInRequest(SSID->valuestring, uniqueSSIDs, uniqueSSIDCount)) {

                            // Add the current network item to the response array

                            cJSON_AddItemToArray(root, cJSON_Duplicate(item, 1));

                            addSSIDToRequest(SSID->valuestring, uniqueSSIDs, &uniqueSSIDCount);  // Mark the SSID as added in this request

                        } else {

                            // Log the skipped SSID for debugging

//                            sprintf(str, "Duplicate SSID skipped in this request: %s\n", SSID->valuestring);
//                            Add_Response_msg(str, s_Message_Rx);

                        }

                    }

                    // Build the final response object with the unique SSID networks

                    cJSON *root_new = cJSON_CreateObject();
                    cJSON_AddItemToObject(root_new, "WIFI_NETWORKS", root);
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(root_new, payLoadData, sizeof(payLoadData), false);
					strcpy((char*)s_Message_Rx->payload_p8, payLoadData);


                    // Clean up all allocated JSON objects before returning

                    cJSON_Delete(root_new);
                    cJSON_Delete(In_JSON);
                    cJSON_Delete(RAM_SSID);
                    console_send_responce_to_console_xface(s_Message_Rx);
                    return;

                }
                else
                {
					// Get the network corresponding to the specific index

					cJSON *element = cJSON_GetArrayItem(RAM_Array, index->valueint);
					if(index->valueint == 0)
					{
						memset(uniqueSSIDs, 0 , sizeof(uniqueSSIDs));
						uniqueSSIDCount = 0;
					}

					// If the network exists, process it

                	if (element != NULL)
					{

                		if(temp_element == 0)
						{
							memset(uniqueSSIDs, 0 , sizeof(uniqueSSIDs));
							uniqueSSIDCount = 0;

							// Iterate through all the networks in the scan list

							for (int j = 0; j < cJSON_GetArraySize(RAM_Array); j++) {

								// Get the network at index `j`

								cJSON *item = cJSON_GetArrayItem(RAM_Array, j);

								// Extract the SSID field from the current network item

								cJSON *SSID1 = cJSON_GetObjectItem(item, "SSID");

								// Check if the SSID is unique in this request

								if (SSID1 && isSSIDUniqueInRequest(SSID1->valuestring, uniqueSSIDs, uniqueSSIDCount)) {

									// Add the current network item to the response array

									cJSON_AddItemToArray(root_unique, cJSON_Duplicate(item, 1));


									addSSIDToRequest(SSID1->valuestring, uniqueSSIDs, &uniqueSSIDCount);  // Mark the SSID as added in this request

								} else {

									// Log the skipped SSID for debugging

		//                            sprintf(str, "Duplicate SSID skipped in this request: %s\n", SSID->valuestring);
		//                            Add_Response_msg(str, s_Message_Rx);

								}

							}
							temp_element = 1;
//							memset(payLoadData,0,sizeof(payLoadData));
//							cJSON_PrintPreallocated(root_unique, payLoadData, sizeof(payLoadData), false);
//							printf("payLoadData = %s \n", payLoadData);
						}

						// Create a new object to hold the network information
//                		printf("uniqueSSIDCount = %d, index->valueint = %d \n", uniqueSSIDCount, index->valueint);

                		if(index->valueint < uniqueSSIDCount)
                		{
							cJSON *apObject = cJSON_CreateObject();

							// Extract the relevant fields: SSID, RSSI, Security, and AP_MAC

							cJSON *element1 = cJSON_GetArrayItem(root_unique, index->valueint);

							cJSON *SSID = cJSON_GetObjectItem(element1, "SSID");

							cJSON *RSSI = cJSON_GetObjectItem(element1, "RSSI");

							cJSON *Security = cJSON_GetObjectItem(element1, "Security");

							cJSON *AP_MAC = cJSON_GetObjectItem(element1, "AP_MAC");

							// Check if the SSID is unique in this request

	//						if (SSID && isSSIDUniqueInRequest(SSID->valuestring, uniqueSSIDs, uniqueSSIDCount))
							{

//								printf("SSID->valuestring = %s\n", SSID->valuestring);
								// Add SSID, RSSI, Security, and AP_MAC to the response object

								cJSON_AddStringToObject(apObject, "SSID", SSID->valuestring);

								cJSON_AddNumberToObject(apObject, "RSSI", RSSI->valueint);

								cJSON_AddStringToObject(apObject, "Security", Security->valuestring);

								cJSON_AddStringToObject(apObject, "AP_MAC", AP_MAC->valuestring);

								cJSON_AddItemToArray(root, apObject);
							}
                		}
					}
                }
            }

        }

        // Create the final JSON object for the response

        cJSON *root_new = cJSON_CreateObject();

        cJSON_AddItemToObject(root_new, "WIFI_NETWORKS", root);
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(root_new, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);


        // Clean up all allocated JSON objects before returning

        cJSON_Delete(root_new);
        cJSON_Delete(In_JSON);
        cJSON_Delete(RAM_SSID);

        if(root_unique != NULL)
        {
        	cJSON_Delete(root_unique);
        	root_unique = NULL;
        }

        console_send_responce_to_console_xface(s_Message_Rx);
        return;

    }
    else
    {
        Add_Response_msg("Kindly enter correct payload.", s_Message_Rx, payLoadData);
    }

    // Clean up allocated resources before returning

    cJSON_Delete(root);
    cJSON_Delete(In_JSON);
    cJSON_Delete(RAM_SSID);
    if(root_unique != NULL)
    {
    	cJSON_Delete(root_unique);
    	root_unique = NULL;
    }
}

static void Get_Connectivity_Info_For_update_Twin(AMessage_st* s_Message_Rx)
{
	esp_err_t err;
	char str[100]={0};
	err = esp_wifi_sta_get_ap_info(&app_info);
	sprintf(str, "%02x:%02x:%02x:%02x:%02x:%02x",app_info.bssid[0],app_info.bssid[1],app_info.bssid[2],app_info.bssid[3],
					app_info.bssid[4],app_info.bssid[5]);
	cJSON *connectivity_info = cJSON_CreateObject();
	cJSON *responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "SSID", (char*)app_info.ssid);
	cJSON_AddStringToObject(responseObject, "PASSWORD",(char*)s_Para.Connected_pass);
	cJSON_AddNumberToObject(responseObject, "RSSI", app_info.rssi);
	cJSON_AddStringToObject(responseObject, "WIFI_GATEWAY_MAC_ADDRESS", str);  //ACCESS_POINT_MAC
	cJSON_AddStringToObject(responseObject, "WIFI_MAC_ADDRESS", (char*)s_Para.MAC_ID_a8);
	cJSON_AddStringToObject(responseObject, "GATEWAY_IP_ADDRESS", (char*)s_Para.STA_GW_ADDR_a8);
	cJSON_AddStringToObject(responseObject, "DEVICE_IP_ADDRESS", (char*)s_Para.STA_IP_ADDR_a8);
	cJSON_AddStringToObject(responseObject, "IP_ADDRESS_UPDATED_DATETIME", (char*)s_Para.STA_IP_UPDATED_DATETIME_a8);
	cJSON_AddStringToObject(responseObject, "ETH_GATEWAY_MAC_ADDRESS", "");
	cJSON_AddItemToObject(connectivity_info, "CONNECTIVITY_INFO", responseObject);

	if(err == ESP_OK)
	{
		memset(payLoadData,0,sizeof(payLoadData));
		cJSON_PrintPreallocated(connectivity_info, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(connectivity_info);
		console_send_responce_to_console_xface(s_Message_Rx);
	}
	else
	{
		if(err == ESP_ERR_WIFI_CONN)
		{

			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "ERROR","The station interface don't initialized");
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}

		if(err == ESP_ERR_WIFI_NOT_CONNECT)
		{
			cJSON *responseObject = cJSON_CreateObject();
			cJSON_AddStringToObject(responseObject, "ERROR","The station interface don't initialized");
			memset(payLoadData,0,sizeof(payLoadData));
			cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
			strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
			cJSON_Delete(responseObject);
			console_send_responce_to_console_xface(s_Message_Rx);
		}
	}
}

int should_skip_ssid(const char *ssid) {

    // Check for required prefix "Haven "

    if (strncmp(ssid, PREFIX, PREFIX_LEN) != 0) {

        return 0; // Doesn't start with "Haven ", so don't skip

    }

    // Check that there's a single letter for the product category and a hyphen

    if (!isalpha((int)ssid[(int)CATEGORY_POS]) || ssid[(int)HYPHEN_POS] != '-') {

        return 0; // Invalid format for product category, so don't skip

    }

    // Locate the colon and validate the product series length (3-4 characters) before it

    const char *colon_pos = strchr(ssid, ':');

    int series_length = (colon_pos ? colon_pos - (ssid + HYPHEN_POS + 1) : 0); // Product series length

    if (!colon_pos || series_length < MIN_SERIES_LEN || series_length > MAX_SERIES_LEN) {

        return 0; // No colon or invalid series length, so don't skip

    }

    // Trim spaces and validate the MAC suffix (4 hex characters)

    const char *mac_suffix = colon_pos + 1;

    while (*mac_suffix == ' ') mac_suffix++; // Skip leading spaces

    if (strlen(mac_suffix) != MAC_SUFFIX_LEN) return 0; // Check MAC length

    for (int i = 0; i < MAC_SUFFIX_LEN; i++) {

        if (!isxdigit((int)mac_suffix[i])) return 0; // Ensure each character is hex

    }
    return 1; // Matches the pattern, so should skip
}

//
//------------------------ WIFI Actor Methods Ends ------------------------------------------------------------------------//
