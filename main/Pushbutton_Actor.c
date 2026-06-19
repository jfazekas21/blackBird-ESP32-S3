/*
 * Pushbutton_Actor.c
 *
 *  Created on: 14-Jul-2022
 *      Author: Amruta Dixit and Ashwini
 *  Modified on: [Date]
 *      Modifications:
 *        - Converted polling-based pushbutton handling to interrupt-based.
 *        - Retained the 'monitor' task as per user request.
 *        - Preserved PSRAM_ATTR_BSS attributes.
 *        - Allowed detection of both rising and falling edges.
 *        - Removed unnecessary polling functions.
 */

#include "actor.h"
#include "Config.h"
#include "Console_Actor.h"
#include "Pushbutton_Actor.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "soc/rtc_cntl_reg.h"


#if defined(B480) || defined(B553)
#define PB1 GPIO_NUM_38
#define PB2 GPIO_NUM_39
#else
#define PB1 GPIO_NUM_38
#endif

static const char * THIS_ACTOR = "PUSHBUTTON";
static const char THIS_ACTOR_ID = PUSHBUTTON;  // assign src id
#define MAX_SEQUENCE_LENGTH 16

static char button_sequence[MAX_SEQUENCE_LENGTH];
#define MAX_COMMAND_LENGTH 2048  // Adjust according to your maximum command length
#define TAP_SEQUENCE_TIMEOUT_MS 800     // 500 milliseconds
#define TAP_MAX_DURATION_MS 1000        // 1 second
#define HOLD_MIN_DURATION_MS 2000       // 2 seconds
#define DEBOUNCE_TIME_MS 50             // Debounce time for buttons
#define THREE_HOURS_MS (3 * 60 * 60 * 1000)
#define DEFAULT_DA_URL  "https://stg-api.havenlighting.com/api/Device/DeviceAnnounce"
#define DEFAULT_API_KEY "DEFAULT_API_KEY"
#define DEFAULT_RESCUE_SSID "shopHaven"
#define DEFAULT_RESCUE_PWD  "12345678"

typedef struct {
    int id;
    char command[MAX_COMMAND_LENGTH];
} LightingCommand;

const LightingCommand defaultCommands[] = {
    {1, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[0.0,0.0,0.0],\"peakColor\":[120.0,100.0,10.0],\"valleyColor\":[120.0,100.0,100.0],\"amp1\":1.0,\"wave1\":12.0,\"speed1\":1.0,\"amp2\":0.8,\"wave2\":429.0,\"speed2\":1.0,\"amp3\":1.0,\"wave3\":99.0,\"speed3\":6.0}}"},
    {2, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[148.0,100.0,100.0],\"peakColor\":[240.0,100.0,10.0],\"valleyColor\":[190.0,100.0,68.0],\"amp1\":1.0,\"wave1\":189.0,\"speed1\":1.0,\"amp2\":1.0,\"wave2\":600.0,\"speed2\":-2.0,\"amp3\":1.0,\"wave3\":600.0,\"speed3\":0.0}}"},
    {3, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[0.0,0.0,0.0],\"peakColor\":[120.0,100.0,100.0],\"valleyColor\":[0.0,100.0,100.0],\"amp1\":1.0,\"wave1\":3.0,\"speed1\":2.0,\"amp2\":1.0,\"wave2\":189.0,\"speed2\":-1.0,\"amp3\":0.0,\"wave3\":600.0,\"speed3\":0.0}}"},
    {4, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[0,0,0],\"peakColor\":[240,100,100],\"valleyColor\":[120,100,100],\"amp1\":1,\"wave1\":36,\"speed1\":2,\"amp2\":1,\"wave2\":600,\"speed2\":-1,\"amp3\":0,\"wave3\":600,\"speed3\":0}}"},
	{5, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Custom\" , \"Config\" : {\"colorSelections\":[\"0,100,100\",\"240,100,100\"],\"bgColor\":[0.0,0.0,0.0],\"colorLength\":288.0,\"paddingLength\":0.0,\"transitionType\":\"None\",\"movingSpeed\":0.0,\"enableMirror\":0,\"mirrorPosition\":0.0,\"oscAmp\":0.0,\"oscPeriod\":1.0}}"},
    {6, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[7,100,5],\"peakColor\":[23,94,100],\"valleyColor\":[0,100,100],\"amp1\":1,\"wave1\":36,\"speed1\":1,\"amp2\":1,\"wave2\":1692,\"speed2\":-1,\"amp3\":0.9,\"wave3\":7200,\"speed3\":0}}"},
	{7, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[23.0,94.0,7.0],\"peakColor\":[23.0,94.0,53.0],\"valleyColor\":[23.0,94.0,100.0],\"amp1\":1.0,\"wave1\":105.0,\"speed1\":2.0,\"amp2\":0.0,\"wave2\":600.0,\"speed2\":0.0,\"amp3\":0.0,\"wave3\":600.0,\"speed3\":0.0}}"},
    {8, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Custom\" , \"Config\" : {\"colorSelections\":[\"148,100,100\",\"148,100,50\",\"215,100,40\",\"225,100,30\",\"240,100,20\",\"248,100,10\",\"248,100,9\",\"248,100,8\",\"248,100,7\",\"248,100,6\",\"248,100,5\",\"248,100,4\",\"248,100,3\",\"248,100,2\",\"248,100,1\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\",\"248,100,0\"],\"bgColor\":[0.0,0.0,0.0],\"colorLength\":3.0,\"paddingLength\":0.0,\"transitionType\":\"None\",\"movingSpeed\":-61.0,\"enableMirror\":0,\"mirrorPosition\":0.0,\"oscAmp\":0.0,\"oscPeriod\":1.0}})"},
    {9, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\":\"Ripple\" , \"Config\" : {\"startColor\":[0.0,100.0,5.0],\"peakColor\":[38.0,100.0,100.0],\"valleyColor\":[0.0,0.0,0.0],\"amp1\":1.0,\"wave1\":3.0,\"speed1\":2.0,\"amp2\":1.0,\"wave2\":51.0,\"speed2\":-2.0,\"amp3\":0.9,\"wave3\":96.0,\"speed3\":4.0}}"},
    {10, "{\"CH\":[1,2,3,4] , \"Brightness\" : 100.0, \"Function\" : \"Ripple\" , \"Config\" : {\"startColor\":[0.0,0.0,0.0],\"peakColor\":[7.0,100.0,0.0],\"valleyColor\":[148.0,100.0,100.0],\"amp1\":1.0,\"wave1\":48.0,\"speed1\":2.0,\"amp2\":1.0,\"wave2\":57.0,\"speed2\":-3.0,\"amp3\":0.9,\"wave3\":1200.0,\"speed3\":0.0}}"}
};

