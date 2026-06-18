#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "NTP_Actor.h"
#include "esp_sntp.h"
#include "rtc_wdt.h"
#include "esp_timer.h"
#include "driver/gptimer.h"
#include "esp_private/esp_clk.h"
#include "pcf8563.h"
#include <stdlib.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
//-----------------------------------------------------------------------------------------------//

#define NTP_sync_pin 21
#define CONFIG_SNTP_TIME_SYNC_METHOD_SMOOTH 0
#define RTC_THRESHOLD 1735689600

static const char * THIS_ACTOR 	 	= "NTP";
static const char 			THIS_ACTOR_ID 	= 	NTP;
static int NET_status = -1;
BaseType_t NTPMonitor, NTP_Connect_Method;
TaskHandle_t NTP_Handle = NULL, ConnectHandle = NULL, NTP_Sync_Check_Handle = NULL;	//, SlideSwitch_Handle;
static StaticTask_t xNTPTaskBuffer, xNTP_CONN_TaskBuffer,xNTP_Sync_Check_TaskBuffer;	//, xNTP_SlideSwitch_TaskBuffer;  //// Declare a static task control block
static StaticQueue_t Monitor_pxQueueBuffer;
static AMessage_st s_Message_Tx;//s_Message_Rx			 // ACtors Message structure
unsigned char pin_val=0;
static bool     		FirstEntry_bool = 	false;
char NTP_operation = 0;
volatile int64_t ntp_time_us = 0; // Store time in microseconds
volatile uint64_t ntp_timer_offset = 0; // Timer counter at last NTP sync
PSRAM_ATTR_BSS static struct ntp_registers {
	char URL[32];
	char time_zone[40];
	uint32_t sync_interval;
	uint8_t RTC_Status;
	uint8_t NTP_Mode;
} ntp_Para;

PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &ntp_Para.URL,           "URL",           STRING,   "RW", "URL for NTP server" },
    { &ntp_Para.time_zone,     "TIME_ZONE",     STRING,   "RW", "Time zone for synchronization" },
    { &ntp_Para.sync_interval, "SYNC_INTERVAL", INT,      "R", "Sync interval in sec for NTP synchronization" },
    { &ntp_Para.RTC_Status,    "RTC_STATUS",    U_INT8,   "R",  "RTC Status" },
	{ &ntp_Para.NTP_Mode,      "NTP_MODE",      U_INT8,   "R",  "NTP Mode" }
};

PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_NTP[1024];
PSRAM_ATTR_BSS static uint8_t Monitor_pucQueueStorage [NTP_RX_QUEUE_LENGTH * sizeof(AMessage_st)];
PSRAM_ATTR_BSS static StackType_t NTP_TaskStack [NTP_CONNECT_TASK_STACK_DEPTH], xTaskStack [NTP_TASK_STACK_DEPTH];	//, SlideSwitch_TaskStack[NTP_TASK_STACK_DEPTH_SlideSwitch];
PSRAM_ATTR_BSS static StackType_t NTP_Sync_Check_TaskStack [NTP_SYNC_CHECK_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char Callback_buffer[2048];
QueueHandle_t NTP_Rx_Queue; // NTP_Tx_Queue;

static void init		(void *a, void *b); 							// Init actor
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);					//	Change a parameter
static void get(char *prop, char *val);							//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
static void periodic_timer_callback(void* arg);
static void periodic_synch_pulse(void* arg);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static void NTP_Connect(void *pvParameters __attribute__((unused)));
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void set_initial_time(AMessage_st* s_Message_Rx) ;
static void set_RTC_time(AMessage_st* s_Message_Rx);
static void NTP_Sync_Check(void *pvParameters __attribute__((unused)));
static void reset_time_buffers(void);

static uint8_t NTP_Synched_flag = 0;
#define ONE_SECOND_TIMER_FLAG 1
#define TEN_SECOND_TIMER_FLAG 10
#define GENERATE_TEN_SECOND_PULSE 20
static uint8_t set_second_timer_flag = 0;

const esp_timer_create_args_t periodic_synch_pulse_args = {
		.callback = &periodic_synch_pulse,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodicsynchpulse"
};
static esp_timer_handle_t periodic_synch_timer;

const esp_timer_create_args_t periodic_timer_args = {
		.callback = &periodic_timer_callback,
		/* name is optional, but may help identify the timer when debugging */
		.name = "periodic"
};
static esp_timer_handle_t periodic_timer;

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

