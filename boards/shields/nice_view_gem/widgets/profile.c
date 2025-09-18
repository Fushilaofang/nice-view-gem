#include <zephyr/kernel.h>
#include "profile.h"

LV_IMG_DECLARE(profiles);

static void draw_inactive_profiles(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    // 原代码: lv_canvas_draw_img(canvas, 18, 129 + BUFFER_OFFSET_BOTTOM, &profiles, &img_dsc);
    // 原始 y 坐标: 129 + BUFFER_OFFSET_BOTTOM
    // 修改后 y 坐标: 131 + BUFFER_OFFSET_BOTTOM (下移 2 像素)
    lv_canvas_draw_img(canvas, 18, 131 + BUFFER_OFFSET_BOTTOM, &profiles, &img_dsc);
}

static void draw_active_profile(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);

    int offset = state->active_profile_index * 7;

    // 原代码: lv_canvas_draw_rect(canvas, 18 + offset, 129 + BUFFER_OFFSET_BOTTOM, 3, 3, &rect_white_dsc);
    // 修改后 y 坐标: 131 + BUFFER_OFFSET_BOTTOM (下移 2 像素)
    // x 坐标保持不变，因为它只与 profile index 相关
    lv_canvas_draw_rect(canvas, 18 + offset, 131 + BUFFER_OFFSET_BOTTOM, 3, 3, &rect_white_dsc);
}

void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {
    draw_inactive_profiles(canvas, state);
    draw_active_profile(canvas, state);
}