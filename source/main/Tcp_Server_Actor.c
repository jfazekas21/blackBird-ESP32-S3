/*
 * Tcp_Actor.c
 *
 *  Created on: 31-Oct-2023
 *      Author: Sai
 */


#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "Tcp_Server_Actor.h"
#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"


PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
//#define PORT CONFIG_EXAMPLE_PORT
#define PORT 1001
#define PORT1 1002
#define PORT2 1003
#define PORT3 1004

#define KEEPALIVE_IDLE              5
#define KEEPALIVE_INTERVAL          5
#define KEEPALIVE_COUNT             3

#define OBJ_QUE_COUNT                	5

static const char * THIS_ACTOR = "TCP_SERVER";
static const char 			THIS_ACTOR_ID 	= 	TCP_SERVER;  // assign src id

BaseType_t tcpMonitor;
TaskHandle_t tcpHandle;
TaskHandle_t TCP_Server_Listen_Handle_1,TCP_Server_Listen_Handle_2,TCP_Server_Listen_Handle_3,TCP_Server_Listen_Handle_4;
QueueHandle_t tcp_Rx_Queue=NULL, TCP_Socket1task_Que = NULL, TCP_Socket2task_Que = NULL, TCP_Socket3task_Que = NULL, TCP_Socket4task_Que = NULL; //, tcp_Tx_Queue;
static StaticTask_t xTCPTaskBuffer, xTCPSock1TaskBuffer, xTCPSock2TaskBuffer, xTCPSock3TaskBuffer, xTCPSock4TaskBuffer;  //// Declare a static task control block
static StaticQueue_t Sock1_pxQueueBuffer ,Sock2_pxQueueBuffer,Sock3_pxQueueBuffer ,Sock4_pxQueueBuffer ;

static StaticQueue_t Monitor_pxQueueBuffer;
//static Actor_st tcp;                           // Tcp  Actor structure
static AMessage_st s_Message_Tx;//s_Message_Rx,

static int FirsttcpEntry = 0;
static int NET_status = -1;
static void init(void *a, void *b);                         //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			    //	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
//static void print(char *str_buf);
static void help(AMessage_st* s_Message_Rx) ;
static void monitor(void *pvParameters __attribute__((unused)));
//static void getAll(char *str_prop, char *val_a8,AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
static void tcp_server_destroy_socket(AMessage_st* s_Message_Rx);
static void tcp_server_create_socket(AMessage_st* s_Message_Rx);
static void tcp_server_send_data(AMessage_st* s_Message_Rx);
static void tcp_server_get_sock_states(AMessage_st* s_Message_Rx);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void tcp_server_task_1(void *pvParameters);
static void tcp_server_task_2(void *pvParameters);
static void tcp_server_task_3(void *pvParameters);
static void tcp_server_task_4(void *pvParameters);
static void do_retransmit(int sock,AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
//static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer);

#define MAX_TCP_SRVER_SOCK_INSTANCE 4
#define TCP_SERVER_SOCKET_OPEN 0
#define TCP_SERVER_SOCKET_CLOSED 1
#define TCP_SERVER_SOCKET_LISTEN 2
#define TCP_SERVER_SOCKET_CONNECTED 3

PSRAM_ATTR_BSS static struct tcp_srverparameter {
	uint16_t portNo;
	int listen_sock;
	int sock;
	char host_ip[25];
	char states;
	char TCPSockRespFg;

} tcp_server_para,tcp_srverpara_info[MAX_TCP_SRVER_SOCK_INSTANCE];
TaskHandle_t TCP_Server_Listen_Handle_1,TCP_Server_Listen_Handle_2,TCP_Server_Listen_Handle_3,TCP_Server_Listen_Handle_4;


static int8_t tcp_srver_sock_instance=0;
static uint8_t u8Socket1DestroyFg=0,u8Socket2DestroyFg=0,u8Socket3DestroyFg=0,u8Socket4DestroyFg=0;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &tcp_server_para.portNo, "PORT_NO",  INT,     "RW", "Port number of the TCP server" },
    { &tcp_server_para.host_ip, "HOST_IP", STRING, "RW", "Host IP address of the TCP server" }
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES];
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage[OBJ_QUE_COUNT * sizeof(AMessage_st)];
PSRAM_ATTR_BSS static uint8_t Sock1_pucQueueStorage[10 * 1000],Sock2_pucQueueStorage [10 * 1000],Sock3_pucQueueStorage [10 * 1000],Sock4_pucQueueStorage[10 * 1000];
PSRAM_ATTR_BSS StackType_t xTaskStack1 [TCP_SOCKET1_TASK_STACK_DEPTH], xTaskStack2 [TCP_SOCKET2_TASK_STACK_DEPTH], xTaskStack3 [TCP_SOCKET3_TASK_STACK_DEPTH], xTaskStack4 [TCP_SOCKET4_TASK_STACK_DEPTH];
//static Actor_st tcp = { prop, get, set, init, print, monitor, help,
//		&tcp_Tx_Queue, &tcp_Rx_Queue, 0, 0, 0, getAll, 0, 0};

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

