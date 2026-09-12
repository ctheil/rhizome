idf_component_register(SRCS "src/mqtt_client.c" "src/wifi.c"
                       PRIV_REQUIRES  esp_wifi mqtt nvs_flash
                       INCLUDE_DIRS "include")
