/*
 * UART_Actor.c
 *
 *  Created on: 18-Jul-2022
 *      Author: Ashwini
 */


/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include "actor.h"
#include "Config.h"
#include "UART_Actor.h"
#include "console_Actor.h"
#include <stdio.h>
#include "driver/uart.h"
/**
 * This is an example which echos any data it receives on configured UART back to the sender,
 * with hardware flow control turned off. It does not use UART driver event queue.
 *
 * - Port: configured UART
 * - Receive (Rx) buffer: on
 * - Transmit (Tx) buffer: off
 * - Flow control: off
 * - Event queue: off
 * - Pin assignment: see defines below (See Kconfig)
 */

//------------------------ UART Actor Resources -------------------------------------------------//

/* Configure parameters of an UART driver,
 * communication pins and install the driver */
BaseType_t Uart0EventTask;
TaskHandle_t UartEventHandle;
QueueHandle_t Uart0_event_queue;

uart_config_t uart_config = {
    .baud_rate 	= ECHO_UART_BAUD_RATE,
    .data_bits 	= UART_DATA_8_BITS,
    .parity    	= UART_PARITY_DISABLE,
    .stop_bits 	= UART_STOP_BITS_1,
    .flow_ctrl 	= UART_HW_FLOWCTRL_DISABLE,
    .source_clk = UART_SCLK_APB,
};

static struct property prop[] = // Actor Property
{
    { &uart_config.baud_rate,    "BAUD_RATE",     U_INT32,  "RW",  "Baud rate for UART communication" },
    { &uart_config.data_bits,    "DATA_BITS",     U_INT16,  "RW",  "Number of data bits for UART communication" },
    { &uart_config.parity,       "PARITY",        U_INT8,   "RW",  "Parity setting for UART communication" },
    { &uart_config.stop_bits,    "STOP_BITS",     U_INT8,   "RW",  "Number of stop bits for UART communication" }
};
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static char s_tmp_line_buf[MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN];
PSRAM_ATTR_BSS static StackType_t xTaskStack[UART_TASK_STACK_DEPTH];
//-------------------------- Actor Parameters ------------------------------/

#define  UART_READ_WRITE_SIZE COMMAND_LEN
#define UART_Que_Count  10
//#define UART_TASK_STACK_DEPTH (8*1024)

static const char 			*THIS_ACTOR 	= 	"UART";
static const char 			THIS_ACTOR_ID 	= 	UART;

PSRAM_ATTR_BSS static char 				RxBuff[UART_READ_WRITE_SIZE];
TaskHandle_t 		uart_Handle 	= NULL;	// uart Task Handler
static QueueHandle_t 		msg_Rx_Queue 	= NULL;;	// uart RX Queue

TaskHandle_t 	uart_Handle; 	// uart Task Handler
static StaticTask_t xUARTTaskBuffer;  //// Declare a static task control block
PSRAM_ATTR_BSS static uint8_t pucQueueStorage [UART_OBJ_QUE_COUNT * sizeof(AMessage_st)];
static StaticQueue_t pxQueueBuffer;
static SemaphoreHandle_t s_console_cmd_mutex = NULL;
	 					// Init the actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					// Set or Change a parameter
static void get				(char *prop, char *val_a8);					// Get or Read a Parameter
static void help(AMessage_st* s_Message_Rx);											// Print help for actor use
static void monitor			(void *pvParameters __attribute__((unused)));   // Execute the Actor
static void Uart0_Event		(void *pvParameters);
static void process_uart_command_frame(char *cmd_frame);
static size_t parse_uart_command_frame(char *cmd_frame, char **argv);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Get_Property(AMessage_st* s_Message_Rx);
static char Dest_actor_que_sel(char *str_prop);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties

static bool	FirstEntry_bool	= false;
/** run-time configuration options */
static esp_console_config_t s_config;

//----------------------- Commen Actor Methods ------------------------------------------------------------------//

