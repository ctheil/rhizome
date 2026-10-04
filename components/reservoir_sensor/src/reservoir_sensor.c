#include "reservoir_sensor.h"
#include "freertos/FreeRTOS.h"
#include <esp_log.h>
#include <stdint.h>
#include "analog_read.h"
#include "math.h"
#include "router.h"
#include "transport.h"
#include "cJSON.h"
#include "soc/clk_tree_defs.h"
#include <sys/time.h>

#define TAG "reservoir_sensor"

SemaphoreHandle_t sensor_mutex;
reservoir_level_t cached_level = RESERVOIR_UNKNOWN;
char* last_ota_update = NULL;
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
    ESP_LOGD(TAG, "returning cached ota level");
    return cached_level;
  }

  reservoir_level_t level;
  uint32_t avg = 0;
  xSemaphoreTake(sensor_mutex, portMAX_DELAY);
  uint32_t now = xTaskGetTickCount();
  if (now - last_read_tick > STALE_THRESHOLD) {
    level = RESERVOIR_STALE;
  } else {
    avg = reads_sum / reads_count;
    reads_sum = reads_count = 0; // reset
    level = classify(avg);
  } 
  xSemaphoreGive(sensor_mutex);
  ESP_LOGD(TAG, "external request for level. last_read: %d; avg: %d, level: %s", onboard_sensor_raw, avg, reservoir_level_to_name(level));
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

void publish_rs_level() {
  cJSON *root = cJSON_CreateObject();

  cJSON_AddNumberToObject(root, "level", cached_level);
  struct timeval tv_now;
  if (gettimeofday(&tv_now, NULL) == 0) {
      int64_t time_us = (int64_t)tv_now.tv_sec * 1000000L + (int64_t)tv_now.tv_usec;
      cJSON_AddNumberToObject(root, "timestamp", time_us);
  }

  char *payload = cJSON_Print(root);
  if (payload != NULL) {
    ESP_LOGD(TAG, "publishing ota reservoir level");
    transport_publish("rhizome/g01/reservoir-level", payload, 0, 1, 1);
    cJSON_free(payload);
  } else {
    ESP_LOGE(TAG, "failed to marshal json");
  }
  cJSON_Delete(root);
}

static void sensor_poll_task(void *arg) 
{
  while (1)
  {
    poll_sensor();
    publish_rs_level();
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
  vTaskDelete(NULL);
}

void ota_rs_cb(char* payload, uint16_t len) {
  ESP_LOGD(TAG, "oat reservoir sensor update: %s", payload);

  char json_string[len];
  strncpy(json_string, payload, len);

  cJSON *root = cJSON_Parse(json_string);
  if (root == NULL) {
    ESP_LOGE(TAG, "ota_rs_cb: failed to parse payload to json");
    return;
  }

  cJSON *timestamp = cJSON_GetObjectItem(root, "timestamp");
  if (cJSON_IsString(timestamp) && (timestamp->valuestring != NULL)) {
    ESP_LOGD(TAG, "ota update timestamp: %s", timestamp->valuestring);
    last_ota_update = timestamp->valuestring;
  }

  cJSON *level = cJSON_GetObjectItem(root, "level");
  if (cJSON_IsNumber(level)) {
    ESP_LOGD(TAG, "ota update level: %s", reservoir_level_to_name(level->valueint));
    cached_level = level->valueint;
  }

  cJSON_Delete(root);
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
  } else {
    ESP_LOGD(TAG, "no onboard sensor. registering ota callback");
    register_subscription("rhizome/g01/reservoir-sensor", 1, ota_rs_cb);
  }
}