PSRAM_ATTR_BSS static StackType_t PBpostTaskStack[PB_MON_UPDATE_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static char line[MAX_COMMAND_LENGTH];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_PB[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData_Post_Info[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static StackType_t xTaskStack[PBUTTON_TASK_STACK_DEPTH];
PSRAM_ATTR_BSS static StackType_t PBtensecondTaskStack[PB_MON_UPDATE_TASK_STACK_DEPTH], PBtensecondTaskStack1[PB_MON_UPDATE_TASK_STACK_DEPTH];

#define DEFAULT_DA_URL	"https://stg-api.havenlighting.com/api/Device/DeviceAnnounce" //"https://ase-hvnlght-dealers-api-dev.azurewebsites.net/api/Device/DeviceAnnounce"
#define DEFAULT_API_KEY "DEFAULT_API_KEY"
#define DEFAULT_RESCUE_SSID	"shopHaven"
#define DEFAULT_RESCUE_PWD	"12345678"


//struct PushBt_Type PushBut1_Para,PushBut2_Para;
static AMessage_st s_Message_Tx;  //s_Message_Rx,
BaseType_t PBMonitor, PBpollMonitor, PBupdateMonitor;
TaskHandle_t PBHandle =NULL, PBpollHandle=NULL, PBupdateHandle=NULL, PBTENSECONDHandle =NULL, PBTENSECONDHandle1 =NULL, Post_Information_Handle = NULL;
QueueHandle_t PB_Rx_Queue, Post_Information_Que = NULL,gpio_evt_queue=NULL;;  //PB_Tx_Queue
static StaticTask_t xPUSHBUTTONTaskBuffer,xPBtensecondTaskBuffer,xPBtensecondTaskBuffer1,xPBpostTaskBuffer; //// Declare a static task control block xPBpollTaskBuffer,xPBupdateTaskBuffer,
static uint8_t dest_actor[15]={0};
static uint64_t playListTime = 0;
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
static uint64_t pingListTime = 0;
static int NET_status = -1;
static uint8_t testLoopCmd_sent = false,dimWhiteCmd_sent = false;
static uint64_t lastCmdSentTime = 0;
char Event[10]="EVENT";
uint8_t tap_count;
static int FirstEntry = 0;

static uint8_t isOn = 0; // Static variable to keep track of the current state
static uint8_t l_color_Index = 0;

static void init(void *a, void *b);
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
void ten_second_tsk(void *pvParameters __attribute__((unused)));
static void Post_Information (void *pvParameters __attribute__((unused)));
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx, char *payLoadData_new);
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value);
static void toggle_lighting_state(void);
static void start_ble_advertisement_with_timer();
static void stop_ble_advertisement(void* arg);
static void ToggleAllColors(void);
void CreateAndSendLightingOnCommand(void);
static void sendLedState(const char *stateName, int duration);
static uint64_t get_current_time_ms();
static void send_ping(void);
static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason);
static void IRAM_ATTR gpio_isr_handler(void* arg);
static void gpio_task(void* arg);
static void handle_button_event(uint32_t gpio_num, int level, int64_t now);
static void process_pb1_event(int level, int64_t now);
static void process_pb2_event(int level, int64_t now);

// Time of the last button release (in microseconds)
static int64_t last_button_release_time = 0;

// Sequence buffer
#define MAX_SEQUENCE_LENGTH 16
typedef struct {
    char button;          // 'A' for PB1, 'B' for PB2
    bool is_hold;         // true if hold, false if tap
    int64_t duration_ms;  // Duration of hold in milliseconds
} ButtonEvent;

// Button states
static int pb1_state = 0;
static int pb2_state = 0;

// Button press times (in microseconds)
static int64_t pb1_press_time = 0;
static int64_t pb2_press_time = 0;

// Hold duration thresholds (in milliseconds)
#define HOLD_MIN_DURATION_MS 2000  // Adjust as needed

// Timer handle for stopping BLE advertisement
esp_timer_handle_t ble_timer = NULL;
PSRAM_ATTR_BSS static uint8_t PB_Rx_QueueStorage [PB_RX_QUEUE_LENGTH * sizeof(AMessage_st)], Post_PB_Rx_QueueStorage[PB_Post_QUEUE_LENGTH * PUSH_POST_INFO_TASK_QUEUE_ITEMSIZE],PB_gpio_evt_QueueStorage[PB_gpio_evt_queue_LENGTH * PB_gpio_evt_queue_ITEMSIZE];
static StaticQueue_t PB_Rx_QueueBuffer, Post_PB_Rx_QueueBuffer,PB_gpio_evt_queue_QueueBuffer;

#define MAX_SEQUENCE_LENGTH 16

static char button_sequence[MAX_SEQUENCE_LENGTH];
static int sequence_length = 0;
static char sequence_type[8]; // "Tap" or "Hold"

// For PB1
static int64_t pb1_last_event_time = 0;  // In milliseconds

// For PB2
static int64_t pb2_last_event_time = 0;  // In milliseconds



#define MAX_SEQUENCE_LENGTH 16

static char button_sequence[MAX_SEQUENCE_LENGTH];

static char sequence_type[8]; // "Tap" or "Hold"

// Array to store durations of hold events
static int64_t hold_durations[MAX_SEQUENCE_LENGTH];
// Debounce variables
#define DEBOUNCE_DELAY_MS 50  // Debounce delay
// Hold duration threshold (in milliseconds)
#define HOLD_MIN_DURATION_MS 2000  // Adjust as needed

static void init(void *a, void *b){    //Init push button config

	if (FirstEntry == 0)
	{
		FirstEntry = 1;
		
		 // Configure pushbutton GPIOs for interrupts
        gpio_config_t io_conf = {};
        io_conf.intr_type = GPIO_INTR_ANYEDGE; // Interrupt on any edge (rising and falling)

#if defined B480  ||  defined(B553)
        io_conf.pin_bit_mask = (1ULL << PB1) | (1ULL << PB2);
#else      
         io_conf.pin_bit_mask = (1ULL << PB1);
#endif


        io_conf.mode = GPIO_MODE_INPUT;
        io_conf.pull_up_en = 1; // Enable pull-up resistors if necessary
        gpio_config(&io_conf);

        // Create a queue to handle GPIO events from ISR
		gpio_evt_queue = xQueueCreateStatic(PB_gpio_evt_queue_LENGTH, PB_gpio_evt_queue_ITEMSIZE, PB_gpio_evt_QueueStorage, &PB_gpio_evt_queue_QueueBuffer);


PBTENSECONDHandle = xTaskCreateStaticPinnedToCore(
											gpio_task,                 // Task function
											"PB_GPIO_TASK",            // Task name
											PB_MON_UPDATE_TASK_STACK_DEPTH,        // Stack size in words
											NULL,                    // Task parameters (not used here)
											PB_MON_UPDATE_TASK_PRIORITY,                       // Task priority
											PBtensecondTaskStack,              // Pointer to task stack (allocated in PSRAM)
											&xPBtensecondTaskBuffer,             // Pointer to task control block
											1
											);

        // Install GPIO ISR service
        gpio_install_isr_service(0);

        // Hook ISR handler for specific GPIO pins
#if defined B480  ||  defined(B553)
        gpio_isr_handler_add(PB1, gpio_isr_handler, (void*) PB1);
        gpio_isr_handler_add(PB2, gpio_isr_handler, (void*) PB2);
#else      
        gpio_isr_handler_add(PB1, gpio_isr_handler, (void*) PB1);
#endif
        
		if(PB_Rx_Queue == NULL)
		{
			PB_Rx_Queue = xQueueCreateStatic(PB_RX_QUEUE_LENGTH, sizeof(AMessage_st), PB_Rx_QueueStorage, &PB_Rx_QueueBuffer);
		}
		PBHandle = xTaskCreateStaticPinnedToCore(
				monitor,                 // Task function
				"PB Monitor",            // Task name
				PBUTTON_TASK_STACK_DEPTH,        // Stack size in words
				NULL,                    // Task parameters (not used here)
				PBUTTON_TASK_PRIORITY,                       // Task priority
				xTaskStack,              // Pointer to task stack (allocated in PSRAM)
				&xPUSHBUTTONTaskBuffer,           // Pointer to task control block
				1	//0
			);
		if (PBHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
				printf("Failed to create task\n");
#endif
				// Handle error
			}
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	}
}

