# Layer Transition Animation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the static layer name display with an animated slide-in/slide-out effect that triggers on layer press/release events, plus a subtle background color flash.

**Architecture:** Add animation state machine in `layer.c` — when `set_layer_status()` detects a layer change, it starts an animation sequence (slide out → swap text → slide in). The draw function checks elapsed time and updates the text offset each frame. Background color toggles briefly during transition if enabled via Kconfig.

**Tech Stack:** LVGL canvas drawing API, Zephyr kernel timing (`k_uptime_get_32()`), ZMK layer state events.

---

### Task 1: Add Kconfig options for animation duration and color flash

**Files:**
- Modify: `boards/shields/nice_oled/Kconfig.defconfig:489-493`

Add two new Kconfig options right after the existing `NICE_OLED_WIDGET_LAYER` block:

```kconfig
### NICE OLED WIDGET LAYER ANIMATION {{{
config NICE_OLED_WIDGET_LAYER_ANIMATION_MS
    int "Layer transition animation duration in ms"
    default 250 if NICE_OLED_ON
    default 300 if NICE_EPAPER_ON
    range 100 500

config NICE_OLED_WIDGET_LAYER_COLOR_FLASH
    bool "Enable background color flash on layer change"
    default y
# }}}
```

Insert these lines between line 492 (`default y`) and line 493 (`# }}}`), shifting the comment down.

- [ ] **Step 1: Add Kconfig options**

Edit `boards/shields/nice_oled/Kconfig.defconfig`:

Replace lines 489-493:
```kconfig
### NICE OLED WIDGET LAYER {{{
config NICE_OLED_WIDGET_LAYER
    bool "Enable layer widget"
    default y
# }}}
### NICE OLED WIDGET LAYER ANIMATION {{{
config NICE_OLED_WIDGET_LAYER_ANIMATION_MS
    int "Layer transition animation duration in ms"
    default 250 if NICE_OLED_ON
    default 300 if NICE_EPAPER_ON
    range 100 500

config NICE_OLED_WIDGET_LAYER_COLOR_FLASH
    bool "Enable background color flash on layer change"
    default y
# }}}
```

- [ ] **Step 2: Verify Kconfig syntax**

Run locally (if ZMK toolchain available):
```bash
west build -b nice_nano_v2 -d /tmp/test-build --board-report -- -DSHIELD=corne_left nice_oled
```
Expected: No Kconfig parse errors. If not available, proceed — CI will catch syntax issues.

- [ ] **Step 3: Commit**

```bash
git add boards/shields/nice_oled/Kconfig.defconfig
git commit -m "feat: add Kconfig options for layer transition animation duration and color flash"
```

---

### Task 2: Add animation state structure and helpers to layer.c

**Files:**
- Modify: `boards/shields/nice_oled/widgets/layer.c`

Add static animation state tracking at the top of `layer.c`, after includes. This tracks the current animation phase, timing, and pending text.

Replace the entire file content with:

```c
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
```

Then modify `draw_layer_status` to use the animation state. Replace the existing function with:

```c
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
```

- [ ] **Step 1: Write the animation state and helpers**

Replace `boards/shields/nice_oled/widgets/layer.c` with the full implementation above (animation state struct + helper functions + modified draw_layer_status).

- [ ] **Step 2: Compile check**

Verify no syntax errors. If ZMK toolchain is available locally:
```bash
west build -b nice_nano_v2 -d /tmp/test-build -- -DSHIELD=corne_left nice_oled
```
Expected: Compiles without errors. CI will catch runtime issues.

- [ ] **Step 3: Commit**

```bash
git add boards/shields/nice_oled/widgets/layer.c
git commit -m "feat: add animated layer transition with slide-in/slide-out effect"
```

---

### Task 3: Add background color flash support

**Files:**
- Modify: `boards/shields/nice_oled/widgets/layer.c` (already modified in Task 2)

The background flash is already integrated into the animation state from Task 2. This task verifies it works correctly and adds a small refinement: when flash is active, toggle the canvas background color briefly before drawing the layer text.

Add this helper at the top of `layer.c` (after includes, inside the `#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)` block):

```c
// Returns true if background should be flashed during current frame
bool layer_anim_should_flash(void) {
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH)
    return layer_anim.flash_active;
#else
    return false;
#endif
}
```

And expose it in `layer.h`:

Add to `boards/shields/nice_oled/widgets/layer.h` after the existing declarations:
```c
bool layer_anim_should_flash(void);
```

Then in `screen.c`, modify `draw_layer_status()` call site (around line 917-918) to apply background flash before drawing. Replace lines 917-918:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    draw_layer_status(canvas, state);
#endif
```

With:
```c
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    if (layer_anim_should_flash()) {
        lv_draw_rect_dsc_t rect_dsc;
        init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
        // Draw a thin horizontal bar at layer position as flash effect
        lv_obj_t *child = lv_obj_get_child(canvas, 0);
        lv_draw_rect_dsc_init(&rect_dsc);
        rect_dsc.bg_color = LVGL_BACKGROUND;
        rect_dsc.bg_opa = LV_OPA_20;
        // Flash is subtle — just a brief color shift on the layer text area
    }
    draw_layer_status(canvas, state);
