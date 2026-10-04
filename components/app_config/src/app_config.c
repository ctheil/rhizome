#include "app_config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "router.h"

#define TAG "config"
#define PART "nvs"
#define STORAGE_NAMESPACE "storage"


esp_err_t get_config_from_nvs(config_t *cfg, nvs_handle_t handle) {
  esp_err_t err;

  ESP_LOGI(TAG, "reading config blob");
  size_t cfg_size = sizeof(*cfg);
  
  err = nvs_get_blob(handle, "node_config", cfg, &cfg_size);
  if (err != ESP_OK || err != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "failed to get config from storage: %s", esp_err_to_name(err));
    return err;
  }
  return err;
}


esp_err_t write_default_config(config_t *cfg, nvs_handle_t handle) {
  esp_err_t err;

  cfg->schema_version = 1;
  cfg->channel_count = 1;
  reservoir_config_t rs = {
    .enabled = 0, 
  };
  cfg->reservoir = rs;
  reservoir_pressure_transducer_t pt_rs = {
    .enabled = 1, 
    .data_pin = 0, 
    .dry_pressure = 495, 
    .max_reservoir_pressure = 530
  };
  cfg->pt_reservoir = pt_rs;
  channel_config_t chan = {
   .enabled = 1, 
   .max_pump_run_time_ms = 1000 * 60 * 30, // 30 minutes
   .pump_in1_pin = 9, 
   .pump_in2_pin = 8, 
   .ms_sensor_pin = 1, 
   .ms_dry_reading = 3875, 
   .ms_wet_reading = 1430, 
  };
  cfg->channels[0] = chan;
  cfg->mosfet_pin = 10;
  cfg->wifi_ssid = "Brickhouse";
  cfg->wifi_password = "Br1ckHous3";
  cfg->mqtt_broker_uri =  "mqtt://192.168.0.57:1883";
  cfg->mqtt_username =  "rhizome";
  cfg->mqtt_password =  "123456789";

  return ESP_OK;
}

void ota_cfg_cb(char* payload, uint16_t len) {
  ESP_LOGI(TAG, "ota post config");
}
void init_config(void)
{
  register_subscription("rhizome/d01/post-config", 0, ota_cfg_cb);
}
esp_err_t get_config(config_t *cfg) 
{
    // Open NVS handle

  nvs_handle_t nvs_handle;
  esp_err_t ret = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &nvs_handle);
  if (ret != ESP_OK) {
      ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(ret));
      return ret;
  }

  ret = get_config_from_nvs(cfg, nvs_handle);
  if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
    ESP_LOGE(TAG, "failed to get config from nvs");
    return ret;
  }

  if (ret == ESP_OK) {
    return ret;
  }

  ESP_LOGD(TAG, "config not found. writing default");
  return write_default_config(cfg, nvs_handle);
}
