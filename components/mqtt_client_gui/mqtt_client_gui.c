#include "mqtt_client.h"
#include "mqtt_client_gui.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "mqtt_client_wrapper";
static esp_mqtt_client_handle_t client = NULL;
static char saved_topic[64] = "sensor/dht11"; 
static char saved_uri[128] = "mqtt://192.168.0.103:1883";
static char saved_user[32] = "";
static char saved_pass[64] = "";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client_instance = event->client;
    
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "=====================================================");
            ESP_LOGI(TAG, "=== MQTT_EVENT_CONNECTED - Conectado al broker    ===");
            ESP_LOGI(TAG, "=====================================================");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED - Desconectado del broker");
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
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR - Error en la conexión o transporte");
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

    // Leer configuración guardada en NVS
    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        char temp_uri[128];
        size = sizeof(temp_uri);
        if (nvs_get_str(nvs_handle, "mqtt_uri", temp_uri, &size) == ESP_OK && strlen(temp_uri) > 0) {
            // Evitar URIs corruptas con caracteres de codificación web (%)
            if (strchr(temp_uri, '%') == NULL) {
                strcpy(saved_uri, temp_uri);
            }
        }

        size = sizeof(saved_user);
        nvs_get_str(nvs_handle, "mqtt_user", saved_user, &size);

        size = sizeof(saved_pass);
        nvs_get_str(nvs_handle, "mqtt_pass", saved_pass, &size);

        size = sizeof(saved_topic);
        char temp_topic[64];
        if (nvs_get_str(nvs_handle, "mqtt_topic", temp_topic, &size) == ESP_OK && strlen(temp_topic) > 0) {
            if (strchr(temp_topic, '%') == NULL) {
                strcpy(saved_topic, temp_topic);
            }
        }

        nvs_close(nvs_handle);
    }

    ESP_LOGI(TAG, "=====================================================");
    ESP_LOGI(TAG, "=== CONFIGURACIÓN INICIAL CLIENTE MQTT            ===");
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
    if (client != NULL) {
        char payload[64];
        snprintf(payload, sizeof(payload), "{\"temperature\": %.1f, \"humidity\": %.1f}", temp, hum);
        
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
            ESP_LOGE(TAG, "-> [ERROR] Falló al encolar el mensaje MQTT");
        }
    } else {
        ESP_LOGW(TAG, "[MQTT] Omitiendo publicación: Cliente MQTT es NULL.");
    }
}