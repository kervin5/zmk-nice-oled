#pragma once

#include <stdint.h>

#include "../display/model/dirty_domains.h"
#include "../display/model/raw_hid_state.h"
#include "protocol_types.h"

nice_oled_dirty_mask_t nice_oled_raw_hid_apply_message(struct nice_oled_raw_hid_state *state,
                                                       const struct nice_oled_raw_hid_message *message);
