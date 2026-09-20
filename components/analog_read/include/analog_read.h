#ifndef __ANALOG_READ_H__
#define __ANALOG_READ_H__

#include "esp_err.h"

esp_err_t analog_peripheral_init(uint8_t pin);
esp_err_t analog_read(uint8_t pin, int *result);
esp_err_t analog_burst_read(uint8_t pin, uint32_t *mean_result, uint8_t poll_count, uint16_t poll_delay);
uint8_t read_to_percentage(uint16_t read, uint16_t min, uint16_t max);

#endif