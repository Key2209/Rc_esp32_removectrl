#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "lcd/lcd_init.h"
#include "wifi/ap/ap_connect.h"


void app_main(void)
{
    led_init_all();
    wifi_app_init();
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
