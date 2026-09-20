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
  // int reads[BURST_COUNT];
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

SemaphoreHandle_t sensor_mutex;
uint8_t cached_percentage = 0;
uint32_t last_read_tick = UINT32_MAX;
#define STALE_THRESHOLD 10000
static uint32_t raw_reading;
static uint32_t reads_sum = 0;
static uint32_t reads_count = 0;
static uint8_t data_pin = 1;
static uint16_t dry_reading = 0;
static uint16_t wet_reading = 0;

uint8_t classify(uint16_t raw_reading) {
  return 100;
}
uint8_t moisture_sensor_get_level(uint8_t pin) 
{
  uint16_t result;
  xSemaphoreTake(sensor_mutex, portMAX_DELAY);
  uint32_t now = xTaskGetTickCount();
  if (now - last_read_tick > STALE_THRESHOLD) {
    level = RESERVOIR_STALE;
  } else {
    uint16_t avg = reads_sum / reads_count;
    reads_sum = reads_count = 0; // reset
    level = classify(avg);
    ESP_LOGI(TAG, "external request for level. last_read: %d; avg: %d, moisture_percentage: %s", raw_reading, avg, perc);
  } 
  xSemaphoreGive(sensor_mutex);
  return level;
}

#define POLL_COUNT 15
static void poll_sensor(void) {
    
    uint16_t reads[POLL_COUNT];
    uint16_t sum = 0;
    int result;
    for (int i = 0; i < POLL_COUNT; i++) {
      esp_err_t ret = analog_read(data_pin, &result);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to read sensor, skipping");
        continue;
      }
      reads[i] = result;
      sum += result;
      reads_sum += result;
      reads_count++;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    uint16_t avg = sum / POLL_COUNT;
    uint16_t sd = calc_sd(reads, POLL_COUNT);
    // if (sd >= 20) {
    //   ESP_LOGW(TAG, "Standard deviation above acceptable level: %d; skipping result", sd);
    //   return;
    // }
    if (avg >= max_pressure + 100 || avg <= dry_pressure - 100)  {
      ESP_LOGD(TAG, "result from poll is over under range; skipping; avg: %d, low: %d, high: %d", avg, dry_pressure, max_pressure);
      return;
    }

    xSemaphoreTake(sensor_mutex, portMAX_DELAY);
    last_read_tick = xTaskGetTickCount();
    onboard_sensor_raw = avg;

    cached_level = classify(avg);
    xSemaphoreGive(sensor_mutex);
    ESP_LOGV(TAG, "raw reading: %d from %d; Standard Deviation: %d;", onboard_sensor_raw, last_read_tick, sd);
}

static void sensor_poll_task(void *arg) 
{
  while (1)
  {
    poll_sensor();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  vTaskDelete(NULL);
}

void reservoir_sensor_init(uint8_t use_onboard_sensor, uint8_t data_pin, uint16_t _dry_pressure, uint16_t _max_pressure) 
{
  ESP_LOGD(TAG, "global init pressure transducer reservoir sensor");
  sensor_mutex = xSemaphoreCreateMutex();

  if (use_onboard_sensor) {
  max_pressure = _max_pressure;
  dry_pressure = _dry_pressure;
  is_initialized = 1;
  cached_level = RESERVOIR_UNKNOWN;

  xTaskCreate(sensor_poll_task, "pressure_transducer_reservoir_sensor_poll_task", 2048, NULL, 0, NULL);
  }
}