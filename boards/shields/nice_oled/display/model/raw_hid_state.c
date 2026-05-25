#include "raw_hid_state.h"

#include <string.h>

static void nice_oled_raw_hid_copy_media_player(struct nice_oled_raw_hid_state *state,
                                                const char *media_player) {
    if (media_player == NULL) {
        state->media_player[0] = '\0';
        return;
    }

    strncpy(state->media_player, media_player, NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN - 1);
    state->media_player[NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN - 1] = '\0';
}

void nice_oled_raw_hid_state_init(struct nice_oled_raw_hid_state *state) {
    memset(state, 0, sizeof(*state));
    state->temperature = 127;
}

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_connection(struct nice_oled_raw_hid_state *state,
                                                          bool is_connected) {
    if (state->is_connected == is_connected) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->is_connected = is_connected;
    return NICE_OLED_DIRTY_RAW_HID;
}

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_time(struct nice_oled_raw_hid_state *state,
                                                    uint8_t hour, uint8_t minute) {
    if (state->hour == hour && state->minute == minute) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->hour = hour;
    state->minute = minute;
    return NICE_OLED_DIRTY_RAW_HID;
}

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_volume(struct nice_oled_raw_hid_state *state,
                                                      uint8_t volume) {
    if (state->volume == volume) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->volume = volume;
    return NICE_OLED_DIRTY_RAW_HID;
}

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_layout(struct nice_oled_raw_hid_state *state,
                                                      uint8_t layout) {
    if (state->layout == layout) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->layout = layout;
    return NICE_OLED_DIRTY_RAW_HID;
}

nice_oled_dirty_mask_t
nice_oled_raw_hid_apply_temperature(struct nice_oled_raw_hid_state *state, int8_t temperature) {
    if (state->temperature == temperature) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->temperature = temperature;
    return NICE_OLED_DIRTY_RAW_HID;
}

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_media_player(struct nice_oled_raw_hid_state *state,
                                                            const char *media_player) {
    char next_media_player[NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN] = {0};

    if (media_player != NULL) {
        strncpy(next_media_player, media_player, NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN - 1);
    }

    if (strncmp(state->media_player, next_media_player, NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN) == 0) {
        return NICE_OLED_DIRTY_NONE;
    }

    nice_oled_raw_hid_copy_media_player(state, next_media_player);
    return NICE_OLED_DIRTY_RAW_HID;
}