static void monitor(void *pvParameters __attribute__((unused))){
		while (1) {
			AMessage_st s_Message_Rx_data;
			AMessage_st *s_Message_Rx = &s_Message_Rx_data;
			memset(Rx_buffer,0,sizeof(Rx_buffer));
			memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(PB_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))
			{
//			printf("PB msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("PB DT = %s\n",s_Message_Rx->payload_p8);
//			}				
				strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
				if(s_Message_Rx->payload_p8 != NULL)
				{
					console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
					s_Message_Rx->payload_p8 = NULL;
				}
				s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "INIT"))
			{
				if (FirstEntry == 0){
				init(0,0);
				}
				Add_Response_msg("Pushbutton initialization is done.", s_Message_Rx, payLoadData);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
//			 {
//				get_actor_properties(s_Message_Rx);
//			 }
			else
			{
				//Pushbutton error message: invalid method
			    Add_Response_msg("Invalid method", s_Message_Rx, payLoadData);
			}
		}
	}
}

void ten_second_tsk(void *pvParameters __attribute__((unused))) {
	while(1){
		uint64_t u64CurrentTime = get_current_time_ms();

		if (playListTime != 0 && llabs(u64CurrentTime - playListTime) >= 10000) // 10 seconds in milliseconds
		{
			playListTime = get_current_time_ms();
			ToggleAllColors();
		}

		if (playListTime == 0)
		{
			break;
		}
		vTaskDelay(2000 / portTICK_PERIOD_MS);  //500
	}
	PBTENSECONDHandle1 = NULL;
	vTaskDelete(PBTENSECONDHandle1);  // Delete the task
}

