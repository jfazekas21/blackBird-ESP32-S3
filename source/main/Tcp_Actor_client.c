/*
 * Tcp_Actor.c
 *
 *  Created on: 31-Oct-2023
 *      Author: Sai
 */


#include "actor.h"
#include "Console_Actor.h"
#include "Tcp_Actor.h"
#include "sdkconfig.h"
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <netdb.h>            // struct addrinfo
#include <arpa/inet.h>
#include "esp_netif.h"
#include "esp_log.h"
#include "wifi_actor.h"


//#define PORT CONFIG_EXAMPLE_PORT
#define PORT 1001
#define PORT1 1002
#define PORT2 1003
#define PORT3 1004

//#define OBJ_QUE_COUNT                	2
static const char * THIS_ACTOR = "TCP";

BaseType_t tcpMonitor;
TaskHandle_t tcpHandle;
QueueHandle_t tcp_Rx_Queue, tcp_Tx_Queue;
static Actor_st tcp;                           // Tcp  Actor structure
static AMessage_st s_Message_Rx, s_Message_Tx;

static int FirsttcpEntry = 0;

static void init(void *a, void *b);                         //	Initialized Actor to default Values
static void set(char *property, char *value);			    //	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void print(char *str_buf);
static void help(void);
static void monitor(void *pvParameters __attribute__((unused)));
static void getAll(char *str_prop, char *val_a8);
static void Get_Property(const uint8_t* payload);
static void tcp_client_socket1(void);
static void tcp_create_socket(char* payload);
static void tcp_destroy_socket(char* payload);
static void tcp_client_listen(char* payload);
#define MAX_TCP_SOCKET_INSTANCE 4
struct tcp_parameter {
	uint16_t portNo;
	char host_ip[25];
	int sock;

} tcp_para,tcp_para_info[MAX_TCP_SOCKET_INSTANCE];
TaskHandle_t TCP_Client_Listen_Handle;
//struct tcp_parameter {
//	uint16_t portNo;
//	char host_ip[25];
//	int sock;
//
//} ;
static uint8_t tcp_socket_instance=0;


PSRAM_ATTR static struct property prop[] = 								//Actor Property
		{
			{ &tcp_para.portNo, "RED_DUTY", INT },
			{ &tcp_para.host_ip, "RED_DUTY", STRING },

		};

static Actor_st tcp = { prop, get, set, init, print, monitor, help,
		&tcp_Tx_Queue, &tcp_Rx_Queue, 0, 0, 0, getAll, 0, 0};
static const char *TAG = "example";
static const char *payload = "Message from ESP32 ";
static const char *payloadbuff = "Message Received by esp32 ";


static void set(char *property, char *value) {
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(property, prop[i].str_name)) {
			switch (prop[i].type) {

			case U_INT8:
				*(uint8_t*) prop[i].name = atoi(value);
				break;

			case U_INT16:
				*(uint16_t*) prop[i].name = atoi(value);
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
			return;
		}
	}
}//	set

