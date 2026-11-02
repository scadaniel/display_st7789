#include "mqtt_client.h"
#include "mqtt_client_gui.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static const char *TAG = "mqtt_client_wrapper";
static esp_mqtt_client_handle_t client = NULL;
static char saved_topic[64] = "sensor/dht11"; 
static char saved_uri[128] = "mqtt://192.168.0.103:1883";
static char saved_user[32] = "";
static char saved_pass[64] = "";
static bool is_mqtt_connected = false;

#define NVS_QUEUE_NAMESPACE "mqtt_queue"
#define MAX_QUEUED_ITEMS 50

typedef struct {
    uint32_t timestamp;
    float temperature;
    float humidity;
} sensor_record_t;

// Función auxiliar para decodificar cadenas con codificación URL (%3A, %2F, etc.)
static void url_decode(char *dst, const char *src, size_t max_len)
{
    size_t i = 0;
    while (*src && i < max_len - 1) {
        if (*src == '%' && isxdigit((unsigned char)*(src + 1)) && isxdigit((unsigned char)*(src + 2))) {
            char hex[3] = { src[1], src[2], '\0' };
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

// Guardar lectura en memoria NVS (cuando falla la red o el envío)
static void save_record_to_nvs(float temp, float hum)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_QUEUE_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(handle, "queue_count", &count);

        if (count < MAX_QUEUED_ITEMS) {
            char key[16];
            snprintf(key, sizeof(key), "rec_%ld", (long)count);
            
            sensor_record_t record = {
                .timestamp = (uint32_t)(esp_timer_get_time() / 1000000), // Uptime en segundos
                .temperature = temp,
                .humidity = hum
            };

            nvs_set_blob(handle, key, &record, sizeof(sensor_record_t));
            count++;
            nvs_set_i32(handle, "queue_count", count);
            nvs_commit(handle);
            
            ESP_LOGW(TAG, "[NVS BACKUP] ¡AVISO! Sin conexión o fallo de envío. Lectura guardada en memoria flash local. Total pendientes en buffer: %ld", (long)count);
        } else {
            ESP_LOGE(TAG, "[NVS BACKUP] Cola NVS llena (%d ítems). Descartando lectura más antigua.", MAX_QUEUED_ITEMS);
        }
        nvs_close(handle);
    }
}

// Sincronizar y enviar registros guardados en memoria cuando vuelve la conexión
static void flush_nvs_queue(void)
{
    nvs_handle_t handle;
    if (nvs_open(NVS_QUEUE_NAMESPACE, NVS_READWRITE, &handle) == ESP_OK) {
        int32_t count = 0;
        nvs_get_i32(handle, "queue_count", &count);

        if (count > 0) {
            ESP_LOGI(TAG, "[SYNC] =====================================================");
            ESP_LOGI(TAG, "[SYNC] ¡Conexión restablecida! Enviando %ld registros guardados en memoria...", (long)count);
            ESP_LOGI(TAG, "[SYNC] =====================================================");
            
            for (int32_t i = 0; i < count; i++) {
                char key[16];
                snprintf(key, sizeof(key), "rec_%ld", (long)i);
                
                sensor_record_t record;
                size_t size = sizeof(sensor_record_t);
                
                if (nvs_get_blob(handle, key, &record, &size) == ESP_OK) {
                    char payload[96];
                    snprintf(payload, sizeof(payload), "{\"temperature\": %.1f, \"humidity\": %.1f, \"time_uptime_s\": %lu, \"buffered\": true}", 
                             record.temperature, record.humidity, record.timestamp);
                    
                    int msg_id = esp_mqtt_client_publish(client, saved_topic, payload, 0, 1, 0);
                    if (msg_id >= 0) {
                        ESP_LOGI(TAG, "[SYNC] -> Registro en memoria enviado OK [Uptime: %lu s] (ID: %d)", record.timestamp, msg_id);
                    }
                }
            }
            
            // Limpiar la cola tras el envío exitoso de todos los pendientes
            nvs_set_i32(handle, "queue_count", 0);
            nvs_commit(handle);
            ESP_LOGI(TAG, "[SYNC] ¡Cola NVS sincronizada, enviada y vaciada por completo!");
        }
        nvs_close(handle);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "=====================================================");
            ESP_LOGI(TAG, "=== MQTT_EVENT_CONNECTED - Conectado al broker    ===");
            ESP_LOGI(TAG, "=====================================================");
            is_mqtt_connected = true;
            flush_nvs_queue(); // Disparar sincronización automática
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED - Desconectado del broker");
            is_mqtt_connected = false;
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;

        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA recibido en tópico: %.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "Contenido: %.*s", event->data_len, event->data);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR - Error en la conexión");
            is_mqtt_connected = false;
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                ESP_LOGE(TAG, "Error de transporte TCP (Código errno: %d)", event->error_handle->esp_transport_sock_errno);
            }
            break;

        default:
            break;
    }
}

