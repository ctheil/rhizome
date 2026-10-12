idf_component_register(SRCS "src/channel_control.c"
                       REQUIRES pump reservoir_sensor app_config led app_mqtt
                       PRIV_REQUIRES cjson
                       INCLUDE_DIRS "include")
