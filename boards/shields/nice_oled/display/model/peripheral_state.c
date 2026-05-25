#include "peripheral_state.h"

#include <string.h>

void nice_oled_peripheral_state_init(struct nice_oled_peripheral_state *state) {
    memset(state, 0, sizeof(*state));
}

nice_oled_dirty_mask_t
nice_oled_peripheral_apply_battery_state(struct nice_oled_peripheral_state *state, uint8_t battery,
                                         bool charging) {
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
nice_oled_peripheral_apply_connection(struct nice_oled_peripheral_state *state, bool connected) {
    if (state->connected == connected) {
        return NICE_OLED_DIRTY_NONE;
    }

    state->connected = connected;
    return NICE_OLED_DIRTY_CONNECTION;
}
