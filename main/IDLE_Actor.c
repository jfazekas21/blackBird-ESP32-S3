#include "actor.h"
#include "Config.h"
#include "IDLE_Actor.h"
#include "Console_Actor.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_chip_info.h"
#include "spi_flash_mmap.h"
//#include "esp_spi_flash.h"
#include "esp_flash.h"
#include "rtc_wdt.h"

static AMessage_st s_Message_Tx;   //s_Message_Rx,
static bool     		FirstEntry_bool = false;
static char aTag[] = ">s_IDLE.";
static const char 	*THIS_ACTOR 	= 	"IDLE";
static const char 			THIS_ACTOR_ID 	= 	IDLE;
unsigned long long int idle_cnt_cpu0 = 0, idle_cnt_cpu1 = 0;
const int CPU0_LOAD_START_BIT = BIT0;
const int CPU1_LOAD_START_BIT = BIT1;
EventGroupHandle_t cpu_load_event_group;
uint64_t last_time =0;
char display_loading = 1;
#define OBJ_QUE_COUNT                	5
PSRAM_ATTR_BSS static char Rx_buffer[MAX_JSON_PAYLOAD_BYTES/2];
PSRAM_ATTR_BSS static char payLoadData[MAX_JSON_PAYLOAD_BYTES/2];

//------------------ Real Time State --------------------------------//

static BaseType_t 		Task_State;					// Msg Debug Execution Loop
static TaskHandle_t 	Task_State_Handle  	= NULL;	// Msg Debug Task Handler
static TaskHandle_t  	IDLE_Handle		= NULL;
//static SemaphoreHandle_t sync_stats_task;
static StaticTask_t xIDLETaskBuffer,xRT_statsTaskBuffer;  //// Declare a static task control block
static uint8_t *IDLE_Rx_Queue_pucQueueStorage = NULL;
static StackType_t *xTaskStack = NULL,*xRT_statsTaskStack=NULL;
static StaticQueue_t *IDLE_Rx_Queue_pxQueueBuffer = NULL;
#define STATS_TASK_PRIO     3  //old 2
#define STATS_TICKS         pdMS_TO_TICKS(1000)
#define ARRAY_SIZE_OFFSET   5   //Increase this if print_real_time_stats returns ESP_ERR_INVALID_SIZE

void Get_CPU_Load_Info(void);
void Get_CPU_Info(void);
//void Get_CPU_Status(void);
//static void print(char *str_buf);
//static void idle_task_CPU0(void *parm);
//static void idle_task_CPU1(void *parm);
//static void mon_task(void *parm);
//void Test_TASK(void);
static void Init();
static void stats_task(void *arg);
static uint8_t set(char *property, char *value, AMessage_st* s_Message_Rx);			//	Change a parameter
static void get(char *prop, char *val);					//	Read a Parameter
//static void print(char *str_buf);
static void help(AMessage_st* s_Message_Rx);
//static void getAll(char *str_prop, char *val_a8,  AMessage_st* s_Message_Rx);
static void monitor(void *pvParameters __attribute__((unused)));
static void Get_Property(AMessage_st* s_Message_Rx);
//static void Add_response_to_Tx_Queue(const char *Destactor, char *response, int16_t size, char *CmdFunc);
static void Analyse_Response(AMessage_st* s_Message_Rx);
static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx);
//static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static esp_err_t print_real_time_stats(TickType_t xTicksToWait,AMessage_st* s_Message_Rx);
static void get_actor_properties(AMessage_st* s_Message_Rx);                  // Get actor properties
static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer);

//static char out[MAX_OUT_SIZE];
static struct IDLE_registers {
	int CPU_loading;
	char CPU_Status;
} IDLE_Para;

//IDLE_user_config config_IDLE;

static struct property prop[] = // Actor Property
{
    { &IDLE_Para.CPU_loading, "CPU_LOAD", INT, "R", "CPU loading percentage" },
    { &IDLE_Para.CPU_Status, "CPU_STATUS", INT,"R", "CPU status indicator" }
};

static QueueHandle_t IDLE_Rx_Queue; //, IDLE_Tx_Queue;

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
	cJSON_AddStringToObject(responseObject, "INIT()", "Initialize the SQL actor.");
	cJSON_AddStringToObject(responseObject, "SET(INT CPU_LOAD)", "Set the parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "GET(CPU_LOAD)", "Get the parameters of the property table.");
//	cJSON_AddStringToObject(responseObject, "GETALL()", "Get all parameters of the property table.");
	cJSON_AddStringToObject(responseObject, "TEST()", "Start checking CPU load.");
	cJSON_AddStringToObject(responseObject, "TEST_STOP()","Stop checking CPU load.");
	cJSON_AddStringToObject(responseObject, "PROPERTIES()", "Display the values, read/write access, and help of property table.");
	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}//	help

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


