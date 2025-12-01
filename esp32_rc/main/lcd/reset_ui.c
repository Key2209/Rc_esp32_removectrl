#include "lvgl.h"
#include "lcd_init.h"
#include "esp_log.h"

// --- 全局对象指针 (确保在 .c 文件顶部定义) ---
static lv_obj_t *reset_screen;
static lv_obj_t *arc_reset;
static lv_obj_t *label_reset;

// 静态样式对象（优化，仅初始化一次）
static lv_style_t style_arc_bg;
static lv_style_t style_arc_ind;
static lv_style_t style_screen;

/**
 * @brief 创建 "升级版炫酷 Arc" 重置进度条屏幕
 */
void ui_create_reset_screen_arc(void)
{
    // 1. 创建全新屏幕对象 (不会污染当前活动屏幕)
    reset_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(reset_screen);

    // 设置深蓝黑背景，并移除边框等默认样式
    // 运行时设置样式，需要选择器参数
    lv_obj_set_style_bg_color(reset_screen, lv_color_hex(0x050510), 0);
    lv_obj_set_style_bg_opa(reset_screen, LV_OPA_COVER, 0);

    // 2. 顶部标题 - SYSTEM RESET
    lv_obj_t *header_label = lv_label_create(reset_screen);
    lv_label_set_text(header_label, "CONFIGURATION ERASE");
    lv_obj_set_style_text_color(header_label, lv_color_make(0x00, 0xAA, 0xFF), 0); // 亮蓝色
    lv_obj_set_style_text_font(header_label, &lv_font_montserrat_14, 0);
    lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 15);

    // 3. 创建 Arc (圆弧) 对象
    arc_reset = lv_arc_create(reset_screen); // 以新屏幕为父对象
    lv_obj_set_size(arc_reset, 180, 180);//略小一些，留出空间给标题和阴影
    lv_obj_center(arc_reset);
    lv_obj_set_y(arc_reset, 20); // 略微向下移动

    // Arc 基础配置
    lv_arc_set_rotation(arc_reset, 270);
    lv_arc_set_bg_angles(arc_reset, 0, 360);
    lv_arc_set_range(arc_reset, 0, 100);
    lv_arc_set_value(arc_reset, 0);
    lv_obj_remove_style(arc_reset, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_reset, LV_OBJ_FLAG_CLICKABLE);

    // 4. 样式美化 (使用阴影增加炫酷感)

    // 轨道样式 (MAIN) - 灰色底，厚度 18px
    lv_style_init(&style_arc_bg);
    // 🔴 修正：移除多余的参数 0，兼容旧版 lv_style_set_X API
    lv_style_set_arc_color(&style_arc_bg, lv_color_hex(0x303030));
    lv_style_set_arc_width(&style_arc_bg, 18);
    lv_obj_add_style(arc_reset, &style_arc_bg, LV_PART_MAIN);

    // 指示器样式 (INDICATOR) - 纯色和发光
    lv_style_init(&style_arc_ind);
    lv_style_set_arc_width(&style_arc_ind, 18);
    lv_style_set_arc_rounded(&style_arc_ind, true); // 圆角端点

    // 进度条纯色：青色
    lv_style_set_arc_color(&style_arc_ind, lv_palette_main(LV_PALETTE_CYAN));
    // 🔴 移除：不支持的 arc_grad_color

    // 阴影/发光效果
    // 🔴 修正：移除多余的参数，兼容旧版 lv_style_set_X API
    lv_style_set_shadow_color(&style_arc_ind, lv_palette_main(LV_PALETTE_CYAN));
    lv_style_set_shadow_width(&style_arc_ind, 10);
    lv_style_set_shadow_spread(&style_arc_ind, 2);

    lv_obj_add_style(arc_reset, &style_arc_ind, LV_PART_INDICATOR);

    // 5. 中间添加提示文字
    label_reset = lv_label_create(arc_reset);
    lv_obj_center(label_reset);
    lv_label_set_text(label_reset, "HOLD\n3.0s"); // 提示用户需要时长
    lv_obj_set_style_text_align(label_reset, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(label_reset, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_reset, &lv_font_montserrat_14, 0); // 字体稍大
}

/**
 * @brief 将重置屏幕的状态重置为初始的 0% 状态。
 * （在每次加载屏幕前调用）
 */
