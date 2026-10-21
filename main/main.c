#include <stdio.h>
#include "display.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
void app_main(void)
{
    display_init();

    while(1){

  

        vTaskDelay(pdMS_TO_TICKS(1000));

    }
}
