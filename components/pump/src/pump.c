#include "pump.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "esp_log.h"
#include "reservoir_sensor.h"

#define TAG "pump"

esp_err_t pump_drive(pump_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  esp_err_t err;
  err = gpio_set_level(pump->in1_pin, 0);
  if (err != ESP_OK) {return err;}
  err = gpio_set_level(pump->in2_pin, 1);
  if (err != ESP_OK) {return err;}
  return ESP_OK;
}

esp_err_t pump_off(pump_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  gpio_set_level(pump->in1_pin, 0);
  gpio_set_level(pump->in2_pin, 0);

  return ESP_OK;
}

esp_err_t pump_test(pump_t *pump) 
{
  if (!pump) return ESP_ERR_INVALID_ARG;

  esp_err_t ret;
  ret = pump_drive(pump);
  if (ret != ESP_OK) {
    pump_off(pump);
    return ret;
  }
  vTaskDelay(pdMS_TO_TICKS(1000));
  return pump_off(pump);
}

esp_err_t mosfet_off(uint8_t pin) 
{
return gpio_set_level(pin, 0);
}

esp_err_t mosfet_on(uint8_t pin) 
{
return gpio_set_level(pin, 1);
}

esp_err_t pump_init(uint8_t mosfet_pin, pump_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  gpio_config_t mosfet_conf = {
        .pin_bit_mask = (1ULL << mosfet_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&mosfet_conf);

  gpio_config_t in1_conf = {
        .pin_bit_mask = (1ULL << pump->in1_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&in1_conf);

  gpio_config_t in2_conf = {
        .pin_bit_mask = (1ULL << pump->in2_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&in2_conf);

  return ESP_OK;
}