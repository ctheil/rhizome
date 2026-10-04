idf_component_register(SRCS "src/transport.c" "src/wifi.c" "src/router.c"
                       PRIV_REQUIRES  esp_wifi mqtt nvs_flash
                       REQUIRES app_config 
                       INCLUDE_DIRS "include")
