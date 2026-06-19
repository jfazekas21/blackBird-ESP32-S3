/*
 * UDP_Actor.c
 *
 *  Created on: 03-Nov-2023
 *      Author: Sanjeev Gupta
 */
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>

#include "actor.h"
#include "Config.h"
#include "UDP_Actor.h"
#include "Console_Actor.h"

#define OBJ_QUE_COUNT                	5

static const char * THIS_ACTOR = "UDP";
static const char 			THIS_ACTOR_ID 	= 	UDP;  // assign src id
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
BaseType_t udpMonitor;
TaskHandle_t udpHandle = NULL, udpTaskHandle = NULL;
QueueHandle_t udp_Rx_Queue,UDP_Sockettask_Que; //, udp_Tx_Queue;
//static uint8_t *Sock_pucQueueStorage = NULL;
PSRAM_ATTR_BSS static uint8_t Sock_pucQueueStorage [10 * 1000];

static StaticQueue_t Sock_pxQueueBuffer;
static AMessage_st s_Message_Tx;//s_Message_Rx,
static int NET_status = -1;
//static char out[MAX_OUT_SIZE];
static int FirstudpEntry = 0;
static uint8_t UDPSockRespFg =0;
//static QueueHandle_t 	msg_Tx_Queue	= NULL; 	// HTTP Tx Queue
//static QueueHandle_t 	msg_Rx_Queue	= NULL; 	// HTTP Rx Queue
static StaticTask_t xUDPTaskBuffer,xUDPSockTaskBuffer;  //// Declare a static task control block
//static uint8_t *Monitor_pucQueueStorage = NULL;
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [OBJ_QUE_COUNT * sizeof(AMessage_st)];
PSRAM_ATTR_BSS static  StackType_t xTaskStack[UDP_TASK_STACK_DEPTH];
static StaticQueue_t Monitor_pxQueueBuffer;
PSRAM_ATTR_BSS static char filedata_1[COMMAND_LEN];
PSRAM_ATTR_BSS static StackType_t xTaskStack1[UDP_SOCKET_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static char line[COMMAND_LEN];

//static void UDP_add_response_to_UDP_Tx_Queue(const char *Destactor, char *response, int16_t size, char *CmdFunc);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);

static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx) ;			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);

//static void udp_connect();
static void udp_server_send_data(AMessage_st* s_Message_Rx);
static void udp_server_task(void *pvParameters);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
//static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer);
static void Process_Passthrough_UDP_Methods(AMessage_st* s_Message_Rx, char* methodspayload);

//static uint8_t twin_data_received = 0;

//char filedata_1[MAX_OUT_SIZE];

static uint8_t data_to_send = 0;

PSRAM_ATTR_BSS static struct udp_parameter {
	uint8_t status_u8;
//	uint16_t client_port;
	uint16_t server_port;
//	uint8_t client_ip_addr [30];
	uint8_t server_ip_addr [30];
}s_para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &s_para.status_u8,             "STATUS",         U_INT8,   "R", "Status of the communication" },
//    { &s_para.client_port,           "CLIENT_PORT",    U_INT16,  "RW", "Port number of the client" },
    { &s_para.server_port,           "SERVER_PORT",    U_INT16,  "RW", "Port number of the server" },
//    { &s_para.client_ip_addr,        "CLIENT_IP_ADDR", STRING,   "RW", "IP address of the client" },
    { &s_para.server_ip_addr,        "SERVER_IP_ADDR", STRING,   "RW", "IP address of the server" }
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_UDP[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static uint8_t UDP_buffer[COMMAND_LEN];
PSRAM_ATTR_BSS static char UDP_buffer_QRec[4096];
PSRAM_ATTR_BSS static char rx_buffer[COMMAND_LEN];
PSRAM_ATTR_BSS static char print_buffer[COMMAND_LEN+200];


//***************UDP Actor Methods Function*************************************
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
		}
	}
}//	get

