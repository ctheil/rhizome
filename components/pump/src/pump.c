
#include "pump.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <driver/gpio.h>
#include "esp_log.h"

esp_err_t pump_mosfet_set_level(pump_t *pump, uint8_t level) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  return gpio_set_level(pump->mosfet_pin, level);
}

esp_err_t pump_drive(pump_t *pump, direction_t dir, uint8_t speed) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  uint8_t a1_level = 0;
  uint8_t a2_level =1;
  if (dir == DIR_REVERSE) {
    a1_level = 1;
    a2_level=0;
  }

  gpio_set_level(pump->ain1, a1_level);
  gpio_set_level(pump->ain2, a2_level);
  pump_mosfet_set_level(pump, 1);
  return ESP_OK;
}

esp_err_t pump_off(pump_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  pump_mosfet_set_level(pump, 0);
  return ESP_OK;

}

esp_err_t pump_init(pump_t *pump) 
{
  if (!(pump)) return ESP_ERR_INVALID_ARG;

  gpio_config_t mosfet_conf = {
        .pin_bit_mask = (1ULL << pump->mosfet_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&mosfet_conf);

  gpio_config_t a1_conf = {
        .pin_bit_mask = (1ULL << pump->ain1),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&a1_conf);

  gpio_config_t a2_conf = {
        .pin_bit_mask = (1ULL << pump->ain2),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
  };
  gpio_config(&a2_conf);

  return ESP_OK;
}