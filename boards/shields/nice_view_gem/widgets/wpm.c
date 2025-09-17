#include <math.h>
#include <zephyr/kernel.h>
#include "wpm.h"
#include "../assets/custom_fonts.h"
LV_IMG_DECLARE(gauge);
LV_IMG_DECLARE(grid);

// === 新增或修改的常量定义 ===
// 原始宽度
#define WPM_CANVAS_WIDTH_ORIGINAL 68
// 减少 4 像素后的新宽度
#define WPM_CANVAS_WIDTH_REDUCED 64
// 宽度缩放因子 (用于浮点计算)
#define WPM_WIDTH_SCALE_FACTOR ((float)WPM_CANVAS_WIDTH_REDUCED / (float)WPM_CANVAS_WIDTH_ORIGINAL)

// 原始 centerX, centerY, radius, offset
// centerX_orig = 33, centerY_orig = 67 + BUFFER_OFFSET_MIDDLE
// radius_orig = 25.45585, offset_orig = 13
// 计算新的值
#define CENTER_X_REDUCED (WPM_CANVAS_WIDTH_REDUCED / 2) // 64 / 2 = 32
#define CENTER_Y_REDUCED (67 + BUFFER_OFFSET_MIDDLE)    // Y 坐标保持不变，因为只减少宽度
#define RADIUS_REDUCED ((int)(25.45585f * WPM_WIDTH_SCALE_FACTOR + 0.5f)) // 四舍五入取整, 约 24
#define OFFSET_REDUCED ((int)(13.0f * WPM_WIDTH_SCALE_FACTOR + 0.5f))     // 四舍五入取整, 约 12
// ============================

static void draw_gauge(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    // Gauge 图像可能也需要调整位置或被裁剪版本，如果它本身超出了新范围
    // 这里暂时保持原样，假设 gauge 图像设计时已考虑居中或适应性
    lv_canvas_draw_img(canvas, 16, 44 + BUFFER_OFFSET_MIDDLE, &gauge, &img_dsc);
}

static void draw_needle(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 1);
    // === 使用新的、按比例缩小的中心点和半径 ===
    int centerX = CENTER_X_REDUCED; // 32
    int centerY = CENTER_Y_REDUCED; // 67 + BUFFER_OFFSET_MIDDLE (保持不变)
    int offset = OFFSET_REDUCED;    // 12
    int value = state->wpm[9];
    // ==========================================

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
    // === 使用新的、按比例缩小的半径 ===
    float radius = (float)RADIUS_REDUCED; // 24.0f
    // ================================
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
    // Grid 图像同样可能需要调整或使用裁剪版本
    // 暂时保持原样
    lv_canvas_draw_img(canvas, 0, 65 + BUFFER_OFFSET_MIDDLE, &grid, &img_dsc);
}

static void draw_graph(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_line_dsc_t line_dsc;
    init_line_dsc(&line_dsc, LVGL_FOREGROUND, 2);
    lv_point_t points[10];
    int baselineY = 97 + BUFFER_OFFSET_MIDDLE;

    // --- 计算 X 坐标的部分 ---
    // 原来的逻辑是 points[i].x = 0 + i * 7.4;
    // 现在需要在 0 到 (WPM_CANVAS_WIDTH_REDUCED - 1) = 63 的范围内均匀分布 10 个点
    // 步长 = (63 - 0) / (10 - 1) = 63 / 9 = 7
    const int x_step_reduced = (WPM_CANVAS_WIDTH_REDUCED - 1) / (10 - 1); // 7

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
        points[i].x = 0 + i * x_step_reduced; // 使用新的步长
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
        points[i].x = 0 + i * x_step_reduced; // 使用新的步长
        points[i].y = baselineY - (state->wpm[i] - min) * 32 / range;
    }
#endif
    // --- 绘制线条 ---
    lv_canvas_draw_line(canvas, points, 10, &line_dsc);
}

static void draw_label(lv_obj_t *canvas, const struct status_state *state) {
    // 标签绘制通常在固定区域，不太受 X 范围减少影响
    // 但为了安全，可以检查其 x 坐标和宽度
    lv_draw_label_dsc_t label_left_dsc;
    init_label_dsc(&label_left_dsc, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_LEFT);
    // 原 x=0, w=25。保持不变或微调。
    lv_canvas_draw_text(canvas, 0, 101 + BUFFER_OFFSET_MIDDLE, 25, &label_left_dsc, "WPM");

    lv_draw_label_dsc_t label_dsc_wpm;
    init_label_dsc(&label_dsc_wpm, LVGL_FOREGROUND, &pixel_operator_mono, LV_TEXT_ALIGN_RIGHT);
    char wpm_text[6] = {};
    snprintf(wpm_text, sizeof(wpm_text), "%d", state->wpm[9]);
    // 原 x=26, w=42。总宽度 68。现在总宽度 64。
    // 新的 x 可能需要调整，例如从 22 开始，宽度 42 保持不变（只要不超出 64）
    // 或者 x=22, w=40。让我们保守一点，x=22, w=38 (22+38=60 < 64)
    // 原代码: lv_canvas_draw_text(canvas, 26, 101 + BUFFER_OFFSET_MIDDLE, 42, &label_dsc_wpm, wpm_text);
    // 尝试调整 x 和 width
    lv_canvas_draw_text(canvas, 22, 101 + BUFFER_OFFSET_MIDDLE, 38, &label_dsc_wpm, wpm_text);
}

void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_gauge(canvas, state);
    draw_needle(canvas, state);
    draw_grid(canvas);
    draw_graph(canvas, state);
    draw_label(canvas, state);
}



