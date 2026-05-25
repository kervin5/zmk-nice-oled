#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#include "protocol_types.h"

bool nice_oled_raw_hid_decode(const uint8_t *data, size_t len, struct nice_oled_raw_hid_message *out);
