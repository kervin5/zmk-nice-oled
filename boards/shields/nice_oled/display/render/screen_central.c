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

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID)
#include <lvgl.h>
#include <raw_hid/hid.h>
#endif

/* Forward declarations — draw helpers moved from screen.c */
static void draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state);
static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state);

/* Canvas orchestration */
static void draw_canvas_central(lv_obj_t *canvas, const struct status_state *state) {
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
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
    draw_hid_status(canvas, state);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
    draw_mods_status(canvas, state);
#endif
}

int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
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

void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas || !comp->state) {
        return;
    }
    draw_canvas_central(comp->canvas, comp->state);
}

/* ========================================================================
 * Draw helpers moved from screen.c (Task 4 of compositor boundaries plan)
 * These were static in screen.c — now static here for renderer ownership.
 * The old widget headers still declare non-static versions for external callers.
 * ======================================================================== */

/* draw_battery_text_central — renamed from draw_battery_text (screen.c:63-120) */
static void draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state) {
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

/* draw_mods_status — moved from screen.c:254-507 (includes associated data) */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)

struct mods_status_state {
    uint8_t mods;
};

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL)
LV_IMG_DECLARE(control_0);
LV_IMG_DECLARE(control_white_0);
LV_IMG_DECLARE(shift_0);
LV_IMG_DECLARE(shift_white_0);
LV_IMG_DECLARE(opt_0);
LV_IMG_DECLARE(opt_white_0);
LV_IMG_DECLARE(alt_0);
LV_IMG_DECLARE(alt_white_0);
LV_IMG_DECLARE(cmd_0);
LV_IMG_DECLARE(cmd_white_0);
LV_IMG_DECLARE(win_0);
LV_IMG_DECLARE(win_white_0);

static const lv_img_dsc_t *mod_imgs_normal[4];
static const lv_img_dsc_t *mod_imgs_active[4];

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL_WINDOWS)
static void init_mod_imgs(void) {
    mod_imgs_normal[0] = &control_0;  mod_imgs_active[0] = &control_white_0;
    mod_imgs_normal[1] = &shift_0;    mod_imgs_active[1] = &shift_white_0;
    mod_imgs_normal[2] = &alt_0;      mod_imgs_active[2] = &alt_white_0;
    mod_imgs_normal[3] = &win_0;      mod_imgs_active[3] = &win_white_0;
}
#else
static void init_mod_imgs(void) {
    mod_imgs_normal[0] = &control_0;  mod_imgs_active[0] = &control_white_0;
    mod_imgs_normal[1] = &shift_0;    mod_imgs_active[1] = &shift_white_0;
    mod_imgs_normal[2] = &opt_0;      mod_imgs_active[2] = &opt_white_0;
    mod_imgs_normal[3] = &cmd_0;      mod_imgs_active[3] = &cmd_white_0;
}
#endif
#endif

static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state) {
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL)
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);

    const int img_size = 14;
    const int spacing = 2;

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER)
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_RIGHT)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = 68 - img_size - 2;
#else
    const int base_x = 128 - img_size - 2;
#endif
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_LEFT)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
#else
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = (68 - img_size) / 2;
#else
    const int base_x = (128 - img_size) / 2;
#endif
#endif
    const int base_y = 62;

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x;
        int current_y = base_y + i * (img_size + spacing);
        const lv_img_dsc_t *img = selected ? mod_imgs_active[i] : mod_imgs_normal[i];
        lv_canvas_draw_img(canvas, current_x, current_y, img, &img_dsc);
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_HOR)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + i * (img_size + spacing);
        int current_y = base_y;
        const lv_img_dsc_t *img = selected ? mod_imgs_active[i] : mod_imgs_normal[i];
        lv_canvas_draw_img(canvas, current_x, current_y, img, &img_dsc);
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_BOX)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    static const int offsets_box[4][2] = {
        {0, 0},                              // C (0,0)
        {img_size + spacing, 0},             // S (0,1)
        {0, img_size + spacing},             // A (1,0)
        {img_size + spacing, img_size + spacing} // G (1,1)
    };

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + offsets_box[i][0];
        int current_y = base_y + offsets_box[i][1];
        const lv_img_dsc_t *img = selected ? mod_imgs_active[i] : mod_imgs_normal[i];
        lv_canvas_draw_img(canvas, current_x, current_y, img, &img_dsc);
    }

