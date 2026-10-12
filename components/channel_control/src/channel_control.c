#include "channel_control.h"
#include "analog_read.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "portmacro.h"
#include "pump.h"
#include "reservoir_sensor.h"
#include "app_config.h"
#include "led.h"
#include "cJSON.h"
#include "transport.h"

#define TAG "channel_control"
static QueueHandle_t msg_queue;
static const uint8_t msg_queue_len = 4;
static channel_runtime_t **CHANNELS = NULL;
uint8_t NUM_CHANNELS = 0;

void publish_ch_status(channel_runtime_t *ch, uint8_t id, char *error) {
  cJSON *root = cJSON_CreateObject();

  cJSON_AddNumberToObject(root, "start_time_ms", ch->start_time_ms);
  cJSON_AddNumberToObject(root, "pre_water_dry_percentage", ch->pre_water_dry);
  cJSON_AddNumberToObject(root, "override_active", ch->override_active);
  cJSON_AddNumberToObject(root, "override_expiry_ms", ch->override_expiry_ms);
  cJSON_AddNumberToObject(root, "pump_state", ch->state);
  cJSON_AddNumberToObject(root, "pump_fault", ch->fault);
  cJSON_AddNumberToObject(root, "moisture_sensor_dry_percentage", ch->ms_dry_percentage);
  cJSON_AddNumberToObject(root, "moisture_sensor_raw_reading", ch->ms_raw_reading);

  cJSON_AddStringToObject(root, "profile_name", ch->profile->name);

  cJSON_AddStringToObject(root, "error", error);

  if (strcmp(error, "") != 0) {
    ESP_LOGE(TAG, "publishing channel status with error: %s", error);
  }

  char *payload = cJSON_Print(root);
  if (payload != NULL) {
    char topic[35] = "rhizome/g01/channel-state/";
    uint8_t curr_len = strlen(topic);
    snprintf(topic + curr_len, sizeof(topic) - curr_len, "%d", id);
    ESP_LOGD(TAG, "publishing ota channel state to topic: %s", topic);
    transport_publish(topic, payload, 0, 1, 1);
    cJSON_free(payload);
  } else {
    ESP_LOGE(TAG, "failed to marshal json");
  }
  cJSON_Delete(root);
}

void handle_fault(channel_runtime_t *ch, pump_fault_reason_t reason) 
{
  ch->fault = reason;
  ESP_LOGW(TAG, "handling fault: %s... triggering fault led", pump_fault_to_str(reason));
  led_send(LED_FAULT, LED_BLINK, 250);
}

void clear_fault(channel_runtime_t *ch) 
{
  ch->fault = FAULT_NONE;
  led_send(LED_FAULT, LED_OFF, 0);
}

static void all_pumps_off(pump_fault_reason_t fault_reason) {
      for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        handle_fault(CHANNELS[i], fault_reason);
        if (CHANNELS[i]->state != PUMP_WATERING) {
          continue;
        }
        mosfet_off(CHANNELS[i]->mosfet_pin);
        pump_off(&CHANNELS[i]->pump);
        publish_ch_status(CHANNELS[i], i, pump_fault_to_str(fault_reason));
        led_send(LED_WATERING, LED_OFF, 0);
        CHANNELS[i]->state = PUMP_FAULT;
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
          handle_fault(ch, FAULT_UNKNOWN);
          return ret;
        }
        clear_fault(ch);
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
        handle_fault(ch, FAULT_UNKNOWN);
        ch->state = FAULT_UNKNOWN;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      ch->pre_water_dry = read_to_percentage(avg, ch->ms_wet_reading, ch->ms_dry_reading);
      ret = pump_drive(&ch->pump);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to drive pump: %s", esp_err_to_name(ret));
        handle_fault(ch, PUMP_FAULT);
        ch->state = PUMP_FAULT;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      ch->state = PUMP_WATERING;
      ch->start_time_ms = xTaskGetTickCount();
      ch->override_expiry_ms = 1200000; // 20 minutes
      xSemaphoreGive(ch->mu);
      led_send(LED_WATERING, LED_BLINK, 500);
      return ESP_OK;
    case PUMP_IDLE: 
      ret = pump_off(&ch->pump);
      xSemaphoreTake(ch->mu, portMAX_DELAY);
      if (ret != ESP_OK) {
        ESP_LOGE(TAG, "failed to turn pump off: %s", esp_err_to_name(ret));
        handle_fault(ch, FAULT_UNKNOWN);
        ch->state = FAULT_UNKNOWN;
        xSemaphoreGive(ch->mu);
        return ret;
      }
      clear_fault(ch);
      ch->state = PUMP_IDLE;
      ch->start_time_ms = 0;
      xSemaphoreGive(ch->mu);
      led_send(LED_WATERING, LED_OFF, 0);
      return ESP_OK;
    default: 
      ESP_LOGW(TAG, "cannot change pump state to fault");
      return ESP_ERR_INVALID_ARG;
  }
}

int to_wait_ms = 2000;
#define task_continue {vTaskDelay(pdMS_TO_TICKS(to_wait_ms)); continue;}

