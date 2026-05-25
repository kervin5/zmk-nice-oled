/* boards/shields/nice_oled/display/render/screen_central.c */
#pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/central_state.h"
#include "../model/dirty_domains.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../widgets/battery.h"
#include "../../widgets/output.h"
#include "../../widgets/layer.h"
#include "../../widgets/wpm.h"
#include "../../widgets/profile.h"

/* Forward declarations — draw helpers that will be moved from screen.c in Task 4 */
static void draw_background(lv_obj_t *canvas, const struct status_state *state);
static void draw_output_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state);
static void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_profile_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_layer_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state);

/* Canvas orchestration */
static void draw_canvas_central(lv_obj_t *canvas, const struct status_state *state) {
    draw_background(canvas, state);
    draw_output_status(canvas, state);
    draw_battery_text_central(canvas, state);
    draw_wpm_status(canvas, state);
    draw_profile_status(canvas, state);
    draw_layer_status(canvas, state);
    draw_hid_status(canvas, state);
    draw_mods_status(canvas, state);
}

int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->canvas = NULL;
    comp->state = NULL;
    comp->dirty = NICE_OLED_DIRTY_NONE;
    comp->initialized = false;

    int canvas_width = CONFIG_NICE_OLED_CUSTOM_CANVAS_WIDTH;
    int canvas_height = CONFIG_NICE_OLED_CUSTOM_CANVAS_HEIGHT;
    lv_color_t *buf = lv_mem_alloc(canvas_width * canvas_height * sizeof(lv_color_t));
    if (!buf) return -1;
    comp->cbuf = lv_img_buf_alloc(canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, buf);
    if (!comp->cbuf) {
        lv_mem_free(buf);
        return -1;
    }
    comp->canvas = lv_canvas_create(parent);
    if (!comp->canvas) {
        lv_img_buf_free(comp->cbuf);
        lv_mem_free(buf);
        return -1;
    }
    lv_canvas_set_buffer(comp->canvas, buf, canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, 0);

    comp->initialized = true;
    return 0;
}

void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas) {
        return;
    }
    draw_canvas_central(comp->canvas, comp->state);
}

/* Stub implementations — to be replaced with real code from screen.c in Task 4 */
static void draw_background(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_output_status(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_profile_status(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state) {}
static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state) {}
