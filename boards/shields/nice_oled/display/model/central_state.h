#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <zmk/endpoints.h>

#include "dirty_domains.h"
#include "raw_hid_state.h"

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
#define NICE_OLED_SPLIT_TOTAL_DEVICES (1 + CONFIG_ZMK_SPLIT_BLE_CENTRAL_PERIPHERALS)
#else
#define NICE_OLED_SPLIT_TOTAL_DEVICES 1
#endif

#define NICE_OLED_WPM_HISTORY_LEN 10

struct battery_info {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

struct nice_oled_central_state {
    uint8_t battery;
    bool charging;
    struct battery_info batteries[NICE_OLED_SPLIT_TOTAL_DEVICES];
    struct zmk_endpoint_instance selected_endpoint;
    int active_profile_index;
    bool active_profile_connected;
    bool active_profile_bonded;
    uint8_t layer_index;
    const char *layer_label;
    uint8_t wpm[NICE_OLED_WPM_HISTORY_LEN];
    uint8_t mod_state;
    struct nice_oled_raw_hid_state raw_hid;
};

void nice_oled_central_state_init(struct nice_oled_central_state *state);
nice_oled_dirty_mask_t nice_oled_central_apply_battery_state(struct nice_oled_central_state *state,
                                                             uint8_t battery, bool charging);
nice_oled_dirty_mask_t
nice_oled_central_apply_split_battery_state(struct nice_oled_central_state *state, uint8_t source,
                                            uint8_t level, bool usb_present);
nice_oled_dirty_mask_t nice_oled_central_apply_layer(struct nice_oled_central_state *state,
                                                     uint8_t index, const char *label);
nice_oled_dirty_mask_t nice_oled_central_apply_output(struct nice_oled_central_state *state,
                                                      const struct zmk_endpoint_instance *endpoint,
                                                      int active_profile_index,
                                                      bool active_profile_connected,
                                                      bool active_profile_bonded);
nice_oled_dirty_mask_t nice_oled_central_apply_wpm(struct nice_oled_central_state *state,
                                                   uint8_t wpm);
nice_oled_dirty_mask_t nice_oled_central_apply_modifiers(struct nice_oled_central_state *state,
                                                         uint8_t mods);
