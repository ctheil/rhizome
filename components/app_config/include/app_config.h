
#ifndef __APP_CONFIG_H__
#define __APP_CONFIG_H__

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "pump.h"

/**
 * @brief v1 ultrasonic reservoir sensor config
 * 
 */
typedef struct
{
  uint8_t enabled;
  uint8_t echo_pin;
  uint8_t trigger_pin;
  uint16_t reservoir_depth_cm;
} reservoir_config_t;

/**
 * @brief v2 pressure transducer sensor config
 * 
 */
typedef struct
{
  uint8_t enabled;
  uint8_t data_pin;
  uint16_t dry_pressure;
  uint16_t max_reservoir_pressure;
} reservoir_pressure_transducer_t;

typedef struct 
{
  uint8_t enabled;
  uint8_t pump_in1_pin;
  uint8_t pump_in2_pin;
  uint64_t max_pump_run_time_ms;

  uint8_t ms_sensor_pin;
  uint16_t ms_dry_reading;
  uint16_t ms_wet_reading;
} channel_config_t;

#define MAX_CHANNELS 2

typedef struct
{
  uint8_t schema_version;
  uint8_t channel_count;
  reservoir_config_t reservoir;
  reservoir_pressure_transducer_t pt_reservoir;
  channel_config_t channels[MAX_CHANNELS];
  uint8_t mosfet_pin;
  char *wifi_ssid;
  char *wifi_password;
} config_t;


/*
 {
  "schema_version": 1,
  "channel_count": 1,
  "reservoir": {
    "enabled": 1, 
    "echo_pin": 1, 
    "trigger_pin": 2, 
    "reservoir_depth_cm": 23
  },
  "channels": [
    {
      "enabled": 1, 
      "pump_in1_pin": 20, 
      "pump_in2_pin": 21,
      "pump_max_run_time_ms": 6000,
      "ms_sensor_pin": 8
    }
  ], 
  "mosfet_pin": 10
}
*/

// esp_err_t get_default_config(config_t *cfg);
esp_err_t get_config(config_t *cfg);

#endif /* __APP_CONFIG_H__ */