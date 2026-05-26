#include "layer.h"
#include <fonts.h>
#include <zephyr/kernel.h>

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)

// Animation state machine
enum layer_anim_state {
    LAYER_ANIM_IDLE,
    LAYER_ANIM_SLIDE_OUT,
    LAYER_ANIM_SLIDE_IN,
};

static struct {
    const char *current_text;       // Currently displayed text
    uint32_t anim_start_time;       // Timestamp when animation started
    enum layer_anim_state state;    // Current animation phase
    int16_t offset_x;               // Horizontal text offset for animation
    bool flash_active;              // Whether background color is flashed
} layer_anim = {
    .current_text = NULL,
    .anim_start_time = 0,
    .state = LAYER_ANIM_IDLE,
    .offset_x = 0,
    .flash_active = false,
};

// Animation phase durations (percentages of total)
#define SLIDE_OUT_RATIO 5   // 20% for slide out
#define FLASH_DURATION_MS 100  // How long the background flash lasts

static void layer_anim_reset(void) {
    layer_anim.state = LAYER_ANIM_IDLE;
    layer_anim.offset_x = 0;
    layer_anim.anim_start_time = 0;
    layer_anim.flash_active = false;
}

// Called when a new layer is activated — starts the animation sequence
static void layer_anim_start(const char *new_text) {
    // If already animating, reset to start fresh with new text
    if (layer_anim.state != LAYER_ANIM_IDLE) {
        layer_anim_reset();
    }

    layer_anim.current_text = new_text;
    layer_anim.state = LAYER_ANIM_SLIDE_OUT;
    layer_anim.offset_x = 0;
    layer_anim.anim_start_time = k_uptime_get_32();

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH)
    layer_anim.flash_active = true;
#endif
}

// Returns the text to draw and its horizontal offset for current frame
static const char *layer_anim_get_frame(char *out_text, size_t out_size) {
    uint32_t elapsed = k_uptime_get_32() - layer_anim.anim_start_time;
    uint32_t total_ms = CONFIG_NICE_OLED_WIDGET_LAYER_ANIMATION_MS;
    uint32_t slide_out_ms = total_ms * SLIDE_OUT_RATIO / 100;

    // Flash duration check — turn off flash after FLASH_DURATION_MS
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH)
    if (layer_anim.flash_active && elapsed > FLASH_DURATION_MS) {
        layer_anim.flash_active = false;
    }
#endif

    switch (layer_anim.state) {
    case LAYER_ANIM_SLIDE_OUT:
        // Animate offset from 0 to -20 (slide left and out)
        if (elapsed < slide_out_ms) {
            int progress = elapsed * 20 / slide_out_ms;
            layer_anim.offset_x = -progress;
            return layer_anim.current_text;
        }
        // Slide out complete — swap to new text, start slide in
        layer_anim.state = LAYER_ANIM_SLIDE_IN;
        layer_anim.anim_start_time = k_uptime_get_32();
        layer_anim.offset_x = 20;  // Start off-screen right
        return layer_anim.current_text;

    case LAYER_ANIM_SLIDE_IN:
        // Animate offset from +20 to 0 (slide in from right)
        if (elapsed < total_ms - slide_out_ms) {
            int progress = elapsed * 20 / (total_ms - slide_out_ms);
            layer_anim.offset_x = 20 - progress * 2;  // 20 → 0
            return layer_anim.current_text;
        }
        // Animation complete — reset to idle
        layer_anim_reset();
        return layer_anim.current_text;

    case LAYER_ANIM_IDLE:
    default:
        return NULL;  // Caller should use static text
    }
}

#endif /* CONFIG_NICE_OLED_WIDGET_LAYER */

void draw_layer_status(lv_obj_t *canvas, const struct status_state *state) {
    lv_draw_label_dsc_t label_dsc;
#if IS_ENABLED(CONFIG_NICE_EPAPER_ON)
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_CENTER);
#else
    init_label_dsc(&label_dsc, LVGL_FOREGROUND, &pixel_operator_mono_16, LV_TEXT_ALIGN_LEFT);
#endif // CONFIG_NICE_EPAPER_ON

    char text[10] = {};
    const char *display_text;

    if (state->layer_label == NULL) {
        sprintf(text, "Layer %i", state->layer_index);
    } else {
        strncpy(text, state->layer_label, 9);
        to_uppercase(text);
    }

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    // Check if layer changed and start animation if so
    static const char *last_text = NULL;
    if (strcmp(last_text != NULL ? last_text : "", text) != 0) {
        last_text = text;
        layer_anim_start(text);
    }

    // Get animated frame text/offset
    display_text = layer_anim_get_frame(text);
    if (display_text == NULL) {
        // Idle state — use normal text
        display_text = text;
    }
#else
    display_text = text;
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
#endif

    // Draw with animation offset if applicable
    int16_t x = CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X + layer_anim.offset_x;
    lv_canvas_draw_text(canvas, x, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc, display_text);
}
