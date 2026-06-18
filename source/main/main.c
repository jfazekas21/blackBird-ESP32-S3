/* Console example

 */


#include "actor.h"
#include "FS.h"
//-----------Actors------//
#include "UART_Actor.h"
#include "console_Actor.h"
#include "esp_psram.h"
#include "config.h"
#include "esp_private/system_internal.h"  // Access to internal RTC control
#include "esp_task_wdt.h"
#include "hal/wdt_hal.h"
#include "pcf8563.h"
#include "driver/gptimer.h"
#include "esp_flash.h"
#include "soc/rtc_cntl_reg.h"

#ifdef CONFIG_ESP_CONSOLE_USB_CDC                               // For CLI
#error This example is incompatible with USB CDC console. Please try "console_usb" example instead.
#endif // CONFIG_ESP_CONSOLE_USB_CDC

//static const char *TAG = "example";
#define PROMPT_STR CONFIG_IDF_TARGET
#define MWDT_DEFAULT_TICKS_PER_US       500

#if defined(B527)
#define LIGHT1 GPIO_NUM_4
#define LIGHT2 GPIO_NUM_5
#endif

#ifdef B527
static void Init_Lights(void);
#endif


static cJSON_Hooks memory_hooks;
PSRAM_ATTR_BSS static StackType_t watchdogTaskStack[WATCHDOG_TASK_STACK_SIZE];
static StaticTask_t watchdogTaskControlBlock;
static TaskHandle_t watchdogTaskHandle;
static void watchdogMonitoredTask(void *pvParameter);
static void watchdog_Init();
void set_custom_memory_hooks();
static void disable_all_watchdogs(void);
static void reconfig_watchdogs(uint32_t timeout_ms);
static void Send_CMD_To_Other_Actor(char dest_id, const char *DestActor, char *response, int16_t size, char *function);
static void Get_ESP_Reset_Reason(char *CPU0_RST_Reason, char *CPU1_RST_Reason);


static i2c_dev_t dev;
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
static TaskHandle_t USB_Handle 	= NULL;
PSRAM_ATTR_BSS static StackType_t xUSBTaskStack[UART_TASK_STACK_DEPTH];
static StaticTask_t xUSBTaskBuffer;
static void usb_console_task(void *param);
#endif
/* Console command history can be stored to and loaded from a file.
 * The easiest way to do this is to use FATFS filesystem on top of
 * wear_levelling library.
 */
#if CONFIG_STORE_HISTORY

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"


static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
            .max_files = 4,
            .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount FATFS (%s)", esp_err_to_name(err));
        return;
    }
}
#endif // CONFIG_STORE_HISTORY


#ifdef B527
static void Init_Lights(void)
{
    // Configure LIGHT1 as output
    gpio_config_t io_conf1 =
    {
        .pin_bit_mask = 1ULL << LIGHT1,  // bit mask for pin 4
        .mode = GPIO_MODE_OUTPUT,        // set as output mode
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf1);

    // Configure LIGHT2 as output
    gpio_config_t io_conf2 =
    {
        .pin_bit_mask = 1ULL << LIGHT2,  // bit mask for pin 5
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf2);
    gpio_set_level(LIGHT1, 0);  // turn LIGHT1 OFF
     gpio_set_level(LIGHT2, 0);  // turn LIGHT2 OFF
}
#endif


static void initialize_nvs(void) {
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
}


static void initialize_console(void){
	/* Drain stdout before reconfiguring it */
	fflush(stdout);
	fsync(fileno(stdout));

	/* Disable buffering on stdin */
	setvbuf(stdin, NULL, _IONBF, 0);
#if !CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
	/* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
	esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_CR);
	/* Move the caret to the beginning of the next line on '\n' */
	esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_CRLF);

	/* Tell VFS to use UART driver */
	esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);
#endif
	Debug_Uart_Init(0,0);

	/* Initialize the console */
	esp_console_config_t console_config = { .max_cmdline_args = 8,
			//.max_cmdline_length = 256,
			.max_cmdline_length = COMMAND_LEN,	//512,  //280 //
	#if CONFIG_LOG_COLORS
			.hint_color = atoi(LOG_COLOR_CYAN)
	#endif
			};
//	ESP_ERROR_CHECK(esp_console_init(&console_config));

	/* Configure linenoise line completion library */
	/* Enable multiline editing. If not set, long commands will scroll within
	 * single line.
	 */
	linenoiseSetMultiLine(1);

	/* Tell linenoise where to get command completions and hints */
	linenoiseSetCompletionCallback(&esp_console_get_completion);
	linenoiseSetHintsCallback((linenoiseHintsCallback*) &esp_console_get_hint);

	/* Set command history size */
	linenoiseHistorySetMaxLen(100);

	/* Set command maximum length */
	linenoiseSetMaxLineLen(console_config.max_cmdline_length);

	/* Don't return empty lines */
	linenoiseAllowEmpty(false);

