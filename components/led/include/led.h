#ifndef __LED_H__
#define __LED_H__

#include "esp_err.h"
#include "stdint.h"

esp_err_t led_init_pin(uint8_t pin);
esp_err_t led_off(uint8_t pin);
esp_err_t led_on(uint8_t pin);

void led_flash_onboard(uint8_t should_flash);

void init_led_flash();

#endif /*__LED_H__*/