static void ui_reset_state_arc(void)
{
    if (!arc_reset || !label_reset)
        return;

    // 1. 重置 Arc 值到 0
    lv_arc_set_value(arc_reset, 0);

    // 2. 恢复指示器和文本的样式和颜色

    // 恢复 Arc 指示器颜色 (纯色)
    lv_obj_set_style_arc_color(arc_reset, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
    
    // 🔴 修正：移除不支持的 bg_grad_color。Arc 指示器样式通过 arc_color 控制。
    // lv_obj_set_style_bg_grad_color(arc_reset, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
    
    // 恢复阴影/发光颜色
    lv_obj_set_style_shadow_color(arc_reset, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);

    // 恢复文字
    lv_obj_set_style_text_color(label_reset, lv_color_white(), 0);
    lv_label_set_text(label_reset, "HOLD\n3.0s");
}
// 屏幕加载辅助函数
void ui_load_reset_screen(void)
{
    if (reset_screen)
    {
        ui_reset_state_arc();
        lv_disp_load_scr(reset_screen);
    }
}




/**
 * @brief 更新进度条状态
 * @param progress 0 - 100 的整数
 */
void ui_update_reset_progress_arc(int progress) {
    if (!arc_reset) return;

    if(progress < 0) progress = 0;
    if(progress > 100) progress = 100;

    // 设置 Arc 的值
    lv_arc_set_value(arc_reset, progress);
    
    // 动态效果逻辑
    if (progress >= 100) {
        // --- 状态：完成 (强烈的红色警告) ---
        
        // 进度条和发光变为纯红
        lv_obj_set_style_arc_color(arc_reset, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
        //lv_obj_set_style_arc_grad_color(arc_reset, lv_palette_darken(LV_PALETTE_RED, 2), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(arc_reset, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
        
        // 文字变为红色，提示用户操作完成
        lv_obj_set_style_text_color(label_reset, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(label_reset, "RESET\nDONE!");
        
    } else {
        // --- 状态：进行中 ---
        
        // 恢复为青色/渐变色
        lv_obj_set_style_arc_color(arc_reset, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
        //lv_obj_set_style_arc_grad_color(arc_reset, lv_palette_main(LV_PALETTE_BLUE), LV_PART_INDICATOR);
        lv_obj_set_style_shadow_color(arc_reset, lv_palette_main(LV_PALETTE_CYAN), LV_PART_INDICATOR);
        
        // 恢复文字为白色
        lv_obj_set_style_text_color(label_reset, lv_color_white(), 0);
        
        // 动态显示百分比
        if (progress > 0) {
            lv_label_set_text_fmt(label_reset, "Wait...\n%d%%", progress);
        } else {
            lv_label_set_text(label_reset, "HOLD\n3.0s");
        }
    }
}

#include "udp_task.h"
// This file was customized for LVGL 8.3 with SquareLine style
// Variables and function names kept exactly the same as your project


#include <stdio.h>

lv_obj_t * ui_DataScreen = NULL;
lv_obj_t * uic_DataScreen = NULL;

// // labels
static lv_obj_t * lbl_title;

// joystick1
static lv_obj_t * lbl_j1_x;
static lv_obj_t * lbl_j1_y;
static lv_obj_t * lbl_j1_l;
static lv_obj_t * lbl_j1_a;

// joystick2
static lv_obj_t * lbl_j2_x;
static lv_obj_t * lbl_j2_y;
static lv_obj_t * lbl_j2_l;
static lv_obj_t * lbl_j2_a;

// scrollers
static lv_obj_t * lbl_h1;
static lv_obj_t * lbl_v1;

// buttons
static lv_obj_t * lbl_b1[10];
static lv_obj_t * lbl_b2[10];

static lv_obj_t * add_label(lv_obj_t *parent, int x, int y, const char *txt)
{
    // lv_obj_t *lbl = lv_label_create(parent);
    // lv_obj_set_pos(lbl, x, y);
    // lv_label_set_text(lbl, txt);
    // lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    // return lbl;

    lv_obj_t *lbl = lv_label_create(parent);                  // 创建 label
    lv_obj_set_pos(lbl, x, y);                                // 设置位置
    lv_label_set_text(lbl, txt);                              // 设置文本
    //lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0); // 白色字体
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);  // 14号字体
    return lbl;                                               // 返回对象
}

// void ui_DataScreen_screen_init(void)
// {
//     ui_DataScreen = lv_obj_create(NULL);
//     lv_obj_clear_flag(ui_DataScreen, LV_OBJ_FLAG_SCROLLABLE);

//     uic_DataScreen = ui_DataScreen;  // SquareLine style

//     // Title
//     lbl_title = add_label(ui_DataScreen, 5, 5, "RC DATA");

//     // --- Joystick 1 ---
//     add_label(ui_DataScreen, 5, 25, "J1");
//     lbl_j1_x = add_label(ui_DataScreen, 30, 25, "x:0");
//     lbl_j1_y = add_label(ui_DataScreen, 100, 25, "y:0");
//     lbl_j1_l = add_label(ui_DataScreen, 170, 25, "L:0");
//     lbl_j1_a = add_label(ui_DataScreen, 240, 25, "A:0");

//     // --- Joystick 2 ---
//     add_label(ui_DataScreen, 5, 45, "J2");
//     lbl_j2_x = add_label(ui_DataScreen, 30, 45, "x:0");
//     lbl_j2_y = add_label(ui_DataScreen, 100, 45, "y:0");
//     lbl_j2_l = add_label(ui_DataScreen, 170, 45, "L:0");
//     lbl_j2_a = add_label(ui_DataScreen, 240, 45, "A:0");

//     // --- Scrollers ---
//     add_label(ui_DataScreen, 5, 70, "H1:");
//     lbl_h1 = add_label(ui_DataScreen, 40, 70, "0");

//     add_label(ui_DataScreen, 120, 70, "V1:");
//     lbl_v1 = add_label(ui_DataScreen, 155, 70, "0");

//     // --- Buttons group 1 ---
//     add_label(ui_DataScreen, 5, 95, "B1:");
//     int x = 35;
//     for (int i = 0; i < 10; i++) {
//         char buf[8];
//         sprintf(buf, "%d:0", i);
//         lbl_b1[i] = add_label(ui_DataScreen, x, 95, buf);
//         x += 25;
//     }

//     // --- Buttons group 2 ---
//     add_label(ui_DataScreen, 5, 115, "B2:");
//     x = 35;
//     for (int i = 0; i < 10; i++) {
//         char buf[8];
//         sprintf(buf, "%d:0", i);
//         lbl_b2[i] = add_label(ui_DataScreen, x, 115, buf);
//         x += 25;
//     }
// }

// void ui_DataScreen_screen_destroy(void)
// {
//     if (ui_DataScreen)
//         lv_obj_del(ui_DataScreen);

//     ui_DataScreen = NULL;
//     uic_DataScreen = NULL;

//     // labels auto cleaned by lv_obj_del
// }


// // ======================
// //  Update function
// // ======================
// void ui_update_data_screen(UiDataStruct data)
// {
//     char buf[32];

//     // J1
//     sprintf(buf, "x:%.2f", data.joystick1.x);
//     lv_label_set_text(lbl_j1_x, buf);

//     sprintf(buf, "y:%.2f", data.joystick1.y);
//     lv_label_set_text(lbl_j1_y, buf);

//     sprintf(buf, "L:%.2f", data.joystick1.long_value);
//     lv_label_set_text(lbl_j1_l, buf);

//     sprintf(buf, "A:%d", data.joystick1.angle);
//     lv_label_set_text(lbl_j1_a, buf);

//     // J2
//     sprintf(buf, "x:%.2f", data.joystick2.x);
//     lv_label_set_text(lbl_j2_x, buf);

//     sprintf(buf, "y:%.2f", data.joystick2.y);
//     lv_label_set_text(lbl_j2_y, buf);

//     sprintf(buf, "L:%.2f", data.joystick2.long_value);
//     lv_label_set_text(lbl_j2_l, buf);

//     sprintf(buf, "A:%d", data.joystick2.angle);
//     lv_label_set_text(lbl_j2_a, buf);

//     // Scrollers
//     sprintf(buf, "%.2f", data.scroller_horiz1);
//     lv_label_set_text(lbl_h1, buf);

//     sprintf(buf, "%.2f", data.scroller_vertical1);
//     lv_label_set_text(lbl_v1, buf);

//     // Buttons
//     for (int i = 0; i < 10; i++) {
//         sprintf(buf, "%d:%d", i, data.button_group1[i]);
//         lv_label_set_text(lbl_b1[i], buf);

//         sprintf(buf, "%d:%d", i, data.button_group2[i]);
//         lv_label_set_text(lbl_b2[i], buf);
//     }
// }






// void ui_DataScreen_screen_init(void)
// {
//     ui_DataScreen = lv_obj_create(NULL);
//     lv_obj_clear_flag(ui_DataScreen, LV_OBJ_FLAG_SCROLLABLE);

//     // 背景颜色
//     lv_obj_set_style_bg_color(ui_DataScreen, lv_color_hex(0x202020), 0);
//     lv_obj_set_style_text_color(ui_DataScreen, lv_color_hex(0xffffff), 0);

//     // 字体统一
//     const lv_font_t *font = &lv_font_montserrat_14;

//     // ====== 标题 ======
//     lbl_title = lv_label_create(ui_DataScreen);
//     lv_label_set_text(lbl_title, "RC MONITOR");
//     lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0);
//     lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 5);


//     // ====== J1 BOX ======
//     lv_obj_t *j1_box = lv_obj_create(ui_DataScreen);
//     lv_obj_set_size(j1_box, 220, 55);
//     lv_obj_align(j1_box, LV_ALIGN_TOP_MID, 0, 35);
//     lv_obj_set_style_bg_color(j1_box, lv_color_hex(0x303030), 0);
//     lv_obj_set_style_radius(j1_box, 10, 0);
//     lv_obj_set_style_pad_all(j1_box, 6, 0);

//     add_label(j1_box, 5, 0, "J1");

//     lbl_j1_x = add_label(j1_box, 40, 0, "X:0");
//     lbl_j1_y = add_label(j1_box, 130, 0, "Y:0");

//     lbl_j1_l = add_label(j1_box, 40, 25, "L:0");
//     lbl_j1_a = add_label(j1_box, 130, 25, "A:0");


//     // ====== J2 BOX ======
//     lv_obj_t *j2_box = lv_obj_create(ui_DataScreen);
//     lv_obj_set_size(j2_box, 220, 55);
//     lv_obj_align(j2_box, LV_ALIGN_TOP_MID, 0, 95);
//     lv_obj_set_style_bg_color(j2_box, lv_color_hex(0x303030), 0);
//     lv_obj_set_style_radius(j2_box, 10, 0);
//     lv_obj_set_style_pad_all(j2_box, 6, 0);

//     add_label(j2_box, 5, 0, "J2");

//     lbl_j2_x = add_label(j2_box, 40, 0, "X:0");
//     lbl_j2_y = add_label(j2_box, 130, 0, "Y:0");

//     lbl_j2_l = add_label(j2_box, 40, 25, "L:0");
//     lbl_j2_a = add_label(j2_box, 130, 25, "A:0");


//     // ====== 滚轮 H / V ======
//     lv_obj_t *scroll_box = lv_obj_create(ui_DataScreen);
//     lv_obj_set_size(scroll_box, 220, 40);
//     lv_obj_align(scroll_box, LV_ALIGN_TOP_MID, 0, 155);
//     lv_obj_set_style_bg_color(scroll_box, lv_color_hex(0x303030), 0);
//     lv_obj_set_style_radius(scroll_box, 10, 0);
//     lv_obj_set_style_pad_all(scroll_box, 6, 0);

//     add_label(scroll_box, 5, 5, "H:");
//     lbl_h1 = add_label(scroll_box, 35, 5, "0");

//     add_label(scroll_box, 120, 5, "V:");
//     lbl_v1 = add_label(scroll_box, 150, 5, "0");


//     // ====== 按钮组 ======
//     lv_obj_t *btn_box = lv_obj_create(ui_DataScreen);
//     lv_obj_set_size(btn_box, 220, 55);
//     lv_obj_align(btn_box, LV_ALIGN_BOTTOM_MID, 0, -5);
//     lv_obj_set_style_bg_color(btn_box, lv_color_hex(0x303030), 0);
//     lv_obj_set_style_radius(btn_box, 10, 0);
//     lv_obj_set_style_pad_all(btn_box, 5, 0);

//     add_label(btn_box, 5, 0, "B1:");
//     for (int i = 0; i < 10; i++) {
//         lbl_b1[i] = add_label(btn_box, 35 + i * 18, 0, "0");
//     }

//     add_label(btn_box, 5, 25, "B2:");
//     for (int i = 0; i < 10; i++) {
//         lbl_b2[i] = add_label(btn_box, 35 + i * 18, 25, "0");
//     }
// }


void ui_DataScreen_screen_init(void)
{
    ui_DataScreen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_DataScreen, LV_OBJ_FLAG_SCROLLABLE);

    // 背景深灰
    //lv_obj_set_style_bg_color(ui_DataScreen, lv_color_hex(0x181818), 0);
    //lv_obj_set_style_text_color(ui_DataScreen, lv_color_hex(0xffffff), 0);

    //const lv_color_t box_color = lv_color_hex(0x000025);
    //const lv_font_t *font14 = &lv_font_montserrat_14;

    // ====== Title ======
    lbl_title = lv_label_create(ui_DataScreen);
    lv_label_set_text(lbl_title, "RC DATA");
    lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_14, 0); // 小一点更紧凑
    lv_obj_align(lbl_title, LV_ALIGN_TOP_MID, 0, 4);


    // ========= J1 =========
    lv_obj_t *j1 = lv_obj_create(ui_DataScreen);
    lv_obj_set_size(j1, 220, 48);
    lv_obj_align(j1, LV_ALIGN_TOP_MID, 0, 28);
    //lv_obj_set_style_bg_color(j1, box_color, 0);
    lv_obj_set_style_radius(j1, 6, 0);
    lv_obj_set_style_pad_all(j1, 4, 0);

    add_label(j1, 2, 0, "J1");

    lbl_j1_x = add_label(j1, 32, 0, "X:0");
    lbl_j1_y = add_label(j1, 110, 0, "Y:0");
    lbl_j1_l = add_label(j1, 32, 20, "L:0");
    lbl_j1_a = add_label(j1, 110, 20, "A:0");


    // ========= J2 =========
    lv_obj_t *j2 = lv_obj_create(ui_DataScreen);
    lv_obj_set_size(j2, 220, 48);
    lv_obj_align(j2, LV_ALIGN_TOP_MID, 0, 82);
    //lv_obj_set_style_bg_color(j2, box_color, 0);
    lv_obj_set_style_radius(j2, 6, 0);
    lv_obj_set_style_pad_all(j2, 4, 0);

    add_label(j2, 2, 0, "J2");

    lbl_j2_x = add_label(j2, 32, 0, "X:0");
    lbl_j2_y = add_label(j2, 110, 0, "Y:0");
    lbl_j2_l = add_label(j2, 32, 20, "L:0");
    lbl_j2_a = add_label(j2, 110, 20, "A:0");


    // ========= H / V Scrollers =========
    lv_obj_t *scroll = lv_obj_create(ui_DataScreen);
    lv_obj_set_size(scroll, 220, 35);
    lv_obj_align(scroll, LV_ALIGN_TOP_MID, 0, 136);
    //lv_obj_set_style_bg_color(scroll, box_color, 0);
    lv_obj_set_style_radius(scroll, 6, 0);
    lv_obj_set_style_pad_all(scroll, 4, 0);

    add_label(scroll, 5, 5, "H:");
    lbl_h1 = add_label(scroll, 35, 5, "0");

    add_label(scroll, 120, 5, "V:");
    lbl_v1 = add_label(scroll, 150, 5, "0");


    // ========= Buttons =========
    lv_obj_t *btn_box = lv_obj_create(ui_DataScreen);
    lv_obj_set_size(btn_box, 220, 63);
    lv_obj_align(btn_box, LV_ALIGN_BOTTOM_MID, 0, -5);
    //lv_obj_set_style_bg_color(btn_box, box_color, 0);
    lv_obj_set_style_radius(btn_box, 6, 0);
    lv_obj_set_style_pad_all(btn_box, 4, 0);

    add_label(btn_box, 5, 0, "B1:");
    for (int i = 0; i < 10; i++) {
        lbl_b1[i] = add_label(btn_box, 35 + i * 17, 0, "0");
    }

    add_label(btn_box, 5, 28, "B2:");
    for (int i = 0; i < 10; i++) {
        lbl_b2[i] = add_label(btn_box, 35 + i * 17, 28, "0");
    }
}

void ui_update_data_screen(UiDataStruct data)
{
    char buf[32];

    // J1
    sprintf(buf, "x:%.2f", data.joystick1.x);
    lv_label_set_text(lbl_j1_x, buf);

    sprintf(buf, "y:%.2f", data.joystick1.y);
    lv_label_set_text(lbl_j1_y, buf);

    sprintf(buf, "L:%.2f", data.joystick1.long_value);
    lv_label_set_text(lbl_j1_l, buf);

    sprintf(buf, "A:%d", data.joystick1.angle);
    lv_label_set_text(lbl_j1_a, buf);

    // J2
    sprintf(buf, "x:%.2f", data.joystick2.x);
    lv_label_set_text(lbl_j2_x, buf);

    sprintf(buf, "y:%.2f", data.joystick2.y);
    lv_label_set_text(lbl_j2_y, buf);

    sprintf(buf, "L:%.2f", data.joystick2.long_value);
    lv_label_set_text(lbl_j2_l, buf);

    sprintf(buf, "A:%d", data.joystick2.angle);
    lv_label_set_text(lbl_j2_a, buf);

    // Scrollers
    sprintf(buf, "%.2f", data.scroller_horiz1);
    lv_label_set_text(lbl_h1, buf);

    sprintf(buf, "%.2f", data.scroller_vertical1);
    lv_label_set_text(lbl_v1, buf);

    // Buttons
    for (int i = 0; i < 10; i++) {
        sprintf(buf, "%d",data.button_group1[i]);
        lv_label_set_text(lbl_b1[i], buf);

        sprintf(buf, "%d",data.button_group2[i]);
        lv_label_set_text(lbl_b2[i], buf);
    }
}
