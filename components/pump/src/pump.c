
#include "pump.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "esp_log.h"
#include "reservoir_sensor.h"

#define TAG "pump"


channel_runtime_t **PUMP_CHANNELS = NULL;
uint8_t NUM_CHANNELS = 0;

static QueueHandle_t msg_queue;
static const uint8_t msg_queue_len = 4;


static void all_pumps_off(pump_fault_reason_t fault_reason) {
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        pump_off(PUMP_CHANNELS[i]);
        PUMP_CHANNELS[i]->fault = fault_reason;
        // fallback and turn off via gpio manually?
      }
}

static void vPumpControlTask(void *arg) 
{
  pump_queue_msg_t msg; // queue item
  int to_wait_ms = 1000;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(to_wait_ms);

  while (1) {
    ESP_LOGI(TAG, "running pump control task");
    if (xQueueReceive(msg_queue, (void *)&msg, xTicksToWait) == pdTRUE) {
      ESP_LOGI(TAG, "received msg = %d", msg);
      if (msg.ch >= NUM_CHANNELS) {
        ESP_LOGE(TAG, "requested channel is OOB");
        continue;
      }
      channel_runtime_t *pump = PUMP_CHANNELS[msg.ch];
      if (pump->fault != FAULT_NONE) continue; // how to handle better? Exit 1?
      if (msg.power_mode == 0) {
        ESP_LOGI(TAG, "received pump stop command");
        esp_err_t ret = pump_off(pump);
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "failed to stop pump");
          pump->fault = FAULT_UNKNOWN;
          continue;
        }
        pump->fault = FAULT_NONE;
      } else if (msg.power_mode == 1) {
        ESP_LOGI(TAG, "received pump start command");
        if (msg.timeout_ms < 1) {
          ESP_LOGE(TAG, "timeout cannot be less than 1");
          continue;
        }

        TickType_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        pump->start_time_ms = current_time_ms;
        pump->override_expiry_ms = current_time_ms + msg.timeout_ms;

        pump_drive(pump, DIR_FORWARD);
      }
    }

    reservoir_status_t rs_status;
    reservoir_get_status(&rs_status);
    if (rs_status.fault != RS_FAULT_NONE) {
      switch (rs_status.fault) {
        case RS_FAULT_ECHO_TIMEOUT:
          ESP_LOGE(TAG, "reservoir fault echo timeout");
          break;
        case RS_FAULT_PING_TIMEOUT:
          ESP_LOGE(TAG, "reservoir fault ping timeout");
          break;
        case RS_FAULT_PING:
          ESP_LOGE(TAG, "reservoir fault ping error");
          break;
        default:
          ESP_LOGE(TAG, "reservoir fault unknown");
          break;
      }
      // TURN ALL PUMPS OFF
      all_pumps_off(FAULT_RESERVOIR_FAULT);
    }
    if (rs_status.is_empty) {
      all_pumps_off(FAULT_RESERVOIR_DRY);
    }
    // poll moisture sensor per channel (one per pump) for adequate moisture given the channel's profile
    // check for fault/stop 

    vTaskDelay(pdMS_TO_TICKS(to_wait_ms));
  }

  vTaskDelete(NULL);
}

esp_err_t pump_drive(channel_runtime_t *pump, direction_t dir) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  uint8_t a1_level = 0;
  uint8_t a2_level =1;
  if (dir == DIR_REVERSE) {
    a1_level = 1;
    a2_level=0;
  }

  gpio_set_level(pump->in1_pin, a1_level);
  gpio_set_level(pump->in2_pin, a2_level);
  gpio_set_level(pump->mosfet_pin, 1);
  pump->state = PUMP_WATERING;
  return ESP_OK;
}

esp_err_t pump_off(channel_runtime_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  gpio_set_level(pump->mosfet_pin, 0);
  gpio_set_level(pump->in1_pin, 0);
  gpio_set_level(pump->in2_pin, 0);
  pump->state = PUMP_IDLE;

  return ESP_OK;
}

esp_err_t pump_init(channel_runtime_t **pumps_arr, uint8_t size) 
{
  if (!(pumps_arr)) return ESP_ERR_INVALID_ARG;

  msg_queue = xQueueCreate(msg_queue_len, sizeof(pump_queue_msg_t));
  if (msg_queue != NULL) {
    xTaskCreate(vPumpControlTask, "pump_control_task", 2048, NULL, 0, NULL);
  }

  NUM_CHANNELS = size;
  *PUMP_CHANNELS = malloc(sizeof(channel_runtime_t) * NUM_CHANNELS);

  for (uint8_t i = 0; i < size; i++) {
    PUMP_CHANNELS[i] = pumps_arr[i];

  gpio_config_t mosfet_conf = {
        .pin_bit_mask = (1ULL << pumps_arr[i]->mosfet_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&mosfet_conf);

  gpio_config_t in1_conf = {
        .pin_bit_mask = (1ULL << pumps_arr[i]->in1_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&in1_conf);

  gpio_config_t in2_conf = {
        .pin_bit_mask = (1ULL << pumps_arr[i]->in2_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&in2_conf);
  }

  return ESP_OK;
}