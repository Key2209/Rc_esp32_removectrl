#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_event.h"
#include "udp_task.h"


/* UART asynchronous example, that uses separate RX and TX tasks

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_system.h"
#include "esp_log.h"
#include "driver/uart.h"
#include "string.h"
#include "driver/gpio.h"

static const int RX_BUF_SIZE = 1024;

#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

void init(void)
{
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    // We won't use a buffer for sending data.
    uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
}

int sendData(const char* logName, const char* data)
{
    const int len = strlen(data);
    const int txBytes = uart_write_bytes(UART_NUM_1, data, len);
    ESP_LOGI(logName, "Wrote %d bytes", txBytes);
    return txBytes;
}


static void rx_task(void *arg)
{
    static const char *RX_TASK_TAG = "RX_TASK";
    esp_log_level_set(RX_TASK_TAG, ESP_LOG_INFO);
    uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE + 1);
    while (1) {
        const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
        if (rxBytes > 0) {
            data[rxBytes] = 0;
            ESP_LOGI(RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
            ESP_LOG_BUFFER_HEXDUMP(RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
        }
    }
    free(data);
}


void uart_send_task(void *pvParameters)
{
    const char *TAG = "UART_SEND_TASK";
    UiDataStruct ui_data;
    for(;;)
    {
        //ESP_LOGE(TAG, "Waiting for data from UDP task..." );
        // 等待接收来自 UDP 任务的控制数据
        // if (xQueueReceive(robot_ctrl_queue, &ui_data, portMAX_DELAY) == pdTRUE)
        // {
            
        //     // 这里将接收到的数据通过 UART 发送出去
        //     // 假设有一个函数 uart_send_data() 用于发送数据
        //     // uart_send_data(&ui_data, sizeof(UiDataStruct));
        //     char buffer[2048];
        //     snprintf(buffer,sizeof(buffer),
        //              "BEGIN,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,END",
        //              ui_data.joystick1.x, ui_data.joystick1.y,
        //              ui_data.joystick2.x, ui_data.joystick2.y,
        //              ui_data.scroller_horiz1, ui_data.scroller_vertical1,
        //                 ui_data.button_group1[0], ui_data.button_group1[1], ui_data.button_group1[2],
        //                 ui_data.button_group1[3], ui_data.button_group1[4], ui_data.button_group1[5],
        //                 ui_data.button_group1[6], ui_data.button_group1[7], ui_data.button_group1[8], 
        //                 ui_data.button_group1[9]
        //              );


        //     sendData("UART_SEND",buffer);
        //     ESP_LOGE(TAG, "Sent Data: %s", buffer );
        //     //ESP_LOGE(TAG, "Sent data over UART" );
        // }


        if (xQueueReceive(robot_ctrl_queue, &ui_data, 500) == pdTRUE)
        {
            
            // 这里将接收到的数据通过 UART 发送出去
            // 假设有一个函数 uart_send_data() 用于发送数据
            // uart_send_data(&ui_data, sizeof(UiDataStruct));
            char buffer[2048];
            snprintf(buffer,sizeof(buffer),
                     "BEGIN,%.5f,%.5f,%.5f,%.5f,%.2f,%.2f,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,END",
                     ui_data.joystick1.x, ui_data.joystick1.y,
                     ui_data.joystick2.x, ui_data.joystick2.y,
                     ui_data.scroller_horiz1, ui_data.scroller_vertical1,
                        ui_data.button_group1[0], ui_data.button_group1[1], ui_data.button_group1[2],
                        ui_data.button_group1[3], ui_data.button_group1[4], ui_data.button_group1[5],
                        ui_data.button_group1[6], ui_data.button_group1[7], ui_data.button_group1[8], 
                        ui_data.button_group1[9]
                     );


            sendData("UART_SEND",buffer);
            ESP_LOGE(TAG, "Sent Data: %s", buffer );
            //ESP_LOGE(TAG, "Sent data over UART" );
        }
        else
        {
            // 超时未收到数据，可以选择发送心跳包或执行其他操作
            char buffer[512*2];
            snprintf(buffer,sizeof(buffer),
                     "BEGIN,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,END"
                     );
            sendData("UART_SEND",buffer);
            ESP_LOGE(TAG, "Sent Heartbeat Data: %s", buffer );
        }
    }
    vTaskDelete(NULL);
}





void uart_task_init(void)
{
    init();
    xTaskCreate(rx_task, "uart_rx_task", 1024 * 2, NULL, configMAX_PRIORITIES - 1, NULL);
    xTaskCreate(uart_send_task, "uart_tx_task", 1024 * 8, NULL, configMAX_PRIORITIES - 1, NULL);
}




