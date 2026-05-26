#pragma once

#include <stdint.h>

#include "../../display/model/raw_hid_state.h"

enum nice_oled_raw_hid_kind {
    NICE_OLED_RAW_HID_TIME,     // 0xAA, needs 3 bytes total (type + hour + minute)
    NICE_OLED_RAW_HID_VOLUME,   // 0xAB, needs 2 bytes (type + volume)
    NICE_OLED_RAW_HID_LAYOUT,   // 0xAC, needs 2 bytes (type + layout)
    NICE_OLED_RAW_HID_WEATHER,  // 0xAF, needs 2 bytes (type + temperature)
    NICE_OLED_RAW_HID_SPOTIFY,  // 0xAE, needs 12 bytes (type + 11 char media_player)
};

struct nice_oled_raw_hid_message {
    enum nice_oled_raw_hid_kind kind;
    union {
        struct { uint8_t hour; uint8_t minute; } time;
        struct { uint8_t value; } volume;
        struct { uint8_t value; } layout;
        struct { int8_t temperature; } weather;
        struct { char media_player[NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN]; } spotify;
    };
};
