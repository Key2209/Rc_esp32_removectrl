#include "lcd_init.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "lvgl.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "ui.h"
#include "button_gpio.h"
#include "led_strip.h"
#include "iot_button.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "ui/screens/ui_DataScreen.h"
#include "ui/screens/ui_mainScr.h"
#include "ui/screens/ui_wifiINFOScreen.h"
#include "nvs_manager.h"

#include "lvgl_task.h"
// 屏幕分辨率
#define EXAMPLE_LCD_H_RES 240
#define EXAMPLE_LCD_V_RES 240

// SPI引脚
#define LCD_HOST SPI2_HOST
#define EXAMPLE_PIN_NUM_LCD_DC 40
#define EXAMPLE_PIN_NUM_LCD_CS 41
#define EXAMPLE_PIN_NUM_SCLK 21
#define EXAMPLE_PIN_NUM_DATA0 47
#define EXAMPLE_PIN_NUM_LCD_RST 45

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

#define EXAMPLE_LVGL_BUFF_SIZE EXAMPLE_LCD_H_RES * 50

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2


#define EXAMPLE_LVGL_TASK_STACK_SIZE (5 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY 2

// 定义要使用的 GPIO 引脚，例如 GPIO 2
#define LED_GPIO_PIN GPIO_NUM_42

void init_gpio_output(void)
{
    // 1. 定义 GPIO 配置结构体
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,         // 禁用中断
        .mode = GPIO_MODE_OUTPUT,               // 设置为输出模式
        .pin_bit_mask = (1ULL << LED_GPIO_PIN), // 仅配置 LED_GPIO_PIN
        .pull_down_en = 0,                      // 禁用下拉电阻
        .pull_up_en = 0,                        // 禁用上拉电阻
    };
    // 2. 应用配置
    ESP_ERROR_CHECK(gpio_config(&io_conf));
}

static const char *TAG = "LED_INIT_TAG";
static lv_disp_draw_buf_t disp_buf; // contains internal graphic buffer(s) called draw buffer(s)
static lv_disp_drv_t disp_drv;      // contains callback functions
esp_lcd_panel_handle_t panel_handle = NULL;
esp_lcd_panel_io_handle_t io_handle = NULL;
led_strip_handle_t led_strip;


// esp_lcd面板io 写完成回调函数
static bool example_notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;

    /* 告诉 LVGL：一次 flush（显示刷新）已经完成，LVGL 可以继续内部处理。
       lv_disp_flush_ready 的实现会通知 LVGL 的调度器该显示缓冲已被显示设备使用完毕。*/
    lv_disp_flush_ready(disp_driver);
    return false;
}

void spi_init()
{
    ESP_LOGI(TAG, "Install SPI bus");

    // 1. SPI 总线配置 (使用通用配置)
    const spi_bus_config_t buscfg = {
        .mosi_io_num = EXAMPLE_PIN_NUM_DATA0,
        .sclk_io_num = EXAMPLE_PIN_NUM_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = EXAMPLE_LCD_H_RES * EXAMPLE_LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    // 2. LCD 面板 IO 配置 (使用通用配置)
    ESP_LOGI(TAG, "Install panel IO");
    const esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = EXAMPLE_PIN_NUM_LCD_CS,
        .dc_gpio_num = EXAMPLE_PIN_NUM_LCD_DC,
        .spi_mode = 0,
        .pclk_hz = 80 * 1000 * 1000, // 80 MHz
        .trans_queue_depth = 10,
        .on_color_trans_done = example_notify_lvgl_flush_ready,
        .user_ctx = &disp_drv,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &io_handle));
    // 3. 安装 ST7789 面板驱动
    ESP_LOGI(TAG, "Install ST7789 panel driver");
    // Vendor config 设为 NULL (使用驱动内置的初始化序列)
    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // 颜色顺序可能需要根据您的屏幕调整为 BGR
        .bits_per_pixel = 16,
        .vendor_config = NULL, // 使用官方驱动，不需要自定义初始化命令
    };
    // ✅ 关键调用：使用 ST7789 驱动
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // 4. 配置 ST7789 特有的 Gap（如果您的屏幕不是 240x240，例如 135x240）
    // 大多数 240x240 的 ST7789 屏幕不需要设置 gap，但有些小尺寸屏幕需要。
    // 如果您的屏幕分辨率与设置 EXAMPLE_LCD_H_RES/V_RES 匹配，请忽略此步骤或根据需要调整。
    // ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel_handle, 0, 80)); // 示例：如果屏幕是 240x135
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
}

