#include "reservoir_sensor.h"
#include <driver/gpio.h>
#include <esp_log.h>
#include <esp32c3/rom/ets_sys.h>
#include <esp_timer.h>

#define TRIGGER_LOW_DELAY 4
#define TRIGGER_HIGH_DELAY 10
#define PING_TIMEOUT 6000
#define ROUNDTRIP_CM 58
#define timeout_expired(start, len) ((esp_timer_get_time() - (start)) >= (len))


esp_err_t rs_init(const reservoir_sensor_t *dev) 
{
  if (!(dev)) {return ESP_ERR_INVALID_ARG;}

  gpio_set_direction(dev->trigger_pin, GPIO_MODE_OUTPUT);
  gpio_set_direction(dev->echo_pin, GPIO_MODE_INPUT);

  return gpio_set_level(dev->trigger_pin, 0);
}
esp_err_t rs_measure_raw(const reservoir_sensor_t *dev, uint32_t max_time_us, uint32_t *time_us) {
  if (!(dev) || !(time_us)) {return ESP_ERR_INVALID_ARG;}

  gpio_set_level(dev->trigger_pin, 0);
  ets_delay_us(TRIGGER_LOW_DELAY);
  gpio_set_level(dev->trigger_pin, 1);
  ets_delay_us(TRIGGER_HIGH_DELAY);

  if (gpio_get_level(dev->echo_pin)) return ESP_ERR_RS_PING;

  int64_t start = esp_timer_get_time();
  while (!gpio_get_level(dev->echo_pin)) {
    if (timeout_expired(start, PING_TIMEOUT)) 
      return ESP_ERR_RS_PING_TIMEOUT;
  }

  // got echo; measure
  int64_t echo_start = esp_timer_get_time();
  int64_t time = echo_start;
  while(gpio_get_level(dev->echo_pin)) 
  {
    time = esp_timer_get_time();
    if (timeout_expired(echo_start, max_time_us))
      return ESP_ERR_RS_ECHO_TIMEOUT;
  }

  *time_us = time - echo_start;

  return ESP_OK;
}

esp_err_t rs_measure_cm(const reservoir_sensor_t *dev, uint32_t max_distance, uint32_t *distance) 
{
  if (!(dev) || !(distance)) return ESP_ERR_INVALID_ARG;

    uint32_t time_us;
    rs_measure_raw(dev, max_distance * ROUNDTRIP_CM, &time_us);

    *distance = time_us / ROUNDTRIP_CM;

    return ESP_OK;
}

esp_err_t rs_get_res_fill_percent(const reservoir_sensor_t *dev, uint8_t *percent) 
{
  if (!(dev) || !(percent)) return ESP_ERR_INVALID_ARG;

  uint32_t distance;
  rs_measure_cm(dev, dev->res_depth_cm, &distance);

  float empty_percent = (float)distance / dev->res_depth_cm;

  *percent = 100 - (empty_percent * 100);

  return ESP_OK;
}