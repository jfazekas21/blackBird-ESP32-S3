/*
 * LED_Actor.c
 *
 *  Created on: Apr 29, 2022
 *      Author: shyam
 */

#include "actor.h"
#include "Config.h"
#include "LED_Actor.h"
#include "Console_Actor.h"
#include "math.h"
#include "NTP_Actor.h"
#include "driver/gptimer.h"

#define MAX_STATES 12
#define OBJ_QUE_COUNT_LED                	40    //30

#ifdef B480
#define Pulse_Out GPIO_NUM_0
#define Pulse_In GPIO_NUM_1
#else
#define Pulse_Out GPIO_NUM_NC
#define Pulse_In GPIO_NUM_NC
#endif

#if defined (B543) || defined(B542) || defined(B394)
#define LEDC_RED_OUTPUT_IO     	GPIO_NUM_2 // Define the output GPIO as power LED
#define LEDC_GREEN_OUTPUT_IO  	GPIO_NUM_1 // Define the output GPIO as Feedback LED
#define LEDC_BLUE_OUTPUT_IO   GPIO_NUM_3 // Define the output GPIO as WIFI LED
#elif defined(B527)
#define LEDC_RED_OUTPUT_IO     	GPIO_NUM_7 // Define the output GPIO as power LED
#define LEDC_GREEN_OUTPUT_IO  	GPIO_NUM_6 // Define the output GPIO as Feedback LED
#define LEDC_BLUE_OUTPUT_IO   GPIO_NUM_8 // Define the output GPIO as WIFI LED
#else
#define LEDC_RED_OUTPUT_IO     CLCK_SYNC // Define the output GPIO as power LED
#define LEDC_GREEN_OUTPUT_IO  	FB2 //25 // Define the output GPIO as Feedback LED
#define LEDC_BLUE_OUTPUT_IO     FB1// 26 // Define the output GPIO as WIFI LED
#define LEDC_FUSE_OUTPUT_IO   FUSE
#define LEDC_BLE_OUTPUT_IO   BLE_STAT
#endif

#if defined(B394) || defined(B543) || defined(B527) || defined(B542)
#define SlideSwitch GPIO_NUM_35
#else
#define SlideSwitch GPIO_NUM_4
#endif

#if defined(B394)
#define Input0 GPIO_NUM_15
#define Input1 GPIO_NUM_16
#define Input2 GPIO_NUM_17
#define Input3 GPIO_NUM_18
//#define Input4 GPIO_NUM_38
#define Input4 GPIO_NUM_21
#define Input5 GPIO_NUM_39
#define Input6 GPIO_NUM_47
#define Input7 GPIO_NUM_48
#endif

//#if defined(B527)
//#define LIGHT1 GPIO_NUM_4
//#define LIGHT2 GPIO_NUM_5
//#endif

static const char * THIS_ACTOR = "LED";
static const char 	THIS_ACTOR_ID 	= 	LED;  // assign src id


typedef struct
{
    float hue;        // Hue (0-360 degrees)
    float saturation; // Saturation (0-1)
    float brightness; // Brightness (0-1)
} Color;
BaseType_t ledMonitor, blinkMonitor, chaseMonitor;
TaskHandle_t ledHandle = NULL, UpdateBlinkStateHandle = NULL,SetStateHandle=NULL; //blinkHandle = NULL, chaseHandle = NULL,
QueueHandle_t led_Rx_Queue; //, led_Tx_Queue;
static StaticTask_t xLEDTaskBuffer, xLEDSubTaskBuffer,xSetStateReqTaskBuffer;  //// Declare a static task control block xChaseTaskBuffer, xBlinkTaskBuffer,
StackType_t *Chase_TaskStack = NULL, *Blink_TaskStack = NULL;
PSRAM_ATTR_BSS static StackType_t xTaskStack[LED_TASK_STACK_DEPTH], SetStateTaskStack[LED_SETSTATE_TASK_STACK_DEPTH];	//*xTaskStack1 = NULL,
PSRAM_ATTR_BSS static StackType_t xTaskStack1[LED_UPDATE_BLINKSTATE_STACK_DEPTH];
static AMessage_st s_Message_Tx; //s_Message_Rx,
static ledc_timer_config_t ledc_timer_RED, ledc_timer_GREEN, ledc_timer_BLUE,ledc_timer_BLE,ledc_timer_Fuse;//	Peripheral related Variables
static ledc_channel_config_t ledc_channel_RED, ledc_channel_GREEN, ledc_channel_BLUE,ledc_channel_BLE,ledc_channel_Fuse;

#if defined(B394)
TaskHandle_t CheckInputsHandle = NULL;
PSRAM_ATTR_BSS static StackType_t CheckInputxTaskStack[CHECK_INPUT_STACK_DEPTH];
static StaticTask_t CheckInputTaskBuffer;
QueueHandle_t CheckInputQueue;
PSRAM_ATTR_BSS static uint8_t CheckInput_gpio_evt_QueueStorage[CheckInput_gpio_evt_queue_LENGTH * CheckInput_gpio_evt_queue_ITEMSIZE];
static StaticQueue_t CheckInput_gpio_evt_queue_QueueBuffer;
#endif


static char BLE_LED_Status = BLE_LED_DISABLED;
//static char BLE_Up_Down = 0;
//static int BLE_LED_Duty = 0;
static uint8_t prev_blink_state = 0;
static uint8_t prev_blink_state_2 = 0;
static uint32_t LEDC_DUTY = 0;
uint8_t toggle_flag = 0;
uint8_t *blink_payload, *chase_payload;
static int FirstledEntry = 0;
static int cpu_load = 100;
QueueHandle_t SetStateReqToQueue = NULL;
PSRAM_ATTR_BSS static uint8_t ledRxQueueStorage [OBJ_QUE_COUNT_LED * sizeof(AMessage_st)], SetStateReqToQueueStorage [SETSTATEREQTOQUEUE_LENGTH * LED_SETSTATE_QUEUE_ITEMSIZE];
static StaticQueue_t SetStateReqToQueueBuffer, ledRxQueueBuffer;
static void init(void *a, void *b);  //	Initialized Actor to default Values
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);						//	Read a Parameter
static void help(AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
void update_blink_state(void *pvParameters __attribute__((unused)));
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
int ConvertStateNameToInt(const char *stateName);
static void handleRampAndFlash(int waitSeconds, uint64_t rampDuration, uint64_t flashDuration,int flashLEDwaitInterval) ;
void BLE_LED_RampCycle(uint64_t cycleDuration);
void BLE_LED_Fifty_Cycle(void);
void BLE_LED_RampDown(uint64_t duration, uint64_t elapsed);
void BLE_LED_RampUp(uint64_t duration, uint64_t elapsed);
static void chase_mode_withDirn(void *duty, void *delay,char dirn);
void LED_FlashThreeTimes(int flashDuration, int offDuration);
static void handleBlinkStates(int state,int duty);
static void chase_mode(void *duty, void *delay);	    //Method2
static void BLUE_LED_ON(int duty);
static void BLUE_LED_OFF();
static void RED_LED_ON(int duty);
static void RED_LED_OFF();
static void GREEN_LED_ON(int duty);
static void GREEN_LED_OFF();
static void BLE_LED_ON(int duty);
static void BLE_LED_OFF();
static void FUSE_LED_ON(int duty);
static void FUSE_LED_OFF();
static void RED_LED_init(void *a, void *b);
static void BLUE_LED_init(void *a, void *b);
static void BLE_LED_init(void *a, void *b);
static void FUSE_LED_init(void *a, void *b);
static void GREEN_LED_init(void *a, void *b);
static void control_led(int state);
static void SetLEDState(void *pvParameters __attribute__((unused)));
static uint64_t get_current_time_ms(void);
static void update_d3_led_based_on_cpu_idle(void);
static void Get_Property(AMessage_st* s_Message_Rx);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
#ifdef B394
static void Init_GPIOs(void);
static void Check_Input_states(void *pvParameters __attribute__((unused)));
static void IRAM_ATTR Check_Input_isr_handler(void* arg) ;
#endif

//void rgb_led_set_color(uint8_t r, uint8_t g, uint8_t b);
#ifdef B537
static void Init_Lights(void);
static void Test_Lights(void);
#endif

#if !defined(B480) && !defined(B553)
static uint16_t NewPattern_u16Hue[3];
static uint8_t PatternStartFlag = 0;
static void led_strip_hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b);
static void interpolate_color_led(Color *result, const Color *start, const Color *end, float t);
static void pattern_mode(void);
#endif

