/*
 * ETH_Actor.c
 *
 *  Created on: Sep 07, 2022
 *  Author: Suraj
 */

/* Ethernet (ETH Controller) basic example
 */

#include "actor.h"
#include "Console_Actor.h"
#include "ETH_Actor.h"
#include "Config.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_netif.h"
#include "esp_netif_net_stack.h"
#include "esp_netif_ip_addr.h"
#include "esp_netif_types.h"
#include "esp_eth.h"
#include "esp_eth_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "ping/ping.h"
#include "lwip/etharp.h"
#include "lwip/netif.h"

//----------------------------- Actor Tags ---------------------------------------//

static const char * ACTOR_TAG  = ">[ETH]:\t";
static const char * THIS_ACTOR = "ETH";
//------------------------- ETH Actor Resources -----------------------------//

#define DEBUG_ON  0

#if DEBUG_ON
#define EXAMPLE_DEBUG ESP_LOGI
#endif

#define IP_STR_LEN       	20
#define PING_URL_STR_LEN 	50

#define DHCP_DISABLED 0
#define DHCP_ENABLED  1

#define RX_QUE_COUNT  10

typedef struct {
    uint8_t spi_cs_gpio;
    uint8_t int_gpio;
    int8_t  phy_reset_gpio;
    uint8_t phy_addr;
}spi_eth_module_config_t;
esp_netif_t* get_ethernet_netif_handle() ;
static void Ethernet_Init();
static void Ethernet_Connect(AMessage_st* s_Message_Rx);
static void Ethernet_DHCP_Refresh(AMessage_st* s_Message_Rx);
static void pingServer(AMessage_st* s_Message_Rx);
static void update_ethernet_status(eth_et eth_status);
static void sw_reset_w5500(AMessage_st* s_Message_Rx);
static void reset_property_table();

static esp_eth_handle_t eth_handle_spi 	=  NULL;
static esp_netif_t *eth_netif_spi 		=  NULL;
static spi_device_handle_t spi_handle   =  NULL;
static esp_eth_mac_t *mac_spi = NULL;
static esp_eth_phy_t  *phy_spi = NULL; //esp_eth_phy_t *phy_spi = NULL; //esp_eth_mac_t *phy_spi = NULL;

//-------------------------- Common Actor Resources ------------------------------//

static bool 			FirstEntry_bool 	= false;
static bool 			FirstEntry_ETH_bool 	= false;
static TaskHandle_t  	Task_Handle		= NULL; 	// BLE Task Handler
static QueueHandle_t 	msg_Rx_Queue	= NULL; 	// BLE Rx Queue
static StaticTask_t xETHTaskBuffer;  //// Declare a static task control block
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [RX_QUE_COUNT * sizeof(AMessage_st)];
static StaticQueue_t Monitor_pxQueueBuffer;
PSRAM_ATTR_BSS static StackType_t xTaskStack[ETH_TASK_STACK_DEPTH];

static void init(void *a, void *b); 							// Init actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					// Set or Change a parameter
static void get(char *prop, char *val_a8);						// Get or Read a Parameter
static void Get_Property(AMessage_st* s_Message_Rx);
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Get_Connectivity_Info_For_update_Twin(AMessage_st* s_Message_Rx);
static void update_ip_change_datetime(char *dest, size_t dest_len);
//-------------------------- Actor Parameters ------------------------------//

PSRAM_ATTR_BSS static struct actor_parameter {
	uint8_t status_u8;
	int    DHCP;
	char   IP[IP_STR_LEN];
	char   SM[IP_STR_LEN];
	char   GW[IP_STR_LEN];
	char   IP_UPDATED_DATETIME[32];
	char DNS1[IP_STR_LEN];
	char DNS2[IP_STR_LEN];
	char pingServer[PING_URL_STR_LEN];
	uint8_t MAC_ID_a8[18];
	uint8_t ETH_GATEWAY_MAC_ADDR_a8[18];
} s_Para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &s_Para.status_u8   ,   "STATUS"     ,   INT,    "R", "Status of the actor" },
    { &s_Para.DHCP        ,   "DHCP"      ,   INT,    "RW", "Status of DHCP" },
    { &s_Para.IP          ,   "IP"        ,   STRING, "RW", "IP address" },
    { &s_Para.SM          ,   "SM"        ,   STRING, "RW", "Subnet mask" },
    { &s_Para.GW          ,   "GW"        ,   STRING, "RW", "Gateway address" },
    { &s_Para.IP_UPDATED_DATETIME, "IP_UPDATED_DATETIME", STRING, "R", "UTC dateTime when the Ethernet IP address was assigned or changed" },
    { &s_Para.DNS1        ,   "DNS1"      ,   STRING, "RW", "Primary DNS server" },
    { &s_Para.DNS2        ,   "DNS2"      ,   STRING, "RW", "Secondary DNS server" },
    { &s_Para.pingServer  ,   "PINGS1"    ,   STRING, "RW", "Ping server address" },
	{ &s_Para.MAC_ID_a8	  ,  "MAC_ADD"	,	  STRING,  "R", "MAC address of the Ethernet"},
	{ &s_Para.ETH_GATEWAY_MAC_ADDR_a8, "ETH_GATEWAY_MAC_ADDR", STRING, "R", "MAC address of the Ethernet gateway"}
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_EVT[1024];
PSRAM_ATTR_BSS static char Rx_buffer[8192];

