#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "wifi_manager.h"

#define TAG "WIFI_MANAGER"

// Página HTML con campos para Wi-Fi y Broker MQTT
static const char *config_html = 
"<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
"<title>Configuracion CoolGuard</title></head>"
"<body style=\"font-family:Arial; text-align:center; margin-top:20px; background:#f4f4f9;\">"
"<div style=\"max-width:400px; margin:auto; background:white; padding:20px; border-radius:8px; box-shadow:0px 0px 10px rgba(0,0,0,0.1);\">"
"<h2>CoolGuard - Configuracion</h2>"
"<form action=\"/save\" method=\"POST\" style=\"text-align:left;\">"
"<b>Wi-Fi SSID:</b><br><input type=\"text\" name=\"ssid\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br>"
"<b>Wi-Fi Password:</b><br><input type=\"password\" name=\"pass\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br><hr>"
"<b>MQTT Broker URI (ej: mqtt://192.168.0.103:1883):</b><br><input type=\"text\" name=\"mqtt_uri\" value=\"mqtt://\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br>"
"<b>MQTT User:</b><br><input type=\"text\" name=\"mqtt_user\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br>"
"<b>MQTT Password:</b><br><input type=\"password\" name=\"mqtt_pass\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br>"
"<b>MQTT Topic:</b><br><input type=\"text\" name=\"mqtt_topic\" value=\"sensor/dht11\" style=\"width:100%%; padding:8px; margin:5px 0;\"><br><br>"
"<input type=\"submit\" value=\"Guardar y Conectar\" style=\"width:100%%; padding:12px; background:#4CAF50; color:white; border:none; border-radius:4px; font-size:16px; cursor:pointer;\">"
"</form></div></body></html>";

static esp_err_t root_get_handler(httpd_req_t *req) {
    httpd_resp_send(req, config_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_post_handler(httpd_req_t *req) {
    char content[512];
    int total_len = req->content_len;
    int cur_len = 0;
    
    if (total_len >= sizeof(content)) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Contenido demasiado largo");
        return ESP_FAIL;
    }

    if ((cur_len = httpd_req_recv(req, content, total_len)) <= 0) {
        return ESP_FAIL;
    }
    content[cur_len] = '\0';

    char ssid[32] = {0};
    char pass[64] = {0};
    char mqtt_uri[128] = {0};
    char mqtt_user[32] = {0};
    char mqtt_pass[64] = {0};
    char mqtt_topic[64] = {0};
    
    // Función auxiliar simple para parsear URL encoded (POST body)
    // Nota: Para mayor robustez en producción se puede usar sscanf avanzado o url decoding
    // Aquí asumimos formato estándar limpio enviado por el form
    sscanf(content, "ssid=%31[^&]&pass=%63[^&]&mqtt_uri=%127[^&]&mqtt_user=%31[^&]&mqtt_pass=%63[^&]&mqtt_topic=%63s", 
           ssid, pass, mqtt_uri, mqtt_user, mqtt_pass, mqtt_topic);

    // Guardar credenciales en la memoria NVS
    nvs_handle_t nvs_handle;
    if (nvs_open("storage", NVS_READWRITE, &nvs_handle) == ESP_OK) {
        nvs_set_str(nvs_handle, "wifi_ssid", ssid);
        nvs_set_str(nvs_handle, "wifi_pass", pass);
        nvs_set_str(nvs_handle, "mqtt_uri", mqtt_uri);
        nvs_set_str(nvs_handle, "mqtt_user", mqtt_user);
        nvs_set_str(nvs_handle, "mqtt_pass", mqtt_pass);
        nvs_set_str(nvs_handle, "mqtt_topic", mqtt_topic);
        nvs_commit(nvs_handle);
        nvs_close(nvs_handle);
    }

    const char *resp = "<h3>¡Configuracion guardada con exito!</h3>"
                       "<p>El dispositivo se reiniciara para conectarse a la red y al broker.</p>";
    httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);

    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static void start_webserver(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t server = NULL;

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_uri_t root_uri = {
            .uri      = "/",
            .method   = HTTP_GET,
            .handler  = root_get_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &root_uri);

        httpd_uri_t save_uri = {
            .uri      = "/save",
            .method   = HTTP_POST,
            .handler  = save_post_handler,
            .user_ctx = NULL
        };
        httpd_register_uri_handler(server, &save_uri);
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *disconn = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "Desconectado del Wi-Fi. Motivo código: %d", disconn->reason);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "¡Conectado exitosamente! IP asignada: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta_or_ap(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    nvs_handle_t nvs_handle;
    char ssid[32] = {0};
    char pass[64] = {0};
    size_t required_size = sizeof(ssid);
    bool has_credentials = false;

    if (nvs_open("storage", NVS_READONLY, &nvs_handle) == ESP_OK) {
        if (nvs_get_str(nvs_handle, "wifi_ssid", ssid, &required_size) == ESP_OK && strlen(ssid) > 0) {
            required_size = sizeof(pass);
            nvs_get_str(nvs_handle, "wifi_pass", pass, &required_size);
            has_credentials = true;
        }
        nvs_close(nvs_handle);
    }

    if (has_credentials) {
        ESP_LOGI(TAG, "Iniciando en modo Station. Buscando red: %s", ssid);
        esp_netif_create_default_wifi_sta();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
        ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

        wifi_config_t wifi_config = {0};
        strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
        strlcpy((char *)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        esp_wifi_connect();
        ESP_LOGI(TAG, "Wi-Fi iniciado en modo STA. Conectando...");
    } else {
        ESP_LOGI(TAG, "No hay credenciales guardadas. Iniciando Access Point CoolGuard...");
        esp_netif_create_default_wifi_ap();
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_ERROR_CHECK(esp_wifi_init(&cfg));

        wifi_config_t wifi_config = {
            .ap = {
                .ssid = "CoolGuard",
                .ssid_len = strlen("CoolGuard"),
                .channel = 1,
                .max_connection = 4,
                .authmode = WIFI_AUTH_OPEN
            },
        };

        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
        ESP_ERROR_CHECK(esp_wifi_start());

        start_webserver();
        ESP_LOGI(TAG, "=====================================================");
        ESP_LOGI(TAG, "Conectate al Wi-Fi: 'CoolGuard'");
        ESP_LOGI(TAG, "Entra al navegador web en: http://192.168.4.1");
        ESP_LOGI(TAG, "=====================================================");
    }
}