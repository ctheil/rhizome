#include "moisture_sensor.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "hal/adc_types.h"
#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


#define MS_TAG "moisture_sensor"
//ADC Channels


void ms_get_reading(uint8_t channel, int *result) 
{
  int reads[BURST_COUNT];
  read_adc(channel, result);
  // burst_read_adc(channel, reads);

  // *result = calc_sd(reads, BURST_COUNT);
}

void burst_read_adc(uint8_t channel, int *reads_arr) 
{
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

adc_oneshot_chan_cfg_t config = {
  .bitwidth = ADC_BITWIDTH_DEFAULT, 
  .atten = ADC_ATTEN_DB_12,
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &config));

for (int i = 0; i < BURST_COUNT; i++) {
ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, &reads_arr[i]));
    vTaskDelay(10/portTICK_PERIOD_MS);
}

// RELEASE
ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}
void read_adc(uint8_t channel, int *result) 
{
  adc_oneshot_unit_handle_t adc1_handle;
  adc_oneshot_unit_init_cfg_t init_config1 = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
  };

ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));

adc_oneshot_chan_cfg_t config = {
  .bitwidth = ADC_BITWIDTH_DEFAULT, 
  .atten = ADC_ATTEN_DB_12,
};
ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, channel, &config));

ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, channel, result));

// RELEASE
ESP_ERROR_CHECK(adc_oneshot_del_unit(adc1_handle));
}

int calc_sd(int *reads_arr, int size) {
  int total = 0; 
  for (int i = 0; i < size; i++) {
    total += reads_arr[i];
  }
  int avg = total / size;

  int sq_sum = 0;
  for (int i = 0; i < size; i++) {
    int diff = reads_arr[i] - avg;
    sq_sum += diff * diff;
  }

  int quotient = sq_sum / (size - 1);
  return sqrt(quotient);
}

