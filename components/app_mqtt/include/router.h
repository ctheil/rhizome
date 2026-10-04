#ifndef __ROUTER_H__
#define __ROUTER_H__

#include <stdint.h>
#include "esp_err.h"

#pragma once

typedef struct {
  char* topic;
  uint8_t qos;
  void* handler;
} registration_object_t;

typedef enum {
  EVENT_CONNECTED,
  EVENT_DISCONNECTED,
  EVENT_MESSAGE,
} event_type_t;

typedef struct {
  event_type_t event_type; 
  char* topic;
  uint8_t topic_len;
  char* payload;
  uint16_t payload_len;
} Sink_Message_t;

typedef void (*event_cb)(char* payload, uint16_t len);

esp_err_t register_subscription(char* topic, uint8_t qos, event_cb handler);
esp_err_t router_sink(Sink_Message_t event);
void router_init(void);
void enqueue_message();

#endif // __ROUTER_H__