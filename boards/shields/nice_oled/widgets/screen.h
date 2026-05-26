#pragma once
#ifndef SCREEN_H_
#define SCREEN_H_

#include "util.h"
#include "../display/model/central_state.h"
#include "../display/render/screen_common.h"
#include <lvgl.h>
#include <zephyr/kernel.h>

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_HEIGHT * CANVAS_HEIGHT];
    struct nice_oled_central_state central;  /* Typed model — replaces struct status_state */
    struct nice_oled_compositor compositor;
};

// TODO: batt
struct zmk_widget_battery_status {
    sys_snode_t node;
    lv_obj_t *obj;
};

int zmk_widget_battery_status_init(struct zmk_widget_battery_status *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_battery_status_obj(struct zmk_widget_battery_status *widget);
// TODO: batt END

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);
#endif
