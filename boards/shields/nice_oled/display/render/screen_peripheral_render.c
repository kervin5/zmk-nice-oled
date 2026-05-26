/* boards/shields/nice_oled/display/render/screen_peripheral_render.c */

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/peripheral_state.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../widgets/output.h"

void draw_peripheral_battery_status(lv_obj_t *canvas,
                                    const struct nice_oled_peripheral_state *state);

/* Peripheral compositor responsibilities:
 * - background
 * - peripheral connection canvas chrome
 * - peripheral battery image/text rendering
 * Persistent widgets own sleep art and animation-only LVGL objects.
 */
static void draw_canvas_peripheral(lv_obj_t *canvas,
                                   const struct nice_oled_peripheral_state *state) {
    draw_background(canvas);
    draw_peripheral_connection_status(canvas, state);
    draw_peripheral_battery_status(canvas, state);
}

int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent, void *cbuf) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->raw_cbuf = cbuf;
    comp->canvas = NULL;
    comp->central_state = NULL;
    comp->peripheral_state = NULL;
    comp->dirty = NICE_OLED_DIRTY_NONE;
    comp->initialized = false;

    comp->canvas = lv_canvas_create(parent);
    if (!comp->canvas) {
        return -1;
    }

    /* Wire the canvas to its pixel buffer — without this, all draw calls are no-ops */
    lv_canvas_set_buffer(comp->canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);

    comp->initialized = true;
    return 0;
}

void nice_oled_screen_peripheral_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas || !comp->peripheral_state) {
        return;
    }
    draw_canvas_peripheral(comp->canvas, comp->peripheral_state);

#if !IS_ENABLED(CONFIG_NICE_OLED_NATIVE_PORTRAIT)
    /* Rotate canvas for portrait orientation — matches main branch behavior */
    rotate_canvas(comp->canvas, (lv_color_t *)comp->raw_cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT);
#endif
}
