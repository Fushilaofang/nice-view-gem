#include <math.h>
#include <zephyr/kernel.h>
#include "wpm.h"
#include "../assets/custom_fonts.h"
LV_IMG_DECLARE(gauge);
LV_IMG_DECLARE(grid);

// 居中偏移量常量
#define WPM_CENTERING_OFFSET_X 1

static void draw_gauge(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // Gauge 位置
    lv_canvas_draw_img(canvas, 16, 44 + BUFFER_OFFSET_MIDDLE, &gauge, &img_dsc);
}

static void draw_needle(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    // Needle 参数
    int centerX = 33;
    int centerY = 67 + BUFFER_OFFSET_MIDDLE;
    int offset = 13;
    int value = state->wpm[9];
#if IS_ENABLED(CONFIG_NICE_VIEW_GEM_WPM_FIXED_RANGE)
    float max = CONFIG_NICE_VIEW_GEM_WPM_FIXED_RANGE_MAX;
#else
    float max = 0;
    for (int i = 0; i < 10; i++) {
        if (state->wpm[i] > max) {
            max = state->wpm[i];
        }
    }
#endif
    if (max == 0)
        max = 100;
    if (value < 0)
        value = 0;
    if (value > max)
        value = max;
    float radius = 25.45585;
    float angleDeg = 225 + ((float)value / max) * 90;
    float angleRad = angleDeg * (3.14159f / 180.0f);
    int needleStartX = centerX + (int)((float)offset * cosf(angleRad));
    int needleStartY = centerY + (int)((float)offset * sinf(angleRad));
    int needleEndX = centerX + (int)(radius * cosf(angleRad));
    int needleEndY = centerY + (int)(radius * sinf(angleRad));
    lv_point_t points[2] = {{needleStartX, needleStartY}, {needleEndX, needleEndY}};
    lv_canvas_draw_line(canvas, points, 2, &line_dsc);
}

static void draw_grid(lv_obj_t *canvas) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // 应用居中偏移量到 grid 的 x 坐标
    // 原代码: lv_canvas_draw_img(canvas, 0, 65 + BUFFER_OFFSET_MIDDLE, &grid, &img_dsc);
    lv_canvas_draw_img(canvas, WPM_CENTERING_OFFSET_X, 65 + BUFFER_OFFSET_MIDDLE, &grid, &img_dsc);
    // 注意：如果 grid 图像本身是 68 像素宽，
    // LVGL 可能会自动裁剪掉超出画布 (68x68) 右边界的 4 个像素 (如果画布最终显示区域被限制)。
    // 如果没有自动裁剪，且 grid 图像宽度确实是 68，则保留原样。
}

static void draw_graph(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);
    lv_point_t points[10];
    // Y 坐标计算
    int baselineY = 97 + BUFFER_OFFSET_MIDDLE;

    // --- X 坐标计算逻辑 ---
#if IS_ENABLED(CONFIG_NICE_VIEW_GEM_WPM_FIXED_RANGE)
    int max = CONFIG_NICE_VIEW_GEM_WPM_FIXED_RANGE_MAX;
    if (max == 0) {
        max = 100;
    }
    int value = 0;
    for (int i = 0; i < 10; i++) {
        value = state->wpm[i];
        if (value > max) {
            value = max;
        }
        // 应用居中偏移量
        // 原代码: points[i].x = 0 + i * 7.4;
        points[i].x = WPM_CENTERING_OFFSET_X + (int)(i * 7.4);
        points[i].y = baselineY - (value * 32 / max);
    }
#else
    int max = 0;
    int min = 256;
    for (int i = 0; i < 10; i++) {
        if (state->wpm[i] > max) {
            max = state->wpm[i];
        }
        if (state->wpm[i] < min) {
            min = state->wpm[i];
        }
    }
    int range = max - min;
    if (range == 0) {
        range = 1;
    }
    for (int i = 0; i < 10; i++) {
        // 应用居中偏移量
        // 原代码: points[i].x = 0 + i * 7.4;
        points[i].x = WPM_CENTERING_OFFSET_X + (int)(i * 7.4);
        points[i].y = baselineY - (state->wpm[i] - min) * 32 / range;
    }
#endif
    // --- 绘制线条 ---
    lv_canvas_draw_line(canvas, points, 10, &line_dsc);
}

static void draw_label(lv_obj_t *canvas, const struct status_state *state) {
    // 绘制 "WPM" 文本 - 向右移动 4 像素
    lv_draw_label_dsc_t label_left_dsc;
    init_label_dsc(&label_left_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
    // 原始 x=0, 修改为 x=4
    lv_canvas_draw_text(canvas, 3, 101 + BUFFER_OFFSET_MIDDLE, 25, &label_left_dsc, "WPM");

    // 绘制 WPM 计数值文本 - 向右移动 2 像素 (文本框整体右移)
    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_RIGHT);
    char wpm_text[6] = {};
    snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    // 原始 x=26, 修改为 x=28
    // width 和对齐方式保持不变
    lv_canvas_draw_text(canvas, 24, 101 + BUFFER_OFFSET_MIDDLE, 42, &label_dsc_wpm, wpm_text);
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_gauge(canvas, state);
    draw_needle(canvas, state);
    draw_grid(canvas);
    draw_graph(canvas, state);
    draw_label(canvas, state);
}