#else
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    static const int offsets_default[4][2] = {
        {0, 0},
        {img_size + spacing, 0},
        {0, img_size + spacing},
        {img_size + spacing, img_size + spacing}
    };

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + offsets_default[i][0];
        int current_y = base_y + offsets_default[i][1];
        const lv_img_dsc_t *img = selected ? mod_imgs_active[i] : mod_imgs_normal[i];
        lv_canvas_draw_img(canvas, current_x, current_y, img, &img_dsc);
    }
#endif

#else
    const char *items[4] = {"C", "S", "A", "G"};

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_rect_dsc_t rect_white_dsc;
    init_rect_dsc(&rect_white_dsc, LVGL_FOREGROUND);
    lv_draw_label_dsc_t mod_dsc;
    init_label_dsc(&mod_dsc, LVGL_FOREGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);
    lv_draw_label_dsc_t mod_dsc_black;
    init_label_dsc(&mod_dsc_black, LVGL_BACKGROUND, &lv_font_unscii_8, LV_TEXT_ALIGN_CENTER);

    const int box_width = 12;
    const int box_height = 14;
    const int inner_box_offset = 2;
    const int text_offset_y = 4;
    const int inner_box_width = box_width - (2 * inner_box_offset);
    const int inner_box_height = box_height - (2 * inner_box_offset);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER)
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_RIGHT)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = 68 - box_width - 2;
#else
    const int base_x = 128 - box_width - 2;
#endif
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_VER_ALIGN_LEFT)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
#else
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    const int base_x = (68 - box_width) / 2;
#else
    const int base_x = (128 - box_width) / 2;
#endif
#endif
    const int base_y = 38;

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x;
        int current_y = base_y + i * (box_height + 2);

        lv_canvas_draw_rect(canvas, current_x, current_y, box_width, box_height, &rect_black_dsc);
        if (selected && inner_box_width > 0 && inner_box_height > 0) {
            lv_canvas_draw_rect(canvas, current_x + inner_box_offset,
                                current_y + inner_box_offset, inner_box_width, inner_box_height,
                                &rect_white_dsc);
        }
        lv_canvas_draw_text(canvas, current_x, current_y + text_offset_y, box_width,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_HOR)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + i * (box_width + 2);
        int current_y = base_y;

        lv_canvas_draw_rect(canvas, current_x, current_y, box_width, box_height, &rect_black_dsc);
        if (selected && inner_box_width > 0 && inner_box_height > 0) {
            lv_canvas_draw_rect(canvas, current_x + inner_box_offset,
                                current_y + inner_box_offset, inner_box_width, inner_box_height,
                                &rect_white_dsc);
        }
        lv_canvas_draw_text(canvas, current_x, current_y + text_offset_y, box_width,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }

#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_BOX)
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    static const int offsets_box[4][2] = {
        {0, 0}, {box_width + 2, 0}, {0, box_height + 2}, {box_width + 2, box_height + 2}
    };

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + offsets_box[i][0];
        int current_y = base_y + offsets_box[i][1];

        lv_canvas_draw_rect(canvas, current_x, current_y, box_width, box_height, &rect_black_dsc);
        if (selected && inner_box_width > 0 && inner_box_height > 0) {
            lv_canvas_draw_rect(canvas, current_x + inner_box_offset,
                                current_y + inner_box_offset, inner_box_width, inner_box_height,
                                &rect_white_dsc);
        }
        lv_canvas_draw_text(canvas, current_x, current_y + text_offset_y, box_width,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }

#else
    const int base_x = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_X;
    const int base_y = CONFIG_NICE_OLED_WIDGET_MODIFIERS_CUSTOM_Y;

    static const int offsets_default[4][2] = {
        {0, 0}, {box_width + 2, 0}, {0, box_height + 2}, {box_width + 2, box_height + 2}
    };

    for (int i = 0; i < 4; i++) {
        bool selected = (state->mod_state >> i) & 1 || (state->mod_state >> (i + 4)) & 1;
        int current_x = base_x + offsets_default[i][0];
        int current_y = base_y + offsets_default[i][1];

        lv_canvas_draw_rect(canvas, current_x, current_y, box_width, box_height, &rect_black_dsc);
        if (selected && inner_box_width > 0 && inner_box_height > 0) {
            lv_canvas_draw_rect(canvas, current_x + inner_box_offset,
                                current_y + inner_box_offset, inner_box_width, inner_box_height,
                                &rect_white_dsc);
        }
        lv_canvas_draw_text(canvas, current_x, current_y + text_offset_y, box_width,
                            (selected ? &mod_dsc_black : &mod_dsc), items[i]);
    }
