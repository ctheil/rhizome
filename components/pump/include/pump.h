#ifndef __PUMP_H__
#define __PUMP_H__
#include "stdint.h"
#include "esp_err.h"

typedef struct {
  uint8_t ain1;
  uint8_t ain2;
  uint8_t mosfet_pin;
} pump_t;

typedef enum {
  DIR_FORWARD, 
  DIR_REVERSE
} direction_t;

esp_err_t pump_init(pump_t *pump);

esp_err_t pump_drive(pump_t *pump, direction_t dir, uint8_t speed);
esp_err_t pump_off(pump_t *pump);


#endif