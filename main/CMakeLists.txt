idf_component_register(SRCS "main.c"
                     INCLUDE_DIRS "."
                     REQUIRES display dht11 mqtt_client_gui wifi_manager esp_timer reset_button)

