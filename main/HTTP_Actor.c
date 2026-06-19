/*
 * HTTP_Actor.c
 *
 *  Created on: Jun 2, 2022
 *      Author: Shyam and Amruta
 */
#include "actor.h"
#include "Config.h"
#include "console_Actor.h"
#include "HTTP_Actor.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include <regex.h>
#include "pcf8563.h"
#define CONFIG_MBEDTLS_CERTIFICATE_BUNDLE 1
#define POLYNOMIAL 0xA001
#define INITIAL_VALUE 0xFFFF
#define RX_QUE_COUNT  200

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

//------------------------ HTTP Actor Resources -------------------------------------------------//
#define BOUNDARY               "----ESP32Boundary"

static void http_get_file(AMessage_st* s_Message_Rx);
static void http_get_request(AMessage_st* s_Message_Rx);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void http_get_key(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void Process_Post_Method_Response(AMessage_st* s_Message_Rx);
static void http_post_request(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer_data, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void periodic_timer_callback(void* arg);
static const esp_timer_create_args_t periodic_timer_args_new = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"
};
static esp_timer_handle_t periodic_timer = NULL;
static void start_periodic_timer(void);

extern const uint8_t server_root_cert_pem_start[] asm("_binary_server_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_server_root_cert_pem_end");
//extern const uint8_t local_server_cert_pem_start[] asm("_binary_local_server_cert_pem_start");
//extern const uint8_t local_server_cert_pem_end[]   asm("_binary_local_server_cert_pem_end");

static uint64_t get_current_time_ms();

//bool send_attachment=0;
bool  filedata_free_http = 0; //smtp_Response=0,
int16_t size_http = 0;
int ret_http;//, len_http;
size_t base64_len_http;
//-------------------------- Actor Parameters ------------------------------//

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Post[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Fetch[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char buffer [MAX_HTTP_RECV_BUFFER];
PSRAM_ATTR_BSS static char http_payload_buffer[2048];
PSRAM_ATTR_BSS static char line[COMMAND_LEN];
PSRAM_ATTR_BSS static char payLoadData_Save[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Send_File_http[512];

PSRAM_ATTR_BSS static struct actor_parameter {
	uint32_t	content_length_u32;
	uint8_t 	server_a8[32];
	uint8_t 	Retry_Cnt;
	uint8_t 	Connection_status;
	uint8_t     sessionId[50];
	uint64_t	http_dl_time_u64;	//HTTP_DL_TIME
} s_Para;

// Define the structure to hold file request information
typedef struct {
    char url[MAX_FILE_URL_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    char mode[MAX_FILE_MODE_LEN];
    char ReceivedCRC[10];
    bool ReceivedAckIfFileExists;
} FileRequest_t;

// Define the structure to hold file request information
typedef struct {
    char url[MAX_FILE_URL_LEN];
    char file_name[MAX_FILE_NAME_LEN];
    char api_key[MAX_FILE_URL_LEN];
    uint32_t file_size;
} FileUpload_t;

FileUpload_t fileUpload;

typedef struct{
	uint8_t u16CRC[5];
	uint32_t File_Size;
}CRCRequest_t;

#define BUF_SIZE_http            5*1024 //1026 //10240

PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char fetch_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char local_response_buffer[4096];
PSRAM_ATTR_BSS static char File_buffer_http[BUF_SIZE_http];
PSRAM_ATTR_BSS static char buf_http[BUF_SIZE_http];

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &s_Para.content_length_u32, "CONTENT_LEN",      U_INT32, "R",		"Length of the content" },
    { &s_Para.server_a8,          "SERVER",           STRING,  "RW",	"Server address" },
    { &s_Para.Retry_Cnt,          "RETRY_COUNT",      U_INT8,  "RW",  	"Retry count" },
	{ &s_Para.Connection_status, "CONNECTION_STATUS",U_INT8,  "R",  	"HTTP Connection Status" },
	{ &s_Para.sessionId,          "SESSIONID",        STRING,  "RW",	"Session ID for HTTP post" },
    { &s_Para.http_dl_time_u64,   "HTTP_DL_TIME", 	  U_INT64, "R", 	"HTTP file download last updated time" },
};

//-------------------------- Common Actor Resources ------------------------------//
static const char * THIS_ACTOR 	 = "HTTP";
static const char 			THIS_ACTOR_ID 	= 	HTTP;  // assign src id
static AMessage_st 		  s_Message_Tx;// s_Message_Rx_New;	// ACtors Message structure , s_Message_Rx,
static uint8_t JFS_Response		= E_JFS_RESP_OK;
static int NET_status = -1;
uint32_t JFS_Size = 0xFFFFFFFF;
PSRAM_ATTR char Authorization[300];
static bool     		FirstEntry_bool = 	false;
static TaskHandle_t  	Task_Handle		= NULL, Task_Handle_HTTP_Receive=NULL, PostHandle = NULL, Task_Handle_HTTP_Upload=NULL; 	// HTTP Task Handler
static QueueHandle_t 	msg_Rx_Queue	= NULL, Post_Queue	= NULL,fileQueue = NULL, CRC_Queue=NULL, file_UploadQueue = NULL; 	// HTTP Rx Queue
static StaticTask_t xHTTPTaskBuffer, xFetchTaskBuffer, xPostTaskBuffer, xUploadTaskBuffer;  //// Declare a static task control block
PSRAM_ATTR_BSS static StackType_t xPostTaskStack[HTTP_POST_TASK_STACK_DEPTH],xFetchTaskStack[HTTP_FETCH_TASK_STACK_DEPTH], xUploadTaskStack[HTTP_FILE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t xTaskStack [HTTP_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [RX_QUE_COUNT * sizeof(AMessage_st)], Fetch_pucQueueStorage [HTTP_FETCH_QUE_LENGTH * sizeof(FileRequest_t)], Post_pucQueueStorage [HTTP_POST_QUE_LENGTH * sizeof(AMessage_st)], CRC_pucQueueStorage[HTTP_FETCH_QUE_LENGTH * 150], Upload_pucQueueStorage [UPLOAD_FILE_QUE_COUNT * UPLOAD_FILE_QUE_COUNT_ITEMSIZE];
static StaticQueue_t Monitor_pxQueueBuffer,Fetch_pxQueueBuffer, Post_pxQueueBuffer, CRC_pxQueueBuffer, Upload_pxQueueBuffer;
static void init		(void *a, void *b); 							// Init actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					// Set or Change a parameter
static void get			(char *prop, char *val_a8);						// Get or Read a Parameter
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor		(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void Deinit_Actor(AMessage_st* s_Message_Rx);
static int isValidFileName(const char *fileName);
static int isValidFilePath(const char *filePath);

PSRAM_ATTR_BSS static char filedata[UPLOAD_FILE_QUE_COUNT_ITEMSIZE];
//----------------------- Commen Actor Methods ------------------------------------------------------------------//
static uint64_t get_current_time_ms() {
	return get_current_rtc_time_ms(0);
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
			if(!(strcmp(prop[i].str_name, "CONNECTION_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_SERVER_NOT_CONNECTED:
					strcpy(val_a8, "HTTP NOT CONNECTED");
					break;

				case E_SERVER_CONNECTED:
					strcpy(val_a8, "HTTP CONNECTED");
					break;

				default:
					break;
				}
			}

		}
	}
}//	get


static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[300] = {0};
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

				case U_INT64:
					sprintf(val_a8, "%016llu", *(uint64_t*) prop[i].name);
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
			if(!(strcmp(prop[i].str_name, "CONNECTION_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case E_SERVER_NOT_CONNECTED:
					strcpy(val_a8, "HTTP NOT CONNECTED");
					break;

				case E_SERVER_CONNECTED:
					strcpy(val_a8, "HTTP CONNECTED");
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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the HTTP actor.");
	cJSON_AddStringToObject(responseObject, "SET(string SERVER)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "FETCH_FILE(string FILE_NAME, string URL, string MODE)", "Fetch the file from HTTP and store it in the JFS.");
	cJSON_AddStringToObject(responseObject, "UPLOAD(string URL, string FILE_NAME, string API_KEY)", "To Upload file.");
	cJSON_AddStringToObject(responseObject, "POST(string URL, string HTTP_PAYLOAD, string API_KEY, string  DEVICEID, string RETRY_PERIOD)", "POST the data to HTTP.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "DEINIT()", "Deinit the actor. It delete the monitor task and its queue.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void init(void *a, void *b) {
	if (FirstEntry_bool) {
		return;
	}
	memset(&s_Message_Tx,0,sizeof(s_Message_Tx));
	//Create Rx_Queue
    if(msg_Rx_Queue == NULL)
    {
		msg_Rx_Queue = xQueueCreateStatic(RX_QUE_COUNT, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
		if (msg_Rx_Queue == NULL) {
	#ifdef ENABLE_PRINT_MSG
			printf("HTTP RX Queue is not created. \n");
	#endif
		}
    }

	Task_Handle = xTaskCreateStaticPinnedToCore(
						monitor,                 // Task function
						"HTTP Monitor",            // Task name
						HTTP_TASK_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						HTTP_TASK_PRIORITY,                       // Task priority
						xTaskStack,              // Pointer to task stack (allocated in PSRAM)
						&xHTTPTaskBuffer,            // Pointer to task control block
						1
	);

		if (Task_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create task\n");
#endif
				//heap_caps_free(xTaskStack);
				// Handle error
			}

		// Creating Monitor Task
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor
		s_Para.content_length_u32 = 0;
		s_Para.Retry_Cnt = 5;
		strcpy((char*)&s_Para.server_a8, "http://httpbin.org/get");
		s_Para.Connection_status = E_SERVER_NOT_CONNECTED;
		s_Para.http_dl_time_u64 = 0;
		FirstEntry_bool = true;
}// init

static void monitor(void *pvParameters __attribute__((unused))) {
	cJSON *name_JSON 	= NULL;
	cJSON *head_JSON 	= NULL;  //refer to head_JSON and tail as in linked list
	char str[100] = {0};
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		  if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))  //5
		{
//			printf("HTTP msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("HTTP DT = %s\n",s_Message_Rx->payload_p8);
//			}
			memcpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES/2-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				init(0, 0);
				Add_Response_msg("HTTP is initialized.", s_Message_Rx, payLoadData);
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
				   // Loop through each key-value pair
				    do {
				    	// Check if the value string is not NULL
				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
						{
							// Set the key-value pair
							u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
							if(u8Result==2){
								sprintf(str,"'%s' is a read only property", head_JSON->string);
								 Add_Response_msg(str, s_Message_Rx, payLoadData);
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
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "FETCH_FILE")) {
				http_get_request(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "UPLOAD")) {
				http_get_file(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "POST"))
			{
				http_post_request(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
					get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "DEINIT"))
			{
				Deinit_Actor(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "MAIL_BINARY"))
			 {
				size_http  = s_Message_Rx->payload_size;
				if(file_UploadQueue != NULL)
				{
					xQueueSend(file_UploadQueue, s_Message_Rx->payload_p8, QUE_DELAY);
				}
			 }
			else
			{
				//HTTP error message: invalid method
				Add_Response_msg("invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}// monitor

//void http_ConsoleWriteToActor_xface(AMessage_st *msg)
void http_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	char retry = 0;
	s_Message = (AMessage_st*)msg;
	if (FirstEntry_bool == false)
	{
		if(strcmp((char*)s_Message->cmdFun_a8, "DEINIT") == 0)
			return;

		init(0,0);
	}
	do
	{
		uint8_t state = xQueueSend(msg_Rx_Queue, s_Message, QUE_DELAY); //5,1
		if (state == pdTRUE)
		{
			break;
		}
		else
		{
			if (state == errQUEUE_FULL)
			{
				printf(">HTTP.ERROR(HTTP RX Queue is full)\n");
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
			else
			{
				printf(">HTTP.ERROR(HTTP RX Queue send unsuccessful)\n");
				vTaskDelay(1000 / portTICK_PERIOD_MS);
			}
		}
	} while(++retry < 3);

	if(retry >= 3)
	{
		printf("\n\n\n>HTTP.ERROR(Flush the Rx queue.)\n\n\n");
		xQueueReset(msg_Rx_Queue);  // reset the queue
		xQueueSend(msg_Rx_Queue, s_Message, QUE_DELAY);
	}
}//	http_ConsolWriteToActor


//------------------------ HTTP Actor Methods Starts ------------------------------------------------------------------------//

esp_err_t http_event_handler(esp_http_client_event_t *evt) {
	static char *output_buffer; // Buffer to store response of http request from event handler
	static int output_len;       // Stores number of bytes read
	esp_err_t err;
	switch (evt->event_id) {
	//case HTTP_EVENT_REDIRECT:
	case HTTP_EVENT_ERROR:
#ifdef ENABLE_PRINT_MSG
		printf("\n Error in HTTP connection");
#endif
		break;
	case HTTP_EVENT_ON_CONNECTED:
		break;
	case HTTP_EVENT_HEADER_SENT:
		break;
	case HTTP_EVENT_ON_HEADER:
		break;
	case HTTP_EVENT_ON_DATA:
		/*
		 *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
		 *  However, event handler can also be used in case chunked encoding is used.
		 */
		if (!esp_http_client_is_chunked_response(evt->client)) {
			// If user_data buffer is configured, copy the response into the buffer
			if (evt->user_data) {
				memcpy(evt->user_data + output_len, evt->data, evt->data_len);
			} else {
				if (output_buffer == NULL) {
					output_buffer = (char*) heap_caps_malloc(
							esp_http_client_get_content_length(evt->client), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
					output_len = 0;
					if (output_buffer == NULL)
					{
						printf("Memory allocation failed\n");
						return ESP_FAIL;
					}
				}
				memcpy(output_buffer + output_len, evt->data, evt->data_len);
			}
			output_len += evt->data_len;
		}

		break;
	case HTTP_EVENT_ON_FINISH:
		if (output_buffer != NULL) {
			free(output_buffer);
			output_buffer = NULL;
		}
		output_len = 0;
		break;
	case HTTP_EVENT_DISCONNECTED:
		if (err != 0) {
		}
		if (output_buffer != NULL) {
			free(output_buffer);
			output_buffer = NULL;
		}
		output_len = 0;
		break;

	case HTTP_EVENT_REDIRECT:
		esp_http_client_set_header(evt->client, "From", "user@example.com");
		esp_http_client_set_header(evt->client, "Accept", "text/html");
		esp_http_client_set_redirection(evt->client);
		break;
	}
	return ESP_OK;
}

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer = NULL;
    if (strcmp(function, "WRITE_FILE") == 0 || strcmp(function, "WRITE_FILE1") == 0)
	{
		newpointer  		= (uint8_t*) heap_caps_calloc(size,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (newpointer == NULL)
		{
			printf("Memory allocation failed\n");
			return;
		}
		memcpy(newpointer, response, size);
	}
	else
	{
		newpointer  		= (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
		if (newpointer == NULL)
		{
			printf("Memory allocation failed\n");
			return;
		}
		strcpy((char*)newpointer, response);
	}
	s_Message_Tx_new.payload_p8 	=  newpointer;
	s_Message_Tx_new.Dest_ID_a8 = dest_id;
	s_Message_Tx_new.payload_size = size;  //0 , size;  // if you don't want to free payload at dest actor then give payload size =0
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, THIS_ACTOR);
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
} //	COP_add_response_to_COP_Tx_Queue

static void http_file_reader(void *pvParameters)
{
	AMessage_st* s_Message_Rx_data = (AMessage_st*)pvParameters;

    cJSON *in_JSON = cJSON_Parse((char *)s_Message_Rx_data->payload_p8);
    if (in_JSON == NULL) {
        Add_Response_msg("Invalid JSON input.", s_Message_Rx_data, payLoadData);
        goto exit;
    }

    cJSON *name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
    if (name_JSON != NULL && name_JSON->valuestring != NULL) {

    	if (isValidFileName(name_JSON->valuestring) &&
    	                		isValidFilePath(name_JSON->valuestring))
    	{
			strcpy(fileUpload.file_name, name_JSON->valuestring);

    	}
    	else
    	{
    		Add_Response_msg("Received Invalid File name/path in upload",s_Message_Rx_data, payLoadData);
    	    Add_Response_msg("Kindly enter the correct filepath & filename. Use drive 'A:/' for JFS and 'B:/' for SD card.",s_Message_Rx_data, payLoadData);
    	    goto exit;
    	}
    }

    name_JSON = cJSON_GetObjectItem(in_JSON, "URL");
    if (name_JSON != NULL && name_JSON->valuestring != NULL) {
        strcpy(fileUpload.url, name_JSON->valuestring);

        if ( (strlen(fileUpload.url) == 0) || (strncmp(fileUpload.url, "https://", 8) != 0))
        {
    		Add_Response_msg("Received invalid or missing URL",s_Message_Rx_data, payLoadData);
    	    goto exit;
        }
    }
	else
	{
		Add_Response_msg("Received invalid or missing URL",s_Message_Rx_data, payLoadData);
	    goto exit;
	}

    name_JSON = cJSON_GetObjectItem(in_JSON, "API_KEY");
    if (name_JSON != NULL && name_JSON->valuestring != NULL)
    {
        strcpy(fileUpload.api_key, name_JSON->valuestring);

        if(strlen(fileUpload.api_key) == 0)
        {
    		Add_Response_msg("Received invalid or missing API key",s_Message_Rx_data, payLoadData);
    	    goto exit;
        }
    }
	else
	{
		Add_Response_msg("Received invalid or missing API key",s_Message_Rx_data, payLoadData);
	    goto exit;
	}

//    printf(" fileUpload.file_name  = %s, fileUpload.url = %s, fileUpload.api_key = %s \n", fileUpload.file_name, fileUpload.url, fileUpload.api_key);
    cJSON_Delete(in_JSON);

//    Add_Response_msg("url file upload is in progress. Please wait ...", s_Message_Rx_data, payLoadData);

    char str[100]={0};	//, str1[50]={0};
    char binary_file_fg = 0;
	char out[100] =  {0}; // out1[100] = {0};//;  (char*) heap_caps_calloc(100, sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	sprintf(out, "{\"FILE_NAME\":\"%s\"}",(char*)fileUpload.file_name);
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", out, strlen(out), "GET_FILE_SIZE");
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", out, strlen(out), "SMTP_READ");
    const char *dot = strrchr((char*)fileUpload.file_name, '.');

    // Build header & footer
    char header[256];
    int header_len = snprintf(header, sizeof(header),
        "--%s\r\n"
        "Content-Disposition: form-data; name=\"file\"; filename=\"%s\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n",
        BOUNDARY, fileUpload.file_name);
    char footer[64];
    int footer_len = snprintf(footer, sizeof(footer),
        "\r\n--%s--\r\n", BOUNDARY);

    int64_t total_len = 0;
    uint8_t tempflag = 0;
    int written = 0;
    esp_err_t err = ESP_FAIL;


    esp_http_client_handle_t client = NULL;

    // Configure HTTP client
    fileUpload.url[sizeof(fileUpload.url) - 1] = '\0';
    fileUpload.api_key[sizeof(fileUpload.api_key) - 1] = '\0';

     esp_http_client_config_t cfg =
     {
         .url                = (char*)fileUpload.url,
         .method             = HTTP_METHOD_POST,
         .transport_type     = HTTP_TRANSPORT_OVER_SSL,
         .port               = 443,
         .crt_bundle_attach  = esp_crt_bundle_attach,
         .timeout_ms         = 10000,
     };
     client = esp_http_client_init(&cfg);
     if (!client)
     {
         printf("  http_client_init failed\n");
         goto exit1;
     }

    while (1)
    {
    	filedata[0] = '\0';

		if (pdTRUE == xQueueReceive(file_UploadQueue, (void*)filedata, 20000))
		{
			if(tempflag == 0)
			{

				total_len = (int64_t)header_len + fileUpload.file_size + footer_len;

//				printf("  total POST body = %lld bytes\n", total_len);

			     // Set headers
			     char ct[64];
			     snprintf(ct, sizeof(ct), "multipart/form-data; boundary=%s", BOUNDARY);
			     esp_http_client_set_header(client, "Content-Type", ct);
			     esp_http_client_set_header(client, "x-api-key", (char*)fileUpload.api_key);

			     // Open connection with retries
			     #define MAX_RETRIES 3
			     esp_err_t rc = ESP_FAIL;
			     for (int retry = 0; retry < MAX_RETRIES; retry++)
			     {
			         rc = esp_http_client_open(client, total_len);
			         if (rc == ESP_OK)
			         {
			             break;
			         }
//			         printf("  Retry %d: http open failed: %s\n", retry + 1, esp_err_to_name(rc));
			         vTaskDelay(1000 / portTICK_PERIOD_MS);
			         if (retry == MAX_RETRIES - 1)
			         {
//			             printf("  Max retries reached\n");
			             goto exit1;
			         }
			     }

			     // Send header
			     written = esp_http_client_write(client, header, header_len);
			     if (written < 0)
			     {
//			         printf("  header write failed: %s\n", esp_err_to_name(written));
			         goto exit1;
			     }
			     else if (written != header_len)
			     {
//			         printf("  header write incomplete: sent %d of %d bytes\n", written, header_len);
			         goto exit1;
			     }

				tempflag = 1;
			}

			if(strlen(filedata) != 0)
			{
				if(strcmp(filedata, "Delete")==0)  // Delete the mail task as file is not present
				{
					Add_Response_msg("File is not present. Deleting the http_file_reader ...", s_Message_Rx_data, payLoadData);
					goto exit1;
				}
			}
			ret_http = -1;
			memset(buf_http,0,BUF_SIZE_http);
			memset(File_buffer_http,0,sizeof(File_buffer_http));

			if(binary_file_fg == 1)
			{
				ret_http = 	mbedtls_base64_encode((unsigned char *) File_buffer_http, BUF_SIZE_http, &base64_len_http, (unsigned char *)filedata, size_http);// strlen(filedata)
				if (ret_http != 0)
				{
					sprintf(str,"Error in mbedtls encode! ret = -0x%x", -ret_http);
					Add_Response_msg(str, s_Message_Rx_data, payLoadData);
				}
			}
			else
			{
			    memcpy(File_buffer_http,  filedata, size_http);
//				printf("File_buffer_http = %s \n", File_buffer_http);
			}


			// Stream file
			    printf("File posting please wait...\n");

			        int sent = esp_http_client_write(client, (const char*)File_buffer_http, size_http);
			        if (sent < 0)
			        {
			            printf("  file write failed: %s\n", esp_err_to_name(sent));
			            goto exit1;
			        }
			        else if (sent != size_http)
			        {
			            printf("  file write incomplete: sent %d of %zu bytes\n", sent, size_http);
			            goto exit1;
			        }

				if(fileUpload.file_size < size_http)
				{
					fileUpload.file_size = 0;
				}
				else
				{
					fileUpload.file_size  = fileUpload.file_size  - size_http; //strlen(filedata);
				}

//				printf("remaining fileUpload.file_size = %ld \n", fileUpload.file_size);

				if(fileUpload.file_size  > 10)
				{
					cJSON *out_JSON 	= cJSON_CreateObject();
					cJSON_AddNumberToObject(out_JSON, "SMTP_RESP", E_JFS_RESP_OK);
					memset(payLoadData_Send_File_http,0,sizeof(payLoadData_Send_File_http));
					cJSON_PrintPreallocated(out_JSON, payLoadData_Send_File_http, sizeof(payLoadData_Send_File_http), false);
					Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", payLoadData_Send_File_http, strlen(payLoadData_Send_File_http), "RESPONSE");
					cJSON_Delete(out_JSON);
				}

				if(fileUpload.file_size  <= 10)
				{

				    // Send footer
				    written = esp_http_client_write(client, footer, footer_len);
				    if (written < 0)
				    {
				        printf("  footer write failed: %s\n", esp_err_to_name(written));
				        goto exit1;
				    }
				    else if (written != footer_len)
				    {
				        printf("  footer write incomplete: sent %d of %d bytes\n", written, footer_len);
				        goto exit1;
				    }

				    // Finish and check status
				    int64_t content_length = esp_http_client_fetch_headers(client);
				    if (content_length < 0)
				    {
				        printf("  fetch headers failed: %s\n", esp_err_to_name(content_length));
				        goto exit1;
				    }
				    int status = esp_http_client_get_status_code(client);
				    printf(">({\"FILE_POST\":%d})\r\n", status);

				    // Read response body
				    char response_buf[512];
				    int read_len = esp_http_client_read(client, response_buf, sizeof(response_buf) - 1);
				    if (read_len >= 0)
				    {
				        response_buf[read_len] = '\0';
				    }
				    else
				    {
				        printf("  Failed to read response: %s\n", esp_err_to_name(read_len));
				    }

				    err = (status >= 200 && status < 300) ? ESP_OK : ESP_FAIL;

				    printf("err = %d\n", err);

				    goto exit1;
				}
		}  // end of xQueueReceive fileQueue
		else
		{
			Add_Response_msg("Queue not received. Deleting the http_file_reader ...", s_Message_Rx_data, payLoadData);
			goto exit1;
		}
  }  // end of while(1)
exit1:

	if(client)
	{
		esp_http_client_cleanup(client);
	}

exit:
	Task_Handle_HTTP_Upload = NULL;
    vTaskDelete(NULL);
}

static void http_stream_reader(void *pvParameters)
{
	AMessage_st* s_Message_Rx_data = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx1;
	AMessage_st *s_Message_Rx = &s_Message_Rx1;
	char JFS_SIZE_Retry = 0;
	FileRequest_t fileRequest = {0};
	memset(fetch_buffer,0,sizeof(fetch_buffer));
	memcpy(s_Message_Rx, s_Message_Rx_data, sizeof(AMessage_st));
	if((char*)s_Message_Rx->payload_p8 != NULL)
	{
		strncpy(fetch_buffer, (char*)s_Message_Rx_data->payload_p8,(MAX_JSON_PAYLOAD_BYTES/2-1));
	}
	s_Message_Rx->payload_p8 = (uint8_t*)fetch_buffer;
	strcpy((char*)s_Message_Rx->cmdFun_a8, "FETCH_FILE");
	strcpy((char*)s_Message_Rx->dest_Actor_a8, "HTTP");
	strcpy((char*)s_Message_Rx->src_Actor_a8, "IHUB");

	char str[500]= {0}, binary_file = 0, LED_download_state = 0; // buffer[500]= {0};
	cJSON *my_JSON = NULL;
	CRCRequest_t CRCBuffer = {0};
	int64_t content_length = 0;
	int64_t downloaded_len = 0;
	char buffer_new [150]= {0};
	FileDownload_t FileDownloadptr;
	esp_http_client_handle_t client = NULL;
	if (CRC_Queue == NULL)
	CRC_Queue = xQueueCreateStatic(HTTP_FETCH_QUE_LENGTH, 150, CRC_pucQueueStorage, &CRC_pxQueueBuffer);

	if (CRC_Queue == NULL) {
	        Add_Response_msg("Failed to create CRC Queue.", s_Message_Rx, payLoadData_Fetch);
	        goto Send_Ack;
	    }
	Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", "\0", 0, "GET_VOLUME_INFO"); // check JFS free memory
    while (1)
    {
		memset(&fileRequest, 0, sizeof(fileRequest));
		if (xQueueReceive(fileQueue, &fileRequest, portMAX_DELAY) == pdPASS)
		{
			do{
				JFS_SIZE_Retry++;
				if(JFS_Size != 0xFFFFFFFF)
					{break;}
				else
					{vTaskDelay(500);}
			}while(JFS_SIZE_Retry < 5);
			if(JFS_Size == 0xFFFFFFFF)
			{
				Add_Response_msg("Error! No response from JFS to obtain JFS size.",s_Message_Rx, payLoadData_Fetch);
				continue;
			}
			//printf("\n\n 1 NET_status = %d, JFS_Size = %ld \n\n",NET_status, JFS_Size);
			if((NET_status == E_NET_CONNECTED) && (JFS_Size != 0xFFFFFFFF))
			{
			downloaded_len = 0;
			sprintf(str,"Received file request: %s", fileRequest.file_name);
			Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
			{
				my_JSON  	= cJSON_CreateObject();
				 if (my_JSON == NULL)
				 {
						Add_Response_msg("Failed to create JSON object for CRC request.", s_Message_Rx, payLoadData_Fetch);
						goto Send_Ack;
					}
				cJSON_AddStringToObject(my_JSON, "FILE_NAME",fileRequest.file_name );
				memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
				cJSON_PrintPreallocated(my_JSON, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "GET_FILE_CRC");
				cJSON_Delete(my_JSON);
#define WAIT_FOR_CRC_15_SECONDS 15000

				if (xQueueReceive(CRC_Queue, buffer_new, WAIT_FOR_CRC_15_SECONDS) == pdPASS)
				{
					cJSON *data = cJSON_Parse(buffer_new);
					if(data == NULL)
						continue;
					cJSON *CRC 		= cJSON_GetObjectItem(data, "CRC16");
					if((CRC != NULL) && cJSON_IsString(CRC))
						strcpy((char*)CRCBuffer.u16CRC,CRC->valuestring);

					cJSON *FILE_SIZE 		= cJSON_GetObjectItem(data, "FILE_SIZE");
					if((FILE_SIZE != NULL) && cJSON_IsNumber(FILE_SIZE))
						CRCBuffer.File_Size = FILE_SIZE->valueint;
					cJSON_Delete(data);
					sprintf(str,"Received CRC value is: %s, Calculated CRC value is: %s",fileRequest.ReceivedCRC, CRCBuffer.u16CRC);
					Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
					if (CRCBuffer.u16CRC[0] != '\0' && fileRequest.ReceivedCRC[0] != '\0' && (strcmp((char*)CRCBuffer.u16CRC, fileRequest.ReceivedCRC) == 0)) {
						sprintf(str,"File '%s' will not be downloaded as CRC matches.",fileRequest.file_name);
						Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);

						if(fileRequest.ReceivedAckIfFileExists == true)
						{
							cJSON *responseObject2 = cJSON_CreateObject();
							if(responseObject2!= NULL){
							cJSON_AddStringToObject(responseObject2, "file_url", fileRequest.url);
							cJSON_AddStringToObject(responseObject2, "file_path", fileRequest.file_name);
							cJSON_AddStringToObject(responseObject2, "File_crc", (char*)CRCBuffer.u16CRC);
							cJSON_AddNumberToObject(responseObject2, "file_size", CRCBuffer.File_Size);
							cJSON_AddStringToObject(responseObject2, "Message", "FILE EXISTS");
							memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
							cJSON_PrintPreallocated(responseObject2, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
							cJSON_Delete(responseObject2);
							Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "POST_FILE_ACK");
							}
						}
					}
		else
		{
			sprintf(str,"File '%s' will be downloaded as CRC doesn't match.",fileRequest.file_name);
			Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);

			memset(buffer, 0, MAX_HTTP_RECV_BUFFER);
			memset(&FileDownloadptr, 0, sizeof(FileDownloadptr));
			FileDownloadptr.buffer = buffer;
			strcpy(FileDownloadptr.file_name,fileRequest.file_name);
			if((fileRequest.mode[0]=='w') || (fileRequest.mode[0]=='a'))
				 strcpy(FileDownloadptr.mode,fileRequest.mode);
			else
				strcpy(FileDownloadptr.mode,"w");
			esp_http_client_config_t config = {
				.url = fileRequest.url,
				.transport_type = HTTP_TRANSPORT_OVER_SSL,
				.crt_bundle_attach = esp_crt_bundle_attach,
				.timeout_ms = 10000,
			};

			client = esp_http_client_init(&config);
			if (client == NULL) {
				Add_Response_msg("Failed to initialize HTTP client.",s_Message_Rx, payLoadData_Fetch);
				goto Send_Ack;
			}

			int retry_count = 0;
			esp_err_t err;
			while (retry_count < 5)
			{
				err = esp_http_client_open(client, 0);
				if (err == ESP_OK) {
					break;
				} else {
					sprintf(str,"Retry %d: Failed to open HTTP connection: %s", retry_count + 1, esp_err_to_name(err));
					Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
					retry_count++;
					vTaskDelay(pdMS_TO_TICKS(500));
				}
			}

			if (err != ESP_OK) {
				sprintf(str,"Failed to open HTTP connection after 5 retries: %s", esp_err_to_name(err));
				Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
				esp_http_client_cleanup(client);
				goto Send_Ack;
			}
			int64_t tickstart = get_current_time_ms();
			content_length = esp_http_client_fetch_headers(client);
			int Http_status = esp_http_client_get_status_code(client);
			FileDownloadptr.file_size = content_length;
			cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
			if(responseObject != NULL)
			{
				cJSON_AddStringToObject(responseObject, "FILE NAME", fileRequest.file_name);
				cJSON_AddNumberToObject(responseObject, "HTTP STATUS", Http_status);
				cJSON_AddNumberToObject(responseObject, "FILE SIZE", content_length);
				memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
				cJSON_PrintPreallocated(responseObject, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Fetch);
				cJSON_Delete(responseObject);
				console_send_responce_to_console_xface(s_Message_Rx);
			}

			binary_file = 0;
			const char *dot = strrchr((char*)fileRequest.file_name, '.');
			// Check if the dot is present and if it is followed by "bin"
			if (dot != NULL && ((strcmp(dot + 1, "bin") == 0)))
			{
				binary_file = 1;
			}
			if((content_length > JFS_Size) && (Http_status > 0) && (binary_file == 0))  // Check for available JFS memory
			{
				Add_Response_msg("Error! Received file size is larger than the available JFS memory. Hence, deleting the task..",s_Message_Rx, payLoadData_Fetch);
				goto exit;
			}

			if ((content_length <= 0) || (content_length >= 4294938617) || (Http_status != 200)) {
				esp_http_client_close(client);
				esp_http_client_cleanup(client);
				goto Send_Ack;
			}

			int64_t remaining_read_len = content_length;
			int64_t read_len;
			JFS_Response = E_JFS_RESP_OK;
			if(content_length > 200000)   // If file size > 200kB, then show file download process on LED
			{
				cJSON *responseObject = cJSON_CreateObject();
				if(responseObject != NULL)
				{
					cJSON_AddStringToObject(responseObject, "stateName", "Download");
					cJSON_AddNumberToObject(responseObject, "duration", -1);
					memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
					cJSON_PrintPreallocated(responseObject, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
					Send_CMD_To_Other_Actor(LED, "LED", payLoadData_Fetch, strlen(payLoadData_Fetch), "SETSTATE");
					cJSON_Delete(responseObject);
					LED_download_state = 1;
				}
			}
			while (remaining_read_len > 0)
			{
				if(JFS_Response	== E_JFS_RESP_OK)
				{
				JFS_Response = E_JFS_RESP_WAIT;
					memset(buffer, 0, MAX_HTTP_RECV_BUFFER);
					read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
					if (read_len <= 0)
					{
						retry_count = 0;
						while (retry_count < 5)
						{
							sprintf(str,"Retry %d: No data available for reading, remaining length: %lld", retry_count + 1, remaining_read_len);
							Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
							if(client != NULL)
							{
								esp_http_client_close(client);
								esp_http_client_cleanup(client);
							}

							client = esp_http_client_init(&config);
							if (client == NULL) {
								Add_Response_msg("Failed to initialize HTTP client.",s_Message_Rx, payLoadData_Fetch);
								retry_count++;
								vTaskDelay(pdMS_TO_TICKS(1000));
								continue;
							}
							err = esp_http_client_open(client, 0);
							if (err != ESP_OK) {
								sprintf(str,"Failed to open HTTP connection: %s", esp_err_to_name(err));
								Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
								retry_count++;
								vTaskDelay(pdMS_TO_TICKS(1000));
								continue;
							}
							content_length = esp_http_client_fetch_headers(client);
							int Http_status = esp_http_client_get_status_code(client);
							sprintf(str,"content_length is %lld, Http_status is %d, remaining_read_len is %lld", content_length, Http_status, remaining_read_len);
							Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
							if(Http_status == 200)
							{
								int64_t download_count = downloaded_len;
								while(download_count > 0)
								{
									if(download_count > MAX_HTTP_RECV_BUFFER)
										read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
									else
									{
										if(download_count > remaining_read_len)
											read_len = esp_http_client_read(client, buffer, (download_count - remaining_read_len));
										else
											read_len = esp_http_client_read(client, buffer, download_count);
									}
									if (read_len > 0)
									{
										download_count = download_count - read_len;
									}
									else
									{
										break;
									}
									 printf("read_len = %lld, remaining_read_len = %lld, download_count = %lld\n",read_len, remaining_read_len, download_count);
									vTaskDelay(pdMS_TO_TICKS(100));
								}
								if(download_count == 0)
								{
									read_len = esp_http_client_read(client, buffer, MAX_HTTP_RECV_BUFFER);
									printf("1 read_len = %lld\n", read_len);
									if(read_len > 0)
										break;
								}
							}
							retry_count++;
							vTaskDelay(pdMS_TO_TICKS(1000));
						}
					}

				if (read_len <= 0) {
					sprintf(str,"Failed to read data after 5 retries, remaining length: %lld", remaining_read_len);
					Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
					if(LED_download_state == 1)
					{
						LED_download_state = 0;
						Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE"); // Restore the LED state
					}
					if(strcmp(FileDownloadptr.file_name,"A:/firmware.bin") == 0)
					{
						Send_CMD_To_Other_Actor(WIFI,"WIFI", "\0", 0, "INIT");

						cJSON *my_JSON = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
						cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 1);
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
						Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "SETSERVCONNFLAG");
						cJSON_Delete(my_JSON); // Free the cJSON object

						cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
						cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 1);
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);
						Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "SETEVTTASKFG");
						cJSON_Delete(my_JSON1); // Free the cJSON object

						Send_CMD_To_Other_Actor(IHUB,"IHUB", "\0", 0, "CONNECT");
					}
					goto Send_Ack;
					//break;
				}
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", (char *) &FileDownloadptr, (read_len+sizeof(FileDownload_t)), "WRITE_FILE1");
				remaining_read_len -= read_len;
				downloaded_len += read_len;
				sprintf(str,"File name: %s, Bytes downloaded: %lld, Bytes remaining: %lld", FileDownloadptr.file_name, downloaded_len, remaining_read_len);
				Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);

				if (strcmp(fileRequest.mode, "w") == 0 && remaining_read_len > 0) {
				   strcpy(FileDownloadptr.file_name,fileRequest.file_name);
				   strcpy(FileDownloadptr.mode,"a");
			   }
				}
				if(JFS_Response	== E_JFS_RESP_ERR)
				{
					Add_Response_msg("Error in file writing process.",s_Message_Rx, payLoadData_Fetch);
					break;
				}
				else if(JFS_Response == E_JFS_RESP_TERMINATE)
				{
					sprintf(str,"Terminating the fetch file process of %s file", fileRequest.file_name);
					Add_Response_msg(str,s_Message_Rx, payLoadData_Fetch);
					break;
				}
			}  //end of while (remaining_read_len > 0)

			int64_t tickend = (get_current_time_ms() - tickstart);
			cJSON *responseObject1 = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
			if(responseObject1 != NULL)
			{
				cJSON_AddStringToObject(responseObject1, "FILE NAME", fileRequest.file_name);
				cJSON_AddNumberToObject(responseObject1, "Execution Time (msec)", tickend);
				memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
				cJSON_PrintPreallocated(responseObject1, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
				strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Fetch);
				cJSON_Delete(responseObject1);
				console_send_responce_to_console_xface(s_Message_Rx);
			}
			if(LED_download_state == 1)
			{
				LED_download_state = 0;
				Send_CMD_To_Other_Actor(LED, "LED", "\0", 0 , "RESTORE_STATE"); // Restore the LED state
			}
			do
			{
				vTaskDelay(pdMS_TO_TICKS(50));

			}while(JFS_Response == E_JFS_RESP_WAIT);
		  if(!strcasecmp(fileRequest.file_name,"A:/Database/Schedule.db"))
		  {
			  Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "UPDATE_SCH_TABLE");
		  }
		  else if(!strcasecmp(fileRequest.file_name,"A:/Database/Location.db"))
			{
			  Send_CMD_To_Other_Actor(EVENT_ACTOR, "EVENT_ACTOR", "\0", 0, "UPDATE_LOC");
			}
		  else if(!strcasecmp(fileRequest.file_name,"A:/Database/Color.db"))
			{
			  Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", "\0", 0, "READ_COLOR_TABLE");
			}
		  else if((!strcasecmp(fileRequest.file_name,"A:/Database/Virtual_Table.db")) ||
				  (!strcasecmp(fileRequest.file_name,"A:/Database/Virtual_Group_Table.db")))
			{
			  Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", "\0", 0, "READ_VIRTUAL_TABLE");
			}
		  else if(!strcasecmp(fileRequest.file_name,"A:/Database/Command_Table.db"))
			{
			  Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", "\0", 0, "READ_COMMAND_TABLE");
			}
		  else if(!strcasecmp(fileRequest.file_name,"A:/Database/Playlist_Table.db"))
			{
			  Send_CMD_To_Other_Actor(LIGHTING,"LIGHTING", "\0", 0, "READ_PLAYLIST_TABLE");
			  vTaskDelay(pdMS_TO_TICKS(10000));
			}

		  	struct timeval currentTime;
			_gettimeofday_r(NULL, &currentTime, NULL);
			uint64_t current_epos_sec = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);
		  s_Para.http_dl_time_u64 = current_epos_sec;
		 if(client != NULL)
		 {
			esp_http_client_close(client);
			esp_http_client_cleanup(client);
		 }
Send_Ack:
			if((downloaded_len > 0) && (JFS_Response != E_JFS_RESP_TERMINATE) )
			{
				my_JSON  	= cJSON_CreateObject();
				if(my_JSON != NULL)
				{
					if(strcmp(fileRequest.file_name, "A:/firmware.bin") != 0) //Get file CRC for non binary files
					{
						cJSON_AddStringToObject(my_JSON, "FILE_NAME",fileRequest.file_name );
						memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
						cJSON_PrintPreallocated(my_JSON, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
						Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "GET_FILE_CRC");
						cJSON_Delete(my_JSON);
					}
				}
				else
				{
					Add_Response_msg("Error! Failed to allocate memory to cJSON object. Deleting the task..",s_Message_Rx, payLoadData_Fetch);
					goto exit;
				}

				if (xQueueReceive(CRC_Queue, buffer_new, WAIT_FOR_CRC_15_SECONDS) == pdPASS)
				{
					memset(&CRCBuffer, 0, sizeof(CRCBuffer));
					cJSON *data = cJSON_Parse(buffer_new);
					if(data == NULL)
						continue;
					cJSON *CRC 		= cJSON_GetObjectItem(data, "CRC16");
					if((CRC != NULL) && cJSON_IsString(CRC))
						strcpy((char*)CRCBuffer.u16CRC,CRC->valuestring);

					cJSON *FILE_SIZE 		= cJSON_GetObjectItem(data, "FILE_SIZE");
					if((FILE_SIZE != NULL) && cJSON_IsNumber(FILE_SIZE))
						CRCBuffer.File_Size = FILE_SIZE->valueint;

					cJSON_Delete(data);

					cJSON *responseObject2 = cJSON_CreateObject();
					if(responseObject2!=NULL)
					{
						cJSON_AddStringToObject(responseObject2, "file_url", fileRequest.url);
						cJSON_AddStringToObject(responseObject2, "file_path", fileRequest.file_name);
						cJSON_AddStringToObject(responseObject2, "File_crc", (char*)CRCBuffer.u16CRC);
						cJSON_AddNumberToObject(responseObject2, "file_size", CRCBuffer.File_Size);
						if((downloaded_len == content_length) && (strcmp((char*)CRCBuffer.u16CRC, fileRequest.ReceivedCRC) == 0))
							cJSON_AddStringToObject(responseObject2, "Message", "SUCCESS");
						else
							cJSON_AddStringToObject(responseObject2, "Message", "FAILED");

						memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
						cJSON_PrintPreallocated(responseObject2, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
						cJSON_Delete(responseObject2);
						Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "POST_FILE_ACK");
						if(strcmp(fileRequest.file_name, "A:/firmware.bin") == 0)
							start_periodic_timer();  // Start the timer to keep watch on 'ESP_firmware_update' method is received or not
						}
					else
					{
						Add_Response_msg("Error! Failed to allocate memory to cJSON object. Deleting the task..",s_Message_Rx, payLoadData_Fetch);
						goto exit;
					}
				}
			}
			else if(JFS_Response == E_JFS_RESP_TERMINATE)
			{
				cJSON *responseObject2 = cJSON_CreateObject();
				if(responseObject2!=NULL)
				{
					cJSON_AddStringToObject(responseObject2, "file_url", fileRequest.url);
					cJSON_AddStringToObject(responseObject2, "file_path", fileRequest.file_name);
					cJSON_AddStringToObject(responseObject2, "File_crc", (char*)CRCBuffer.u16CRC);
					cJSON_AddNumberToObject(responseObject2, "file_size", -1);
					cJSON_AddStringToObject(responseObject2, "Message", "FAILED");
					memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
					cJSON_PrintPreallocated(responseObject2, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
					Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "POST_FILE_ACK");
					cJSON_Delete(responseObject2);
				}
				else
				{
					Add_Response_msg("Error! Failed to allocate memory to cJSON object. Deleting the task..",s_Message_Rx, payLoadData_Fetch);
					goto exit;
				}
			}
			else
			{
				cJSON *responseObject2 = cJSON_CreateObject();
				if(responseObject2!=NULL)
				{
					cJSON_AddStringToObject(responseObject2, "file_url", fileRequest.url);
					cJSON_AddStringToObject(responseObject2, "file_path", fileRequest.file_name);
					cJSON_AddStringToObject(responseObject2, "File_crc", (char*)CRCBuffer.u16CRC);
					cJSON_AddNumberToObject(responseObject2, "file_size", downloaded_len);
					cJSON_AddStringToObject(responseObject2, "Message", "FAILED");
					memset(payLoadData_Fetch,0,sizeof(payLoadData_Fetch));
					cJSON_PrintPreallocated(responseObject2, payLoadData_Fetch, sizeof(payLoadData_Fetch), false);
					Send_CMD_To_Other_Actor(SYSTEM, "SYSTEM", payLoadData_Fetch, strlen(payLoadData_Fetch), "POST_FILE_ACK");
					cJSON_Delete(responseObject2);
				}
				else
				{
					Add_Response_msg("Error! Failed to allocate memory to cJSON object. Deleting the task..",s_Message_Rx, payLoadData_Fetch);
					goto exit;
				}
			}
			JFS_Size = 0xFFFFFFFF;
			Send_CMD_To_Other_Actor(FILE_SYSTEM,"FILE_SYSTEM", "\0", 0, "GET_VOLUME_INFO"); // check JFS free memory
			}  // end of else
		  }  // end of xQueueReceive CRC_Queue
	    }
      } // end of net status
    }  // end of xQueueReceive fileQueue
  }  // end of while(1)
exit:
    Task_Handle_HTTP_Receive = NULL;
    vTaskDelete(NULL);
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	uint8_t	value=0, temp[10];
	char  str[200] = {0};  //keyValue[200]= {0},
	cJSON *file_data_value = NULL;

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL)
	{
		printf("\n HTTP s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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
				memset(payLoadData,0,sizeof(payLoadData));//\0';
				cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
				printf(">%s.%s(%s, %s)\n", s_Message_Rx->src_Actor_a8, commandKey->valuestring,  payLoadData, THIS_ACTOR);
			}
			cJSON_Delete(in_JSON);
	}
	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL)
		{
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx, payLoadData);
			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"WRITE_FILE1")==0))
		{
			name_JSON 		= cJSON_GetObjectItem(in_JSON, "RESPONSE");
			name_JSON = name_JSON->child;
			if (cJSON_IsNumber(name_JSON) && (!(strcmp(name_JSON->string, "JFS_Resp"))))
			{
				value	= (uint8_t)cJSON_GetNumberValue(name_JSON);
				JFS_Response= value;
			}
			cJSON *Response 		= cJSON_GetObjectItem(in_JSON, "RESPONSE");
			cJSON *CRC_OTA 		= cJSON_GetObjectItem(Response, "CRC16");
			if((CRC_OTA != NULL) && (cJSON_IsString(CRC_OTA)))
			{
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(Response, payLoadData, sizeof(payLoadData), false);
				if(CRC_Queue != NULL)
				{
					uint8_t state = xQueueSend(CRC_Queue, payLoadData, QUE_DELAY);
					if (state != pdTRUE)
					{
					   printf("\n failed to send CRC_Queue ");
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}

		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_VOLUME_INFO")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL) //&& cJSON_IsObject(responseKey->child)
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "Available Memory");
				if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
				{
					JFS_Size = name_JSON->valueint;
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_FILE_CRC")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL) //&& cJSON_IsObject(responseKey->child)
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "CRC16");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(CRC_Queue != NULL)
					{
						uint8_t state = xQueueSend(CRC_Queue, payLoadData, QUE_DELAY);
						if (state != pdTRUE)
						{
						  printf("\n failed to send CRC_Queue ");
						}
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
		}

		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET_FILE_SIZE")==0))
		{
			cJSON *root = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (root != NULL)
			{
				name_JSON 		= cJSON_GetObjectItem(root, "FILE_SIZE");

				if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
				{
					fileUpload.file_size = name_JSON->valueint;
				}
				else
				{
					strcpy(payLoadData,"Delete");
					if(file_UploadQueue != NULL)
					{
						xQueueSend(file_UploadQueue, payLoadData, QUE_DELAY);
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
	    }
	   if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"SMTP_READ")==0))
	   {
		   cJSON *jmsg_object = cJSON_GetObjectItem(in_JSON, "RESPONSE");

		   if ((jmsg_object != NULL)  && (cJSON_IsObject(jmsg_object)))
			{

//			   file_data_value = cJSON_GetObjectItemCaseSensitive(jmsg_object, "RESP");
				file_data_value = cJSON_GetObjectItemCaseSensitive(jmsg_object, "FILE_DATA");
				// Check if "File Data" value exists
				if ((file_data_value != NULL))	// && (smtp_Response == 0))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(file_data_value, payLoadData, sizeof(payLoadData), false);
					size_http  = strlen(payLoadData);

					printf("payLoadData = %s, size_http = %d \n", payLoadData, size_http);

					if(file_UploadQueue != NULL)
					{
						xQueueSend(file_UploadQueue, payLoadData, QUE_DELAY);
					}
				}
				else
				{
					file_data_value = cJSON_GetObjectItemCaseSensitive(jmsg_object, "RESP");
					if ((file_data_value != NULL))	// && (smtp_Response == 0))
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(file_data_value, payLoadData, sizeof(payLoadData), false);
						size_http  = strlen(payLoadData);
						if(file_UploadQueue != NULL)
						{
							xQueueSend(file_UploadQueue, payLoadData, QUE_DELAY);
						}
					}
					else
					{
						memset(payLoadData,0,sizeof(payLoadData));
						cJSON_PrintPreallocated(jmsg_object, payLoadData, sizeof(payLoadData), false);
						size_http  = strlen(payLoadData);
						if(file_UploadQueue != NULL)
						{
							xQueueSend(file_UploadQueue, payLoadData, QUE_DELAY);
						}
					}
				}
			}
			cJSON_Delete(in_JSON);
			return;
	   }

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
			name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
			{
				strcpy((char*)temp,name_JSON->valuestring);
				NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
}


// Function to handle HTTP GET request
static void http_get_file(AMessage_st* s_Message_Rx)
{

    if (Task_Handle_HTTP_Upload == NULL)
    {
    	if (file_UploadQueue == NULL)
    		file_UploadQueue = xQueueCreateStatic(UPLOAD_FILE_QUE_COUNT, UPLOAD_FILE_QUE_COUNT_ITEMSIZE, Upload_pucQueueStorage, &Upload_pxQueueBuffer);
        if (Task_Handle_HTTP_Upload == NULL)
        {
        	Task_Handle_HTTP_Upload = xTaskCreateStaticPinnedToCore(http_file_reader, "http_file_reader", HTTP_FILE_TASK_STACK_DEPTH, s_Message_Rx, HTTP_FILE_TASK_PRIORITY, xUploadTaskStack,&xUploadTaskBuffer,1);
			if(Task_Handle_HTTP_Upload == NULL)
			{
                Add_Response_msg("Failed to create HTTP Upload task.", s_Message_Rx, payLoadData);
            }
        }
        else
        {
            Add_Response_msg("Failed to create Upload queue.", s_Message_Rx, payLoadData);
        }
    }
}
// Function to handle HTTP GET request
static void http_get_request(AMessage_st* s_Message_Rx)
{
    if (Task_Handle_HTTP_Receive == NULL)
    {
    	if (fileQueue == NULL)
    	fileQueue = xQueueCreateStatic(HTTP_FETCH_QUE_LENGTH, sizeof(FileRequest_t), Fetch_pucQueueStorage, &Fetch_pxQueueBuffer);
        if (Task_Handle_HTTP_Receive == NULL)
        {
			Task_Handle_HTTP_Receive = xTaskCreateStaticPinnedToCore(http_stream_reader, "http_stream_reader", HTTP_FETCH_TASK_STACK_DEPTH, s_Message_Rx, HTTP_FETCH_TASK_PRIORITY, xFetchTaskStack,&xFetchTaskBuffer,1);
			if(Task_Handle_HTTP_Receive == NULL)
			{
                Add_Response_msg("Failed to create HTTP receive task.", s_Message_Rx, payLoadData);
            }
        }
        else
        {
            Add_Response_msg("Failed to create file queue.", s_Message_Rx, payLoadData);
        }
    }

    cJSON *in_JSON = cJSON_Parse((char *)s_Message_Rx->payload_p8);
    if (in_JSON == NULL) {
        Add_Response_msg("Invalid JSON input.", s_Message_Rx, payLoadData);
        return;
    }

    FileRequest_t fileRequest;
    cJSON *name_JSON = cJSON_GetObjectItem(in_JSON, "FILE_NAME");
    if (name_JSON != NULL && name_JSON->valuestring != NULL) {

    	if (isValidFileName(name_JSON->valuestring) &&
    	                		isValidFilePath(name_JSON->valuestring))
    	{
			strcpy(fileRequest.file_name, name_JSON->valuestring);

    	}
    	else
    	{
    		Add_Response_msg("Received Invalid File name/path in fetch file",s_Message_Rx, payLoadData);
    	    Add_Response_msg("Kindly enter the correct filepath & filename. Use drive 'A:/' for JFS and 'B:/' for SD card.",s_Message_Rx, payLoadData);
    	    return;
    	}
    }

    name_JSON = cJSON_GetObjectItem(in_JSON, "URL");
    if (name_JSON != NULL && name_JSON->valuestring != NULL) {
        strcpy(fileRequest.url, name_JSON->valuestring);
    }

    name_JSON = cJSON_GetObjectItem(in_JSON, "MODE");
    if (name_JSON != NULL && name_JSON->valuestring != NULL) {
        strcpy(fileRequest.mode, name_JSON->valuestring);
    }

    // Check for "PURGE" parameter
       name_JSON = cJSON_GetObjectItem(in_JSON, "PURGE");
       bool purgeQueue = false;
       if (name_JSON != NULL && name_JSON->valuestring != NULL && strcmp(name_JSON->valuestring, "Y") == 0) {
           purgeQueue = true;
       }

     UBaseType_t waitingFiles = uxQueueMessagesWaiting(fileQueue);
     if(waitingFiles <= 2)
    	 purgeQueue = false;

       name_JSON = cJSON_GetObjectItem(in_JSON, "CRC");
	  if (name_JSON != NULL && name_JSON->valuestring != NULL) {
		  strcpy(fileRequest.ReceivedCRC, (name_JSON->valuestring+2));//+2 to ignore "0x"
	  }
	  else
	  {
		  strcpy(fileRequest.ReceivedCRC, "");
	  }

      name_JSON = cJSON_GetObjectItem(in_JSON, "AckIfFileExists");
	  if ((name_JSON != NULL) && (cJSON_IsTrue(name_JSON))) {
		  fileRequest.ReceivedAckIfFileExists = true;
	  }
	  else
	  {
		  fileRequest.ReceivedAckIfFileExists = false;
	  }

    cJSON_Delete(in_JSON);
    if (fileQueue != NULL) {
    	 char response[100];
    	if (purgeQueue) {
    	            // Purge the queue
    	            xQueueReset(fileQueue);
    	            uint8_t state = xQueueSend(fileQueue, &fileRequest, portMAX_DELAY);
					if (state != pdTRUE)
					{
					  printf("\n failed to send fileQueue ");
					}


    	            snprintf(response, sizeof(response), "Queue was purged and starting over again");
    	        } else
    	        {
    	            // Add request to the queue
    	        	uint8_t state = xQueueSend(fileQueue, &fileRequest, portMAX_DELAY);
					if (state != pdTRUE)
					{
					  Add_Response_msg("failed to send fileQueue for HTTP fetch task", s_Message_Rx, payLoadData);
					}
    	            UBaseType_t waitingFiles = uxQueueMessagesWaiting(fileQueue);
    	            snprintf(response, sizeof(response), "Task already running. Waiting files: %d", waitingFiles);
    	        	cJSON *my_JSON  	= cJSON_CreateArray();  // get net status
    	        	if (my_JSON == NULL) {
    	        	        Add_Response_msg("Failed to create JSON object for network status.", s_Message_Rx, payLoadData);
    	        	        return;
    	        	    }
    	        	cJSON_AddItemToArray(my_JSON, cJSON_CreateString("NET_STATUS"));
    	        	cJSON *jsonObject = cJSON_CreateObject();
    	        	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON);
    	        	memset(payLoadData,0,sizeof(payLoadData));
    	        	cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
    	        	cJSON_Delete(jsonObject);
    	        	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData, strlen(payLoadData), "GET");
    	        }
    	 Add_Response_msg(response, s_Message_Rx, payLoadData);
    }
}


esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
	static char *output_buffer; // Buffer to store response of http request from event handler
	static int output_len=0;       // Stores number of bytes read
    switch (evt->event_id)
    {
		case HTTP_EVENT_ON_DATA:

								{
									// If user_data buffer is configured, copy the response into the buffer

									if (evt->user_data)
									{
										memcpy(evt->user_data + output_len, evt->data, evt->data_len);
									}
									else
									{
										if (output_buffer == NULL)
										{
											output_buffer = (char*) heap_caps_malloc(esp_http_client_get_content_length(evt->client), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
											output_len = 0;
											if (output_buffer == NULL)
											{
												printf("Memory allocation failed\n");
												return ESP_FAIL;
											}
										}
										memcpy(output_buffer + output_len, evt->data, evt->data_len);
									}
									output_len += evt->data_len;
								}
								break;

		case HTTP_EVENT_ON_FINISH:
								if (output_buffer != NULL)
								{
									free(output_buffer);
									output_buffer = NULL;
								}
								output_len = 0;
								break;
			case HTTP_EVENT_DISCONNECTED:
								if (output_buffer != NULL) {
									free(output_buffer);
									output_buffer = NULL;
								}
								output_len = 0;
								break;

			case HTTP_EVENT_ERROR: 			//printf("\n HTTP_EVENT_ERROR \n");   		 break;
			case HTTP_EVENT_ON_CONNECTED: 	//printf("\n HTTP_EVENT_ON_CONNECTED \n");     break;
			case HTTP_EVENT_REDIRECT: 		//printf("\n HTTP_EVENT_REDIRECT \n");   		 break;

		default:
        						break;
    }
    if (output_buffer != NULL)
    {
		free(output_buffer);
		output_buffer = NULL;
	}
    return ESP_OK;
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

static void http_get_key(void *pvParameters __attribute__((unused)))
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	AMessage_st s_Message_Rx;
	char *HTTP_Data = NULL;
	char str[600]={0};
	char content_len[20] = {0}, Link[200] = {0};
    char Retry_count = 0;
    uint8_t 	APIkey_Value[100]= {0};
    uint32_t 	HTTP_retry_interval_u32_Value=0;
    uint8_t 	DeviceId[32]= {0};
    esp_err_t err = 0;
    char Net_Chk_Retry = 0;
	int httpstatus=0;
    esp_http_client_handle_t client = NULL;
	while(NET_status != E_NET_CONNECTED);
	while(1)
	{
			if (pdTRUE == xQueueReceive(Post_Queue, (void*) (&s_Message_Rx), portMAX_DELAY))
			{
				do{
					Net_Chk_Retry++;
					if(NET_status == E_NET_CONNECTED)
						{break;}
					else
						{vTaskDelay(500);}
				}while(Net_Chk_Retry < 5);
				if(NET_status != E_NET_CONNECTED)
				{
					Add_Response_msg("Error! Device is not connected to an Internet.",&s_Message_Rx, payLoadData_Post);
					continue;
				}
				strncpy(http_payload_buffer, (char*)s_Message_Rx.payload_p8, 2047);
				if(s_Message_Rx.payload_p8 != NULL)
				{
					heap_caps_free(s_Message_Rx.payload_p8);
				}
				s_Message_Rx.payload_p8 = (uint8_t*) http_payload_buffer;
				if((char*)s_Message_Rx.payload_p8 == NULL)
				{
					printf("\n\n HTTP post payload is NULL \n\n");
					continue;
				}

				in_JSON 		= cJSON_Parse((char*)s_Message_Rx.payload_p8);
				printf("<HTTP.POST(%s)\n", s_Message_Rx.payload_p8);
				if (in_JSON == NULL)
				{
					sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,&s_Message_Rx, payLoadData_Post);
					goto exit;

				}
				name_JSON 		= cJSON_GetObjectItem(in_JSON, "URL");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
					strcpy(Link,name_JSON->valuestring);

				name_JSON 		= cJSON_GetObjectItem(in_JSON, "HTTP_PAYLOAD");
				if((name_JSON != NULL))
				{
					memset(payLoadData_Post,0,sizeof(payLoadData_Post));
					cJSON_PrintPreallocated(name_JSON, payLoadData_Post, sizeof(payLoadData_Post), false);
					sprintf(content_len,"%d",strlen(payLoadData_Post));
				}
				name_JSON 		= cJSON_GetObjectItem(in_JSON, "API_KEY");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					strcpy((char*)APIkey_Value,name_JSON->valuestring);

				name_JSON 		= cJSON_GetObjectItem(in_JSON, "DEVICEID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
					strcpy((char*)DeviceId,name_JSON->valuestring);

				name_JSON 		= cJSON_GetObjectItem(in_JSON, "RETRY_PERIOD");
				if((name_JSON != NULL) && (cJSON_IsNumber(name_JSON)))
					HTTP_retry_interval_u32_Value = name_JSON->valueint;

				payLoadData_Save[0] = 0;
				sprintf(payLoadData_Save, "%s - %s", "Device Announce Post", (char *)s_Message_Rx.payload_p8);
		        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Save, strlen(payLoadData_Save), "SAVE_WIFI_AUDIT_LOG");

				cJSON_Delete(in_JSON);

				memset(local_response_buffer, 0, sizeof(local_response_buffer));
				esp_http_client_config_t config_post = {
					.url = Link,
					.method = HTTP_METHOD_POST,
					.transport_type = HTTP_TRANSPORT_OVER_SSL,//HTTP_TRANSPORT_OVER_TCP, //HTTP_TRANSPORT_OVER_TCP
					.crt_bundle_attach = esp_crt_bundle_attach,
					.cert_pem = (const char *) server_root_cert_pem_start, //NULL,
					.cert_len = server_root_cert_pem_end - server_root_cert_pem_start,
					.user_data = local_response_buffer,        // Pass address of local buffer to get response
					.buffer_size = sizeof(local_response_buffer),
					.disable_auto_redirect = true,
					.max_redirection_count=10,
					.timeout_ms = 10000,
					.event_handler = client_event_post_handler};

			Retry_count = 0;
			do   // after inserting esp_http_client_init and esp_http_client_cleanup in do_while loop, ESP_ERR_HTTP_CONNECT issue is resolved
			{
				client = NULL;
				client = esp_http_client_init(&config_post);
				if(client != NULL)
				{
					esp_http_client_set_post_field(client, payLoadData_Post, strlen(payLoadData_Post));
					esp_http_client_set_header(client, "x-api-key",(char*)APIkey_Value);//
					esp_http_client_set_header(client, "Device",(char*)DeviceId);  //"225_SAT1");//  // source info//"SAT_TESTDEV1");
					esp_http_client_set_header(client, "Content-Type", "application/json");
					esp_http_client_set_header(client, "Cache-Control","no-cache");
					esp_http_client_set_header(client, "Connection","keep-alive");
					esp_http_client_set_header(client, "Content-Length",content_len);

					httpstatus = 0;
					err = esp_http_client_perform(client);
					if (err == ESP_OK)
					{
						httpstatus=esp_http_client_get_status_code(client);
						s_Para.Connection_status = E_SERVER_CONNECTED;
						break;
					}
					else
					{
						esp_http_client_close(client);
						esp_http_client_cleanup(client);  // clean up client even if esp_http_client_perform failed
						client = NULL;
					}
				}
				Retry_count++;
				if(((Retry_count >= s_Para.Retry_Cnt) && (client == NULL) ))  //|| (client == NULL)
				{
					Add_Response_msg("Error! HTTP client initialization is failed.", &s_Message_Rx, payLoadData_Post);
					break;
				}
				vTaskDelay(pdMS_TO_TICKS(HTTP_retry_interval_u32_Value));
			}while((Retry_count < s_Para.Retry_Cnt) && (NET_status == E_NET_CONNECTED));

			if(((Retry_count >= s_Para.Retry_Cnt) ))  //|| (client == NULL)
			{
				sprintf(str, "HTTP client initialization is failed. Failed reason is %s", esp_err_to_name(err));
				s_Para.Connection_status = E_SERVER_NOT_CONNECTED;
				cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
				cJSON_AddNumberToObject(responseObject, "HTTP STATUS",httpstatus);
				cJSON_AddStringToObject(responseObject, "ERROR",str);
				memset(payLoadData_Post,0,sizeof(payLoadData_Post));
				cJSON_PrintPreallocated(responseObject, payLoadData_Post, sizeof(payLoadData_Post), false);
				strcpy((char*)s_Message_Rx.payload_p8, payLoadData_Post);
				cJSON_Delete(responseObject);
				console_send_responce_to_console_xface(&s_Message_Rx);
				continue;
			}
				if(httpstatus!=200)
				{
					char string[200] = {0};
					s_Para.Connection_status = E_SERVER_NOT_CONNECTED;
					cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddNumberToObject(responseObject, "HTTP STATUS",httpstatus);
					switch(httpstatus)
					{
						case 400: strcpy(string, "Bad Request");  break;
						case 401: strcpy(string, "Wrong API key");  break;
						case 404: strcpy(string, "The server cannot find the requested resource");  break;
						case 405: strcpy(string, "Invalid URL. The request method is not supported for the requested resource");  break;
						case 408: strcpy(string, "Server timeout");  break;
						default:  sprintf(string, "Error code:%d, Failed reason: %s (Network Connectivity Issues/Server Unavailability/Firewall or Security Settings))", esp_http_client_get_status_code(client), esp_err_to_name(err));	break;
					}
					cJSON_AddStringToObject(responseObject, "ERROR",string);

					memset(payLoadData_Post,0,sizeof(payLoadData_Post));
					cJSON_PrintPreallocated(responseObject, payLoadData_Post, sizeof(payLoadData_Post), false);
					strcpy((char*)s_Message_Rx.payload_p8, payLoadData_Post);
					cJSON_Delete(responseObject);
					console_send_responce_to_console_xface(&s_Message_Rx);
				}
				else if((local_response_buffer[0] != '\0') && ((httpstatus==200)))
				{
					if(strlen(local_response_buffer) == 0)
					{
						if(client != NULL)
						{
							esp_http_client_close(client);
							esp_http_client_cleanup(client);
							client = NULL;
						}
						continue;
					}
					cJSON *root = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddNumberToObject(root, "HTTP STATUS",httpstatus);
					cJSON *responseObject = cJSON_Parse((char*)config_post.user_data);  	// Create a RESPONSE object and add it to the RESPONSE array
		     		if (responseObject == NULL) {
		     			sprintf(str,"1 Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		     			Add_Response_msg(str,&s_Message_Rx, payLoadData_Post);
		     			goto exit;
		     		}
		     		cJSON_AddItemToObject(root, "MESSAGE", responseObject);
		     		memset(local_response_buffer, 0, 4096);
					memset(payLoadData_Post,0,sizeof(payLoadData_Post));
					cJSON_PrintPreallocated(root, payLoadData_Post, sizeof(payLoadData_Post), false);
					strcpy((char*)s_Message_Rx.payload_p8, payLoadData_Post);

					cJSON_Delete(root);
					console_send_responce_to_console_xface(&s_Message_Rx);
					
					strcpy((char*)s_Message_Rx.payload_p8, payLoadData_Post);
					Process_Post_Method_Response(&s_Message_Rx);
				}
				else  // Send HTTP status only. No response received from server
				{
					cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
					cJSON_AddNumberToObject(responseObject, "HTTP STATUS",httpstatus);
					if(HTTP_Data != NULL)
					{
						cJSON *file_path = cJSON_Parse(HTTP_Data);
						cJSON *file = cJSON_GetObjectItem(file_path, "file_path");
						if((file != NULL) && (cJSON_IsString(file)))
							if(file->valuestring != NULL)
								cJSON_AddStringToObject(responseObject, "file_path",file->valuestring);
						cJSON_Delete(file_path);
					}
					memset(payLoadData_Post,0,sizeof(payLoadData_Post));
					cJSON_PrintPreallocated(responseObject, payLoadData_Post, sizeof(payLoadData_Post), false);
					strcpy((char*)s_Message_Rx.payload_p8, payLoadData_Post);
					cJSON_Delete(responseObject);
					console_send_responce_to_console_xface(&s_Message_Rx);
				}
				if(client != NULL)
				{
					esp_http_client_close(client);
					esp_http_client_cleanup(client);
					client = NULL;
				}
			}  // end of if que receive
	}  // end of while (1)
exit:
	printf("\n\n delete the post task \n\n");
	if(HTTP_Data != NULL)
	{
		cJSON_free(HTTP_Data);
		HTTP_Data = NULL;
	}
	PostHandle = NULL;
	vTaskDelete(PostHandle);  // Delete the task
}