static void get(char *str_prop, char *val_a8) {
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	uint32_t duty = 0;
	for (int i = 0; i < no_of_elements; i++) {
		if (!strcmp(str_prop, prop[i].str_name)) {
			switch (prop[i].type) {

			case U_INT8:
				sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
				break;

			case U_INT16:
				sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
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

static void getAll(char *str_prop, char *val_a8) {
	cJSON *out_JSON  = cJSON_CreateObject();
	uint32_t duty = 0;
	//no of elements
	int no_of_elements = sizeof(prop) / sizeof(struct property);
	for (int i = 0; i < no_of_elements; i++) {

		switch (prop[i].type) {
		case U_INT8:
			sprintf(val_a8, "%d", *(uint8_t*) prop[i].name);
			break;

		case U_INT16:
			sprintf(val_a8, "%d", *(uint16_t*) prop[i].name);
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

		cJSON_AddStringToObject(out_JSON, prop[i].str_name, val_a8);
	}

	cJSON_Delete(out_JSON);
	console_send_responce_to_console_xface(JSON_STR,THIS_ACTOR,out_JSON);
}

static void init(void *a, void *b) {

	if (FirsttcpEntry == 0){

		tcp_Tx_Queue = xQueueCreate(TCPAC_MON_TASK_TX_QUEUE_LENGTH, sizeof(s_Message_Tx));
		if (tcp_Tx_Queue == NULL) {
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP TX Queue is not created.");
		}

		tcp_Rx_Queue = xQueueCreate(TCPAC_MON_TASK_RX_QUEUE_LENGTH, sizeof(s_Message_Rx));
		if (tcp_Rx_Queue == NULL) {
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP RX Queue is not created.");
		}

		tcpMonitor = xTaskCreate(monitor, (const char*) "TCP Monitor", TCPAC_MON_TASK_STACK_DEPTH, NULL, TCPAC_MON_TASK_PRIORITY, &tcpHandle);
//		 xTaskCreate(tcp_client_socket1, "tcp_server1_task", 4096, NULL, 5, NULL);
		FirsttcpEntry = 1;
		for(int i=0;i<MAX_TCP_SOCKET_INSTANCE;i++)
		{
			tcp_para_info[i].portNo = 0;
			tcp_para_info[i].sock = -2;
//			printf("\n host_ip: %s sizeof(tcp_para_info[i].host_ip) %d\n",tcp_para_info[i].host_ip,sizeof(tcp_para_info[i].host_ip));
			memset(tcp_para_info[i].host_ip,0,sizeof(tcp_para_info[i].host_ip));
//			printf("\n host_ip: %s sizeof(tcp_para_info[i].host_ip) %d\n",tcp_para_info[i].host_ip,sizeof(tcp_para_info[i].host_ip));

		}
		tcp_socket_instance = 0;

		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP Initialization Done.");
	}

	else
		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP is already Initialization Done.");

}

static void monitor(void *pvParameters __attribute__((unused))) {

	cJSON *in_JSON;
	cJSON *out_JSON;
	cJSON *root = NULL;
	cJSON *name = NULL;
	uint8_t *val_p8  = NULL;
	char keysTemp[MAX_OUT_SIZE];
	char keysValue[MAX_OUT_SIZE];

	while (1) {
		if (pdTRUE == xQueueReceive(tcp_Tx_Queue, (void*) (&s_Message_Tx), //Tcp Tx queue Monitor
				QUE_DELAY))
		{
//				printf ("Tcp_Tx_DBG:>M_ID:\t%d\r\nCmd:\t%s\r\nCOUNT:\t%d\r\nSRC_Actor:\t%s\r\ndest_ACtor:\t%s\r\nARG:\t%s %s %s %s %s %s %s %s\r\n",s_MyMessage.ucMessageID,s_MyMessage.cmdFun_a8,s_MyMessage.payload_p8c,s_MyMessage.src_Actor, s_MyMessage.dest_Actor,s_MyMessage.payload_p8,s_MyMessage.payload_p8[1],s_MyMessage.payload_p8[2],s_MyMessage.payload_p8[3],s_MyMessage.payload_p8[4],s_MyMessage.payload_p8[5],s_MyMessage.payload_p8[6],s_MyMessage.payload_p8[7]);

//			xQueueSend(*(s_console.RxQueue), &s_MyMessage, 1000);
//			printf("\n TCP_Tx_Queue\n");
//			printf("Src = %s, Dest = %s, Command = %s, DT = %s\n\n", s_Message_Tx.src_Actor_a8,	s_Message_Tx.dest_Actor_a8, s_Message_Tx.cmdFun_a8,s_Message_Tx.payload_p8);

			console_ActorWriteToConsole_xface( &s_Message_Tx);
		}
		if (pdTRUE == xQueueReceive(tcp_Rx_Queue, (void*) (&s_Message_Rx), QUE_DELAY))
		{

//			printf("\n TCP_Rx_Queue\n");
		//	printf("\n TCP_Rx_Queue S = %s, D = %s, C = %s, DT = %s\n\n", s_Message_Rx.src_Actor_a8,	s_Message_Rx.dest_Actor_a8, s_Message_Rx.cmdFun_a8,s_Message_Rx.payload_p8);

			//printf ("TCP_Rx_DBG:>M_ID:\t%d\r\nCmd:\t%s\r\nCOUNT:\t%d\r\nSRC_Actor:\t%s\r\ndest_ACtor:\t%s\r\nARG:\t%s %s %s %s %s %s %s %s\r\n",s_MyMessage.ucMessageID,s_MyMessage.cmdFun_a8,s_MyMessage.payload_p8c,s_MyMessage.src_Actor, s_MyMessage.dest_Actor,s_MyMessage.payload_p8,s_MyMessage.payload_p8[1],s_MyMessage.payload_p8[2],s_MyMessage.payload_p8[3],s_MyMessage.payload_p8[4],s_MyMessage.payload_p8[5],s_MyMessage.payload_p8[6],s_MyMessage.payload_p8[7]);

			if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "INIT"))
			{   // INIT Properties

				if (FirsttcpEntry == 0)
				{
//					tcp.Init(0, 0);
				init(0,0);
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "INIT DONE.");
				}
			}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "GET")) // Get Actor Properties
				{
					Get_Property(s_Message_Rx.payload_p8);
				}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "SET"))
			{ // Set Actor Properties
				root = cJSON_Parse((char*)s_Message_Rx.payload_p8);
				name = root;
				cJSON *head = NULL;  //refer to head and tail as in linked list
				head = name->child;
				do {
					strcpy(keysTemp, head->string);
					strcpy(keysValue, head->valuestring);
					set(keysTemp, keysValue);
					head = head->next;
				} while (head != NULL);
				console_send_responce_to_console_xface(JSON_STR, THIS_ACTOR, name);
				cJSON_Delete(name);
			}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "HELP"))
			{
				tcp.help();
			}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "GETALL"))
			{
				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
				getAll(prop, (char*) val_p8);
				free(val_p8);
			}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "CREATE_SOCKET"))
			{
				if (wifi_status == WIFI_CONNECTED)
				{
					tcp_create_socket((const char*)s_Message_Rx.payload_p8);
				}
				else
				{
					console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "NETWORK NOT CONNECTED");
				}
			}
			else if (!strcmp((char*)s_Message_Rx.cmdFun_a8, "DESTROY_SOCKET"))
			{
				if (wifi_status == WIFI_CONNECTED)
				{
					tcp_destroy_socket((const char*)s_Message_Rx.payload_p8);
				}
				else
				{
					console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "NETWORK NOT CONNECTED");
				}
			}

			else
			{
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Command Function Not Found");
			}

			// Free s_MyMessage.payload_p8 here
			if(s_Message_Rx.payload_size!=0)
				console_MessageRelease_xface((char*)s_Message_Rx.payload_p8);
		}
		vTaskDelay(100 / portTICK_PERIOD_MS);
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
		if (state == errQUEUE_FULL)
		{
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP RX Queue is full.");
		}
		else
		{
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "TCP RX Queue send unsuccessful.");
		}
	}
}//	LED_ConsolWriteToActor