static void Init() {
	// Calculate the total size of memory required for the stack array
		size_t stack_size = IDLE_TASK_STACK_DEPTH * sizeof(StackType_t);

	if (FirstEntry_bool) {
				return;
			}
	char* out;

//	IDLE_Tx_Queue = xQueueCreate(OBJ_QUE_COUNT, sizeof(s_Message_Tx));
//	if (IDLE_Tx_Queue == NULL) {
//	}

	//s_IDLE.print("Creating Rx Queue . . .");
	IDLE_Rx_Queue = xQueueCreateInPSRAM(OBJ_QUE_COUNT,sizeof(AMessage_st),&IDLE_Rx_Queue_pucQueueStorage,&IDLE_Rx_Queue_pxQueueBuffer);
			//xQueueCreate(OBJ_QUE_COUNT, sizeof(AMessage_st));
//	if (IDLE_Rx_Queue == NULL) {
//		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "IDLE RX Queue is not created.");
//
//	}

	//	 Creating Monitor Task
	if (xTaskStack!=NULL) {
		free(xTaskStack);
		xTaskStack = NULL;  // Set pointer to NULL after freeing
	}
	xTaskStack = (StackType_t *)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
				if (xTaskStack == NULL) {
				    // Memory allocation failed
#ifdef ENABLE_PRINT_MSG
				    printf( "Failed to allocate memory for the task stack");
#endif
				    // Handle error
				    return;
				}
				IDLE_Handle = xTaskCreateStaticPinnedToCore(
								monitor,                 // Task function
								"IDLE Monitor",            // Task name
								IDLE_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								IDLE_TASK_PRIORITY,                       // Task priority
								xTaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xIDLETaskBuffer,             // Pointer to task control block
								0
			   );

		if (IDLE_Handle == NULL) {
#ifdef ENABLE_PRINT_MSG
			    printf("Failed to create task\n");
#endif
			    // Handle error
			}

//	printf("Creating CPU Status Monitor Task . . .");
//	xTaskCreate(monitor, (const char*) "IDLE Monitor", 4096, NULL, 2, NULL);

	//sync_stats_task = xSemaphoreCreateBinary();
//	 xTaskCreatePinnedToCore(stats_task, "RT_stats", 4096, NULL, STATS_TASK_PRIO, NULL, tskNO_AFFINITY);
//    xSemaphoreGive(sync_stats_task);


	IDLE_Para.CPU_loading = 50;
	IDLE_Para.CPU_Status = 1;

//	   cJSON *responseObject = cJSON_CreateObject();
//		cJSON_AddStringToObject(responseObject, "FILE_NAME","System/IDLE.json");
//		out= cJSON_PrintUnformatted(responseObject);
//		Send_CMD_To_Other_Actor(SYS_FILES,"SYS_FILES", out, strlen(out), "READ");
//		cJSON_free(out);
//		cJSON_Delete(responseObject);

//	strcpy(out, "{\"filename\":\"A:/System/IDLE.json\"}");
//	Add_response_to_Tx_Queue("FILE_MGT", out, strlen(out), "READ");
	s_Message_Tx.Src_ID_u8	= THIS_ACTOR_ID;
	FirstEntry_bool = true;
//	console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR,"IDLE actor is Initialized");
}


/**
 * @brief   Function to print the CPU usage of tasks over a given duration.
 *
 * This function will measure and print the CPU usage of tasks over a specified
 * number of ticks (i.e. real time stats). This is implemented by simply calling
 * uxTaskGetSystemState() twice separated by a delay, then calculating the
 * differences of task run times before and after the delay.
 *
 * @note    If any tasks are added or removed during the delay, the stats of
 *          those tasks will not be printed.
 * @note    This function should be called from a high priority task to minimize
 *          inaccuracies with delays.
 * @note    When running in dual core mode, each core will correspond to 50% of
 *          the run time.
 *
 * @param   xTicksToWait    Period of stats measurement
 *
 * @return
 *  - ESP_OK                Success
 *  - ESP_ERR_NO_MEM        Insufficient memory to allocated internal arrays
 *  - ESP_ERR_INVALID_SIZE  Insufficient array size for uxTaskGetSystemState. Trying increasing ARRAY_SIZE_OFFSET
 *  - ESP_ERR_INVALID_STATE Delay duration too short
 */
