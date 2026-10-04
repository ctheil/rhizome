idf_component_register(SRCS "src/reservoir_sensor.c"
                       PRIV_REQUIRES esp_driver_gpio esp_timer cjson
                       REQUIRES analog_read app_mqtt
                       INCLUDE_DIRS "include")
