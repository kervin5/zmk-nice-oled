#pragma once
#ifndef SCREEN_PERIPHERAL_H_
#define SCREEN_PERIPHERAL_H_

#include "util.h"
#include <lvgl.h>
#include <zephyr/kernel.h>
#include "../display/model/peripheral_state.h"
#include "../display/render/screen_common.h"

struct zmk_widget_screen {
    sys_snode_t node;
    lv_obj_t *obj;
    lv_color_t cbuf[CANVAS_HEIGHT * CANVAS_HEIGHT];
    struct nice_oled_peripheral_state peripheral;  /* Typed model — replaces struct status_state */
    struct nice_oled_compositor compositor;
};

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent);
lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget);

#endif /* SCREEN_PERIPHERAL_H_ */
