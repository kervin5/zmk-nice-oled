#include "raw_hid/protocol_decode.h"

#include <string.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#define _TIME 0xAA
#define _VOLUME (_TIME + 1)
#define _LAYOUT (_VOLUME + 1)
#define _SPOTIFY 0xAE
#define _WEATHER 0xAF

bool nice_oled_raw_hid_decode(const uint8_t *data, size_t len, struct nice_oled_raw_hid_message *out) {
    if (!data || !out || len == 0) {
        return false;
    }

    uint8_t type = data[0];

    switch (type) {
    case _TIME:
        if (len < 3) {
            LOG_WRN("TIME packet too short: %u < 3", len);
            return false;
        }
        out->kind = NICE_OLED_RAW_HID_TIME;
        out->time.hour = data[1];
        out->time.minute = data[2];
        break;

    case _VOLUME:
        if (len < 2) {
            LOG_WRN("VOLUME packet too short: %u < 2", len);
            return false;
        }
        out->kind = NICE_OLED_RAW_HID_VOLUME;
        out->volume.value = data[1];
        break;

    case _LAYOUT:
        if (len < 2) {
            LOG_WRN("LAYOUT packet too short: %u < 2", len);
            return false;
        }
        out->kind = NICE_OLED_RAW_HID_LAYOUT;
        out->layout.value = data[1];
        break;

    case _WEATHER:
        if (len < 2) {
            LOG_WRN("WEATHER packet too short: %u < 2", len);
            return false;
        }
        out->kind = NICE_OLED_RAW_HID_WEATHER;
        out->weather.temperature = (int8_t)data[1];
        break;

    case _SPOTIFY:
        if (len < 1 + NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN) {
            LOG_WRN("SPOTIFY packet too short: %u < %u", len, 1 + NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN);
            return false;
        }
        out->kind = NICE_OLED_RAW_HID_SPOTIFY;
        memcpy(out->spotify.media_player, &data[1], NICE_OLED_RAW_HID_MEDIA_PLAYER_LEN);
        break;

    default:
        LOG_WRN("Unknown RAW HID type 0x%02X", type);
        return false;
    }

    return true;
}
