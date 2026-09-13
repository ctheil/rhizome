idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash nvs_flash
                       INCLUDE_DIRS "."
                       REQUIRES moisture_sensor reservoir_sensor pump mqtt_client app_config
                       )
