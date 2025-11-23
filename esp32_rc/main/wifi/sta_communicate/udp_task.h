#ifndef UDP_TASK_H
#define UDP_TASK_H

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "mdns.h"
#include "cJSON.h"
#include "ap_connect.h"




// 超时设置 (毫秒)
#define SESSION_TIMEOUT_MS 3000  // 3秒没收到控制者的消息，自动踢下线
#define MOTOR_FAILSAFE_MS  500   // 500ms 没收到新指令，电机自动停转
#define UDP_PORT       3333
// 会话状态
typedef enum {
    SESSION_IDLE,      // 空闲，等待连接
    SESSION_LOCKED     // 锁定，只听这一个人的
} session_state_t;

// 会话上下文
typedef struct {
    session_state_t state;
    struct sockaddr_in client_addr; // 记录当前连接者的地址
    uint32_t last_packet_tick;      // 最后一次收到合法包的时间
} udp_session_t;




static udp_session_t g_session = { .state = SESSION_IDLE };

// 控制指令结构体
typedef struct {
    int x; // 
    int y; // 
} robot_ctrl_t;




void wifi_init_sta(app_config_t *config);

#endif

