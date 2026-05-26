#pragma once

#include <string.h>

#include <lvgl.h>

#include "../display/model/central_state.h"
#include "../display/model/peripheral_state.h"

// nice_epaper and nice_oled standard width = 68, height = 160
#define CANVAS_WIDTH CONFIG_NICE_OLED_CUSTOM_CANVAS_WIDTH
#define CANVAS_HEIGHT CONFIG_NICE_OLED_CUSTOM_CANVAS_HEIGHT
#define NICE_OLED_LEGACY_ROTATION_CANVAS_SIZE CANVAS_HEIGHT

#define LVGL_BACKGROUND                                                                            \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_INVERTED) ? lv_color_black() : lv_color_white()
#define LVGL_FOREGROUND                                                                            \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_INVERTED) ? lv_color_white() : lv_color_black()

void to_uppercase(char *str);
void draw_background(lv_obj_t *canvas);
void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color);
void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color, uint8_t width);
void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color, const lv_font_t *font,
                    lv_text_align_t align);
void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[], lv_coord_t width, lv_coord_t height);