PSRAM_ATTR_BSS static struct Led_parameter {
	uint32_t duty;
	uint32_t duty_blink;
	uint32_t duty_glow;
	uint32_t delay_blink;
	uint32_t delay_chase;
	uint8_t blink_state;
	uint8_t act_blink_state;
	int64_t duration;
	uint8_t category[30];
} led_para;

PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];


PSRAM_ATTR static struct property prop[] = // Actor Property
{
    { &ledc_channel_RED.duty,    "RED_DUTY",    INT, "RW", "Duty cycle for red LED" },
    { &ledc_channel_GREEN.duty,  "GREEN_DUTY",  INT, "RW", "Duty cycle for green LED" },
    { &ledc_channel_BLUE.duty, "BLUE_DUTY", INT, "RW", "Duty cycle for BLUE LED" },
    { &led_para.duty,            "CHASE_DUTY",  INT, "RW", "Duty cycle for chase effect" },
    { &led_para.duty_blink,      "BLINK_DUTY",  INT, "RW",  "Duty cycle for blink effect" },
    { &led_para.duty_glow,       "GLOW_DUTY",   INT, "RW", "Duty cycle for glow effect" },
    { &led_para.delay_blink,     "DELAY_BLINK", INT, "RW", "Delay for blink effect" },
    { &led_para.delay_chase,     "DELAY_CHASE", INT, "RW", "Delay for chase effect" },
	{ &led_para.blink_state,     "BLINK_STATE", U_INT8, "RW", "Blink state" },
    { &led_para.act_blink_state, "ACTIVITY_BLINK_STATE", U_INT8, "RW", "Activity Blink state" },
	{ &led_para.duration,     "DURATION", U_INT64, "R", "Duration" },
    { &led_para.category,     "CATEGORY", STRING, "R", "category" }
};

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
	if (FirstledEntry == 0){
		LEDC_DUTY = (uint32_t) pow (2, LEDC_DUTY_RES)-1;
//		char* out;
		RED_LED_init(0, 0);  //Init RED Led
		BLUE_LED_init(0, 0);
		GREEN_LED_init(0, 0);
		RED_LED_OFF();
		BLUE_LED_OFF();
		GREEN_LED_OFF();
#if !defined(B543) && !defined(B527) && !defined(B542) && !defined(B394)
		FUSE_LED_init(0,0);
		BLE_LED_init(0,0);
		FUSE_LED_ON(1000);
		BLE_LED_OFF();
#endif
		if(led_Rx_Queue==NULL)
		{
			led_Rx_Queue = xQueueCreateStatic(OBJ_QUE_COUNT_LED, sizeof(AMessage_st), ledRxQueueStorage, &ledRxQueueBuffer);
			if (led_Rx_Queue == NULL) {
				printf("\n LED RX Queue is not created.\n");
			}
		}
		ledHandle = xTaskCreateStaticPinnedToCore(
				monitor,                 // Task function
				"LED Monitor",            // Task name
				LED_TASK_STACK_DEPTH,        // Stack size in words
				NULL,                    // Task parameters (not used here)
				LED_TASK_PRIORITY,                       // Task priority
				xTaskStack,              // Pointer to task stack (allocated in PSRAM)
				&xLEDTaskBuffer,           // Pointer to task control block
				0
			);
			if (ledHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
					printf("Failed to create task\n");
#endif
					// Handle error
				}
				UpdateBlinkStateHandle = xTaskCreateStaticPinnedToCore(
						update_blink_state,                 // Task function
						"UPDATE BLINK STATE",            // Task name
						LED_UPDATE_BLINKSTATE_STACK_DEPTH,        // Stack size in words
						NULL,                    // Task parameters (not used here)
						LED_UPDATE_BLINKSTATE_PRIORITY,                       // Task priority
						xTaskStack1,              // Pointer to task stack (allocated in PSRAM)
						&xLEDSubTaskBuffer,           // Pointer to task control block
						0
					);
			if (UpdateBlinkStateHandle == NULL) {
#ifdef ENABLE_PRINT_MSG
					printf("Failed to create task\n");
#endif
					// Handle error
				}

		if(SetStateReqToQueue==NULL)
		{
			SetStateReqToQueue = xQueueCreateStatic(SETSTATEREQTOQUEUE_LENGTH, LED_SETSTATE_QUEUE_ITEMSIZE, SetStateReqToQueueStorage, &SetStateReqToQueueBuffer);
		}
		s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;  //set ID of src actor

		/* B394: digital inputs owned by B394_DI actor; LED does not init GPIO (Init_GPIOs only called when B394 was defined). */
#if defined(B394)
		/* Do not call Init_GPIOs(); B394_DI actor sends DI events. */
#endif
		FirstledEntry = 1;
	}
}

// Define the period in milliseconds (100 ms in this example)
#define PERIOD_MS 20

// Timing-based GPIO control (safe for idle hook)
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
static void set_out_pin()
{
	int64_t now_ms = get_current_time_ms();
    if (((now_ms % 10000) < 60) ||       // Every 10 sec: 60 ms LOW
        ((now_ms % 60000) < 250) ||      // Every 60 sec: 250 ms LOW
        ((now_ms % 300000) < 750))       // Every 5 min: 750 ms LOW
    {
        gpio_set_level(Pulse_Out, 0);   // Set LOW
    }
    else
    {
        gpio_set_level(Pulse_Out, 1);   // Set HIGH
    }
}
#endif

void update_blink_state(void *pvParameters __attribute__((unused)))
{
    char count = 0;
    static uint32_t DebounceGPIOState=0;
#if !defined(B542) && !defined(B527)  && !defined(B543) && !defined(B394)
	gpio_set_direction(Pulse_In, GPIO_MODE_INPUT);
	gpio_set_direction(Pulse_Out, GPIO_MODE_OUTPUT);
	gpio_set_level(Pulse_Out, 1);   // Set HIGH
	gpio_set_direction(SlideSwitch, GPIO_MODE_INPUT);
	gpio_set_pull_mode(SlideSwitch, GPIO_PULLDOWN_ONLY);
#endif

	while (1)
	{
		control_led(led_para.blink_state);
    	if ((BLE_LED_Status == BLE_LED_ADVERT_NOT_CONNECTED))
    	{

    		if(count >= 5)
    		{
    			BLE_LED_RampCycle(4000);
    			count = 0;
    		}
    		else
    		{
    			count++;
    		}
    	}
    	else if ((BLE_LED_Status == BLE_LED_CONNECTED))
    	{
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
    		BLUE_LED_ON(1000);
#else
    		BLE_LED_ON(1000);
#endif
			count = 0;
    	}
    	if(BLE_LED_Status == BLE_LED_DISABLED)
    	{
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
//    		BLUE_LED_ON(0);
#else
			BLE_LED_ON(0);
#endif
			count = 0;
    	}
#if !defined(B543) && !defined(B527) && !defined(B542) && !defined(B394)
		DebounceGPIOState = (DebounceGPIOState<<1)| gpio_get_level(SlideSwitch);
		if(DebounceGPIOState)
		{
			set_out_pin();
		}
#endif
		vTaskDelay(20 / portTICK_PERIOD_MS);  //100
	}
}

