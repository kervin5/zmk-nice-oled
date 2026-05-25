#pragma once

#include <string.h>

#include <lvgl.h>

#include "../display/model/central_state.h"
#include "../display/model/peripheral_state.h"

// nice_epaper and nice_oled standard width = 68, height = 160
#define CANVAS_WIDTH CONFIG_NICE_OLED_CUSTOM_CANVAS_WIDTH
#define CANVAS_HEIGHT CONFIG_NICE_OLED_CUSTOM_CANVAS_HEIGHT

#define LVGL_BACKGROUND                                                                            \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_INVERTED) ? lv_color_black() : lv_color_white()
#define LVGL_FOREGROUND                                                                            \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_INVERTED) ? lv_color_white() : lv_color_black()

struct status_state {
    nice_oled_dirty_mask_t dirty;
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct nice_oled_central_state central;
#else
    struct nice_oled_peripheral_state peripheral;
#endif
    uint8_t battery;
    bool charging;
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    struct battery_info batteries[NICE_OLED_SPLIT_TOTAL_DEVICES];
#endif

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    uint8_t layer_index;
    const char *layer_label;
    uint8_t wpm[10];
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
    bool is_connected;
    uint8_t hour;
    uint8_t minute;
    uint8_t volume;
    uint8_t layout;
    int8_t temperature;
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    char media_player[11];
#endif
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
    uint8_t mod_state;
#endif

#else
    bool connected;
#endif
};

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
static inline void nice_oled_status_state_sync_from_central(struct status_state *state) {
    state->battery = state->central.battery;
    state->charging = state->central.charging;
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    memcpy(state->batteries, state->central.batteries, sizeof(state->batteries));
#endif
    state->selected_endpoint = state->central.selected_endpoint;
    state->active_profile_index = state->central.active_profile_index;
    state->active_profile_connected = state->central.active_profile_connected;
    state->active_profile_bonded = state->central.active_profile_bonded;
    state->layer_index = state->central.layer_index;
    state->layer_label = state->central.layer_label;
    memcpy(state->wpm, state->central.wpm, sizeof(state->wpm));
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
    state->is_connected = state->central.raw_hid.is_connected;
    state->hour = state->central.raw_hid.hour;
    state->minute = state->central.raw_hid.minute;
    state->volume = state->central.raw_hid.volume;
    state->layout = state->central.raw_hid.layout;
    state->temperature = state->central.raw_hid.temperature;
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    memcpy(state->media_player, state->central.raw_hid.media_player, sizeof(state->media_player));
#endif
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
    state->mod_state = state->central.mod_state;
#endif
}
#else
static inline void nice_oled_status_state_sync_from_peripheral(struct status_state *state) {
    state->battery = state->peripheral.battery;
    state->charging = state->peripheral.charging;
    state->connected = state->peripheral.connected;
}
#endif

static inline void nice_oled_status_state_init(struct status_state *state) {
    memset(state, 0, sizeof(*state));

#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    nice_oled_central_state_init(&state->central);
    nice_oled_status_state_sync_from_central(state);
#else
    nice_oled_peripheral_state_init(&state->peripheral);
    nice_oled_status_state_sync_from_peripheral(state);
#endif
}

void to_uppercase(char *str);
void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]);
void draw_background(lv_obj_t *canvas);
void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color);
void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width);
void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align);