static esp_err_t print_real_time_stats(TickType_t xTicksToWait,AMessage_st* s_Message_Rx)
{
    TaskStatus_t *start_array = NULL, *end_array = NULL;
    UBaseType_t start_array_size, end_array_size;
    uint32_t start_run_time, end_run_time;
    esp_err_t ret;
    char str[100];
    char cpu_load_str[10];
    uint32_t cpu_loading = 0;//, cpu1_loading;
//    int j=0;

    //Allocate array to store current task states
    start_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    start_array = heap_caps_malloc(sizeof(TaskStatus_t) * start_array_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (start_array == NULL)
    {
    	Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get current task states
    start_array_size = uxTaskGetSystemState(start_array, start_array_size, &start_run_time);
    if (start_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }
    vTaskDelay(xTicksToWait);

    //Allocate array to store tasks states post delay
    end_array_size = uxTaskGetNumberOfTasks() + ARRAY_SIZE_OFFSET;
    end_array = heap_caps_malloc(sizeof(TaskStatus_t) * end_array_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (end_array == NULL)
    {
    	Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
        ret = ESP_ERR_NO_MEM;
        goto exit;
    }
    //Get post delay task states
    end_array_size = uxTaskGetSystemState(end_array, end_array_size, &end_run_time);
    if (end_array_size == 0) {
        ret = ESP_ERR_INVALID_SIZE;
        goto exit;
    }

    //Calculate total_elapsed_time in units of run time stats clock period.
    uint32_t total_elapsed_time = (end_run_time - start_run_time);
    if (total_elapsed_time == 0) {
        ret = ESP_ERR_INVALID_STATE;
        goto exit;
    }

    //Match each task in start_array to those in the end_array
    for (int i = 0; i < start_array_size; i++) {
        int k = -1;
        for (int j = 0; j < end_array_size; j++) {
            if (start_array[i].xHandle == end_array[j].xHandle) {
                k = j;
                //Mark that task have been matched by overwriting their handles
                start_array[i].xHandle = NULL;
                end_array[j].xHandle = NULL;
                break;
            }
        }
        //Check if matching task found
        if (k >= 0) {
            uint32_t task_elapsed_time = end_array[k].ulRunTimeCounter - start_array[i].ulRunTimeCounter;
            uint32_t percentage_time = (task_elapsed_time * 100UL) / (total_elapsed_time * portNUM_PROCESSORS);
            if((!strcmp(start_array[i].pcTaskName,"IDLE"))) //configIDLE_TASK_NAME
            {
            	cpu_loading += percentage_time;
            }

        }
    }

    IDLE_Para.CPU_loading = 100 - cpu_loading;
    if(display_loading == 1)
    {
//    	sprintf(str,"CPU Loading: %ld%%",(100-cpu_loading));
//    	Add_Response_msg(str,s_Message_Rx);

    	cJSON *responseObject = cJSON_CreateObject();

    	snprintf(cpu_load_str, sizeof(cpu_load_str), "%d", IDLE_Para.CPU_loading);

    	cJSON_AddStringToObject(responseObject, "CPU_LOAD",cpu_load_str);
    	memset(payLoadData,0,sizeof(payLoadData));
    	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
    	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

		cJSON_Delete(responseObject);
		console_send_responce_to_console_xface(s_Message_Rx);
    	//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, str);
    }

    ret = ESP_OK;


exit:    //Common return path
	if(start_array != NULL)
	{
		free(start_array);
		start_array = NULL;
	}
	if(end_array != NULL)
	{
	    free(end_array);
	    end_array = NULL;
	}
    return ret;
}

static void stats_task(void *arg)
{
	AMessage_st* s_Message_Rx = (AMessage_st*)arg;
	// xSemaphoreTake(sync_stats_task, portMAX_DELAY);
    //Print real time stats periodically
    while (1) {
     //   printf("\n\nGetting real time stats over %d ticks\n", STATS_TICKS);
        if (print_real_time_stats(STATS_TICKS,s_Message_Rx) == ESP_OK) {
           // printf("Real time stats obtained\n");
        } else {
        	Add_Response_msg("Error getting real time stats",s_Message_Rx);
        	//console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Error getting real time stats");

        }
        vTaskDelay(10000/portTICK_PERIOD_MS);  //pdMS_TO_TICKS(1000)
    }
}
//static void idle_task_CPU0(void *parm)
//
//{
////	IDLE_Para.CPU_Status = 0;
////	s_IDLE.print("CPU0 is in IDLE state.");
////	const TickType_t xTicksToWait = 1000 / portTICK_PERIOD_MS;
//	while (1) {
//		xEventGroupWaitBits(cpu_load_event_group, CPU0_LOAD_START_BIT, false, false, portMAX_DELAY);
//	//	xEventGroupWaitBits(cpu_load_event_group, CPU0_LOAD_START_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
//		uint64_t now = esp_timer_get_time();     // time anchor
//		//vTaskDelay(0 / portTICK_RATE_MS);
//		uint64_t now2 = esp_timer_get_time();
//		idle_cnt_cpu0 += (now2 - now);    // 1000    // diff
//		//printf("\n CPU 0 idle_cnt=%llu", idle_cnt);
//
//		//
//
//		// rtc_wdt_feed();							// [SKS]:kick watch dog timer so it does not reset due to inactivity.
//		// if cpu is 100 % busy , this task will never be scheduled
//		//  watchdog will still reset the CPU
//		//   printf("\n IDLE Count=%d",idle_cnt);
//		//vTaskDelay(0/ portTICK_RATE_MS);
//	}
//}
//
//static void idle_task_CPU1(void *parm)
//
//{
////	IDLE_Para.CPU_Status = 0;
////	s_IDLE.print("CPU1 is in IDLE state.");
//	//const TickType_t xTicksToWait = 1000 / portTICK_PERIOD_MS;
//	while (1) {
//		xEventGroupWaitBits(cpu_load_event_group, CPU1_LOAD_START_BIT,  pdFALSE, pdFALSE, portMAX_DELAY);
//		uint64_t now = esp_timer_get_time();     // time anchor
//		//vTaskDelay(0 / portTICK_RATE_MS);
//		uint64_t now2 = esp_timer_get_time();
//		idle_cnt_cpu1 += (now2 - now) ;    // 1000    // diff
//	//	printf("\n CPU 1 idle_cnt=%llu", idle_cnt);
//		// rtc_wdt_feed();							// [SKS]:kick watch dog timer so it does not reset due to inactivity.
//		// if cpu is 100 % busy , this task will never be scheduled
//		//  watchdog will still reset the CPU
//		//   printf("\n IDLE Count=%d",idle_cnt);
//	//	vTaskDelay(1/ portTICK_RATE_MS);
//	}
//}

//static void mon_task(void *parm)  // This function is executed after each 10 msec
//
//{
//	static uint8_t ucParameterToPass;
//	TaskHandle_t xHandle1 = NULL,xHandle2 = NULL;
//	float cpuPercent;
//	uint64_t elapsed_time=0;
//	float adjust = 1;  //1.1  , 1
//
////	printf("\n Inside mon task");
//	cpu_load_event_group = xEventGroupCreate();
//	if( cpu_load_event_group == NULL )
//	{
//	    printf("\n The event group was not created because there was insufficient FreeRTOS heap available.");
//	}
//	else
//	{
//		printf("\n The event group was created.");
//	}
//	xEventGroupClearBits(cpu_load_event_group, CPU0_LOAD_START_BIT);
//	xEventGroupClearBits(cpu_load_event_group, CPU1_LOAD_START_BIT);
//
//	//xTaskCreatePinnedToCore(&idle_task_CPU0, "idle_task_CPU0", configMINIMAL_STACK_SIZE,&ucParameterToPass, tskIDLE_PRIORITY, &xHandle,0); //lowest priority CPU 0 task
//	xTaskCreatePinnedToCore(&idle_task_CPU0, "idle_task_CPU0", (CONFIG_FREERTOS_IDLE_TASK_STACKSIZE+1000),&ucParameterToPass, tskIDLE_PRIORITY, &xHandle1,0); //lowest priority CPU 0 task
//		 configASSERT( xHandle1 );
//	xTaskCreatePinnedToCore(&idle_task_CPU1, "idle_task_CPU1", (CONFIG_FREERTOS_IDLE_TASK_STACKSIZE+1000), &ucParameterToPass, tskIDLE_PRIORITY, &xHandle2,1); //lowest priority CPU 1 task
//	 configASSERT( xHandle2 );
////		 xEventGroupSetBits(cpu_load_event_group, CPU0_LOAD_START_BIT);
////		 xEventGroupSetBits(cpu_load_event_group, CPU1_LOAD_START_BIT);
//	while (true)
//	{
//
//			 xEventGroupSetBits(cpu_load_event_group, CPU0_LOAD_START_BIT); // Signal idleCPU0Task to start timing
//		     xEventGroupSetBits(cpu_load_event_group, CPU1_LOAD_START_BIT); // Signal idleCPU1Task to start timing
//	//measure CPU0
//	//	printf("\n measure CPU0\n ");
//	//idle_cnt = 0; // Reset usec timer
//	// Signal idleCPU0Task to start timing
//		     vTaskDelay(1000 / portTICK_RATE_MS); //measure for 1 second
//		 	xEventGroupClearBits(cpu_load_event_group, CPU0_LOAD_START_BIT); // Signal to stop the timing
//		 	xEventGroupClearBits(cpu_load_event_group, CPU1_LOAD_START_BIT); // Signal to stop the timing
//		 	//vTaskDelay(0 / portTICK_RATE_MS); //make sure idleCnt isnt being used
//		 	uint64_t current_time = esp_timer_get_time();
//		 	elapsed_time = current_time - last_time ;
//		 	last_time = current_time;
//		 	//elapsed_time = 1000000;
//
////	xEventGroupClearBits(cpu_load_event_group, CPU0_LOAD_START_BIT); // Signal to stop the timing
////	vTaskDelay(1 / portTICK_RATE_MS); //make sure idleCnt isnt being used
//
//	// Compensate for the 100 ms delay artifact: 900 msec = 100%
//	//cpuPercent = ((99.9 / 90.0) * idleCnt/1000.0) / 10.0;
////	if(idle_cnt>100000)
////		idle_cnt=100000;
//	cpuPercent = ((float)((float)(idle_cnt_cpu0)/(float)elapsed_time))*100.0;
//		 	//printf("\n cpuPercent=%f", cpuPercent);
//	printf("\n CPU0: idle_cnt_cpu0 = %lld,      elapsed_time = %lld,  IdleTime = %.2f%% ",idle_cnt_cpu0, elapsed_time, cpuPercent);
//	fflush(stdout);
//	idle_cnt_cpu0 = 0;
//
//
//	cpuPercent = ((float)((float)(idle_cnt_cpu1)/(float)elapsed_time))*100.0;
//		//printf("\n cpuPercent=%f", cpuPercent);
//	printf("\n CPU1: idle_cnt_cpu1 = %lld,       elapsed_time = %lld,      IdleTime = %.2f%% ",idle_cnt_cpu1, elapsed_time, cpuPercent);
//	fflush(stdout);
//	idle_cnt_cpu1 = 0;
//
//
//	}
//
//}
//static void test_task(void *parm)
//
//{
//	while (1) {
//		 uint64_t now = esp_timer_get_time();     // time anchor
//		for(int i=0;i<60000;i++)
//			{
//				for(int j=0;j<60000;j++)
//				{
//					for(int k=0;k<60000;k++)
//					{
//
//					}
//				}
//			}
//		 uint64_t now2 = esp_timer_get_time();
//		printf("\n Busy time = %lld", (now2 - now));
//		vTaskDelay(1/ portTICK_RATE_MS);
//	}
//}

void Get_CPU_Load_Info(void) {
	//s_IDLE.print("CPU Load Demo.\n");
	//xTaskCreate(idle_task_CPU0, "idle_task", 1024 * 2, NULL, 0, NULL);
//	last_time = esp_timer_get_time();
//	xTaskCreate(mon_task, "mon_task", 1024 * 2, NULL, 5, NULL);
	//Task_State = xTaskCreate(stats_task, "stats" ,4096, NULL, STATS_TASK_PRIO, &Task_State_Handle);
//	xTaskCreate(test_task, "test_task", 1024 * 2, NULL, 10, NULL);
	//xTaskCreate(Test_TASK, "Test_TASK", 1024 * 2, NULL, 5, NULL);
	int cnt = 0;
//	while (true) {
////		// Every 10 seconds, we put the processor to work.
////		if (++cnt % 10 == 0) {
////			int cnt_dummy = 0;
////
////			s_IDLE.print(" work [[ ");
////			fflush(stdout);
////
////			//Make sure the watchdog is not triggered ...
////			for (int aa = 0; aa < 30000000; aa++) {
////				cnt_dummy += 22;
////			}
////
////			s_IDLE.print(" ]] rest ");
////			fflush(stdout);
////		}
//		vTaskDelay(100 / portTICK_RATE_MS);
//	}

}

void Get_CPU_Info(void) {
	esp_chip_info_t chip_info;
	esp_chip_info(&chip_info);

#ifdef ENABLE_PRINT_MSG
	printf("ESP32, %d CPU cores, WiFi%s%s, ", chip_info.cores,
			(chip_info.features & CHIP_FEATURE_BT) ? "/BT" : "",
			(chip_info.features & CHIP_FEATURE_BLE) ? "/BLE" : "");


	printf("silicon revision %d, ", chip_info.revision);

	printf("%dMB %s flash\n", esp_flash_get_size(0,0) / (1024 * 1024),
			(chip_info.features & CHIP_FEATURE_EMB_FLASH) ?
					"embedded" : "external");
#endif

}

//void Get_CPU_Status(void) {
//	if (IDLE_Para.CPU_Status == 1)
//		//s_IDLE.print(" CPU is Busy.");
//	else
//	//	s_IDLE.print("CPU is in IDLE state.");
//
//}

//void Test_TASK(void) {
//	unsigned int cnt = 0, cnt1, cnt2;
//	while (1) {
//		for (cnt = 0; cnt < 0xFFFF; cnt++) {
//			for (cnt1 = 0; cnt1 < 0xFFFF; cnt1++) {
//				for (cnt2 = 0; cnt2 < 0xFFFF; cnt2++) {
//					//printf("\n cnt= %x,  cnt1=%x,  cnt2=%x", cnt,cnt1,cnt2);
//					//vTaskDelay(10 / portTICK_RATE_MS);
//				}
//			}
//		}
//
//		//cnt++;
//		//printf("\n Task cnt=%d", cnt);
//		//	printf("\n cnt= %x,  cnt1=%x,  cnt2=%x", cnt,cnt1,cnt2);
//		vTaskDelay(10 / portTICK_PERIOD_MS);
//
//	}
//
////	printf("\n Task is Over.");
////	vTaskDelay(1000 / portTICK_RATE_MS);
//
//}

static void monitor(void *pvParameters __attribute__((unused))) {

	cJSON *in_JSON;
	cJSON *out_JSON;
	cJSON *name_JSON = NULL;
    cJSON *head_JSON = NULL;  //refer to head_JSON and tail as in linked list
	uint8_t *val_p8  = NULL;
	char str[100] = {0};
	char Rx_buffer_used = 0;
	uint8_t u8Result =0;

	while (1)
	{
		AMessage_st s_Message_Rx_data;
		AMessage_st *s_Message_Rx = &s_Message_Rx_data;
//		char Rx_buffer[8192];
		memset(Rx_buffer,0,sizeof(Rx_buffer));
		memset(&s_Message_Rx_data,0,sizeof(AMessage_st));
		if (pdTRUE == xQueueReceive(IDLE_Rx_Queue, (void*) (s_Message_Rx), portMAX_DELAY))
		{
//		    printf("Src = %s, Dest = %s, Command = %s, DT = %s\n\n", s_Message_Rx.src_Actor_a8,	s_Message_Rx.dest_Actor_a8, s_Message_Rx.cmdFun_a8,s_Message_Rx.payload_p8);
			Rx_buffer_used = 0;
			strncpy(Rx_buffer, (char*)s_Message_Rx->payload_p8,((MAX_JSON_PAYLOAD_BYTES/2)-1));
			if(s_Message_Rx->payload_p8 != NULL)
			{
				console_MessageRelease_xface((char*)s_Message_Rx->payload_p8);
				s_Message_Rx->payload_p8 = NULL;
			}
			s_Message_Rx->payload_p8 = (uint8_t*)Rx_buffer;
			if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "TEST"))
			{
				if(Task_State_Handle == NULL)
				{
					display_loading = 1;
					Rx_buffer_used =1;
					strcpy((char *)s_Message_Rx->src_Actor_a8, "LED");
					s_Message_Rx->Src_ID_u8 = LED;
					strcpy((char *)s_Message_Rx->cmdFun_a8, "GET");
					//Task_State = xTaskCreate(stats_task, "RT_stats" ,IA_STATS_TASK_STACK_DEPTH, Rx_buffer, STATS_TASK_PRIO, &Task_State_Handle);
					size_t stack_size = IA_STATS_TASK_STACK_DEPTH * sizeof(StackType_t);

					if (xRT_statsTaskStack!=NULL) {
						free(xRT_statsTaskStack);
						xRT_statsTaskStack = NULL;  // Set pointer to NULL after freeing
					}
					 xRT_statsTaskStack = (StackType_t *)heap_caps_malloc(stack_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
					if (xRT_statsTaskStack == NULL) {
						// Memory allocation failed
			#ifdef ENABLE_PRINT_MSG
						printf( "Failed to allocate memory for the task stack");
			#endif
						Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
						// Handle error
						return;
					}
					Task_State_Handle = xTaskCreateStaticPinnedToCore(
									stats_task,                 // Task function
									"RT_stats",            // Task name
									IA_STATS_TASK_STACK_DEPTH,        // Stack size in words
									Rx_buffer,                    // Task parameters (not used here)
									STATS_TASK_PRIO,                       // Task priority
									xRT_statsTaskStack,              // Pointer to task stack (allocated in PSRAM)
									&xRT_statsTaskBuffer,             // Pointer to task control block
									0
				   );

				if (Task_State_Handle == NULL) {
			#ifdef ENABLE_PRINT_MSG
						printf("Failed to create task\n");
			#endif
						// Handle error
					}
				}
				 else
				 {
					Rx_buffer_used = 0;
					Add_Response_msg("Task for TEST command is already created.", s_Message_Rx);
				 }
				//Get_CPU_Load_Info();
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "TEST_STOP"))
			{
				if ((display_loading == 1) && (Task_State_Handle != NULL))
				{
					display_loading = 0;
					vTaskDelete(Task_State_Handle);
					Task_State_Handle = NULL;
					Add_Response_msg("Stopped display of CPU loading messages.",s_Message_Rx);
				}
				else
				    Add_Response_msg("Display of CPU loading messages are already stopped.",s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "INIT")) // Get Actor Properties
			{
				if (FirstEntry_bool == 0)
				    Init();
				else if(FirstEntry_bool==1)
					 Add_Response_msg("IDLE actor is Initialized",s_Message_Rx);
			} else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "GET")) // Get Actor Properties
			{
				Get_Property(s_Message_Rx);
			}
//			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8, "GETALL"))
//			{
//				val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
//				getAll(prop, (char*) val_p8,s_Message_Rx);
//				free(val_p8);
//			}
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
//					cJSON *root_JSON  = cJSON_CreateObject();
//					cJSON_AddStringToObject(root_JSON, "FILE_NAME", "A:/System/IDLE.json");
				   // Loop through each key-value pair
				    do {
				    	// Check if the value string is not NULL
				    	if (cJSON_IsString(head_JSON)) //if (cJSON_IsString(head_JSON) && (strlen(head_JSON->valuestring)!= 0))
							{
								// Set the key-value pair
								u8Result = set(head_JSON->string, head_JSON->valuestring,s_Message_Rx);
								if(u8Result==1)
								{
//								cJSON_AddStringToObject(root_JSON, head_JSON->string, head_JSON->valuestring);
								}
								else if(u8Result==2){
									sprintf(str,"'%s' is a read only property", head_JSON->string);
									 Add_Response_msg(str, s_Message_Rx);
								}
//								else{
////								cJSON_AddStringToObject(root_JSON,head_JSON->string, "Invalid Key" );
//								}
							} else {
				            // Handle the case where value string is NULL (e.g., log an error or take appropriate action)
				            sprintf(str, "Invalid parameter '%s'", head_JSON->string);
				            Add_Response_msg(str,s_Message_Rx);
				            // Handle the error as per your application's requirements
				        }
				        head_JSON = head_JSON->next;
				    } while (head_JSON != 0);

				    // Free the parsed JSON
				    cJSON_Delete(name_JSON);

				    if(u8Result==1){
				    //  save parameters to JFS
//				    char* cJOSN_string = cJSON_PrintUnformatted(root_JSON);
//				   Send_CMD_To_Other_Actor(SYS_FILES, "SYS_FILES",cJOSN_string,strlen(cJOSN_string),"WR_PARA");
//				    cJSON_free(cJOSN_string);
//				    cJSON_Delete(root_JSON);
				    console_send_responce_to_console_xface(s_Message_Rx);  //Send response to console interface
				    }

				}
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "HELP"))
			{
				help(s_Message_Rx);
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "CPU_INFO"))
			{
				Get_CPU_Info();
			}
			else if (!strcmp((char*) s_Message_Rx->cmdFun_a8, "RESPONSE"))
			{
				Analyse_Response(s_Message_Rx);
			}
			else if (!strcmp((char*)s_Message_Rx->cmdFun_a8 , "PROPERTIES"))
			 {
				get_actor_properties(s_Message_Rx);
			 }
			else
			{
				//IDLE error message: invalid method
				Add_Response_msg("invalid method", s_Message_Rx);
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
//			}
		}
	}
}