static void Process_Post_Method_Response(AMessage_st* s_Message_Rx)
{
	cJSON *name_JSON = NULL;
	cJSON *root = NULL;
	char Database_Name[50] = {0}, str[200] = {0};
	int Record_ID = 0, Record_Cnt = 0;
	root = cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (root == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx, payLoadData_Post);
		return;
	}

	 cJSON *Message = cJSON_GetObjectItem(root, "MESSAGE");
	 if(Message == NULL)
	 {
		  Add_Response_msg("No response received from server",s_Message_Rx, payLoadData_Post);
		  char str1[100] = {0};
		  strcpy(str1, "Device Announce - No response received from server.");
		  Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", str1, strlen(str1), "SAVE_WIFI_AUDIT_LOG");
		  cJSON_Delete(root);
		  return;
	 }

    cJSON *sessionId = cJSON_GetObjectItem(Message, "sessionId");
    if (cJSON_IsString(sessionId) && (sessionId->valuestring != NULL))
    {

        strcpy((char*)s_Para.sessionId,sessionId->valuestring);
    }

    cJSON *UTCTime = cJSON_GetObjectItem(Message, "utcTime");
     if (cJSON_IsNumber(UTCTime) && (UTCTime != NULL))
     {
		struct tm RTC_time = {0};
		uint8_t timems = 0;
		time_t tOfDay = 0;
		i2c_dev_t *dev = pcf8563_get_device_handle();
		if (pcf8563_get_time_ms(dev, &RTC_time, &timems) == ESP_OK) {
			RTC_time.tm_year-=1900;
			tOfDay = mktime(&RTC_time);
		}
	   int RTC_milliseconds = timems * 10;
	   int64_t RTC_Epoch = (tOfDay * 1000) + RTC_milliseconds;
	   int64_t diff = llabs(RTC_Epoch - (int64_t)UTCTime->valuedouble);
	   cJSON *root = cJSON_CreateObject();
	   cJSON_AddNumberToObject(root, "RTC_EPOC", RTC_Epoch);
	   cJSON_AddNumberToObject(root, "DEVICE ANNOUNCE", UTCTime->valuedouble);
	   cJSON_AddNumberToObject(root, "DIFFERENCE", diff);
	   if (diff >= 30000)    // if difference is greater than 30 sec then write to internal and external RTC.
	   {
		   cJSON *responseObject = cJSON_CreateObject();
		   cJSON_AddNumberToObject(responseObject, "EPOCH_MILLS", UTCTime->valuedouble);
		   memset(payLoadData_Post,0,sizeof(payLoadData_Post));
		   cJSON_PrintPreallocated(responseObject, payLoadData_Post, sizeof(payLoadData_Post), false);
		   Send_CMD_To_Other_Actor(NTP, "NTP", payLoadData_Post, strlen(payLoadData_Post), "SET_INITIAL_RTC");
		   cJSON_Delete(responseObject);
		   cJSON_AddBoolToObject(root, "ADJUSTMENT APPLIED", true);
	   }
	   else
	   {
		   cJSON_AddBoolToObject(root, "ADJUSTMENT APPLIED", false);
	   }
		memset(payLoadData_Post,0,sizeof(payLoadData_Post));
		cJSON_PrintPreallocated(root, payLoadData_Post, sizeof(payLoadData_Post), false);
		strcpy((char*)s_Message_Rx->payload_p8, payLoadData_Post);
		cJSON_Delete(root);
		payLoadData_Save[0] = 0;
		sprintf(payLoadData_Save, "%s - %s", "Successful Device Announce", (char *)s_Message_Rx->payload_p8);
        Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", payLoadData_Save, strlen(payLoadData_Save), "SAVE_WIFI_AUDIT_LOG");
		console_send_responce_to_console_xface(s_Message_Rx);
     }

    cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(Message, "methods");
    if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
    {
    	int arraySize = cJSON_GetArraySize(methodsArray);
		// Loop through each element in the array
		for (int i = 0; i < arraySize; i++) {
			cJSON *element = cJSON_GetArrayItem(methodsArray, i);
			if (cJSON_IsString(element) && (element->valuestring != NULL)) {
				if(strlen(element->valuestring) == 0)
					continue;
				/* execute the command */
				memset(line,0,sizeof(line));
				strcpy(line, element->valuestring);
				int ret;
				if(line[0]=='<')
				{
					if(strcmp(line, "<FILE_SYSTEM.ESP_FW_UPDATE({\"FILE_NAME\" : \"A:/firmware.bin\"})") == 0)
					{
						printf("\n Stop the timer. \n");
						esp_timer_stop(periodic_timer);
					}
					esp_err_t err = esp_console_run_Custom(line, &ret, THIS_ACTOR);
					if (err == ESP_ERR_NOT_FOUND) {
#ifdef ENABLE_PRINT_MSG
						printf("Unrecognized command\n");
#endif
					} else if (err == ESP_ERR_INVALID_ARG) {
						// command was empty
					} else if (err == ESP_OK && ret != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
						printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
#endif

					} else if (err != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
						printf("Internal error: %s\n", esp_err_to_name(err));
#endif
					}
				}
			}
			else
			{
#ifdef ENABLE_PRINT_MSG
				printf("Element %d: %s\n", i, element->string);
#endif
			}
		}
        cJSON_Delete(root);
        return;
    }
