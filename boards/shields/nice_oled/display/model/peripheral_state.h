#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "dirty_domains.h"

struct nice_oled_peripheral_state {
    uint8_t battery;
    bool charging;
    bool connected;
};

void nice_oled_peripheral_state_init(struct nice_oled_peripheral_state *state);
nice_oled_dirty_mask_t
nice_oled_peripheral_apply_battery_state(struct nice_oled_peripheral_state *state, uint8_t battery,
                                         bool charging);
nice_oled_dirty_mask_t
nice_oled_peripheral_apply_connection(struct nice_oled_peripheral_state *state, bool connected);