#if CONFIG_STORE_HISTORY
    /* Load command history from filesystem */
    linenoiseHistoryLoad(HISTORY_PATH);
#endif

}//	initialize_console

time_t tOfDay = 0;
#ifndef B527
static void initialize_rtc(void)
{
	Hardware_RevH_flag = false;
	if (pcf8563_init_desc(&dev, I2C_NUM, SDA_GPIO_PIN, SCL_GPIO_PIN) != ESP_OK) {
		return;
	}
	pcf8563_set_device_handle(&dev);
	pcf8563_enable_100th_second_mode(&dev);

	struct tm time1 = {0};
	struct tm time2 = {0};
	uint8_t cs1 = 0;
	uint8_t cs2 = 0;

	// Step 1: Wait for a centisecond rollover
	if (pcf8563_get_time_ms(&dev, &time1, &cs1) == ESP_OK)
	{
		Hardware_RevH_flag = true;
		int64_t start_us = esp_timer_get_time();
		const int64_t timeout_us = 500000; // 500ms timeout

		do {
			pcf8563_get_time_ms(&dev, &time2, &cs2);
		} while (cs2 == cs1 && (esp_timer_get_time() - start_us) < timeout_us);
	} 

	// Step 2: Now read time again to set system time as close as possible to cs2 start
	time2.tm_year -= 1900;
	time_t tOfDay = mktime(&time2);

	// Calculate the exact time to set
	struct timeval currentTime;
	currentTime.tv_sec = tOfDay;
	currentTime.tv_usec = cs2 * 10000;  // 1 cs = 10ms = 10,000us

	// Step 3: Set system time
	settimeofday(&currentTime, NULL);

	get_current_rtc_time_ms(1);
}
#endif

const char *prompt;    //= LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;


// How long to sleep before "reboot-like-power-on"
#define SLEEP_TIME_SEC   3      // deep sleep duration
#define PREPARE_DELAY_MS 5000   // delay before entering deep sleep (demo only)

static void check_reset_reason()
{
	uint32_t reset_reason_raw = REG_READ(RTC_CNTL_RESET_STATE_REG);
	unsigned int CPU0_Reset_Reason = 0;
	unsigned int CPU1_Reset_Reason = 0;
	CPU0_Reset_Reason = (unsigned int)(reset_reason_raw & 0x0000003F);
	CPU1_Reset_Reason = (unsigned int)(reset_reason_raw & 0x00000FC0)>>6;

	esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

	printf("\n Entering deep sleep. CPU0_Reset_Reason = %d, CPU1_Reset_Reason = %d\n", CPU0_Reset_Reason,CPU1_Reset_Reason);

	if((CPU0_Reset_Reason != ESP_RST_PANIC)  	|| 	(CPU1_Reset_Reason != ESP_RST_PANIC) 	||
	   (CPU0_Reset_Reason != ESP_RST_INT_WDT)  	|| 	(CPU1_Reset_Reason != ESP_RST_INT_WDT) 	||
	   (CPU0_Reset_Reason != ESP_RST_TASK_WDT)  || 	(CPU1_Reset_Reason != ESP_RST_TASK_WDT) ||
	   (CPU0_Reset_Reason != ESP_RST_WDT)  		|| 	(CPU1_Reset_Reason != ESP_RST_WDT))
	{
		printf("CPU reset is not due to panic/WDT. \n");
		return;
	}
	if (cause == ESP_SLEEP_WAKEUP_TIMER)
	{
		printf("ESP reset is not due to panic. \n");
		return;
	}
	else
		printf("Not a deep sleep reset\n");

	vTaskDelay(1000 / portTICK_PERIOD_MS);
    // enter deep sleep
    esp_deep_sleep_start();
}

static void example_deep_sleep_register_rtc_timer_wakeup(void)
{
    const int wakeup_time_sec = 20;
    printf("Enabling timer wakeup, %ds\n", wakeup_time_sec);
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup(wakeup_time_sec * 1000000));
}

