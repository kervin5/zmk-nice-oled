#pragma once

#include <lvgl.h>
#include "../display/model/central_state.h"

struct layer_status_state {
    uint8_t index;
    const char *label;
};

void draw_layer_status(lv_obj_t *canvas, const struct nice_oled_central_state *state);