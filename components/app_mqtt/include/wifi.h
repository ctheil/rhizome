#ifndef __WIFI_H__
#define __WIFI_H__

esp_err_t wifi_init(void);
// void wifi_init_sta(void);
esp_err_t wifi_connect(char* wifi_ssid, char* wifi_password);
#endif