
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

    pump_t pump = {
        .ain1 =8,
        .ain2 = 9,
        .mosfet_pin = 10,
    };
    esp_err_t ret = pump_init(&pump);
    if (ret != ESP_OK) return;

    while (1) {
        // TEST MOSFET
        
        pump_drive(&pump, DIR_FORWARD, 255);
        // vTaskDelay(pdMS_TO_TICKS(4000));

        // pump_off(&pump);
        // vTaskDelay(pdMS_TO_TICKS(4000));

        uint8_t res_perc;
        esp_err_t ret = rs_get_res_fill_percent(&reservoir_sensor, &res_perc);
        uint32_t distance;
        rs_measure_cm(&reservoir_sensor, reservoir_sensor.res_depth_cm, &distance);
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
            ESP_LOGI("main", "distance to water: %ucm", distance );
            ESP_LOGI("main", "reservoir is %d%% full", res_perc );
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