static void help(void) {
	printf(
			"||==============================================================||\r\n");
	printf(
			"||TCP.HELP()<CR>\t Prints all available methods and Properties--||\r\n");
	printf(
			"||TCP.SET(JSON)<CR>\t Assigns value to property-----------------||\r\n");
	printf(
			"||TCP.GET(JSON)<CR>\t returns property and its value in JSON----||\r\n");
	printf(
			"||TCP.INIT(JSON)<CR>\t Initialize TCP actor---------------------||\r\n");

}

static void Get_Property(const uint8_t* payload)
{
	uint8_t *val_p8  = NULL;
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;

	val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

	if (val_p8 == NULL)
	{
		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Payload is NULL in GET method");
		return;
	}

	in_JSON 	= cJSON_Parse((char*) payload);
	out_JSON 	= cJSON_CreateObject();
	head_JSON 	= in_JSON->child;

	//loop
	do {

		get(head_JSON->string, (char*) val_p8);
		cJSON_AddStringToObject(out_JSON, head_JSON->string, (char*) val_p8);
		head_JSON = head_JSON->next;
	} while (head_JSON != NULL);

	console_send_responce_to_console_xface(JSON_STR, THIS_ACTOR, out_JSON);
	cJSON_Delete(out_JSON);
	cJSON_Delete(head_JSON);
	cJSON_Delete(in_JSON);
	free(val_p8);
}

