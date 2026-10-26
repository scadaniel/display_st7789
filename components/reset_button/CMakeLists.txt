idf_component_register(SRCS "reset_button.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_driver_gpio nvs_flash freertos log esp_system )