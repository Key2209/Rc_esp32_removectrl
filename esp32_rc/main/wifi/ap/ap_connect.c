/* Captive Portal Example

    This example code is in the Public Domain (or CC0 licensed, at your option.)

    Unless required by applicable law or agreed to in writing, this
    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
    CONDITIONS OF ANY KIND, either express or implied.
*/

#include "ap_connect.h"
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"

#include "nvs_manager.h"
#include "esp_event_base.h"
#include "udp_task.h"
#define EXAMPLE_ESP_WIFI_SSID "ESP32_1034"
#define EXAMPLE_ESP_WIFI_PASS "20041219"
#define EXAMPLE_MAX_STA_CONN 6

app_config_t my_wifi_config;
/*
root_html
_binary_root_html_start: 指向 HTML 内容在 Flash 中存储的首地址。
_binary_root_html_end: 指向 HTML 内容结束的地址。
*/
extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

static const char *TAG = "example";


//wifi事件回调函数
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    //注册事件处理，用于监听 "有设备连入" 或 "断开"
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));

    //配置wifi信息
    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;//获取wifi的ip信息
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

// HTTP GET Handler
//劫持DNS重定向时候跳转这个页面(回调函数)
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    // 设置响应头 Content-Type 为 text/html
    httpd_resp_set_type(req, "text/html");
    // 发送嵌入在固件里的 HTML 数据
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

static esp_err_t api_config_post_handler(httpd_req_t *req)
{
    char buf[200];
    
    int remaining =req->content_len;
    if (remaining>=sizeof(buf))
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;

    }

    int ret;
    ret=httpd_req_recv(req,buf,remaining);
    if (ret<=0)
    {
        if(ret==HTTPD_SOCK_ERR_TIMEOUT)
        {
            httpd_resp_send_404(req);
        }
    }
    buf[ret]='\0';

    ESP_LOGI(TAG, "收到数据: %s", buf);

    cJSON *root=cJSON_Parse(buf);
    if (root==NULL)
    {
        ESP_LOGE(TAG, "JSON 解析失败 服务器内部错误");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    
// 4. 提取字段 (根据前端传来的 key: ssid, password, dev_name)
    app_config_t app_cfg;
    cJSON *ssid_item = cJSON_GetObjectItem(root, "ssid");
    cJSON *pass_item = cJSON_GetObjectItem(root, "password");
    cJSON *name_item = cJSON_GetObjectItem(root, "dev_name");


    if (cJSON_IsString(ssid_item) && ssid_item->valuestring) {
        strncpy(app_cfg.ssid, ssid_item->valuestring, sizeof(app_cfg.ssid) - 1);
        app_cfg.ssid[sizeof(app_cfg.ssid) - 1] = '\0';  // 确保终止符
    } else {
        strcpy(app_cfg.ssid, "");
    }
    
    if (cJSON_IsString(pass_item) && pass_item->valuestring) {
        strncpy(app_cfg.password, pass_item->valuestring, sizeof(app_cfg.password) - 1);
        app_cfg.password[sizeof(app_cfg.password) - 1] = '\0';
    } else {
        strcpy(app_cfg.password, "");
    }
    
    if (cJSON_IsString(name_item) && name_item->valuestring) {
        strncpy(app_cfg.device_name, name_item->valuestring, sizeof(app_cfg.device_name) - 1);
        app_cfg.device_name[sizeof(app_cfg.device_name) - 1] = '\0';
    } else {
        strcpy(app_cfg.device_name, "");
    }
    app_cfg.config_done=1;
    esp_err_t err=save_config_to_nvs(&app_cfg);
    cJSON_Delete(root);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "配置已保存到 NVS");
        // 发送 HTTP 200 OK
        httpd_resp_send(req, "{\"status\":\"ok\"}", HTTPD_RESP_USE_STRLEN);
        int cnt=0;
        for (int i = 0; i < 4000; i++)
        {
            cnt++;
        }
        

        // 可选：在此处设置一个定时器，1秒后重启系统 esp_restart()
        // 不要直接在这里重启，否则浏览器收不到 OK 响应
        esp_restart();
    } else {
        httpd_resp_send_500(req);
    }

    return ESP_OK;


}


// 定义 URI 结构体，绑定 "/" (即root)到处理函数
static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

static const httpd_uri_t api_config={
    .uri="/api/config",
    .method=HTTP_POST,
    .user_ctx=NULL,
    .handler=api_config_post_handler
};