void tcp_create_socket(char* payload)
{

	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char print_buffer[150];
	int addr_family = 0;
	int ip_protocol = 0;
	int rc=0;

	if(tcp_socket_instance<MAX_TCP_SOCKET_INSTANCE)
	{
	//	printf("\n payload %s\n", payload);
		in_JSON = cJSON_Parse((char*) payload);
		name_JSON = cJSON_GetObjectItem(in_JSON, "Host_IP");
		strcpy(tcp_para_info[tcp_socket_instance].host_ip,name_JSON->valuestring);
	//	printf("\n Host_IP: %s\n", tcp_para_info[tcp_socket_instance].host_ip);
		name_JSON = cJSON_GetObjectItem(in_JSON, "Port_No");
		tcp_para_info[tcp_socket_instance].portNo = (uint16_t) name_JSON->valueint;
	//	printf("\n Port_No: %d\n", tcp_para_info[tcp_socket_instance].portNo);
		cJSON_Delete(in_JSON);
		struct sockaddr_in dest_addr;

		inet_pton(AF_INET, tcp_para_info[tcp_socket_instance].host_ip, &dest_addr.sin_addr);

		dest_addr.sin_family = AF_INET;
		dest_addr.sin_port = htons(tcp_para_info[tcp_socket_instance].portNo);

		addr_family = AF_INET;
		ip_protocol = IPPROTO_IP;
		if(tcp_socket_instance>0)
		{
			for(int i=0;i<MAX_TCP_SOCKET_INSTANCE;i++)
			{
				if(tcp_para_info[tcp_socket_instance-1].portNo == tcp_para_info[i].portNo )
				{
					//already port occupied
					rc= 1;
				}

			}
		}
		if(tcp_socket_instance==0)
		{
			xTaskCreate(tcp_client_listen, "tcp_server1_task", TCPAC_ClientListen_TASK_STACK_DEPTH, NULL, TCPAC_ClientListen_TASK_PRIORITY, &TCP_Client_Listen_Handle);
		}

		if((rc==0) && (tcp_socket_instance>0))
		{
			tcp_socket_instance--;
		}

		tcp_para_info[tcp_socket_instance].sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
		if (tcp_para_info[tcp_socket_instance].sock < 0) {
			sprintf(print_buffer,"Unable to create socket: Host_IP: %s Port_No: %d", tcp_para_info[tcp_socket_instance].host_ip,tcp_para_info[tcp_socket_instance].portNo);
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
//				printf("\n Unable to create socket");
			//break;
		}
		else
		{


//					printf("\n Socket created, connecting to %s:%d", tcp_para_info[tcp_socket_instance].host_ip, tcp_para_info[tcp_socket_instance].portNo);
			sprintf(print_buffer,"Socket created, connecting to Host_IP: %s Port_No: %d instance:%d", tcp_para_info[tcp_socket_instance].host_ip,tcp_para_info[tcp_socket_instance].portNo,(tcp_socket_instance+1));
			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);

			int err = connect(tcp_para_info[tcp_socket_instance].sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
			if (err != 0) {
//				printf("\n Socket unable to connect\n");
				sprintf(print_buffer,"Socket unable to connect to Host_IP: %s Port_No: %d instance:%d", tcp_para_info[tcp_socket_instance].host_ip,tcp_para_info[tcp_socket_instance].portNo,(tcp_socket_instance+1));
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
				//break;
			}
			else
			{
//				printf("\n Successfully connected with port\n");
				sprintf(print_buffer,"Successfully connected with Host_IP: %s Port_No: %d instance:%d", tcp_para_info[tcp_socket_instance].host_ip,tcp_para_info[tcp_socket_instance].portNo,(tcp_socket_instance+1));
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
			}
			tcp_socket_instance++;
//					printf("\n tcp_socket_instance %d\n", tcp_socket_instance);

		}


	}
	else
	{
	//	printf("\n TCP sockets created %d\n", tcp_socket_instance);
		sprintf(print_buffer,"MAX number %d of TCP sockets created", tcp_socket_instance);
		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
	}
}