//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx){
//
//	cJSON *out_JSON  = cJSON_CreateObject();
//	//no of elements
//	int no_of_elements = sizeof(prop) / sizeof(struct property);
//	for (int i = 0; i < no_of_elements; i++) {
//
//		switch (prop[i].type) {
//		case U_INT8:
//			sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
//			break;
//
//		case U_INT16:
//			sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
//			break;
//
//		case U_INT32:
//			sprintf(val_a8, "%ld", *(uint32_t*) prop[i].name);
//			break;
//
//		case INT:
//			sprintf(val_a8, "%d", *(int*) prop[i].name);
//			break;
//
//		case FLOAT:
//			sprintf(val_a8, "%f", *(float*) prop[i].name);
//			break;
//
//		case STRING:
//			strcpy(val_a8, prop[i].name);
//			break;
//
//		default:
//			break;
//		}
//		cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
//	}
//	if(s_Message_Rx->payload_p8!= NULL)
//	{
//		cJSON_free(s_Message_Rx->payload_p8);
//		s_Message_Rx->payload_p8 = NULL;
//	}
//	s_Message_Rx->payload_p8 = (uint8_t*)cJSON_PrintUnformatted(out_JSON);
//	console_send_responce_to_console_xface(s_Message_Rx);
//	cJSON_Delete(out_JSON);
//}	//	getAll

