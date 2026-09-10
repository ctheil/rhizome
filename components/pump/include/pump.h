#ifndef __PUMP_H__
#define __PUMP_H__
#include "stdint.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define PUMP_TAG "pump"

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
    pump_state_t state;
    uint64_t start_time_ms;
    uint32_t pre_water_moisture;   // for pump-without-effect detection
    bool override_active;
    uint64_t override_expiry_ms;
    pump_fault_reason_t fault;

    uint8_t mosfet_pin;
    uint8_t in1_pin;
    uint8_t in2_pin;
} channel_runtime_t;
typedef struct {
  uint8_t ch;
  uint32_t ttl_ms;
} MANUAL_START;

typedef struct {
  uint8_t ch;
} MANUAL_STOP;


typedef struct {
  uint8_t power_mode;
  uint32_t timeout_ms;
  uint8_t ch;
} pump_queue_msg_t;
typedef struct {
  uint8_t ain1;
  uint8_t ain2;
  uint8_t mosfet_pin;
} pump_t;

typedef enum {
  DIR_FORWARD, 
  DIR_REVERSE
} direction_t;


extern channel_runtime_t **PUMP_CHANNELS;
extern uint8_t NUM_CHANNELS;

esp_err_t pump_init(channel_runtime_t **pumps_arr, uint8_t size);

esp_err_t pump_drive(channel_runtime_t *pump, direction_t dir);
esp_err_t pump_off(channel_runtime_t *pump);


#endif