void tcp_destroy_socket(char* payload)
{
	cJSON *in_JSON = NULL;
	cJSON *name_JSON = NULL;
	char input_host_ip[25];
	uint16_t input_portNo;
	char print_buffer[150];
	int i=0,rc=0;
//	printf("\n payload %s\n", payload);
	in_JSON = cJSON_Parse((char*) payload);
	name_JSON = cJSON_GetObjectItem(in_JSON, "Host_IP");
	strcpy(input_host_ip,name_JSON->valuestring);
//	printf("\n input_host_ip %s\n", input_host_ip);
	name_JSON = cJSON_GetObjectItem(in_JSON, "Port_No");
	input_portNo = (uint16_t) name_JSON->valueint;
//	printf("\n input_portNo %d\n", input_portNo);
	cJSON_Delete(in_JSON);

	for( i=0;i<MAX_TCP_SOCKET_INSTANCE;i++)
	{
		if((strcmp(tcp_para_info[i].host_ip,input_host_ip)==0)
				&& input_portNo == tcp_para_info[i].portNo )
		{

//			printf("\n Shutting down socket and restarting... \n");
			shutdown(tcp_para_info[i].sock, 0);
			close(tcp_para_info[i].sock);
			tcp_para_info[i].portNo = 0;
			tcp_para_info[i].sock = -2;
//			printf("\n host_ip: %s sizeof(tcp_para_info[i].host_ip) %d\n",tcp_para_info[i].host_ip,sizeof(tcp_para_info[i].host_ip));
			memset(tcp_para_info[i].host_ip,0,sizeof(tcp_para_info[i].host_ip));
//			printf("\n host_ip: %s sizeof(tcp_para_info[i].host_ip) %d\n",tcp_para_info[i].host_ip,sizeof(tcp_para_info[i].host_ip));
			if(tcp_socket_instance>0)
			{
				sprintf(print_buffer,"Shutting down socket with Host_IP: %s Port_No: %d instance:%d", input_host_ip,input_portNo,i);
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
				tcp_socket_instance--;
			}
			else
			{
				sprintf(print_buffer,"sockets already shutdown with Host_IP: %s Port_No: %d ", input_host_ip,input_portNo);
				console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
			}

			rc = 1;
		}

	}
	if(rc==0)
	{
//		printf("\n socket not found... \n");
		sprintf(print_buffer,"socket not found... Host_IP: %s Port_No: %d", input_host_ip,input_portNo);
		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
	}
	if(tcp_socket_instance==0)
	{
		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Task Deleted");
//		printf("\n Task Deleted \n");
		vTaskDelete(TCP_Client_Listen_Handle);
	}

}
void tcp_client_listen()
{
	int i=0,len=0;
	char rx_buffer[150],print_buffer[300];

//	printf("\n Successfully connected with port\n");
	while (1)
	{

		for( i=0;i<MAX_TCP_SOCKET_INSTANCE;i++)
		{
			if(tcp_para_info[i].sock>=0)
			{
				len = recv(tcp_para_info[i].sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
				ESP_LOGI(TAG, "len = %d", len);
//				printf("\n i=%d , len = %d sock=%d\n", i, len,tcp_para_info[i].sock);
				// Error occurred during receiving
				if (len < 0) {
					//printf("\n recv failed: \n");
					//break;
					;
				}else {    // Data received
					rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
//					printf("\n Received %d bytes from IP %s: PortNo: %d rx_buffer %s: \n", len, tcp_para_info[i].host_ip,tcp_para_info[i].portNo,rx_buffer);
					sprintf(print_buffer,"Received %d bytes from IP: %s PortNo: %d Data: %s ", len, tcp_para_info[i].host_ip,tcp_para_info[i].portNo,rx_buffer);
					console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, print_buffer);
					int err = send(tcp_para_info[i].sock, payloadbuff, strlen(payloadbuff), 0);
					if (err < 0) {
//						printf("\n Error occurred during sending \n");
						console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error occurred during sending");
						//break;
					}
				}
			}

		}

		vTaskDelay(100 / portTICK_PERIOD_MS);
	}
}
