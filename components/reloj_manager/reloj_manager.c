#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_event.h"

// Incluimos el header de display para usar la función segura de actualización de la UI
#include "display.h" 

static const char *TAG = "RELOJ_MANAGER";
static bool sntp_initialized = false;

// Tarea periódica que lee el RTC interno segundo a segundo y actualiza la UI
static void reloj_task(void *pvParameters)
{
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char bufferHora[10];
        strftime(bufferHora, sizeof(bufferHora), "%H:%M:%S", &timeinfo);
        
        // Actualizamos la etiqueta gráfica de la hora de forma segura
        // a través de la función del display (protegida por el cerrojo de LVGL)
        display_gui_update_time(bufferHora);

        vTaskDelay(pdMS_TO_TICKS(1000)); // Se ejecuta cada 1 segundo
    }
}

// Handler para inicializar SNTP al obtener IP de la red
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (!sntp_initialized) {
            ESP_LOGI(TAG, "IP obtenida. Inicializando servicio SNTP...");
            esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            
            // Sincronización cada 10 minutos corregida (10 * 60 * 1000 ms)
            esp_sntp_set_sync_interval(10 * 60 * 1000); 
            
            esp_sntp_init();
            sntp_initialized = true;
        }
    }
}

void reloj_manager_init(void)
{
    ESP_LOGI(TAG, "Configurando zona horaria...");
    // Configura tu zona horaria local (Ejemplo: Argentina UTC-3)
    setenv("TZ", "ART-3", 1);
    tzset();

    // Registrar evento para escuchar cuando el Wi-Fi obtiene IP y arrancar el SNTP
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    // Crear la tarea FreeRTOS encargada de refrescar el reloj en pantalla
    xTaskCreate(reloj_task, "reloj_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Componente de reloj inicializado correctamente.");
}