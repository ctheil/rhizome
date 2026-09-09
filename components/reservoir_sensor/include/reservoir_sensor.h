#ifndef __RESERVOIR_SENSOR_H__
#define __RESERVOIR_SENSOR_H__

#include <esp_err.h>
#include <stdint.h>

#define TAG "reservoir_sensor"

#define ESP_ERR_RS_PING 0x200
#define ESP_ERR_RS_PING_TIMEOUT 0x201
#define ESP_ERR_RS_ECHO_TIMEOUT 0x202

typedef struct 
{
  int trigger_pin;
  int echo_pin;
  uint32_t res_depth_cm;
} reservoir_sensor_t;

esp_err_t rs_init(const reservoir_sensor_t *dev);
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



#endif