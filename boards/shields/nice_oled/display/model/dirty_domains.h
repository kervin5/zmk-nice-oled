#pragma once

#include <stdint.h>
#include <zephyr/sys/util_macro.h>

typedef uint32_t nice_oled_dirty_mask_t;

enum nice_oled_dirty_domain {
    NICE_OLED_DIRTY_NONE = 0,
    NICE_OLED_DIRTY_LAYOUT = BIT(0),
    NICE_OLED_DIRTY_BATTERY = BIT(1),
    NICE_OLED_DIRTY_OUTPUT = BIT(2),
    NICE_OLED_DIRTY_LAYER = BIT(3),
    NICE_OLED_DIRTY_WPM = BIT(4),
    NICE_OLED_DIRTY_MODIFIERS = BIT(5),
    NICE_OLED_DIRTY_RAW_HID = BIT(6),
    NICE_OLED_DIRTY_SLEEP = BIT(7),
    NICE_OLED_DIRTY_CONNECTION = BIT(8),
    NICE_OLED_DIRTY_THEME = BIT(9),
};
