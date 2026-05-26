#include "central_draw_compat.h"

#include <zephyr/sys/util.h>

#include "../../widgets/util.h"

static lv_point_t nice_oled_central_map_point(lv_coord_t x, lv_coord_t y) {
    /* Legacy central widget coordinates already describe the portrait layout.
     * The adapter owns the mapping so future remaps stay centralized.
     */
    return (lv_point_t){.x = x, .y = y};
}

static lv_coord_t nice_oled_central_text_width(lv_coord_t x, lv_coord_t max_width) {
#if IS_ENABLED(CONFIG_NICE_OLED_NATIVE_PORTRAIT)
    if (x >= 0 && max_width > 0) {
        const lv_coord_t remaining = CANVAS_WIDTH - x;

        if (remaining <= 0) {
            return 0;
        }

        if (max_width > remaining) {
            return remaining;
        }
    }
#endif

    return max_width;
}

void nice_oled_central_draw_text_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t max_width,
                                        lv_draw_label_dsc_t *dsc, const char *text) {
    const lv_point_t mapped = nice_oled_central_map_point(x, y);
    const lv_coord_t mapped_width = nice_oled_central_text_width(mapped.x, max_width);

    if (mapped_width == 0) {
        return;
    }

    lv_canvas_draw_text(canvas, mapped.x, mapped.y, mapped_width, dsc, text);
}

void nice_oled_central_draw_img_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                       const void *src, lv_draw_img_dsc_t *dsc) {
    const lv_point_t mapped = nice_oled_central_map_point(x, y);

    lv_canvas_draw_img(canvas, mapped.x, mapped.y, src, dsc);
}

void nice_oled_central_draw_rect_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        lv_draw_rect_dsc_t *dsc) {
    const lv_point_t mapped = nice_oled_central_map_point(x, y);

    lv_canvas_draw_rect(canvas, mapped.x, mapped.y, w, h, dsc);
}

void nice_oled_central_draw_line_compat(lv_obj_t *canvas, const lv_point_t *points,
                                        uint32_t point_cnt, lv_draw_line_dsc_t *dsc) {
    lv_canvas_draw_line(canvas, points, point_cnt, dsc);
}
