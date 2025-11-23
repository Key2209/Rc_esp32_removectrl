#include "udp_task.h"
#include "ap_connect.h"
#include "mdns.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_event_base.h"

static char TAG[] = "UDP_TASK";

// 全局队列句柄
QueueHandle_t robot_ctrl_queue = NULL;

/**
 * @brief 初始化 mDNS 服务
 * 手机可以通过 "esp32-robot.local" 找到设备
 */
void start_mdns_service()
{
    // 初始化 mDNS
    ESP_ERROR_CHECK(mdns_init());

    // 设置主机名: my-robot.local
    // my_wifi_config.device_name
    ESP_ERROR_CHECK(mdns_hostname_set("my-robot"));

    // 设置实例名 (可选，用于服务发现)
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Robot Chassis"));

    // 添加服务: _udp 表示我们使用 UDP 协议, Port 3333
    ESP_ERROR_CHECK(mdns_service_add("ESP32-Robot", "_robot", "_udp", UDP_PORT, NULL, 0));

    ESP_LOGI("MDNS", "mDNS started. You can ping 'my-robot.local'");
}
// 比较两个 sockaddr_in 是否相同 (IP 和 Port 都要一样)
static bool is_same_client(struct sockaddr_in *a, struct sockaddr_in *b)
{
    return (a->sin_addr.s_addr == b->sin_addr.s_addr) &&
           (a->sin_port == b->sin_port);
}
void udp_server_task(void *pvParameters)
{

    robot_ctrl_queue = xQueueCreate(5, sizeof(robot_ctrl_t));
    char rx_buffer[256];
    struct sockaddr_in dest_addr;

    // 准备 Socket 地址
    dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(UDP_PORT);

    // 创建 Socket
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }

    // 绑定端口
    int err = bind(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0)
    {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // 默认情况下，recvfrom 是死等的（阻塞）。如果没有数据，任务会一直卡在那里不动。
    // 我们设置 100ms 超时，意味着 recvfrom 最多等 100ms。
    // 重要：设置 Socket 接收超时
    // 这样 recvfrom 不会死等，每隔 100ms 会返回一次，让我们有机会检查会话是否超时
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100ms
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in source_addr;
    socklen_t socklen = sizeof(source_addr);

    ESP_LOGI("UDP", "Waiting for data...");

    while (1)
    {
        // 1. 尝试接收数据 (带超时)
        int len = recvfrom(sock, rx_buffer, sizeof(rx_buffer) - 1, 0, (struct sockaddr *)&source_addr, &socklen);

        uint32_t now = xTaskGetTickCount(); // 获取当前系统滴答
        //ESP_LOGE("UDP", "正在运行");
        // 2. 检查会话是否超时 (看门狗)
        if (g_session.state == SESSION_LOCKED)
        {
            uint32_t diff = (now - g_session.last_packet_tick) * portTICK_PERIOD_MS;
            if (diff > SESSION_TIMEOUT_MS)
            {
                ESP_LOGW("SESSION", "Client timed out! Resetting to IDLE.");
                g_session.state = SESSION_IDLE;
            }
        }

        // 3. 处理接收到的数据
        if (len > 0)
        {
            rx_buffer[len] = 0;
            ESP_LOGI("UDP", "Recv: %s", rx_buffer);

            cJSON *root = cJSON_Parse(rx_buffer);
            if (root)
            {
                cJSON *cmd_item = cJSON_GetObjectItem(root, "cmd");
                char *cmd_str = cmd_item ? cJSON_GetStringValue(cmd_item) : "";

                // ============================
                // 场景 A: 处理连接请求 "connect"
                // ============================
                if (strcmp(cmd_str, "connect") == 0)
                {
                    if (g_session.state == SESSION_IDLE)
                    {
                        g_session.state = SESSION_LOCKED;    // 状态变更为锁定
                        g_session.client_addr = source_addr; // 记下这个人的地址
                        g_session.last_packet_tick = now;    // 记录时间

                        ESP_LOGI("SESSION", "New client connected!");

                        // 发送回复 (ACK)，告诉手机连接成功
                        const char *reply = "{\"status\":\"ok\"}";
                        sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    }
                    else if (is_same_client(&source_addr, &g_session.client_addr))
                    {
                        // 已经是这个人了，重置心跳
                        g_session.last_packet_tick = now;
                        const char *reply = "{\"status\":\"ok\"}";
                        sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    }
                    else
                    {
                        // 已经有别人连接了，拒绝!
                        ESP_LOGW("SESSION", "Rejecting connection from other IP");
                        const char *reply = "{\"status\":\"busy\"}";
                        sendto(sock, reply, strlen(reply), 0, (struct sockaddr *)&source_addr, sizeof(source_addr));
                    }
                }

                // ============================
                // 场景 B: 处理控制指令 "ctrl"
                // ============================
                else if (strcmp(cmd_str, "ctrl") == 0)
                {
                    // 只有处于锁定状态，且 IP 匹配，才执行
                    if (g_session.state == SESSION_LOCKED && is_same_client(&source_addr, &g_session.client_addr))
                    {
                        // 喂狗：更新最后通信时间
                        g_session.last_packet_tick = now;

                        cJSON *t = cJSON_GetObjectItem(root, "t");
                        cJSON *s = cJSON_GetObjectItem(root, "s");

                        if (t && s)
                        {
                            robot_ctrl_t ctrl_data;
                            ctrl_data.x = t->valueint;
                            ctrl_data.y = s->valueint;
                            // 发送到电机任务
                            xQueueOverwrite(robot_ctrl_queue, &ctrl_data);
                        }
                    }
                    else
                    {
                        // 忽略非连接者的控制指令
                        // ESP_LOGD("UDP", "Ignored packet from unauthorized source");
                    }
                }

                // ============================
                // 场景 C: 处理断开请求 "disconnect"
                // ============================
                else if (strcmp(cmd_str, "disconnect") == 0)
                {
                    if (g_session.state == SESSION_LOCKED && is_same_client(&source_addr, &g_session.client_addr))
                    {
                        ESP_LOGI("SESSION", "Client requested disconnect.");
                        g_session.state = SESSION_IDLE;
                        // 电机应该也要停下来
                        robot_ctrl_t stop_data = {0, 0};
                        xQueueOverwrite(robot_ctrl_queue, &stop_data);
                    }
                }

                cJSON_Delete(root);
            }
        }
    }
    vTaskDelete(NULL);
}

