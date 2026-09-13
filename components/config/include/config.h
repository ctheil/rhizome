
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

typedef struct
{
  bool enabled;
  uint8_t echo_pin;
  uint8_t trigger_pin;
  uint16_t reservoir_depth_cm;
} reservoir_config_t;

typedef struct
{
  bool enabled;
  uint8_t in1_pin;
  uint8_t in2_pin;
  uint64_t max_run_time_ms;
} pump_config_t;

typedef struct
{
  reservoir_config_t reservoir;
  pump_config_t pumps[2];
} config_t;

#endif /* __CONFIG_H__ */