void Idle_ConsolWriteToActor_xface(void *msg)
{
	AMessage_st *s_Message_Rx;
	s_Message_Rx = (AMessage_st*)msg;

	Init();

	uint8_t state = xQueueSend(IDLE_Rx_Queue, s_Message_Rx, QUE_DELAY);
	if (state != pdTRUE)
	{
		if(s_Message_Rx->payload_p8 != NULL)
		{
			cJSON_free(s_Message_Rx->payload_p8);
			s_Message_Rx->payload_p8 = NULL;
		}
		if (state == errQUEUE_FULL)
		{
			printf("<IDLE.ERROR(IDLE RX Queue is full)\n");
//			console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "HTTP RX Queue is full.");
		}
		else
		{
			printf("<IDLE.ERROR(IDLE RX Queue send unsuccessful)\n");
		}
	}

}//	Idle_ConsolWriteToActor_xface

static void Get_Property(AMessage_st* s_Message_Rx)
{
//	uint8_t *val_p8  = NULL;
	cJSON *in_JSON   = NULL;
	cJSON *out_JSON  = NULL;
	cJSON *head_JSON = NULL;
	char str[100] = {0};
	uint8_t val_p8[256]={0};

//	val_p8 = (uint8_t*) heap_caps_calloc(256, sizeof(uint8_t),MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
//
//	if (val_p8 == NULL)
//	{
//		console_send_responce_to_console_xface(MSG_STR, THIS_ACTOR, "Payload is NULL in GET method");
//		printf("Memory allocation failed\n");
//		Add_Response_msg("Error! Failed to allocate memory for storing parameters in the JFS.",s_Message_Rx);
//		return;
//	}

	in_JSON 	= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
		Add_Response_msg(str,s_Message_Rx);
		  //  return 1;
		return;
	}
	out_JSON 	= cJSON_CreateObject();
	head_JSON 	= in_JSON->child;

	//loop
	do {
		memset(val_p8,0,sizeof(val_p8));
		get(head_JSON->string, (char*) val_p8);
		cJSON_AddStringToObject(out_JSON, head_JSON->string, (char*) val_p8);
		head_JSON = head_JSON->next;
	} while (head_JSON != NULL);


	memset(payLoadData,0,sizeof(payLoadData));
	cJSON_PrintPreallocated(out_JSON, payLoadData, sizeof(payLoadData), false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);


	cJSON_Delete(out_JSON);
	cJSON_Delete(head_JSON);
	cJSON_Delete(in_JSON);
	console_send_responce_to_console_xface(s_Message_Rx);
