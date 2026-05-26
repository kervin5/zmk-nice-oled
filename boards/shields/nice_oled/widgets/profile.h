#pragma once

#include <lvgl.h>
#include "../display/model/central_state.h"

void draw_profile_status(lv_obj_t *canvas, const struct nice_oled_central_state *state);