void PUSHBUTTON_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;

	strcpy((char*)dest_actor,(char*)s_Message->src_Actor_a8);

	if (FirstEntry == 0)
		init(0,0);

	uint8_t state = xQueueSend(PB_Rx_Queue, s_Message, QUE_DELAY); //10
	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<PB.ERROR(PB RX Queue is full)\n");
		}
		else
		{
			printf("<PB.ERROR(PB RX Queue send unsuccessful)\n");
		}
	}
}//	PUSHBUTTON_ConsolWriteToActor


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
	char str[200]={0};
	uint8_t	temp[10];

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n PB s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"WIFI")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"AP_INFO")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "SSID");
                // Send ping command to check server connectivity
                cJSON *responseObject = cJSON_CreateObject();
                cJSON_AddStringToObject(responseObject, "IP_ADDRESS", "www.google.com");
                cJSON_AddNumberToObject(responseObject, "TIMEOUT", 1000);                
				memset(payLoadData,0,sizeof(payLoadData));
				cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					Send_CMD_To_Other_Actor(WIFI, "WIFI", payLoadData, strlen(payLoadData), "PING");
				}
				else
				{
					Send_CMD_To_Other_Actor(ETH, "ETH", payLoadData, strlen(payLoadData), "PING");
				}
				cJSON_Delete(responseObject);
			}
		}
		cJSON_Delete(in_JSON);
		return;

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
			cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "NET_STATUS");
			if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
			{
				strcpy((char*)temp,name_JSON->valuestring);
				NET_status = (uint8_t) (temp[0]-0x30);  // convert ASCII to hex
				if(Post_Information_Que != NULL)
				{					
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);				
					xQueueSend(Post_Information_Que, payLoadData, QUE_DELAY);					
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"SYSTEM")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");
		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");
			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *name_JSON 		= cJSON_GetObjectItem(responseKey, "DEVICEID");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				{
					memset(payLoadData,0,sizeof(payLoadData));
					cJSON_PrintPreallocated(responseKey, payLoadData, sizeof(payLoadData), false);
					if(Post_Information_Que != NULL)
						xQueueSend(Post_Information_Que, payLoadData, QUE_DELAY);				
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
	cJSON_PrintPreallocated(responseObject, payLoadData_new, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData_new);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static void help(AMessage_st* s_Message_Rx) {
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the Pushbutton actor.");
	cJSON_AddStringToObject(responseObject, "SET(string FOTA_STRING)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help



static void toggle_lighting_state(void) {
    if (isOn==0) {
        // Create the "ALL OFF" command
        cJSON *responseObject = cJSON_CreateObject();
        cJSON *channelArray = cJSON_CreateArray();
        cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(-1));
        cJSON_AddItemToObject(responseObject, "CH", channelArray);
		memset(payLoadData_PB,0,sizeof(payLoadData_PB));
		cJSON_PrintPreallocated(responseObject, payLoadData_PB, sizeof(payLoadData_PB), false);			
        Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", payLoadData_PB, strlen(payLoadData_PB), "OFF");
        cJSON_Delete(responseObject);
    }
    else {

		// Create the main JSON object
		cJSON *jsonObject = cJSON_CreateObject();

		// Create and add the array for the "CH" key
		cJSON *channelArray = cJSON_CreateArray();
		cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(-1));
		cJSON_AddItemToObject(jsonObject, "CH", channelArray);

		cJSON_AddNumberToObject(jsonObject, "Brightness", 100);
		// Add the "Function" key to the main JSON object
		cJSON_AddStringToObject(jsonObject, "Function", "colorIndex");


		// Create the nested "Config" JSON object
		cJSON *configObject = cJSON_CreateObject();
		if (isOn==1)
		{
			cJSON_AddNumberToObject(configObject, "Index", 27);
		}
		else if (isOn==2)
		{
			cJSON_AddNumberToObject(configObject, "Index", 25);
		}
		else if (isOn==3)
		{
			cJSON_AddNumberToObject(configObject, "Index", 20);
		}
		else if (isOn==4)
		{
			cJSON_AddNumberToObject(configObject, "Index", 0);
		}
		else if (isOn==5)
		{
			cJSON_AddNumberToObject(configObject, "Index", 7);
		}
		else if (isOn==6)
		{
			cJSON_AddNumberToObject(configObject, "Index", 10);
		}
		else if (isOn==7)
		{
			cJSON_AddNumberToObject(configObject, "Index", 12);
		}
		else if (isOn==8)
		{
			cJSON_AddNumberToObject(configObject, "Index", 14);
		}
		else if (isOn==9)
		{
			cJSON_AddNumberToObject(configObject, "Index", 16);
		}
		else if (isOn==10)
		{
			cJSON_AddNumberToObject(configObject, "Index", 19);
		}

		// Add the "Config" object to the main JSON object
		cJSON_AddItemToObject(jsonObject, "Config", configObject);

		// Convert the JSON object to a string
		memset(payLoadData_PB,0,sizeof(payLoadData_PB));
		cJSON_PrintPreallocated(jsonObject, payLoadData_PB, sizeof(payLoadData_PB), false);	
		// Send the command
		Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", payLoadData_PB, strlen(payLoadData_PB), "ON");


		// Free the JSON object
		cJSON_Delete(jsonObject);
    }

    isOn++;
	if (isOn>=11)
	{
		isOn = 0;
	}
}
static void set_to_other_actor(const char* dest_Actor,const uint8_t data_type, const char* parameter, const void * value) {
	uint8_t out_val [128] 	= {0}; //(uint8_t*) heap_caps_calloc(128,sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	char payload[100] = {0};
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
	cJSON_PrintPreallocated(my_JSON, payload, sizeof(payload), false);
		
	uint8_t *newpointer = (uint8_t*) heap_caps_calloc(strlen((char*) payload) + 1, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	if (newpointer == NULL)
	{
		printf("Memory allocation failed\n");
		return;
	}
	s_Message_Tx.payload_p8 = newpointer;
	strcpy((char*) (s_Message_Tx.payload_p8), (char*) payload);
	s_Message_Tx.payload_size = strlen((char*)payload);
	strcpy((char*) s_Message_Tx.src_Actor_a8	, THIS_ACTOR	);
	strcpy((char*) s_Message_Tx.dest_Actor_a8	, dest_Actor	);
	strcpy((char*) s_Message_Tx.cmdFun_a8		, "SET"			);
	console_ActorWriteToConsole_xface( &s_Message_Tx);
	cJSON_Delete(my_JSON);
}//	set_to_other_actor

static void start_ble_advertisement_with_timer() {

	   if (ble_timer != NULL) {
	        // Timer is already created, delete it
	        esp_err_t err = esp_timer_stop(ble_timer);
	        if (err != ESP_OK) {
	#ifdef ENABLE_PRINT_MSG
	            printf("Failed to stop existing timer: %d\n", err);
	#endif
	        }

	        err = esp_timer_delete(ble_timer);
	        if (err != ESP_OK) {
	#ifdef ENABLE_PRINT_MSG
	            printf("Failed to delete existing timer: %d\n", err);
	#endif
	            return;
	        } else {
	#ifdef ENABLE_PRINT_MSG
	            printf("Existing timer deleted successfully\n");
	#endif
	        }

	        ble_timer = NULL;  // Reset the timer handle
	    }

    char BLE_connect = 1;
//    Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_STOP");
    set_to_other_actor("BLE", U_INT8, "CONN_MODE", &BLE_connect);
    Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_START");

    // Create a one-shot timer that triggers after 10 minutes (600000 milliseconds)
    const esp_timer_create_args_t ble_timer_args = {
        .callback = &stop_ble_advertisement,
        .name = "ble_timer"
    };

    esp_timer_create(&ble_timer_args, &ble_timer);
    esp_timer_start_once(ble_timer, 1200000000); // 20 minutes in microseconds
}

// Function to stop BLE advertisement
static void stop_ble_advertisement(void* arg) {
	// Delete the timer
	    esp_err_t err = esp_timer_delete(ble_timer);
	    if (err != ESP_OK) {
	#ifdef ENABLE_PRINT_MSG
	        printf("Failed to delete timer: %d\n", err);
	#endif
	    } else {
	#ifdef ENABLE_PRINT_MSG
	        printf("Timer deleted successfully\n");
	#endif
	        ble_timer = NULL;  // Reset the timer handle
	    }
    Send_CMD_To_Other_Actor(BLE, "BLE", "\0", 0, "ADVERT_STOP");
}

static void ToggleAllColors(void)
{
	int numberOfCommands = sizeof(defaultCommands) / sizeof(LightingCommand);
	strcpy(line, defaultCommands[l_color_Index].command);
	Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", line, strlen(line), "ON");
	l_color_Index++;
	if(l_color_Index >= numberOfCommands)
	{
		l_color_Index = 0;
	}
}

void CreateAndSendLightingOnCommand(void) {
    // Create the main JSON object
    cJSON *jsonObject = cJSON_CreateObject();

    // Create and add the array for the "CH" key
    cJSON *channelArray = cJSON_CreateArray();
    cJSON_AddItemToArray(channelArray, cJSON_CreateNumber(-1)); // Single channel -1
    cJSON_AddItemToObject(jsonObject, "CH", channelArray);

    cJSON_AddNumberToObject(jsonObject, "Brightness", 100);
    // Add the "Function" key-value pair
    cJSON_AddStringToObject(jsonObject, "Function", "Custom");

    // Create the nested "Config" JSON object
    cJSON *configObject = cJSON_CreateObject();

    // Create and add the "colorSelections" array to the "Config" object
    cJSON *colorSelectionsArray = cJSON_CreateArray();
    cJSON_AddItemToArray(colorSelectionsArray, cJSON_CreateString("0.00 , 100.00 , 100.00"));
    cJSON_AddItemToArray(colorSelectionsArray, cJSON_CreateString("0.00 , 0.00 , 100.00"));
    cJSON_AddItemToArray(colorSelectionsArray, cJSON_CreateString("120.00 , 100.00 , 100.00"));
    cJSON_AddItemToArray(colorSelectionsArray, cJSON_CreateString("240.00 , 100.00 , 100.00"));
    cJSON_AddItemToObject(configObject, "colorSelections", colorSelectionsArray);

    // Add other key-value pairs to the "Config" object
    cJSON_AddItemToObject(configObject, "bgColor", cJSON_CreateFloatArray((const float[]){0.00, 0.00, 0.00}, 3));
    cJSON_AddNumberToObject(configObject, "colorLength", 98.00);     // Updated colorLength
    cJSON_AddNumberToObject(configObject, "paddingLength", 98.00);   // Updated paddingLength
    cJSON_AddStringToObject(configObject, "transitionType", "Fade Out");
    cJSON_AddNumberToObject(configObject, "movingSpeed", 66.17);     // Updated movingSpeed
    cJSON_AddNumberToObject(configObject, "enableMirror", 0);
    cJSON_AddNumberToObject(configObject, "mirrorPosition", 15.00);  // Updated mirrorPosition
    cJSON_AddNumberToObject(configObject, "oscAmp", 0.00);
    cJSON_AddNumberToObject(configObject, "oscPeriod", 1.00);       // Updated oscPeriod

    // Add the "Config" object to the main JSON object
    cJSON_AddItemToObject(jsonObject, "Config", configObject);

    // Convert the JSON object to a string
	memset(payLoadData_PB,0,sizeof(payLoadData_PB));
	cJSON_PrintPreallocated(jsonObject, payLoadData_PB, sizeof(payLoadData_PB), false);
	
    // Send the command
    Send_CMD_To_Other_Actor(LIGHTING, "LIGHTING", payLoadData_PB, strlen(payLoadData_PB), "ON");

    // Free the JSON object
    cJSON_Delete(jsonObject);
}

static void sendLedState(const char *stateName, int duration) {
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    // Print the JSON object as a string
	payLoadData_PB [0]= '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData_PB, sizeof(payLoadData_PB), false);
    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoadData_PB, strlen(payLoadData_PB), "SETSTATE");

    // Free the allocated memory
    cJSON_Delete(responseObject);
}
static uint64_t get_current_time_ms() {
	return get_current_rtc_time_ms(0);
}

static void Post_Information (void *pvParameters __attribute__((unused)))
{
	char buffer[600] = {0}, DEVICEID_VERSION[30] = {0}, FIRMWARE_VERSION[30] = {0},  HARDWARE_VERSION[30] = {0}, DEVICE_ANNOUNCE_URL_D[100] = {0}, API_KEY_P[40] = {0};
	char found = 0;
	char DEVICE_ANNOUNCE_PERIOD_P[30] = {0};
	cJSON *name_JSON = NULL;

	cJSON *my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
	        goto exit;
	    }
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("NET_STATUS"));
	cJSON *jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Post_Info,0,sizeof(payLoadData_Post_Info));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Post_Info, sizeof(payLoadData_Post_Info), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(CONSOLE,"CONSOLE", payLoadData_Post_Info, strlen(payLoadData_Post_Info), "GET");

	my_JSON1  	= cJSON_CreateArray();
	if (my_JSON1 == NULL) {
	        goto exit;
	    }
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICEID"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("FIRMWARE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("HARDWARE"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICE_ANNOUNCE_URL"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("API_KEY"));
	cJSON_AddItemToArray(my_JSON1, cJSON_CreateString("DEVICE_ANNOUNCE_PERIOD"));
	jsonObject = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonObject, "Property_Names", my_JSON1);
	memset(payLoadData_Post_Info,0,sizeof(payLoadData_Post_Info));
	cJSON_PrintPreallocated(jsonObject, payLoadData_Post_Info, sizeof(payLoadData_Post_Info), false);
	cJSON_Delete(jsonObject);
	Send_CMD_To_Other_Actor(SYSTEM,"SYSTEM", payLoadData_Post_Info, strlen(payLoadData_Post_Info), "GET");

	while(1)
	{
		if (pdTRUE == xQueueReceive(Post_Information_Que, (void*)buffer, portMAX_DELAY))
		{
			cJSON *In_JSON 	  = cJSON_Parse((char*)buffer);
			if (In_JSON == NULL) {
		    	printf("Invalid Json input at %d in %s",__LINE__,__FUNCTION__);
				goto exit;
			}

			name_JSON 		= cJSON_GetObjectItem(In_JSON, "DEVICEID");
			if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
			{
				strcpy(DEVICEID_VERSION, name_JSON->valuestring );

				name_JSON = cJSON_GetObjectItem(In_JSON, "FIRMWARE");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				strcpy(FIRMWARE_VERSION, name_JSON->valuestring );

				name_JSON 		= cJSON_GetObjectItem(In_JSON, "HARDWARE");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
				strcpy(HARDWARE_VERSION, name_JSON->valuestring );

				cJSON *name_JSON = cJSON_GetObjectItem(In_JSON, "DEVICE_ANNOUNCE_URL");
				if((name_JSON != NULL) && (cJSON_IsString(name_JSON)))
				strcpy(DEVICE_ANNOUNCE_URL_D, name_JSON->valuestring);


				name_JSON 		= cJSON_GetObjectItem(In_JSON, "API_KEY");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
				strcpy(API_KEY_P, name_JSON->valuestring );

				name_JSON 		= cJSON_GetObjectItem(In_JSON, "DEVICE_ANNOUNCE_PERIOD");
				if((name_JSON != NULL) && (name_JSON->valuestring!=NULL))
				strcpy(DEVICE_ANNOUNCE_PERIOD_P, name_JSON->valuestring );

				found++;
			}
			cJSON_Delete(In_JSON);
		}
		if(found == 1)
			break;
	}

	if(NET_status == E_NET_CONNECTED)
	{
		char CPU0_REST_REA[50] = {0};
		char CPU1_REST_REA[50] = {0};
		Get_ESP_Reset_Reason(CPU0_REST_REA, CPU1_REST_REA);

		uint64_t mills =0;
		struct timeval currentTime;

		gettimeofday(&currentTime, NULL); // Get current time
		mills = (uint64_t)(currentTime.tv_sec * 1000L) + (uint64_t)(currentTime.tv_usec / 1000L); // Convert to milliseconds

		// Create the root JSON object
		    cJSON *root = cJSON_CreateObject();
		    if (root == NULL)
		    {
		        return;
		    }
		    cJSON_AddStringToObject(root, "DEVICEID", (char*)&DEVICEID_VERSION);
		    // Add UniqueProductName
		    cJSON_AddStringToObject(root, "UniqueProductName", "X-Series");
		    cJSON *firmware_versions = cJSON_AddArrayToObject(root, "FirmwareVersions");
		    if (firmware_versions == NULL) {
		        cJSON_Delete(root);
		        return;
		    }
		    cJSON_AddItemToArray(firmware_versions, cJSON_CreateString((char*) FIRMWARE_VERSION));
		    cJSON *hardware_versions = cJSON_AddArrayToObject(root, "HardwareVersions");
		    if (hardware_versions == NULL) {
		        cJSON_Delete(root);
		        return;
		    }
		    cJSON_AddItemToArray(hardware_versions, cJSON_CreateString((char*) HARDWARE_VERSION));
		    cJSON_AddNumberToObject(root, "DeviceTime", mills);
			cJSON_AddStringToObject(root, "CPU0ResetReason",CPU0_REST_REA);//mills);  //682355204);
			cJSON_AddStringToObject(root, "CPU1ResetReason",CPU1_REST_REA);//mills);  //682355204);

		    // Add customProperties (nested object)
		    cJSON *custom_properties = cJSON_AddObjectToObject(root, "customProperties");
		    if (custom_properties == NULL) {
		        cJSON_Delete(root);
		        return;
		    }
		    cJSON_AddBoolToObject(custom_properties, "pingLocation", true);

		    cJSON *root1 = cJSON_CreateObject();
			cJSON_AddStringToObject(root1, "URL", (char*)&DEVICE_ANNOUNCE_URL_D);
			cJSON_AddItemToObject(root1, "HTTP_PAYLOAD", root);
			cJSON_AddStringToObject(root1, "API_KEY", (char*)&API_KEY_P);
			cJSON_AddStringToObject(root1, "DEVICEID", (char*)&DEVICEID_VERSION);
			uint32_t 	deviceAnnouncePeriod1 = atoi(DEVICE_ANNOUNCE_PERIOD_P);
			cJSON_AddNumberToObject(root1, "RETRY_PERIOD", deviceAnnouncePeriod1);
			memset(payLoadData_Post_Info,0,sizeof(payLoadData_Post_Info));
			cJSON_PrintPreallocated(root1, payLoadData_Post_Info, sizeof(payLoadData_Post_Info), false);

		    if (strcmp(DEVICE_ANNOUNCE_URL_D, "") == 0)
		    {
				printf("URL is empty.\n");
			}
		    else
		    {
				Send_CMD_To_Other_Actor(HTTP,"HTTP", payLoadData_Post_Info, strlen(payLoadData_Post_Info), "POST");
			}
		    cJSON_Delete(root1);  // It will free the memory allocated for out_JSON also
	}
	else
	{
		printf("Kindly connect to internet at first.\n");
	}

