/* boards/shields/nice_oled/display/render/screen_peripheral_render.c */
#pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/peripheral_state.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../widgets/battery.h"
#include "../../widgets/output.h"

/* Canvas orchestration */
static void draw_canvas_peripheral(lv_obj_t *canvas, const struct status_state *state) {
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_status(canvas, state);
}

int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->canvas = NULL;
    comp->state = NULL;
    comp->dirty = NICE_OLED_DIRTY_NONE;
    comp->initialized = false;

    comp->canvas = lv_canvas_create(parent);
    if (!comp->canvas) {
        return -1;
    }

    comp->initialized = true;
    return 0;
}

void nice_oled_screen_peripheral_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas || !comp->state) {
        return;
    }
    draw_canvas_peripheral(comp->canvas, comp->state);
}
