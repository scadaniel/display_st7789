#include "reset_button.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_system.h"

static const char *TAG = "RESET_BUTTON";
static int s_button_gpio = -1;
static int s_hold_time = 3;

static void button_task(void *arg)
{
    int counter = 0;
    
    while (1) {
        // Como usamos pull-up interna, cuando se presiona el botón el nivel es 0 (LOW)
        if (gpio_get_level(s_button_gpio) == 0) {
            counter++;
            ESP_LOGI(TAG, "Botón presionado... mantén (%d/%ds)", counter, s_hold_time);
            
            if (counter >= s_hold_time) {
                ESP_LOGW(TAG, "¡Tiempo cumplido! Borrando configuración NVS (Factory Reset)...");
                
                // Borrar NVS para eliminar credenciales de Wi-Fi y MQTT
                nvs_flash_erase();
                
                ESP_LOGW(TAG, "Reiniciando dispositivo en modo Access Point...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            }
        } else {
            if (counter > 0) {
                ESP_LOGI(TAG, "Botón liberado. Cancelando reset.");
            }
            counter = 0;
        }

        // Revisar el estado del botón cada 1 segundo
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

esp_err_t reset_button_init(int gpio_num, int hold_time_seconds)
{
    s_button_gpio = gpio_num;
    s_hold_time = hold_time_seconds;

    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << s_button_gpio),
        .pull_up_en = GPIO_PULLUP_ENABLE, // Pull-up interna habilitada
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    
    esp_err_t err = gpio_config(&io_conf);
    if (err != ESP_OK) {
        return err;
    }

    // Crear tarea en segundo plano para monitorear el tiempo de pulsación
    xTaskCreate(button_task, "button_task", 2048, NULL, 3, NULL);
    
    ESP_LOGI(TAG, "Pulsador de reset inicializado en GPIO %d (Pull-up activa)", s_button_gpio);
    return ESP_OK;
}