static void init(void *a, void *b) {
	if (FirstEntry_bool) {
			return;
	}
	FirstEntry_bool = true;
	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	NTP_Rx_Queue = xQueueCreateStatic(NTP_RX_QUEUE_LENGTH, sizeof(AMessage_st), Monitor_pucQueueStorage, &Monitor_pxQueueBuffer);
	if(NTP_Rx_Queue == NULL)
	{
#ifdef ENABLE_PRINT_MSG
		printf("<NTP.ERROR(SD RX Queue is not created.)<CR>");
#endif
	}

	NTP_Handle = xTaskCreateStaticPinnedToCore(
					monitor,                 // Task function
					"NTP Monitor",            // Task name
					NTP_TASK_STACK_DEPTH,        // Stack size in words
					NULL,                    // Task parameters (not used here)
					NTP_TASK_PRIORITY,                       // Task priority
					xTaskStack,              // Pointer to task stack (allocated in PSRAM)
					&xNTPTaskBuffer,             // Pointer to task control block
					1
   );

	if (NTP_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			printf("Failed to create task\n");
#endif
		}

	ntp_Para.NTP_Mode = SNTP_SYNC_MODE_IMMED;
	ntp_Para.sync_interval = 600; //60000; // FAST_POLL_INTERVAL_MS; //60000;  // NTP sync interval = 60 min
	struct tm time = {0};
	uint8_t timems = 0;
	time_t tOfDay = 0;
	i2c_dev_t *dev = pcf8563_get_device_handle();
	if (pcf8563_get_time_ms(dev, &time, &timems) == ESP_OK) {
		time.tm_year-=1900;
		tOfDay = mktime(&time);
	}
	if(tOfDay > RTC_THRESHOLD)
	{
		ntp_Para.RTC_Status = RTC_SET_POWER_ON;
	}
	else
	{
		ntp_Para.RTC_Status = RTC_NEVER_SET;
	}
	ESP_ERROR_CHECK(esp_timer_create(&periodic_synch_pulse_args, &periodic_synch_timer));  // create periodic timer
	ESP_ERROR_CHECK(esp_timer_create(&periodic_timer_args, &periodic_timer));  // create periodic timer
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
		}
	}
}//	get

