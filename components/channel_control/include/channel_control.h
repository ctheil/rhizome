#ifndef __CHANNEL_CONTROL_H__
#define __CHANNEL_CONTROL_H__

#include "pump.h"
#include "app_config.h"

#pragma once

extern uint8_t NUM_CHANNELS;

typedef struct {
  uint8_t power_mode;
  uint32_t timeout_ms;
  uint8_t ch;
} channel_queue_msg_t;
typedef struct {
  uint16_t id;
} profile_t;
typedef struct
{
  uint64_t start_time_ms;
  uint32_t pre_water_moisture; // for pump-without-effect detection
  bool override_active;
  uint64_t override_expiry_ms;
  pump_state_t state;
  pump_fault_reason_t fault;
  pump_t pump;
  uint8_t mosfet_pin;

  uint8_t ms_sensor_pin;
  uint16_t ms_wet_reading;
  uint16_t ms_dry_reading;
  uint8_t profile_id;

  SemaphoreHandle_t mu;

} channel_runtime_t;

esp_err_t init_control_task(config_t *cfg);

#endif /*__CHANNEL_CONTROL_H__*/