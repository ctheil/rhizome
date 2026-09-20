#ifndef __PUMP_H__
#define __PUMP_H__
#include "stdint.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


typedef enum { PUMP_IDLE, PUMP_WATERING, PUMP_FAULT } pump_state_t;
typedef enum {
  FAULT_NONE,
  FAULT_UNKNOWN,
  FAULT_RESERVOIR_DRY, 
  FAULT_RESERVOIR_FAULT,
  FAULT_PUMP_WITHOUT_EFFECT,
  FAULT_SENSOR_OUT_OF_RANGE,
} pump_fault_reason_t;

typedef struct {
    uint8_t in1_pin;
    uint8_t in2_pin;
} pump_t;


typedef enum {
  DIR_FORWARD, 
  DIR_REVERS
} direction_t;



esp_err_t pump_init(uint8_t mosfet_pin, pump_t *pump);

esp_err_t pump_drive(pump_t *pump);
esp_err_t pump_off(pump_t *pump);
esp_err_t pump_test(pump_t *pump);
esp_err_t mosfet_on(uint8_t pin);
esp_err_t mosfet_off(uint8_t pin);


#endif