idf_component_register(SRCS "src/channel_control.c"
                       REQUIRES pump reservoir_sensor app_config
                       INCLUDE_DIRS "include")