static void monitor(void *pvParameters __attribute__((unused))) {

	cJSON *name_JSON;
	cJSON *head_JSON = NULL;
	char str[100] ={0};
	uint8_t u8Result =0;

	while (1) {
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(led_Rx_Queue, (void*) (s_Message_Rx),portMAX_DELAY))  //5
		{
//			printf("LED msg_Rx_Queue S = %s, D = %s, C = %s \n", s_Message_Rx->src_Actor_a8,	s_Message_Rx->dest_Actor_a8, s_Message_Rx->cmdFun_a8);
//			if(s_Message_Rx->payload_p8 != NULL)
//			{
//				if(strlen((char*)s_Message_Rx->payload_p8) != 0)
//					printf("LED DT = %s\n",s_Message_Rx->payload_p8);
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

				if (FirstledEntry == 0)
					init(0,0);
				else if(FirstledEntry==1){
					Add_Response_msg("LED initialization is done.", s_Message_Rx);
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
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESTORE_STATE"))
			{
				led_para.duration = get_current_time_ms();
				if(prev_blink_state_2 != 0)
				{
					prev_blink_state = prev_blink_state_2;
					prev_blink_state_2 = 0;
				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "STOP_BLE_LED"))
			{
				BLE_LED_Status = BLE_LED_DISABLED;   // reset BLE mode parameters
#if !defined(B542) && !defined(B527)  && !defined(B543)  && !defined(B394)
				BLE_LED_ON(0);
#endif
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "BLE_CONNECTED"))
			{
				BLE_LED_Status = BLE_LED_CONNECTED;   // reset BLE mode parameters
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "BLE_DISCONNECTED"))
			{
				BLE_LED_Status = BLE_LED_ADVERT_NOT_CONNECTED;   // reset BLE mode parameters
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "SETSTATE"))
			{
				 char payload [LED_SETSTATE_QUEUE_ITEMSIZE]= {0};
				 {
					strcpy(payload, (char*)s_Message_Rx->payload_p8);
					if(SetStateReqToQueue==NULL)
					{
						SetStateReqToQueue = xQueueCreateStatic(SETSTATEREQTOQUEUE_LENGTH, LED_SETSTATE_QUEUE_ITEMSIZE, SetStateReqToQueueStorage, &SetStateReqToQueueBuffer);	// Create a queue that can hold
						if(SetStateReqToQueue==NULL)
						{
							Add_Response_msg("Queue for SetState command is not created.", s_Message_Rx);
							return;
						}
					}
					if(SetStateHandle == NULL)
					{
						SetStateHandle = xTaskCreateStaticPinnedToCore(
													SetLEDState,                 // Task function
													"SetLEDState",            // Task name
													LED_SETSTATE_TASK_STACK_DEPTH,        // Stack size in words
													Rx_buffer,                    // Task parameters (not used here)
													LED_SETSTATE_TASK_PRIORITY,                       // Task priority
													SetStateTaskStack,              // Pointer to task stack (allocated in PSRAM)
													&xSetStateReqTaskBuffer,             // Pointer to task control block
													0
													);
						if (SetStateHandle == NULL) {
			#ifdef ENABLE_PRINT_MSG
								printf("Failed to create SetLEDState task\n");
			#endif
								Add_Response_msg("Failed to create SetLEDState task", s_Message_Rx);
								continue;
							}
					}

					if(SetStateReqToQueue!=NULL)
					{
						if (xQueueSend(SetStateReqToQueue, payload, portMAX_DELAY) != pdPASS) {
							Add_Response_msg("Failed to queue SetState payload.", s_Message_Rx);
						}
					}
				 }
			}
			else
			{
				  Add_Response_msg("Invalid method", s_Message_Rx);
			}
		}
	}
}

void LED_ConsoleWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message;
	s_Message = (AMessage_st*)msg;
	if (FirstledEntry == 0)
	init(0,0);
	uint8_t state = xQueueSend(led_Rx_Queue, s_Message, QUE_DELAY);

	if (state != pdTRUE)
	{
		if(s_Message->payload_p8 != NULL)
		{
			free(s_Message->payload_p8);
			s_Message->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<LED.ERROR(LED RX Queue is full)\n");
		}
		else
		{
			printf("<LED.ERROR(LED RX Queue send unsuccessful)\n");
		}
	}
}//	LED_ConsolWriteToActor

static void chase_mode(void *duty, void *delay) {		//method2
	int u32duty = 0;
	int  u32delay = 0;
	float fduty = 0;

	u32duty = *((int*) duty);
	u32delay = *((int*) delay);

	if(u32duty > 100)
		u32duty = 100;

	fduty = ((float)((pow (2, LEDC_DUTY_RES))-1)) * ((float)((float)u32duty/(float)100.0));
	u32duty = ((int) fduty);

#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
	RED_LED_ON(u32duty);
	GREEN_LED_ON(u32duty);
	BLUE_LED_ON(u32duty);
	vTaskDelay(u32delay / portTICK_PERIOD_MS);
	RED_LED_OFF();
	GREEN_LED_OFF();
	BLUE_LED_OFF();
	vTaskDelay(u32delay / portTICK_PERIOD_MS);
#else
	RED_LED_ON(u32duty);
	GREEN_LED_OFF();
	vTaskDelay(u32delay / portTICK_PERIOD_MS);
	RED_LED_OFF();
	GREEN_LED_ON(u32duty);
	vTaskDelay(u32delay / portTICK_PERIOD_MS);
#endif
}

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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the LED actor.");
	cJSON_AddStringToObject(responseObject, "SET(string RED_DUTY)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(json Property_Names)", "Get the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	cJSON_AddStringToObject(responseObject, "SETSTATE(string stateName, INT duration, string category)", "Set blink sate along with duration in seconds.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);
	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

static void RED_LED_init(void *a, void *b) {
	ledc_timer_RED.speed_mode = LEDC_MODE;
	ledc_timer_RED.timer_num = LEDC_TIMER;
	ledc_timer_RED.duty_resolution = LEDC_DUTY_RES;
	ledc_timer_RED.freq_hz = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
	ledc_timer_RED.clk_cfg = LEDC_AUTO_CLK;
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_RED));

	ledc_channel_RED.speed_mode = LEDC_MODE;
	ledc_channel_RED.timer_sel = LEDC_TIMER;
	ledc_channel_RED.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_RED.channel = LEDC_CHANNEL_RED;
	ledc_channel_RED.duty = LEDC_DUTY; // Set duty to 0%
	ledc_channel_RED.gpio_num = LEDC_RED_OUTPUT_IO;
	ledc_channel_RED.hpoint = 0;
	led_para.blink_state = 0;
	led_para.duration = 0;
	strcpy((char*)&led_para.category, "none");
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_RED));
}

static void GREEN_LED_init(void *a, void *b) {
	ledc_timer_GREEN.speed_mode = LEDC_MODE;
	ledc_timer_GREEN.timer_num = LEDC_TIMER;
	ledc_timer_GREEN.duty_resolution = LEDC_DUTY_RES;
	ledc_timer_GREEN.freq_hz = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
	ledc_timer_GREEN.clk_cfg = LEDC_AUTO_CLK;
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_GREEN));

	ledc_channel_GREEN.speed_mode = LEDC_MODE;
	ledc_channel_GREEN.timer_sel = LEDC_TIMER;
	ledc_channel_GREEN.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_GREEN.channel = LEDC_CHANNEL_GREEN;
	ledc_channel_GREEN.duty = LEDC_DUTY; // Set duty to 0%
	ledc_channel_GREEN.gpio_num = LEDC_GREEN_OUTPUT_IO;
	ledc_channel_GREEN.hpoint = 0;
//	led_para.act_blink_state = 0;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_GREEN));
}