static void get_actor_properties(AMessage_st* s_Message_Rx){

	char val_a8[40] = {0};
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
			if(!(strcmp(prop[i].str_name, "RTC_STATUS")))
			{
				switch (*(char*)prop[i].name)
				{
				case RTC_NEVER_SET:
					strcpy(val_a8, "RTC_NEVER_SET");
					break;

				case RTC_DEVICE_ANNOUNCE:
					strcpy(val_a8, "RTC_DEVICE_ANNOUNCE");
					break;

				case RTC_IMMEDIATE_MODE:
					strcpy(val_a8, "RTC_IMMEDIATE_MODE");
					break;

				case RTC_SMOOTH_MODE:
					strcpy(val_a8, "RTC_SMOOTH_MODE");
					break;

				case RTC_SET_POWER_ON:
					strcpy(val_a8, "RTC_SET_POWER_ON");
					break;

				default:
					break;
				}
			}
			if(!(strcmp(prop[i].str_name, "NTP_MODE")))
			{
				switch (*(char*)prop[i].name)
				{
					case SNTP_SYNC_MODE_IMMED:
						strcpy(val_a8, "SNTP_SYNC_MODE_IMMED");
						break;

					case SNTP_SYNC_MODE_SMOOTH:
						strcpy(val_a8, "SNTP_SYNC_MODE_SMOOTH");
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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the NTP actor.");
	cJSON_AddStringToObject(responseObject, "SET(string URL)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table. Pass the parameter in prop and its value is return in val_a8");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "CONNECT()", "Connect to NTP server.");
	cJSON_AddStringToObject(responseObject, "STARTSYNCHTEST()", "Start the clock sync test.");
	cJSON_AddStringToObject(responseObject, "SET_INITIAL_RTC(U64 EPOCH_MILLS)", "Sets the Epoch milliseconds.");
	cJSON_AddStringToObject(responseObject, "STOPSYNCHTEST()", "Stop the clock sync test.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "SET_IMMED_MODE()", "Set NTP sync mode to immediate");
	cJSON_AddStringToObject(responseObject, "SET_SMOOTH_MODE()", "Set NTP sync mode to smooth.");
	cJSON_AddStringToObject(responseObject, "SET_SYNC_INTERVAL(INT SYNC_INTERVAL)", "Set NTP sync interval (msec).");
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
		if (pdTRUE == xQueueReceive(NTP_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))
		{
//			printf("NTP msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("NTP DT = %s\n",s_Message_Rx->payload_p8);
//			}
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT")) // Get Actor Properties
			{
				if(FirstEntry_bool==0)
				{
					init(0, 0);
				}
				if(FirstEntry_bool==1)
				{
					Add_Response_msg("NTP actor is initialized.",s_Message_Rx);
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET"))
			{
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
							if(u8Result==2){
								sprintf(str,"'%s' is a read only property", head_JSON->string);
								 Add_Response_msg(str, s_Message_Rx);
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
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "STARTSYNCHTEST"))
			{
				NTP_Synched_flag = 1;
				gpio_set_direction(NTP_sync_pin, GPIO_MODE_OUTPUT);
				vTaskDelay(100 / portTICK_PERIOD_MS);
				gpio_set_level(NTP_sync_pin,0);
				esp_timer_stop(periodic_synch_timer);
				esp_timer_stop(periodic_timer);
				set_second_timer_flag = 0;
				ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000)); // start the timer with the period of 10sec
				Add_Response_msg("Synch Test Start. Generate pulse at every 10 seconds interval",s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "STOPSYNCHTEST"))
			{
				gpio_set_direction(NTP_sync_pin, GPIO_MODE_OUTPUT);
				vTaskDelay(100 / portTICK_PERIOD_MS);
				gpio_set_level(NTP_sync_pin,0);
				esp_timer_stop(periodic_synch_timer);
				esp_timer_stop(periodic_timer);
				set_second_timer_flag = 0;
				Add_Response_msg("Synch Test Stopped",s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SET_INITIAL_RTC"))
			{
				reset_time_buffers();
				set_initial_time(s_Message_Rx);
				set_RTC_time(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE")) // Get Actor Properties
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "CONNECT")) // Get Actor Properties
			{
				if((ConnectHandle == NULL) && (NTP_operation ==0))
				{
					ConnectHandle = xTaskCreateStaticPinnedToCore(
									NTP_Connect,                 // Task function
									"NTP Connect",            // Task name
									NTP_CONNECT_TASK_STACK_DEPTH,        // Stack size in words
									s_Message_Rx,                    // Task parameters (not used here)
									NTP_CONNECT_TASK_PRIORITY,                       // Task priority
									NTP_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xNTP_CONN_TaskBuffer,             // Pointer to task control block
									1
					);
					if (ConnectHandle == NULL)
					{
						Add_Response_msg("Failed to create task for NTP.CONNECT method.", s_Message_Rx);
					}
				}
				else
				{
					Add_Response_msg("NTP is already connected.", s_Message_Rx);
				}
				if(NTP_Sync_Check_Handle == NULL)
				{
					NTP_Sync_Check_Handle = xTaskCreateStaticPinnedToCore(
							        NTP_Sync_Check,                 // Task function
									"NTP_Sync_Check",            // Task name
									NTP_SYNC_CHECK_TASK_STACK_DEPTH,        // Stack size in words
									NULL,                    // Task parameters (not used here)
									NTP_SYNC_CHECK_TASK_PRIORITY,                       // Task priority
									NTP_Sync_Check_TaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xNTP_Sync_Check_TaskBuffer,             // Pointer to task control block
									1
					);
					if (NTP_Sync_Check_Handle == NULL)
					{
						Add_Response_msg("Failed to create a task for NTP Sync check.", s_Message_Rx);
					}
				}
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else
			{
				 Add_Response_msg("Invalid method", s_Message_Rx);
			}
		}
	}
}


void NTP_ConsolWriteToActor_xface(void *msg)
{
	AMessage_st *msg_new;
	msg_new = (AMessage_st*)msg;

	if (!FirstEntry_bool)
	{
		init(0,0);
	}
	uint8_t state = xQueueSend(NTP_Rx_Queue, msg_new, QUE_DELAY);//100, 5
	if (state != pdTRUE)
	{
		if(msg_new->payload_p8 != NULL){
		free(msg_new->payload_p8);
		msg_new->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<NTP.ERROR(NTP RX Queue is full)\n");
		}
		else
		{
			printf("<NTP.ERROR(NTP RX Queue send unsuccessful)\n");
		}
	}

}//	NTP_ConsolWriteToActor

/*
 * The callback is used to generate 50ms pulse.
 * The GPIO pin is set from another function and then 50ms timer callback is set
 * to reset the gpio pin after 50ms
 */

static void periodic_synch_pulse(void* arg)
{
	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	gpio_set_level(NTP_sync_pin,0);
	esp_timer_stop(periodic_synch_timer);
}
/*
 * This callback is used to trigger 1ms periodic timer at start.
 * at every 1ms this callback checks for perfect second.
 * once perfect second is identified same timer is stopped and restarted to identify perfect 10 seconds
 * on perfect 10 seconds pulse is generated and timer is kept running with 10seconds callback
 */
static void periodic_timer_callback(void* arg)
{
	uint64_t milliseconds;
	struct timeval currentTime;
	_gettimeofday_r(NULL, &currentTime, NULL);
	milliseconds = (uint64_t) (currentTime.tv_sec * 1000L) + (uint64_t) (currentTime.tv_usec / 1000L);

	if(set_second_timer_flag==0)
	{
		//at start 1ms timer is set to find perfect second
		if(((milliseconds%1000)==0) )
		{
			esp_timer_stop(periodic_timer);
			//restart timer with one second callback
			ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000000));
			//printf("\n one second timer set %lld\n", milliseconds);
			set_second_timer_flag = TEN_SECOND_TIMER_FLAG;

		}
	}
	if(NTP_Synched_flag)
	{

		if((set_second_timer_flag==TEN_SECOND_TIMER_FLAG))
		{
			//at perfect second find perfect 10 seconds
			if((milliseconds%10000)==0)
			{
				esp_timer_stop(periodic_timer);
				//restart timer with ten second callback
				ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 10000000)); // start the timer with the period of 10sec
				//printf("\n ten second timer set %lld\n", milliseconds);
				//generate 10 seconds pulse
				set_second_timer_flag = GENERATE_TEN_SECOND_PULSE;

			}
			else
			{
				esp_timer_stop(periodic_timer);
				set_second_timer_flag = 0;
				ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_timer, 1000)); // start the timer with the period of 1msec
				//printf("\n Restarted one millisecond timer %lld\n", milliseconds);

			}

		}
		if(set_second_timer_flag == GENERATE_TEN_SECOND_PULSE )
		{
			ESP_ERROR_CHECK(esp_timer_start_periodic(periodic_synch_timer, 50000)); // start the timer with the period of 50msec
			gpio_set_level(NTP_sync_pin,1);
		}
	}
}