exit:
	Post_Information_Handle = NULL;
	vTaskDelete(Post_Information_Handle);  // Delete the task
}

static void handle_button_event(uint32_t gpio_num, int level, int64_t now) {
#if defined B480  ||  defined(B553)
    if (gpio_num == PB1) {
        process_pb1_event(level, now);
    } else if (gpio_num == PB2) {
        process_pb2_event(level, now);
    }
#else      
     if (gpio_num == PB1) {
        process_pb1_event(level, now);
    }
#endif
}



static void execute_sequence_actions()
{

#if defined(B543) || defined(B542) || defined(B394) || defined(B527)
    if (strcmp(sequence_type, "Tap") == 0) {
        // Handle tap sequences based on the button_sequence
        if (strcmp(button_sequence, "A") == 0) {
            // Single tap on PB1
        	playListTime = 0;
            toggle_lighting_state();
            send_ping();
        }
    } else if (strcmp(sequence_type, "Hold") == 0) {
        // Handle hold sequences using durations
        for (int i = 0; i < sequence_length; i++) {
            int64_t duration_ms = hold_durations[i];  // Get hold duration
			if ((strcmp(button_sequence, "A") == 0))
            {
                // Handle hold on PB1 based on duration
                if (duration_ms >= 1000 && duration_ms <= 3000)	//(duration_ms >= 3000 && duration_ms <= 10000)
                {
                	printf("Executing test mode for PB1 hold of %lld ms\n", duration_ms);
					CreateAndSendLightingOnCommand();
					testLoopCmd_sent = true;
					dimWhiteCmd_sent = false;
					lastCmdSentTime = get_current_time_ms();
					playListTime = 0;
				}
                else if(duration_ms > 5000 && duration_ms <= 8000)
				{
					printf("Executing ble start mode for PB1 hold of %lld ms\n", duration_ms);
					start_ble_advertisement_with_timer();
				}
			}
			else
			{
				// Default action for PB1 hold
				printf("Default action for PB1 hold of %lld ms\n", duration_ms);
				// Implement any default action if needed
			}
        }
    }
    else
    {
        // Handle unknown event type
        printf("Unknown sequence type: %s\n", sequence_type);
    }




#else
    if (strcmp(sequence_type, "Tap") == 0) {
        // Handle tap sequences based on the button_sequence
        if (strcmp(button_sequence, "A") == 0) {
            // Single tap on PB1
        	playListTime = 0;
            toggle_lighting_state();
        } else if (strcmp(button_sequence, "B") == 0) {
        	playListTime = 0;
            // Single tap on PB2
            ToggleAllColors();
        } else if (strcmp(button_sequence, "AB") == 0 || strcmp(button_sequence, "BA") == 0) {
//        	playListTime = 0;
            // Both buttons tapped within the sequence
            send_ping();
        } else if (strcmp(button_sequence, "ABBBA") == 0) {
			sendLedState("REBOOT_MODE",-1);

			vTaskDelay(1000 / portTICK_PERIOD_MS);
            // Double tap on PB1
            	printf("\n\n JFS_FORMAT \n\n");
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", "\0", 0, "JFS_FORMAT");

        } else if (strcmp(button_sequence, "AABA") == 0) {
            // Double tap on PB1
				Send_CMD_To_Other_Actor(FILE_SYSTEM, "FILE_SYSTEM", "\0", 0, "CPY_ALLJFS_TO_SD");

        } else if (strcmp(button_sequence, "AAA") == 0) {
			start_ble_advertisement_with_timer();
        } else if (strcmp(button_sequence, "BBB") == 0) {
				start_ble_advertisement_with_timer();
        } 
        else {
            // Handle other tap sequences
            // For example, unrecognized tap sequence
        }
    } else if (strcmp(sequence_type, "Hold") == 0) {
        // Handle hold sequences using durations
        for (int i = 0; i < sequence_length; i++) {
//            char button = button_sequence[i];
            int64_t duration_ms = hold_durations[i];  // Get hold duration
            if ((strcmp(button_sequence, "AB") == 0) || (strcmp(button_sequence, "BA") == 0)) {
                // Handle hold on PB1 based on duration
                if (duration_ms >= 3000 && duration_ms <= 10000) {
                     printf("Executing ESP_RESET for PB1 hold of %lld ms\n", duration_ms);
                     Restart_ESP_Xface(0);
                    playListTime = 0;
                }  else {
                    // Default action for PB1 hold
                    printf("Default action for PB1 hold of %lld ms\n", duration_ms);
                    // Implement any default action if needed
                }
            }
            else if ((strcmp(button_sequence, "B") == 0)) {
                // Handle hold on PB1 based on duration
                if (duration_ms >= 3000 && duration_ms <= 10000) {
                    printf("Executing playlist mode for PB2 hold of %lld ms\n", duration_ms);

					ToggleAllColors();
					testLoopCmd_sent = false;
					dimWhiteCmd_sent = false;

					playListTime = get_current_time_ms();

					if(PBTENSECONDHandle1 == NULL)
					{
						PBTENSECONDHandle1 = xTaskCreateStaticPinnedToCore(
								ten_second_tsk,                 // Task function
								"PB ten_second",            // Task name
								PB_MON_UPDATE_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								PB_MON_UPDATE_TASK_PRIORITY,                       // Task priority
								PBtensecondTaskStack1,              // Pointer to task stack (allocated in PSRAM)
								&xPBtensecondTaskBuffer1,             // Pointer to task control block
								1
								);
						if (PBTENSECONDHandle1 == NULL)
						{
							printf("Failed to create task\n");
						}
					}
                }  else {
                    // Default action for PB1 hold
                    printf("Default action for PB2 hold of %lld ms\n", duration_ms);
                    // Implement any default action if needed
                }
            }
            else if ((strcmp(button_sequence, "A") == 0))
            {
                // Handle hold on PB1 based on duration
                if (duration_ms >= 3000 && duration_ms <= 10000)
                {
                	printf("Executing test mode for PB1 hold of %lld ms\n", duration_ms);
					CreateAndSendLightingOnCommand();
					testLoopCmd_sent = true;
					dimWhiteCmd_sent = false;
					lastCmdSentTime = get_current_time_ms();
					playListTime = 0;
				}
			}
			else
			{
				// Default action for PB1 hold
				printf("Default action for PB1 hold of %lld ms\n", duration_ms);
				// Implement any default action if needed
			}
        }
    }
    else
    {
        // Handle unknown event type
        printf("Unknown sequence type: %s\n", sequence_type);
    }
#endif
}