static void vChannelControlTask(void *arg) 
{
  channel_queue_msg_t msg; // queue item
  const TickType_t xTicksToWait = pdMS_TO_TICKS(to_wait_ms);

  while (1) {
    // first check reservoir status and continue if !OK
    reservoir_level_t level = reservoir_handler();
    if (level != RESERVOIR_OK) {
      ESP_LOGE(TAG, "reservoir fault. cannot proceed: %s", reservoir_level_to_name(level));
      all_pumps_off(RESERVOIR_EMPTY);
      task_continue;
    }
    

    // handle message
    esp_err_t err;
    if (xQueueReceive(msg_queue, (void *)&msg, 0) == pdTRUE) {
      err = controlTaskMsgHandler(&msg);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to handle message");
        task_continue;
      }
    }

    // // TODO: setup profiles, check moisture status against profile, control pump based
    // // poll moisture sensor per channel (one per pump) for adequate moisture given the channel's profile
    // loop over each channel and check in on state
    TickType_t now = xTaskGetTickCount();
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
      if (CHANNELS[i]->fault == FAULT_PUMP_WITHOUT_EFFECT) {
        ESP_LOGW(TAG, "channel %d has uncleared PUMP_WITHOUT_EFFECT_FAULT. skipping.", i);
        continue;
      }

      esp_err_t err;
      uint32_t avg;
      err = analog_burst_read(CHANNELS[i]->ms_sensor_pin, &avg, 15, 10);
      if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to read moisture sensor: %s", esp_err_to_name(err));
        publish_ch_status(CHANNELS[i], i, "failed to read moisture sensor");
        continue;
      }
      xSemaphoreTake(CHANNELS[i]->mu, portMAX_DELAY);
      uint8_t dry_percentage = read_to_percentage(avg, CHANNELS[i]->ms_wet_reading, CHANNELS[i]->ms_dry_reading);
      clear_fault(CHANNELS[i]);
      CHANNELS[i]->ms_dry_percentage = dry_percentage;
      CHANNELS[i]->ms_raw_reading = avg;
      ESP_LOGD(TAG, "moisture sensor burst read: %d; dry percentage: %d", avg, dry_percentage);

      pump_state_t state = CHANNELS[i]->state;
      // TODO: check pump_without_effect
      TickType_t start = CHANNELS[i]->start_time_ms;
      uint8_t pre_water_dry_perc = CHANNELS[i]->pre_water_dry;
      xSemaphoreGive(CHANNELS[i]->mu);


      // check time elapsed and cmp pre-water-moisture to current - fault water without effect 
      if (state == PUMP_WATERING && (now - start) >= pdMS_TO_TICKS(CHANNELS[i]->profile->pump_without_effect_ms)) {
        ESP_LOGE(TAG, "detected pump without effect fault after %dms", CHANNELS[i]->profile->pump_without_effect_ms);
        err = change_pump_state(CHANNELS[i], PUMP_WATERING);
        handle_fault(CHANNELS[i], FAULT_PUMP_WITHOUT_EFFECT);
        publish_ch_status(CHANNELS[i], i, "PUMP_WITHOUT_EFFECT_FAULT");
        if (err != ESP_OK) {
          pump_off(&CHANNELS[i]->pump);
          ESP_LOGE(TAG, "failed to halt pump: %s", esp_err_to_name(err));
        }
        continue;
      } else if (dry_percentage >= CHANNELS[i]->profile->dry_trigger_threshold && state != PUMP_WATERING) {
        ESP_LOGI(TAG, "channel is %d%% dry above ch profile dry trigger threshold: %d%%; starting pump", dry_percentage, CHANNELS[i]->profile->dry_trigger_threshold);
        err = change_pump_state(CHANNELS[i], PUMP_WATERING);
        if (err != ESP_OK) {
          pump_off(&CHANNELS[i]->pump);
          ESP_LOGE(TAG, "failed to drive pump on dry moisture reading: %s", esp_err_to_name(err));
          publish_ch_status(CHANNELS[i], i, "failed to drive pump on dry moisture reading");
          continue;
        }
      } else if (state == PUMP_WATERING && dry_percentage <= CHANNELS[i]->profile->wet_trigger_threshold) {
        ESP_LOGI(TAG, "channel is %d%% dry below ch profile wet trigger threshold; stopping pump", dry_percentage, CHANNELS[i]->profile->wet_trigger_threshold);
        err = change_pump_state(CHANNELS[i], PUMP_IDLE);
        if (err != ESP_OK) {
          ESP_LOGE(TAG, "failed to turn off pump on adequate moisture reading: %s", esp_err_to_name(err));
          publish_ch_status(CHANNELS[i], i, "failed to turn off pump on adequate moisture reading");
          continue;
        }
      }
      publish_ch_status(CHANNELS[i], i, "");
    }
    vTaskDelay(pdMS_TO_TICKS(to_wait_ms));
  }

  vTaskDelete(NULL);
}

esp_err_t init_control_task(config_t *cfg) {

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
  CHANNELS[i]->profile = cfg->channels[i].profile;
  CHANNELS[i]->mosfet_pin = cfg->mosfet_pin;
  CHANNELS[i]->mu = xSemaphoreCreateMutex();
  }

  msg_queue = xQueueCreate(msg_queue_len, sizeof(channel_queue_msg_t));
  if (msg_queue != NULL) {
    xTaskCreate(vChannelControlTask, "channel_control_task", 4096, NULL, 0, NULL);
  }

  return ESP_OK;
}
