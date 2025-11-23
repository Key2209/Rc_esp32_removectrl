#if !defined(LCD_INIT_H)
#define LCD_INIT_H


#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "ui/screens/ui_DataScreen.h"
#include "ui/screens/ui_mainScr.h"
#include "ui/screens/ui_wifiINFOScreen.h"

extern SemaphoreHandle_t lvgl_mux;
void led_init_all();





#endif // LED_INIT_H