void app_main(void) {
	example_deep_sleep_register_rtc_timer_wakeup();
	check_reset_reason();
	set_custom_memory_hooks();
	initialize_nvs();

#if defined(B527)
	Init_Lights();
	gpio_set_level(LIGHT1, 0);  // turn LIGHT1 OFF
	gpio_set_level(LIGHT2,0);  // turn LIGHT1 OFF
#endif

#ifndef B527
	initialize_rtc();
#endif

#if CONFIG_STORE_HISTORY
    initialize_filesystem();
    printf("\n Command history enabled");
#else
    printf("\n Command history disabled");
#endif

#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG

				USB_Handle = xTaskCreateStatic(
								usb_console_task,                 // Task function
								"USB Monitor",            // Task name
								UART_TASK_STACK_DEPTH,        // Stack size in words
								NULL,                    // Task parameters (not used here)
								UART_TASK_PRIORITY,                       // Task priority
								xUSBTaskStack,              // Pointer to task stack (allocated in PSRAM)
								&xUSBTaskBuffer             // Pointer to task control block
			   );

		if (USB_Handle == NULL) {
			    printf("Failed to create USB task\n");

			}
#endif			
	initialize_console();
	watchdog_Init();
	Console_Initialize();

	/* Register commands */
	esp_console_register_help_command();
}