/**
 * example_lvgl_flush_cb
 *
 * LVGL 调用该回调以把一个矩形区域的像素数据发送到显示设备。
 * 我们实现该回调时，需要把 LVGL 提供的 color_map 数据写到显示面板对应区域。
 *
 * 参数：
 *   drv       : lv_disp_drv_t*，LVGL 的显示驱动对象，drv->user_data 在后面被设置为 panel_handle
 *   area      : const lv_area_t*，要刷新的矩形区域（包含 x1,y1,x2,y2）
 *   color_map : lv_color_t*，指向要写入的像素数据缓存（格式由 LVGL 的 lv_color_t 定义）
 *
 * 说明：
 *   - area 的坐标通常是包含边界（inclusive），因此示例中对右下角 +1 以匹配底层 driver 的要求（不同驱动接口可能有所不同）。
 *   - 该函数中我们调用 esp_lcd_panel_draw_bitmap 来真正向屏幕写入像素。
 *   - 写入后不要直接调用 lv_disp_flush_ready（因为这里是同步调用，实际写入触发的 IO 事件会触发 example_notify_lvgl_flush_ready，
 *     或者驱动可能是阻塞写入，这两种模式根据驱动实现不同），示例配合 example_notify_lvgl_flush_ready 一起工作。
 */
static void example_lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map)
{
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    const int offsetx1 = area->x1;
    const int offsetx2 = area->x2;
    const int offsety1 = area->y1;
    const int offsety2 = area->y2;

    // copy a buffer's content to a specific area of the display
    esp_lcd_panel_draw_bitmap(panel_handle, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, color_map);
}

SemaphoreHandle_t lvgl_mux = NULL;
static void example_increase_lvgl_tick(void *arg)
{
    /* Tell LVGL how many milliseconds has elapsed */
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}
void lvgl_init()
{
    ESP_LOGI(TAG, "Initialize LVGL library");
    lv_init();
    // alloc draw buffers used by LVGL
    // it's recommended to choose the size of the draw buffer(s) to be at least 1/10 screen sized
    /* 为 LVGL 分配绘图缓冲区（建议使用能被 DMA 访问的内存，如果驱动需要 DMA）：
    heap_caps_malloc(EXAMPLE_LVGL_BUFFER_SIZE, MALLOC_CAP_DMA) 在 ESP32 中分配具有 DMA 能力的内存区域。
    buf1/buf2 是两个绘图缓冲区，LVGL 可以在它们之间切换以减少闪烁。
    如果内存不足，可以只分配一个缓冲区或减小 EXAMPLE_LVGL_BUFFER_SIZE。 */
    lv_color_t *buf1 = heap_caps_malloc(EXAMPLE_LVGL_BUFF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1);
    lv_color_t *buf2 = heap_caps_malloc(EXAMPLE_LVGL_BUFF_SIZE * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf2);
    // initialize LVGL draw buffers
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, EXAMPLE_LVGL_BUFF_SIZE);

    /* =============== 注册显示驱动到 LVGL =============== */
    ESP_LOGI(TAG, "Register display driver to LVGL");
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = EXAMPLE_LCD_H_RES;
    disp_drv.ver_res = EXAMPLE_LCD_V_RES;
    disp_drv.flush_cb = example_lvgl_flush_cb; // 回调函数！！！！！

    // 驱动更新回调
    disp_drv.draw_buf = &disp_buf;
    disp_drv.user_data = panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    /* =============== 注册 LVGL tick 定时器（用 esp_timer） =============== */
    ESP_LOGI(TAG, "Install LVGL tick timer");
    // Tick interface for LVGL (using esp_timer to generate 2ms periodic event)
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &example_increase_lvgl_tick, // 回调函数！！！！！！！！！
        .name = "lvgl_tick"};
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);
    xTaskCreate(example_lvgl_port_task, "LVGL", EXAMPLE_LVGL_TASK_STACK_SIZE, NULL, EXAMPLE_LVGL_TASK_PRIORITY, NULL);
}




// Set to 1 to use DMA for driving the LED strip, 0 otherwise
// Please note the RMT DMA feature is only available on chips e.g. ESP32-S3/P4
#define LED_STRIP_USE_DMA 0

#if LED_STRIP_USE_DMA
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 256
#define LED_STRIP_MEMORY_BLOCK_WORDS 1024 // this determines the DMA block size
#else
// Numbers of the LED in the strip
#define LED_STRIP_LED_COUNT 1
#define LED_STRIP_MEMORY_BLOCK_WORDS 0 // let the driver choose a proper memory block size automatically
#endif                                 // LED_STRIP_USE_DMA

// GPIO assignment
#define LED_STRIP_GPIO_PIN 48

// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ (10 * 1000 * 1000)

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO_PIN,                        // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_COUNT,                             // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,                               // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: GRB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }};

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,                    // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ,             // RMT counter clock frequency
        .mem_block_symbols = LED_STRIP_MEMORY_BLOCK_WORDS, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = LED_STRIP_USE_DMA, // Using DMA can improve performance when driving more LEDs
        }};

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void led_init_all()
{
    init_gpio_output();//cs片选
    spi_init();
    gpio_set_level(LED_GPIO_PIN, 1); // 屏幕背光
    button_init();
    led_strip = configure_led();
    lvgl_init();
}

