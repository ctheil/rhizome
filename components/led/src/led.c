#include "led.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/gpio_types.h"

#define TAG "led"

#define num_leds 2
led_t fault_led =  {.pin = 5, .target = LED_FAULT, .mode = LED_OFF, .state = 0};
led_t watering_led =  {.pin = 6, .target = LED_WATERING, .mode = LED_OFF, .state = 0};
led_t *leds[num_leds] = {&fault_led, &watering_led};

QueueHandle_t led_queue;


esp_err_t led_sink(led_t *msg) {
  if (msg == NULL) {
    return ESP_ERR_INVALID_ARG;
  }

  return xQueueSend(led_queue, msg, 0) == pdPASS ? ESP_OK
                                                                  : ESP_FAIL;
}

esp_err_t led_send(led_target_t target, led_mode_t mode, uint16_t period_ms) 
{
  led_t msg = {
    .target = target, 
    .mode = mode, 
    .period_ms = period_ms
  };
  return xQueueSend(led_queue, &msg, 0);
}

static void v_led_task(void *pvParameters) {
  led_t msg;
  while (1) {
    if (xQueueReceive(led_queue, &msg, pdMS_TO_TICKS(20)) == pdPASS) {
      for (uint8_t i = 0; i < num_leds; i++) {
        if (leds[i]->target != msg.target) continue;

        if (leds[i]->mode != msg.mode || leds[i]->period_ms != msg.period_ms) {
          leds[i]->mode = msg.mode;
          leds[i]->period_ms = msg.period_ms;
        }
      }
    }

    TickType_t now = xTaskGetTickCount();
    for (uint8_t i = 0; i < num_leds; i++) {
      if (leds[i]->mode == LED_BLINK) {
        uint32_t next_flash =
            leds[i]->led_last_flash + pdMS_TO_TICKS(leds[i]->period_ms);
        if (now - leds[i]->led_last_flash >= pdMS_TO_TICKS(leds[i]->period_ms)) {
          leds[i]->state ^= 1;
          leds[i]->led_last_flash = now;
        }
      } else {
        leds[i]->state = (leds[i]->mode == LED_ON);
      }
        gpio_set_level(leds[i]->pin, leds[i]->state);
    }
  }

  vTaskDelete(NULL);
}

void led_init() {
  for (uint8_t i = 0; i < num_leds; i++) {
    gpio_reset_pin(leds[i]->pin);
    gpio_set_direction(leds[i]->pin, GPIO_MODE_OUTPUT);
  }

  led_queue = xQueueCreate(4, sizeof(led_t));
  if (led_queue != NULL) {
    xTaskCreate(v_led_task, "led_task", 1028, NULL, 0, NULL);
  }
}
