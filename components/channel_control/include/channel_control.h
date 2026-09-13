#ifndef __CHANNEL_CONTROL_H__
#define __CHANNEL_CONTROL_H__

#include "pump.h"

#pragma once

typedef struct
{
  uint64_t start_time_ms;
  uint32_t pre_water_moisture; // for pump-without-effect detection
  bool override_active;
  uint64_t override_expiry_ms;
  pump_state_t state;
  pump_fault_reason_t fault;
  pump_t pump;

  uint8_t ms_sensor_pin;
  uint8_t profile_id;

} channel_runtime_t;

#endif /*__CHANNEL_CONTROL_H__*/