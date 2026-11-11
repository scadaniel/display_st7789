#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "display.h" 

static const char *TAG = "RELOJ_MANAGER";
static bool sntp_initialized = false;

static void reloj_task(void *pvParameters)
{
    while (1) {
        time_t now;
        struct tm timeinfo;
        time(&now);
        localtime_r(&now, &timeinfo);

        char bufferDateTime[32];
        // Formato combinado: "DD/MM/YYYY HH:MM:SS" (puedes ajustarlo si prefieres otro orden)
        strftime(bufferDateTime, sizeof(bufferDateTime), "%d/%m/%Y %H:%M:%S", &timeinfo);
        
        display_gui_update_datetime(bufferDateTime);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void wifi_event_handler_sntp(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        if (!sntp_initialized) {
            ESP_LOGI(TAG, "IP obtenida. Inicializando servicio SNTP...");
            esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
            esp_sntp_setservername(0, "pool.ntp.org");
            esp_sntp_set_sync_interval(10 * 60 * 1000); 
            esp_sntp_init();
            sntp_initialized = true;
        }
    }
}

void reloj_manager_init(void)
{
    ESP_LOGI(TAG, "Configurando zona horaria (Argentina UTC-3)...");
    setenv("TZ", "<-03>3", 1);
    tzset();

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler_sntp,
                                                        NULL,
                                                        NULL));

    xTaskCreate(reloj_task, "reloj_task", 2048, NULL, 5, NULL);
    ESP_LOGI(TAG, "Componente de reloj inicializado correctamente.");
}

// ESTA FUNCIÓN ES LA QUE EL MAIN.C ESTÁ INTENTANDO LLAMAR:
void reloj_manager_get_datetime_str(char *dest, size_t max_len)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(dest, max_len, "%Y-%m-%d %H:%M:%S", &timeinfo);
}