// HTTP Error (404) Handler - Redirects all requests to the root page
/* * ★★★ 核心逻辑：HTTP 404 错误处理 (重定向) ★★★
 * 这里的机制是：
 * 当手机连接 Wi-Fi 后，会尝试访问 "http://www.google.com/generate_204" 等地址。
 * DNS 服务器欺骗手机说 "google.com 的 IP 是 192.168.4.1 (ESP32)"。
 * 手机向 ESP32 发送 GET 请求，路径是 "/generate_204"。
 * ESP32 的 Web 服务器发现没有注册 "/generate_204" 这个路径，触发 404 错误。
 * 我们注册了这个错误处理函数，用来强制把用户“踢”回首页。
 */
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // 设置 HTTP 状态码 302 (Temporary Redirect，临时重定向)
    httpd_resp_set_status(req, "302 Temporary Redirect");
    
    // 设置 Location 头，告诉浏览器：“你要找的资源不在那，请去 '/' (根目录)”
    httpd_resp_set_hdr(req, "Location", "/");
    
    // iOS 的特殊兼容性处理：iOS 需要响应体里有点内容才能识别出这是个 Captive Portal
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);//根目录页面
        httpd_register_uri_handler(server, &api_config);//api接收接口
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);//404处理
    }
    return server;
}

void wifi_ap_init(void)
{

    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
        关闭日志
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);


    // // Initialize networking stack
    // ESP_ERROR_CHECK(esp_netif_init());

    // // Create default event loop needed by the  main app
    // ESP_ERROR_CHECK(esp_event_loop_create_default());

    // // Initialize NVS needed by Wi-Fi
    // ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi including netif with default config
    esp_netif_create_default_wifi_ap();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    // Start the server for the first time
    start_webserver();

    // Start the DNS server that will redirect all queries to the softAP IP
    start_dns_server();
}





// WiFi事件处理函数
// static void wifi_sta_event_handler(void* arg, esp_event_base_t event_base,
//                                 int32_t event_id, void* event_data)
// {
//    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
//         ESP_LOGI(TAG, "WiFi STA 启动");
//         // esp_wifi_connect(); // 可以在这里自动连接
//     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
//         ESP_LOGI(TAG, "WiFi 断开连接，尝试重连...");
//         // esp_wifi_connect(); // 可以在这里自动重连
//     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
//         ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
//         ESP_LOGI(TAG, "获取到IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        
//         // 方法1：使用事件数据中的IP信息（推荐）
//         ESP_LOGI(TAG, "=== 网络信息（来自事件）===");
//         ESP_LOGI(TAG, "IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
//         ESP_LOGI(TAG, "子网掩码: " IPSTR, IP2STR(&event->ip_info.netmask));
//         ESP_LOGI(TAG, "网关: " IPSTR, IP2STR(&event->ip_info.gw));
        
//         // 方法2：获取STA网络接口的IP信息
//         esp_netif_t *sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
//         if (sta_netif) {
//             esp_netif_ip_info_t ip_info;
//             if (esp_netif_get_ip_info(sta_netif, &ip_info) == ESP_OK) {
//                 ESP_LOGI(TAG, "=== 网络信息（从接口获取）===");
//                 ESP_LOGI(TAG, "IP地址: " IPSTR, IP2STR(&ip_info.ip));
//                 ESP_LOGI(TAG, "子网掩码: " IPSTR, IP2STR(&ip_info.netmask));
//                 ESP_LOGI(TAG, "网关: " IPSTR, IP2STR(&ip_info.gw));
//             }
//         }
//         ESP_LOGI(TAG, "=================");
//     }
// }
// void wifi_init_sta(app_config_t *config)
// {
//     esp_netif_create_default_wifi_sta();
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));

//     // 配置 STA
//     wifi_config_t wifi_config = {0};
    
//     // 把我们的 struct 数据复制给 ESP-IDF 的 struct
//     strncpy((char *)wifi_config.sta.ssid, config->ssid, sizeof(wifi_config.sta.ssid));
//     strncpy((char *)wifi_config.sta.password, config->password, sizeof(wifi_config.sta.password));
    
//     ESP_LOGI(TAG, "Connecting to SSID: %s", wifi_config.sta.ssid);
//         // 注册事件处理
//     esp_event_handler_instance_t instance_any_id;
//     esp_event_handler_instance_t instance_got_ip;
//     esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_sta_event_handler, NULL, &instance_any_id);
//     esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_sta_event_handler, NULL, &instance_got_ip);

//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config));
//     ESP_ERROR_CHECK(esp_wifi_start());
    
//     esp_wifi_connect(); // 开始连接
// }



void wifi_app_init()
{

    // 1. 初始化 NVS (必须放在最前面)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // 2. 尝试读取配置
    
    esp_err_t load_err = load_config_from_nvs(&my_wifi_config);

    // 3. 判断逻辑
    if (load_err == ESP_OK && my_wifi_config.config_done == 1) {
        // --- 模式 A: 有配置，连接路由器 ---
        ESP_LOGI(TAG, "Configuration found! Device Name: %s", my_wifi_config.device_name);
        wifi_init_sta(&my_wifi_config);

    } else {
        // --- 模式 B: 无配置，开启热点配网 ---
        ESP_LOGI(TAG, "No config found. Starting Captive Portal...");
        
        // 调用你之前的 wifi_init_softap()
        // 注意：在 wifi_init_softap 里记得调用 start_webserver() 和 start_dns_server()
        wifi_ap_init(); 
    }


}