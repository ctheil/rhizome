idf_component_register(SRCS "src/app_config.c"
                       PRIV_REQUIRES nvs_flash
                       REQUIRES pump
                       INCLUDE_DIRS "include")
