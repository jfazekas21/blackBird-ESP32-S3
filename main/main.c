#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "blackbird";

void app_main(void)
{
    ESP_LOGI(TAG, "blackBird ESP32-S3 starting up");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