void Debug_Uart_Init(void *a, void *b)
{
	// Calculate the total size of memory required for the stack array
	//size_t stack_size = UART_TASK_STACK_DEPTH * sizeof(StackType_t);

	if(FirstEntry_bool)
	{
		return;
	}

 #if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
	 uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &Uart0_event_queue, 0);
	 uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config);
	 //Set UART pins (using UART0 default pins ie no changes.)
	 uart_set_pin(CONFIG_ESP_CONSOLE_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);



	Uart0EventTask = xTaskCreate(Uart0_Event,(const char*) "uart0_event_task", UART0_EVENT_TASK_STACK_DEPTH, NULL, UART0_EVENT_TASK_PRIORITY, &UartEventHandle);
#endif

	//Create msg_Rx_Queue
	if(msg_Rx_Queue == NULL)
	{
//		msg_Rx_Queue = xQueueCreateInPSRAM(UART_OBJ_QUE_COUNT, sizeof(AMessage_st)); //xQueueCreate(UART_OBJ_QUE_COUNT, sizeof(AMessage_st));    //create a msg_Rx_Queue queue.
		msg_Rx_Queue = xQueueCreateStatic(UART_OBJ_QUE_COUNT, sizeof(AMessage_st), pucQueueStorage, &pxQueueBuffer);
		if(msg_Rx_Queue == NULL)
		{
	#ifdef ENABLE_PRINT_MSG
			printf("UART RX Queue is not created. \n");
	#endif
		}
	}
				uart_Handle = xTaskCreateStatic(
								monitor,                 // Task function
								"UART Monitor",            // Task name
								UART_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								UART_TASK_PRIORITY,                       // Task priority
								xTaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xUARTTaskBuffer             // Pointer to task control block
			   );

		if (uart_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			    printf("Failed to create task\n");
#endif
			    // Handle error
			}

	// Creating Monitor Task
	if (s_console_cmd_mutex == NULL)
	{
		s_console_cmd_mutex = xSemaphoreCreateMutex();
	}
	s_config.max_cmdline_length = MAX_JSON_PAYLOAD_BYTES;
	s_config.max_cmdline_args =  32;
	s_config.hint_color = 39;
	s_config.hint_bold = 0;
	FirstEntry_bool = true;
}//	init


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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the UART actor.");
	cJSON_AddStringToObject(responseObject, "SET(string BAUD_RATE)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help



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

		if (pdTRUE == xQueueReceive(msg_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY)){          // Uart Rx queue Monitor
//			printf("UART msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("UART DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,(MAX_JSON_PAYLOAD_BYTES+COMMAND_LEN-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
				if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) { // INIT Properties
				Debug_Uart_Init(0, 0);
				if(FirstEntry_bool)
				{
					Add_Response_msg("UART is initialized.", s_Message_Rx);
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
					sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
					Add_Response_msg(str,s_Message_Rx);
				    }
				else{
					head_JSON = name_JSON->child;
					cJSON *root_JSON  = cJSON_CreateObject();
					cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/UART.json");
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
								 Add_Response_msg(str, s_Message_Rx);
							}
							else{
							cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
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
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE")) {
				Analyse_Response(s_Message_Rx);
				}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP")) {
				help(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else
			{
				//UART error message: invalid method
				  Add_Response_msg("Invalid method", s_Message_Rx);
			}
		}
	}
}//	monitor

void UART_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	Debug_Uart_Init(0,0);
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
			printf("<UART.ERROR(UART RX Queue is full)\n");
		}
		else
		{
			printf("<UART.ERROR(UART RX Queue send unsuccessful)\n");
		}
	}
}//	UART_ConsolWriteToActor

//---------------------------------------------------------------------------------------------------//

int UartArbiter(int argc, char **argv, const char* source_actor) {
	AMessage_st s_Message_Tx = {0};
	if(FirstEntry_bool == false)
		Debug_Uart_Init(0, 0);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, source_actor);
	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, argv[0]+1	);
	s_Message_Tx.Dest_ID_a8	= Dest_actor_que_sel((char*) s_Message_Tx.dest_Actor_a8);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, argv[1]	);
	if(argv[2] == NULL)
	{
		s_Message_Tx.payload_size = 0;
	}
	else
	{
		s_Message_Tx.payload_size=strlen((char*) argv[2]);
	}
	char *newpointer 	    = (char*) heap_caps_calloc((s_Message_Tx.payload_size + 1), sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return 0;
	}
	s_Message_Tx.payload_p8	= (uint8_t*)newpointer;
	if(argv[2] != NULL)
	{
		strcpy((char*) ((s_Message_Tx.payload_p8)), (char*) argv[2]);
	}
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	return 0;
}//	UartArbiter