static void init(void *a, void *b) {

	if (FirstudpEntry == 0){
		if (udp_Rx_Queue == NULL)
		{
			udp_Rx_Queue = xQueueCreateStatic(OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

			if (udp_Rx_Queue == NULL) {
				printf(">%s.INIT(Error! UDP RX Queue is not created.)\n", THIS_ACTOR);
			}
		}

		udpHandle = xTaskCreateStaticPinnedToCore(
						monitor,                 // Task function
						"UDP Monitor",            // Task name
						UDP_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						UDP_TASK_PRIORITY,                       // Task priority
						xTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xUDPTaskBuffer,             // Pointer to task control block
						1
	   );

		if (udpHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
				    printf("Failed to create task\n");
#endif
				    // Handle error
		}

		s_para.server_port =1111;
		s_para.status_u8 = UDP_INITIALISED;
		FirstudpEntry = 1;
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor

//		cJSON *responseObject = cJSON_CreateObject();
//		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/UDP.json");
//		memset(payLoadData,0,sizeof(payLoadData));
//		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
//		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
//		cJSON_Delete(responseObject);
	}
}

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

		if (pdTRUE == xQueueReceive(udp_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
			//printf("UDP msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("UDP DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties

				if (FirstudpEntry == 0)
					init(0, 0);
				else if(FirstudpEntry==1){
					Add_Response_msg("UDP initialization is done.", s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
				{
				Get_Property(s_Message_Rx);
				}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
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
				cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/UDP.json");
			   // Loop through each key-value pair
				do {
					// Check if the value string is not NULL
					if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
					{
						// Set the key-value pair
						u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
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
				payLoadData[0] = '\0';
				cJSON_PrintPreallocated(root_JSON, payLoadData, sizeof(payLoadData), false);				
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM",payLoadData,strlen(payLoadData),"WRITE_VAR");
				cJSON_Delete(root_JSON);
				console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				}
				// Free the parsed JSON
				cJSON_Delete(name_JSON);
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
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DISCONNECT"))
			{
				if(udpTaskHandle != NULL)
				{
					vTaskDelete(udpTaskHandle);
					udpTaskHandle = NULL;
					Add_Response_msg("UDP task is deleted.", s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "CONNECT"))
			{
				if(UDP_Sockettask_Que == NULL)
					//	TCP_Socket1task_Que = xQueueCreateInPSRAM(5, 1000);
					if (UDP_Sockettask_Que == NULL)
					{
//						UDP_Sockettask_Que = xQueueCreateInPSRAM(10, 1000, &Sock_pucQueueStorage, &Sock_pxQueueBuffer);
						UDP_Sockettask_Que = xQueueCreateStatic(10, 1000, Sock_pucQueueStorage, &Sock_pxQueueBuffer);

						if (UDP_Sockettask_Que == NULL){
		#ifdef ENABLE_PRINT_MSG
						 printf("ERROR(Socket1task_Que is not created)\n");
		#endif
						}
					}
				if(udpTaskHandle == NULL){
					AMessage_st s_Message_Rx_data;
					strcpy((char*)s_Message_Rx_data.dest_Actor_a8, (char*)s_Message_Rx->dest_Actor_a8);
					strcpy((char*)s_Message_Rx_data.src_Actor_a8, (char*)s_Message_Rx->src_Actor_a8);
					strcpy((char*)s_Message_Rx_data.cmdFun_a8, (char*)s_Message_Rx->cmdFun_a8);

					s_Message_Rx_data.payload_p8 = UDP_buffer;
					udpTaskHandle = xTaskCreateStaticPinnedToCore(
							            udp_server_task,                 // Task function
										"udp_server",            // Task name
										UDP_SOCKET_TASK_STACK_DEPTH,        // Stack size in words
										&s_Message_Rx_data,                    // Task parameters (not used here)
										UDP_SOCKET_TASK_PRIORITY,                       // Task priority
										xTaskStack1,              // Pointer to task stack (allocated in PSRAM)
										&xUDPSockTaskBuffer,             // Pointer to task control block
										1
					         	);
			    	}
				 else
				 {
					 Add_Response_msg("Task for UDP CONNECT command is already created.", s_Message_Rx, payLoadData);
					if (udpTaskHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
							printf("Failed to create task\n");
							// Handle error
#endif
					}
				 }
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "UDP_SEND_DATA"))
			{
#ifdef ENABLE_PRINT_MSG
				printf("Enter in UDP_SEND_DATA case\n");
#endif
				udp_server_send_data(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
			}
			else
			{
				  Add_Response_msg("Invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}

void UDP_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstudpEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(udp_Rx_Queue, s_Message, QUE_DELAY);

	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<UDP.ERROR(UDP RX Queue is full)\n");
		}
		else
		{
			printf("<UDP.ERROR(UDP RX Queue send unsuccessful)\n");
		}
	}
}//	UDP_ConsolWriteToActor

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[32] = {0};
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
				case UDP_INITIALISED:
					strcpy(val_a8, "UDP_INITIALISED");
					break;

				case UDP_DISCONNECTED:
					strcpy(val_a8, "UDP_DISCONNECTED");
					break;

				case UDP_CONNECTED:
					strcpy(val_a8, "UDP_CONNECTED");
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
		payLoadData[0] = '\0';
		cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
		cJSON_Delete(jsonObject);
		console_send_responce_to_console_xface(s_Message_Rx);
}	//	get_actor_properties

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the UDP actor.");
	cJSON_AddStringToObject(responseObject, "SET(string SERVER_IP_ADDR)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "CONNECT()", "Connect to the UDP socket.");
	cJSON_AddStringToObject(responseObject, "DISCONNECT()", "Delete UDP task. ");
	cJSON_AddStringToObject(responseObject, "UDP_SEND_DATA(string DATA)", "Send message to UDP socket.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	payLoadData[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);

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
		Add_Response_msg(str, s_Message_Rx, payLoadData);
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


//***************UDP Functions*************************************

//static void udp_server_task(void *pvParameters)
//{
//    char rx_buffer[128]={0};
//    char addr_str[128];
//    int addr_family = (int)pvParameters;
//    int ip_protocol = 0;
//    struct sockaddr_in6 dest_addr;
//    char UDP_Data_Payload[300]={0};
//    char u8printflag = 1;
//
//   printf("Before while1");
//    while (1) {
//
//        if (addr_family == AF_INET) {
//            struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
//            dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
//            dest_addr_ip4->sin_family = AF_INET;
//            dest_addr_ip4->sin_port = htons(s_para.server_port);
//            ip_protocol = IPPROTO_IP;
//        } else if (addr_family == AF_INET6) {
//            bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
//            dest_addr.sin6_family = AF_INET6;
//            dest_addr.sin6_port = htons(s_para.server_port);
//            ip_protocol = IPPROTO_IPV6;
//        }
//
//        int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
//        if (sock < 0) {
//        	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to create socket");
//         //   ESP_LOGE(ACTOR_TAG, "Unable to create socket: errno %d", errno);
//            break;
//        }
//        if(u8printflag==1)
//       {
//        console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket created");
//        u8printflag=2;
//       }
//       // ESP_LOGI(ACTOR_TAG, "Socket created");
//
//        // Set timeout
//        struct timeval timeout;
//        timeout.tv_sec = 10;
//        timeout.tv_usec = 0;
//        setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
//
//        int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
//        if (err < 0) {
//            ESP_LOGE(ACTOR_TAG, "Socket unable to bind: errno %d", errno);
//        }
//        if(u8printflag==2)
//        {
//        s_para.status_u8 = UDP_CONNECTED;
//        console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "UDP CONNECTED");
//        u8printflag=0;
//        }
//        ESP_LOGI(ACTOR_TAG, "Socket bound, port %d", s_para.server_port);
//
//        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
//        socklen_t socklen = sizeof(source_addr);
//
//
//
//        while (1) {
//           // ESP_LOGI(ACTOR_TAG, "Waiting for data");
//           // printf("Waiting for data\n");
//
//        	if(twin_data_received == 1)
//        	{
//                int err = sendto(sock, filedata, strlen(filedata), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
//                twin_data_received = 0;
//                if (err < 0) {
//                    ESP_LOGE(ACTOR_TAG, "Error occurred during sending: errno %d", errno);
//                    break;
//                }
//        	}
//        	printf("rx_buffer3:%s\n",rx_buffer);
//            int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);
//               printf("len:%d",len);
//            // Error occurred during receiving
//            if (len < 0) {
//              //  ESP_LOGE(ACTOR_TAG, "recvfrom failed: errno %d", errno);
//            	printf("rx_buffer1:%s\n",rx_buffer);
//                break;
//            }
//            // Data received
//            else {
//            	printf("rx_buffer2:%s\n",rx_buffer);
//                // Get the sender's ip address as string
//                if (source_addr.ss_family == PF_INET) {
//                    inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
//
//                } else if (source_addr.ss_family == PF_INET6) {
//                    inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
//                }
//
//                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...
//               printf("Received %d bytes from %s:\n", len, addr_str);
//                //ESP_LOGI(ACTOR_TAG, "Received %d bytes from %s:", len, addr_str);
//
//                sprintf(UDP_Data_Payload, "\n [{\"UDP_Data\": \"%s\"}]",(char*)rx_buffer);
//                printf("UDP_Data_Payload:%s\n",UDP_Data_Payload);
//                console_send_responce_to_console_xface(MSG_STR_MED, THIS_ACTOR,UDP_Data_Payload);
//
//               //printf("Data:%s\n", rx_buffer);
//                //ESP_LOGI(ACTOR_TAG, "%s", rx_buffer);
//
///*                sprintf(rx_buffer,"Data Received\r\n");
//                int err = sendto(sock, rx_buffer, strlen(rx_buffer), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
//                if (err < 0) {
//                    ESP_LOGE(ACTOR_TAG, "Error occurred during sending: errno %d", errno);
//                    break;
//                }*/
//				char* buffer =  (char*) heap_caps_calloc(50, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//
//                sprintf(buffer,"{\"filename\":\"A:/System/twin.json\"}");
//                printf("buffer=%s",buffer);
//                UDP_add_response_to_UDP_Tx_Queue("FILE_MGT", buffer, strlen(buffer), "READ");
//
//            }
//        }
//
//        if (sock != -1) {
////        	s_para.status_u8 = UDP_DISCONNECTED;
////        	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "UDP DISCONNECTED");
//            ESP_LOGE(ACTOR_TAG, "Shutting down socket and restarting...");
//            shutdown(sock, 0);
//            close(sock);
//        }
//    }
//    vTaskDelete(NULL);
//}

static void udp_server_task(void *pvParameters)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
//    char rx_buffer[3000];
    char addr_str[128], str[100] = {0};
//    char print_buffer[3512];
    int addr_family = AF_INET; //(int)pvParameters;
    int ip_protocol = 0;
    struct sockaddr_in6 dest_addr;
    char u8printflag = 1;



    cJSON *my_JSON  	= cJSON_CreateArray();
	if (my_JSON == NULL) {
	        Add_Response_msg("Failed to create JSON object for network status.", s_Message_Rx, payLoadData_UDP);
	        goto exit;
	    }
	cJSON_AddItemToArray(my_JSON, cJSON_CreateString("NET_STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON);
	memset(payLoadData_UDP,0,sizeof(payLoadData_UDP));
	cJSON_PrintPreallocated(jsonObject, payLoadData_UDP, sizeof(payLoadData_UDP), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_UDP, strlen(payLoadData_UDP), "GET");

	//printf("\n udp_server_task \n");
//    while (1)
//    {
    	//if(NET_status == E_NET_CONNECTED)
    	if (pdTRUE == xQueueReceive(UDP_Sockettask_Que, (void*)UDP_buffer_QRec, portMAX_DELAY))
		{

//    		printf("\n UDP_buffer_QRec = %s \n", UDP_buffer_QRec);
    		root_new 	  = cJSON_Parse((char*)UDP_buffer_QRec);
				 if (root_new == NULL)
				 {
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData_UDP);
					goto exit;
				 }
					name_new_JSON  = cJSON_GetObjectItem(root_new, "NET_STATUS");  // for SQL
						if((name_new_JSON != NULL)&& cJSON_IsString(name_new_JSON))
							NET_status = atoi(name_new_JSON->valuestring);
				//				printf("file_size:%d\n",file_size);
						cJSON_Delete(root_new);

				if(NET_status != E_wifi_CONNECTED_LOW)
					goto exit;

			if (addr_family == AF_INET)
			{
				struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
				dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
				dest_addr_ip4->sin_family = AF_INET;
				dest_addr_ip4->sin_port = htons(s_para.server_port);
				ip_protocol = IPPROTO_IP;
			}
			else if (addr_family == AF_INET6)
			{
				bzero(&dest_addr.sin6_addr.un, sizeof(dest_addr.sin6_addr.un));
				dest_addr.sin6_family = AF_INET6;
				dest_addr.sin6_port = htons(s_para.server_port);
				ip_protocol = IPPROTO_IPV6;
			}

			int sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
			if (sock < 0)
			{
				sprintf(str,"Unable to create socket: errno %d", errno);
				Add_Response_msg(str,s_Message_Rx, payLoadData_UDP);
				goto exit;
			}
			if(u8printflag==1)
		   {
				Add_Response_msg("Socket created",s_Message_Rx, payLoadData_UDP);
				u8printflag=2;
		   }

			// Set timeout
			struct timeval timeout;
			timeout.tv_sec = 10;
			timeout.tv_usec = 0;
			setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

			int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err < 0)
			{
				sprintf(str, "Socket unable to bind: errno %d", errno);
				Add_Response_msg(str,s_Message_Rx, payLoadData_UDP);
				goto exit;
			}
			if(u8printflag==2)
			{
				s_para.status_u8 = UDP_CONNECTED;
				Add_Response_msg("UDP CONNECTED",s_Message_Rx, payLoadData_UDP);
				sprintf(str,"Socket bound, port %d", s_para.server_port);
				Add_Response_msg(str,s_Message_Rx, payLoadData_UDP);
				u8printflag=0;
			}

			struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
			socklen_t socklen = sizeof(source_addr);

			while (1)
			{
//				if(twin_data_received == 1)
//				{
//					int err = sendto(sock, filedata, strlen(filedata), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
//					twin_data_received = 0;
//					if (err < 0)
//					{
//						sprintf(str,"Error occurred during sending: errno %d", errno);
//						Add_Response_msg(str,s_Message_Rx);
//						break;
//					}
//				}
				if(data_to_send == 1)
				{
					//printf("filedata_1:%s\n",filedata_1);
					int err = sendto(sock, filedata_1, strlen(filedata_1), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
					data_to_send = 0;
					if (err < 0)
					{
						sprintf(str,"Error occurred during sending: errno %d", errno);
						Add_Response_msg(str,s_Message_Rx, payLoadData_UDP);
						goto exit;
					}
				}

				int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

				// Error occurred during receiving
				if (len < 0) {
					//printf("recvfrom failed: errno %d \n", errno);
					//goto exit;
				}
				// Data received
				else
				{
						// Get the sender's ip address as string
						if (source_addr.ss_family == PF_INET) {
							inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);

						} else if (source_addr.ss_family == PF_INET6) {
							inet6_ntoa_r(((struct sockaddr_in6 *)&source_addr)->sin6_addr, addr_str, sizeof(addr_str) - 1);
						}
						rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string...

						sprintf(print_buffer,"Received %d bytes: %s", len, rx_buffer);
						Add_Response_msg(print_buffer,s_Message_Rx, payLoadData_UDP);
						UDPSockRespFg = 1;

						Process_Passthrough_UDP_Methods(s_Message_Rx,rx_buffer);

	    	}
				vTaskDelay(10 / portTICK_PERIOD_MS);
			}

			if (sock != -1)
			{
				shutdown(sock, 0);
				close(sock);
				s_para.status_u8 = UDP_DISCONNECTED;
			}
		}// end of if(NET_status == E_NET_CONNECTED)

 exit:
				Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_UDP);
				udpTaskHandle = NULL;
				vTaskDelete(udpTaskHandle);  // Delete the task
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
{	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON = NULL;
//	char keyValue[100] = {0};
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n s_Message_Rx->payload_p8 = %s \n",s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  //  return 1;
		return;
	}
	else
	{
			// Obtain the COMMAND and RESPONSE keys
			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

			if (commandKey != NULL && responseKey != NULL)
			{
				payLoadData[0] = '\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);

				if(UDPSockRespFg ==1)
				{
					cJSON *UDP_RESP = cJSON_CreateObject();
					cJSON_AddStringToObject(UDP_RESP, "DATA", payLoadData);
					 UDPSockRespFg =0;
				/* 	if(s_Message_Rx->payload_p8 != NULL)
						{
							cJSON_free(s_Message_Rx->payload_p8);
							s_Message_Rx->payload_p8 = NULL;
						} */

/* 					char *line11=NULL;
					line11   =cJSON_PrintUnformatted(UDP_RESP);

		        	s_Message_Rx->payload_p8 = heap_caps_calloc((strlen(line11)+1),sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

					if(s_Message_Rx->payload_p8 != NULL)
					strcpy(((char*)s_Message_Rx->payload_p8),line11); */
//					s_Message_Rx->payload_p8 = (uint8_t*)cJSON_PrintUnformatted(UDP_RESP);

//					printf("payload_p8 = %s \n", s_Message_Rx->payload_p8);

					payLoadData[0] = '\0';
					cJSON_PrintPreallocated(UDP_RESP, payLoadData, sizeof(payLoadData), false);
					strcpy(((char*)s_Message_Rx->payload_p8),payLoadData);
					udp_server_send_data(s_Message_Rx);

					/* if(line11!=NULL)
					{
						free(line11);
						line11 = NULL;
					}

					cJSON_free(cJSON_string); */
					return;
				 }
//				cJSON_free(cJSON_string);
			}
			// Free the parsed JSON
			cJSON_Delete(in_JSON);
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
		 {
			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
			if (in_JSON == NULL) {
				sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData);
				  //  return 1;
				return;
			}
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
//					strcpy((char*)temp,name_JSON->valuestring);
//					NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
					payLoadData[0] = '\0';
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);					
					if(UDP_Sockettask_Que!=NULL)
						xQueueSend(UDP_Sockettask_Que, payLoadData, QUE_DELAY);
				}
			}
			cJSON_Delete(in_JSON);
			return;
		 }

//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
//	{
//		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//		if (in_JSON == NULL) {
//			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//			Add_Response_msg(str,s_Message_Rx, payLoadData);
//			  //  return 1;
//			return;
//		}
//		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
//		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
//		{
//			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
//			if (root != NULL)
//			{
//				// Iterate over the keys
//				cJSON *currentItem = root->child;
//				if(currentItem->valuestring != NULL)
//				{
////					if(strcmp(currentItem->string,"Twin Message")==0)
////					{
////						filedata = cJSON_PrintUnformatted(currentItem);
////						//printf("\n filedata=%s\n", filedata);
//////						twin_data_received = 1;
////						cJSON_Delete(in_JSON);
////						return;
////					}
////					if(!strcmp(currentItem->valuestring, "System/UDP.json"))
//					if(!strcasecmp(currentItem->valuestring, "System/UDP.json"))
//					{
//						currentItem = currentItem->next;
//						while (currentItem != NULL)
//						{
//							if (cJSON_IsString(currentItem))   // Check the type of the value
//							{
//								set(currentItem->string, currentItem->valuestring,s_Message_Rx);
//							}
//							else if (cJSON_IsNumber(currentItem))
//							{
//								sprintf(keyValue, "%d", currentItem->valueint);
//								set(currentItem->string, keyValue,s_Message_Rx);
//							}
//							currentItem = currentItem->next;    // Move to the next key-value pair
//						}
//					}
//				}
//			}
//		}
//		cJSON_Delete(in_JSON);
//		return;
//	}
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

static void udp_server_send_data(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char str[100]={0};

	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  //  return 1;

		return;
	}
	else{

	name_JSON = cJSON_GetObjectItem(in_JSON, "DATA");
	strcpy(filedata_1,name_JSON->valuestring);
	data_to_send = 1;
	cJSON_Delete(in_JSON);
	}
}

static void Process_Passthrough_UDP_Methods(AMessage_st* s_Message_Rx, char* methodspayload)
{
	cJSON *Message = cJSON_Parse(methodspayload);
	 if(Message == NULL)
	 {
		 Add_Response_msg("No command received from UDP server",s_Message_Rx, payLoadData_UDP);
		 cJSON_Delete(Message);
		 return;
	 }
    cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(Message, "methods");

	if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
	{
		int arraySize = cJSON_GetArraySize(methodsArray);

		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(methodsArray, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL)) {
#ifdef ENABLE_PRINT_MSG
				printf("Element %d: %s\n", i, element->valuestring);
#endif
				/* execute the command */
//				char line[2048] = {0};
				strcpy(line, element->valuestring);
				int ret;
				if(line[0]=='<')
				{
					esp_err_t err = esp_console_run_Custom(line, &ret, THIS_ACTOR);

					if (err == ESP_ERR_NOT_FOUND) {
#ifdef ENABLE_PRINT_MSG
						printf("Unrecognized command\n");
#endif
					} else if (err == ESP_ERR_INVALID_ARG) {

						// command was empty
					} else if (err == ESP_OK && ret != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
						printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
						esp_err_to_name(ret));
#endif
					} else if (err != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
						printf("Internal error: %s\n", esp_err_to_name(err));
#endif
					}
					//vTaskDelay(300/portTICK_PERIOD_MS);

				}
			}
			else
			{
#ifdef ENABLE_PRINT_MSG
				printf("Element %d: %s\n", i, element->string);
#endif
			}
			// vTaskDelay(100/ portTICK_PERIOD_MS);
		}
		cJSON_Delete(Message);
		return;
	}
	   cJSON_Delete(Message);
}
