#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dirty_domains.h"

#define NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN 11

struct nice_oled_raw_hid_state {
    bool is_connected;
    uint8_t hour;
    uint8_t minute;
    uint8_t volume;
    uint8_t layout;
    int8_t temperature;
    char media_player[NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN];
};

void nice_oled_raw_hid_state_init(struct nice_oled_raw_hid_state *state);
nice_oled_dirty_mask_t nice_oled_raw_hid_apply_connection(struct nice_oled_raw_hid_state *state,
                                                          bool is_connected);
nice_oled_dirty_mask_t nice_oled_raw_hid_apply_time(struct nice_oled_raw_hid_state *state,
                                                    uint8_t hour, uint8_t minute);
nice_oled_dirty_mask_t nice_oled_raw_hid_apply_volume(struct nice_oled_raw_hid_state *state,
                                                      uint8_t volume);
nice_oled_dirty_mask_t nice_oled_raw_hid_apply_layout(struct nice_oled_raw_hid_state *state,
                                                      uint8_t layout);
nice_oled_dirty_mask_t
nice_oled_raw_hid_apply_temperature(struct nice_oled_raw_hid_state *state, int8_t temperature);
nice_oled_dirty_mask_t nice_oled_raw_hid_apply_media_player(struct nice_oled_raw_hid_state *state,
                                                            const char *media_player);