static void init(void *a, void *b) {

	// Calculate the total size of memory required for the stack array
		size_t stack_size = TCP_TASK_STACK_DEPTH * sizeof(StackType_t);

	if (FirsttcpEntry == 0){

//		char* out;

//		tcp_Tx_Queue = xQueueCreate(OBJ_QUE_COUNT, sizeof(s_Message_Tx));
//		if (tcp_Tx_Queue == NULL) {
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP TX Queue is not created.");
//		}

		tcp_Rx_Queue = xQueueCreateStatic(OBJ_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

//		tcp_Rx_Queue = xQueueCreateInPSRAM(OBJ_QUE_COUNT, sizeof(AMessage_st), &Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);

		if (tcp_Rx_Queue == NULL) {
			printf(">%s.INIT(Error! TCP RX Queue is not created.)\n", THIS_ACTOR);
		//	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP RX Queue is not created.");
		}
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor

		//	 Creating Monitor Task
			StackType_t *xTaskStack = (StackType_t *)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
					if (xTaskStack == NULL) {
					    // Memory allocation failed
#ifdef ENABLE_PRINT_MSG
					    printf( "Failed to allocate memory for the task stack");
#endif
					    // Handle error
					    return;
					}
					tcpHandle = xTaskCreateStaticPinnedToCore(
									monitor,                 // Task function
									"TCP Monitor",            // Task name
									TCP_TASK_STACK_DEPTH,        // Stack size in words
									NULL,                    // Task parameters (not used here)
									TCP_TASK_PRIORITY,                       // Task priority
									xTaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xTCPTaskBuffer,             // Pointer to task control block
									0
				   );

			if (tcpHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
				    printf("Failed to create task\n");
#endif
				    // Handle error
				}

//		tcpMonitor = xTaskCreate(monitor, (const char*) "TCP Monitor", 4096, NULL, 2, &tcpHandle);
		FirsttcpEntry = 1;

		cJSON *responseObject = cJSON_CreateObject();
		cJSON_AddStringToObject(responseObject, "FILE_NAME","A:/System/TCP.json");
		payLoadData[0] = '\0';
		cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
		Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData, strlen(payLoadData), "READ");
		cJSON_Delete(responseObject);

//		strcpy(out, "{\"filename\":\"A:/System/TCP.json\"}");
//	    Add_response_to_Tx_Queue("FILE_MGT", out, strlen(out), "READ");

		for(int i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
		{
			tcp_srverpara_info[i].portNo = 0;
			tcp_srverpara_info[i].listen_sock = -2;
			tcp_srverpara_info[i].sock = -2;
			tcp_srverpara_info[i].states = TCP_SERVER_SOCKET_OPEN;
		}
		tcp_srver_sock_instance = 0;

	//	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP Initialization Done.");
	}

//	else
//		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP is already Initialization Done.");

}

static void monitor(void *pvParameters __attribute__((unused))) {

//	cJSON *root = NULL;
//	cJSON *name = NULL;
	cJSON *name_JSON = NULL;
    cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
//	uint8_t *val_p8  = NULL;
	char str[100] = {0};
////	char keysTemp[MAX_OUT_SIZE];
////	char keysValue[MAX_OUT_SIZE];
////	char Rx_buffer_used = 0;
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
//		char Rx_buffer[8192];
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));

		if (pdTRUE == xQueueReceive(tcp_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
//			Rx_buffer_used = 0;
//			printf("\n TCP_Rx_Queue\n");
		//	printf("\n TCP_Rx_Queue S = %s, D = %s, C = %s, DT = %s\n\n", s_Message_Rx.src_Actor_a8,	s_Message_Rx.dest_Actor_a8, s_Message_Rx.cmdFun_a8,s_Message_Rx.payload_p8);

			//printf ("TCP_Rx_DBG:>M_ID:\t%d\r\nCmd:\t%s\r\nCOUNT:\t%d\r\nSRC_Actor:\t%s\r\ndest_ACtor:\t%s\r\nARG:\t%s %s %s %s %s %s %s %s\r\n",s_MyMessage.ucMessageID,s_MyMessage.cmdFun_a8,s_MyMessage.payload_p8c,s_MyMessage.src_Actor, s_MyMessage.dest_Actor,s_MyMessage.payload_p8,s_MyMessage.payload_p8[1],s_MyMessage.payload_p8[2],s_MyMessage.payload_p8[3],s_MyMessage.payload_p8[4],s_MyMessage.payload_p8[5],s_MyMessage.payload_p8[6],s_MyMessage.payload_p8[7]);
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{   // INIT Properties

				if (FirsttcpEntry == 0)
				{
					init(0,0);
				}
				else if(FirsttcpEntry==1){
				Add_Response_msg("TCP initialization is done.", s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
				{
					Get_Property(s_Message_Rx);
				}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET"))
			{
				name_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
				if (name_JSON == NULL) {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData);
				    }
				else{
				head_JSON = name_JSON->child;
				cJSON *root_JSON  = cJSON_CreateObject();
				cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/TCP.json");
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
//				getAll(prop, (char*) val_p8, s_Message_Rx);
//				free(val_p8);
//			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "CREATE_SOCKET"))
			{
//				Rx_buffer_used = 1;
				tcp_server_create_socket(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "DESTROY_SOCKET"))
			{
				if (NET_status == E_wifi_CONNECTED_AP)
				{
					tcp_server_destroy_socket(s_Message_Rx);
				}
				else
				{
					Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SEND_DATA"))
			{
				if (NET_status == E_wifi_CONNECTED_AP)
				{
					tcp_server_send_data(s_Message_Rx);
				}
				else
				{
					Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SOCKET_STATES"))
			{
				if (NET_status == E_wifi_CONNECTED_AP)
				{
					tcp_server_get_sock_states(s_Message_Rx);
				}
				else
				{
					Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData);
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
//			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "START_TCP_SERVER"))
//			{
//				Send_CMD_To_Other_Actor(WIFI,"WIFI","/0", 0, "SWITCH_AP_MODE");
//			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
				{
					Analyse_Response(s_Message_Rx);
				}
          else
			{
				//TCP error message: invalid method
				  Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}

//			if (Rx_buffer_used == 0)
//			{
//			  if(s_Message_Rx->payload_p8 !=  NULL)
//			  {
//				console_MessageRelease_xface((char*) s_Message_Rx->payload_p8);
//			  }
//				if(Rx_buffer != NULL)
//				{
//					free(Rx_buffer);
//					Rx_buffer = NULL;
//				}
//
//			}
			// Free s_MyMessage.payload_p8 here
//			if(s_Message_Rx->payload_size!=0)
//				console_MessageRelease_xface((char*)s_Message_Rxpayload_p8);
		}
		//vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}

void TCP_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirsttcpEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(tcp_Rx_Queue, s_Message, QUE_DELAY);

	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}

		if (state == errQUEUE_FULL)
		{
			printf("<TCP_SERVER.ERROR(TCP_SERVER RX Queue is full)\n");
		}
		else
		{
			printf("<TCP_SERVER.ERROR(TCP_SERVER RX Queue send unsuccessful)\n");
		}
	}
}//	LED_ConsolWriteToActor

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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the TCP_SERVER actor.");
	cJSON_AddStringToObject(responseObject, "SET(string HOST_IP)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(HOST_IP)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "CREATE_SOCKET(INT PORT_NO)", "Create the TCP socket.");
	cJSON_AddStringToObject(responseObject, "DESTROY_SOCKET(INT PORT_NO)", "Destroy the TCP socket.");
	cJSON_AddStringToObject(responseObject, "SEND_DATA(string DATA, INT PORT_NO)", "Send data to the TCP socket.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
//	cJSON_AddStringToObject(responseObject, "START_TCP_SERVER()", "Start TCP server in AP mode.");
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


static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON = NULL;
	//char keyValue[100] = {0};
//	uint8_t temp[10];
    char str[200]={0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n TCP s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
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

			if ((tcp_srverpara_info[0].TCPSockRespFg ==1)||(tcp_srverpara_info[1].TCPSockRespFg ==1)
					||(tcp_srverpara_info[2].TCPSockRespFg ==1)||(tcp_srverpara_info[3].TCPSockRespFg ==1))
			{
				if (NET_status == E_wifi_CONNECTED_AP)
					{
					cJSON *TCP_RESP = cJSON_CreateObject();
					cJSON_AddStringToObject(TCP_RESP, "DATA", payLoadData);
					if(tcp_srverpara_info[0].TCPSockRespFg ==1){
					  cJSON_AddNumberToObject(TCP_RESP, "PORT_NO", tcp_srverpara_info[0].portNo);
					  tcp_srverpara_info[0].TCPSockRespFg =0;
					}
					else if(tcp_srverpara_info[1].TCPSockRespFg ==1){
					  cJSON_AddNumberToObject(TCP_RESP, "PORT_NO", tcp_srverpara_info[1].portNo);
					  tcp_srverpara_info[1].TCPSockRespFg =0;
					}
					else if(tcp_srverpara_info[2].TCPSockRespFg ==1){
					  cJSON_AddNumberToObject(TCP_RESP, "PORT_NO", tcp_srverpara_info[2].portNo);
					  tcp_srverpara_info[2].TCPSockRespFg =0;
					}
					else if(tcp_srverpara_info[3].TCPSockRespFg ==1){
					  cJSON_AddNumberToObject(TCP_RESP, "PORT_NO", tcp_srverpara_info[3].portNo);
					  tcp_srverpara_info[3].TCPSockRespFg =0;
					}
						s_Message_Rx->payload_p8[0] = '\0';
						cJSON_PrintPreallocated(TCP_RESP, (char*)s_Message_Rx->payload_p8, MAX_JSON_PAYLOAD_BYTES, false);
						tcp_server_send_data(s_Message_Rx);
					}
				return;
			 }
		}
		// Free the parsed JSON
		cJSON_Delete(in_JSON);
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
	 {
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
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
//				printf("NET_status:%s\n",(char*)name_JSON->valuestring);
//				strcpy((char*)temp,name_JSON->valuestring);
//				NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
//				printf("NET_status1:%d\n",NET_status);

				payLoadData[0] = '\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
						//printf("\n\n ETH EVENT 7 %s\n\n",filedata);
					if(TCP_Socket1task_Que!=NULL)
						xQueueSend(TCP_Socket1task_Que, payLoadData, QUE_DELAY);
					if(TCP_Socket2task_Que!=NULL)
						xQueueSend(TCP_Socket2task_Que, payLoadData, QUE_DELAY);
					if(TCP_Socket3task_Que!=NULL)
						xQueueSend(TCP_Socket3task_Que, payLoadData, QUE_DELAY);
					if(TCP_Socket4task_Que!=NULL)
						xQueueSend(TCP_Socket4task_Que, payLoadData, QUE_DELAY);
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
////					if(!strcmp(currentItem->valuestring, "System/TCP_SERVER.json"))
//					if(!strcasecmp(currentItem->valuestring, "System/TCP_SERVER.json"))
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
//	else
//		return;
//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_MGT")==0)
//		{
//			in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//			if(in_JSON == NULL)
//			{
//				return;
//			}
//			cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
//			if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"READ")==0))
//			{
//				cJSON *jmsg_array = cJSON_GetObjectItem(in_JSON, "RESPONSE");
//				if (jmsg_array != NULL && cJSON_IsArray(jmsg_array))
//				{
//					//printf("Items in the array:\n");
//					cJSON *json_item;
//					cJSON_ArrayForEach(json_item, jmsg_array)
//					{
//						if (json_item->type == cJSON_Object)
//						{
//							// Process each JSON object in the array
//							process_json_object(json_item);
//						}
//					}
//				}
//			}
//			cJSON_Delete(in_JSON);
//			return;
//		}


//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
//	{
//		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//		if(in_JSON == NULL)
//		{
//			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"Error in parsing JSON.");
//		    return;
//		}
//		if(strcmp(in_JSON->child->string,"JMSG")==0)
//	    {
//			cJSON *jmsg_array = cJSON_GetObjectItem(in_JSON, "JMSG");
//			if (jmsg_array != NULL && cJSON_IsArray(jmsg_array))
//			{
//				//printf("Items in the array:\n");
//				cJSON *json_item;
//				cJSON_ArrayForEach(json_item, jmsg_array)
//				{
//					if (json_item->type == cJSON_Object)
//					{
//						// Process each JSON object in the array
//						process_json_object(json_item);
//					}
////					else
////						console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"Error: Unexpected item type in the array.");
//				}
//			}
////			else
////				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"Error: 'JMSG' is not an array or doesn't exist in the JSON.");
//		 }
//		cJSON_Delete(in_JSON);
//	}
//	return;
}



static void do_retransmit(int sock, AMessage_st* s_Message_Rx)
{
    int len;
    char rx_buffer[512];
    char print_buffer[600];
//	char line[100] = {0};
    int i;
//    do {
//    	printf("\nd o_retransmit \n");
 //   printf("sock:%d\n",sock);
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
//            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
 //           console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during receiving");
 //           Add_Response_msg("Error occurred during receiving",s_Message_Rx);

        } else if (len == 0) {
//            ESP_LOGW(TAG, "Connection closed");
//            console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Connection closed");
            Add_Response_msg("Connection closed",s_Message_Rx, payLoadData);
            for(i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
			{
				if((tcp_srverpara_info[i].sock == sock ) && (tcp_srverpara_info[i].sock>=0))
				{
					tcp_srverpara_info[i].states = TCP_SERVER_SOCKET_CLOSED;
				}
			}
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
//            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);
            sprintf(print_buffer,"Received %d bytes: %s", len, rx_buffer);
            //            console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
            Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);

            for(i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
			{
				if((tcp_srverpara_info[i].sock == sock ) && (tcp_srverpara_info[i].sock>=0))
				{
					tcp_srverpara_info[i].TCPSockRespFg = 1;
				}
			}

            cJSON *Message = cJSON_Parse(rx_buffer);
            	 if(Message == NULL)
            	 {
            		 Add_Response_msg("No response received from server",s_Message_Rx, payLoadData);
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
        				//printf("Element %d: %s\n", i, element->valuestring);
        				/* execute the command */
        				char line[2048] = {0};
        				strcpy(line, element->valuestring);
        				int ret;
        				if(line[0]=='<')
        				{
        					esp_err_t err = esp_console_run_Custom(line, &ret, THIS_ACTOR);
        					if (err == ESP_ERR_NOT_FOUND) {
        						printf("Unrecognized command\n");
        					} else if (err == ESP_ERR_INVALID_ARG) {
        						// command was empty
        					} else if (err == ESP_OK && ret != ESP_OK) {
        						printf("Command returned non-zero error code: 0x%x (%s)\n", ret,
        						esp_err_to_name(ret));
        					} else if (err != ESP_OK) {
        						printf("Internal error: %s\n", esp_err_to_name(err));
        					}
        					//vTaskDelay(300/portTICK_PERIOD_MS);
        				}
        			}
        			else
        			{
        				printf("Element %d: %s\n", i, element->string);
        			}
        			// vTaskDelay(100/ portTICK_PERIOD_MS);
        		}
                cJSON_Delete(Message);
                return;
            }

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
//            int to_write = len;
//            while (to_write > 0) {
//                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
//                if (written < 0) {
////                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
////                    console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during sending");
//                    Add_Response_msg("Error occurred during sending",s_Message_Rx);
//                    // Failed to retransmit, giving up
//                    return;
//                }
//                to_write -= written;
//            }
        }
 //   } while (len > 0);
}

static void tcp_server_task_1(void *pvParameters)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
	char str[2000] = {0}, payLoadData_task1[100] = {0};
    char addr_str[128];
    int addr_family = AF_INET; //(int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;
    char print_buffer[300];

    char *buffer = (char*) heap_caps_calloc(4096, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    if (buffer == NULL) {
//        printf("Memory allocation failed\n");
    	Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData_task1);
        goto exit;
//	        printf("Memory allocation failed\n");
  //      return; // Return an error code
    }

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
     //   dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(tcp_srverpara_info[0].portNo);
        dest_addr_ip4->sin_addr.s_addr = inet_addr("10.10.100.254"); // Set IP address

        ip_protocol = IPPROTO_IP;
    }

	cJSON *my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "NET_STATUS", "");
	payLoadData_task1[0] = '\0';
	cJSON_PrintPreallocated(my_JSON, payLoadData_task1, sizeof(payLoadData_task1), false);	
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_task1, strlen(payLoadData_task1), "GET");
	cJSON_Delete(my_JSON);


    	if (pdTRUE == xQueueReceive(TCP_Socket1task_Que, (void*)buffer, portMAX_DELAY))
		{
    		root_new 	  = cJSON_Parse((char*)buffer);
			 if (root_new == NULL)
			 {
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_task1);
				goto exit;
			 }
				name_new_JSON  = cJSON_GetObjectItem(root_new, "NET_STATUS");  // for SQL
					if((name_new_JSON != NULL)&& cJSON_IsString(name_new_JSON))
						NET_status = atoi(name_new_JSON->valuestring);
			//				printf("file_size:%d\n",file_size);
					cJSON_Delete(root_new);

			if(NET_status != E_wifi_CONNECTED_AP)
				goto exit;

			tcp_srverpara_info[0].listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
		//	printf("\n listen_sock %d  addr_family: %d SOCK_STREAM: %d ip_protocol: %d\n",tcp_srverpara_info[0].listen_sock,addr_family,SOCK_STREAM,ip_protocol);
			if (tcp_srverpara_info[0].listen_sock < 0) {
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to create socket, Try creating socket again");
				Add_Response_msg("Unable to create socket, Try creating socket again", s_Message_Rx, payLoadData_task1);
				goto exit;
			}
			int opt = 1;
			setsockopt(tcp_srverpara_info[0].listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

			Add_Response_msg("Socket created", s_Message_Rx, payLoadData_task1);
			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket created");
		//    printf("\n Socket created \n");
			int err = bind(tcp_srverpara_info[0].listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0) {
				Add_Response_msg("Socket unable to bind", s_Message_Rx, payLoadData_task1);
				goto exit;
			}
			sprintf(print_buffer,"Socket bound, port %d", tcp_srverpara_info[0].portNo);
			Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task1);
			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
		//    printf("\n Socket bound, port %d", PORT);

			err = listen(tcp_srverpara_info[0].listen_sock, 1);
			if (err != 0) {
			  //  console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during listen");
				Add_Response_msg("Error occurred during listen", s_Message_Rx, payLoadData_task1);
				goto exit;

			}

  			Add_Response_msg("Socket listening", s_Message_Rx, payLoadData_task1);
  			tcp_srverpara_info[0].states = TCP_SERVER_SOCKET_LISTEN;
  			struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
  			socklen_t addr_len = sizeof(source_addr);

  			tcp_srverpara_info[0].sock = accept(tcp_srverpara_info[0].listen_sock, (struct sockaddr *)&source_addr, &addr_len);
 // 	        printf("\n sock %d \n",tcp_srverpara_info[0].sock );
  			if (tcp_srverpara_info[0].sock  < 0) {
  				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to accept connection");
  				Add_Response_msg("Unable to accept connection", s_Message_Rx, payLoadData_task1);
  				goto exit;
  			}

  			// Set tcp keepalive option
  			setsockopt(tcp_srverpara_info[0].sock , SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
  			setsockopt(tcp_srverpara_info[0].sock , IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
  			setsockopt(tcp_srverpara_info[0].sock , IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
  			setsockopt(tcp_srverpara_info[0].sock , IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
  			// Convert ip address to string
  			if (source_addr.ss_family == AF_INET) {
  				inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
  			}
  		   sprintf(print_buffer,"Socket accepted ip address: %s port %d", addr_str, tcp_srverpara_info[0].portNo);
  		   tcp_srverpara_info[0].states = TCP_SERVER_SOCKET_CONNECTED;
  //		    console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
  			Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task1);
 // 	        printf("\n Socket accepted ip address: %s %d", addr_str,tcp_srverpara_info[0].sock);
    while(1)
    {
			do_retransmit(tcp_srverpara_info[0].sock ,s_Message_Rx);

			if(u8Socket1DestroyFg==1)
			{
				goto exit;
			}

    	vTaskDelay(10 / portTICK_PERIOD_MS);
    }  // end of  while ((1) && (execute_exception==0))

}  //end of ifif (pdTRUE == xQueueReceive(TCP_Socket1task_Que, (void*)buffer, portMAX_DELAY))

exit:

	if(u8Socket1DestroyFg !=1)
	Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_task1);

	u8Socket1DestroyFg=0;
	if(tcp_srverpara_info[0].listen_sock>=0)
		{
			close(tcp_srverpara_info[0].listen_sock);
			tcp_srverpara_info[0].listen_sock = -2;
			printf("\n closing listen_sock %d\n",tcp_srverpara_info[0].listen_sock );
		}

		tcp_srverpara_info[0].portNo = 0;
		if(tcp_srver_sock_instance>0)
		tcp_srver_sock_instance--;

	if(s_Message_Rx->payload_p8 != NULL)
	{
		cJSON_free(s_Message_Rx->payload_p8);
		s_Message_Rx->payload_p8 = NULL;
	}
	if(s_Message_Rx!=NULL)
	{
		free(s_Message_Rx);
		s_Message_Rx = NULL;
	}
	if(TCP_Socket1task_Que != NULL)
	vQueueDelete(TCP_Socket1task_Que);
	TCP_Socket1task_Que = NULL;
	TCP_Server_Listen_Handle_1 =NULL;
	vTaskDelete(TCP_Server_Listen_Handle_1);  // Delete the task

}
static void tcp_server_task_2(void *pvParameters)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
	char str[2000] = {0}, payLoadData_task2[100] = {0};
    char addr_str[128];
    int addr_family = AF_INET; //(int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;
    char print_buffer[300];

    char *buffer = (char*) heap_caps_calloc(4096, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

        if (buffer == NULL) {
//            printf("Memory allocation failed\n");
        	Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData_task2);
            goto exit;
    //	        printf("Memory allocation failed\n");
         //   return; // Return an error code
        }

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
      //  dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(tcp_srverpara_info[1].portNo);
        dest_addr_ip4->sin_addr.s_addr = inet_addr("10.10.100.254"); // Set IP address
        ip_protocol = IPPROTO_IP;
    }
    cJSON *my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "NET_STATUS", "");
	payLoadData_task2[0] = '\0';
	cJSON_PrintPreallocated(my_JSON, payLoadData_task2, sizeof(payLoadData_task2), false);	
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_task2, strlen(payLoadData_task2), "GET");
	cJSON_Delete(my_JSON);

	if (pdTRUE == xQueueReceive(TCP_Socket2task_Que, (void*)buffer, portMAX_DELAY))
			{
	    		root_new 	  = cJSON_Parse((char*)buffer);
				 if (root_new == NULL)
				 {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData_task2);
					goto exit;
				 }
					name_new_JSON  = cJSON_GetObjectItem(root_new, "NET_STATUS");  // for SQL
						if((name_new_JSON != NULL)&& cJSON_IsString(name_new_JSON))
							NET_status = atoi(name_new_JSON->valuestring);
				//				printf("file_size:%d\n",file_size);
						cJSON_Delete(root_new);

				if(NET_status != E_wifi_CONNECTED_AP)
					goto exit;

			tcp_srverpara_info[1].listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
			if (tcp_srverpara_info[1].listen_sock < 0) {
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to create socket, Try creating socket again");
				Add_Response_msg("Unable to create socket, Try creating socket again", s_Message_Rx, payLoadData_task2);
				goto exit;
			}
			int opt = 1;
			setsockopt(tcp_srverpara_info[1].listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket created");
			Add_Response_msg("Socket created", s_Message_Rx, payLoadData_task2);
			//    printf("\n Socket created \n");
			int err = bind(tcp_srverpara_info[1].listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0) {
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket unable to bind");
				Add_Response_msg("Socket unable to bind", s_Message_Rx, payLoadData_task2);
				goto exit;
			}
			sprintf(print_buffer,"Socket bound, port %d", tcp_srverpara_info[1].portNo);
			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
			Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task2);
			//    printf("\n Socket bound, port %d", PORT);

			err = listen(tcp_srverpara_info[1].listen_sock, 1);
			if (err != 0) {
				Add_Response_msg("Error occurred during listen", s_Message_Rx, payLoadData_task2);
				goto exit;
			}

	        Add_Response_msg("Socket listening", s_Message_Rx, payLoadData_task2);
			tcp_srverpara_info[1].states = TCP_SERVER_SOCKET_LISTEN;
			struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
			socklen_t addr_len = sizeof(source_addr);
			tcp_srverpara_info[1].sock = accept(tcp_srverpara_info[1].listen_sock, (struct sockaddr *)&source_addr, &addr_len);
			if (tcp_srverpara_info[1].sock < 0) {
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to accept connection");
				Add_Response_msg("Unable to accept connection", s_Message_Rx, payLoadData_task2);
				  goto exit;
			}

			// Set tcp keepalive option
			setsockopt(tcp_srverpara_info[1].sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
			setsockopt(tcp_srverpara_info[1].sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
			setsockopt(tcp_srverpara_info[1].sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
			setsockopt(tcp_srverpara_info[1].sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
			// Convert ip address to string

			if (source_addr.ss_family == AF_INET) {
				inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
			}

			sprintf(print_buffer,"Socket accepted ip address: %s port %d", addr_str, tcp_srverpara_info[1].portNo);
					tcp_srverpara_info[1].states = TCP_SERVER_SOCKET_CONNECTED;
			   //     console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
					Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task2);
			//        printf("\n Socket accepted ip address: %s", addr_str);

    while(1)
    {

			do_retransmit(tcp_srverpara_info[1].sock,s_Message_Rx);

			if(u8Socket2DestroyFg==1)
			{
			   goto exit;
			}


        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}  //end of ifif (pdTRUE == xQueueReceive(TCP_Socket2task_Que, (void*)buffer, portMAX_DELAY))

exit:

	if(u8Socket2DestroyFg !=1)
	Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_task2);

	u8Socket2DestroyFg=0;
	if(tcp_srverpara_info[1].listen_sock>=0)
		{
			close(tcp_srverpara_info[1].listen_sock);
			tcp_srverpara_info[1].listen_sock = -2;
			printf("\n closing listen_sock %d\n",tcp_srverpara_info[1].listen_sock );
		}

		tcp_srverpara_info[1].portNo = 0;
		if(tcp_srver_sock_instance>0)
		tcp_srver_sock_instance--;

	if(s_Message_Rx->payload_p8 != NULL)
	{
		cJSON_free(s_Message_Rx->payload_p8);
		s_Message_Rx->payload_p8 = NULL;
	}
	if(s_Message_Rx!=NULL)
	{
		free(s_Message_Rx);
		s_Message_Rx = NULL;
	}
	if(TCP_Socket2task_Que != NULL)
	vQueueDelete(TCP_Socket2task_Que);
	TCP_Socket2task_Que = NULL;
	TCP_Server_Listen_Handle_2 =NULL;
	vTaskDelete(TCP_Server_Listen_Handle_2);  // Delete the task

}
static void tcp_server_task_3(void *pvParameters)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
	char str[2000] = {0}, payLoadData_task3[100] = {0};
    char addr_str[128];
    int addr_family = AF_INET; //(int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;
    char print_buffer[300];

    char *buffer = (char*) heap_caps_calloc(4096, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

            if (buffer == NULL) {
//                printf("Memory allocation failed\n");
            	Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData_task3);
                goto exit;
        //	        printf("Memory allocation failed\n");
        //        return; // Return an error code
            }

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
       // dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(tcp_srverpara_info[2].portNo);
        dest_addr_ip4->sin_addr.s_addr = inet_addr("10.10.100.254"); // Set IP address
        ip_protocol = IPPROTO_IP;
    }
	cJSON *my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "NET_STATUS", "");
	payLoadData_task3[0] = '\0';
	cJSON_PrintPreallocated(my_JSON, payLoadData_task3, sizeof(payLoadData_task3), false);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_task3, strlen(payLoadData_task3), "GET");
	cJSON_Delete(my_JSON);

	if (pdTRUE == xQueueReceive(TCP_Socket3task_Que, (void*)buffer, portMAX_DELAY))
			{
				root_new 	  = cJSON_Parse((char*)buffer);
				 if (root_new == NULL)
				 {
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx, payLoadData_task3);
					goto exit;
				 }
					name_new_JSON  = cJSON_GetObjectItem(root_new, "NET_STATUS");  // for SQL
						if((name_new_JSON != NULL)&& cJSON_IsString(name_new_JSON))
							NET_status = atoi(name_new_JSON->valuestring);
				//				printf("file_size:%d\n",file_size);
						cJSON_Delete(root_new);

				if(NET_status != E_wifi_CONNECTED_AP)
					goto exit;

	tcp_srverpara_info[2].listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
    if (tcp_srverpara_info[2].listen_sock < 0) {
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to create socket, Try creating socket again");
    	Add_Response_msg("Unable to create socket, Try creating socket again", s_Message_Rx, payLoadData_task3);
    	goto exit;
    }
    int opt = 1;
    setsockopt(tcp_srverpara_info[2].listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));


   // console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket created");
    Add_Response_msg("Socket created", s_Message_Rx, payLoadData_task3);
