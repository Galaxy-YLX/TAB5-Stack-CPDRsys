#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "Tab5.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Tab5 RS485 Test starting...");

    // Initialize your Tab5 component
    func();

    while (1) {
        ESP_LOGI(TAG, "Hello from Tab5 RS485 Test!");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