static void BLUE_LED_init(void *a, void *b) {
	ledc_timer_BLUE.speed_mode = LEDC_MODE;
	ledc_timer_BLUE.timer_num = LEDC_TIMER;
	ledc_timer_BLUE.duty_resolution = LEDC_DUTY_RES;
	ledc_timer_BLUE.freq_hz = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
	ledc_timer_BLUE.clk_cfg = LEDC_AUTO_CLK;
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_BLUE));

	ledc_channel_BLUE.speed_mode = LEDC_MODE;
	ledc_channel_BLUE.timer_sel = LEDC_TIMER;
	ledc_channel_BLUE.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_BLUE.channel = LEDC_CHANNEL_BLUE;
	ledc_channel_BLUE.duty = LEDC_DUTY; // Set duty to 0%
	ledc_channel_BLUE.gpio_num = LEDC_BLUE_OUTPUT_IO;
	ledc_channel_BLUE.hpoint = 0;

	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_BLUE));
}

static void BLE_LED_init(void *a, void *b) {
	ledc_timer_BLE.speed_mode = LEDC_MODE;
	ledc_timer_BLE.timer_num = LEDC_TIMER;
	ledc_timer_BLE.duty_resolution = LEDC_DUTY_RES;
	ledc_timer_BLE.freq_hz = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
	ledc_timer_BLE.clk_cfg = LEDC_AUTO_CLK;
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_BLE));

	ledc_channel_BLE.speed_mode = LEDC_MODE;
	ledc_channel_BLE.timer_sel = LEDC_TIMER;
	ledc_channel_BLE.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_BLE.channel = LEDC_CHANNEL_BLE;
	ledc_channel_BLE.duty = LEDC_DUTY; // Set duty to 0%
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
	ledc_channel_BLE.gpio_num = LEDC_BLE_OUTPUT_IO;
#endif
	ledc_channel_BLE.hpoint = 0;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_BLE));
}
static void FUSE_LED_init(void *a, void *b) {
	ledc_timer_Fuse.speed_mode = LEDC_MODE;
	ledc_timer_Fuse.timer_num = LEDC_TIMER;
	ledc_timer_Fuse.duty_resolution = LEDC_DUTY_RES;
	ledc_timer_Fuse.freq_hz = LEDC_FREQUENCY;  // Set output frequency at 5 kHz
	ledc_timer_Fuse.clk_cfg = LEDC_AUTO_CLK;
	ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer_Fuse));

	ledc_channel_Fuse.speed_mode = LEDC_MODE;
	ledc_channel_Fuse.timer_sel = LEDC_TIMER;
	ledc_channel_Fuse.intr_type = LEDC_INTR_DISABLE;
	ledc_channel_Fuse.channel = LEDC_CHANNEL_FUSE;
	ledc_channel_Fuse.duty = LEDC_DUTY; // Set duty to 0%
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
	ledc_channel_Fuse.gpio_num = LEDC_FUSE_OUTPUT_IO;
#endif
	ledc_channel_Fuse.hpoint = 0;
	ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel_Fuse));
}
static void BLUE_LED_ON(int duty) {
	ledc_channel_BLUE.gpio_num = LEDC_BLUE_OUTPUT_IO;
	ledc_channel_BLUE.duty = duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_BLUE.channel, ledc_channel_BLUE.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_BLUE.channel));
}

static void BLUE_LED_OFF() {
	int u32duty = 0;
	ledc_channel_BLUE.gpio_num = LEDC_BLUE_OUTPUT_IO;
	ledc_channel_BLUE.duty = u32duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_BLUE.channel, ledc_channel_BLUE.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_BLUE.channel));
}

static void RED_LED_ON(int duty) {
	ledc_channel_RED.gpio_num = LEDC_RED_OUTPUT_IO;
	ledc_channel_RED.duty = duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_RED.channel, ledc_channel_RED.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_RED.channel));
}

static void RED_LED_OFF() {
	int u32duty = 0;
	ledc_channel_RED.gpio_num = LEDC_RED_OUTPUT_IO;
	ledc_channel_RED.duty = u32duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_RED.channel, ledc_channel_RED.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_RED.channel));
}

static void GREEN_LED_ON(int duty) {
	ledc_channel_GREEN.gpio_num = LEDC_GREEN_OUTPUT_IO;
	ledc_channel_GREEN.duty = duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_GREEN.channel, ledc_channel_GREEN.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_GREEN.channel));
}

static void GREEN_LED_OFF() {
	int u32duty = 0;
	ledc_channel_GREEN.gpio_num = LEDC_GREEN_OUTPUT_IO;
	ledc_channel_GREEN.duty = u32duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_GREEN.channel, ledc_channel_GREEN.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_GREEN.channel));
}

#if !defined(B542) && !defined(B527) && !defined(B543)  && !defined(B394)
static void BLE_LED_ON(int duty) {
	ledc_channel_BLE.gpio_num = LEDC_BLE_OUTPUT_IO;
	ledc_channel_BLE.duty = duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_BLE.channel, ledc_channel_BLE.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_BLE.channel));
}

static void BLE_LED_OFF() {
	int u32duty = 0;
	ledc_channel_BLE.gpio_num = LEDC_BLE_OUTPUT_IO;
	ledc_channel_BLE.duty = u32duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_BLE.channel, ledc_channel_BLE.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_BLE.channel));
}
static void FUSE_LED_ON(int duty) {
	ledc_channel_Fuse.gpio_num = LEDC_FUSE_OUTPUT_IO;
	ledc_channel_Fuse.duty = duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_Fuse.channel, ledc_channel_Fuse.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_Fuse.channel));
}

static void FUSE_LED_OFF() {
	int u32duty = 0;
	ledc_channel_Fuse.gpio_num = LEDC_FUSE_OUTPUT_IO;
	ledc_channel_Fuse.duty = u32duty;
	ESP_ERROR_CHECK(
			ledc_set_duty(LEDC_MODE, ledc_channel_Fuse.channel, ledc_channel_Fuse.duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, ledc_channel_Fuse.channel));
}
#endif

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

static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n LED s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
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

	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"IDLE")==0)
	{
		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
		if (in_JSON == NULL) {
			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
			Add_Response_msg(str,s_Message_Rx);
			return;
		}
		cJSON *commandKey = cJSON_GetObjectItem(in_JSON, "COMMAND");

		if (cJSON_IsString(commandKey) && (strcmp(commandKey->valuestring,"GET")==0))
		{
			cJSON *responseKey = cJSON_GetObjectItem(in_JSON, "RESPONSE");

			if (responseKey != NULL && cJSON_IsString(responseKey->child))
			{
				cJSON *cpuLoadItem = cJSON_GetObjectItem(responseKey, "CPU_LOAD");
				if (cpuLoadItem != NULL && cJSON_IsString(cpuLoadItem))
				{
					cpu_load = 100 - (atoi(cpuLoadItem->valuestring));
				}
			}
		}
		cJSON_Delete(in_JSON);
		return;
	}
	return;
}

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

/**
 * @brief Control the LED based on the given blink state and current time.
 *
 * @param state Current blink state.
 *
 * Uses esp_timer_get_time to get the current time and determines
 * whether the LED should be on or off.
 */
