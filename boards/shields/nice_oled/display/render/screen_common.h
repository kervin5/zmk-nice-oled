/* boards/shields/nice_oled/display/render/screen_common.h */
#pragma once

#include <zephyr/kernel.h>
#include <lvgl.h>
#include "../model/dirty_domains.h"

struct status_state;

struct nice_oled_compositor {
    lv_obj_t *obj;              /* LVGL object (screen container) */
    lv_img_dsc_t *cbuf;         /* Canvas framebuffer buffer descriptor */
    lv_obj_t *canvas;           /* Canvas LVGL object */
    const struct status_state *state;  /* Pointer to caller's status_state for redraw */
    nice_oled_dirty_mask_t dirty;   /* Accumulated dirty domains */
    bool initialized;
};

/* Central compositor entry points */
int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent);
void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp);

/* Peripheral compositor entry points */
int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent);
void nice_oled_screen_peripheral_redraw(struct nice_oled_compositor *comp);
