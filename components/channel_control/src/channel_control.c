#include "channel_control.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "pump.h"
#include "reservoir_sensor.h"
#include "app_config.h"

#define TAG "channel_control"
static QueueHandle_t msg_queue;
static const uint8_t msg_queue_len = 4;
channel_runtime_t **CHANNELS = NULL;
uint8_t NUM_CHANNELS = 0;


static void all_pumps_off(pump_fault_reason_t fault_reason) {
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        mosfet_off(CHANNELS[i]->mosfet_pin);
        pump_off(CHANNELS[i]->pump);
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
      if (msg->power_mode == 0) {
        ESP_LOGI(TAG, "received pump stop command");
        esp_err_t ret = pump_off(ch->pump);
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

        pump_drive(ch->pump, DIR_FORWARD);
        CHANNELS[msg->ch]->state = PUMP_WATERING;
      }
      return ESP_OK;
}

static reservoir_fault_reason_t reservoir_handler() 
{
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
    return rs_status.fault;
}

static void vChannelControlTask(void *arg) 
{
  channel_queue_msg_t msg; // queue item
  int to_wait_ms = 1000;
  const TickType_t xTicksToWait = pdMS_TO_TICKS(to_wait_ms);

  while (1) {
    ESP_LOGD(TAG, "running channel control task");

    // first check reservoir status and continue if !OK
    reservoir_fault_reason_t rs_fault = reservoir_handler();
    if (rs_fault != RS_FAULT_NONE) {
      ESP_LOGE(TAG, "reservoir fault. cannot proceed");
      continue;
    }

    // handle message
    esp_err_t err;
    if (xQueueReceive(msg_queue, (void *)&msg, xTicksToWait) == pdTRUE) {
      err = controlTaskMsgHandler(&msg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to handle messag");
        continue;
      }
    }

    for (int i = 0; i < NUM_CHANNELS; i++) {
      if (CHANNELS[i]->state == PUMP_IDLE) {
        ESP_LOGD(TAG, "channels pump is off, continue");
        continue;
      }

      TickType_t current_time_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
      if (CHANNELS[i]->override_expiry_ms >= current_time_ms) {
        ESP_LOGI(TAG, "pump hit runtime expiry. turning off");
        pump_off(CHANNELS[i]->pump);
        CHANNELS[i]->state = PUMP_IDLE;
      }
    }

    // TODO: setup profiles, check moisture status against profile, control pump based
    // poll moisture sensor per channel (one per pump) for adequate moisture given the channel's profile


    vTaskDelay(pdMS_TO_TICKS(to_wait_ms));
  }

  vTaskDelete(NULL);
}

esp_err_t init_control_task(config_t *cfg) {
  msg_queue = xQueueCreate(msg_queue_len, sizeof(channel_queue_msg_t));
  if (msg_queue != NULL) {
    xTaskCreate(vChannelControlTask, "channel_control_task", 2048, NULL, 0, NULL);
  }

  NUM_CHANNELS = cfg->channel_count;
  CHANNELS = (channel_runtime_t **)malloc(sizeof(channel_runtime_t *) * NUM_CHANNELS);
  if (CHANNELS == NULL) {
    ESP_LOGE(TAG, "failed to allocate pump channels array memory");
    return ESP_FAIL;
  }

  for (int i = 0; i < NUM_CHANNELS; i++) {
    pump_t pump = {
      .in1_pin = cfg->channels[i].pump_in1_pin,
      .in2_pin = cfg->channels[i].pump_in2_pin
    };
    channel_runtime_t ch = {
      .ms_sensor_pin = cfg->channels[i].ms_sensor_pin,
      .pump = &pump,
      .mosfet_pin = cfg->mosfet_pin
    };
    CHANNELS[i] = &ch;
  }

  return ESP_OK;
}
