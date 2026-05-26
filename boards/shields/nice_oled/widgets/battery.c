#include "battery.h"
#include "util.h"
#include "../display/render/central_draw_compat.h"
#include <fonts.h>
#include <zephyr/kernel.h>

LV_IMG_DECLARE(bolt);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_SMART_BATTERY)
LV_IMG_DECLARE(crystal_01);
LV_IMG_DECLARE(crystal_02);
LV_IMG_DECLARE(crystal_03);
LV_IMG_DECLARE(crystal_04);
LV_IMG_DECLARE(crystal_05);
LV_IMG_DECLARE(crystal_06);
LV_IMG_DECLARE(crystal_07);
LV_IMG_DECLARE(crystal_08);
LV_IMG_DECLARE(crystal_09);
LV_IMG_DECLARE(crystal_10);
LV_IMG_DECLARE(crystal_11);
LV_IMG_DECLARE(crystal_12);
LV_IMG_DECLARE(crystal_13);
LV_IMG_DECLARE(crystal_14);
LV_IMG_DECLARE(crystal_15);
LV_IMG_DECLARE(crystal_16);

#ifndef SET_ANIMATION_SMART_BATTERY_OFF
#define SET_ANIMATION_SMART_BATTERY_OFF &crystal_01
#endif

const lv_img_dsc_t *crystal_imgs_test[] = {
    &crystal_01, &crystal_02, &crystal_03, &crystal_04, &crystal_05, &crystal_06,
    &crystal_07, &crystal_08, &crystal_09, &crystal_10, &crystal_11, &crystal_12,
    &crystal_13, &crystal_14, &crystal_15, &crystal_16,
};

static void animation_obj_deleted_cb(lv_event_t *event) {
    lv_obj_t **slot = lv_event_get_user_data(event);

    if (slot != NULL) {
        *slot = NULL;
    }
}

static void delete_if_present(lv_obj_t **obj) {
    if (*obj == NULL) {
        return;
    }

    lv_obj_del(*obj);
    *obj = NULL;
}

void animation_smart_battery_on(lv_obj_t *canvas, lv_obj_t **anim_obj, lv_obj_t **static_obj) {
    delete_if_present(static_obj);
    delete_if_present(anim_obj);

    *anim_obj = lv_animimg_create(canvas);
    lv_obj_add_event_cb(*anim_obj, animation_obj_deleted_cb, LV_EVENT_DELETE, anim_obj);
    lv_obj_center(*anim_obj);

    lv_animimg_set_src(*anim_obj, (const void **)crystal_imgs_test, 16);
    lv_animimg_set_duration(*anim_obj, CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_MS);
    lv_animimg_set_repeat_count(*anim_obj, LV_ANIM_REPEAT_INFINITE);
    lv_animimg_start(*anim_obj);
    lv_obj_align(*anim_obj, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CUSTOM_X,
                 CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CUSTOM_Y);
}

void animation_smart_battery_off(lv_obj_t *canvas, lv_obj_t **anim_obj, lv_obj_t **static_obj) {
    delete_if_present(anim_obj);
    delete_if_present(static_obj);

    *static_obj = lv_img_create(canvas);
    lv_obj_add_event_cb(*static_obj, animation_obj_deleted_cb, LV_EVENT_DELETE, static_obj);
    lv_img_set_src(*static_obj, SET_ANIMATION_SMART_BATTERY_OFF);
    lv_obj_align(*static_obj, LV_ALIGN_TOP_LEFT,
                 CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CUSTOM_X,
                 CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CUSTOM_Y);
}
#endif

static void draw_level(lv_obj_t *canvas, uint8_t battery) {
    lv_draw_label_dsc_t label_right_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_right_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_RIGHT);
#else
    init_label_dsc(&label_right_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};

    sprintf(text, "%i%%", battery);
    // x, y, width, dsc, text
    nice_oled_central_draw_text_compat(canvas, CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_X,
                                       CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_Y, 42,
                                       &label_right_dsc, text);
}

static void draw_charging_level(lv_obj_t *canvas, uint8_t battery) {
    lv_draw_img_dsc_t img_dsc;
    lv_draw_img_dsc_init(&img_dsc);
    lv_draw_label_dsc_t label_right_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_right_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_RIGHT);
#else
    init_label_dsc(&label_right_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};

    sprintf(text, "%i", battery);
    nice_oled_central_draw_text_compat(canvas, CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_X,
                                       CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_Y, 35,
                                       &label_right_dsc, text);
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    nice_oled_central_draw_img_compat(canvas, CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_X + 36,
                                      CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_Y + 2, &bolt,
                                      &img_dsc);
#else
    nice_oled_central_draw_img_compat(canvas, CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_X + 25,
                                      CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_Y, &bolt,
                                      &img_dsc);
#endif // CONFIG_NICE_EPAPER_ON
}

static void draw_battery_label(lv_obj_t *canvas) {
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    lv_draw_label_dsc_t label_left_dsc;
    init_label_dsc(&label_left_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
    nice_oled_central_draw_text_compat(canvas, 0, 19, 25, &label_left_dsc, "BAT");
#endif // CONFIG_NICE_EPAPER_ON
}

static void draw_battery_value(lv_obj_t *canvas, uint8_t battery, bool charging) {
    if (charging) {
        draw_charging_level(canvas, battery);
    } else {
        draw_level(canvas, battery);
    }
}

void draw_battery_status(lv_obj_t *canvas, const struct nice_oled_central_state *state) {
    draw_battery_label(canvas);
    draw_battery_value(canvas, state->battery, state->charging);
}

void draw_peripheral_battery_status(lv_obj_t *canvas,
                                    const struct nice_oled_peripheral_state *state) {
    draw_battery_label(canvas);
    draw_battery_value(canvas, state->battery, state->charging);
}
