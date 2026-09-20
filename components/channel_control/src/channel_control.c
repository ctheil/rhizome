#include "channel_control.h"
#include "analog_read.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump.h"
#include "reservoir_sensor.h"
#include "app_config.h"

#define TAG "channel_control"
static QueueHandle_t msg_queue;
static const uint8_t msg_queue_len = 4;
static channel_runtime_t **CHANNELS = NULL;
uint8_t NUM_CHANNELS = 0;


static void all_pumps_off(pump_fault_reason_t fault_reason) {
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        if (CHANNELS[i]->state != PUMP_WATERING) {
          continue;
        }
        mosfet_off(CHANNELS[i]->mosfet_pin);
        pump_off(&CHANNELS[i]->pump);
        CHANNELS[i]->fault = fault_reason;
        CHANNELS[i]->state = PUMP_FAULT;
        // fallback and turn off via gpio manually?
      }
}

static esp_err_t controlTaskMsgHandler(channel_queue_msg_t *msg) 
{
      ESP_LOGI(TAG, "received msg = %d", msg);
      if (msg->ch >= NUM_CHANNELS) {
        ESP_LOGE(TAG, "requested channel is OOB");
        return ESP_ERR_INVALID_SIZE;
      }
      channel_runtime_t *ch = CHANNELS[msg->ch];
      if (ch->fault != FAULT_NONE) {
        ESP_LOGE(TAG, "cannot handle message: requested channel has fault.");
        return ESP_FAIL;
      }
      if (msg->power_mode == 0 && CHANNELS[msg->ch]->state == PUMP_WATERING) {
        ESP_LOGI(TAG, "received pump stop command");
        esp_err_t ret = pump_off(&ch->pump);
        CHANNELS[msg->ch]->state = PUMP_IDLE;
        if (ret != ESP_OK) {
          ESP_LOGE(TAG, "failed to stop pump");
          ch->fault = FAULT_UNKNOWN;
          return ret;
        }
        ch->fault = FAULT_NONE;
      } else if (msg->power_mode == 1) {
        ESP_LOGI(TAG, "received pump start command");
        if (msg->timeout_ms < 1) {
          ESP_LOGE(TAG, "timeout cannot be less than 1");
          return ESP_ERR_INVALID_ARG;
        }

        TickType_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        ch->start_time_ms = current_time_ms;
        ch->override_expiry_ms = current_time_ms + msg->timeout_ms;

        pump_drive(&ch->pump);
        CHANNELS[msg->ch]->state = PUMP_WATERING;
      }
      return ESP_OK;
}

reservoir_level_t reservoir_handler() 
{
    reservoir_level_t level = reservoir_sensor_get_level();
    ESP_LOGD(TAG, "reservoir level returned: %s", reservoir_level_to_name(level));
    if (level != RESERVOIR_OK && level != RESERVOIR_EMPTY) {
      // TURN ALL PUMPS OFF
      all_pumps_off(FAULT_RESERVOIR_FAULT);
    } else if (level == RESERVOIR_EMPTY) {
      all_pumps_off(FAULT_RESERVOIR_DRY);
    }
    return level;
}

esp_err_t change_pump_state(channel_runtime_t *ch, pump_state_t new_state)
{
  if (!(ch)) {
    return ESP_ERR_INVALID_ARG;
  }
  if (ch->state == new_state) {
    ESP_LOGW(TAG, "pump is already at state: %d", new_state);
    return ESP_OK;
  }
  esp_err_t ret;
  switch (new_state) {
    case PUMP_WATERING: 
      uint32_t avg;
      ret = analog_burst_read(ch->ms_sensor_pin, &avg, 10, 10);
      xSemaphoreTake(ch->mu, portMAX_DELAY);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to get pre water moisture state: %s", esp_err_to_name(ret));
        ch->fault = FAULT_UNKNOWN;
        ch->state = FAULT_UNKNOWN;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      ch->pre_water_moisture = read_to_percentage(avg, ch->ms_wet_reading, ch->ms_dry_reading);
      ret = pump_drive(&ch->pump);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to drive pump: %s", esp_err_to_name(ret));
        ch->fault = PUMP_FAULT;
        ch->state = PUMP_FAULT;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      ch->state = PUMP_WATERING;
      ch->start_time_ms = xTaskGetTickCount();
      ch->override_expiry_ms = 1200000; // 20 minutes
      xSemaphoreGive(ch->mu);
      return ESP_OK;
    case PUMP_IDLE: 
      ret = pump_off(&ch->pump);
      xSemaphoreTake(ch->mu, portMAX_DELAY);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to turn pump off: %s", esp_err_to_name(ret));
        ch->fault = FAULT_UNKNOWN;
        ch->state = FAULT_UNKNOWN;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      ch->fault=FAULT_NONE;
      ch->state = PUMP_IDLE;
      ch->start_time_ms = 0;
      xSemaphoreGive(ch->mu);
      return ESP_OK;
    default: 
      ESP_LOGW(TAG, "cannot change pump state to fault");
      return ESP_ERR_INVALID_ARG;
  }
}

