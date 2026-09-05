
#include "esp_log.h"
#include "esp_log_level.h"
#include "moisture_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    ESP_LOGI("main", "hello world!");
    esp_log_level_set("*",  ESP_LOG_VERBOSE);


    while(1) {
    int result;
    ms_get_reading(0, &result);
    ESP_LOGI("main", "read result: %d", result);

    vTaskDelay(1000/portTICK_PERIOD_MS);
    } 
}
