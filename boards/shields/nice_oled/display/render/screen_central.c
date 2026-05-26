/* boards/shields/nice_oled/display/render/screen_central.c */

#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/central_state.h"
#include "../model/dirty_domains.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../include/fonts.h"
#include "../../widgets/battery.h"
#include "../../widgets/output.h"
#include "../../widgets/layer.h"
#include "../../widgets/wpm.h"
#include "../../widgets/profile.h"

/* Forward declarations — draw helpers moved from screen.c */
static void draw_battery_text_central(lv_obj_t *canvas, const struct nice_oled_central_state *state);

/* Canvas orchestration */
static void draw_canvas_central(lv_obj_t *canvas, const struct nice_oled_central_state *state) {
    draw_background(canvas);
    draw_output_status(canvas, state);
    draw_battery_text_central(canvas, state);
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    draw_wpm_status(canvas, state);
#endif
    draw_profile_status(canvas, state);
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    draw_layer_status(canvas, state);
#endif
/* RAW HID fields (time, volume, layout, weather, media_player) use persistent LVGL labels
 * via raw_hid_label widget — no canvas drawing needed. */
}

int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
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

void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas || !comp->central_state) {
        return;
    }
    draw_canvas_central(comp->canvas, comp->central_state);
}

/* ========================================================================
 * Draw helpers moved from screen.c (Task 4 of compositor boundaries plan)
 * These were static in screen.c — now static here for renderer ownership.
 * The old widget headers still declare non-static versions for external callers.
 * ======================================================================== */

/* draw_battery_text_central — renamed from draw_battery_text (screen.c:63-120) */
static void draw_battery_text_central(lv_obj_t *canvas, const struct nice_oled_central_state *state) {
    char text[32] = "";
    lv_draw_label_dsc_t label_dsc;

#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_LEFT);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)
    char *p = text;
    char *end = text + sizeof(text);
    for (int i = 0; i < CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES; i++) {
        int written = snprintf(p, end - p, "%d ", state->batteries[i].level);
        if (written > 0) {
            p += written;
        }
    }
    if (p > text) {
        *(p - 1) = '\0';
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY)
    char *p = text;
    char *end = text + sizeof(text);
    for (int i = 1; i < CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES; i++) {
        int written = snprintf(p, end - p, "%d  ", state->batteries[i].level);
        if (written > 0) {
            p += written;
        }
    }
    if (p > text) {
        *(p - 1) = '\0';
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    if (CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES >= 2) {
        snprintf(text, sizeof(text), "%d  %d", state->batteries[0].level,
                 state->batteries[1].level);
    } else {
        snprintf(text, sizeof(text), "%d", state->batteries[0].level);
    }
#endif

    lv_canvas_draw_text(canvas, 0, 19, lv_obj_get_width(canvas), &label_dsc, text);
}