static void vChannelControlTask(void *arg) 
{
  channel_queue_msg_t msg; // queue item
  int to_wait_ms = 2000;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(to_wait_ms);

  while (1) {
    // first check reservoir status and continue if !OK
    reservoir_level_t level = reservoir_handler();
    if (level != RESERVOIR_OK) {
      ESP_LOGE(TAG, "reservoir fault. cannot proceed");
      all_pumps_off(RESERVOIR_EMPTY);
    }
    

    // handle message
    esp_err_t err;
    if (xQueueReceive(msg_queue, (void *)&msg, xTicksToWait) == pdTRUE) {
      err = controlTaskMsgHandler(&msg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to handle message");
        vTaskDelay(pdMS_TO_TICKS(to_wait_ms));
        continue;
      }
    }

    // // TODO: setup profiles, check moisture status against profile, control pump based
    // // poll moisture sensor per channel (one per pump) for adequate moisture given the channel's profile
    // loop over each channel and check in on state
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      xSemaphoreTake(CHANNELS[i]->mu, portMAX_DELAY);
      uint64_t override_expiry_ms = CHANNELS[i]->override_expiry_ms;
      pump_state_t state = CHANNELS[i]->state;
      xSemaphoreGive(CHANNELS[i]->mu);
      TickType_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if(current_time_ms >= override_expiry_ms && state == PUMP_WATERING)
      {
        ESP_LOGI(TAG, "pump hit runtime expiry. turning off");
        change_pump_state(CHANNELS[i], PUMP_IDLE);
      }

      esp_err_t err;
      uint32_t avg;
      err = analog_burst_read(CHANNELS[i]->ms_sensor_pin, &avg, 15, 10);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to read moisture sensor: %s", esp_err_to_name(err));
        return;
      }
      uint8_t dry_percentage = read_to_percentage(avg, CHANNELS[i]->ms_wet_reading, CHANNELS[i]->ms_dry_reading);
      ESP_LOGD(TAG, "moisture sensor burst read: %d; dry percentage: %d", avg, dry_percentage);

      xSemaphoreTake(CHANNELS[i]->mu, portMAX_DELAY);
      state = CHANNELS[i]->state;
      xSemaphoreGive(CHANNELS[i]->mu);

      if (dry_percentage >= 50 && state != PUMP_WATERING) {
        ESP_LOGI(TAG, "channel is %d%% dry; starting pump", dry_percentage);
        err = change_pump_state(CHANNELS[i], PUMP_WATERING);
        if (err != ESP_OK) {
          pump_off(&CHANNELS[i]->pump);
          ESP_LOGE(TAG, "failed to drive pump on dry moisture reading: %s", esp_err_to_name(err));
          return;
        }
      } else if (state == PUMP_WATERING && dry_percentage <= 25) {
        ESP_LOGI(TAG, "channel is %d%% dry; stopping pump", dry_percentage);
        err = change_pump_state(CHANNELS[i], PUMP_IDLE);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "failed to turn off pump on adequate moisture reading: %s", esp_err_to_name(err));
          return;
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(to_wait_ms));
  }

  vTaskDelete(NULL);
}

esp_err_t init_control_task(config_t *cfg) {

  reservoir_sensor_init(cfg->pt_reservoir.enabled, cfg->pt_reservoir.data_pin, cfg->pt_reservoir.dry_pressure, cfg->pt_reservoir.max_reservoir_pressure);

  NUM_CHANNELS = cfg->channel_count;
  CHANNELS = calloc(NUM_CHANNELS, sizeof(*CHANNELS));
  if (CHANNELS == NULL) {
    ESP_LOGE(TAG, "failed to allocate pump channels array memory");
    return ESP_ERR_NO_MEM;
  }

  for (int i = 0; i < NUM_CHANNELS; i++) {
    CHANNELS[i] = calloc(1, sizeof(*CHANNELS[i]));
  if (CHANNELS[i] == NULL) {
    ESP_LOGE(TAG, "failed to allocate pump channel memory");
    return ESP_ERR_NO_MEM;
  }
  CHANNELS[i]->ms_sensor_pin = cfg->channels[i].ms_sensor_pin;
  CHANNELS[i]->ms_wet_reading = cfg->channels[i].ms_wet_reading;
  CHANNELS[i]->ms_dry_reading = cfg->channels[i].ms_dry_reading;
  CHANNELS[i]->pump.in1_pin = cfg->channels[i].pump_in1_pin;
  CHANNELS[i]->pump.in2_pin = cfg->channels[i].pump_in2_pin;
  CHANNELS[i]->mosfet_pin = cfg->mosfet_pin;
  CHANNELS[i]->mu = xSemaphoreCreateMutex();
  }

  msg_queue = xQueueCreate(msg_queue_len, sizeof(channel_queue_msg_t));
  if (msg_queue != NULL) {
    xTaskCreate(vChannelControlTask, "channel_control_task", 4096, NULL, 0, NULL);
  }

  return ESP_OK;
}
