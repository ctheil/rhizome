#ifndef MOISTURE_SENSOR_H
#define MOISTURE_SENSOR_H

#include "esp_log.h"

#define BURST_COUNT 100

void ms_get_reading(uint8_t channel, int *result);
void burst_read_adc(uint8_t channel, int *reads_arr);
void read_adc(uint8_t channel, int *result);
int calc_sd(int *reads_arr, int size);


#endif