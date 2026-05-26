#include "util.h"
#include <ctype.h>
#include <zephyr/kernel.h>

void to_uppercase(char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    str[i] = toupper(str[i]);
  }
}

void draw_background(lv_obj_t *canvas) {
  lv_draw_rect_dsc_t rect_black_dsc;
  init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

  lv_canvas_draw_rect(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                      &rect_black_dsc);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color,
                    const lv_font_t *font, lv_text_align_t align) {
  lv_draw_label_dsc_init(label_dsc);
  label_dsc->color = color;
  label_dsc->font = font;
  label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
  lv_draw_rect_dsc_init(rect_dsc);
  rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color,
                   uint8_t width) {
  lv_draw_line_dsc_init(line_dsc);
  line_dsc->color = color;
  line_dsc->width = width;
}

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[], lv_coord_t width, lv_coord_t height) {
  size_t pixel_count = (size_t)width * (size_t)height;
  static lv_color_t cbuf_tmp[NICE_OLED_LEGACY_ROTATION_CANVAS_SIZE *
                             NICE_OLED_LEGACY_ROTATION_CANVAS_SIZE];

  if (pixel_count > sizeof(cbuf_tmp) / sizeof(lv_color_t)) {
    return;
  }

  memcpy(cbuf_tmp, cbuf, pixel_count * sizeof(lv_color_t));

  lv_img_dsc_t img;
  img.data = (void *)cbuf_tmp;
  img.header.cf = LV_IMG_CF_TRUE_COLOR;
  img.header.w = width;
  img.header.h = height;

  lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
  lv_canvas_transform(canvas, &img, 900, LV_IMG_ZOOM_NONE, -1, 0,
                      width / 2, height / 2, false);
}
