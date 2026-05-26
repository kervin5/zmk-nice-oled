/* boards/shields/nice_oled/display/render/screen_common.h */
#ifndef SCREEN_COMMON_H
#define SCREEN_COMMON_H

#include <zephyr/kernel.h>
#include <lvgl.h>
#include "../model/dirty_domains.h"

struct nice_oled_central_state;
struct nice_oled_peripheral_state;

struct nice_oled_compositor {
    lv_obj_t *obj;              /* LVGL object (screen container) */
    lv_img_dsc_t *cbuf;         /* Canvas framebuffer buffer descriptor */
    lv_obj_t *canvas;           /* Canvas LVGL object */
    const struct nice_oled_central_state *central_state;  /* Central typed model pointer */
    const struct nice_oled_peripheral_state *peripheral_state;  /* Peripheral typed model pointer */
    nice_oled_dirty_mask_t dirty;   /* Accumulated dirty domains */
    bool initialized;
};

/* Central compositor entry points */
int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent);
void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp);

/* Peripheral compositor entry points */
int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent);
void nice_oled_screen_peripheral_redraw(struct nice_oled_compositor *comp);

#endif /* SCREEN_COMMON_H */
