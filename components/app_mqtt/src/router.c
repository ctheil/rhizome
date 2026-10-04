#include "router.h"
#include <stdlib.h>
#include <stdint.h>
#include "transport.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TAG "router"
static registration_object_t* reg_table = NULL;
static uint8_t capacity = 0;
static uint8_t size = 0;

esp_err_t register_subscription(char* topic, uint8_t qos, event_cb handler) {
  if (size >= capacity) {
    // double or set to 2
    capacity = (capacity == 0) ? 2 : capacity*2;

    registration_object_t *temp = realloc(reg_table, capacity * sizeof(registration_object_t));

    if (temp == NULL) {
      free(reg_table);
      return ESP_ERR_NO_MEM;
    }

    reg_table = temp;
    ESP_LOGD(TAG, "resized capacity to %d", capacity);
  }
  registration_object_t reg = {
    .topic = topic, 
    .handler = handler,
    .qos = qos,
  };

  reg_table[size] = reg;
  size++;

  return ESP_OK;
}


QueueHandle_t x_sink_queue;
esp_err_t router_sink(Sink_Message_t event) 
{
  ESP_LOGD(TAG, "router_sink called with event %d", event.event_type);
  if (x_sink_queue == NULL) {
    ESP_LOGE(TAG, "queue is not ready");
    return ESP_ERR_INVALID_STATE;
  }

  ESP_LOGD(TAG, "sink: queueing event");
  BaseType_t x_status = xQueueSend(x_sink_queue, (void *)&event, pdMS_TO_TICKS(10));
  if (x_status != pdPASS) {
    ESP_LOGE(TAG, "failed to enqueue message");
    return ESP_FAIL;
  }

  return ESP_OK;
}

void subscribe_to_topics() 
{
  for (uint8_t i = 0; i < size; i++) {
    transport_subscribe(reg_table[i].topic, reg_table[i].qos);
  }
}

void queue_drain_task(void *pv_parameters) {
  Sink_Message_t event;

  for (;;) {
    if (xQueueReceive(x_sink_queue, &event, portMAX_DELAY) == pdPASS) {
      ESP_LOGD(TAG, "sink drain received a message %d", event.event_type);
      switch (event.event_type) {
        case EVENT_CONNECTED: 
          ESP_LOGD(TAG, "transport connected; subscribing registered table.");
          subscribe_to_topics();
          break;
        case EVENT_DISCONNECTED: 
          ESP_LOGW(TAG, "transport disconnected...");
          break;
        case EVENT_MESSAGE: 
          ESP_LOGD(TAG, "draining event message");
          for (uint8_t i = 0; i < size; i++ ) {
            if (event.topic_len == strlen(reg_table[i].topic) &&
              strncmp(event.topic, reg_table[i].topic, event.topic_len) == 0
            ) {
              if (reg_table[i].handler != NULL) {
              event_cb handler = reg_table[i].handler;
              handler(event.payload, event.payload_len);
              } else {
                ESP_LOGE(TAG, "event callback nullptr");
              }
            }
          }
          break;
      }
    }

    vTaskDelay(pdMS_TO_TICKS(500));
  }

  vTaskDelete(NULL);
}

void router_init() {
  x_sink_queue = xQueueCreate(25, sizeof(Sink_Message_t));

  if (x_sink_queue != NULL) {
    xTaskCreate(queue_drain_task, "sink_drain_task", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
  }
}