idf_component_register(SRCS "src/app_mqtt.c" "src/wifi.c"
                       PRIV_REQUIRES  esp_wifi mqtt nvs_flash
                       REQUIRES app_config 
                       INCLUDE_DIRS "include")
