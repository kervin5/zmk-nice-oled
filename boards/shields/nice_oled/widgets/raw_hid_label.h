#pragma once

#include <lvgl.h>
#include <stdint.h>

/**
 * Style configuration for RAW HID labels.
 * Controls placement and appearance of persistent label widgets.
 */
struct raw_hid_label_style {
    lv_coord_t x;
    lv_coord_t y;
    const lv_font_t *font;
    lv_color_t color;
};

/**
 * Initialize a persistent LVGL label for a RAW HID field.
 * Creates the label object on `parent` with explicit placement and style.
 * Called once per field type during screen initialization.
 */
lv_obj_t *raw_hid_label_init_weather(lv_obj_t *parent, const struct raw_hid_label_style *style);
lv_obj_t *raw_hid_label_init_time(lv_obj_t *parent, const struct raw_hid_label_style *style);
lv_obj_t *raw_hid_label_init_volume(lv_obj_t *parent, const struct raw_hid_label_style *style);
lv_obj_t *raw_hid_label_init_layout(lv_obj_t *parent, const struct raw_hid_label_style *style);
lv_obj_t *raw_hid_label_init_media_player(lv_obj_t *parent, const struct raw_hid_label_style *style);

/**
 * Update a RAW HID label with the current state value.
 * Only updates LVGL if the displayed text actually changed.
 */
void raw_hid_label_update_weather(int8_t temperature);
void raw_hid_label_update_time(uint8_t hour, uint8_t minute);
void raw_hid_label_update_volume(uint8_t volume);
void raw_hid_label_update_layout(uint8_t layout_index, const char *layout_list);
void raw_hid_label_update_media_player(const char *media_player);
