#include "layer.h"
#include "util.h"
#include "../display/render/central_draw_compat.h"
#include <fonts.h>
#include <zephyr/kernel.h>

void draw_layer_status(lv_obj_t *canvas, const struct nice_oled_central_state *state) {
    lv_draw_label_dsc_t label_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_CENTER);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};

    if (state->layer_label == NULL) {
        sprintf(text, "Layer %i", state->layer_index);
    } else {
        strncpy(text, state->layer_label, 9);
        to_uppercase(text);
    }

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    lv_draw_rect_dsc_t rect_bg_dsc;
    init_rect_dsc(&rect_bg_dsc, LVGL_BACKGROUND);
    nice_oled_central_draw_rect_compat(canvas, 0, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y - 2, 68,
                                       19, &rect_bg_dsc);
#endif
    nice_oled_central_draw_text_compat(canvas, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X,
                                       CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc,
                                       text);
}