static void watchdogMonitoredTask(void *pvParameter)
{
    // Register the task with the Task Watchdog Timer (TWDT)
    esp_task_wdt_add(NULL); // 'NULL' adds the current task

    // Check if the task is being monitored
       esp_err_t status = esp_task_wdt_status(NULL);
       if (status == ESP_OK) {
#ifdef ENABLE_PRINT_MSG
           printf("Task is being monitored by TWDT\n");
#endif
       } else {
#ifdef ENABLE_PRINT_MSG
           printf("Task is not being monitored by TWDT: %s\n", esp_err_to_name(status));
#endif
       }

    while (1) {
        // Perform task operations
        // ...

        // Reset the task watchdog timer
        esp_task_wdt_reset();

        // Delay for some time (example: 1 second)
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#define WATCHDOG_TIMEOUT_MS (15*1000)
static void watchdog_Init()
{
#if !CONFIG_ESP_TASK_WDT_INIT
	 // Configure the Task Watchdog Timer
	    //TWDT_TIMEOUT_MS
	    esp_task_wdt_config_t wdt_config = {
	            .timeout_ms = WATCHDOG_TIMEOUT_MS,
	            .idle_core_mask = 0x03, 	//  Monitor both idle tasks (core 0 and core 1)   (1 << portNUM_PROCESSORS) - 1,    // Bitmask of all cores
	            .trigger_panic = true, 	// Trigger a panic if a task times out
				//.timeout_reset = true, 		// Reset chip if a task times out
	        };
	    // Initialize the Task Watchdog Timer (TWDT)
	    esp_err_t ret = esp_task_wdt_init(&wdt_config);
	    if (ret != ESP_OK) {
	        printf("Failed to initialize Task Watchdog Timer %d\n",ret);
	    }
#endif // CONFIG_ESP_TASK_WDT_INIT
	    watchdogTaskHandle = xTaskCreateStatic(
	        watchdogMonitoredTask,      // Task function
	        "WATCHDOG_TASK",            // Task name
	        WATCHDOG_TASK_STACK_SIZE,   // Stack size in words
	        NULL,                       // Task parameters (not used here)
	        WATCHDOG_TASK_PRIORITY,     // Task priority
	        watchdogTaskStack,          // Pointer to task stack (allocated in PSRAM)
	        &watchdogTaskControlBlock   // Pointer to task control block
	    );

	    if (watchdogTaskHandle == NULL) {
	#ifdef ENABLE_PRINT_MSG
	        printf("Failed to create task\n");
	#endif
	        // Handle error
	    }
}


void *custom_malloc(size_t sz)
{
    // Call heap_caps_malloc with your desired capabilities
    return heap_caps_malloc(sz, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}

void custom_free(void *ptr)
{
    // Use heap_caps_free to free the memory
    heap_caps_free(ptr);
}

void set_custom_memory_hooks() {
    memory_hooks.malloc_fn = custom_malloc;
    memory_hooks.free_fn = custom_free;
    cJSON_InitHooks(&memory_hooks);
}

/*
  This disables all the watchdogs for when we call the gdbstub.
*/
void disable_all_watchdogs(void)
{
    wdt_hal_context_t wdt0_context = {.inst = WDT_MWDT0, .mwdt_dev = &TIMERG0};
#if SOC_TIMER_GROUPS >= 2
    wdt_hal_context_t wdt1_context = {.inst = WDT_MWDT1, .mwdt_dev = &TIMERG1};
#endif

    //Todo: Refactor to use Interrupt or Task Watchdog API, and a system level WDT context
    //Task WDT is the Main Watchdog Timer of Timer Group 0
    wdt_hal_write_protect_disable(&wdt0_context);
    wdt_hal_disable(&wdt0_context);
    wdt_hal_write_protect_enable(&wdt0_context);

#if SOC_TIMER_GROUPS >= 2
    //Interupt WDT is the Main Watchdog Timer of Timer Group 1
    wdt_hal_write_protect_disable(&wdt1_context);
    wdt_hal_disable(&wdt1_context);
    wdt_hal_write_protect_enable(&wdt1_context);
#endif
}


void reconfig_watchdogs(uint32_t timeout_ms)
{
    wdt_hal_context_t wdt0_context = {.inst = WDT_MWDT0, .mwdt_dev = &TIMERG0};
#if SOC_TIMER_GROUPS >= 2
	// IDF-3825
    wdt_hal_context_t wdt1_context = {.inst = WDT_MWDT1, .mwdt_dev = &TIMERG1};
#endif

    //Todo: Refactor to use Interrupt or Task Watchdog API, and a system level WDT context
    wdt_hal_init(&wdt0_context, WDT_MWDT0, MWDT_LL_DEFAULT_CLK_PRESCALER, false); //Prescaler: wdt counts in ticks of TG0_WDT_TICK_US
    wdt_hal_write_protect_disable(&wdt0_context);
    wdt_hal_config_stage(&wdt0_context, 0, timeout_ms * 1000 / MWDT_DEFAULT_TICKS_PER_US, WDT_STAGE_ACTION_RESET_SYSTEM); //1 second before reset
    wdt_hal_enable(&wdt0_context);
    wdt_hal_write_protect_enable(&wdt0_context);

#if SOC_TIMER_GROUPS >= 2
    //Disable IWDT (Timer Group 1)
    wdt_hal_write_protect_disable(&wdt1_context);
    wdt_hal_disable(&wdt1_context);
    wdt_hal_write_protect_enable(&wdt1_context);
#endif
}

static void sendLedState(const char *stateName, int duration) {
	char payLoadData[100] = {0};
    // Create a new JSON object
    cJSON *responseObject = cJSON_CreateObject();

    // Add stateName to the JSON object
    cJSON_AddStringToObject(responseObject, "stateName", stateName);

    // Add duration to the JSON object
    cJSON_AddNumberToObject(responseObject, "duration", duration);

    // Print the JSON object as a string
	cJSON_PrintPreallocated(responseObject, payLoadData, sizeof(payLoadData), false);
    // Send the JSON string to the other actor
    Send_CMD_To_Other_Actor(LED, "LED", payLoadData, strlen(payLoadData), "SETSTATE");

    // Free the allocated memory
    cJSON_Delete(responseObject);
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
	strcpy((char*) s_Message_Tx_new.src_Actor_a8	, "CONSOLE");
	strcpy((char*) s_Message_Tx_new.dest_Actor_a8	, DestActor);
	strcpy((char*) s_Message_Tx_new.cmdFun_a8		, function);
//	console_ActorWriteToConsole_xface( &s_Message_Tx_new);
	console_que_sel(&s_Message_Tx_new);
}

void Restart_ESP_Xface(char BLUE_LED)
{
	if(BLUE_LED == 1)
	{
		sendLedState("REBOOT_MODE",-1);
	}
	FS_Unmount("nor:0:");   //unmount JFS
    Send_CMD_To_Other_Actor(SYS_FILES, "SYS_FILES","\0",0,"UNMOUNT");  //Unmount SPIFFS volume 2
 	Send_CMD_To_Other_Actor(SPIFFS, "SPIFFS","\0",0,"UNMOUNT");   //Unmount SPIFFS volume 1
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	disable_all_watchdogs();
	reconfig_watchdogs(200);
	while(1);
}
#if CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
PSRAM_ATTR_BSS static char Command[COMMAND_LEN];
static void usb_console_task(void *param)
{
    char line[256] = {0};
    while (1) {
        if (fgets(line, sizeof(line), stdin)) {
        	line[strcspn(line, "\r\n")] = 0;  // Strip newline
            strcat(Command, line);  // Append this chunk to full command
            // Check if the command looks complete (e.g., ends in ')')
            if ((strchr(Command, ')') != NULL) && (strchr(Command, '<') != NULL)) {
                // Process the full command
                int ret;
                esp_err_t err = esp_console_run_Custom(Command, &ret, "CONSOLE");
                if (err != 0)
                    printf("\n esp_console_run_Custom err = %d \n", err);

                // Clear buffer after processing
                Command[0] = 0;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
}
#endif