#endif
#endif
}

#else /* CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED */

static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state) {
    (void)canvas;
    (void)state;
}

#endif /* CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED */

/* draw_hid_status — moved from screen.c:566-700 */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID)

static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state) {

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL_VERTICAL) ||              \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_ONE_LINE_VERTICAL)

#define DRAW_HID_STATUS_TEXT_ALIGN LV_TEXT_ALIGN_LEFT

#else
#define DRAW_HID_STATUS_TEXT_ALIGN LV_TEXT_ALIGN_LEFT

#endif

#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
#define DRAW_HID_STATUS_FONTS &lv_font_montserrat_14
#else
#define DRAW_HID_STATUS_FONTS &pixel_operator_mono_12
#endif

    lv_draw_rect_dsc_t rect_black_dsc;
    init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);
    lv_draw_label_dsc_t label_time;
    init_label_dsc(&label_time, LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS, DRAW_HID_STATUS_TEXT_ALIGN);
    lv_draw_label_dsc_t label_layout;
    init_label_dsc(&label_layout, LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS,
                   DRAW_HID_STATUS_TEXT_ALIGN);
    lv_draw_label_dsc_t label_volume;
    init_label_dsc(&label_volume, LVGL_FOREGROUND, DRAW_HID_STATUS_FONTS,
                   DRAW_HID_STATUS_TEXT_ALIGN);

    int hid_area_x = CONFIG_NICE_OLED_WIDGET_RAW_HID_CUSTOM_X;
    int hid_area_y = CONFIG_NICE_OLED_WIDGET_RAW_HID_CUSTOM_Y;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    int hid_area_width = 68;
#else
    int hid_area_width = 32;
#endif

    lv_coord_t current_y = hid_area_y;
    lv_point_t text_size;
    const lv_coord_t line_gap = 0;

    if (state->is_connected) {
        char text_buffer[20];

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
        sprintf(text_buffer, "%dC", state->temperature);
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER_CUSTOM_Y,
                            hid_area_width, &label_volume, text_buffer);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME)
        sprintf(text_buffer, "%02i:%02i", state->hour, state->minute);
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME_CUSTOM_Y,
                            hid_area_width, &label_time, text_buffer);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT)
        char layout_str[10] = {};
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST
        char layouts_config[sizeof(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST)];
        strcpy(layouts_config, CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST);
        char *current_layout_token = strtok(layouts_config, ",");
        size_t i = 0;
        while (current_layout_token != NULL && i < state->layout) {
            i++;
            current_layout_token = strtok(NULL, ",");
        }
        if (current_layout_token != NULL) {
            snprintf(layout_str, sizeof(layout_str), "%s", current_layout_token);
        } else {
            snprintf(layout_str, sizeof(layout_str), "%i", state->layout);
        }
#else
        snprintf(layout_str, sizeof(layout_str), "L%i", state->layout);
#endif
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_CUSTOM_Y,
                            hid_area_width, &label_layout, layout_str);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME)
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
        sprintf(text_buffer, "Vol: %i", state->volume);
#else
        sprintf(text_buffer, "V:%i", state->volume);
#endif
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME_CUSTOM_Y,
                            hid_area_width, &label_volume, text_buffer);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
        lv_canvas_draw_text(canvas, CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_CUSTOM_X,
                            CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_CUSTOM_Y,
                            hid_area_width, &label_volume, state->media_player);
#endif

    } else {
        lv_txt_get_size(&text_size, "HID", label_time.font, label_time.letter_space,
                        label_time.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_area_x, current_y, hid_area_width, &label_time, "HID");
        current_y += text_size.y + line_gap;

        lv_txt_get_size(&text_size, "not", label_layout.font, label_layout.letter_space,
                        label_layout.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_area_x, current_y, hid_area_width, &label_layout, "not");
        current_y += text_size.y + line_gap;

        lv_txt_get_size(&text_size, "found", label_volume.font, label_volume.letter_space,
                        label_volume.line_space, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        lv_canvas_draw_text(canvas, hid_area_x, current_y, hid_area_width, &label_volume, "found");
    }
}

#else /* CONFIG_NICE_OLED_WIDGET_RAW_HID */

static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state) {
    (void)canvas;
    (void)state;
}

#endif /* CONFIG_NICE_OLED_WIDGET_RAW_HID */