#ifdef CONFIG_SNTP_TIME_SYNC_METHOD_CUSTOM
void sntp_sync_time(struct timeval *tv)
{
   settimeofday(tv, NULL);
#ifdef ENABLE_PRINT_MSG
   printf("Time is synchronized from custom code");
#endif
   sntp_set_sync_status(SNTP_SYNC_STATUS_COMPLETED);
}
#endif

static void Analyse_Response(AMessage_st* s_Message_Rx)
{
	cJSON *in_JSON 		= NULL;
	cJSON *name_JSON 	= NULL;
	char  str[200] = {0}; // keyValue[50] = {0};
	uint8_t	temp[10];

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n NTP s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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
	 if(strcmp((char*)s_Message_Rx->src_Actor_a8,"CONSOLE")==0)
	 {
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
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
	return;
}


static void set_RTC_time(AMessage_st* s_Message_Rx) {
    cJSON *in_JSON 		= NULL;
    struct timeval currentTime;
    char str[200] = {0};
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
	}

	cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "EPOCH_MILLS");
	if((commandKey!=NULL) && cJSON_IsNumber(commandKey) )
	{
		uint64_t epochUTCmilliseconds=commandKey->valuedouble;
		currentTime.tv_sec =  (epochUTCmilliseconds)/1000L;
		currentTime.tv_usec = ((epochUTCmilliseconds%1000)*1000);
	}
	cJSON_Delete(in_JSON);

		time_t now;
		struct tm timeinfo;
		time(&now);
		localtime_r(&now, &timeinfo);
		struct tm time = {
			.tm_year = timeinfo.tm_year + 1900,
			.tm_mon  = timeinfo.tm_mon,  // 0-based
			.tm_mday = timeinfo.tm_mday,
			.tm_hour = timeinfo.tm_hour,
			.tm_min  = timeinfo.tm_min,
			.tm_sec  = timeinfo.tm_sec
		};
		i2c_dev_t *dev = pcf8563_get_device_handle();
		int milliseconds = (currentTime.tv_usec / 1000L);

		if ((pcf8563_set_time_ms(dev, &time, (milliseconds/10))) != ESP_OK) {
			Add_Response_msg("NTP RTC synch failed",s_Message_Rx);
		}
		else {
			Add_Response_msg("External RTC time is set.",s_Message_Rx);
			ntp_Para.RTC_Status = RTC_DEVICE_ANNOUNCE;
		}
}


// Function to set an initial time
static void set_initial_time(AMessage_st* s_Message_Rx) {
    cJSON *in_JSON 		= NULL;
    struct timeval currentTime;
    char str[200] = {0};
	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
	}

	cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "EPOCH_MILLS");
	if((commandKey!=NULL) && cJSON_IsNumber(commandKey) )
	{
		uint64_t epochUTCmilliseconds=commandKey->valuedouble;
		currentTime.tv_sec =  (epochUTCmilliseconds)/1000L;
		currentTime.tv_usec = ((epochUTCmilliseconds%1000)*1000);
	}
	cJSON_Delete(in_JSON);
    // Get the current time
    struct timeval now;
    gettimeofday(&now, NULL);
	settimeofday(&currentTime, NULL);
	Add_Response_msg("Internal RTC time is set.",s_Message_Rx);
}


