

idf_component_register(SRCS "mqtt_client_gui.c"
                       INCLUDE_DIRS "."
                       REQUIRES mqtt  nvs_flash esp_netif)