//    printf("\n Socket created \n");
    int err = bind(tcp_srverpara_info[2].listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
 //       console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket unable to bind");
        Add_Response_msg("Socket unable to bind", s_Message_Rx, payLoadData_task3);
        goto exit;
    }
    sprintf(print_buffer,"Socket bound, port %d", tcp_srverpara_info[2].portNo);
    Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task3);

    err = listen(tcp_srverpara_info[2].listen_sock, 1);
    if (err != 0) {
 //       console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during listen");
        Add_Response_msg("Error occurred during listen", s_Message_Rx, payLoadData_task3);
        goto exit;
    }

    //        console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket listening");
    			Add_Response_msg("Socket listening", s_Message_Rx, payLoadData_task3);
    			tcp_srverpara_info[2].states = TCP_SERVER_SOCKET_LISTEN;
    			struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
    			socklen_t addr_len = sizeof(source_addr);
    			tcp_srverpara_info[2].sock = accept(tcp_srverpara_info[2].listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    			if (tcp_srverpara_info[2].sock < 0) {
    	//        	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to accept connection");
    				Add_Response_msg("Unable to accept connection", s_Message_Rx, payLoadData_task3);
    	            goto exit;
    			}

    			// Set tcp keepalive option
    			setsockopt(tcp_srverpara_info[2].sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    			setsockopt(tcp_srverpara_info[2].sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    			setsockopt(tcp_srverpara_info[2].sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    			setsockopt(tcp_srverpara_info[2].sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
    			// Convert ip address to string
    			if (source_addr.ss_family == AF_INET) {
    				inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    			}

    			sprintf(print_buffer,"Socket accepted ip address: %s port %d", addr_str, tcp_srverpara_info[2].portNo);
    			tcp_srverpara_info[2].states = TCP_SERVER_SOCKET_CONNECTED;
    	//        console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
    			Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task3);
    	//        printf("\n Socket accepted ip address: %s", addr_str);

    while(1)
    {
			do_retransmit(tcp_srverpara_info[2].sock,s_Message_Rx);

			if(u8Socket3DestroyFg==1)
			{
				goto exit;
			}

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }

}  //end of ifif (pdTRUE == xQueueReceive(TCP_Socket3task_Que, (void*)buffer, portMAX_DELAY))

exit:
	if(u8Socket3DestroyFg !=1)
	Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_task3);

	u8Socket3DestroyFg=0;
	if(tcp_srverpara_info[2].listen_sock>=0)
		{
			close(tcp_srverpara_info[2].listen_sock);
			tcp_srverpara_info[2].listen_sock = -2;
			printf("\n closing listen_sock %d\n",tcp_srverpara_info[2].listen_sock );
		}

		tcp_srverpara_info[2].portNo = 0;
		if(tcp_srver_sock_instance>0)
		tcp_srver_sock_instance--;

	if(s_Message_Rx->payload_p8 != NULL)
	{
		cJSON_free(s_Message_Rx->payload_p8);
		s_Message_Rx->payload_p8 = NULL;
	}
	if(s_Message_Rx!=NULL)
	{
		free(s_Message_Rx);
		s_Message_Rx = NULL;
	}
	if(TCP_Socket3task_Que != NULL)
	vQueueDelete(TCP_Socket3task_Que);
	TCP_Socket3task_Que = NULL;
	TCP_Server_Listen_Handle_3 =NULL;
	vTaskDelete(TCP_Server_Listen_Handle_3);  // Delete the task

}
static void tcp_server_task_4(void *pvParameters)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	cJSON *root_new 	= NULL;
	cJSON *name_new_JSON 	= NULL;
	char str[2000] = {0}, payLoadData_task4[100] = {0};
    char addr_str[128];
//    int count = 0, display_msg = 0;
    int addr_family = AF_INET; //(int)pvParameters;
    int ip_protocol = 0;
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    struct sockaddr_storage dest_addr;
    char print_buffer[300];
//    char execute_exception = 0;

    char *buffer = (char*) heap_caps_calloc(4096, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

			if (buffer == NULL) {
//				printf("Memory allocation failed\n");
				Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx, payLoadData_task4);
				goto exit;
		//	        printf("Memory allocation failed\n");
		//        return; // Return an error code
			}

    if (addr_family == AF_INET) {
        struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
        //dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
        dest_addr_ip4->sin_family = AF_INET;
        dest_addr_ip4->sin_port = htons(tcp_srverpara_info[3].portNo);
        dest_addr_ip4->sin_addr.s_addr = inet_addr("10.10.100.254"); // Set IP address
        ip_protocol = IPPROTO_IP;
    }
	cJSON *my_JSON  	= cJSON_CreateObject();
	cJSON_AddStringToObject(my_JSON, "NET_STATUS", "");
	payLoadData_task4[0] = '\0';
	cJSON_PrintPreallocated(my_JSON, payLoadData_task4, sizeof(payLoadData_task4), false);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_task4, strlen(payLoadData_task4), "GET");
	cJSON_Delete(my_JSON);

	if (pdTRUE == xQueueReceive(TCP_Socket4task_Que, (void*)buffer, portMAX_DELAY))
		{
			root_new 	  = cJSON_Parse((char*)buffer);
			 if (root_new == NULL)
			 {
				sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
				Add_Response_msg(str,s_Message_Rx, payLoadData_task4);
				goto exit;
			 }
				name_new_JSON  = cJSON_GetObjectItem(root_new, "NET_STATUS");  // for SQL
					if((name_new_JSON != NULL)&& cJSON_IsString(name_new_JSON))
						NET_status = atoi(name_new_JSON->valuestring);
			//				printf("file_size:%d\n",file_size);
					cJSON_Delete(root_new);

			 if(NET_status != E_wifi_CONNECTED_AP)
				goto exit;

			tcp_srverpara_info[3].listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
			if (tcp_srverpara_info[3].listen_sock < 0) {
			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to create socket, Try creating socket again");
			Add_Response_msg("Unable to create socket, Try creating socket again", s_Message_Rx, payLoadData_task4);
			goto exit;
		}
		int opt = 1;
		setsockopt(tcp_srverpara_info[3].listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

	//    console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket created");
		Add_Response_msg("Socket created", s_Message_Rx, payLoadData_task4);
	//    printf("\n Socket created \n");
		int err = bind(tcp_srverpara_info[3].listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
		if (err != 0) {
	 //       console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket unable to bind");
			Add_Response_msg("Socket unable to bind", s_Message_Rx, payLoadData_task4);
			goto exit;
		}
		sprintf(print_buffer,"Socket bound, port %d", tcp_srverpara_info[3].portNo);
	//    console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
		Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task4);
	//    printf("\n Socket bound, port %d", PORT);

		err = listen(tcp_srverpara_info[3].listen_sock, 1);
		if (err != 0) {
	//        console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during listen");
			Add_Response_msg("Error occurred during listen", s_Message_Rx, payLoadData_task4);
			goto exit;
		}

		      Add_Response_msg("Socket listening", s_Message_Rx, payLoadData_task4);
				tcp_srverpara_info[3].states = TCP_SERVER_SOCKET_LISTEN;
				struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
				socklen_t addr_len = sizeof(source_addr);
				tcp_srverpara_info[3].sock = accept(tcp_srverpara_info[3].listen_sock, (struct sockaddr *)&source_addr, &addr_len);
				if (tcp_srverpara_info[3].sock < 0) {
					//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Unable to accept connecti"Unable to accept connection"on");
					Add_Response_msg("Unable to accept connection", s_Message_Rx, payLoadData_task4);
					goto exit;
				}

				// Set tcp keepalive option
				setsockopt(tcp_srverpara_info[3].sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
				setsockopt(tcp_srverpara_info[3].sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
				setsockopt(tcp_srverpara_info[3].sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
				setsockopt(tcp_srverpara_info[3].sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));
				// Convert ip address to string
				if (source_addr.ss_family == AF_INET) {
					inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
				}
				sprintf(print_buffer,"Socket accepted ip address: %s port %d", addr_str, tcp_srverpara_info[3].portNo);
				tcp_srverpara_info[3].states = TCP_SERVER_SOCKET_CONNECTED;
			   // console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
				Add_Response_msg(print_buffer, s_Message_Rx, payLoadData_task4);
		//        printf("\n Socket accepted ip address: %s", addr_str);

    while(1)
    {
			do_retransmit(tcp_srverpara_info[3].sock,s_Message_Rx);

			if(u8Socket4DestroyFg==1)
			{
				goto exit;
			}

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
		}  //end of ifif (pdTRUE == xQueueReceive(TCP_Socket3task_Que, (void*)buffer, portMAX_DELAY))

exit:
	if(u8Socket4DestroyFg !=1)
	Add_Response_msg("Kindly connect to internet at first.",s_Message_Rx, payLoadData_task4);

	u8Socket4DestroyFg=0;
	if(tcp_srverpara_info[3].listen_sock>=0)
		{
			close(tcp_srverpara_info[3].listen_sock);
			tcp_srverpara_info[3].listen_sock = -2;
			printf("\n closing listen_sock %d\n",tcp_srverpara_info[3].listen_sock );
		}

		tcp_srverpara_info[3].portNo = 0;
		if(tcp_srver_sock_instance>0)
		tcp_srver_sock_instance--;

	if(s_Message_Rx->payload_p8 != NULL)
	{
		cJSON_free(s_Message_Rx->payload_p8);
		s_Message_Rx->payload_p8 = NULL;
	}
	if(s_Message_Rx!=NULL)
	{
		free(s_Message_Rx);
		s_Message_Rx = NULL;
	}
	if(TCP_Socket4task_Que != NULL)
	vQueueDelete(TCP_Socket4task_Que);
	TCP_Socket4task_Que = NULL;
	TCP_Server_Listen_Handle_4 =NULL;
	vTaskDelete(TCP_Server_Listen_Handle_4);  // Delete the task

}

static void tcp_server_create_socket(AMessage_st* s_Message_Rx)
{
	char print_buffer[300];
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	uint16_t Input_portNo;
	char str[100]={0};

	int rc=0;
	//	printf("\n payload %s\n", payload);
	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  //  return 1;
		return;
	}
	//	printf("\n Host_IP: %s\n", tcp_para_info[tcp_socket_instance].host_ip);
	name_JSON = cJSON_GetObjectItem(in_JSON, "PORT_NO");
	Input_portNo  = (uint16_t) name_JSON->valueint;
	//	printf("\n Port_No: %d\n", tcp_para_info[tcp_socket_instance].portNo);
	cJSON_Delete(in_JSON);

	if(tcp_srver_sock_instance<MAX_TCP_SRVER_SOCK_INSTANCE)
	{
		if(tcp_srver_sock_instance>0)
		{
			for(int i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
			{
				if(tcp_srverpara_info[i].portNo == Input_portNo )
				{
					//already port occupied
					rc= 1;
				}

			}
		}
		if(rc==0)
		{
			switch(tcp_srver_sock_instance)
			{
				case 0:
					tcp_srverpara_info[0].portNo = Input_portNo;
					if(TCP_Socket1task_Que == NULL)
					{
						TCP_Socket1task_Que = xQueueCreateStatic(10, 1000, Sock1_pucQueueStorage, &Sock1_pxQueueBuffer);
					}
//					TCP_Socket1task_Que = xQueueCreateInPSRAM(10, 1000, &Sock1_pucQueueStorage, &Sock1_pxQueueBuffer);


						if (TCP_Socket1task_Que == NULL)
							printf("ERROR(Socket1task_Que is not created)\n");

//						size_t stack_size1 = TCP_SOCKET1_TASK_STACK_DEPTH * sizeof(StackType_t);
//						StackType_t *xTaskStack1 = (StackType_t *)heap_caps_malloc(stack_size1,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//							if (xTaskStack1 == NULL) {
//								// Memory allocation failed
//#ifdef ENABLE_PRINT_MSG
//								printf( "Failed to allocate memory for the task stack");
//#endif
//								Add_Response_msg("Error! Failed to allocate memory for the task stack.",s_Message_Rx, payLoadData);
//								// Handle error
//								return;
//							}
							TCP_Server_Listen_Handle_1 = xTaskCreateStaticPinnedToCore(
									        tcp_server_task_1,                 // Task function
											"tcp_server_1",            // Task name
											TCP_SOCKET1_TASK_STACK_DEPTH,        // Stack size in words
											s_Message_Rx,                    // Task parameters (not used here)
											TCP_SOCKET1_TASK_PRIORITY,                       // Task priority
											xTaskStack1,              // Pointer to task stack (allocated in PSRAM)
											&xTCPSock1TaskBuffer,             // Pointer to task control block
											0
							);


						if (TCP_Server_Listen_Handle_1 == NULL) {
								printf("Failed to create task\n");
								// Handle error
							}

					//xTaskCreate(tcp_server_task_1, "tcp_server_1", 4096, s_Message_Rx, 5, &TCP_Server_Listen_Handle_1);
					tcp_srver_sock_instance++;
				break;
				case 1:
					tcp_srverpara_info[1].portNo = Input_portNo;

					if(TCP_Socket2task_Que == NULL)
					{
						TCP_Socket2task_Que = xQueueCreateStatic(10, 1000, Sock2_pucQueueStorage, &Sock2_pxQueueBuffer);
					}
					//	TCP_Socket2task_Que = xQueueCreateInPSRAM(5, 1000);
//					 TCP_Socket2task_Que = xQueueCreateInPSRAM(10, 1000, &Sock2_pucQueueStorage, &Sock2_pxQueueBuffer);

					if (TCP_Socket2task_Que == NULL)
						printf("ERROR(Socket1task_Que is not created)\n");

//					size_t stack_size2 = TCP_SOCKET2_TASK_STACK_DEPTH * sizeof(StackType_t);
//					StackType_t *xTaskStack2 = (StackType_t *)heap_caps_malloc(stack_size2,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//						if (xTaskStack2 == NULL) {
//							// Memory allocation failed
//#ifdef ENABLE_PRINT_MSG
//							printf( "Failed to allocate memory for the task stack");
//#endif
//							Add_Response_msg("Error! Failed to allocate memory for the task stack.",s_Message_Rx, payLoadData);
//							// Handle error
//							return;
//						}
						TCP_Server_Listen_Handle_2 = xTaskCreateStaticPinnedToCore(
								        tcp_server_task_2,                 // Task function
										"tcp_server_2",            // Task name
										TCP_SOCKET2_TASK_STACK_DEPTH,        // Stack size in words
										s_Message_Rx,                    // Task parameters (not used here)
										TCP_SOCKET2_TASK_PRIORITY,                       // Task priority
										xTaskStack2,              // Pointer to task stack (allocated in PSRAM)
										&xTCPSock2TaskBuffer,             // Pointer to task control block
										0
						);


					if (TCP_Server_Listen_Handle_2 == NULL) {
							printf("Failed to create task\n");
							// Handle error
						}
					//xTaskCreate(tcp_server_task_2, "tcp_server_2", 4096, s_Message_Rx, 5, &TCP_Server_Listen_Handle_2);
					tcp_srver_sock_instance++;
					break;
				case 2:
					tcp_srverpara_info[2].portNo = Input_portNo;

					if(TCP_Socket3task_Que == NULL)
					{
						TCP_Socket3task_Que = xQueueCreateStatic(10, 1000, Sock3_pucQueueStorage, &Sock3_pxQueueBuffer);
					}
					//	TCP_Socket2task_Que = xQueueCreateInPSRAM(5, 1000);
//					 TCP_Socket3task_Que = xQueueCreateInPSRAM(10, 1000, &Sock3_pucQueueStorage, &Sock3_pxQueueBuffer);

					if (TCP_Socket3task_Que == NULL)
						printf("ERROR(Socket1task_Que is not created)\n");

//					size_t stack_size3 = TCP_SOCKET3_TASK_STACK_DEPTH * sizeof(StackType_t);
//					StackType_t *xTaskStack3 = (StackType_t *)heap_caps_malloc(stack_size3,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//						if (xTaskStack3 == NULL) {
//							// Memory allocation failed
//#ifdef ENABLE_PRINT_MSG
//							printf( "Failed to allocate memory for the task stack");
//#endif
//							Add_Response_msg("Error! Failed to allocate memory for the task stack.",s_Message_Rx, payLoadData);
//							// Handle error
//							return;
//						}
						TCP_Server_Listen_Handle_3 = xTaskCreateStaticPinnedToCore(
										tcp_server_task_3,                 // Task function
										"tcp_server_3",            // Task name
										TCP_SOCKET3_TASK_STACK_DEPTH,        // Stack size in words
										s_Message_Rx,                    // Task parameters (not used here)
										TCP_SOCKET3_TASK_PRIORITY,                       // Task priority
										xTaskStack3,              // Pointer to task stack (allocated in PSRAM)
										&xTCPSock3TaskBuffer,             // Pointer to task control block
										0
						);


					if (TCP_Server_Listen_Handle_3 == NULL) {
							printf("Failed to create task\n");
							// Handle error
						}
					//xTaskCreate(tcp_server_task_3, "tcp_server_3", 4096, s_Message_Rx, 5, &TCP_Server_Listen_Handle_3);
					tcp_srver_sock_instance++;
					break;
				case 3:
					tcp_srverpara_info[3].portNo = Input_portNo;

					if(TCP_Socket4task_Que == NULL)
					{
						TCP_Socket4task_Que = xQueueCreateStatic(10, 1000, Sock4_pucQueueStorage, &Sock4_pxQueueBuffer);
					}
					//	TCP_Socket2task_Que = xQueueCreateInPSRAM(5, 1000);
//					 TCP_Socket4task_Que = xQueueCreateInPSRAM(10, 1000, &Sock4_pucQueueStorage, &Sock4_pxQueueBuffer);

					if (TCP_Socket4task_Que == NULL)
						printf("ERROR(Socket1task_Que is not created)\n");

//					size_t stack_size4 = TCP_SOCKET3_TASK_STACK_DEPTH * sizeof(StackType_t);
//					StackType_t *xTaskStack4 = (StackType_t *)heap_caps_malloc(stack_size4,MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//						if (xTaskStack4 == NULL) {
//							// Memory allocation failed
//#ifdef ENABLE_PRINT_MSG
//							printf( "Failed to allocate memory for the task stack");
//#endif
//							Add_Response_msg("Error! Failed to allocate memory for the task stack.",s_Message_Rx, payLoadData);
//							// Handle error
//							return;
//						}
						TCP_Server_Listen_Handle_4 = xTaskCreateStaticPinnedToCore(
										tcp_server_task_4,                 // Task function
										"tcp_server_4",            // Task name
										TCP_SOCKET4_TASK_STACK_DEPTH,        // Stack size in words
										s_Message_Rx,                    // Task parameters (not used here)
										TCP_SOCKET4_TASK_PRIORITY,                       // Task priority
										xTaskStack4,              // Pointer to task stack (allocated in PSRAM)
										&xTCPSock4TaskBuffer,             // Pointer to task control block
										0
						);


					if (TCP_Server_Listen_Handle_4 == NULL) {
							printf("Failed to create task\n");
							// Handle error
						}

					//xTaskCreate(tcp_server_task_4, "tcp_server_4", 4096, s_Message_Rx, 5, &TCP_Server_Listen_Handle_4);
					tcp_srver_sock_instance++;
					break;
				default:
					break;
			}
		}
		else
			    Add_Response_msg("Port already occupied",s_Message_Rx, payLoadData);
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Port already occupied");



	}
	else
	{
	//	printf("\n TCP sockets created %d\n", tcp_socket_instance);
		sprintf(print_buffer,"MAX number %d of TCP Server sockets created", tcp_srver_sock_instance);
		 Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
	}

}
static void tcp_server_destroy_socket(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	uint16_t input_portNo;
	char print_buffer[150];
	char str[100]={0};

	int i=0,rc=MAX_TCP_SRVER_SOCK_INSTANCE+1;
//	printf("\n payload %s\n", payload);
	in_JSON = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData);
		  //  return 1;
		return;
	}
	else{
	name_JSON = cJSON_GetObjectItem(in_JSON, "PORT_NO");
	input_portNo = (uint16_t) name_JSON->valueint;
//	printf("\n input_portNo %d\n", input_portNo);
	cJSON_Delete(in_JSON);
	}



	if(tcp_srver_sock_instance>0)
	{
		for(i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
		{
			if(tcp_srverpara_info[i].portNo == input_portNo )
			{
				//found port
				rc = i;
//				printf("\n found port %d\n",rc);
				break;
			}

		}
//		printf("\n switch %d\n",rc);
		switch(rc)
		{
			case 0:
			{
				u8Socket1DestroyFg=1;
				//vTaskDelete(TCP_Server_Listen_Handle_1);
//				 printf("\n TCP Server sockets Destroyed Port_No: %d, instance %d", tcp_srverpara_info[rc].portNo,rc);
				sprintf(print_buffer,"TCP Server sockets Destroyed Port_No: %d, instance No  %d", tcp_srverpara_info[rc].portNo,rc);
				Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
//				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
//
				if(tcp_srverpara_info[0].sock>=0)
				{
//					printf("\n closing sock %d\n",tcp_srverpara_info[0].sock );
					shutdown(tcp_srverpara_info[0].sock , 0);
					close(tcp_srverpara_info[0].sock );
					tcp_srverpara_info[0].sock = -2;

				}
				if(tcp_srverpara_info[0].listen_sock>=0)
				{
//					printf("\n closing listen_sock %d\n",tcp_srverpara_info[0].listen_sock );
					close(tcp_srverpara_info[0].listen_sock);
					tcp_srverpara_info[0].listen_sock = -2;

				}

				tcp_srverpara_info[0].portNo = 0;
				tcp_srverpara_info[0].states = TCP_SERVER_SOCKET_OPEN;
				if(tcp_srver_sock_instance>0)
				tcp_srver_sock_instance--;
			}
			break;
			case 1:
			{
				u8Socket2DestroyFg=1;
				//vTaskDelete(TCP_Server_Listen_Handle_2);
				sprintf(print_buffer,"TCP Server sockets Destroyed Port_No: %d, instance No  %d", tcp_srverpara_info[rc].portNo,rc);
				Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
				if(tcp_srverpara_info[1].sock>=0)
				{
				//	printf("\n closing sock %d\n",tcp_srverpara_info[1].sock );
					shutdown(tcp_srverpara_info[1].sock , 0);
					close(tcp_srverpara_info[1].sock );
					tcp_srverpara_info[1].sock = -2;

				}
				if(tcp_srverpara_info[1].listen_sock>=0)
				{
//					printf("\n closing listen_sock %d\n",tcp_srverpara_info[1].listen_sock );
					close(tcp_srverpara_info[1].listen_sock);
					tcp_srverpara_info[1].listen_sock = -2;

				}

				tcp_srverpara_info[1].portNo = 0;
				tcp_srverpara_info[1].states = TCP_SERVER_SOCKET_OPEN;
				if(tcp_srver_sock_instance>0)
				tcp_srver_sock_instance--;
			}
				break;
			case 2:
			{
				u8Socket3DestroyFg=1;
				//vTaskDelete(TCP_Server_Listen_Handle_3);
				sprintf(print_buffer,"TCP Server sockets Destroyed Port_No: %d, instance No  %d", tcp_srverpara_info[rc].portNo,rc);
				Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);

				if(tcp_srverpara_info[2].sock>=0)
				{
//					printf("\n closing sock %d\n",tcp_srverpara_info[2].sock );
					shutdown(tcp_srverpara_info[2].sock , 0);
					close(tcp_srverpara_info[2].sock );
					tcp_srverpara_info[2].sock = -2;

				}
				if(tcp_srverpara_info[2].listen_sock>=0)
				{
//					printf("\n closing listen_sock %d\n",tcp_srverpara_info[2].listen_sock );
					close(tcp_srverpara_info[2].listen_sock);
					tcp_srverpara_info[2].listen_sock = -2;

				}

				tcp_srverpara_info[2].portNo = 0;
				tcp_srverpara_info[2].states = TCP_SERVER_SOCKET_OPEN;
				if(tcp_srver_sock_instance>0)
				tcp_srver_sock_instance--;
			}
				break;

			case 3:
			{
				u8Socket4DestroyFg=1;
				//vTaskDelete(TCP_Server_Listen_Handle_4);
				sprintf(print_buffer,"TCP Server sockets Destroyed Port_No: %d, instance No %d", tcp_srverpara_info[rc].portNo,rc);
				Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);

				if(tcp_srverpara_info[3].sock>=0)
				{
//					printf("\n closing sock %d\n",tcp_srverpara_info[3].sock );
					shutdown(tcp_srverpara_info[3].sock , 0);
					close(tcp_srverpara_info[3].sock );
					tcp_srverpara_info[3].sock = -2;

				}
				if(tcp_srverpara_info[3].listen_sock>=0)
				{
//					printf("\n closing listen_sock %d\n",tcp_srverpara_info[3].listen_sock );
					close(tcp_srverpara_info[3].listen_sock);
					tcp_srverpara_info[3].listen_sock = -2;

				}

				tcp_srverpara_info[3].portNo = 0;
				tcp_srverpara_info[3].states = TCP_SERVER_SOCKET_OPEN;
				if(tcp_srver_sock_instance>0)
				tcp_srver_sock_instance--;
			}
				break;
			default:
			{
//				printf("\n default\n");
				sprintf(print_buffer,"Port_No: %d, Not Found.",input_portNo );
				Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);
				//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
			}
				break;
		}
//		printf("\n END \n");

	}
	else
		Add_Response_msg("Created Socket instances list is empty",s_Message_Rx, payLoadData);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Created Socket instances list is empty");


}

