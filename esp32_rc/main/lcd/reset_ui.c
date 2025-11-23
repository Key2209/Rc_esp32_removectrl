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












// #include "lvgl.h"
// #include <stdlib.h> // 用于 rand()
// #include <stdint.h> // 用于 intptr_t

// // --- 配置参数 ---
// #define SCREEN_SIZE 240
// #define PARTICLE_COUNT 10
// #define PARTICLE_SPEED 20     // 粒子定时器间隔 (ms)
// #define MAX_PROGRESS_SIZE 180 // 中心圆最大直径

// // --- 全局对象指针 ---
// static lv_obj_t *reset_screen;
// static lv_obj_t *progress_circle;
// static lv_obj_t *header_label;
// static lv_obj_t *status_label;
// static lv_obj_t *particles[PARTICLE_COUNT]; // 粒子对象数组
// static lv_timer_t *particle_timer;

// // ------------------------------------
// // PART 1: 粒子效果逻辑 (Particle System)
// // ------------------------------------

// /**
//  * @brief 粒子定时器回调函数
//  * 用于模拟粒子缓慢向上浮动的效果
//  */
// static void particle_anim_timer_cb(lv_timer_t *timer)
// {
//     if (reset_screen != lv_disp_get_scr_act(NULL))
//     {
//         // 如果当前屏幕不是 reset_screen，则停止粒子动画
//         return;
//     }

//     for (int i = 0; i < PARTICLE_COUNT; i++)
//     {
//         lv_obj_t *p = particles[i];
//         if (!p)
//             continue;

//         // 1. 向上移动 (Y坐标减小)
//         lv_coord_t y = lv_obj_get_y(p) - 1;
//         lv_obj_set_y(p, y);

//         // 2. 达到顶部后重置到底部随机位置，并随机重置大小
//         if (y < -10)
//         {
//             lv_obj_set_y(p, SCREEN_SIZE + rand() % 20);         // 随机重置到屏幕外底部
//             lv_obj_set_x(p, rand() % SCREEN_SIZE);              // 随机重置X坐标
//             lv_obj_set_size(p, 3 + rand() % 3, 3 + rand() % 3); // 随机大小 (3-5px)
//         }

//         // 3. 随机透明度变化，模拟闪烁/浮动
//         // 靠近顶部时透明度降低
//         int opa_val = LV_OPA_30 + (rand() % 40);
//         lv_obj_set_style_opa(p, opa_val, 0);
//     }
// }

// /**
//  * @brief 创建并初始化所有粒子对象
//  * @param parent 粒子所属的父对象 (通常是屏幕)
//  */
// static void create_particles(lv_obj_t *parent)
// {
//     for (int i = 0; i < PARTICLE_COUNT; i++)
//     {
//         lv_obj_t *p = lv_obj_create(parent);
//         particles[i] = p;

//         // 粒子基础样式
//         lv_obj_remove_style_all(p);
//         lv_obj_set_style_bg_color(p, lv_color_white(), 0);
//         lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0); // 确保是圆点
//         lv_obj_set_style_border_width(p, 0, 0);

//         // 随机初始位置和大小
//         lv_obj_set_size(p, 3 + rand() % 3, 3 + rand() % 3);
//         lv_obj_set_x(p, rand() % SCREEN_SIZE);
//         lv_obj_set_y(p, rand() % SCREEN_SIZE);
//         lv_obj_set_style_opa(p, LV_OPA_30 + (rand() % 40), 0);
//     }

//     // 启动粒子动画定时器
//     particle_timer = lv_timer_create(particle_anim_timer_cb, PARTICLE_SPEED, NULL);
// }

// // ------------------------------------
// // PART 2: UI 主体构建
// // ------------------------------------

// /**
//  * @brief 创建 "粒子涌动" 重置屏幕
//  */
// void ui_create_reset_screen_custom(void)
// {
//     // 1. 创建新屏幕对象
//     reset_screen = lv_obj_create(NULL);
//     lv_obj_remove_style_all(reset_screen);
//     lv_obj_set_style_bg_color(reset_screen, lv_color_hex(0x050510), 0); // 深蓝黑背景
//     lv_obj_set_style_bg_opa(reset_screen, LV_OPA_COVER, 0);

//     // 2. 创建粒子效果 (在屏幕上浮动)
//     create_particles(reset_screen);

//     // 3. 顶部标题
//     header_label = lv_label_create(reset_screen);
//     lv_label_set_text(header_label, "CONFIG WIPEOUT");
//     lv_obj_set_style_text_color(header_label, lv_color_make(0x6A, 0xC8, 0xED), 0); // 科技感蓝色
//     lv_obj_set_style_text_font(header_label, &lv_font_montserrat_14, 0);
//     lv_obj_align(header_label, LV_ALIGN_TOP_MID, 0, 15);

