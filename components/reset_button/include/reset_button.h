#ifndef RESET_BUTTON_H
#define RESET_BUTTON_H
#include "esp_err.h"
#include "driver/gpio.h"


esp_err_t reset_button_init(int gpio_num, int hold_time_seconds);
#endif

