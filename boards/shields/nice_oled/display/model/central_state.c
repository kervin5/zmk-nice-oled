#include "central_state.h"

#include <string.h>

void nice_oled_central_state_init(struct nice_oled_central_state *state) {
    memset(state, 0, sizeof(*state));

    for (uint8_t i = 0; i < NICE_OLED_SPLIT_TOTAL_DEVICES; i++) {
        state->batteries[i].source = i;
    }

    state->layer_label = NULL;
    nice_oled_raw_hid_state_init(&state->raw_hid);
}

nice_oled_dirty_mask_t nice_oled_central_apply_battery_state(struct nice_oled_central_state *state,
                                                             uint8_t battery, bool charging) {
    nice_oled_dirty_mask_t dirty = NICE_OLED_DIRTY_NONE;

    if (state->battery != battery) {
        state->battery = battery;
        dirty |= NICE_OLED_DIRTY_BATTERY;
    }

    if (state->charging != charging) {
        state->charging = charging;
        dirty |= NICE_OLED_DIRTY_BATTERY;
    }

    return dirty;
}

nice_oled_dirty_mask_t
nice_oled_central_apply_split_battery_state(struct nice_oled_central_state *state, uint8_t source,
                                            uint8_t level, bool usb_present) {
    if (source >= NICE_OLED_SPLIT_TOTAL_DEVICES) {
        return NICE_OLED_DIRTY_NONE;
    }

    nice_oled_dirty_mask_t dirty = NICE_OLED_DIRTY_NONE;

    if (state->batteries[source].source != source) {
        state->batteries[source].source = source;
        dirty |= NICE_OLED_DIRTY_BATTERY;
    }

    if (state->batteries[source].level != level) {
        state->batteries[source].level = level;
        dirty |= NICE_OLED_DIRTY_BATTERY;
    }

    if (state->batteries[source].usb_present != usb_present) {
        state->batteries[source].usb_present = usb_present;
        dirty |= NICE_OLED_DIRTY_BATTERY;
    }

    if (source == 0) {
        dirty |= nice_oled_central_apply_battery_state(state, level, usb_present);
    }

    return dirty;
}

nice_oled_dirty_mask_t nice_oled_central_apply_layer(struct nice_oled_central_state *state,
                                                     uint8_t index, const char *label) {
    if (state->layer_index == index && state->layer_label == label) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->layer_index = index;
    state->layer_label = label;
    return NICE_OLED_DIRTY_LAYER;
}

nice_oled_dirty_mask_t nice_oled_central_apply_output(struct nice_oled_central_state *state,
                                                      const struct zmk_endpoint_instance *endpoint,
                                                      int active_profile_index,
                                                      bool active_profile_connected,
                                                      bool active_profile_bonded) {
    nice_oled_dirty_mask_t dirty = NICE_OLED_DIRTY_NONE;

    if (memcmp(&state->selected_endpoint, endpoint, sizeof(*endpoint)) != 0) {
        state->selected_endpoint = *endpoint;
        dirty |= NICE_OLED_DIRTY_OUTPUT;
    }

    if (state->active_profile_index != active_profile_index) {
        state->active_profile_index = active_profile_index;
        dirty |= NICE_OLED_DIRTY_OUTPUT;
    }

    if (state->active_profile_connected != active_profile_connected) {
        state->active_profile_connected = active_profile_connected;
        dirty |= NICE_OLED_DIRTY_OUTPUT;
    }

    if (state->active_profile_bonded != active_profile_bonded) {
        state->active_profile_bonded = active_profile_bonded;
        dirty |= NICE_OLED_DIRTY_OUTPUT;
    }

    return dirty;
}

nice_oled_dirty_mask_t nice_oled_central_apply_wpm(struct nice_oled_central_state *state,
                                                   uint8_t wpm) {
    memmove(&state->wpm[0], &state->wpm[1], sizeof(state->wpm) - sizeof(state->wpm[0]));
    state->wpm[NICE_OLED_WPM_HISTORY_LEN - 1] = wpm;
    return NICE_OLED_DIRTY_WPM;
}
