/* boards/shields/nice_oled/display/render/screen_peripheral_render.c */

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/peripheral_state.h"
#include "screen_common.h"
#include "../../widgets/util.h"

/* Canvas orchestration — peripheral only draws background (no output/battery/profile on peripheral) */
static void draw_canvas_peripheral(lv_obj_t *canvas) {
    draw_background(canvas);
}

int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->canvas = NULL;
    comp->central_state = NULL;
    comp->peripheral_state = NULL;
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
    if (!comp || !comp->initialized || !comp->canvas) {
        return;
    }
    draw_canvas_peripheral(comp->canvas);
}
