/* boards/shields/nice_oled/widgets/raw_hid_label.c */
/* Generic parameterized widget for incremental RAW HID label updates.
 * Replaces canvas-based draw_hid_status() with persistent LVGL labels.
 * Each field type gets its own label object created once, updated incrementally.
 */

#define _GNU_SOURCE
#include "raw_hid_label.h"
#include <zephyr/kernel.h>
#include <string.h>

/* Persistent label objects — one per field type */
static lv_obj_t *s_weather_label = NULL;
static lv_obj_t *s_time_label = NULL;
static lv_obj_t *s_volume_label = NULL;
static lv_obj_t *s_layout_label = NULL;
static lv_obj_t *s_media_player_label = NULL;

/* Last displayed values for diff guards */
static int8_t s_last_weather_temp = 127;
static uint8_t s_last_time_hour = 0;
static uint8_t s_last_time_minute = 0;
static uint8_t s_last_volume = 0;
static uint8_t s_last_layout_index = 0;
static char s_last_media_player[12] = "";

/* ========================================================================
 * Initialization — creates persistent LVGL label objects
 * ======================================================================== */

lv_obj_t *raw_hid_label_init_weather(lv_obj_t *parent) {
    s_weather_label = lv_label_create(parent);
    lv_label_set_text(s_weather_label, "N/A");
    return s_weather_label;
}

lv_obj_t *raw_hid_label_init_time(lv_obj_t *parent) {
    s_time_label = lv_label_create(parent);
    lv_label_set_text(s_time_label, "--:--");
    return s_time_label;
}

lv_obj_t *raw_hid_label_init_volume(lv_obj_t *parent) {
    s_volume_label = lv_label_create(parent);
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    lv_label_set_text(s_volume_label, "Vol: 0%");
#else
    lv_label_set_text(s_volume_label, "V:0");
#endif
    return s_volume_label;
}

lv_obj_t *raw_hid_label_init_layout(lv_obj_t *parent) {
    s_layout_label = lv_label_create(parent);
    lv_label_set_text(s_layout_label, "L0");
    return s_layout_label;
}

lv_obj_t *raw_hid_label_init_media_player(lv_obj_t *parent) {
    s_media_player_label = lv_label_create(parent);
    lv_label_set_text(s_media_player_label, "---");
    return s_media_player_label;
}

/* ========================================================================
 * Update functions — incremental label updates with diff guards
 * ======================================================================== */

void raw_hid_label_update_weather(int8_t temperature) {
    if (s_weather_label == NULL) {
        return;
    }

    /* Skip update if value hasn't changed */
    if (temperature == s_last_weather_temp) {
        return;
    }
    s_last_weather_temp = temperature;

    char text[16];
    if (temperature == 127) {
        lv_label_set_text(s_weather_label, "N/A");
    } else {
        snprintf(text, sizeof(text), "%dC", temperature);
        lv_label_set_text(s_weather_label, text);
    }
}

void raw_hid_label_update_time(uint8_t hour, uint8_t minute) {
    if (s_time_label == NULL) {
        return;
    }

    /* Skip update if values haven't changed */
    if (hour == s_last_time_hour && minute == s_last_time_minute) {
        return;
    }
    s_last_time_hour = hour;
    s_last_time_minute = minute;

    char text[16];
    snprintf(text, sizeof(text), "%02d:%02d", hour, minute);
    lv_label_set_text(s_time_label, text);
}

void raw_hid_label_update_volume(uint8_t volume) {
    if (s_volume_label == NULL) {
        return;
    }

    /* Skip update if value hasn't changed */
    if (volume == s_last_volume) {
        return;
    }
    s_last_volume = volume;

    char text[16];
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    snprintf(text, sizeof(text), "Vol: %d%%", volume);
#else
    snprintf(text, sizeof(text), "V:%d", volume);
#endif
    lv_label_set_text(s_volume_label, text);
}

void raw_hid_label_update_layout(uint8_t layout_index, const char *layout_list) {
    if (s_layout_label == NULL) {
        return;
    }

    /* Skip update if index hasn't changed */
    if (layout_index == s_last_layout_index) {
        return;
    }
    s_last_layout_index = layout_index;

    char layout_str[16] = "";

#if defined(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST)
    if (layout_list != NULL) {
        /* Parse comma-separated layout list */
        char layouts_config[sizeof(CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST)];
        strcpy(layouts_config, CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT_LIST);

        size_t i = 0;
        char *token = layouts_config;
        while (token != NULL && i < layout_index) {
            char *comma = strchr(token, ',');
            if (comma) {
                *comma = '\0';
                token = comma + 1;
            } else {
                token = NULL;
            }
            i++;
        }
        /* Find the layout_index-th token */
        token = layouts_config;
        i = 0;
        while (i < layout_index) {
            char *comma = strchr(token, ',');
            if (comma) {
                *comma = '\0';
                token = comma + 1;
            } else {
                break;
            }
            i++;
        }
        char *end = strchr(token, ',');
        if (end) {
            *end = '\0';
        }
        snprintf(layout_str, sizeof(layout_str), "%s", token);
    } else {
        snprintf(layout_str, sizeof(layout_str), "L%d", layout_index);
    }
#else
    snprintf(layout_str, sizeof(layout_str), "L%d", layout_index);
#endif

    lv_label_set_text(s_layout_label, layout_str);
}

void raw_hid_label_update_media_player(const char *media_player) {
    if (s_media_player_label == NULL) {
        return;
    }

    /* Skip update if value hasn't changed */
    if (media_player != NULL && strcmp(media_player, s_last_media_player) == 0) {
        return;
    }
    if (media_player == NULL && s_last_media_player[0] == '\0') {
        return;
    }

    if (media_player != NULL) {
        strncpy(s_last_media_player, media_player, sizeof(s_last_media_player) - 1);
        s_last_media_player[sizeof(s_last_media_player) - 1] = '\0';
        lv_label_set_text(s_media_player_label, media_player);
    } else {
        s_last_media_player[0] = '\0';
        lv_label_set_text(s_media_player_label, "---");
    }
}