void wifi_udp_init()
{

    start_mdns_service();

    xTaskCreate(udp_server_task, "udp_task", 4096, NULL, 5, NULL);
}

static void wifi_sta_event_handler(void *arg, esp_event_base_t event_base,
                                   int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "WiFi STA 启动");
        esp_wifi_connect(); // 可以在这里自动连接
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "WiFi 断开连接，尝试重连...");
        esp_wifi_connect(); // 可以在这里自动重连
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        // ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        // // 方法1：使用事件数据中的IP信息（推荐）
        // ESP_LOGI(TAG, "=== 网络信息（来自事件）===");
        // ESP_LOGI(TAG, "IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        // ESP_LOGI(TAG, "子网掩码: " IPSTR, IP2STR(&event->ip_info.netmask));
        // ESP_LOGI(TAG, "网关: " IPSTR, IP2STR(&event->ip_info.gw));

        // 方法2：获取STA网络接口的IP信息
        esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (sta_netif)
        {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK)
            {
                ESP_LOGI(TAG, "=== 网络信息（从接口获取）===");
                ESP_LOGI(TAG, "IP地址: " IPSTR, IP2STR(&ip_info.ip));
                ESP_LOGI(TAG, "子网掩码: " IPSTR, IP2STR(&ip_info.netmask));
                ESP_LOGI(TAG, "网关: " IPSTR, IP2STR(&ip_info.gw));
                // 启动网络服务
                ESP_LOGI(TAG, "网络服务启动......");
                
                wifi_udp_init();

                ESP_LOGI(TAG, "网络服务启动完成");
            }
        }
        ESP_LOGI(TAG, "=================");
    }
}
void wifi_init_sta(app_config_t *config)
{
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 配置 STA
    wifi_config_t wifi_config = {0};

    // 把我们的 struct 数据复制给 ESP-IDF 的 struct
    strncpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));

    ESP_LOGI(TAG, "Connecting to SSID: %s", wifi_config.sta.ssid);
    // 注册事件处理
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, &instance_any_id);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, &instance_got_ip);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_set_ps(WIFI_PS_NONE);
    // esp_wifi_connect(); // 开始连接
}