//	free(val_p8);
}




static void Analyse_Response(AMessage_st* s_Message_Rx)
{	cJSON *in_JSON 		= NULL;
	char keyValue[50] = {0};
	char str[200] = {0};

	if(s_Message_Rx->payload_p8 == NULL)
		return;

	if(strlen((char*)s_Message_Rx->payload_p8) == 0)
		return;

	in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
	if (in_JSON == NULL) {
		printf("\n IDLE s_Message_Rx->payload_p8 = %s \n",(char*)s_Message_Rx->payload_p8);
		sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
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
//	if(strcmp((char*)s_Message_Rx->src_Actor_a8,"FILE_SYSTEM")==0)
//	{
//		in_JSON 		= cJSON_Parse((char*) s_Message_Rx->payload_p8);
//		if (in_JSON == NULL) {
//			sprintf(str,"Invalid Json inputat %d in %s",__LINE__,__FUNCTION__);
//			Add_Response_msg(str,s_Message_Rx);
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
//					if(!strcmp(currentItem->valuestring, "System/IDLE.json"))
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
	return;
}

static void Add_Response_msg(char* buffer, AMessage_st* s_Message_Rx)
{
	cJSON *responseObject = cJSON_CreateObject();  	// Create a RESPONSE object and add it to the RESPONSE array
	cJSON_AddStringToObject(responseObject, "RESP", buffer);
	payLoadData[0] = '\0';
	cJSON_PrintPreallocated(responseObject, payLoadData, MAX_JSON_PAYLOAD_BYTES/2, false);
	strcpy((char*)s_Message_Rx->payload_p8, payLoadData);

	cJSON_Delete(responseObject);
	console_send_responce_to_console_xface(s_Message_Rx);
}

static QueueHandle_t xQueueCreateInPSRAM(UBaseType_t uxQueueLength, UBaseType_t uxItemSize, uint8_t** pucQueueStorage, StaticQueue_t** pxQueueBuffer)
{
    QueueHandle_t xQueue = NULL;

    // Allocate the queue storage area in PSRAM
    *pucQueueStorage = (uint8_t *)heap_caps_malloc((uxQueueLength * uxItemSize), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (*pucQueueStorage == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("\n Failed to allocate queue storage in PSRAM \n");
#endif
        return NULL;
    }
    memset(*pucQueueStorage,0,(uxQueueLength * uxItemSize));

    // Allocate the queue structure itself in PSRAM
    *pxQueueBuffer = (StaticQueue_t *)heap_caps_malloc(sizeof(StaticQueue_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (*pxQueueBuffer == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Failed to allocate queue structure in PSRAM\n");
#endif
        heap_caps_free(*pucQueueStorage);
		*pucQueueStorage = NULL;
        return NULL;
    }
    memset(*pxQueueBuffer,0,sizeof(StaticQueue_t));
    // Create the queue with custom storage
    xQueue = xQueueCreateStatic(uxQueueLength, uxItemSize, *pucQueueStorage, *pxQueueBuffer);

    if (xQueue == NULL) {
#ifdef ENABLE_PRINT_MSG
        printf("Failed to create queue in PSRAM \n");
#endif
        heap_caps_free(*pucQueueStorage);
        heap_caps_free(*pxQueueBuffer);
		*pucQueueStorage = NULL;
        *pxQueueBuffer = NULL;
    }
    return xQueue;
}
