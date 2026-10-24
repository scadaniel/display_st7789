#ifndef DHT11_H
#define DHT11_H

#include <stdint.h>
#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t gpio_num;
} dht11_config_t;

/**
 * @brief Inicializa el pin GPIO para el sensor DHT11.
 * 
 * @param config Estructura con la configuración del GPIO.
 * @return esp_err_t ESP_OK si la inicialización fue exitosa.
 */
esp_err_t dht11_init(const dht11_config_t *config);

/**
 * @brief Lee la humedad y temperatura desde el sensor DHT11.
 * 
 * @param humidity Puntero donde se almacenará el valor de humedad relativa (0-100%).
 * @param temperature Puntero donde se almacenará el valor de temperatura (°C).
 * @return esp_err_t ESP_OK si la lectura y checksum son válidos.
 */
esp_err_t dht11_read(uint8_t *humidity, uint8_t *temperature);

#ifdef __cplusplus
}
#endif

#endif // DHT11_H