// Logic for fetch record data
       // Navigate to the 'data' object
       cJSON *data = cJSON_GetObjectItemCaseSensitive(Message, "data");
       if (data == NULL) {
           Add_Response_msg("Error: 'data' object not found.",s_Message_Rx, payLoadData_Post);
           cJSON_Delete(root);
           return;
       }
	name_JSON 		= cJSON_GetObjectItem(data, "command");
	if((name_JSON != NULL) && name_JSON->valuestring!= NULL)
	{
		if((strcmp(name_JSON->valuestring,"GET RECORD DATA")==0))
		{
			 cJSON *name_JSON_new		= cJSON_GetObjectItem(data, "data");
			 if((name_JSON_new != NULL) && (cJSON_IsObject(name_JSON_new)))
			 {
				 name_JSON 		= cJSON_GetObjectItem(name_JSON_new, "dbName");
				  if((name_JSON!= NULL)  && (name_JSON->valuestring != NULL))
					  strcpy(Database_Name, name_JSON->valuestring);

				  name_JSON 		= cJSON_GetObjectItem(name_JSON_new, "startingRecord");
				  if((name_JSON!= NULL)  && (name_JSON->valuedouble != 0))
					  Record_ID = name_JSON->valuedouble;

				  name_JSON 		= cJSON_GetObjectItem(name_JSON_new, "recordCount");
				  if((name_JSON!= NULL)  && (name_JSON->valuedouble != 0))
					  Record_Cnt = name_JSON->valuedouble;

				  if((strlen(Database_Name) != 0) && (Record_ID > 0) && (Record_Cnt > 0))
				  {
					  cJSON *root_new = cJSON_CreateObject();
					  cJSON_AddStringToObject(root_new, "DB_NAME", Database_Name);
					  cJSON_AddNumberToObject(root_new, "ID_START", Record_ID);
					  cJSON_AddNumberToObject(root_new, "ID_END", Record_Cnt);
					  memset(payLoadData_Post,0,sizeof(payLoadData_Post));
					  cJSON_PrintPreallocated(root_new, payLoadData_Post, sizeof(payLoadData_Post), false);	   
					  Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData_Post, strlen(payLoadData_Post), "FETCH_RECORD");
					  cJSON_Delete(root_new);
				  }
				  else
					  Add_Response_msg("Error: Incorrect response received from server.",s_Message_Rx, payLoadData_Post);
				}
			  else
			  {
				cJSON *methodsArray = cJSON_GetObjectItemCaseSensitive(data, "methods");
				if ((methodsArray != NULL) && (cJSON_IsArray(methodsArray)))
				{
					int arraySize = cJSON_GetArraySize(methodsArray);

					// Loop through each element in the array
					for (int i = 0; i < arraySize; i++) {
						cJSON *element = cJSON_GetArrayItem(methodsArray, i);
						if (cJSON_IsString(element) && (element->valuestring != NULL))
						{
							/* execute the command */
							memset(line,0,sizeof(line));
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
									printf("Command returned non-zero error code: 0x%x (%s)\n", ret,esp_err_to_name(ret));
#endif

								} else if (err != ESP_OK) {
#ifdef ENABLE_PRINT_MSG
									printf("Internal error: %s\n", esp_err_to_name(err));
#endif
								}
//								vTaskDelay(100/portTICK_PERIOD_MS);  //Speed-up
							}
						}
						else
						{
#ifdef ENABLE_PRINT_MSG
							printf("Invalid method %d: %s\n", i, element->string);
#endif
						}
					}
				}
				else
				Add_Response_msg("Error: Incorrect response received from server.",s_Message_Rx, payLoadData_Post);
			}
		}
    else
    {
		 Add_Response_msg("Error: Incorrect response received from server. ",s_Message_Rx, payLoadData_Post);
    }
  }
	cJSON_Delete(root);
}


