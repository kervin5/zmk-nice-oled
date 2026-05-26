#pragma once

#include <lvgl.h>

void nice_oled_central_draw_text_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t max_width,
                                        lv_draw_label_dsc_t *dsc, const char *text);
void nice_oled_central_draw_img_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                       const void *src, lv_draw_img_dsc_t *dsc);
void nice_oled_central_draw_rect_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        lv_draw_rect_dsc_t *dsc);
void nice_oled_central_draw_line_compat(lv_obj_t *canvas, const lv_point_t *points,
                                        uint32_t point_cnt, lv_draw_line_dsc_t *dsc);
