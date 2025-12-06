#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_err.h"
#include "lcd/lcd_init.h"
#include "wifi/ap/ap_connect.h"




#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h" // <-- 新增：解决 %x 警告和 PRIu32 等宏
// 定义用于 NVS 存储和日志打印的常量
static const char *TAG = "OTA_VALIDATION";
static const char *NVS_NAMESPACE = "ota_data";
static const char *NVS_KEY_BOOT_COUNT = "boot_count";

// 定义最大启动尝试次数，如果超过这个次数仍未标记有效，则回滚
#define MAX_BOOT_ATTEMPTS 3
// 定义稳定运行时间 (秒)。如果程序运行超过这个时间，则标记为有效。
#define STABILITY_TIME_SEC 300
/**
 * @brief 启动阶段的 OTA 验证和防回滚检查。
 */
// 核心验证函数 (修正了类型和宏)
void perform_ota_validation(void)
{
    esp_err_t err;
    nvs_handle_t nvs_handle;
    int32_t boot_count = 0; // <-- 修正：使用 int32_t

    // 获取当前正在运行的分区
    const esp_partition_t *running_partition = esp_ota_get_running_partition();

    if (running_partition == NULL) {
        ESP_LOGE(TAG, "无法获取当前运行的分区信息！");
        return;
    }
    // <-- 修正：使用 PRIx32 宏打印 32位地址
    ESP_LOGI(TAG, "当前运行的分区: %s (Address: 0x%" PRIx32 ")", 
             running_partition->label, running_partition->address);

    // 检查分区是否处于 '待验证' (Pending Verification) 状态
    // <-- 修正：使用 running_partition->subtype 检查分区类型
    if (running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0 ||
        running_partition->subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) 
    {
        // 1. 读取并增加启动计数
        err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
        if (err == ESP_OK) {
            // nvs_get_i32 需要 int32_t*，现在 boot_count 是 int32_t，匹配
            err = nvs_get_i32(nvs_handle, NVS_KEY_BOOT_COUNT, &boot_count); 
            if (err == ESP_ERR_NVS_NOT_FOUND) {
                boot_count = 0; // 首次启动
            }
            
            boot_count++;
            nvs_set_i32(nvs_handle, NVS_KEY_BOOT_COUNT, boot_count);
            nvs_commit(nvs_handle);
            nvs_close(nvs_handle);
        }
        ESP_LOGI(TAG, "本次启动次数: %" PRId32, boot_count); // <-- 修正：使用 PRId32 宏

        // 2. 检查是否超过最大尝试次数
        if (boot_count > MAX_BOOT_ATTEMPTS) {
            ESP_LOGE(TAG, "连续启动失败 %" PRId32 " 次！新固件被认定为无效。", (int32_t)MAX_BOOT_ATTEMPTS);
            
            // 将固件标记为无效并强制回滚到上一个有效版本
            esp_ota_mark_app_invalid_rollback_and_reboot();
        }
    }
}
/**
 * @brief 监控任务：运行稳定后调用此函数标记固件有效。
 */
// void stability_monitor_task(void *pvParameter)
// {
//     // 等待预设的稳定时间
//     ESP_LOGI(TAG, "新固件稳定性监测中，等待 %d 秒...", STABILITY_TIME_SEC);
//     vTaskDelay(pdMS_TO_TICKS(STABILITY_TIME_SEC * 1000));

//     // 理论上，可以在这里增加额外的应用层检查 (例如 Wi-Fi 连接是否成功等)
//     if (1 /* 假设系统稳定 */) {
//         // *** 关键 API: 标记为有效，防止 Bootloader 再次回滚 ***
//         esp_err_t err = esp_ota_mark_app_valid_default_direction();
//         if (err == ESP_OK) {
//              ESP_LOGI(TAG, "✅ 固件运行稳定，已成功标记为有效，并清除启动计数。");
             
//              // 清除 NVS 中的启动计数器，防止回滚逻辑干扰正常启动
//              nvs_handle_t nvs_handle;
//              if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle) == ESP_OK) {
//                  nvs_erase_key(nvs_handle, NVS_KEY_BOOT_COUNT);
//                  nvs_commit(nvs_handle);
//                  nvs_close(nvs_handle);
//              }
//         } else {
//              ESP_LOGE(TAG, "标记固件有效失败: %s", esp_err_to_name(err));
//         }
//     } else {
//         ESP_LOGE(TAG, "核心功能不稳定，主动触发回滚！");
//         esp_ota_mark_app_invalid_rollback_and_reboot();
//     }
    
//     vTaskDelete(NULL);
// }


void app_main(void)
{

    // 必须首先初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    perform_ota_validation();
    led_init_all();
    wifi_app_init();
    //xTaskCreate(&stability_monitor_task, "stability_monitor", 4096, NULL, 5, NULL);
    while (1) {

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}