static void control_led(int state) {
    // Get the current time in microseconds
    int led_on = 0;
    int duty =0;
    // Convert the time to milliseconds and get the remainder of division by 6000
    // This gives the time within a 6-second cycle (0 to 5999 ms)
    uint64_t current_time_ms = (get_current_time_ms()) % 6000;
    // Determine the number of 300ms on-off cycles based on the blink state
    int num_blinks = 0;
    if (state >= 1 && state <= MAX_STATES) {
        num_blinks = state;
    } else if (state == 11) {
        num_blinks = 20;  // Special case: continuous on state
    } // If state is 0, num_blinks remains 0 (LED off)

    // Calculate the current blink period (each period is 600ms)
    int period = current_time_ms / 600;
    // Calculate the position within the current period
    int position = current_time_ms % 600;

    if (state == LED_BLINK_STATE_FULLON) {
          // Special case: continuous on state
          led_on = 1;
          duty=1000;
      } else {
          // General case
          led_on = (period < num_blinks) && (position < 300);
          if(led_on==1)
        	  duty = 1000;
          else
        	  duty = 0;
      }
    	handleBlinkStates(state,duty);
    	if(((get_current_time_ms())>led_para.duration) &&
    			(led_para.duration > 0) &&
				(prev_blink_state != led_para.blink_state) && (led_para.blink_state != LED_BLINK_STATE_FULLON ))
		{
    		RED_LED_ON(0);
    		GREEN_LED_ON(0);
    		BLUE_LED_ON(0);

#if !defined(B542) && !defined(B527) && !defined(B543)  && !defined(B394)
			FUSE_LED_ON(0);
			BLE_LED_ON(0);
#endif
			led_para.blink_state  =  prev_blink_state;
		}
}

static void SetLEDState(void *pvParameters __attribute__((unused)))
{
	char StateNamestr[30] = {0}; // payLoadData_LED[100] = {0};
	int duration = 0;
	AMessage_st* s_Message_Rx_data = (AMessage_st*)pvParameters;
	AMessage_st s_Message_Rx1;
	AMessage_st *s_Message_Rx = &s_Message_Rx1;
	char data_buffer[200];
	memset(data_buffer,0,sizeof(data_buffer));
	memcpy(s_Message_Rx, s_Message_Rx_data, sizeof(AMessage_st));
	if((char*)s_Message_Rx->payload_p8 != NULL)
	{
		strncpy(data_buffer, (char*)s_Message_Rx_data->payload_p8,199);
	}
	s_Message_Rx->payload_p8 = (uint8_t*)data_buffer;
	strcpy((char*)s_Message_Rx->cmdFun_a8, "SETSTATE");
	strcpy((char*)s_Message_Rx->dest_Actor_a8, "LED");
	strcpy((char*)s_Message_Rx->src_Actor_a8, "SYSTEM");

	char payload [LED_SETSTATE_QUEUE_ITEMSIZE]= {0}; //heap_caps_calloc(LED_SETSTATE_QUEUE_ITEMSIZE,sizeof(char),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
	while(1)
	{
		if (pdTRUE == xQueueReceive(SetStateReqToQueue, (void*)payload, portMAX_DELAY))
			{
		        cJSON *in_JSON 	= cJSON_Parse(payload);
		       	if (in_JSON == NULL)
		       	{
					#ifdef ENABLE_PRINT_MSG
									printf("\n\nInvalid Json input\n\n");
					#endif
		       		goto exit;
		       	}
		       	else
		       	{
					cJSON *stateName = cJSON_GetObjectItem(in_JSON, "stateName");
					if((stateName != NULL) && (cJSON_IsString(stateName)))
					{
						memset(StateNamestr,0,sizeof(StateNamestr));
						strcpy((char*)StateNamestr, stateName->valuestring);
						int tempblinkstate=0;
						tempblinkstate=ConvertStateNameToInt(StateNamestr);
						if(tempblinkstate!=LED_STATE_BLE_MODE)
						{
							if( (prev_blink_state != LED_STATE_UPLOAD_MODE) && (prev_blink_state != LED_STATE_DOWNLOAD_MODE) && (prev_blink_state != LED_STATE_THREE_FLASH) && (prev_blink_state != LED_STATE_SOLID_MESSAGE) && (prev_blink_state != LED_STATE_REBOOT_MODE) && (prev_blink_state != LED_STATE_NET_CONNECTED) && (prev_blink_state != LED_BLINK_STATE1))
							{
								prev_blink_state_2 = prev_blink_state;
							}
							if(led_para.blink_state != LED_STATE_SOLID_MESSAGE)
							{
								prev_blink_state = led_para.blink_state;
							}
							led_para.blink_state = ConvertStateNameToInt(StateNamestr);
						}
						else
						{
							BLE_LED_Status = BLE_LED_ADVERT_NOT_CONNECTED;
						}
					}

					cJSON *durationItem = cJSON_GetObjectItem(in_JSON, "duration");
					if ((durationItem != NULL) && (cJSON_IsNumber(durationItem)))
					{
						duration = durationItem->valueint;
						if(duration>0)
							led_para.duration = (duration*1000) + (get_current_time_ms());
						else
							led_para.duration = -1;
					}

					cJSON *categoryItem = cJSON_GetObjectItem(in_JSON, "category");
					if ((categoryItem != NULL) && (cJSON_IsString(categoryItem)))
					{
						strcpy((char*)&led_para.category, categoryItem->valuestring);
					}
					else
					{
						strcpy((char*)&led_para.category, "none");
					}
                    cJSON *rootNew_JSON = cJSON_CreateObject();
                    cJSON_AddStringToObject(rootNew_JSON, "stateName", StateNamestr);
                    cJSON_AddNumberToObject(rootNew_JSON, "duration", duration);
                    cJSON_AddStringToObject(rootNew_JSON, "category", (char*)&led_para.category);

                    s_Message_Rx->payload_p8[0] = '\0';
					cJSON_PrintPreallocated(rootNew_JSON, (char*)s_Message_Rx->payload_p8, 200, false);
                    cJSON_Delete(rootNew_JSON);
                    console_send_responce_to_console_xface(s_Message_Rx);
		       	}
		       	cJSON_Delete(in_JSON);
			}
	}
	exit:
			SetStateHandle = NULL;
			vTaskDelete(SetStateHandle);  // Delete the task
}

int ConvertStateNameToInt(const char *stateName)
{
    if (strcmp(stateName, "OFF") == 0) return LED_BLINK_STATE_OFF;
    else if (strcmp(stateName, "1-blink") == 0) return LED_BLINK_STATE1;
    else if (strcmp(stateName, "2-blink") == 0) return LED_BLINK_STATE2;
    else if (strcmp(stateName, "3-blink") == 0) return LED_BLINK_STATE3;
    else if (strcmp(stateName, "4-blink") == 0) return LED_BLINK_STATE4;
    else if (strcmp(stateName, "5-blink") == 0) return LED_BLINK_STATE5;
    else if (strcmp(stateName, "6-blink") == 0) return LED_BLINK_STATE6;
    else if (strcmp(stateName, "7-blink") == 0) return LED_BLINK_STATE7;
    else if (strcmp(stateName, "8-blink") == 0) return LED_BLINK_STATE8;
    else if (strcmp(stateName, "9-blink") == 0) return LED_BLINK_STATE9;
    else if (strcmp(stateName, "10-blink") == 0) return LED_BLINK_STATE10;
    else if (strcmp(stateName, "SOLID") == 0) return LED_BLINK_STATE_FULLON;
    else if (strcmp(stateName, "CHASE") == 0) return LED_BLINK_STATE_CHASE_MODE;
    else if (strcmp(stateName, "BLE Mode") == 0) return LED_STATE_BLE_MODE;
    else if (strcmp(stateName, "Upload") == 0) return LED_STATE_UPLOAD_MODE;
    else if (strcmp(stateName, "Download") == 0) return LED_STATE_DOWNLOAD_MODE;
    else if (strcmp(stateName, "Hold Warning") == 0) return LED_STATE_HOLD_WARNING;
    else if (strcmp(stateName, "3 Flash") == 0) return LED_STATE_THREE_FLASH;
    else if (strcmp(stateName, "SOLID Message") == 0) return LED_STATE_SOLID_MESSAGE;
    else if (strcmp(stateName, "REBOOT_MODE") == 0) return LED_STATE_REBOOT_MODE;
    else if (strcmp(stateName, "NET CONNECTED") == 0) return LED_STATE_NET_CONNECTED;
    else return -1; // Return -1 if the state name does not match any known states
}

