#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_rom_sys.h"
#include "esp_log.h"

static const char *TAG = "dht11";
static gpio_num_t s_dht_gpio = GPIO_NUM_NC;

esp_err_t dht11_init(const dht11_config_t *config)
{
    if (config == NULL || config->gpio_num < 0) {
        return ESP_ERR_INVALID_ARG;
    }

    s_dht_gpio = config->gpio_num;

    gpio_reset_pin(s_dht_gpio);
    gpio_set_direction(s_dht_gpio, GPIO_MODE_INPUT_OUTPUT_OD);
    gpio_set_pull_mode(s_dht_gpio, GPIO_PULLUP_ONLY);

    ESP_LOGI(TAG, "Sensor DHT11 inicializado en GPIO %d", s_dht_gpio);
    return ESP_OK;
}

esp_err_t dht11_read(uint8_t *humidity, uint8_t *temperature)
{
    if (s_dht_gpio == GPIO_NUM_NC) {
        ESP_LOGE(TAG, "El sensor DHT11 no ha sido inicializado");
        return ESP_ERR_INVALID_STATE;
    }

    if (humidity == NULL || temperature == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[5] = {0};

    // 1. Enviar señal de Start (Pull Down al menos 18 ms)
    gpio_set_direction(s_dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(s_dht_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(20));

    gpio_set_level(s_dht_gpio, 1);
    esp_rom_delay_us(30);
    gpio_set_direction(s_dht_gpio, GPIO_MODE_INPUT);

    // Suspendemos el planificador de FreeRTOS para aislar la lectura en microsegundos
    vTaskSuspendAll();

    // 3. Esperar respuesta del sensor (80 us Low, 80 us High)
    int timeout = 100;
    while (gpio_get_level(s_dht_gpio) == 1) {
        if (--timeout == 0) { xTaskResumeAll(); return ESP_ERR_TIMEOUT; }
        esp_rom_delay_us(1);
    }

    timeout = 200;
    while (gpio_get_level(s_dht_gpio) == 0) {
        if (--timeout == 0) { xTaskResumeAll(); return ESP_ERR_TIMEOUT; }
        esp_rom_delay_us(1);
    }

    timeout = 200;
    while (gpio_get_level(s_dht_gpio) == 1) {
        if (--timeout == 0) { xTaskResumeAll(); return ESP_ERR_TIMEOUT; }
        esp_rom_delay_us(1);
    }

    // 4. Leer 40 bits de datos (5 bytes)
    for (int i = 0; i < 40; i++) {
        int bit_timeout = 1000;
        while (gpio_get_level(s_dht_gpio) == 0) {
            if (--bit_timeout == 0) { xTaskResumeAll(); return ESP_ERR_TIMEOUT; }
            esp_rom_delay_us(1);
        }

        int64_t t = esp_timer_get_time();

        bit_timeout = 1000;
        while (gpio_get_level(s_dht_gpio) == 1) {
            if (--bit_timeout == 0) { xTaskResumeAll(); return ESP_ERR_TIMEOUT; }
            esp_rom_delay_us(1);
        }

        // Si la señal permaneció en alto más de 40 microsegundos, es un bit '1'
        if ((esp_timer_get_time() - t) > 40) {
            data[i / 8] |= (1 << (7 - (i % 8)));
        }
    }

    // Reanudamos el planificador de tareas de FreeRTOS
    xTaskResumeAll();

    // 5. Verificar Checksum
    if (data[4] == ((data[0] + data[1] + data[2] + data[3]) & 0xFF)) {
        *humidity = data[0];
        *temperature = data[2];
        return ESP_OK;
    }

    return ESP_ERR_INVALID_CRC;
}