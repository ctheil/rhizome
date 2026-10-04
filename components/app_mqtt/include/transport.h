#ifndef __TRANSPORT_H__
#define __TRANSPORT_H__

#include "app_config.h"
#include "router.h"

void transport_init(esp_err_t (*sink)(Sink_Message_t));
void transport_subscribe(char *topic, uint8_t qos);
void mqtt_app_start(config_t *cfg);
void transport_publish(char *topic, char* data, int len, int qos, int retain);

#endif