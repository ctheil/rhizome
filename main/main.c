
#include "app_config.h"
#include "esp_log.h"
#include "esp_log_level.h"
// #include "moisture_sensor.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump.h"
#include "reservoir_sensor.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "app_config.h"

#define TAG "main"


void app_main(void)
{
    // init NVS
    ESP_LOGI(TAG, "initializing NVS");
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    config_t app_cfg;
    ret = get_config(&app_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get config...");
        return;
    }

    ESP_LOGI(TAG, "initializing wifi");
    ESP_ERROR_CHECK(wifi_init());
    ret = wifi_connect(app_cfg.wifi_ssid, app_cfg.wifi_password);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize wifi...");
        return;
    }

    if (app_cfg.reservoir.enabled) {

    ESP_LOGI(TAG, "initializing reservoir sensor");
    reservoir_sensor_t reservoir_sensor = {
        .trigger_pin = app_cfg.reservoir.trigger_pin,
        .echo_pin = app_cfg.reservoir.echo_pin,
        .res_depth_cm = app_cfg.reservoir.reservoir_depth_cm
    };
    ret = rs_init(&reservoir_sensor);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to initialize reservoir sensor...");
        return;
    }
    }

    ESP_LOGI(TAG, "initializing pump");
    channel_runtime_t pump = {
        .in1_pin =app_cfg.channels[0].pump_in1_pin,
        .in2_pin = app_cfg.channels[0].pump_in2_pin,
        .mosfet_pin = app_cfg.mosfet_pin,
    };
    channel_runtime_t *pumps[1] = {&pump};
    ret = pump_init(pumps, 1);
    if (ret != ESP_OK){
        ESP_LOGE(TAG, "failed to initialize pump...");
        return;
    }

    while (1) {

        // uint32_t distance_mm;
        // ret = rs_measure_mm(&reservoir_sensor, 300, &distance_mm);
        // if (ret != ESP_OK) {
        //             ESP_LOGE("main", "failed to get rs measurement");
        //     switch (ret) {
        //         case ESP_ERR_RS_ECHO_TIMEOUT: 
        //             ESP_LOGE("main", "RS ERR: echo timeout");
        //             break;
        //         case ESP_ERR_RS_PING_TIMEOUT: 
        //             ESP_LOGE("main", "RS ERR: ping timeout");
        //             break;
        //         case ESP_ERR_RS_PING: 
        //             ESP_LOGE("main", "RS ERR: ping err");
        //             break;
        //         default: 
        //             ESP_LOGE("main", "unknown error");
        //             break;

        //     }
        //     ESP_LOGE("main", "failed to get rs measurement");
        // } else {
        //     ESP_LOGI("main", "distance to water: %umm", distance_mm );
        //     uint32_t distance_from_top_of_res = (reservoir_sensor.res_depth_cm * 10) - distance_mm;
        //     ESP_LOGI("main", "distance from top: %umm", distance_from_top_of_res );
        // }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
