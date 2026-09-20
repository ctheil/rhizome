
#include "app_config.h"
#include "channel_control.h"
#include "esp_log.h"
#include "esp_log_level.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump.h"
#include "reservoir_sensor.h"
#include "wifi.h"
#include "nvs_flash.h"
#include "app_config.h"
#include "stdlib.h"

#define TAG "main"


void app_main(void)
{
    esp_log_level_set("*", ESP_LOG_INFO);
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

    // ESP_LOGI(TAG, "initializing wifi");
    // ESP_ERROR_CHECK(wifi_init());
    // ret = wifi_connect(app_cfg.wifi_ssid, app_cfg.wifi_password);
    // if (ret != ESP_OK) {
    //     ESP_LOGE(TAG, "failed to initialize wifi...");
    //     return;
    // }

    ESP_LOGI(TAG, "initializing pumps");
    for (int i = 0; i < app_cfg.channel_count; i++) {
        pump_t p = {
            .in1_pin = app_cfg.channels[i].pump_in1_pin,
            .in2_pin = app_cfg.channels[i].pump_in2_pin,
        };
        ret = pump_init(app_cfg.mosfet_pin, &p);
        if (ret != ESP_OK){
            ESP_LOGE(TAG, "failed to initialize pump: %s", esp_err_to_name(ret));
            return;
        }
    }

    ESP_LOGI(TAG, "stating channel control task");
    esp_log_level_set("channel_control", ESP_LOG_VERBOSE);
    // esp_log_level_set("reservoir_sensor", ESP_LOG_VERBOSE);
    ret = init_control_task(&app_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to init control task");
        return ;
    }
    while(1) {vTaskDelay(pdMS_TO_TICKS(500));}
}
