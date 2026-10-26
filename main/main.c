#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"

// Componentes del proyecto
#include "display.h"
#include "dht11.h"
#include "wifi_manager.h"
#include "mqtt_client_gui.h"
#include "reset_button.h"

static const char *TAG = "MAIN";
static bool mqtt_initialized = false;

static void wifi_connect_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (!mqtt_initialized) {
            ESP_LOGI(TAG, "[MQTT] IP obtenida. Iniciando cliente MQTT al Broker...");
            mqtt_app_start();
            mqtt_initialized = true;
        }
    }
}

static void sensor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Iniciando tarea del sensor DHT11 en GPIO 5...");
    
    dht11_config_t dht_config = {
        .gpio_num = GPIO_NUM_5
    };

    if (dht11_init(&dht_config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar el sensor DHT11");
    }

    int mqtt_publish_counter = 0; 

    while (1) {
        uint8_t humidity = 0;
        uint8_t temperature = 0;

        esp_err_t res = dht11_read(&humidity, &temperature);

        if (res == ESP_OK) {
            ESP_LOGI(TAG, "[LECTURA OK] Temperatura: %d °C | Humedad: %d %%", temperature, humidity);
            display_gui_update_data(temperature, humidity, true);

            mqtt_publish_counter++;
            if (mqtt_publish_counter >= 5) {
                mqtt_publish_counter = 0;
                if (mqtt_initialized) {
                    ESP_LOGI(TAG, "[MQTT] Enviando datos de temperatura y humedad...");
                    mqtt_publish_sensor_data((float)temperature, (float)humidity);
                } else {
                    ESP_LOGW(TAG, "[MQTT] Omitiendo publicación: Cliente MQTT aún no conectado.");
                }
            }
        } else {
            ESP_LOGW(TAG, "[ERROR] Fallo en lectura del DHT11 (Código: %d)", res);
            display_gui_update_data(0, 0, false);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, "=== Iniciando Aplicación CoolGuard (ESP32-S3)     ===");
    ESP_LOGI(TAG, "=====================================================");

    // 1. Inicializar NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // 2. Inicializar la pantalla y LVGL
    ESP_ERROR_CHECK(display_gui_init());
    ESP_LOGI(TAG, "[PANEL] Pantalla e interfaz gráfica listas.");

    // 3. Inicializar el pulsador de reset en el GPIO 4
    ESP_ERROR_CHECK(reset_button_init(GPIO_NUM_4, 3));

    // 4. Inicializar Wi-Fi PRIMERO (crea el netif y el bucle de eventos por debajo)
    wifi_init_sta_or_ap();

    // 5. Registrar el detector de IP DESPUÉS de que el sistema de red esté activo
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_connect_event_handler,
                                                        NULL,
                                                        NULL));

    // 6. Crear la tarea FreeRTOS del sensor
    xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}