//---------------------------------------------------------------------------------------------------//

static void Uart0_Event(void *pvParameters)
{
    uart_event_t event;
    uint16_t rxBytes;
    static char cmd_frame[UART_READ_WRITE_SIZE];
    static uint16_t frame_len = 0;
    static uint8_t frame_started = 0;
    static uint8_t in_quotes = 0;
    static uint8_t escape_next = 0;
    static int16_t paren_depth = 0;
    for(;;){
        //Waiting for UART event.
        if(xQueueReceive(Uart0_event_queue, (void * )&event, (TickType_t)portMAX_DELAY))
        {
        	memset(&RxBuff, 0, sizeof(RxBuff));
            switch(event.type) {
                //Event of UART receving data
                /*We'd better handler data event fast, there would be much more data events than
                other types of events. If we take too much time on data event, the queue might
                be full.*/

                case UART_DATA:
                	{
                		uint16_t read_size = event.size;
                		if (read_size >= UART_READ_WRITE_SIZE)
                		{
                			read_size = (UART_READ_WRITE_SIZE - 1);
                		}
                		rxBytes = uart_read_bytes(CONFIG_ESP_CONSOLE_UART_NUM, (uint8_t *)RxBuff, read_size, 100);
                		if (rxBytes == 0)
                		{
                			break;
                		}
                		if (event.size >= UART_READ_WRITE_SIZE)
                		{
#ifdef ENABLE_PRINT_MSG
                			printf("\n Command size is large. It should be less than 512 bytes.");
#endif
                		}

                		for (uint16_t i = 0; i < rxBytes; i++)
                		{
                			char ch = RxBuff[i];
                			if (!frame_started)
                			{
                				if (ch != '<')
                				{
                					continue;
                				}
                				frame_started = 1;
                				frame_len = 0;
                				in_quotes = 0;
                				escape_next = 0;
                				paren_depth = 0;
                			}
                			/* Ignore physical line breaks inserted by host/logger wrapping. */
                			if ((ch == '\r') || (ch == '\n'))
                			{
                				continue;
                			}

                			if (frame_len >= (UART_READ_WRITE_SIZE - 1))
                			{
#ifdef ENABLE_PRINT_MSG
                				printf("\n Command frame overflow. Dropping frame.");
#endif
                				frame_started = 0;
                				frame_len = 0;
                				in_quotes = 0;
                				escape_next = 0;
                				paren_depth = 0;
                				continue;
                			}

                			cmd_frame[frame_len++] = ch;

                			if (escape_next)
                			{
                				escape_next = 0;
                			}
                			else if (ch == '\\' && in_quotes)
                			{
                				escape_next = 1;
                			}
                			else if (ch == '"')
                			{
                				in_quotes = !in_quotes;
                			}
                			else if (!in_quotes)
                			{
                				if (ch == '(')
                				{
                					paren_depth++;
                				}
                				else if (ch == ')')
                				{
                					if (paren_depth > 0)
                					{
                						paren_depth--;
                					}
                					if (paren_depth == 0)
                					{
                						cmd_frame[frame_len] = '\0';
                						process_uart_command_frame(cmd_frame);
                						frame_started = 0;
                						frame_len = 0;
                						in_quotes = 0;
                						escape_next = 0;
                						paren_depth = 0;
                					}
                				}
                			}
                		}
                	}
                    break;
                //Event of HW FIFO overflow detected
                case UART_FIFO_OVF:
                    //ESP_LOGI(TAG, "hw fifo overflow");
                    // If fifo overflow happened, you should consider adding flow control for your application.
                    // The ISR has already reset the rx FIFO,
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
                    break;
                //Event of UART ring buffer full
                case UART_BUFFER_FULL:
                    //ESP_LOGI(TAG, "ring buffer full");
                    // If buffer full happened, you should consider encreasing your buffer size
                    // As an example, we directly flush the rx buffer here in order to read more data.
                    uart_flush_input(CONFIG_ESP_CONSOLE_UART_NUM);
                    break;
                //Event of UART RX break detected
                case UART_BREAK:
#ifdef ENABLE_PRINT_MSG
                	printf("Event break : %u\r\n", event.type);

//                    ESP_LOGI(THIS_ACTOR, "uart rx break");
                	printf("\n uart rx break");
#endif
                    break;
                //Event of UART parity check error
                case UART_PARITY_ERR:
#ifdef ENABLE_PRINT_MSG
//                    ESP_LOGI(THIS_ACTOR, "uart parity error");
                	printf("\n uart parity error");
#endif
                    break;
                    //Event of UART frame error
                case UART_FRAME_ERR:
#ifdef ENABLE_PRINT_MSG
//                    ESP_LOGI(THIS_ACTOR, "uart frame error");
                	printf("\n uart frame error");
#endif
                    break;
                    //UART_PATTERN_DET
                case UART_PATTERN_DET:
                    break;
                    //Others
                default:
#ifdef ENABLE_PRINT_MSG
//                    ESP_LOGI(THIS_ACTOR, "uart event type: %d", event.type);
                	printf("\n uart event type: %d", event.type);
#endif
                    break;
            }

        }
    }
    vTaskDelete(NULL);
}

