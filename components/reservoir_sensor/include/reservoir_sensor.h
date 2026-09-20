#ifndef __RESERVOIR_SENSOR_H__
#define __RESERVOIR_SENSOR_H__
#pragma once

#include <esp_err.h>
#include <stdint.h>


#define ESP_ERR_RS_PING 0x200
#define ESP_ERR_RS_PING_TIMEOUT 0x201
#define ESP_ERR_RS_ECHO_TIMEOUT 0x202

typedef struct 
{
  int trigger_pin;
  int echo_pin;
  uint32_t res_depth_cm;
} reservoir_sensor_t;


typedef enum {
  RS_FAULT_NONE, 
  RS_FAULT_UNKNOWN,
  RS_FAULT_PING_TIMEOUT,
  RS_FAULT_PING, 
  RS_FAULT_ECHO_TIMEOUT,
} reservoir_fault_reason_t;
typedef struct {
  uint32_t raw_response_time;
  uint32_t distance_cm;
  uint32_t reservoir_depth_cm; 
  bool is_empty;
  reservoir_fault_reason_t fault;
} reservoir_status_t;

esp_err_t rs_init(reservoir_sensor_t *dev);
uint8_t rs_test(const reservoir_sensor_t *dev);

/**
 * @brief  Measure time between ping and echo
 * 
 * @param dev  Pointer to the device
 * @param max_time_us Max time to wait for echo
 * @param time_us Time, us
 * @return `ESP_OK` on success; otherwise: 
 *           - ::ESP_ERR_RESERVOIR_PING         - Invalid state (prev ping is not ended)
 *           - ::ESP_ERR_RESERVOIR_PING_TIMEOUT - Device not responding
 *           - ::ESP_ERR_RESERVOIR_ECHO_TIMEOUT - Distance too large or wave is scattered
 */
esp_err_t rs_measure_raw(const reservoir_sensor_t *dev, uint32_t max_time_us, uint32_t *time_us);

/**
 * @brief  Measure cm
 * 
 * @param dev  Pointer to the device
 * @param max_time_us Max time to wait for echo
 * @param time_us Time, us
 * @return `ESP_OK` on success; otherwise: 
 *           - ::ESP_ERR_ULTRASONIC_PING         - Invalid state (prev ping is not ended)
 *           - ::ESP_ERR_ULTRASONIC_PING_TIMEOUT - Device not responding
 *           - ::ESP_ERR_ULTRASONIC_ECHO_TIMEOUT - Distance too large or wave is scattered
 */
esp_err_t rs_measure_cm(const reservoir_sensor_t *dev, uint32_t max_distance, uint32_t *distance_cm);

/**
 * @brief  Measure mm
 * 
 * @param dev  Pointer to the device
 * @param max_time_us Max time to wait for echo
 * @param time_us Time, us
 * @return `ESP_OK` on success; otherwise: 
 *           - ::ESP_ERR_ULTRASONIC_PING         - Invalid state (prev ping is not ended)
 *           - ::ESP_ERR_ULTRASONIC_PING_TIMEOUT - Device not responding
 *           - ::ESP_ERR_ULTRASONIC_ECHO_TIMEOUT - Distance too large or wave is scattered
 */
esp_err_t rs_measure_mm(const reservoir_sensor_t *dev, uint32_t max_distance, uint32_t *distance_mm);

/**
 * @brief  Measure cm
 * 
 * @param dev  Pointer to the device
 * @param max_time_us Max time to wait for echo
 * @param time_us Time, us
 * @return `ESP_OK` on success; otherwise: 
 *           - ::ESP_ERR_ULTRASONIC_PING         - Invalid state (prev ping is not ended)
 *           - ::ESP_ERR_ULTRASONIC_PING_TIMEOUT - Device not responding
 *           - ::ESP_ERR_ULTRASONIC_ECHO_TIMEOUT - Distance too large or wave is scattered
 */
esp_err_t rs_measure_cm(const reservoir_sensor_t *dev, uint32_t max_distance, uint32_t *distance);


/**
 * @brief  Get Reservoir fill percent
 * 
 * @param dev  Pointer to the device
 * @param percent Fill Percent
 * @return `ESP_OK` on success; otherwise: 
 *           - ::ESP_ERR_ULTRASONIC_PING         - Invalid state (prev ping is not ended)
 *           - ::ESP_ERR_ULTRASONIC_PING_TIMEOUT - Device not responding
 *           - ::ESP_ERR_ULTRASONIC_ECHO_TIMEOUT - Distance too large or wave is scattered
 */
esp_err_t rs_get_res_fill_percent(const reservoir_sensor_t *dev, uint8_t *percent);


void reservoir_get_status(reservoir_status_t *status);



typedef enum {
  RESERVOIR_UNKNOWN,
  RESERVOIR_OK,
  RESERVOIR_STALE,
  RESERVOIR_EMPTY,
  RESERVOIR_FAULT_UNKNOWN,
} reservoir_level_t;

char* reservoir_level_to_name(reservoir_level_t level);

reservoir_level_t reservoir_sensor_get_level(void);
void reservoir_sensor_init(uint8_t use_onboard_sensor, uint8_t data_pin, uint16_t dry_pressure, uint16_t full_pressure);



#endif