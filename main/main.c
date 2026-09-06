
#include "esp_log.h"
#include "esp_log_level.h"
// #include "moisture_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "reservoir_sensor.h"

void app_main(void)
{
    ESP_LOGI("main", "hello world!");

    reservoir_sensor_t reservoir_sensor = {
        .trigger_pin = 2,
        .echo_pin = 1,
        100
    };
    rs_init(&reservoir_sensor);

    while (1) {
        float distance;
        esp_err_t res = rs_measure_cm(&reservoir_sensor, 100, &distance);
        if (res != ESP_OK) {
            ESP_LOGE("main", "failed to get rs measurement");
        } else {
            ESP_LOGI("main", "distance cm: %f", distance);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