static void process_event_sequence() {
    // Build the sequence response
    cJSON* responseObject = cJSON_CreateObject();
    cJSON_AddStringToObject(responseObject, "STATE", "Sequence Completed");

    cJSON* eventsObject = cJSON_CreateObject();
    cJSON_AddStringToObject(eventsObject, "BUTTON", button_sequence);
    cJSON_AddStringToObject(eventsObject, "TYPE", sequence_type);

    if (strcmp(sequence_type, "Hold") == 0) {
        // Create an array for durations
        cJSON* durationsArray = cJSON_CreateArray();
        for (int i = 0; i < sequence_length; i++) {
            cJSON_AddItemToArray(durationsArray, cJSON_CreateNumber(hold_durations[i]));
        }
        cJSON_AddItemToObject(eventsObject, "DURATION_MS", durationsArray);
    }

    cJSON_AddItemToObject(responseObject, "EVENTS", eventsObject);

    memset(payLoadData_PB, 0, sizeof(payLoadData_PB));
    cJSON_PrintPreallocated(responseObject, payLoadData_PB, sizeof(payLoadData_PB), false);

    // Prepare message to send
    AMessage_st s_Message_Rx;
    memset(&s_Message_Rx, 0, sizeof(s_Message_Rx));
    s_Message_Rx.payload_p8 = (uint8_t*)payLoadData_PB;
    strcpy((char*)s_Message_Rx.dest_Actor_a8, THIS_ACTOR);
    strcpy((char*)s_Message_Rx.cmdFun_a8, "EVENT");
    strcpy((char*)s_Message_Rx.src_Actor_a8, "CONSOLE");
    cJSON_Delete(responseObject);
    console_send_responce_to_console_xface(&s_Message_Rx);
    // Execute actions based on the sequence
    execute_sequence_actions();
}



static void process_pb1_event(int level, int64_t now) {
    int64_t now_ms = now / 1000;  // Convert to milliseconds

    if (level == 0 && pb1_state == 0) {
        // Button PB1 pressed (falling edge)
        if ((now_ms - pb1_last_event_time) >= DEBOUNCE_DELAY_MS) {
            pb1_press_time = now;
            pb1_state = 1;
            pb1_last_event_time = now_ms;
        }
    } else if (level == 1 && pb1_state == 1) {
        // Button PB1 released (rising edge)
        if ((now_ms - pb1_last_event_time) >= DEBOUNCE_DELAY_MS) {
            int64_t press_duration_ms = (now - pb1_press_time) / 1000;  // Convert to milliseconds

            // Determine event type
            const char* event_type = (press_duration_ms >= HOLD_MIN_DURATION_MS) ? "Hold" : "Tap";

            // If sequence is empty, start new sequence
            if (sequence_length == 0) {
                strncpy(sequence_type, event_type, sizeof(sequence_type));
                sequence_type[sizeof(sequence_type)-1] = '\0'; // Ensure null termination

                // Add button to sequence
                if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                    button_sequence[sequence_length] = 'A';
                    if (strcmp(event_type, "Hold") == 0) {
                        hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                    }
                    sequence_length++;
                    button_sequence[sequence_length] = '\0';
                }
            } else {
                // Check if event type matches sequence type
                if (strcmp(event_type, sequence_type) == 0) {
                    // Add button to sequence
                    if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                        button_sequence[sequence_length] = 'A';
                        if (strcmp(event_type, "Hold") == 0) {
                            hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                        }
                        sequence_length++;
                        button_sequence[sequence_length] = '\0';
                    }
                } else {
                    // Event type differs, process existing sequence
                    process_event_sequence();

                    // Start new sequence
                    sequence_length = 0;
                    strncpy(sequence_type, event_type, sizeof(sequence_type));
                    sequence_type[sizeof(sequence_type)-1] = '\0';

                    if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                        button_sequence[sequence_length] = 'A';
                        if (strcmp(event_type, "Hold") == 0) {
                            hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                        }
                        sequence_length++;
                        button_sequence[sequence_length] = '\0';
                    }
                }
            }
            // Update last_button_release_time
            last_button_release_time = now;  // In microseconds
            pb1_state = 0;  // Reset state
            pb1_last_event_time = now_ms;
        }
    }
}

