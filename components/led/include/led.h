#ifndef __LED_H__
#define __LED_H__

#include "esp_err.h"
#include "stdint.h"

typedef enum { LED_FAULT, LED_WATERING } led_target_t;
typedef enum { LED_ON, LED_OFF, LED_BLINK } led_mode_t;

typedef struct {
  uint8_t pin;
  led_target_t target;
  uint16_t period_ms;
  uint32_t led_last_flash;
  uint8_t state;
  led_mode_t mode;
} led_t;

void led_init();

esp_err_t led_sink(led_t *msg);
esp_err_t led_send(led_target_t target, led_mode_t mode, uint16_t period_ms);

#endif /*__LED_H__*/
