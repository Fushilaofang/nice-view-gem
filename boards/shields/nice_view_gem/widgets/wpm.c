#include <math.h>
#include <zephyr/kernel.h>
#include "wpm.h"
#include "../assets/custom_fonts.h"
LV_IMG_DECLARE(gauge);
LV_IMG_DECLARE(grid);

// 定义缩小后的宽度和居中偏移量
#define WPM_GRAPH_GRID_WIDTH_REDUCED 66
#define WPM_CENTERING_OFFSET_X_SMALLER 1 // (68 - 66) / 2

static void draw_gauge(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // Gauge 位置保持不变
    lv_canvas_draw_img(canvas, 16, 44 + BUFFER_OFFSET_MIDDLE, &gauge, &img_dsc);
}

static void draw_needle(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    // Needle 参数保持不变
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
    // 将 grid 图像向右移动居中偏移量
    // 原代码: lv_canvas_draw_img(canvas, 0, 65 + BUFFER_OFFSET_MIDDLE, &grid, &img_dsc);
    lv_canvas_draw_img(canvas, WPM_CENTERING_OFFSET_X_SMALLER, 65 + BUFFER_OFFSET_MIDDLE, &grid, &img_dsc);
    // 注意：如果 grid 图像本身是 68 像素宽，
    // LVGL 在将其绘制到 68x68 的画布缓冲区时，
    // 其最左边 1 个像素 (x=0) 和最右边 1 个像素 (x=67) 可能不会显示，
    // 因为它被绘制在 x=1，且画布可视区域逻辑上变为 66 像素宽 (x=1 到 x=66)。
}

static void draw_graph(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);
    lv_point_t points[10];
    // Y 坐标计算保持不变
    int baselineY = 97 + BUFFER_OFFSET_MIDDLE;

    // 计算新的 X 步长，以适应缩小后的宽度 (66 像素)
    const float new_x_step = (float)(WPM_GRAPH_GRID_WIDTH_REDUCED - 1) / (10.0f - 1.0f); // (66 - 1) / 9 = 65 / 9 ≈ 7.222...

    // --- Y 坐标计算逻辑保持不变 ---
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
        // X 坐标：从居中偏移量开始，按新步长计算，并四舍五入
        float precise_x = (float)WPM_CENTERING_OFFSET_X_SMALLER + (float)i * new_x_step;
        points[i].x = (lv_coord_t)(precise_x + 0.5f); // 四舍五入
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
        // X 坐标：从居中偏移量开始，按新步长计算，并四舍五入
        float precise_x = (float)WPM_CENTERING_OFFSET_X_SMALLER + (float)i * new_x_step;
        points[i].x = (lv_coord_t)(precise_x + 0.5f); // 四舍五入
        points[i].y = baselineY - (state->wpm[i] - min) * 32 / range;
    }
#endif
    // --- 绘制线条 ---
    lv_canvas_draw_line(canvas, points, 10, &line_dsc);
}

static void draw_label(lv_obj_t *canvas, const struct status_state *state) {
    // Label 位置保持不变
    lv_draw_label_dsc_t label_left_dsc;
    init_label_dsc(&label_left_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
    lv_canvas_draw_text(canvas, 0, 101 + BUFFER_OFFSET_MIDDLE, 25, &label_left_dsc, "WPM");

    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_RIGHT);
    char wpm_text[6] = {};
    snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    // WPM 数值 Label 位置保持不变
    lv_canvas_draw_text(canvas, 26, 101 + BUFFER_OFFSET_MIDDLE, 42, &label_dsc_wpm, wpm_text);
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_gauge(canvas, state);
    draw_needle(canvas, state);
    draw_grid(canvas);
    draw_graph(canvas, state);
    draw_label(canvas, state);
}



