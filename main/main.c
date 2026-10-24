#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "driver/gpio.h"

// Componentes del proyecto
#include "display.h"
#include "dht11.h"

static const char *TAG = "MAIN";

static void dht11_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Iniciando tarea del sensor DHT11 en GPIO 5...");
    
    dht11_config_t dht_config = {
        .gpio_num = GPIO_NUM_5
    };

    if (dht11_init(&dht_config) != ESP_OK) {
        ESP_LOGE(TAG, "Error al inicializar el sensor DHT11");
    }

    while (1) {
        uint8_t humidity = 0;
        uint8_t temperature = 0;

        esp_err_t res = dht11_read(&humidity, &temperature);

        if (res == ESP_OK) {
            ESP_LOGI(TAG, "Temperatura: %d °C | Humedad: %d %%", temperature, humidity);
            display_gui_update_data(temperature, humidity, true);
        } else {
            ESP_LOGW(TAG, "Error en lectura del DHT11 (Código: %d)", res);
            display_gui_update_data(0, 0, false);
        }

        // El sensor DHT11 requiere al menos 2 segundos entre lecturas consecutivas
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Iniciando Aplicación ESP32-S3 + ST7789 + DHT11 ===");

    ESP_ERROR_CHECK(display_gui_init());
    ESP_LOGI(TAG, "Pantalla e interfaz gráfica listas.");

    xTaskCreate(dht11_task, "dht11_task", 4096, NULL, 5, NULL);
}