#if defined(B480) || defined(B553)
static void process_pb2_event(int level, int64_t now) {
    int64_t now_ms = now / 1000;  // Convert to milliseconds
    if (level == 0 && pb2_state == 0) {
        // Button PB2 pressed (falling edge)
        if ((now_ms - pb2_last_event_time) >= DEBOUNCE_DELAY_MS) {
            pb2_press_time = now;
            pb2_state = 1;
            pb2_last_event_time = now_ms;
        }
    } else if (level == 1 && pb2_state == 1) {
        // Button PB2 released (rising edge)
        if ((now_ms - pb2_last_event_time) >= DEBOUNCE_DELAY_MS) {
            int64_t press_duration_ms = (now - pb2_press_time) / 1000;  // Convert to milliseconds

            // Determine event type
            const char* event_type = (press_duration_ms >= HOLD_MIN_DURATION_MS) ? "Hold" : "Tap";

            // If sequence is empty, start new sequence
            if (sequence_length == 0) {
                strncpy(sequence_type, event_type, sizeof(sequence_type));
                sequence_type[sizeof(sequence_type)-1] = '\0'; // Ensure null termination

                // Add button to sequence
                if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                    button_sequence[sequence_length] = 'B';
                    if (strcmp(event_type, "Hold") == 0) {
                        hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                    }
                    sequence_length++;
                    button_sequence[sequence_length] = '\0';
                }
            } else {
                // Check if event type matches sequence type
                if (strcmp(event_type, sequence_type) == 0) {
                    // Add button to sequence
                    if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                        button_sequence[sequence_length] = 'B';
                        if (strcmp(event_type, "Hold") == 0) {
                            hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                        }
                        sequence_length++;
                        button_sequence[sequence_length] = '\0';
                    }
                } else {
                    // Event type differs, process existing sequence
                    process_event_sequence();

                    // Start new sequence
                    sequence_length = 0;
                    strncpy(sequence_type, event_type, sizeof(sequence_type));
                    sequence_type[sizeof(sequence_type)-1] = '\0';

                    if (sequence_length < MAX_SEQUENCE_LENGTH - 1) {
                        button_sequence[sequence_length] = 'B';
                        if (strcmp(event_type, "Hold") == 0) {
                            hold_durations[sequence_length] = press_duration_ms; // Store hold duration
                        }
                        sequence_length++;
                        button_sequence[sequence_length] = '\0';
                    }
                }
            }
            // Update last_button_release_time
            last_button_release_time = now;  // In microseconds
            pb2_state = 0;  // Reset state
            pb2_last_event_time = now_ms;
        }
    }
}
#endif

static void gpio_task(void* arg) {
    uint32_t io_num;
    int64_t PB_hold_now = 0, last_time_pb_pressed = 0; // PB_hold_reset = 0;
    char Send_hold_warning = 0;	//, Send_ESP_RESET = 0;
    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, 50 / portTICK_PERIOD_MS)) {
            int level = gpio_get_level(io_num);
            int64_t now = esp_timer_get_time();  // Time in microseconds
    		int64_t temp_cal1 = (now - pb1_press_time)/1000000; 	//in Seconds
    		if(temp_cal1 > 180)
    		{
    			isOn = 0; // Static variable to keep track of the current state
    		}

#if defined(B480) || defined(B553)
    		int64_t temp_cal2 = (now - pb2_press_time)/1000000; 	//in Seconds
    		if(temp_cal2 > 180)
    		{
				l_color_Index = 0;
    		}
#endif

#if defined(B480) || defined(B553)
            if (io_num == PB1) {
                handle_button_event(PB1, level, now);
                last_time_pb_pressed = pb1_press_time/1000; //now/1000;
            } else if (io_num == PB2) {
                handle_button_event(PB2, level, now);
                last_time_pb_pressed = pb2_press_time/1000; //now/1000;
            }
#else
            if (io_num == PB1) {
                handle_button_event(PB1, level, now);
                last_time_pb_pressed = pb1_press_time/1000; //now/1000;
            }
#endif
        }
        else
        {

#if defined(B480) || defined(B553)
           	if(pb1_state == 1)  // If any pushbutton is pressed then check for hold warning
            	{
            		PB_hold_now = (esp_timer_get_time() / 1000) - last_time_pb_pressed; // Convert to milliseconds
					if(PB_hold_now > 10000 && PB_hold_now <= 20000)
					{
						//printf("Executing test mode for PB1 hold of %lld ms\n", PB_hold_now);
						testLoopCmd_sent = false;
						dimWhiteCmd_sent = false;

						playListTime = 0;

						printf("\n *******Send Reset ESP command **********\n");
						Restart_ESP_Xface(0);
					}
            	}

            	if(((pb1_state == 1) || (pb2_state == 1))  && (Send_hold_warning == 0))  // If any pushbutton is pressed then check for hold warning
            	{
            		PB_hold_now = (esp_timer_get_time() / 1000) - last_time_pb_pressed; // Convert to milliseconds
                    if((PB_hold_now > 1000) && (PB_hold_now < 1500))
        			{
                    	sendLedState("Hold Warning",3);
                    	Send_hold_warning = 1;
        			}
            	}
                // Timeout occurred, check if 300 ms have passed since last button release
                if (last_button_release_time > 0) {
                    int64_t now = esp_timer_get_time();
                    int64_t elapsed_time_ms = (now - last_button_release_time) / 1000;  // Convert to milliseconds

                    if (elapsed_time_ms >= TAP_SEQUENCE_TIMEOUT_MS) {
                        // 800 ms have passed since last button release, process the sequence
                        process_event_sequence();

                        // Reset the sequence and last_button_release_time
                        sequence_length = 0;
                        button_sequence[0] = '\0';
                        sequence_type[0] = '\0';
                        last_button_release_time = 0;
                        Send_hold_warning = 0;
                    }
                }
#else
           	if(pb1_state == 1)  // If any pushbutton is pressed then check for hold warning
            	{
            		PB_hold_now = (esp_timer_get_time() / 1000) - last_time_pb_pressed; // Convert to milliseconds
    //                printf("\n elapsed_time_ms = %lld \n\n", PB_hold_now);
            	//                if(duration_ms > 10000 && duration_ms <= 20000)

    							if(PB_hold_now > 10000 && PB_hold_now <= 20000)
            	                {
            	                	printf("Executing reboot mode for PB1 hold of %lld ms\n", PB_hold_now);
            						testLoopCmd_sent = false;
            						dimWhiteCmd_sent = false;

            						playListTime = 0;

            	                	printf("\n *******Send Reset ESP command **********\n");
            	                	Restart_ESP_Xface(0);
            	                }
            	}

            	if(((pb1_state == 1))  && (Send_hold_warning == 0))  // If any pushbutton is pressed then check for hold warning
            	{
            		PB_hold_now = (esp_timer_get_time() / 1000) - last_time_pb_pressed; // Convert to milliseconds
             		if((PB_hold_now > 2000) && (PB_hold_now < 2500))
        			{
                    	sendLedState("Hold Warning",3);	//sendLedState("Hold Warning",3);
                    	Send_hold_warning = 1;
        			}
            	}
                // Timeout occurred, check if 300 ms have passed since last button release
                if (last_button_release_time > 0) {
                    int64_t now = esp_timer_get_time();
                    int64_t elapsed_time_ms = (now - last_button_release_time) / 1000;  // Convert to milliseconds

                    if (elapsed_time_ms >= TAP_SEQUENCE_TIMEOUT_MS) {
                        // 800 ms have passed since last button release, process the sequence
                        process_event_sequence();

                        // Reset the sequence and last_button_release_time
                        sequence_length = 0;
                        button_sequence[0] = '\0';
                        sequence_type[0] = '\0';
                        last_button_release_time = 0;
                        Send_hold_warning = 0;
                    }
                }
#endif
        }
    }
}


