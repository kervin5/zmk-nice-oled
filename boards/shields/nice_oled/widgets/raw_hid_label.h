#pragma once

#include <lvgl.h>
#include <stdint.h>

/* Field types that raw_hid_label can display */
enum raw_hid_field_type {
    RAW_HID_FIELD_WEATHER,
    RAW_HID_FIELD_TIME,
    RAW_HID_FIELD_VOLUME,
    RAW_HID_FIELD_LAYOUT,
    RAW_HID_FIELD_MEDIA_PLAYER,
};

/**
 * Initialize a persistent LVGL label for a RAW HID field.
 * Creates the label object on `parent` and returns it.
 * Called once per field type during screen initialization.
 */
lv_obj_t *raw_hid_label_init_weather(lv_obj_t *parent);
lv_obj_t *raw_hid_label_init_time(lv_obj_t *parent);
lv_obj_t *raw_hid_label_init_volume(lv_obj_t *parent);
lv_obj_t *raw_hid_label_init_layout(lv_obj_t *parent);
lv_obj_t *raw_hid_label_init_media_player(lv_obj_t *parent);

/**
 * Update a RAW HID label with the current state value.
 * Only updates LVGL if the displayed text actually changed.
 */
void raw_hid_label_update_weather(int8_t temperature);
void raw_hid_label_update_time(uint8_t hour, uint8_t minute);
void raw_hid_label_update_volume(uint8_t volume);
void raw_hid_label_update_layout(uint8_t layout_index, const char *layout_list);
void raw_hid_label_update_media_player(const char *media_player);