static void process_uart_command_frame(char *cmd_frame)
{
	char *argv_local[3] = {0};
	size_t argc = parse_uart_command_frame(cmd_frame, argv_local);
	if (argc < 2)
	{
		return;
	}
	int ret = UartArbiter((int)argc, argv_local, THIS_ACTOR);
	if (ret != ESP_OK)
	{
#ifdef ENABLE_PRINT_MSG
		printf("Command returned non-zero error code: 0x%x (%s)\n", ret, esp_err_to_name(ret));
#endif
	}
}

static size_t parse_uart_command_frame(char *cmd_frame, char **argv)
{
	char *cmd_dot = strchr(cmd_frame, '.');
	char *cmd_open = strchr(cmd_frame, '(');
	char *cmd_close = strrchr(cmd_frame, ')');
	if ((cmd_frame[0] != '<') || (cmd_dot == NULL) || (cmd_open == NULL) || (cmd_close == NULL) || (cmd_close < cmd_open) || (cmd_dot <= cmd_frame) || (cmd_open <= cmd_dot))
	{
		return 0;
	}

	*cmd_dot = '\0';
	*cmd_open = '\0';
	*cmd_close = '\0';

	argv[0] = cmd_frame;       // "<SQL"
	argv[1] = cmd_dot + 1;     // "DB_EXECUTE"
	argv[2] = cmd_open + 1;    // JSON payload

	for (char *p = argv[0]; *p != '\0'; ++p)
	{
		if ((*p >= 'a') && (*p <= 'z'))
		{
			*p = (char)(*p - 32);
		}
	}
	for (char *p = argv[1]; *p != '\0'; ++p)
	{
		if ((*p >= 'a') && (*p <= 'z'))
		{
			*p = (char)(*p - 32);
		}
	}

	char *payload = argv[2];
	while ((*payload == ' ') || (*payload == '\t'))
	{
		payload++;
	}
	argv[2] = payload;
	if (*argv[2] != '\0')
	{
		char *end = argv[2] + strlen(argv[2]) - 1;
		while ((end >= argv[2]) && ((*end == ' ') || (*end == '\t')))
		{
			*end = '\0';
			end--;
		}
	}

	return 3;
}

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n UART s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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
	return;
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