static void Add_Response_msg(char* buffer_data, AMessage_st* s_Message_Rx, char *payLoadData_new)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer_data);
	payLoadData_new[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void Deinit_Actor(AMessage_st* s_Message_Rx)
{
	if(FirstEntry_bool)
	{
		FirstEntry_bool = false;
//		xQueueReset(fileQueue);  // clear http get queue
//		xQueueReset(Post_Queue); // clear http post queue

		if(Task_Handle != NULL)
		{
			vTaskDelete(Task_Handle);
			Task_Handle = NULL;
		}
	}
}

static int isValidFileName(const char *fileName) {
    if (fileName == NULL || strlen(fileName) == 0) {
        return 0; // Invalid if null or empty
    }
    if (strlen(fileName) > MAX_FILE_NAME_LEN) {
        return 0; // Invalid if filename is too long
    }
    return 1; // Valid file name
}

static int isValidFilePath(const char *filePath) {
    if (filePath == NULL || strlen(filePath) == 0) {
        return 0; // Invalid if null or empty
    }

    // Check for valid file path characters starting with A:/ or B:/
    regex_t regex;
    int reti;
    reti = regcomp(&regex, "^[a-bA-B]:/[a-zA-Z0-9_./:%+-]*$", REG_EXTENDED);
    if (reti) {
        fprintf(stderr, "Could not compile regex\n");
        return 0;
    }
    reti = regexec(&regex, filePath, 0, NULL, 0);
    regfree(&regex);
    if (reti) {
        return 0; // Invalid if regex doesn't match
    }
    return 1; // Valid file path
}

static void http_post_request(AMessage_st* s_Message_Rx)
{
	AMessage_st s_Message_Rx_data;
	strcpy((char*)s_Message_Rx_data.dest_Actor_a8, (char*)s_Message_Rx->dest_Actor_a8);
	strcpy((char*)s_Message_Rx_data.src_Actor_a8, (char*)s_Message_Rx->src_Actor_a8);
	strcpy((char*)s_Message_Rx_data.cmdFun_a8, (char*)s_Message_Rx->cmdFun_a8);
	s_Message_Rx_data.payload_size = s_Message_Rx->payload_size;
	uint8_t *POST_buffer = (uint8_t*) heap_caps_calloc ((s_Message_Rx->payload_size + 1) ,sizeof(uint8_t), MALLOC_CAP_SPIRAM| MALLOC_CAP_8BIT);
	if(POST_buffer == NULL)
	{
		printf("\n Failed to allocate memory \n\n");
		return;
	}
	if((char*)s_Message_Rx->payload_p8 != NULL)
	{
		strcpy((char*)POST_buffer, (char*)s_Message_Rx->payload_p8);
	}
	s_Message_Rx_data.payload_p8 = POST_buffer;

    cJSON *my_JSON  	= cJSON_CreateArray();
	if (my_JSON == NULL) {
		Add_Response_msg("Failed to create JSON object for network status.", s_Message_Rx, payLoadData);
		return;
	}
	cJSON_AddItemToArray(my_JSON, cJSON_CreateString("NET_STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(jsonObject, payLoadData, sizeof(payLoadData), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData, strlen(payLoadData), "GET");

    if (PostHandle == NULL)
    {
    	if (Post_Queue == NULL)
    	{
    		Post_Queue = xQueueCreateStatic(HTTP_POST_QUE_LENGTH, sizeof(AMessage_st), Post_pucQueueStorage, &Post_pxQueueBuffer);
		}
        if (Post_Queue != NULL)
        {
			PostHandle = xTaskCreateStaticPinnedToCore(http_get_key, "POST TASK", HTTP_POST_TASK_STACK_DEPTH, NULL, HTTP_POST_TASK_PRIORITY, xPostTaskStack,&xPostTaskBuffer,1);
			if(PostHandle == NULL)
			{
                Add_Response_msg("Failed to create HTTP post task.", s_Message_Rx, payLoadData);
                return;
            }
        }
        else
        {
            Add_Response_msg("Failed to create post task queue.", s_Message_Rx, payLoadData);
            return;
        }
    }
	if (Post_Queue != NULL) {
	        if (xQueueSend(Post_Queue, &s_Message_Rx_data, QUE_DELAY) != pdTRUE) {
	            Add_Response_msg("Failed to send to post task queue.", s_Message_Rx, payLoadData);
	        }
	    }
}


static void start_periodic_timer(void)
{
	if(periodic_timer == NULL)
	{
		esp_err_t err = esp_timer_create(&periodic_timer_args_new, &periodic_timer);
		if(err != ESP_OK)
		{
			printf("\n\n Error! failed to create period timer in HTTP actor err- %d\n\n", err);
			return;
		}
		err = esp_timer_start_periodic(periodic_timer, (120 *1000000)); // start the timer with the period of 120 sec
		if(err != ESP_OK)
			printf("\n\n Error in start periodic timer. err = %d\n\n", err);
	}
}

static void periodic_timer_callback(void* arg)
{
	esp_timer_stop(periodic_timer);
	cJSON *my_JSON = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
	cJSON_AddNumberToObject(my_JSON, "SER_TASK_DIS_FLAG", 1);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(my_JSON, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData, strlen(payLoadData), "SETSERVCONNFLAG");
	cJSON_Delete(my_JSON); // Free the cJSON object

	cJSON *my_JSON1 = cJSON_CreateObject();  // send command to SQL to fetch the records which are asked by server
	cJSON_AddNumberToObject(my_JSON1, "EVT_TASK_DIS_FLAG", 1);
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(my_JSON1, payLoadData, sizeof(payLoadData), false);
	Send_CMD_To_Other_Actor(EVENT_ACTOR,"EVENT_ACTOR", payLoadData, strlen(payLoadData), "SETEVTTASKFG");
	cJSON_Delete(my_JSON1); // Free the cJSON object

	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", "\0", 0, "STACK_RESET");
}
//------------------------ HTTP Actor Methods Ends  ------------------------------------------------------------------------//