static void NTP_Connect(void *pvParameters __attribute__((unused)))
{
	AMessage_st* s_Message_Rx = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx_data;
	char data_buffer[200] = {0}, payLoadData_NTP[200] = {0};
	memset(data_buffer,0,sizeof(data_buffer));
	memcpy(&s_Message_Rx_data, s_Message_Rx, sizeof(AMessage_st));
	s_Message_Rx_data.payload_p8 = (uint8_t*)data_buffer;
    strcpy((char*)s_Message_Rx_data.src_Actor_a8, "CONSOLE");
    strcpy((char*)s_Message_Rx_data.dest_Actor_a8, "NTP");
    strcpy((char*)s_Message_Rx_data.cmdFun_a8, "CONNECT");

	char count = 0, display_msg = 0;
	cJSON *my_JSON1  	= cJSON_CreateArray();
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("NET_STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_NTP,0,sizeof(payLoadData_NTP));
	cJSON_PrintPreallocated(jsonObject, payLoadData_NTP, sizeof(payLoadData_NTP), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_NTP, strlen(payLoadData_NTP), "GET");

	while(1)
	{
		if(NET_status == E_NET_CONNECTED)
		{
			strcpy(ntp_Para.URL,(const char*)"pool.ntp.org");
			strcpy(ntp_Para.time_zone,(const char*)"UTC");
			Add_Response_msg("Connected to NTP",&s_Message_Rx_data);
			NTP_operation = 1;
		}
		else
		{
			count++;
			if((count > 100) && (display_msg == 0))
			{
				display_msg = 1;
				Add_Response_msg("Kindly connect to internet at first.",&s_Message_Rx_data);
				ConnectHandle = NULL;
				vTaskDelete(ConnectHandle);
			}
		}
		if(NTP_operation == 1)
		{
			ConnectHandle = NULL;
			vTaskDelete(ConnectHandle);  // Delete the task
		}
		vTaskDelay(10 / portTICK_PERIOD_MS);
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


static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	memset(payLoadData,0,sizeof(payLoadData));//\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);		
	console_send_responce_to_console_xface(s_Message_Rx);
	cJSON_Delete(responseObject);
}

// Configuration parameters for NTP sampling, filtering, and timing
#define NTP_SERVERS {"pool.ntp.org", "north-america.pool.ntp.org", "asia.pool.ntp.org", "europe.pool.ntp.org", "time.google.com", "time.windows.com"}
#define NTP_QUERY_COUNT            8
#define INITIAL_POLL_INTERVAL_S    20
#define STEADY_POLL_INTERVAL_S     60
#define INITIAL_SYNC_CYCLES        4
#define RTT_OUTLIER_THRESHOLD      1.2
#define OFFSET_OUTLIER_THRESHOLD   300000
#define MAX_RTT_MS                 5
#define OFFSET_HISTORY_SIZE        8
#define ABSOLUTE_OFFSET_THRESHOLD_US  (500LL * 1000LL)
#define WARMUP_CYCLES               3
#define WARMUP_ABSOLUTE_CAP_US   5000000L
#define WARMUP_RTT_THRESHOLD_US   (200 * 1000)

typedef struct {
    int64_t offset_us;
    int64_t rtt_us;
    bool    valid;
} ntp_sample_t;
static ntp_sample_t samples[NTP_QUERY_COUNT] = {0};
// Globals for tracking state across sync cycles
static volatile bool ntp_initialized = false;
static int64_t last_offset_us = 0;
static int64_t avg_drift_us_per_s = 0;
static int sync_cycle_count = 0;
static int64_t offset_history[OFFSET_HISTORY_SIZE] = {0};
static int offset_history_idx = 0;
static int offset_history_count = 0;

// Calculate median RTT from valid samples
static float calculate_median_rtt(ntp_sample_t *samples, int count) {
    double values[NTP_QUERY_COUNT];
    int n = 0;
    for (int i = 0; i < count; i++)
        if (samples[i].valid)
            values[n++] = (double)samples[i].rtt_us;
    if (n == 0) return 0.0;
    // insertion sort
    for (int i = 1; i < n; i++) {
        double key = values[i];
        int j = i - 1;
        while (j >= 0 && values[j] > key) {
            values[j + 1] = values[j];
            j--;
        }
        values[j + 1] = key;
    }
    return (n % 2 == 0)
         ? (values[n/2 - 1] + values[n/2]) * 0.5
         : values[n/2];
}

// Compute mean of stored offsets
static int64_t calculate_mean_offset(void) {
    if (offset_history_count == 0) return 0;
    int64_t sum = 0;
    for (int i = 0; i < offset_history_count; i++)
        sum += offset_history[i];
    return sum / offset_history_count;
}

// Add a new offset to the circular history buffer
static void update_offset_history(int64_t offset_us) {
    offset_history[offset_history_idx] = offset_us;
    offset_history_idx = (offset_history_idx + 1) % OFFSET_HISTORY_SIZE;
    if (offset_history_count < OFFSET_HISTORY_SIZE)
        offset_history_count++;
}

// Decide whether a given offset is within acceptable bounds
static bool is_offset_valid(int64_t offset_us) {
    int64_t abs_off = llabs(offset_us);

    if (sync_cycle_count < WARMUP_CYCLES) {
        if (abs_off > WARMUP_ABSOLUTE_CAP_US) return false;
        return true;
    }
    if (sync_cycle_count < INITIAL_SYNC_CYCLES) {
        if (abs_off > ABSOLUTE_OFFSET_THRESHOLD_US) return false;
        return true;
    }
    if (offset_history_count < OFFSET_HISTORY_SIZE) {
        return true;
    }
    int64_t mean = calculate_mean_offset();
    int64_t delta = llabs(offset_us - mean);
    return (delta <= OFFSET_OUTLIER_THRESHOLD);
}

// Perform one raw UDP NTP exchange; compute offset & RTT in µs
#include <errno.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define NTP_PORT            "123"
#define NTP_PACKET_SIZE     48
#define NTP_TIMESTAMP_DELTA 2208988800UL

static bool ntp_raw_sample(const char *server, int64_t *offset_us, int64_t *rtt_us) {
    struct addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_DGRAM
    };
    struct addrinfo *res = NULL;
    int err = getaddrinfo(server, NTP_PORT, &hints, &res);
    if (err != 0 || res == NULL) {
        return false;
    }

	char addrbuf[INET_ADDRSTRLEN];
	struct sockaddr_in *sin = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf));

    int sock = socket(res->ai_family, res->ai_socktype, 0);
    if (sock < 0) {
        freeaddrinfo(res);
        return false;
    }

    // Build NTP request packet (LI=0, VN=3, Mode=3)
    uint8_t pkt[NTP_PACKET_SIZE] = {0};
    pkt[0] = 0x1B;

    // --- T1: local send timestamp (epoch µs) ---
    struct timeval tv1;
    gettimeofday(&tv1, NULL);
    int64_t t1_us = (int64_t)tv1.tv_sec * 1000000LL + tv1.tv_usec;

    // Send the NTP request
    int sent = sendto(sock, pkt, NTP_PACKET_SIZE, 0, res->ai_addr, res->ai_addrlen);
    if (sent < 0) {
        close(sock);
        freeaddrinfo(res);
        return false;
    }

    // Wait up to 3 seconds for a reply
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(sock, &rfds);
    struct timeval timeout = { .tv_sec = 3, .tv_usec = 0 };
    int sel = select(sock + 1, &rfds, NULL, NULL, &timeout);
    if (sel < 0) {
        close(sock);
        freeaddrinfo(res);
        return false;
    } else if (sel == 0) {
        close(sock);
        freeaddrinfo(res);
        return false;
    }

    // Receive the NTP reply
    int rec = recvfrom(sock, pkt, NTP_PACKET_SIZE, 0, NULL, NULL);
    // --- T4: local receive timestamp (epoch µs) ---
    struct timeval tv4;
    gettimeofday(&tv4, NULL);
    int64_t t4_us = (int64_t)tv4.tv_sec * 1000000LL + tv4.tv_usec;

    if (rec < 0) {
        close(sock);
        freeaddrinfo(res);
        return false;
    }

    close(sock);
    freeaddrinfo(res);

    // --- Decode server's transmit timestamp (T3) from bytes 40–47 ---
    uint32_t tx_secs = ntohl(*(uint32_t*)&pkt[40]);
    uint32_t tx_frac = ntohl(*(uint32_t*)&pkt[44]);

    // Convert NTP seconds to UNIX epoch seconds
    time_t server_secs = (time_t)(tx_secs - NTP_TIMESTAMP_DELTA);
    // Convert fraction to microseconds
    uint32_t server_frac_us = (uint32_t)((uint64_t)tx_frac * 1000000ULL >> 32);

    // Build full server timestamp in µs since 1970
    int64_t t3_us = (int64_t)server_secs * 1000000LL + server_frac_us;

    // --- Compute round-trip time and offset ---
    *rtt_us    = t4_us - t1_us;
    *offset_us = t3_us - ((t1_us + t4_us) / 2);

    return true;
}


