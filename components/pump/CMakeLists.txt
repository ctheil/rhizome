idf_component_register(SRCS "src/pump.c"
                       PRIV_REQUIRES esp_driver_gpio
                       REQUIRES reservoir_sensor
                       INCLUDE_DIRS "include")
