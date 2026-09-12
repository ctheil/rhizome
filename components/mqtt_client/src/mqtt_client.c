#include "mqtt_client.h"
#include "esp_log.h"


#define TAG "mqtt_client"

static void log_err_if_nonzero(char *message, int error_code)
{
  if (error_code != 0) {
    ESP_LOGE(TAG, "Last error is %s: 0x%x", message, error_code);
  }
}