static void tcp_server_send_data(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char print_buffer[300],rx_buffer[200];
//	int addr_family = 0;
//	int ip_protocol = 0;
	int rc=MAX_TCP_SRVER_SOCK_INSTANCE+1,i=0;
	uint16_t input_portNo;
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
	strcpy(rx_buffer,name_JSON->valuestring);
//	printf("\n Data Rx: %s\n", rx_buffer);
	name_JSON = cJSON_GetObjectItem(in_JSON, "PORT_NO");
	input_portNo = (uint16_t) name_JSON->valueint;
	cJSON_Delete(in_JSON);
	}

	if(tcp_srver_sock_instance>0)
		{
			for(i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
			{
				if(tcp_srverpara_info[i].portNo == input_portNo )
				{
					//found port
					rc = i;
	//				printf("\n found port %d\n",rc);
					break;
				}

			}
			if(rc<MAX_TCP_SRVER_SOCK_INSTANCE)
			{
				if(tcp_srverpara_info[rc].sock>0)
				{
					sprintf(print_buffer,"Bytes Received:%d Data: %s", strlen(rx_buffer), rx_buffer);
					//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
					Add_Response_msg(print_buffer,s_Message_Rx, payLoadData);

					// send() can return less bytes than supplied length.
					// Walk-around for robust implementation.
					int to_write = strlen(rx_buffer);
					while (to_write > 0) {
						int written = send(tcp_srverpara_info[rc].sock, rx_buffer + (strlen(rx_buffer) - to_write), to_write, 0);
						if (written < 0) {

							 Add_Response_msg("Error occurred during sending",s_Message_Rx, payLoadData);
							//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during sending");
							// Failed to retransmit, giving up
							return;
						}
						to_write -= written;
					}
				}
				else
					Add_Response_msg("Socket Closed",s_Message_Rx, payLoadData);
					// console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket Closed");
			}else
				Add_Response_msg("Socket Closed or Port Not found",s_Message_Rx, payLoadData);
				// console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Socket Closed or Port Not found");


	}
	else
		Add_Response_msg("Created Socket instances list is empty",s_Message_Rx, payLoadData);
		//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Created Socket instances list is empty");

}
static void tcp_server_get_sock_states(AMessage_st* s_Message_Rx)
{
//	char print_buffer[300];
	int i;
	char states_buf[10];
	char temp_buf[50];

	cJSON *root = NULL;
	root 	= cJSON_CreateObject();
	cJSON *out_JSON  = NULL;
	cJSON *name_JSON = NULL;
	//char *JSON_String;

	if(tcp_srver_sock_instance>0)
	{

		for(i=0;i<MAX_TCP_SRVER_SOCK_INSTANCE;i++)
		{
			switch(tcp_srverpara_info[i].states)
			{
			case TCP_SERVER_SOCKET_OPEN:
				strcpy(states_buf,"AVAILABLE");
				break;
			case TCP_SERVER_SOCKET_CLOSED:
				strcpy(states_buf,"CLOSED");
				break;
			case TCP_SERVER_SOCKET_LISTEN:
				strcpy(states_buf,"LISTENING");
			break;
			case TCP_SERVER_SOCKET_CONNECTED:
				strcpy(states_buf,"CONNECTED");
				break;
			default:
				strcpy(states_buf," ");
				break;

			}
//			sprintf(print_buffer,"TCP SERVER SOCKET %d- State:%s , Port No used:%d, Instance available:%d",(i+1),
//					states_buf,
//					tcp_srverpara_info[i].portNo,
//					(MAX_TCP_SRVER_SOCK_INSTANCE-tcp_srver_sock_instance));
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
//			TCP SERVER SOCKET 1- State:CONNECTED , Port No used:1004, Instance available:3

			out_JSON 	= cJSON_CreateObject();
//
			cJSON_AddStringToObject(out_JSON, "STATUS", states_buf);
			cJSON_AddNumberToObject(out_JSON, "PORT NO", tcp_srverpara_info[i].portNo);
			sprintf(temp_buf,"PORT%d",(i+1));
			name_JSON = cJSON_Duplicate(out_JSON, 1);
			cJSON_AddItemToObject(root, temp_buf, name_JSON);

			cJSON_Delete(out_JSON);


		}
		payLoadData[0] = '\0';
		cJSON_PrintPreallocated(root, payLoadData, sizeof(payLoadData), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(root);
		console_send_responce_to_console_xface(s_Message_Rx);

	}
	else
		     Add_Response_msg("Created Socket instances list is empty",s_Message_Rx, payLoadData);
			//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Created Socket instances list is empty");


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


