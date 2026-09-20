#include "analog_read.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "freertos/idf_additions.h"

#define TAG "analog_read"

esp_err_t analog_read(uint8_t pin, int *result) 
{
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

esp_err_t ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
if (ret != ESP_OK) {
  ESP_LOGE(TAG, "failed to init oneshot unit: %s", esp_err_to_name(ret));
  return ret;
}

adc_oneshot_chan_cfg_t config = {
  .bitwidth = ADC_BITWIDTH_DEFAULT, 
  .atten = ADC_ATTEN_DB_12,
};
ret = adc_oneshot_config_channel(adc1_handle, pin, &config);
if (ret != ESP_OK) {
  ESP_LOGE(TAG, "failed to set channel config: %s", esp_err_to_name(ret));
  return ret;
}

ret =adc_oneshot_read(adc1_handle, pin, result);
if (ret != ESP_OK) {
  ESP_LOGE(TAG, "failed to perform oneshot read: %s", esp_err_to_name(ret));
  return ret;
}

// RELEASE
ret = adc_oneshot_del_unit(adc1_handle);
if (ret != ESP_OK) {
  ESP_LOGE(TAG, "failed to release oneshot unit: %s", esp_err_to_name(ret));
  return ret;
}
return ESP_OK;

}
esp_err_t analog_burst_read(uint8_t pin, uint32_t *mean_result, uint8_t poll_count, uint16_t poll_delay)
{
  uint32_t sum = 0;
  esp_err_t ret;
  for (uint8_t i = 0; i < poll_count; i++) {
    int result;
    ret = analog_read(pin, &result);
    if (ret != ESP_OK) {
      return ret;
    }
    sum += result;
    vTaskDelay(pdMS_TO_TICKS(poll_delay));
  }

  *mean_result = sum / poll_count;
  return ESP_OK;
}

uint8_t read_to_percentage(uint16_t read, uint16_t min, uint16_t max)
{
  if (read <= min) return 0;
  if (read >= max) return 100;
return (uint8_t)(((uint32_t)(read-min) * 100) / (max - min));
}