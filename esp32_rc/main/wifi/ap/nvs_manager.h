#ifndef NVS_MANAGER_H
#define NVS_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdint.h>

// 定义 WiFi 配置的最大长度 (ESP32 标准限制)
#define MAX_SSID_LEN 32
#define MAX_PASS_LEN 64
#define MAX_NAME_LEN 32


// 定义我们要存储的配置结构体
typedef struct {
    char ssid[32];        // WiFi 名称
    char password[64];    // WiFi 密码
    char device_name[32]; // 设备名称 (自定义字段)
    uint8_t config_done;  // 标记位：0=未配置, 1=已配置
} app_config_t;


esp_err_t save_config_to_nvs(app_config_t *config);
esp_err_t load_config_from_nvs(app_config_t *config);
esp_err_t reset_wifi_config_from_nvs();



#endif