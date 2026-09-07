
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

typedef struct
{
  uint8_t enable_reservoir_sensor;
  uint16_t max_reservoir_depth_cm;
} config_t;

#endif /* __CONFIG_H__ */