//     // 4. 中央进度圆 (初始隐藏/很小)
//     progress_circle = lv_obj_create(reset_screen);
//     lv_obj_remove_style_all(progress_circle);
//     lv_obj_set_size(progress_circle, 0, 0); // 初始为 0
//     lv_obj_center(progress_circle);

//     // 设置为圆角 (模拟圆形)
//     lv_obj_set_style_radius(progress_circle, LV_RADIUS_CIRCLE, 0);
//     // 渐变色背景
//     lv_obj_set_style_bg_color(progress_circle, lv_palette_main(LV_PALETTE_YELLOW), 0);
//     lv_obj_set_style_bg_grad_color(progress_circle, lv_palette_main(LV_PALETTE_RED), 0); // 渐变到红色
//     lv_obj_set_style_bg_grad_dir(progress_circle, LV_GRAD_DIR_HOR, 0);
//     lv_obj_set_style_bg_opa(progress_circle, LV_OPA_60, 0); // 半透明，有发光感

//     // 5. 中央状态标签 (置于进度圆上方)
//     status_label = lv_label_create(progress_circle); // 以 progress_circle 为父对象，便于居中
//     lv_label_set_text(status_label, "PRESS TO INITIATE");
//     lv_obj_center(status_label);
//     lv_obj_set_style_text_align(status_label, LV_TEXT_ALIGN_CENTER, 0);
//     lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
//     lv_obj_set_style_text_font(status_label, &lv_font_montserrat_14, 0);
// }

// // ------------------------------------
// // PART 3: 进度更新 (跨线程安全)
// // ------------------------------------

// /**
//  * @brief 更新进度条状态 (在 LVGL 线程中执行)
//  * @param progress 0 - 100 的整数
//  */
// void ui_update_reset_progress_custom(uint8_t progress)
// {
//     if (!progress_circle)
//         return;

//     if (progress < 0)
//         progress = 0;
//     if (progress > 100)
//         progress = 100;

//     // 1. 核心逻辑：根据进度计算圆形大小
//     // 从 0 增长到最大 MAX_PROGRESS_SIZE
//     uint8_t size = (MAX_PROGRESS_SIZE * progress) / 100;
//     lv_obj_set_size(progress_circle, size, size);
//     lv_obj_center(progress_circle); // 保持居中

//     // 2. 状态和颜色更新
//     if (progress >= 100)
//     {
//         // --- 状态：完成 (爆破效果) ---
//         // 瞬间将圆形放大到超过屏幕
//         lv_obj_set_size(progress_circle, SCREEN_SIZE + 50, SCREEN_SIZE + 50);
//         lv_obj_center(progress_circle);

//         // 颜色变为纯红
//         lv_obj_set_style_bg_color(progress_circle, lv_palette_main(LV_PALETTE_RED), 0);
//         lv_obj_set_style_bg_grad_color(progress_circle, lv_palette_main(LV_PALETTE_RED), 0);
//         lv_obj_set_style_bg_opa(progress_circle, LV_OPA_80, 0); // 提高透明度

//         lv_label_set_text(status_label, "ERASING COMPLETE!");
//         lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
//     }
//     else
//     {
//         // --- 状态：进行中 ---

//         // 透明度根据进度变化 (0% 30% -> 99% 70%)，增强动感
//         lv_obj_set_style_bg_opa(progress_circle, LV_OPA_30 + (LV_OPA_70 * progress / 100), 0);

//         // 改变文字
//         if (progress > 0)
//         {
//             lv_label_set_text_fmt(status_label, "CONFIRMING...\n%d%%", progress);
//             lv_obj_set_style_text_color(status_label, lv_color_white(), 0);
//         }
//         else
//         {
//             lv_label_set_text(status_label, "PRESS TO INITIATE");
//             lv_obj_set_style_text_color(status_label, lv_color_make(0x6A, 0xC8, 0xED), 0);
//         }
//     }
// }

// /**
//  * @brief 切换到新的重置屏幕
//  */
// void ui_load_reset_screen(void)
// {
//     if (reset_screen)
//     {
//         lv_disp_load_scr(reset_screen);
//     }
// }

// /**
//  * @brief LVGL 线程的包装函数，用于 lv_async_call
//  */
// void ui_update_wrapper_async1(void *arg)
// {
//     int progress = (intptr_t)arg;
//     ui_update_reset_progress_custom(progress);
// }