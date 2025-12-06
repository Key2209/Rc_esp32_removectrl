file(GLOB_RECURSE SRC_UI ${CMAKE_SOURCE_DIR} "ui/*.c")



idf_component_register(SRCS 

                   
                    "esp32_rc.c"
                    "lcd/lcd_init.c"
                    "lcd/reset_ui.c"
                    "lcd/lvgl_task.c"
                    ${SRC_UI}


                    "wifi/ap/ap_connect.c"
                    "wifi/ap/dns_server.c"
                    "wifi/ap/nvs_manager.c"
                    "wifi/ap/my_ota.c"


                    "wifi/sta_communicate/udp_task.c"
                    "wifi/sta_communicate/uart_send_task.c"
                    INCLUDE_DIRS
                     "."
                     "lcd"
                     "ui"
                     "wifi"
                     "wifi/ap"
                    "wifi/sta_communicate"


                     EMBED_FILES wifi/ap/root.html
                     )
