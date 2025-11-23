#ifndef LVGL_TASK_H
#define LVGL_TASK_H


// #include "lcd_init.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"



void ui_create_reset_screen_arc(void);
void ui_load_reset_screen(void);


void ui_update_reset_progress_arc(int progress);
// void ui_update_reset_progress_custom(uint8_t progress);

// void ui_create_reset_screen_custom(void);
// void ui_load_reset_screen(void);
// void ui_update_wrapper_async1(void *arg);
void example_lvgl_port_task(void *arg);
void button_init();



#endif
