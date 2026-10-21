idf_component_register(SRCS "display.c" "lvgl_example.c"
                    INCLUDE_DIRS "include"
                    REQUIRES driver esp_timer esp_lcd_st7789 lvgl )