//#if !defined(B542) && !defined(B527) && !defined(B543)
static void handleRampAndFlash(int waitSeconds, uint64_t rampDuration, uint64_t flashDuration,int flashLEDwaitInterval) {
    GREEN_LED_ON(0);
    RED_LED_ON(0);
    BLUE_LED_ON(0);
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
    BLE_LED_ON(0);
    FUSE_LED_ON(0);
#endif
    vTaskDelay(100 / portTICK_PERIOD_MS);
    // Ramp up brightness from 0% to 100% over 3 seconds
    uint64_t startTime = get_current_time_ms();
    while (get_current_time_ms() - startTime < rampDuration) {
    	uint64_t elapsedTime = get_current_time_ms() - startTime;
        int duty = (elapsedTime * 500) / rampDuration; // Linear ramp from 0 to 500
        GREEN_LED_ON(duty);
        RED_LED_ON(duty);
        BLUE_LED_ON(duty);
#if !defined(B542) && !defined(B527) && !defined(B543)  && !defined(B394)
        BLE_LED_ON(duty);
        FUSE_LED_ON(duty);
#endif
        vTaskDelay(10 / portTICK_PERIOD_MS); // Small delay for smooth transition
    }

    // Flash LEDs with 250ms period from 5 to 5.6 seconds
    startTime = get_current_time_ms();
    bool ledState = false;
    while (get_current_time_ms() - startTime < flashDuration) {
        ledState = !ledState;
        GREEN_LED_ON(ledState ? 1000 : 0); // Toggle LED on/off
        RED_LED_ON(ledState ? 1000 : 0); // Toggle LED on/off
        BLUE_LED_ON(ledState ? 1000 : 0);
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
        BLE_LED_ON(ledState ? 1000 : 0);
        FUSE_LED_ON(ledState ? 1000 : 0);
#endif
        vTaskDelay(flashLEDwaitInterval / portTICK_PERIOD_MS); // 100ms period
    }

    // Final state to indicate the user should release the button
    GREEN_LED_ON(0);
    RED_LED_ON(0);
    BLUE_LED_ON(0);
#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
    BLE_LED_ON(0);
    FUSE_LED_ON(0);
#endif
}

void BLE_LED_Fifty_Cycle(void)
{
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
    GREEN_LED_ON(0);
    BLUE_LED_ON(0);
    RED_LED_ON(500);
#else
    BLE_LED_ON(500);
#endif
}

#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
static void chase_mode_withDirn(void *duty, void *delay,char dirn) {		//method2
	int u32duty = 0;
		int  u32delay = 0; //u32freq = 0,
		float fduty = 0;

		u32duty = *((int*) duty);
		u32delay = *((int*) delay);

		if(u32duty > 100)
			u32duty = 100;

		fduty = ((float)((pow (2, LEDC_DUTY_RES))-1)) * ((float)((float)u32duty/(float)100.0));
		u32duty = ((int) fduty);
		if(dirn=='F')
		{
			RED_LED_ON(u32duty);
			GREEN_LED_OFF();
			BLUE_LED_OFF();
			FUSE_LED_OFF();
			BLE_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			RED_LED_OFF();
			GREEN_LED_ON(u32duty);
			BLUE_LED_OFF();
			FUSE_LED_OFF();
			BLE_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			RED_LED_OFF();
			GREEN_LED_OFF();
			BLUE_LED_ON(u32duty);
			FUSE_LED_OFF();
			BLE_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

#if !defined(B542) && !defined(B527) && !defined(B543) && !defined(B394)
			RED_LED_OFF();
			GREEN_LED_OFF();
			BLUE_LED_OFF();
			FUSE_LED_ON(u32duty);
			BLE_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			RED_LED_OFF();
			GREEN_LED_OFF();
			BLUE_LED_OFF();
			FUSE_LED_OFF();
			BLE_LED_ON(u32duty);
			vTaskDelay(u32delay / portTICK_PERIOD_MS);
#endif
		}
		else if(dirn=='B')
		{
#if !defined(B542) && !defined(B527) && !defined(B543)
			BLE_LED_ON(u32duty);
			FUSE_LED_OFF();
			BLUE_LED_OFF();
			GREEN_LED_OFF();
			RED_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			BLE_LED_OFF();
			FUSE_LED_ON(u32duty);
			BLUE_LED_OFF();
			GREEN_LED_OFF();
			RED_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);
#endif
			BLE_LED_OFF();
			FUSE_LED_OFF();
			BLUE_LED_ON(u32duty);
			GREEN_LED_OFF();
			RED_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			BLE_LED_OFF();
			FUSE_LED_OFF();
			BLUE_LED_OFF();
			GREEN_LED_ON(u32duty);
			RED_LED_OFF();
			vTaskDelay(u32delay / portTICK_PERIOD_MS);

			BLE_LED_OFF();
			FUSE_LED_OFF();
			BLUE_LED_OFF();
			GREEN_LED_OFF();
			RED_LED_ON(u32duty);
			vTaskDelay(u32delay / portTICK_PERIOD_MS);
		}
}

void LED_FlashThreeTimes(int flashDuration, int offDuration) {
    for (int i = 0; i < 3; i++) {
		GREEN_LED_ON(1000);
		RED_LED_ON(1000);
		BLUE_LED_ON(1000);
		BLE_LED_ON(1000);
		FUSE_LED_ON(1000);

        vTaskDelay(flashDuration / portTICK_PERIOD_MS); // Wait for the flash duration

        GREEN_LED_ON(0);
        RED_LED_ON(0);
        BLUE_LED_ON(0);
        BLE_LED_ON(0);
        FUSE_LED_ON(0);
        vTaskDelay(offDuration / portTICK_PERIOD_MS); // Wait for the off duration
    }

    // Ensure the LED remains off after the three flashes
    GREEN_LED_ON(0);
    RED_LED_ON(0);
    BLUE_LED_ON(0);
    BLE_LED_ON(0);
    FUSE_LED_ON(0);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
}
#endif