void mqtt_app_start(void)
{
    nvs_handle_t nvs_handle;
    
    // Valores por defecto seguros
    strcpy(saved_uri, "mqtt://192.168.0.103:1883");
    memset(saved_user, 0, sizeof(saved_user));
    memset(saved_pass, 0, sizeof(saved_pass));
    strcpy(saved_topic, "sensor/dht11");
    
    size_t size;

    // Leer configuración guardada en NVS con decodificación URL
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        char temp_uri[128];
        size = sizeof(temp_uri);
        if (nvs_get_str(nvs_handle, "mqtt_uri", temp_uri, &size) == ESP_OK && strlen(temp_uri) > 0) {
            url_decode(saved_uri, temp_uri, sizeof(saved_uri));
        }

        size = sizeof(saved_user);
        char temp_user[32];
        if (nvs_get_str(nvs_handle, "mqtt_user", temp_user, &size) == ESP_OK && strlen(temp_user) > 0) {
            url_decode(saved_user, temp_user, sizeof(saved_user));
        }

        size = sizeof(saved_pass);
        char temp_pass[64];
        if (nvs_get_str(nvs_handle, "mqtt_pass", temp_pass, &size) == ESP_OK && strlen(temp_pass) > 0) {
            url_decode(saved_pass, temp_pass, sizeof(saved_pass));
        }

        size = sizeof(saved_topic);
        char temp_topic[64];
        if (nvs_get_str(nvs_handle, "mqtt_topic", temp_topic, &size) == ESP_OK && strlen(temp_topic) > 0) {
            url_decode(saved_topic, temp_topic, sizeof(saved_topic));
        }

        nvs_close(nvs_handle);
    }

    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, "=== CONFIGURACIÓN INICIAL CLIENTE MQTT (DECODIFICADA) ===");
    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, "-> Broker URI      : %s", saved_uri);
    ESP_LOGI(TAG, "-> Usuario         : %s", strlen(saved_user) > 0 ? saved_user : "(Sin usuario)");
    ESP_LOGI(TAG, "-> Contraseña      : %s", strlen(saved_pass) > 0 ? "********" : "(Sin contraseña)");
    ESP_LOGI(TAG, "-> Tópico por defecto: %s", saved_topic);
    ESP_LOGI(TAG, "=====================================================");

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = saved_uri,
    };

    if (strlen(saved_user) > 0) {
        mqtt_cfg.credentials.username = saved_user;
    }
    if (strlen(saved_pass) > 0) {
        mqtt_cfg.credentials.authentication.password = saved_pass;
    }

    client = esp_mqtt_client_init(&mqtt_cfg);
    if (client != NULL) {
        esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
        esp_mqtt_client_start(client);
    } else {
        ESP_LOGE(TAG, "[MQTT] Error crítico: No se pudo inicializar el cliente MQTT.");
    }
}

void mqtt_publish_sensor_data(float temp, float hum)
{
    char payload[96];
    uint32_t current_uptime = (uint32_t)(esp_timer_get_time() / 1000000);
    snprintf(payload, sizeof(payload), "{\"temperature\": %.1f, \"humidity\": %.1f, \"time_uptime_s\": %lu}", 
             temp, hum, current_uptime);

    if (client != NULL && is_mqtt_connected) {
        ESP_LOGI(TAG, "=====================================================");
        ESP_LOGI(TAG, "=== INTENTO DE PUBLICACIÓN MQTT                   ===");
        ESP_LOGI(TAG, "=====================================================");
        ESP_LOGI(TAG, "-> Broker Activo   : %s", saved_uri);
        ESP_LOGI(TAG, "-> Tópico destino  : %s", saved_topic);
        ESP_LOGI(TAG, "-> Payload JSON    : %s", payload);
        ESP_LOGI(TAG, "=====================================================");

        int msg_id = esp_mqtt_client_publish(client, saved_topic, payload, 0, 1, 0);
        
        if (msg_id >= 0) {
            ESP_LOGI(TAG, "-> [EXITO] Mensaje encolado correctamente con ID: %d", msg_id);
        } else {
            ESP_LOGE(TAG, "[MQTT] Falló al publicar. Guardando lectura en NVS local...");
            save_record_to_nvs(temp, hum);
        }
    } else {
        ESP_LOGW(TAG, "[MQTT] Omitiendo publicación: Sin conexión MQTT. Guardando en NVS...");
        save_record_to_nvs(temp, hum);
    }
}