//-------------------------- Common Actor Methods ------------------------------//

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
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
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
		Add_Response_msg("'Property_Names' array is NULL.",s_Message_Rx);
	}
	cJSON_Delete(out_JSON);
	cJSON_Delete(in_JSON);
}

static void update_ip_change_datetime(char *dest, size_t dest_len)
{
	time_t now;
	struct tm timeinfo = {0};

	time(&now);
	gmtime_r(&now, &timeinfo);
	strftime(dest, dest_len, "%Y-%m-%dT%H:%M:%S.000Z", &timeinfo);
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

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n ETH s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
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
}

static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) {
	uint8_t parameter_found = 0; // Flag to check if actor is found
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
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_ETH_ZERO_STATE:
					strcpy(val_a8, "ETHERNET IDLE STATE");
					break;

				case E_ETH_NOT_CONNECTED:
					strcpy(val_a8, "ETHERNET NOT CONNECTED");
					break;

				case E_ETH_CONNECTED:
					strcpy(val_a8, "ETHERNET CONNECTED");
					break;

				case E_ETH_STOPPED:
					strcpy(val_a8, "ETHERNET STOPPED");
					break;

				default:
					break;
				}
			}
		}
	}
}//	get


static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[PING_URL_STR_LEN] = {0};
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
			if(!(strcmp(prop[i].str_name, "STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_ETH_ZERO_STATE:
					strcpy(val_a8, "ETHERNET IDLE STATE");
					break;

				case E_ETH_NOT_CONNECTED:
					strcpy(val_a8, "ETHERNET NOT CONNECTED");
					break;

				case E_ETH_CONNECTED:
					strcpy(val_a8, "ETHERNET CONNECTED");
					break;

				case E_ETH_STOPPED:
					strcpy(val_a8, "ETHERNET STOPPED");
					break;

				default:
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

		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties


static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the SQL actor.");
	cJSON_AddStringToObject(responseObject, "SET(string IP)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "CONNECT()", "Connect to Ethernet.");
	cJSON_AddStringToObject(responseObject, "PING(string IP_ADDRESS, INT TIMEOUT)", "Ping to the server.");
	cJSON_AddStringToObject(responseObject, "REFRESH()", "Refresh the Ethernet.");
	cJSON_AddStringToObject(responseObject, "RESET()", "Reset the Ethernet.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "DEINIT()", "Deinit the actor. It delete the monitor task and its queue.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help


static void init(void *a, void *b)
{
	if(FirstEntry_bool)
	{
		return;
	}
	if (msg_Rx_Queue == NULL)
	{
		msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
	}
	if (msg_Rx_Queue == NULL) {
#ifdef ENABLE_PRINT_MSG
		printf("HTTP RX Queue is not created. \n");
#endif
	}

	reset_property_table();
	Task_Handle = xTaskCreateStaticPinnedToCore(
					monitor,                 // Task function
					"ETH Monitor",            // Task name
					ETH_TASK_STACK_DEPTH,        // Stack size in words
					NULL,                    // Task parameters (not used here)
					ETH_TASK_PRIORITY,                       // Task priority
					xTaskStack,              // Pointer to task stack (allocated in PSRAM)
					&xETHTaskBuffer,             // Pointer to task control block
					1
   );

	if (Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			    printf("Failed to create task\n");
#endif
			    // Handle error
	}
	FirstEntry_bool = true;
	update_ethernet_status(E_ETH_ZERO_STATE);
	if(FirstEntry_ETH_bool == false)
	{
		Ethernet_Init();
		FirstEntry_ETH_bool = (eth_handle_spi != NULL);
	}
}// init

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON = NULL;
	cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY)) {

//			printf("ETH msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("ETH DT = %s\n",s_Message_Rx->payload_p8);
//			}

			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;

			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				if (FirstEntry_bool == 0){
				init(0,0);
				}
				else if(FirstEntry_bool==1){
				 Add_Response_msg("ETH Initialization Done.",s_Message_Rx);
			  }
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
					Add_Response_msg(str,s_Message_Rx);
				    }
				else{
					head_JSON = name_JSON->child;
				   // Loop through each key-value pair
				    do {
				    	// Check if the value string is not NULL
				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
						{
							// Set the key-value pair
							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
							if(u8Result==1)
							{
//							cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
							}
							else if(u8Result==2){
								sprintf(str,"'%s' is a read only property", head_JSON->string);
								 Add_Response_msg(str, s_Message_Rx);
							}
							else{
//							cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
							}
				        } else {
				            // Handle the case where value string is NULL (e.g., log an error or take appropriate action)
				            sprintf(str, "Invalid parameter '%s'", head_JSON->string);
				            Add_Response_msg(str,s_Message_Rx);
				            // Handle the error as per your application's requirements
				        }
				        head_JSON = head_JSON->next;
				    } while (head_JSON != 0);

					    if(u8Result==1){
					  	//  save parameters to JFS
					    //	printf("\n root_JSON type = %d, root_JSON->child->type = %d, root_JSON = %s", root_JSON->type, root_JSON->child->type, cJSON_Print(root_JSON));
//					    if((cJSON_IsObject(root_JSON))  && (cJSON_IsString(root_JSON->child)))
//					    {
//							memset(payLoadData,0,sizeof(payLoadData));
//							cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);
//							Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
//					    }
//					   	cJSON_Delete(root_JSON);
					    console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
					    }
				    // Free the parsed JSON
				    cJSON_Delete(name_JSON);
				}
				//cJSON_Delete(head_JSON);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "CONNECT")) { // ETH Connect to LAN
				//eth.method2();		//Ethernet_Connect();
				Ethernet_Connect(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "PING")) { // PIN a Server
				pingServer(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "REFRESH")) { // PIN a Server
				Ethernet_DHCP_Refresh(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESET")) { // PIN a Server
				sw_reset_w5500(s_Message_Rx);
				//console_send_responce_to_console_xface("Method RESET Called.");
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET_CONN_INFO"))
			{
				Get_Connectivity_Info_For_update_Twin(s_Message_Rx);
			}
			else
			{
				//ETH error message: invalid method
				  Add_Response_msg("Invalid method", s_Message_Rx);
			}
		}
	}
}  //	monitor


static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {

	uint8_t out_val[128]  	= {0}; //(uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);


	cJSON 	*my_JSON  	= cJSON_CreateObject();
	AMessage_st 		s_Message_Tx;
	switch(data_type)
	{
		case U_INT8  :	sprintf((char*)out_val,	"%d",	*(uint8_t *) value);	break;
		case U_INT16 :	sprintf((char*)out_val,	"%d",	*(uint16_t *) 	value);	break;
		case INT	 :	sprintf((char*)out_val,	"%d", 	*(int *) 		value);	break;
//		case FLOAT   :  sprintf((char*)out_val,	"%f", 	(float) 	value); break;
		case STRING  :	sprintf((char*)out_val,	"%s",	(char*) 	value);	break;
		default 	 : 	break;
	}
	cJSON_AddStringToObject(my_JSON, parameter, (char*)out_val);
	s_Message_Tx.payload_size = strlen((char*)out_val);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);

	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);

	uint8_t *newpointer = (uint8_t*) heap_caps_calloc(strlen((char*) payLoadData) + 1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*)s_Message_Tx.payload_p8, payLoadData);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor


