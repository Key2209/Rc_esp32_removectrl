/* my_ota.c */
#include "my_ota.h"
#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_app_format.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// 日志标签
static const char *TAG = "MY_OTA";

// 定义缓冲区大小 (通常 1024 字节是一个比较平衡的选择)
#define OTA_BUFFSIZE 1024

/* * --------------------------------------------------------------------------
 * 核心函数：处理 OTA 上传请求
 * --------------------------------------------------------------------------
 */
esp_err_t ota_update_handler(httpd_req_t *req)
{
    esp_ota_handle_t update_handle = 0; // OTA 操作句柄
    const esp_partition_t *update_partition = NULL; // 目标分区指针
    esp_err_t err;

    ESP_LOGI(TAG, "开始 OTA 固件更新...");

    /* * 1. 获取下一个用于写入的 OTA 分区 
     * 如果当前运行在 ota_0，它会返回 ota_1，反之亦然。
     */
    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "致命错误：无法找到可用的 OTA 分区！");
        httpd_resp_send_500(req); // 返回服务器错误 500
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "正在写入目标分区: subtype %d at offset 0x%08"PRIx32,
             update_partition->subtype, update_partition->address);

    /* * 2. 准备 OTA 写入 (esp_ota_begin)
     * OTA_SIZE_UNKNOWN: 因为是流式上传，我们可能不知道确切大小，设为未知。
     * 这一步会擦除目标分区，需要一点时间。
     */
    err = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin 失败 (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "esp_ota_begin 成功，开始接收数据...");

    /* * 3. 循环接收 HTTP 数据并写入 Flash
     */
    char *ota_buff = malloc(OTA_BUFFSIZE); // 申请堆内存作为缓冲区
    if (ota_buff == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        esp_ota_abort(update_handle); // 终止 OTA
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    int remaining = req->content_len; // HTTP 请求体总长度（文件大小）
    int total_received = 0;           // 已接收总长度
    bool is_image_header_checked = false; // 是否已检查固件头（可选逻辑）

    while (remaining > 0) {
        // --- A. 从 HTTP 请求读取数据 ---
        // 注意：读取长度不能超过缓冲区大小，也不能超过剩余数据量
        int data_read = httpd_req_recv(req, ota_buff, MIN(remaining, OTA_BUFFSIZE));
        
        // 错误处理：连接断开或超时
        if (data_read < 0) {
            if (data_read == HTTPD_SOCK_ERR_TIMEOUT) {
                // 如果仅仅是超时，可以尝试重试 (continue)，或者直接报错
                // 这里为了安全起见，通常选择报错退出
                ESP_LOGE(TAG, "HTTP 读取超时");
            } else {
                ESP_LOGE(TAG, "HTTP 读取错误");
            }
            free(ota_buff);
            esp_ota_abort(update_handle); // 清理 OTA 句柄
            return ESP_FAIL;
        } else if (data_read == 0) {
            // 读取到 0 字节，通常意味着连接关闭
            ESP_LOGW(TAG, "连接意外关闭");
            free(ota_buff);
            esp_ota_abort(update_handle);
            return ESP_FAIL;
        }

        // --- B. 将读取到的数据写入 Flash ---
        err = esp_ota_write(update_handle, (const void *)ota_buff, data_read);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Flash 写入失败 (%s)", esp_err_to_name(err));
            free(ota_buff);
            esp_ota_abort(update_handle);
            return ESP_FAIL;
        }

        total_received += data_read;
        remaining -= data_read;

        // 打印进度日志 (每收到一部分数据打一次，可选)
        // ESP_LOGD(TAG, "已写入: %d / %d", total_received, req->content_len);
    }
    
    // 释放缓冲区
    free(ota_buff);
    ESP_LOGI(TAG, "数据接收完成，共接收: %d bytes", total_received);

    // /* * 4. 结束 OTA 并进行校验 (esp_ota_end)
    //  * ★★★ 关键安全步骤 ★★★
    //  * - 此函数会验证写入数据的 MD5/SHA256 完整性。
    //  * - 如果你开启了【固件签名 (Secure Boot/App Signing)】，
    //  * 它会在这里使用内部公钥验证固件末尾的签名。
    //  * - 如果签名不对，这里会直接返回错误！
    //  */

    // err = esp_ota_end(update_handle);
    // if (err != ESP_OK) {
    //     if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
    //         ESP_LOGE(TAG, "固件校验失败！(签名无效或文件损坏)");
    //     } else {
    //         ESP_LOGE(TAG, "OTA 结束阶段失败 (%s)", esp_err_to_name(err));
    //     }
    //     httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "OTA Validation Failed");
    //     return ESP_FAIL;
    // }

    /* * 5. 设置启动分区 (修改 otadata)
     * 告诉 Bootloader 下次重启时加载这个新分区。
     */
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置启动分区失败 (%s)", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA 升级成功！准备重启...");

    /* * 6. 向前端发送成功响应
     */
    httpd_resp_send(req, "Success", HTTPD_RESP_USE_STRLEN);

    /* * 7. 延时并重启
     * 必须给一点延时，确保 HTTP "Success" 响应能成功发回给手机。
     * 如果立即 esp_restart()，Wi-Fi 会瞬间断开，手机可能收不到成功提示。
     */
    vTaskDelay(pdMS_TO_TICKS(1000)); // 延时 1 秒
    esp_restart();

    return ESP_OK;
}

/*
 * 注册函数结构体配置
 */
static const httpd_uri_t ota_uri = {
    .uri       = "/api/ota",   // 前端 POST 的路径
    .method    = HTTP_POST,    // 方法必须是 POST
    .handler   = ota_update_handler, // 绑定上面的处理函数
    .user_ctx  = NULL
};

/*
 * 外部调用的注册函数
 */
void register_ota_handler(httpd_handle_t server)
{
    if (server == NULL) {
        ESP_LOGE(TAG, "Web Server 句柄为空，无法注册 OTA 接口");
        return;
    }
    ESP_LOGI(TAG, "注册 OTA 接口: /api/ota");
    httpd_register_uri_handler(server, &ota_uri);
}