static void IRAM_ATTR gpio_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}


static void send_ping(void)
{
				
		uint64_t u64CurrentTime = get_current_time_ms();
		uint8_t PostHttpReq = 0;
		
		if(pingListTime == 0)
		{
			PostHttpReq = 1;
		}
		else if (u64CurrentTime >= pingListTime)
		{

#if defined(B543) || defined(B542) || defined(B394) || defined(B527)
			if ((u64CurrentTime - pingListTime) >= 120000) // 120 seconds in milliseconds
			{
				PostHttpReq = 1;
			}
			else
			{
				printf("Try ping after 120 second from last ping \n");
			}
#else
			if ((u64CurrentTime - pingListTime) >= 300000) // 300 seconds in milliseconds
			{
				PostHttpReq = 1;
			}
			else
			{
				printf("Try ping after 300 second from last ping \n");
			}
#endif
		}
		else
		{
#if defined(B543) || defined(B542) || defined(B394) || defined(B527)
			if ((pingListTime - u64CurrentTime) >= 120000)	// 120 seconds in milliseconds
			{
				PostHttpReq = 1;
			}
			else
			{
				printf("Try ping after 120 second from last ping \n");
			}
#else
			if ((pingListTime - u64CurrentTime) >= 300000)	// 300 seconds in milliseconds
			{
				PostHttpReq = 1;
			}
			else
			{
				printf("Try ping after 300 second from last ping \n");
			}
#endif
		}
		
		if(PostHttpReq == 1)
		{
			pingListTime = get_current_time_ms();
		
			if(Post_Information_Que == NULL)
			{
				Post_Information_Que = xQueueCreateStatic(PB_Post_QUEUE_LENGTH, PUSH_POST_INFO_TASK_QUEUE_ITEMSIZE, Post_PB_Rx_QueueStorage, &Post_PB_Rx_QueueBuffer);
			}
			if(Post_Information_Handle == NULL)
			{
		
				{
					Post_Information_Handle = xTaskCreateStatic(
					                              Post_Information,                 // Task function
					                              "post information",            // Task name
					                              PB_MON_UPDATE_TASK_STACK_DEPTH,        // Stack size in words
					                              NULL,                    // Task parameters (not used here)
					                              PB_MON_UPDATE_TASK_PRIORITY,                       // Task priority
					                              PBpostTaskStack,              // Pointer to task stack (allocated in PSRAM)
					                              &xPBpostTaskBuffer             // Pointer to task control block
					                          );
					if (Post_Information_Handle == NULL)
					{
		#ifdef ENABLE_PRINT_MSG
						printf("Failed to create the Debug task");
		#endif
		
					}
				}
			}
		}
}

static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason)
{
	 // Read the raw register that stores the reset state
	    uint32_t reset_reason_raw = REG_READ(RTC_CNTL_RESET_STATE_REG);
	    unsigned int CPU0_Reset_Reason = 0;
	    unsigned int CPU1_Reset_Reason = 0;
	    CPU0_Reset_Reason = (unsigned int)(reset_reason_raw & 0x0000003F);
	    CPU1_Reset_Reason = (unsigned int)(reset_reason_raw & 0x00000FC0)>>6;
	    switch (CPU0_Reset_Reason) {
	        case ESP_RST_UNKNOWN:
	            strcpy(CPU0_RST_Reason,"Unknown");
	            break;
	        case ESP_RST_POWERON:
	        	strcpy(CPU0_RST_Reason,"Power-on");
	            break;
	        case ESP_RST_EXT:
	        	strcpy(CPU0_RST_Reason,"External reset");
	            break;
	        case ESP_RST_SW:
	        	strcpy(CPU0_RST_Reason,"Software reset");
	            break;
	        case ESP_RST_PANIC:
	        	strcpy(CPU0_RST_Reason,"Panic (exception)");
	            break;
	        case ESP_RST_INT_WDT:
	        	strcpy(CPU0_RST_Reason,"Interrupt Watchdog Timeout");
	            break;
	        case ESP_RST_TASK_WDT:
	        	strcpy(CPU0_RST_Reason,"Task Watchdog Timeout");
	            break;
	        case ESP_RST_WDT:
	        	strcpy(CPU0_RST_Reason,"Forced Watchdog");
	            break;
	        case ESP_RST_DEEPSLEEP:
	        	strcpy(CPU0_RST_Reason,"Wake from Deep Sleep");
	            break;
	        case ESP_RST_BROWNOUT:
	        	strcpy(CPU0_RST_Reason,"Brownout");
	            break;
	        case ESP_RST_SDIO:
	        	strcpy(CPU0_RST_Reason,"SDIO reset");
	            break;
	        default:
	        	strcpy(CPU0_RST_Reason,"Unhandled reason");
	            break;
	    }

	    switch (CPU1_Reset_Reason) {
	        case ESP_RST_UNKNOWN:
	        	strcpy(CPU1_RST_Reason,"Unknown");
	            break;
	        case ESP_RST_POWERON:
	        	strcpy(CPU1_RST_Reason,"Power-on");
	            break;
	        case ESP_RST_EXT:
	        	strcpy(CPU1_RST_Reason,"External reset");
	            break;
	        case ESP_RST_SW:
	        	strcpy(CPU1_RST_Reason,"Software reset");
	            break;
	        case ESP_RST_PANIC:
	        	strcpy(CPU1_RST_Reason,"Panic (exception)");
	            break;
	        case ESP_RST_INT_WDT:
	        	strcpy(CPU1_RST_Reason,"Interrupt Watchdog Timeout");
	            break;
	        case ESP_RST_TASK_WDT:
	        	strcpy(CPU1_RST_Reason,"Task Watchdog Timeout");
	            break;
	        case ESP_RST_WDT:
	        	strcpy(CPU1_RST_Reason,"Forced Watchdog");
	            break;
	        case ESP_RST_DEEPSLEEP:
	        	strcpy(CPU1_RST_Reason,"Wake from Deep Sleep");
	            break;
	        case ESP_RST_BROWNOUT:
	        	strcpy(CPU1_RST_Reason,"Brownout");
	            break;
	        case ESP_RST_SDIO:
	        	strcpy(CPU1_RST_Reason,"SDIO reset");
	            break;
	        default:
	        	strcpy(CPU1_RST_Reason,"Unhandled reason");
	            break;
	    }
}
