#pragma once

#include <lvgl.h>
#include "../display/model/central_state.h"

struct wpm_status_state {
    uint8_t wpm;
};

void draw_wpm_status(lv_obj_t *canvas, const struct nice_oled_central_state *state);