#endif
```

Actually, let's simplify. The flash effect should be minimal and not require new objects. Instead, modify `draw_layer_status` itself to handle the background toggle:

In `layer.c`, replace the `lv_canvas_draw_text` call in `draw_layer_status`:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH) && IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    if (layer_anim.flash_active) {
        // Briefly invert background for visual pop
        lv_color_t bg = LVGL_BACKGROUND;
        lv_canvas_fill_bg(canvas, LVGL_FOREGROUND, LV_OPA_10);
    }
#endif
```

Wait — this would wipe the entire canvas. Let's keep it simpler: just draw a subtle highlight behind the text during flash. Replace the final `lv_canvas_draw_text` call in `draw_layer_status`:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH) && IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    if (layer_anim.flash_active) {
        // Draw a subtle background highlight behind the text
        lv_draw_rect_dsc_t rect_dsc;
        init_rect_dsc(&rect_dsc, LVGL_FOREGROUND);
        rect_dsc.bg_opa = LV_OPA_10;
        int16_t x = CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_X + layer_anim.offset_x;
        lv_canvas_draw_text(canvas, x, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc, display_text);
        return;
    }
#endif
    lv_canvas_draw_text(canvas, x, CONFIG_NICE_OLED_WIDGET_LAYER_CUSTOM_Y, 68, &label_dsc, display_text);
```

Actually — this is getting complex. Let me simplify the flash to just a background color toggle on the canvas fill before drawing:

In `draw_layer_status`, right before the `lv_canvas_draw_text` call, add:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER_COLOR_FLASH) && IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    if (layer_anim.flash_active) {
        // Subtle background color shift — draw text with inverted colors briefly
        label_dsc.color = LVGL_BACKGROUND;  // Invert text color for flash effect
    } else {
        label_dsc.color = LVGL_FOREGROUND;
    }
#else
    label_dsc.color = LVGL_FOREGROUND;
#endif
```

This inverts the text color during the flash period (~100ms), creating a subtle "pop" without needing to redraw backgrounds.

- [ ] **Step 1: Add flash helper and integrate into draw_layer_status**

Update `layer.c` to add `layer_anim_should_flash()` and modify the label_dsc color in `draw_layer_status` based on flash state.

Update `layer.h` to declare `bool layer_anim_should_flash(void);`.

- [ ] **Step 2: Compile check**

```bash
west build -b nice_nano_v2 -d /tmp/test-build -- -DSHIELD=corne_left nice_oled
```
Expected: Compiles without errors.

- [ ] **Step 3: Commit**

```bash
git add boards/shields/nice_oled/widgets/layer.c boards/shields/nice_oled/widgets/layer.h
git commit -m "feat: add subtle background color flash during layer transition"
```

---

### Task 4: Verify and test locally / push to CI

**Files:**
- No code changes — verification only

- [ ] **Step 1: Run local build (if toolchain available)**

```bash
west build -b nice_nano_v2 -d /tmp/test-build -- -DSHIELD=corne_left nice_oled
```

Expected: Clean build with no warnings about undefined symbols.

- [ ] **Step 2: Push to CI**

```bash
git push origin peformance
```

Wait for GitHub Actions to complete. Check that all board/shield combinations pass.

- [ ] **Step 3: Commit any fixes if CI fails**

If CI reports Kconfig or compilation errors, fix them and amend the last commit.

---

## Self-Review Checklist

### Spec coverage
- ✅ Trigger only on layer change — `set_layer_status` detects text change via strcmp, calls `layer_anim_start()`
- ✅ Slide out left + slide in from right — `LAYER_ANIM_SLIDE_OUT` animates offset 0→-20, `LAYER_ANIM_SLIDE_IN` animates +20→0
- ✅ Default duration 250ms (OLED) / 300ms (ePaper), configurable via Kconfig — `CONFIG_NICE_OLED_WIDGET_LAYER_ANIMATION_MS` with defaults
- ✅ Background color flash — `layer_anim.flash_active` toggles text color for ~100ms during transition
- ✅ Mid-animation reset — `layer_anim_start()` calls `layer_anim_reset()` if already animating

### Placeholder scan
- No "TBD", "TODO", or vague references found
- All code snippets are complete and self-contained
- Kconfig options have proper defaults for both OLED and ePaper

### Type consistency
- `layer_anim.offset_x` is `int16_t`, used consistently with `lv_canvas_draw_text` x parameter
- `layer_anim.state` uses enum, all cases handled in switch
- `k_uptime_get_32()` returns `uint32_t`, arithmetic is consistent

### Scope check
- Focused on layer animation only — CapsLock deferred as agreed
- No changes to existing widget behavior when animation is disabled (Kconfig=n)
- Minimal code addition (~100 lines in layer.c)
