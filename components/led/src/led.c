#include "led.h"
#include "driver/gpio.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"


esp_err_t led_init_pin(uint8_t pin) 
{

   // Configure the GPIO pin
    gpio_reset_pin(pin);
    return gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

esp_err_t led_off(uint8_t pin) 
{
  return gpio_set_level(pin, 0);
}
esp_err_t led_on(uint8_t pin) 
{
  return gpio_set_level(pin, 1);
}

#define ONBOARD_LED 8
static uint8_t flashing = 0;
static uint8_t led_state = 0;

void led_flash_onboard(uint8_t should_flash) 
{
  flashing = should_flash;
}

void vLEDFlashTask(void *pvParameters) 
{
  while (1) {
    if (flashing) {
      uint8_t new_state = led_state == 1 ? 0 : 1;
      gpio_set_level(ONBOARD_LED, new_state);
      led_state = new_state;
    }

    vTaskDelay(100);
  }

  vTaskDelete(NULL);
}

void init_led_flash() {
    xTaskCreate(vLEDFlashTask, "led_flash_task", 1028, NULL, 0, NULL);
}