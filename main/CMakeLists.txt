idf_component_register(SRCS "main.c"
                       PRIV_REQUIRES spi_flash nvs_flash
                       INCLUDE_DIRS "."
                       REQUIRES pump app_mqtt app_config channel_control
                       )
