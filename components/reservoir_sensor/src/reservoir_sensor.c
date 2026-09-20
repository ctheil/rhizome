#include "reservoir_sensor.h"
#include "freertos/FreeRTOS.h"
#include <esp_log.h>
#include <stdint.h>
#include "analog_read.h"
#include "math.h"

#define TAG "reservoir_sensor"

SemaphoreHandle_t sensor_mutex;
reservoir_level_t cached_level = RESERVOIR_UNKNOWN;
uint32_t last_read_tick = UINT32_MAX;
#define STALE_THRESHOLD 10000
static uint32_t onboard_sensor_raw;
static uint32_t reads_sum = 0;
static uint32_t reads_count = 0;
static uint8_t is_initialized = 0;
static uint8_t data_pin = 0;
static uint16_t dry_pressure = 0;
static uint16_t max_pressure = 0;

char* reservoir_level_to_name(reservoir_level_t level) {
  switch (level) {
    case RESERVOIR_OK: 
      return "RESERVOIR_OK";
    case RESERVOIR_EMPTY: 
      return "RESERVOIR_EMPTY";
    case RESERVOIR_STALE: 
      return "RESERVOIR_STALE";
    case RESERVOIR_FAULT_UNKNOWN: 
      return "RESERVOIR_FAULT_UNKNOWN";
    default: 
      return "RESERVOIR_UNKNOWN";
  }
}

uint8_t is_within_jitter(int x, int y, int jitter) 
{
  int high = y + jitter;
  int low = y - jitter;
  return x >= low && x <= high;
}

reservoir_level_t classify(int raw_reading) {
  if (raw_reading < 5 || raw_reading > INT_MAX) {
    ESP_LOGW(TAG, "result is below or above sensor thresholds: %d", raw_reading);
    return RESERVOIR_UNKNOWN;
  }
  int jitter = 5;
  if (raw_reading <= (dry_pressure + jitter)) {
    ESP_LOGD(TAG, "sensor read points to dry reservoir: raw %d; air_pressure: %d; jitter: %d", raw_reading, dry_pressure, jitter);
    return RESERVOIR_EMPTY;
  }
  if (raw_reading >= max_pressure) {
    ESP_LOGW(TAG, "result is over max_pressure");
    return RESERVOIR_UNKNOWN;
  }
    ESP_LOGD(TAG, "sensor read points to RESERVOIR_OK: %d", raw_reading);
  return RESERVOIR_OK;
}


reservoir_level_t reservoir_sensor_get_level(void) 
{
  if (!is_initialized) {
    ESP_LOGE(TAG, "No onbaord sensor. Cannot determine reservoir sensor");
    return RESERVOIR_UNKNOWN;
  }

  reservoir_level_t level;
  xSemaphoreTake(sensor_mutex, portMAX_DELAY);
  uint32_t now = xTaskGetTickCount();
  if (now - last_read_tick > STALE_THRESHOLD) {
    level = RESERVOIR_STALE;
  } else {
    uint32_t avg = reads_sum / reads_count;
    reads_sum = reads_count = 0; // reset
    level = classify(avg);
    ESP_LOGI(TAG, "external request for level. last_read: %d; avg: %d, level: %s", onboard_sensor_raw, avg, reservoir_level_to_name(level));
    // level = cached_level;
  } 
  xSemaphoreGive(sensor_mutex);
  return level;
}


uint16_t calc_sd(uint16_t *reads_arr, int size) {
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
