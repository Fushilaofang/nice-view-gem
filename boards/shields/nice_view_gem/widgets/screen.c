#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>
#include "battery.h"
#include "layer.h"
#include "output.h"
#include "profile.h"
#include "screen.h"
#include "wpm.h"

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);

/**
 * Draw buffers
 * 注意：draw_*_status 函数内部的坐标是相对于各自 68x68 的画布的，
 * 旋转由 rotate_canvas 处理，这里的修改不影响它们。
 **/

// 顶部区域绘制函数 (保持不变)
static void draw_top(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 0);
    fill_background(canvas);
    // Draw widgets
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
    // Rotate for horizontal display (now 270 degrees)
    rotate_canvas(canvas, cbuf);
}

// 中部区域绘制函数 (保持不变)
static void draw_middle(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 1);
    fill_background(canvas);
    // Draw widgets
    draw_wpm_status(canvas, state);
    // Rotate for horizontal display (now 270 degrees)
    rotate_canvas(canvas, cbuf);
}

// 底部区域绘制函数 (保持不变)
static void draw_bottom(lv_obj_t *widget, lv_color_t cbuf[], const struct status_state *state) {
    lv_obj_t *canvas = lv_obj_get_child(widget, 2);
    fill_background(canvas);
    // Draw widgets
    draw_profile_status(canvas, state);
    draw_layer_status(canvas, state);
    // Rotate for horizontal display (now 270 degrees)
    rotate_canvas(canvas, cbuf);
}

/**
 * Battery status
 * 逻辑不变
 **/
static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    widget->state.battery = state.level;
    draw_top(widget->obj, widget->cbuf, &widget->state);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

/**
 * Layer status
 * 逻辑不变
 **/
static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)
ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);

/**
 * Output status
 * 逻辑不变
 **/
static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;
    draw_top(widget->obj, widget->cbuf, &widget->state);
    draw_bottom(widget->obj, widget->cbuf3, &widget->state);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/**
 * WPM status
 * 逻辑不变
 **/
static void set_wpm_status(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;
    draw_middle(widget->obj, widget->cbuf2, &widget->state);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);

/**
 * Initialization
 * 修正：调整画布对齐方式以匹配 270 度旋转后的布局
 * 旋转 270 度 = 顺时针旋转 90 度 = 向左转 90 度
 * (x, y) -> (H-1-y, x) 其中 H 是原始屏幕高度 (160)
 * 但 LVGL 的 align 和 offset 是相对于父对象的，我们需要考虑旋转后坐标系的变化
 * 原始对齐 LV_ALIGN_TOP_RIGHT (68, 0) + (x_ofs=0, y_ofs=Y_OFFSET)
 * 旋转 270 度后，(68, 0) 变为 (159, 68)
 * (x_ofs=0, y_ofs=Y_OFFSET) 变为 (x_ofs=-Y_OFFSET, y_ofs=0)
 * 因此，新的对齐应该是 LV_ALIGN_TOP_RIGHT + (x_ofs=-Y_OFFSET, y_ofs=0)
 * 但是，LV_ALIGN_TOP_RIGHT 在旋转后的 160x68 容器中是 (159, 0)
 * 我们需要将画布移动到 (159 - Y_OFFSET, 0)
 * 这可以通过 align_to 或者使用 LV_ALIGN_LEFT_MID 并调整 x_ofs 来实现
 * 更直观的方法是使用 LV_ALIGN_TOP_LEFT 并设置 x_ofs = 159 - Y_OFFSET
 * 159 = SCREEN_HEIGHT - 1
 * 为了简化，我们使用 LV_ALIGN_TOP_RIGHT 并设置 x_ofs = -Y_OFFSET, y_ofs = 0
 * 因为 LV_ALIGN_TOP_RIGHT 在 160x68 中是 (159, 0)，减去 Y_OFFSET 就是 (159 - Y_OFFSET, 0)
 * 这正是我们想要的位置。
 **/
int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    // 1. 创建主控件，尺寸调整为旋转后的屏幕尺寸 (SCREEN_HEIGHT x SCREEN_WIDTH)
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, SCREEN_HEIGHT, SCREEN_WIDTH); // 160 x 68

    // 2. 创建并设置顶部画布 (对应 SIG/BAT 区域)
    lv_obj_t *top = lv_canvas_create(widget->obj);
    // 画布缓冲区仍为 68x68
    lv_canvas_set_buffer(top, widget->cbuf, BUFFER_SIZE, BUFFER_SIZE, LV_IMG_CF_TRUE_COLOR);
    // 旋转270度后，原顶部(0,0) -> (159, 0)
    // LV_ALIGN_TOP_RIGHT in 160x68 is (159, 0). No offset needed for top.
    lv_obj_align(top, LV_ALIGN_TOP_RIGHT, 0, 0); // x_ofs=0, y_ofs=0

    // 3. 创建并设置中部画布 (对应 WPM 图表区域)
    lv_obj_t *middle = lv_canvas_create(widget->obj);
    lv_canvas_set_buffer(middle, widget->cbuf2, BUFFER_SIZE, BUFFER_SIZE, LV_IMG_CF_TRUE_COLOR);
    // 旋转270度后，原中部(0, -44) -> (159 - (-44), 0) = (115, 0)
    // 使用 LV_ALIGN_TOP_RIGHT 并设置 x_ofs = BUFFER_OFFSET_MIDDLE = -44
    lv_obj_align(middle, LV_ALIGN_TOP_RIGHT, BUFFER_OFFSET_MIDDLE, 0); // x_ofs=-44, y_ofs=0

    // 4. 创建并设置底部画布 (对应 Profile/Layer 区域)
    lv_obj_t *bottom = lv_canvas_create(widget->obj);
    lv_canvas_set_buffer(bottom, widget->cbuf3, BUFFER_SIZE, BUFFER_SIZE, LV_IMG_CF_TRUE_COLOR);
    // 旋转270度后，原底部(0, -129) -> (159 - (-129), 0) = (30, 0)
    // 使用 LV_ALIGN_TOP_RIGHT 并设置 x_ofs = BUFFER_OFFSET_BOTTOM = -129
    lv_obj_align(bottom, LV_ALIGN_TOP_RIGHT, BUFFER_OFFSET_BOTTOM, 0); // x_ofs=-129, y_ofs=0

    // 5. 将 widget 添加到监听列表
    sys_slist_append(&widgets, &widget->node);

    // 6. 初始化各个状态监听器
    widget_battery_status_init();
    widget_layer_status_init();
    widget_output_status_init();
    widget_wpm_status_init();

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }



