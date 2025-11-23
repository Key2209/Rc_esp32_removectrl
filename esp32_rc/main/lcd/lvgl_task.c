
#include "lvgl_task.h"
#include "lcd_init.h"

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "ui.h"
#include "button_gpio.h"
#include "iot_button.h"
#include "ui/screens/ui_DataScreen.h"
#include "ui/screens/ui_mainScr.h"
#include "ui/screens/ui_wifiINFOScreen.h"
#include "nvs_manager.h"

char *TAG = "LVGL_TASK";

// lvgl任务
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1

static QueueHandle_t ReSetUiQueue = NULL;

// 按键
#define BUTTON_LONG_PRESS_TARGET_TIME 4000
#define BUTTON_LONG_PRESS_TIME 1000
#define BUTTON_SHORT_PRESS_TIME 150
#define BUTTON_GPIO_PIN 0
#define BUTTON_GPIO_ACTIVE_LEVEL 0
static bool g_operation_executed = false;

static void switch_screen_safe();
static void back_to_main()
{
    lv_disp_load_scr(ui_mainScr);
}
static void button_event_cb(void *arg, void *data)
{

    button_event_t event = iot_button_get_event(arg);
    ESP_LOGI(TAG, "%s", iot_button_get_event_str(event));
    if (event == BUTTON_SINGLE_CLICK) // 短按切换屏幕
    {

        lv_async_call(switch_screen_safe, NULL);
    }

    // 长按足够久执行重置操作
    if (BUTTON_LONG_PRESS_START == event)
    {
        // 切换进度展示屏幕
        lv_async_call(ui_load_reset_screen,NULL);
        g_operation_executed = false; // 重置标志
    }

    if (BUTTON_LONG_PRESS_HOLD == event)
    {

        //ESP_LOGI(TAG, "\tTICKS[%" PRIu32 "]", iot_button_get_ticks_time(arg));
        uint32_t time = iot_button_get_ticks_time(arg);
        // 在lcd中展示秒数
        uint8_t lcd_time = (time-BUTTON_LONG_PRESS_TIME) * 100 / BUTTON_LONG_PRESS_TARGET_TIME;
        // lv_async_call(ui_update_wrapper_async,(void *)(intptr_t)lcd_time);
        if (xQueueSend(ReSetUiQueue, &lcd_time, portMAX_DELAY) == pdPASS)
        {
        }


        if (time >= BUTTON_LONG_PRESS_TARGET_TIME)
        {
            // 执行操作
            g_operation_executed = true;
        }
        else
        {

            // 取消操作
            g_operation_executed = false;

        }
    }
    if (event == BUTTON_LONG_PRESS_UP)
    {
        uint32_t time = iot_button_get_ticks_time(arg);

        if (time >= BUTTON_LONG_PRESS_TARGET_TIME && g_operation_executed == true)
        {
            // 执行操作
            g_operation_executed = false;
            reset_wifi_config_from_nvs();
        }
        else
        {
            g_operation_executed = false;
            // 切换回屏幕
            lv_async_call(back_to_main, NULL);
        }
    }
}

lv_obj_t *LVGL_Scr_List[4];
void button_init()
{
    // create gpio button

    const button_config_t btn_cfg = {
        .long_press_time = BUTTON_LONG_PRESS_TIME,
        .short_press_time = BUTTON_SHORT_PRESS_TIME,
    };
    const button_gpio_config_t btn_gpio_cfg = {
        .gpio_num = BUTTON_GPIO_PIN,
        .active_level = BUTTON_GPIO_ACTIVE_LEVEL,

    };
    button_handle_t gpio_btn = NULL;
    esp_err_t ret = iot_button_new_gpio_device(&btn_cfg, &btn_gpio_cfg, &gpio_btn);
    if (NULL == gpio_btn)
    {
        ESP_LOGE(TAG, "Button create failed");
    }



    iot_button_register_cb(gpio_btn, BUTTON_PRESS_DOWN, NULL, button_event_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_PRESS_UP, NULL, button_event_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_SINGLE_CLICK, NULL, button_event_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_START, NULL, button_event_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_HOLD, NULL, button_event_cb, NULL);
    iot_button_register_cb(gpio_btn, BUTTON_LONG_PRESS_UP, NULL, button_event_cb, NULL);
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
void switch_screen_safe()
{
    // static：保证 cnt 的值在每次函数调用时保持不变
    static int cnt = 0;

    // 1. 获取屏幕数量
    const int num_screens = ARRAY_SIZE(LVGL_Scr_List);

    // 2. 递增计数器并取模，确保 cnt 始终在 [0, num_screens - 1] 范围内
    // 注意：先递增 cnt，再取模，得到下一个屏幕的索引
    cnt = (cnt + 1) % num_screens;

    lv_obj_t *next_screen = LVGL_Scr_List[cnt];

    lv_scr_load_anim(next_screen, LV_SCR_LOAD_ANIM_MOVE_LEFT, 500, 0, false);
}

static bool example_lvgl_lock(int timeout_ms)
{
    assert(lvgl_mux && "bsp_display_start must be called first");

    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(lvgl_mux, timeout_ticks) == pdTRUE;
}
static void example_lvgl_unlock(void)
{
    assert(lvgl_mux && "bsp_display_start must be called first");
    xSemaphoreGive(lvgl_mux);
}

void example_lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");

    // lv_demo_music();
    // lv_demo_widgets();
    // lv_demo_stress();
    // lv_demo_benchmark();
    ui_init();
    ui_create_reset_screen_arc();
    LVGL_Scr_List[0] = ui_mainScr;
    LVGL_Scr_List[1] = ui_DataScreen;
    LVGL_Scr_List[2] = ui_wifiINFOScreen;
    LVGL_Scr_List[3] = ui_Screen1;

    ReSetUiQueue = xQueueCreate(1, sizeof(int));
    uint32_t task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;

    uint8_t ReSetValue=0;
    while (1)
    {

        if (xQueueReceive(ReSetUiQueue, &ReSetValue, 0) == pdPASS)
        {
            //ui_update_reset_progress_custom(ReSetValue);
            ui_update_reset_progress_arc(ReSetValue);
            ESP_LOGE(TAG,"复位进度:%d",ReSetValue);
        }

        // Lock the mutex due to the LVGL APIs are not thread-safe
        if (example_lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            // Release the mutex
            example_lvgl_unlock();
        }
        if (task_delay_ms > EXAMPLE_LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < EXAMPLE_LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = EXAMPLE_LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}