static void handleBlinkStates(int state, int duty) {
    uint32_t l_dutyVal = 1000;
    uint32_t l_delay_chase = 300;

    switch (state) {
        case LED_BLINK_STATE_CHASE_MODE:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
            RED_LED_ON(0);
            chase_mode(&l_dutyVal, &l_delay_chase);
            vTaskDelay(l_delay_chase / portTICK_PERIOD_MS);
#else
            RED_LED_ON(0);
            chase_mode(&l_dutyVal, &l_delay_chase);
            vTaskDelay(l_delay_chase / portTICK_PERIOD_MS);
#endif
            break;

        case LED_STATE_BLE_MODE:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
#else
        	update_d3_led_based_on_cpu_idle();
#endif
            break;

        case LED_STATE_UPLOAD_MODE:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
pattern_mode();
#else
            l_delay_chase = 400;
            chase_mode_withDirn(&l_dutyVal, &l_delay_chase, 'F');
#endif
            break;

        case LED_STATE_DOWNLOAD_MODE:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)

pattern_mode();
#else
            l_delay_chase = 400;
            chase_mode_withDirn(&l_dutyVal, &l_delay_chase, 'B');
#endif
            break;

        case LED_STATE_HOLD_WARNING:
        	handleRampAndFlash(3000, 3000, 1000, 250);
            break;

        case LED_STATE_THREE_FLASH:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
//LED_FlashThreeTimes(500, 500);
#else
            LED_FlashThreeTimes(500, 500);
#endif
            break;

        case LED_STATE_REBOOT_MODE:
//        	update_d3_led_based_on_cpu_idle();
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	BLE_LED_Fifty_Cycle();
#else
            BLE_LED_Fifty_Cycle();
#endif
            break;

        case LED_STATE_NET_CONNECTED:
//        	update_d3_led_based_on_cpu_idle();
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
//            GREEN_LED_ON(0);
//            BLUE_LED_ON(1000);
//            RED_LED_ON(0);
#else
            GREEN_LED_ON(0);
            BLUE_LED_ON(1000);
            RED_LED_ON(0);
#endif
            break;

        case LED_STATE_SOLID_MESSAGE:
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
            GREEN_LED_ON(1000);
            BLUE_LED_ON(1000);
            RED_LED_ON(1000);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            GREEN_LED_OFF();
            BLUE_LED_OFF();
            RED_LED_OFF();
            led_para.blink_state = prev_blink_state;
#else
        	update_d3_led_based_on_cpu_idle();
            GREEN_LED_ON(0);
            BLUE_LED_ON(1000);
            RED_LED_ON(0);
            vTaskDelay(200 / portTICK_PERIOD_MS);
            GREEN_LED_ON(1000);
            RED_LED_ON(1000);
            led_para.blink_state = prev_blink_state;
#endif
            break;
        case LED_BLINK_STATE_OFF:
        	BLUE_LED_ON(0);
			GREEN_LED_ON(0);
			RED_LED_ON(0);
        	break;
        case LED_BLINK_STATE1:
//        	D4,5 are OFF
//        	D6 in a 1-blink loop

#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(duty);
			GREEN_LED_ON(0);
			RED_LED_ON(0);
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(0);
			GREEN_LED_ON(0);
			RED_LED_ON(duty);
#endif

			break;
        case LED_BLINK_STATE2:
//        	D4 in a 2-blink loop
//        	D5,6 are OFF
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(0);
        	RED_LED_ON(duty);
			GREEN_LED_ON(0);
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(duty);
        	RED_LED_ON(0);
			GREEN_LED_ON(0);
#endif
			break;
        case LED_BLINK_STATE3:
//        	D4 in a 3-blink loop
//        	D5,6 are OFF
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(0);
        	RED_LED_ON(duty);
			GREEN_LED_ON(0);
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(duty);
        	RED_LED_ON(0);
			GREEN_LED_ON(0);
#endif
			break;
        case LED_BLINK_STATE4:
//        	D4 on 100%
//        	D5 in a 4-blink loop
//        	D6 OFF
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(0);
        	GREEN_LED_ON(0);
        	RED_LED_ON(duty);
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(1000);
        	GREEN_LED_ON(duty);
        	RED_LED_ON(0);
#endif
			break;
        case LED_BLINK_STATE5:
        case LED_BLINK_STATE6:
        case LED_BLINK_STATE7:
        case LED_BLINK_STATE8:
        case LED_BLINK_STATE9:
        case LED_BLINK_STATE10:
//        	D4 is OFF
//        	D5,6 in a 5-blink loop
#if defined(B542)  || defined(B527)  || defined(B543) || defined(B394)
//        	strcpy((char*)&led_para.category, "none");

        	if (!strcmp((char*)&led_para.category, "iHUB"))
        	{
				update_d3_led_based_on_cpu_idle();
				BLUE_LED_ON(0);
				GREEN_LED_ON(duty);
				RED_LED_ON(0);
        	}
        	else
        	{
				update_d3_led_based_on_cpu_idle();
				BLUE_LED_ON(0);
				GREEN_LED_ON(duty);
				RED_LED_ON(duty);
        	}
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(1000);
        	GREEN_LED_ON(duty);
        	RED_LED_ON(duty);
#endif
			break;
        case LED_BLINK_STATE_FULLON:
//			D4,5,6 on 100%
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(100);
        	GREEN_LED_ON(100);
        	RED_LED_ON(100);
#else
        	update_d3_led_based_on_cpu_idle();
        	BLUE_LED_ON(1000);
        	GREEN_LED_ON(1000);
        	RED_LED_ON(1000);
#endif
			break;
			default:
				update_d3_led_based_on_cpu_idle();
				BLUE_LED_ON(duty);
            break;
    }
}

static uint64_t get_current_time_ms() {
	return get_current_rtc_time_ms(0);
}

static void update_d3_led_based_on_cpu_idle(void)
{
//	int cpu_load_l = cpu_load;
//	FUSE_LED_ON(cpu_load_l*10);
}

static uint64_t rampStartTime = 0;
void BLE_LED_RampCycle(uint64_t cycleDuration)
{
    uint64_t currentTime = get_current_time_ms()%cycleDuration;  // convert to ms
    if (rampStartTime == 0) {
        rampStartTime = currentTime;
    }

    uint64_t elapsed = currentTime - rampStartTime;
    uint64_t halfCycle = cycleDuration / 2;

    if (elapsed < halfCycle) {
        BLE_LED_RampUp(halfCycle, elapsed);
    } else if (elapsed < cycleDuration) {
        BLE_LED_RampDown(halfCycle, elapsed - halfCycle);
    } else {
        rampStartTime = currentTime; // Reset for next full cycle
    }
}

void BLE_LED_RampUp(uint64_t duration, uint64_t elapsed)
{
    if (elapsed > duration)
    	elapsed = duration;

    int duty = (elapsed * 1000) / duration;
//    BLE_LED_Duty = duty;
//    BLE_Up_Down = 0;
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
    	BLUE_LED_ON(duty);
#else
    	BLE_LED_ON(duty);
#endif

}

void BLE_LED_RampDown(uint64_t duration, uint64_t elapsed)
{
    if (elapsed > duration) elapsed = duration;

    int duty = 1000 - ((elapsed * 1000) / duration);
//    BLE_LED_Duty = duty;
//    BLE_Up_Down = 1;
#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
    	BLUE_LED_ON(duty);
#else
    	BLE_LED_ON(duty);
#endif

}

#ifdef B394
static void Init_GPIOs(void)
{
    // build a bitmask for all inputs
    uint64_t pin_mask = (1ULL<<Input0)
                      | (1ULL<<Input1)
                      | (1ULL<<Input2)
                      | (1ULL<<Input3)
                      | (1ULL<<Input4)
                      | (1ULL<<Input5)
                      | (1ULL<<Input6)
                      | (1ULL<<Input7);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,           // pins to configure
        .mode         = GPIO_MODE_INPUT,    // set as input
        .pull_up_en   = GPIO_PULLUP_ENABLE,// change to GPIO_PULLUP_ENABLE if you need pull-ups
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_ANYEDGE   // or set to GPIO_INTR_ANYEDGE / GPIO_INTR_POSEDGE etc.
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    if(CheckInputQueue == NULL)
    	CheckInputQueue = xQueueCreateStatic(CheckInput_gpio_evt_queue_LENGTH, CheckInput_gpio_evt_queue_ITEMSIZE, CheckInput_gpio_evt_QueueStorage, &CheckInput_gpio_evt_queue_QueueBuffer);

    CheckInputsHandle = xTaskCreateStaticPinnedToCore(
    		Check_Input_states,                 // Task function
			"Check Inputs State",            // Task name
			CHECK_INPUT_STACK_DEPTH,        // Stack size in words
			NULL,                    // Task parameters (not used here)
			CHECK_INPUT_TASK_PRIORITY,                       // Task priority
			CheckInputxTaskStack,              // Pointer to task stack (allocated in PSRAM)
			&CheckInputTaskBuffer,           // Pointer to task control block
			1
		);
	if (CheckInputsHandle == NULL) {
		printf("Failed to create Check_Input_states task\n");
	}

    // Install GPIO ISR service
    gpio_install_isr_service(0);

    // Hook ISR handler for specific GPIO pins
    gpio_isr_handler_add(Input0, Check_Input_isr_handler, (void*) Input0);
    gpio_isr_handler_add(Input1, Check_Input_isr_handler, (void*) Input1);
    gpio_isr_handler_add(Input2, Check_Input_isr_handler, (void*) Input2);
    gpio_isr_handler_add(Input3, Check_Input_isr_handler, (void*) Input3);
    gpio_isr_handler_add(Input4, Check_Input_isr_handler, (void*) Input4);
    gpio_isr_handler_add(Input5, Check_Input_isr_handler, (void*) Input5);
    gpio_isr_handler_add(Input6, Check_Input_isr_handler, (void*) Input6);
    gpio_isr_handler_add(Input7, Check_Input_isr_handler, (void*) Input7);
}