static int64_t offs=0, rtt=0;
// Gather multiple raw samples, apply filtering, fill samples[]
static void collect_ntp_samples(ntp_sample_t *samples, int count) {
    const char *servers[] = NTP_SERVERS;
    int nservers = sizeof(servers)/sizeof(servers[0]);
    for (int i = 0; i < count; i++) {
        samples[i].valid = false;
        int si = i % nservers;
//        int64_t offs=0, rtt=0;
        if (!ntp_raw_sample(servers[si], &offs, &rtt)) {
            continue;
        }
        if (sync_cycle_count < WARMUP_CYCLES && rtt > WARMUP_RTT_THRESHOLD_US) {
            continue;
        }
        if (rtt < 0 || rtt > MAX_RTT_MS*1000000) {
            continue;
        }
        if (!is_offset_valid(offs)) {
            continue;
        }
        samples[i].offset_us = offs;
        samples[i].rtt_us    = rtt;
        samples[i].valid     = true;
        update_offset_history(offs);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// From valid samples, choose the one with lowest RTT within cutoff
static bool select_best_sample(ntp_sample_t *samples, int count, int64_t *best_offset_us, int64_t *best_rtt_us) {
    double median = calculate_median_rtt(samples, count);
    double cutoff = median * RTT_OUTLIER_THRESHOLD;
    int64_t min_rtt = INT64_MAX; int best_i = -1;
    for (int i = 0; i < count; i++) {
        if (!samples[i].valid || samples[i].rtt_us > cutoff) continue;
        if (samples[i].rtt_us < min_rtt) {
            min_rtt = samples[i].rtt_us;
            best_i = i;
        }
    }
    if (best_i < 0) return false;
    *best_offset_us = samples[best_i].offset_us;
    *best_rtt_us    = samples[best_i].rtt_us;
    return true;
}

// Read external RTC until two reads differ by <1 ms
static uint64_t read_rtc_epoch_ms(i2c_dev_t *dev) {
    struct tm tm; uint8_t centi;
    if (pcf8563_get_time_ms(dev, &tm, &centi) != ESP_OK) return UINT64_MAX;
    tm.tm_year -= 1900;
    time_t secs = mktime(&tm);
    return (uint64_t)secs * 1000ULL + centi * 10ULL;
}
esp_err_t get_stable_rtc_time_ms(i2c_dev_t *dev, uint64_t *out_ms) {
    if (!dev || !out_ms) return ESP_ERR_INVALID_ARG;
    uint64_t t_prev = read_rtc_epoch_ms(dev);
    if (t_prev == UINT64_MAX) return ESP_FAIL;
    for (int i = 0; i < 10; i++) {
        uint64_t t_cur = read_rtc_epoch_ms(dev);
        if (t_cur == UINT64_MAX) return ESP_FAIL;
        if (t_cur >= t_prev && (t_cur - t_prev) < 1) {
            *out_ms = t_cur;
            return ESP_OK;
        }
        t_prev = t_cur;
    }
    return ESP_FAIL;
}

static void epoch_us_to_timeval(int64_t epoch_us, struct timeval *tv) {
    tv->tv_sec  = epoch_us / 1000000;
    tv->tv_usec = epoch_us % 1000000;
}

/**
 * Returns 0=Sunday…6=Saturday from epoch,
 * without any libc date routines.
 */
int epoch_to_wday_plain(time_t epoch) {
    // count whole days since Jan 1 1970
    int64_t days = epoch / 86400;
    // Jan 1 1970 was Thursday == 4
    int wday = (int)((days + 4) % 7);
    // handle negative epochs correctly
    if (wday < 0) wday += 7;
    return wday;
}

// NTP sync task
static void NTP_Sync_Check(void *pvParameters) {
    // Entry point for the NTP synchronization FreeRTOS task.
    // pvParameters: unused in this context.

    // Variables for storing RTC and ESP internal times (in ms or µs as noted).
    int64_t RTC_Epoch = 0, ESP_RTC = 0;

    // Prepare a message structure to send sync results back to the console actor.
    AMessage_st s_Message_Rx;
    s_Message_Rx.Dest_ID_a8      = NTP;                      // Destination: NTP actor
    s_Message_Rx.Src_ID_u8       = CONSOLE;                  // Source: console actor
    s_Message_Rx.payload_p8      = (uint8_t*)Callback_buffer; // Payload buffer pointer
    s_Message_Rx.payload_size    = 0;                        // No payload bytes yet
    strcpy((char*)s_Message_Rx.cmdFun_a8,    "EVENT");       // Command name for event
    strcpy((char*)s_Message_Rx.dest_Actor_a8, "NTP");         // Destination actor name
    strcpy((char*)s_Message_Rx.src_Actor_a8,  "CONSOLE");     // Source actor name

    // Obtain handle for the PCF8563 external RTC device.
    i2c_dev_t *dev = pcf8563_get_device_handle();

    // Mark NTP subsystem as initialized.
    ntp_initialized = true;

    // Variables for seeding and reporting times.
//    uint64_t initial_rtc_ms = 0;  // Used if seeding at start
    uint64_t espMs;               // Millisecond count from gettimeofday()
    struct timeval new_tv;        // Used for setting system time

    // Main loop: repeat indefinitely with variable poll intervals.
    while (true) {
        // Determine delay before next sync: fast for initial cycles, then configured steady-state.
        int poll_interval_s = (sync_cycle_count < INITIAL_SYNC_CYCLES)
                                  ? INITIAL_POLL_INTERVAL_S
                                  : ntp_Para.sync_interval;

        // Collect a fixed number of NTP samples (each containing RTT and offset).
//        ntp_sample_t samples[NTP_QUERY_COUNT] = {0};
        memset(samples,0,sizeof(samples));
        collect_ntp_samples(samples, NTP_QUERY_COUNT);
        // Prepare a JSON object to accumulate sync results.
        cJSON *root = cJSON_CreateObject();

        // Local variables for time retrieval.
        struct timeval currentTime;   // For internal gettimeofday()
        struct timeval extRTCTime;    // For converted RTC timestamp
        uint64_t rtcMs = 0;           // RTC time in milliseconds

        // Read the “current” RTC time depending on hardware revision.
        if (Hardware_RevH_flag == true) {
            // External RTC present: get a stable millisecond-resolution timestamp.
            get_stable_rtc_time_ms(dev, &rtcMs);
            RTC_Epoch = rtcMs;
        } else {
            // No external RTC: fall back to the ESP32’s internal clock.
            gettimeofday(&currentTime, NULL);
            // Convert microsecond component into milliseconds.
            RTC_Epoch = currentTime.tv_usec / 1000;
        }
        // Convert our rtcMs (ms) into a timeval for potential settimeofday().
        // epoch_us_to_timeval expects microseconds, so multiply by 1000.
        epoch_us_to_timeval(rtcMs * 1000, &extRTCTime);
        // If desired, you could now set the OS clock:
        // settimeofday(&extRTCTime, NULL);

        // Variables to track the best NTP sample’s offset and round-trip time.
        int64_t best_offset_us = 0, best_rtt_us = 0;
        // Select the NTP sample with the lowest valid RTT and valid offset.
        bool sync_success = select_best_sample(
                                samples,
                                NTP_QUERY_COUNT,
                                &best_offset_us,
                                &best_rtt_us
                            );
        // Prepare for possible statistics (mean/std) – currently unused.
//        double mean_rtt = 0, std_rtt = 0;

        // If we found a good sample and its offset is within thresholds...
        if (sync_success && is_offset_valid(best_offset_us)) {
            // Optionally compute detailed stats:
            // calculate_stats(samples, NTP_QUERY_COUNT, &mean_rtt, &std_rtt);

            // Read the current system time to apply offset from NTP.
            gettimeofday(&currentTime, NULL);

            // ==== Apply the time adjustment to system clock ====
            // Start with the raw current time...
            new_tv = currentTime;
            // ...add the integer seconds component of the offset...
            new_tv.tv_sec  += best_offset_us / 1000000;
            // ...and the remaining microseconds.
            new_tv.tv_usec += best_offset_us % 1000000;

            // Normalize if microseconds overflow or underflow.
            if (new_tv.tv_usec >= 1000000) {
                new_tv.tv_sec  += 1;
                new_tv.tv_usec -= 1000000;
            } else if ((int) new_tv.tv_usec < 0) {
                new_tv.tv_sec  -= 1;
                new_tv.tv_usec += 1000000;
            }
            // Commit the adjusted time to the lwIP/OS system clock.
            settimeofday(&new_tv, NULL);
            // ====================================================

            // ==== Mirror the new UTC time back into the external PCF8563 ====
            struct tm tm_utc;
            gmtime_r(&new_tv.tv_sec, &tm_utc);      // Break out year/month/day/hour/etc
            tm_utc.tm_year += 1900;                 // gmtime_r gives years since 1900
            tm_utc.tm_wday = epoch_to_wday_plain(new_tv.tv_sec); // Compute weekday
            uint8_t cs = new_tv.tv_usec / 10000;    // 1 cs = 10 ms = 10 000 µs

            // Handle rare rounding case when µs → centisec reaches 100.
            if (cs >= 100) {
                cs = 0;
                new_tv.tv_sec++;
                gmtime_r(&new_tv.tv_sec, &tm_utc);
                tm_utc.tm_year += 1900;
            }

            // If external RTC present, write back the updated UTC + centiseconds.
            if (Hardware_RevH_flag == true) {
                pcf8563_set_time_ms(dev, &tm_utc, cs);
            }
            // Re‐sync our monotonic helper by seeding it with the new time.
            get_current_rtc_time_ms(1);

            // Record results in JSON.
            cJSON_AddNumberToObject(root, "BEST RTT(uS)",    best_rtt_us);
            cJSON_AddNumberToObject(root, "BEST OFFSET(uS)", best_offset_us);
            cJSON_AddBoolToObject(  root, "ADJUSTMENT APPLIED", true);

            // Update drift estimate and reset initial sync cycles if drift is too large.
            if (last_offset_us != 0) {
                int64_t drift = best_offset_us - last_offset_us;
                avg_drift_us_per_s = (avg_drift_us_per_s * 3 + drift / poll_interval_s) / 4;
                if (llabs(avg_drift_us_per_s) > 5000)  //10
                {
                    sync_cycle_count = 0;
                }
            }
            last_offset_us = best_offset_us;
        }
        else {
            // Sync failure or invalid offset: shorten the next polling interval for a quick retry.
            poll_interval_s = 20;
            // Indicate failure in JSON.
            cJSON_AddNumberToObject(root, "BEST RTT(uS)",    -1);
            cJSON_AddNumberToObject(root, "BEST OFFSET(uS)", -1);
            cJSON_AddBoolToObject(  root, "ADJUSTMENT APPLIED", false);
        }

        // After possible adjustment, read back both clocks for reporting:
        if (Hardware_RevH_flag == true) {
            get_stable_rtc_time_ms(dev, &rtcMs);
            RTC_Epoch = rtcMs;
        } else {
            RTC_Epoch = 0; // or re-read internal if desired
        }
        // Get the ESP32’s internal clock for comparison.
        gettimeofday(&currentTime, NULL);
        espMs  = (uint64_t)currentTime.tv_sec * 1000 + currentTime.tv_usec / 1000;
        ESP_RTC = espMs;

        // Add timing metrics to JSON.
        cJSON_AddNumberToObject(root, "RTC_EPOC(mS)", RTC_Epoch);
        cJSON_AddNumberToObject(root, "NTP_EPOC(mS)", ESP_RTC);
        int64_t difference = (RTC_Epoch - ESP_RTC);
        cJSON_AddNumberToObject(root, "DIFFERENCE(mS)",        difference);
        cJSON_AddNumberToObject(root, "CALCULATED OFFSET (uS)", offs);
        cJSON_AddNumberToObject(root, "CALCULATED RTT(uS)",     rtt);

        // Serialize JSON into our payload buffer.
        memset(payLoadData_NTP, 0, sizeof(payLoadData_NTP));
        cJSON_PrintPreallocated(root, payLoadData_NTP, sizeof(payLoadData_NTP), false);
        strcpy((char*)s_Message_Rx.payload_p8, payLoadData_NTP);

        // Clean up JSON object.
        cJSON_Delete(root);

        // Send response back to console actor.
        console_send_responce_to_console_xface(&s_Message_Rx);

        // Increment sync cycle counter for interval logic.
        sync_cycle_count++;

        // Delay until next poll.
        vTaskDelay(pdMS_TO_TICKS(poll_interval_s * 1000));
    }
}

static void reset_time_buffers(void)
{
	memset(samples,0,sizeof(samples));
	memset(offset_history,0,sizeof(offset_history));
	offset_history_idx = 0;
	offset_history_count = 0;
	offs=0;
	rtt=0;
	sync_cycle_count = 0;
	last_offset_us = 0;
	avg_drift_us_per_s = 0;
}

