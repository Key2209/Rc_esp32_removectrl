/* myota.h */
#ifndef MY_OTA_H
#define MY_OTA_H

#include "esp_err.h"
#include "esp_http_server.h"

/**
 * @brief 注册 OTA 相关的 URI 处理函数到 Web 服务器
 * * 在你的主程序启动 Web Server 后调用此函数，将 /api/ota 接口挂载上去。
 * * @param server Web 服务器的句柄
 */
void register_ota_handler(httpd_handle_t server);

/**
 * @brief OTA 更新的核心处理函数 (HTTP POST Handler)
 * * 当前端向 /api/ota 发送 .bin 文件时，此函数会被触发。
 * 它负责接收数据、写入 Flash、校验固件并设置重启。
 */
esp_err_t ota_update_handler(httpd_req_t *req);

#endif // MY_OTA_H