idf_component_register(SRCS "src/reservoir_sensor.c"
                       PRIV_REQUIRES esp_driver_gpio esp_timer
                       REQUIRES analog_read
                       INCLUDE_DIRS "include")