void ETH_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;

	if (FirstEntry_bool == false)
	{
		init(0,0);
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
			printf("<ETH.ERROR(ETH RX Queue is full)\n");
		}
		else
		{
			printf("<ETH.ERROR(ETH RX Queue send unsuccessful)\n");
		}
	}

}//	ETH_ConsoleWriteToActor

//------------------------- Enter in function ETH Actor Methods -----------------------------//

static void reset_property_table()
{
	s_Para.status_u8 = 0;
	s_Para.DHCP 	= DHCP_ENABLED;
	strcpy(s_Para.IP, 		"0.0.0.0"	);
	strcpy(s_Para.SM, 		"0.0.0.0"	);
	strcpy(s_Para.GW, 		"0.0.0.0"	);
	strcpy(s_Para.IP_UPDATED_DATETIME, "");
	strcpy(s_Para.DNS1, 		"0.0.0.0"		);
	strcpy(s_Para.DNS2, 		"8.8.8.8"		);
	strcpy(s_Para.pingServer, "www.google.com");
	memset(s_Para.ETH_GATEWAY_MAC_ADDR_a8, 0, sizeof(s_Para.ETH_GATEWAY_MAC_ADDR_a8));
	strcpy((char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00");
}

//------------------------- Ethernet Functions -----------------------------//

/** Event handler for IP_EVENT_ETH_GOT_IP */
static void got_ip_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
    const esp_netif_ip_info_t *ip_info = &event->ip_info;
    AMessage_st s_Message_Rx;
    char out[256] = {0}, buffer[300] = {0}, previous_ip[IP_STR_LEN] = {0};

    sprintf(out,""IPSTR"", IP2STR(&ip_info->ip));

    if (strcmp(out, "255.255.255.255") == 0) {
          return;
    }

    update_ethernet_status(E_ETH_CONNECTED);

    cJSON *out_JSON;
  	out_JSON   = cJSON_CreateObject();
 	cJSON_AddStringToObject(out_JSON, "ETH_CONNECTION_STATUS","CONNECTED");
	strncpy(previous_ip, s_Para.IP, sizeof(previous_ip) - 1);
	sprintf(out,""IPSTR"", IP2STR(&ip_info->ip));		strcpy(s_Para.IP,out);
 	cJSON_AddStringToObject(out_JSON, "IP",out);
 	sprintf(out,""IPSTR"", IP2STR(&ip_info->netmask));	strcpy(s_Para.SM,out);
 	cJSON_AddStringToObject(out_JSON, "SM",out);
 	sprintf(out,""IPSTR"", IP2STR(&ip_info->gw));       strcpy(s_Para.GW,out);
 	cJSON_AddStringToObject(out_JSON, "GW",out);
	/* Resolve gateway MAC via ARP cache */
	{
		ip4_addr_t gw_ip4;
		gw_ip4.addr = ip_info->gw.addr;
		struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(eth_netif_spi);
		struct eth_addr *gw_eth_addr = NULL;
		const ip4_addr_t *gw_ip_found = NULL;
		if (lwip_netif != NULL &&
			etharp_find_addr(lwip_netif, &gw_ip4, &gw_eth_addr, &gw_ip_found) >= 0 &&
			gw_eth_addr != NULL)
		{
			sprintf((char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8,
					"%02x:%02x:%02x:%02x:%02x:%02x",
					gw_eth_addr->addr[0], gw_eth_addr->addr[1],
					gw_eth_addr->addr[2], gw_eth_addr->addr[3],
					gw_eth_addr->addr[4], gw_eth_addr->addr[5]);
		}
		else
		{
			strcpy((char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00");
		}
	}
	if ((strcmp(previous_ip, s_Para.IP) != 0) && (strcmp(s_Para.IP, "0.0.0.0") != 0))
	{
		update_ip_change_datetime(s_Para.IP_UPDATED_DATETIME, sizeof(s_Para.IP_UPDATED_DATETIME));
	}

 	//---------------- DNS Server Details -----------------------------//

 	esp_netif_dns_info_t dns_info = {0};

 	// Get DNS Server1 Details
 	esp_netif_get_dns_info(eth_netif_spi, ESP_NETIF_DNS_MAIN, &dns_info);
 	sprintf(out,""IPSTR"", IP2STR(&dns_info.ip.u_addr.ip4));       strcpy(s_Para.DNS1,out);
 	cJSON_AddStringToObject(out_JSON, "DNS1",out);

 	// Get DNS Server2 Details
 	esp_netif_get_dns_info(eth_netif_spi, ESP_NETIF_DNS_BACKUP, &dns_info);
 	sprintf(out,""IPSTR"", IP2STR(&dns_info.ip.u_addr.ip4));
 	strcpy(s_Para.DNS2,out);
 	cJSON_AddStringToObject(out_JSON, "DNS2",out);

	s_Message_Rx.Dest_ID_a8 = ETH;
	s_Message_Rx.Src_ID_u8 = SYSTEM;
	strcpy((char*)s_Message_Rx.cmdFun_a8, "EVENT");
	strcpy((char*)s_Message_Rx.dest_Actor_a8,"ETH");
	strcpy((char*)s_Message_Rx.src_Actor_a8, "CONSOLE");   //SYSTEM
	s_Message_Rx.payload_p8 = (uint8_t*)buffer;

	memset(payLoadData_EVT,0,sizeof(payLoadData_EVT));
	cJSON_PrintPreallocated(out_JSON, payLoadData_EVT, sizeof(payLoadData_EVT), false);
	strcpy((char*)s_Message_Rx.payload_p8, payLoadData_EVT);

	cJSON_Delete(out_JSON);
	console_send_responce_to_console_xface(&s_Message_Rx);
	uint8_t WIFI_Scan_Mode = 1; // Passive
	set_to_other_actor("WIFI",U_INT8, "WIFI_SCAN_MODE", &WIFI_Scan_Mode);  // Set scan mode of wifi to passive
}//	got_ip_event_handler


/** Event handler for Ethernet events */
static void eth_event_handler(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data)
{
    char str1[100] = {0};
	char buf[128] = {0}, buffer[100] = {0};
	AMessage_st s_Message_Tx_new;
	cJSON *root = NULL;
	s_Message_Tx_new.Dest_ID_a8 = ETH;
	s_Message_Tx_new.Src_ID_u8 =SYSTEM;
	 strcpy((char*)s_Message_Tx_new.dest_Actor_a8,"ETH");
	 strcpy((char*)s_Message_Tx_new.src_Actor_a8,"SYSTEM");
	 strcpy((char*)s_Message_Tx_new.cmdFun_a8,"EVENT");
	 s_Message_Tx_new.payload_p8 = (uint8_t*)buffer;
	uint8_t mac_addr[6] = {0};
    /* we can get the ethernet driver handle from event data */
    esp_eth_handle_t eth_handle = *(esp_eth_handle_t *)event_data;
    switch (event_id) {
    case ETHERNET_EVENT_CONNECTED:
    	update_ethernet_status(E_ETH_CONNECTED);
        esp_eth_ioctl(eth_handle, ETH_CMD_G_MAC_ADDR, mac_addr);
    	esp_read_mac(mac_addr, ESP_MAC_ETH);
        sprintf(buf, "Ethernet Link Up \nEthernet HW Addr %02x:%02x:%02x:%02x:%02x:%02x",
                        mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        sprintf((char*)s_Para.MAC_ID_a8, "%02x:%02x:%02x:%02x:%02x:%02x",
                               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
        strcpy(str1, "ETH is connected.");
        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str1, strlen(str1), "SAVE_WIFI_AUDIT_LOG");
        strcpy((char*)s_Message_Tx_new.src_Actor_a8,"CONSOLE");
    	root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "ETH_CONNECTION_STATUS","CONNECTED");
		memset(payLoadData_EVT,0,sizeof(payLoadData_EVT));
		cJSON_PrintPreallocated(root, payLoadData_EVT, sizeof(payLoadData_EVT), false);
		strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_EVT);
		cJSON_Delete(root);
		console_send_responce_to_console_xface(&s_Message_Tx_new);
        break;
    case ETHERNET_EVENT_DISCONNECTED:
    	update_ethernet_status(E_ETH_NOT_CONNECTED);
    	root = cJSON_CreateObject();
		cJSON_AddStringToObject(root, "ETH_CONNECTION_STATUS","DISCONNECTED");
		memset(payLoadData_EVT,0,sizeof(payLoadData_EVT));
		cJSON_PrintPreallocated(root, payLoadData_EVT, sizeof(payLoadData_EVT), false);
		strcpy((char*)s_Message_Tx_new.payload_p8, payLoadData_EVT);
		cJSON_Delete(root);
		console_send_responce_to_console_xface(&s_Message_Tx_new);
        strcpy(str1, "ETH is disconnected.");
        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str1, strlen(str1), "SAVE_WIFI_AUDIT_LOG");
		uint8_t WIFI_Scan_Mode = 0; // Active
		set_to_other_actor("WIFI",U_INT8, "WIFI_SCAN_MODE", &WIFI_Scan_Mode);
        break;
    case ETHERNET_EVENT_START:
        break;
    case ETHERNET_EVENT_STOP:
        break;
    default:
        break;
    }
}//	eth_event_handler


static void Ethernet_DHCP_Refresh(AMessage_st* s_Message_Rx)
{
	esp_err_t err = 0;
	// Check for DHCP parameter
    if(s_Para.DHCP == DHCP_ENABLED)		//	Static IP config start
    {
    	Add_Response_msg("ETH DHCP Refresh Started.",s_Message_Rx);
    	//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "ETH DHCP Refresh Started.");
    	err = esp_netif_dhcpc_stop(eth_netif_spi);		// Stop the DHCP if not running
    	err = esp_netif_dhcpc_start(eth_netif_spi);		// Start the DHCP if not running
    	if(err == ESP_OK){
    		Add_Response_msg("ETH DHCP Refresh Done.",s_Message_Rx);
    	}
    }
    else {
    	Add_Response_msg("ETH DHCP is disabled.",s_Message_Rx);
    }
}//	Ethernet_DHCP_Refresh

static void Ethernet_Connect(AMessage_st* s_Message_Rx)
{
	esp_err_t err = 0;
    if(s_Para.DHCP == DHCP_ENABLED)		//	Static IP config start
    {
    	err = esp_netif_dhcpc_start(eth_netif_spi);
    	if(err == ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED){
//    		Add_Response_msg("ETH DHCP Already Started.",s_Message_Rx);
    	}
    }
    else if(s_Para.DHCP == DHCP_DISABLED)
    {
      	if (eth_netif_spi)
    	{

    		esp_netif_ip_info_t  ip_info;//   = {0};
    		esp_netif_dns_info_t dns1_info;// = {0};
    		esp_netif_dns_info_t dns2_info;// = {0};

    		memset(&ip_info   , 0, sizeof(esp_netif_ip_info_t));
    		memset(&dns1_info , 0, sizeof(esp_netif_dns_info_t));
    		memset(&dns2_info , 0, sizeof(esp_netif_dns_info_t));

    		//ESP_ERROR_CHECK(esp_netif_dhcpc_stop(eth_netif_spi));	 // Stop the DHCP if already running
    		err = esp_netif_dhcpc_stop(eth_netif_spi);
    		ip_info.ip.addr      = esp_ip4addr_aton((const char *)s_Para.IP);
    		ip_info.netmask.addr = esp_ip4addr_aton((const char *)s_Para.SM);
    		ip_info.gw.addr      = esp_ip4addr_aton((const char *)s_Para.GW);

    		dns1_info.ip.u_addr.ip4.addr	= esp_ip4addr_aton((const char *)s_Para.DNS1);
    		dns2_info.ip.u_addr.ip4.addr	= esp_ip4addr_aton((const char *)s_Para.DNS2);

    		// Setup Static IP Info
    		esp_netif_set_ip_info(eth_netif_spi, &ip_info);

    		// Setup DNS IP Info
    		esp_netif_set_dns_info(eth_netif_spi, ESP_NETIF_DNS_MAIN,   &dns1_info);
    		esp_netif_set_dns_info(eth_netif_spi, ESP_NETIF_DNS_BACKUP, &dns2_info);
//    		Add_Response_msg("ETH Started",s_Message_Rx);
    	}
      	else
      		Add_Response_msg("Error! ETH Not Started",s_Message_Rx);
    }//	Static IP config end

}//	Ethernet_Connect

bool esp_event_loop_created = false;

static void Ethernet_Init()
{
	esp_err_t err = ESP_OK;
	// Initialize TCP/IP network interface (should be called only once in application)
    err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    	ESP_LOGE(ACTOR_TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
    	goto eth_init_fail;
    }
    if(!esp_event_loop_created){
    	esp_event_loop_created = true;
    // Create default event loop that running in background
    	err = esp_event_loop_create_default();
    	if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    		ESP_LOGE(ACTOR_TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
    		goto eth_init_fail;
    	}
    }
    // Create instance(s) of esp-netif for SPI Ethernet(s)
    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_ETH();
    esp_netif_config.if_key 	= "ETH_SPI_0";
  	esp_netif_config.if_desc 	= "eth0";
   	esp_netif_config.route_prio = 30;

    esp_netif_config_t cfg_spi = {
        .base  = &esp_netif_config,
        .stack = ESP_NETIF_NETSTACK_DEFAULT_ETH
    };

    eth_netif_spi = esp_netif_new(&cfg_spi);
 //}
    // Init MAC and PHY configs to default
    eth_mac_config_t mac_config_spi = ETH_MAC_DEFAULT_CONFIG();
    eth_phy_config_t phy_config_spi = ETH_PHY_DEFAULT_CONFIG();

    // Install GPIO ISR handler to be able to service SPI Eth modlues interrupts
    gpio_install_isr_service(0);



    // Init SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num   = CONFIG_ETH_SPI_MISO_GPIO,
        .mosi_io_num   = CONFIG_ETH_SPI_MOSI_GPIO,
        .sclk_io_num   = CONFIG_ETH_SPI_SCLK_GPIO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    esp_err_t ret= spi_bus_initialize(CONFIG_ETH_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
#ifdef ENABLE_PRINT_MSG
    printf("\n ret = %d", ret);
#endif
    if(ret != ESP_OK){
#ifdef ENABLE_PRINT_MSG
    	printf("\n Error! in SPI bus init. \n");
#endif
    }
    // Init specific SPI Ethernet module configuration from Kconfig (CS GPIO, Interrupt GPIO, etc.)
    spi_eth_module_config_t spi_eth_module_config = {
    	.spi_cs_gpio	= CONFIG_ETH_SPI_CS0_GPIO,
		.int_gpio    	= CONFIG_ETH_SPI_INT0_GPIO,
		.phy_reset_gpio	= CONFIG_ETH_SPI_PHY_RST0_GPIO,
		.phy_addr 	   	= CONFIG_ETH_SPI_PHY_ADDR0,
    };

// Configure SPI interface and Ethernet driver for W5500 SPI module

    spi_device_interface_config_t devcfg = {
        .command_bits 	= 16, // Actually it's the address phase in W5500 SPI frame
        .address_bits 	= 8,  // Actually it's the control phase in W5500 SPI frame
        .mode         	= 0,
        .clock_speed_hz = CONFIG_ETH_SPI_CLOCK_MHZ * 1000 * 1000,
        .queue_size 	= 20
    };

  	// Set SPI module Chip Select GPIO
  	devcfg.spics_io_num = spi_eth_module_config.spi_cs_gpio;

   	//spi_device_handle_t spi_handle =  NULL;
   	err = spi_bus_add_device(CONFIG_ETH_SPI_HOST, &devcfg, &spi_handle);
   	if (err != ESP_OK) {
   		ESP_LOGE(ACTOR_TAG, "spi_bus_add_device failed: %s", esp_err_to_name(err));
   		goto eth_init_fail;
   	}

//----------------------------	W5500 start here	---------------------------------------------//

// w5500 Ethernet driver is based on spi driver
   	eth_w5500_config_t w5500_config = ETH_W5500_DEFAULT_CONFIG(CONFIG_ETH_SPI_HOST, &devcfg);

// Set remaining GPIO numbers and configuration used by the SPI module
   	w5500_config.int_gpio_num 		= spi_eth_module_config.int_gpio;
 	phy_config_spi.phy_addr 		= spi_eth_module_config.phy_addr;
	phy_config_spi.reset_gpio_num 	= spi_eth_module_config.phy_reset_gpio;

	mac_spi = esp_eth_mac_new_w5500(&w5500_config, &mac_config_spi);
	phy_spi = esp_eth_phy_new_w5500(&phy_config_spi);  //esp_eth_mac_t  = esp_eth_phy_t
	if (mac_spi == NULL || phy_spi == NULL) {
		ESP_LOGE(ACTOR_TAG, "W5500 MAC/PHY create failed (mac=%p phy=%p)", mac_spi, phy_spi);
		goto eth_init_fail;
	}

//----------------------------	W5500 end here	---------------------------------------------//
   	esp_eth_config_t eth_config_spi = ETH_DEFAULT_CONFIG(mac_spi, phy_spi);
  	err = esp_eth_driver_install(&eth_config_spi, &eth_handle_spi);
  	if (err != ESP_OK) {
  		ESP_LOGE(ACTOR_TAG, "esp_eth_driver_install failed: %s", esp_err_to_name(err));
  		goto eth_init_fail;
  	}

  	/* The SPI Ethernet module might not have a burned factory MAC address, we cat to set it manually.
 	02:00:00 is a Locally Administered OUI range so should not be used except when testing on a LAN under your control.
  	*/
  	uint8_t mac[6];
	esp_read_mac(mac, ESP_MAC_ETH);
	err = esp_eth_ioctl(eth_handle_spi, ETH_CMD_S_MAC_ADDR, mac);
	if (err != ESP_OK) {
		ESP_LOGE(ACTOR_TAG, "esp_eth_ioctl(SET_MAC) failed: %s", esp_err_to_name(err));
		goto eth_init_fail;
	}

  	// Register user defined event handers
    err = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &eth_event_handler, NULL);
    if (err != ESP_OK) {
    	ESP_LOGE(ACTOR_TAG, "ETH event handler register failed: %s", esp_err_to_name(err));
    	goto eth_init_fail;
    }
    err = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &got_ip_event_handler, NULL);
    if (err != ESP_OK) {
    	ESP_LOGE(ACTOR_TAG, "IP event handler register failed: %s", esp_err_to_name(err));
    	goto eth_init_fail;
    }

    //Connect to LAN using ETH Properties
//    Ethernet_Connect();

 	// attach Ethernet driver to TCP/IP stack
  	err = esp_netif_attach(eth_netif_spi, esp_eth_new_netif_glue(eth_handle_spi));
  	if (err != ESP_OK) {
  		ESP_LOGE(ACTOR_TAG, "esp_netif_attach failed: %s", esp_err_to_name(err));
  		goto eth_init_fail;
  	}

    /* start Ethernet driver state machine */
    err = esp_eth_start(eth_handle_spi);
    if (err != ESP_OK) {
    	ESP_LOGE(ACTOR_TAG, "esp_eth_start failed: %s", esp_err_to_name(err));
    	goto eth_init_fail;
    }

// **Uncommented**: start DHCP client so “got_ip” events actually fire
	err = esp_netif_dhcpc_start(eth_netif_spi);
	if (err != ESP_OK && err != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STARTED) {
		ESP_LOGE(ACTOR_TAG, "esp_netif_dhcpc_start failed: %s", esp_err_to_name(err));
		goto eth_init_fail;
	}
	update_ethernet_status(E_ETH_STARTED);
	return;

eth_init_fail:
	/* Avoid reboot loop when external Ethernet module is absent/not-ready. */
	ESP_LOGW(ACTOR_TAG, "Ethernet init failed; continuing without ETH.");
	eth_handle_spi = NULL;
	update_ethernet_status(E_ETH_STOPPED);
}//	Ethernet_Init


static void sw_reset_w5500(AMessage_st* s_Message_Rx)
{
	esp_err_t err = ESP_OK;

	if(phy_spi->deinit(phy_spi)      == ESP_OK){
		//console_send_responce_to_console_xface("ETH PHY Deinit Done.");
	}
	else{
		//console_send_responce_to_console_xface("ETH PHY Deinit Failed!");
	}

	vTaskDelay(500 / portTICK_PERIOD_MS);

	if(phy_spi->init(phy_spi)      == ESP_OK){
		//console_send_responce_to_console_xface("ETH PHY Reset Done.");
	}
	else{
		//console_send_responce_to_console_xface("ETH PHY Reset Failed!");
	}

	if(mac_spi->init(mac_spi)      == ESP_OK){
		//console_send_responce_to_console_xface("ETH MAC Reset Done.");
	}
	else{
		//console_send_responce_to_console_xface("ETH MAC Reset Failed!");
	}

	vTaskDelay(100 / portTICK_PERIOD_MS);

	if(err == ESP_OK)
	{
		Add_Response_msg("ETH Reset Done.",s_Message_Rx);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "ETH Reset Done.");
	}
	else{
		Add_Response_msg("ETH Reset Failed!",s_Message_Rx);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "ETH Reset Failed!");
	}
}//	sw_reset_w5500


