
#include "esp_log.h"
#include "esp_log_level.h"
// #include "moisture_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump.h"
#include "reservoir_sensor.h"


void app_main(void)
{
    reservoir_sensor_t reservoir_sensor = {
        .trigger_pin = 20,
        .echo_pin = 21,
        .res_depth_cm = 22
    };
    rs_init(&reservoir_sensor);

    channel_runtime_t pump = {
        .in1_pin =8,
        .in2_pin = 9,
        .mosfet_pin = 10,
    };
    channel_runtime_t *pumps[1] = {&pump};
    esp_err_t ret = pump_init(pumps, 1);

    if (ret != ESP_OK) return;

    while (1) {

        uint32_t distance_mm;
        ret = rs_measure_mm(&reservoir_sensor, 300, &distance_mm);
        if (ret != ESP_OK) {
                    ESP_LOGE("main", "failed to get rs measurement");
            switch (ret) {
                case ESP_ERR_RS_ECHO_TIMEOUT: 
                    ESP_LOGE("main", "RS ERR: echo timeout");
                    break;
                case ESP_ERR_RS_PING_TIMEOUT: 
                    ESP_LOGE("main", "RS ERR: ping timeout");
                    break;
                case ESP_ERR_RS_PING: 
                    ESP_LOGE("main", "RS ERR: ping err");
                    break;
                default: 
                    ESP_LOGE("main", "unknown error");
                    break;

            }
            ESP_LOGE("main", "failed to get rs measurement");
        } else {
            ESP_LOGI("main", "distance to water: %umm", distance_mm );
            uint32_t distance_from_top_of_res = (reservoir_sensor.res_depth_cm * 10) - distance_mm;
            ESP_LOGI("main", "distance from top: %umm", distance_from_top_of_res );
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
