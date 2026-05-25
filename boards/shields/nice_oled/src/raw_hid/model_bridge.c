#include "raw_hid/model_bridge.h"

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_message(struct nice_oled_raw_hid_state *state,
                                                       const struct nice_oled_raw_hid_message *message) {
    if (!state || !message) {
        return NICE_OLED_DIRTY_NONE;
    }

    switch (message->kind) {
    case NICE_OLED_RAW_HID_TIME:
        return nice_oled_raw_hid_apply_time(state, message->time.hour, message->time.minute);

    case NICE_OLED_RAW_HID_VOLUME:
        return nice_oled_raw_hid_apply_volume(state, message->volume.value);

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
    case NICE_OLED_RAW_HID_LAYOUT:
        return nice_oled_raw_hid_apply_layout(state, message->layout.value);
#endif

    case NICE_OLED_RAW_HID_WEATHER:
        return nice_oled_raw_hid_apply_temperature(state, message->weather.temperature);

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    case NICE_OLED_RAW_HID_SPOTIFY:
        return nice_oled_raw_hid_apply_media_player(state, message->spotify.media_player);
#endif

    default:
        return NICE_OLED_DIRTY_NONE;
    }
}
