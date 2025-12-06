#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "lcd/lcd_init.h"
#include "wifi/ap/ap_connect.h"




#include "esp_ota_ops.h"   // 用于获取分区和 OTA 信息

#include "esp_system.h"    // 用于获取 IDF 版本和芯片信息
static const char *TAG = "APP_INFO";

void print_system_info(void)
{
    // 获取应用程序描述信息
    const esp_app_desc_t *app_desc = esp_app_get_description();

    // 1. 打印固件版本和编译时间
    ESP_LOGI(TAG, "=======================================================");
    ESP_LOGI(TAG, "💾 固件信息");
    ESP_LOGI(TAG, "项目名称: %s", app_desc->project_name);
    ESP_LOGI(TAG, "App 版本: %s", app_desc->version);
    ESP_LOGI(TAG, "编译时间: %s %s", app_desc->date, app_desc->time);
    ESP_LOGI(TAG, "IDF 版本: %s", app_desc->idf_ver);

    // 2. 打印当前运行的分区信息
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        ESP_LOGI(TAG, "🚀 运行分区信息");
        ESP_LOGI(TAG, "当前分区: %s", running->label);
        // 使用 PRIx32 宏打印 32 位地址，避免格式错误
        ESP_LOGI(TAG, "起始地址: 0x%" PRIx32, running->address); 
        ESP_LOGI(TAG, "分区类型: App (Type:%d, Subtype:%d)", running->type, running->subtype);
    }

    // 3. 打印启动分区信息 (用于确认 Bootloader 下次会启动哪个分区)
    const esp_partition_t *boot = esp_ota_get_boot_partition();
    if (boot) {
        ESP_LOGI(TAG, "⏭️ 下次启动分区");
        ESP_LOGI(TAG, "Boot 分区: %s", boot->label);
    }
    
    // 4. 打印芯片和内存信息
    ESP_LOGI(TAG, "🖥️ 芯片/内存信息");
    // esp_chip_info_t info;
    // esp_chip_info(&info);
    // ESP_LOGI(TAG, "芯片型号: ESP32-S3"); // 假设是 S3，可根据实际型号打印
    ESP_LOGI(TAG, "可用堆内存: %" PRIu32 " 字节", esp_get_free_heap_size());
    
    ESP_LOGI(TAG, "=======================================================");
}
void app_main(void)
{


    led_init_all();
    wifi_app_init();
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