static void IRAM_ATTR Check_Input_isr_handler(void* arg) {
    uint32_t gpio_num = (uint32_t)arg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(CheckInputQueue, &gpio_num, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

void Check_Input_states(void *pvParameters __attribute__((unused)))
{
	uint32_t io_num;
	int level = 0;
	cJSON *root = NULL;
	char Check_Input_data_buffer[100] = {0};
	char Payload[50] = {0};
	AMessage_st s_Message_Rx;
	s_Message_Rx.Dest_ID_a8 = LED;
	s_Message_Rx.Src_ID_u8 = CONSOLE;
	s_Message_Rx.payload_p8 = (uint8_t*) Check_Input_data_buffer;
	s_Message_Rx.payload_size = 0;
	strcpy((char*)s_Message_Rx.cmdFun_a8,"EVENT");
	strcpy((char*)s_Message_Rx.dest_Actor_a8,"LED");
	strcpy((char*)s_Message_Rx.src_Actor_a8,"CONSOLE");
	while(1)
	{
		 if (xQueueReceive(CheckInputQueue, &io_num, portMAX_DELAY))
		 {
			 level = gpio_get_level(io_num);
			 root = cJSON_CreateObject();
			 cJSON_AddNumberToObject(root, "GPIO", io_num);
			 cJSON_AddNumberToObject(root, "LEVEL", level);
			 memset(Payload,0,sizeof(Payload));
			 cJSON_PrintPreallocated(root, Payload, sizeof(Payload), false);
			 strcpy((char*)s_Message_Rx.payload_p8, Payload);
			 cJSON_Delete(root);
			 console_send_responce_to_console_xface(&s_Message_Rx);
		 }
	}
}
#endif


#if defined(B542) || defined(B527) || defined(B543) || defined(B394)
static void pattern_mode(void)
{
	NewPattern_u16Hue[0] = 0;
	NewPattern_u16Hue[1] = 180;
	NewPattern_u16Hue[2] = 360;


	uint64_t u64CurrentTime = get_current_time_ms();
	uint64_t u64CurrentTime1 = 0;
	u64CurrentTime1 = u64CurrentTime;

//	u64CurrentTime=u64CurrentTime/1000000000;
	u64CurrentTime=u64CurrentTime * 0.000000001;
	u64CurrentTime=u64CurrentTime*1000000000;

	u64CurrentTime1 = u64CurrentTime1 - u64CurrentTime;

	u64CurrentTime = u64CurrentTime1 & 0xFFFFFFFF;


	// Initialize color start and end points based on the pattern flag
	Color start, end, result;
	float sat_pattern = 1.0;  // Saturation percentage
	float val_pattern = 1.0;  // Brightness percentage

	 float period = 5000/2;
	 float remaining_ramp_ms = period - (fmod(u64CurrentTime, period));

	 if (fmod(u64CurrentTime, period * 2) < period) {
		 PatternStartFlag = 1;
	  } else {
		  PatternStartFlag = 2;
	  }
	 float output = remaining_ramp_ms / period;

	output = 1 - output;  // Reverse the output for interpolation

#ifdef ENABLE_PRINT_MSG
	printf("output  = %f \n", output);
#endif

	if (PatternStartFlag == 1) {
		start.hue = NewPattern_u16Hue[0];
		start.saturation = sat_pattern;
		start.brightness = val_pattern;
		end.hue = NewPattern_u16Hue[1];
		end.saturation = sat_pattern;
		end.brightness = val_pattern;
	} else {
		start.hue = NewPattern_u16Hue[1];
		start.saturation = sat_pattern;
		start.brightness = val_pattern;
		end.hue = NewPattern_u16Hue[2];
		end.saturation = sat_pattern;
		end.brightness = val_pattern;
	}

	// Interpolate color based on the output factor
	interpolate_color_led(&result, &start, &end, output);
	result.brightness *= 100;
	result.saturation *= 100;

	// Convert interpolated HSV values to RGB
	uint8_t red=0, green=0, blue=0;
//	hsv_to_rgb_16bit(result.hue, result.saturation, result.brightness, &red, &green, &blue);
	led_strip_hsv2rgb(result.hue, result.saturation, result.brightness, &red, &green, &blue);

	RED_LED_ON(red);
	GREEN_LED_ON(green);
	BLUE_LED_ON(blue);
}

// Helper function to interpolate between two float values
static inline float interpolate(float start, float end, float t) {
    return start + (end - start) * t;
}

// Function to handle hue interpolation specifically, taking the shortest path on the hue circle
static inline float interpolate_hue(float h1, float h2, float t)
{
    // Calculate the difference between the two hues
    float diff = h2 - h1;

    // Check if the absolute difference is greater than 180 degrees
    if (fabs(diff) > 180)
    {
        // Determine the direction to interpolate
        if (h2 > h1)
        {
            // If h2 is greater, adjust h1 upwards by 360 degrees to ensure
            // the interpolation takes the shorter path counter-clockwise
            h1 += 360;
        }
        else
        {
            // If h1 is greater, adjust h2 upwards by 360 degrees to ensure
            // the interpolation takes the shorter path clockwise
            h2 += 360;
        }
    }

    // Interpolate the adjusted hue values
    float hue = interpolate(h1, h2, t);

    // Ensure the interpolated hue value wraps around at 360 degrees,
    // keeping it within the standard hue range (0-360 degrees)
    // Use modulus to correct for any overshoot beyond 360 degrees
    hue = fmod(hue, 360);

    // Return the calculated hue, which is the shortest path interpolation result
    return hue;
}

// Main function to interpolate colors, optimized for precision with floating points
void interpolate_color_led(Color *result, const Color *start, const Color *end, float t)
{
//    if (start->brightness < 0.04)
    if (start->brightness <= 0.0)
    //if (start->brightness < 0.15)
    {
        // Start is black
        result->hue = end->hue;
        result->saturation = end->saturation;
        result->brightness = interpolate(0, end->brightness, t);
    }
    else if (end->brightness  <= 0.00)
    //else if (end->brightness  < 0.15)
    {
        // End is black
        result->hue = start->hue;
        result->saturation = start->saturation;
        result->brightness = interpolate(start->brightness, 0, t);
    }
    else if (start->saturation  <= 0.00 )
    //else if (start->saturation  < 0.15 )
    {
        // Start is white
        result->hue = end->hue;
        result->saturation = interpolate(0, end->saturation, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
    else if (end->saturation  <= 0.00)
    //else if (end->saturation  < 0.15)
    {
        // End is white
        result->hue = start->hue;
        result->saturation = interpolate(start->saturation, 0, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
    else
    {
        // Normal case: interpolate each component
        result->hue = interpolate_hue(start->hue, end->hue, t);
        result->saturation = interpolate(start->saturation, end->saturation, t);
        result->brightness = interpolate(start->brightness, end->brightness, t);
    }
}

//void led_strip_hsv2rgb(float h, float s, float v, uint32_t *r, uint32_t *g, uint32_t *b)
void led_strip_hsv2rgb(float h, float s, float v, uint8_t *r, uint8_t *g, uint8_t *b)
{
    float rgb_max = v * 2.55f;
    float rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    float diff = (uint32_t)h % 60;

    // RGB adjustment amount by hue
    float rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i%6) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
//	// Apply bit depth correction for each color channel
}

#endif