static char Dest_actor_que_sel(char *str_prop) {
	int i =0;
	int num_elements = sizeof(A_Queue) / sizeof(A_Queue[0]);

	do{
		if (!strcmp(str_prop, A_Queue[i].name)) {

			switch (A_Queue[i].type) {

				case UART			:	return UART;			break;
				case WIFI			:	return WIFI;			break;
				case BLE			:	return BLE;				break;
				case HTTP			:	return HTTP;			break;
//				case COPROC_UART	:	return COPROC_UART;		break;
				case IDLE			:	return IDLE;			break;
				case NTP			:	return NTP;				break;
				case LED			:	return LED;				break;
				case PUSHBUTTON		:	return PUSHBUTTON;		break;
				case WEB_SERVER		:	return WEB_SERVER;		break;
				case FILE_SYSTEM	:	return FILE_SYSTEM;		break;
				case SPIFFS			:	return SPIFFS;	        break;
				case ETH			:	return ETH;				break;
				case SMTP_CLIENT	:	return SMTP_CLIENT;		break;
				case IHUB			:	return IHUB;			break;
				case TCP_SERVER		: 	return TCP_SERVER;		break;
				case UDP			:	return UDP;				break;
				case SQL			:	return SQL;				break;
				case CONSOLE		:	return CONSOLE;			break;
				case LIGHTING	    :	return LIGHTING;		    break;
				case SYSTEM	        :	return SYSTEM;		    break;
				case EVENT_ACTOR	:	return EVENT_ACTOR;		break;
				case SYS_FILES	:		return SYS_FILES;		break;
			    default			    :	break;
			}
		}
		i++;
			}while(i<num_elements);
	return 0;
}//	console_que_sel


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

static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function)
{
	AMessage_st s_Message_Tx_new;
	uint8_t *newpointer  = (uint8_t*) heap_caps_calloc((strlen(response)+1),sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT); //strlen((char*)out_val + 1)
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
	console_ActorWriteToConsole_xface(&s_Message_Tx_new);
}



esp_err_t esp_console_run_Custom(const char *cmdline, int *cmd_ret, const char *source_actor)
{
	if ((s_console_cmd_mutex != NULL) && (xSemaphoreTake(s_console_cmd_mutex, portMAX_DELAY) != pdTRUE))
	{
		return ESP_ERR_TIMEOUT;
	}

    // Copy incoming buffer into working buffer
    strlcpy(s_tmp_line_buf, cmdline, s_config.max_cmdline_length);

    // Allocate argv once, reuse for each command
    char **argv = (char **)calloc(s_config.max_cmdline_args, sizeof(char *));
    if (argv == NULL) {
    	if (s_console_cmd_mutex != NULL) {
    		xSemaphoreGive(s_console_cmd_mutex);
    	}
        return ESP_ERR_NO_MEM;
    }

    char *p = s_tmp_line_buf;
    int last_ret = 0;

    while (*p) {
        // Skip leading CR/LF
        while (*p == '\r' || *p == '\n') p++;
        if (!*p) break;

        // Isolate one line
        char *line_start = p;
        while (*p && *p != '\r' && *p != '\n') p++;
        char saved = *p;
        *p = '\0';

        // Parse this single line
        size_t argc = 0;
        if (line_start[0] == '<')
        {
        	argc = parse_uart_command_frame(line_start, argv);
        }
        else
        {
        	argc = esp_console_split_argv(line_start, argv, s_config.max_cmdline_args);
        }
        if (argc > 1) {
            last_ret = UartArbiter(argc, argv, source_actor);
            if (cmd_ret) *cmd_ret = last_ret;   // keep last return value
        }

        // Restore and move forward
        *p = saved;
        if (*p == '\r' && p[1] == '\n') p += 2;
        else if (*p == '\r' || *p == '\n') p++;
    }

    free(argv);
    if (s_console_cmd_mutex != NULL) {
    	xSemaphoreGive(s_console_cmd_mutex);
    }
    return ESP_OK;
}

/* ── Public property accessors for spec-compliant JSON get/set ── */
bool uart_actor_get(const char *name, char *val_out, size_t max_len)
{
    if (!name || !val_out || max_len == 0) return false;
    val_out[0] = '\0';
    get((char *)name, val_out);
    return val_out[0] != '\0';
}

bool uart_actor_set(const char *name, const char *val_in)
{
    if (!name || !val_in) return false;
    return set((char *)name, (char *)val_in, NULL) == 1;
}