static void pingServer(AMessage_st* s_Message_Rx)
{

	cJSON *in_JSON 		= NULL;
	char str[100] = {0};
	char IP_addr[50] = {0};
	int timeout = 0;
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
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
			Add_Response_msg("Ping to the server is successful.",s_Message_Rx);
		else
			Add_Response_msg("Ping to the server is failed.",s_Message_Rx);
	}
	cJSON_Delete(in_JSON);
}//	pingServer


void update_ethernet_status(eth_et eth_status) {
    s_Para.status_u8 = (uint8_t)eth_status;
    set_to_other_actor("CONSOLE", U_INT8, "ETH_STATUS", &s_Para.status_u8);
}



static void Get_Connectivity_Info_For_update_Twin(AMessage_st* s_Message_Rx)
{
	cJSON *connectivity_info = cJSON_CreateObject();
	cJSON *responseObject = cJSON_CreateObject();
	cJSON_AddStringToObject(responseObject, "SSID", "");
	cJSON_AddStringToObject(responseObject, "PASSWORD","");
	cJSON_AddNumberToObject(responseObject, "RSSI",0);
	cJSON_AddStringToObject(responseObject, "WIFI_GATEWAY_MAC_ADDRESS", "");
	cJSON_AddStringToObject(responseObject, "GATEWAY_IP_ADDRESS", (char*)s_Para.GW);
	cJSON_AddStringToObject(responseObject, "DEVICE_IP_ADDRESS", (char*)s_Para.IP);
	cJSON_AddStringToObject(responseObject, "IP_ADDRESS_UPDATED_DATETIME", (char*)s_Para.IP_UPDATED_DATETIME);
	cJSON_AddStringToObject(responseObject, "ETH_MAC_ADDRESS", (char*)s_Para.MAC_ID_a8);
	cJSON_AddStringToObject(responseObject, "ETH_GATEWAY_MAC_ADDRESS", (char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8);
	cJSON_AddItemToObject(connectivity_info, "CONNECTIVITY_INFO", responseObject);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(connectivity_info, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(connectivity_info);
	console_send_responce_to_console_xface(s_Message_Rx);
}

esp_netif_t* get_ethernet_netif_handle() {
    if (eth_netif_spi) {
        return eth_netif_spi;
    } else {
        ESP_LOGE(ACTOR_TAG, "Ethernet netif handle not found");
        return NULL;
    }
}

const char *get_ethernet_device_ip(void)
{
	return s_Para.IP;
}

const char *get_ethernet_gateway_mac(void)
{
	/* If ARP was not resolved at IP-event time, retry up to 5 times with
	 * 200 ms delay between attempts. */
	if (strcmp((char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8, "00:00:00:00:00:00") == 0 &&
		s_Para.GW[0] != '\0' && strcmp(s_Para.GW, "0.0.0.0") != 0)
	{
		ip4_addr_t gw_ip4;
		gw_ip4.addr = esp_ip4addr_aton(s_Para.GW);
		struct netif *lwip_netif = (struct netif *)esp_netif_get_netif_impl(eth_netif_spi);

		for (int retry = 0; retry < 5; retry++)
		{
			struct eth_addr *gw_eth_addr = NULL;
			const ip4_addr_t *gw_ip_found = NULL;
			if (lwip_netif != NULL &&
				etharp_find_addr(lwip_netif, &gw_ip4, &gw_eth_addr, &gw_ip_found) >= 0 &&
				gw_eth_addr != NULL)
			{
				sprintf((char*)s_Para.ETH_GATEWAY_MAC_ADDR_a8,
						"%02x:%02x:%02x:%02x:%02x:%02x",
						gw_eth_addr->addr[0], gw_eth_addr->addr[1],
						gw_eth_addr->addr[2], gw_eth_addr->addr[3],
						gw_eth_addr->addr[4], gw_eth_addr->addr[5]);
				break;
			}
			vTaskDelay(pdMS_TO_TICKS(50));
		}
	}
	return (const char *)s_Para.ETH_GATEWAY_MAC_ADDR_a8;
}
//------------------------------------------------------------//

/* ── Public property accessors for spec-compliant JSON get/set ── */
bool eth_actor_get(const char *name, char *val_out, size_t max_len)
{
    if (!name || !val_out || max_len == 0) return false;
    val_out[0] = '\0';
    get((char *)name, val_out);
    return val_out[0] != '\0';
}

bool eth_actor_set(const char *name, const char *val_in)
{
    if (!name || !val_in) return false;
    return set((char *)name, (char *)val_in, NULL) == 1;
}
