#include "nvs_manager.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG_NVS = "NVS_MANAGER";


// 辅助函数：保存配置到 NVS
esp_err_t save_config_to_nvs(app_config_t *config)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. 打开 NVS 命名空间 "storage"，模式为 读写 (READWRITE)
    err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG_NVS, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return err;
    }

    // 2. 写入数据 (Key 最大长度 15 字符)
    // 写入字符串，自动处理 \0 结束符
    nvs_set_str(my_handle, "ssid", config->ssid);
    nvs_set_str(my_handle, "password", config->password);
    nvs_set_str(my_handle, "dev_name", config->device_name);
    // 写入整数
    nvs_set_u8(my_handle, "cfg_done", config->config_done);

    // 3. 提交修改 (Commit) - 这一步非常关键，不提交不会物理写入 Flash
    err = nvs_commit(my_handle);
    
    // 4. 关闭句柄，释放内存
    nvs_close(my_handle);
    
    ESP_LOGI(TAG_NVS, "Config saved to NVS");
    return err;
}

// 辅助函数：从 NVS 读取配置
esp_err_t load_config_from_nvs(app_config_t *config)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 打开 NVS，模式为 只读 (READONLY)
    err = nvs_open("storage", NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        return err; // 可能是第一次启动，还没有数据
    }

    // 读取数据需要提供缓冲区大小
    size_t required_size = sizeof(config->ssid);
    nvs_get_str(my_handle, "ssid", config->ssid, &required_size);

    required_size = sizeof(config->password);
    nvs_get_str(my_handle, "password", config->password, &required_size);

    required_size = sizeof(config->device_name);
    nvs_get_str(my_handle, "dev_name", config->device_name, &required_size);

    // 读取标记位，如果没读到(比如旧版本)，默认为 0
    err = nvs_get_u8(my_handle, "cfg_done", &config->config_done);
    if (err != ESP_OK) {
        config->config_done = 0; 
    }

    nvs_close(my_handle);
    return ESP_OK;
}


esp_err_t reset_wifi_config_from_nvs()
{

        // 1. 初始化 NVS (必须放在最前面),程序启动时候已经初始化过了
    // esp_err_t ret = nvs_flash_init();
    // if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    //   ESP_ERROR_CHECK(nvs_flash_erase());
    //   ret = nvs_flash_init();
    // }
    // ESP_ERROR_CHECK(ret);

    // ESP_ERROR_CHECK(esp_netif_init());
    // ESP_ERROR_CHECK(esp_event_loop_create_default());


    app_config_t cfg={0};
    cfg.config_done=0;
    save_config_to_nvs(&cfg);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    esp_restart();
    ESP_LOGE("重启","重启");
}