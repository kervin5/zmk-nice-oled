# Display Compositor Boundaries Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extract explicit central and peripheral compositor modules from the monolithic `screen.c` / `screen_peripheral.c`, move pure draw helpers toward renderer-style ownership, and wire everything into CMake — completing Phase 1 "Define widget/layout/theme boundaries in code" without feature loss or build breakage.

**Architecture:** Create a shared compositor header defining canvas lifecycle and redraw entry points. Extract canvas init + `draw_canvas()` orchestration into `display/render/screen_central.c` and `screen_peripheral_render.c`. Move draw helpers (`battery`, `output`, `layer`, `wpm`, `profile`, `hid_status`, `mods_status`) from screen.c into the compositor as static functions — achieving renderer-style ownership without changing function signatures. Keep existing listeners as thin wrappers that call `compositor_redraw()`. Preserve `status_state` bridge throughout — it stays until all consumers migrate.

**Tech Stack:** C, Zephyr, LVGL, Kconfig, CMake

---

### Task 1: Create shared compositor interface header

**Files:**
- Create: `boards/shields/nice_oled/display/render/screen_common.h`

This header defines the compositors' public API and shared types. It is the seam between listeners (which call into compositors) and canvas orchestration (which lives inside compositors).

/* Forward declaration — status_state lives in widgets/util.h */
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

**Verification:** Run `rg -n "screen_common.h" boards/shields/nice_oled` — expect 0 matches (header not yet included anywhere).

---

### Task 2: Create central compositor implementation

**Files:**
- Create: `boards/shields/nice_oled/display/render/screen_central.c`

This file owns canvas creation, buffer setup, and the full `draw_canvas()` orchestration. It contains all draw helpers that were inline in screen.c (background, output, battery text, layer, profile, WPM, HID status, modifiers). Listeners call `nice_oled_screen_central_redraw()` instead of calling `draw_canvas()` directly.

```c
/* boards/shields/nice_oled/display/render/screen_central.c */
#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/central_state.h"
#include "../model/dirty_domains.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../widgets/battery.h"
#include "../../widgets/output.h"
#include "../../widgets/layer.h"
#include "../../widgets/wpm.h"
#include "../../widgets/profile.h"

/* Forward declarations — draw helpers that were in screen.c */
static void draw_background(lv_obj_t *canvas, const struct status_state *state);
static void draw_output_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state);
static void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_profile_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_layer_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_hid_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_mods_status(lv_obj_t *canvas, const struct status_state *state);
static void rotate_canvas(lv_obj_t *canvas);

/* Canvas orchestration — extracted from screen.c */
static void draw_canvas_central(lv_obj_t *canvas, const struct status_state *state) {
    draw_background(canvas, state);
    draw_output_status(canvas, state);
    draw_battery_text_central(canvas, state);
    draw_wpm_status(canvas, state);
    draw_profile_status(canvas, state);
    draw_layer_status(canvas, state);
    draw_hid_status(canvas, state);
    draw_mods_status(canvas, state);
    rotate_canvas(canvas);
}

int nice_oled_screen_central_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->canvas = NULL;
    comp->state = NULL;
    comp->dirty = NICE_OLED_DIRTY_NONE;
    comp->initialized = false;

    /* Canvas buffer setup — copied verbatim from screen.c zmk_widget_screen_init() */
    int canvas_width = 64;
    int canvas_height = 64;
    lv_color_t *buf = lv_mem_alloc(canvas_width * canvas_height * sizeof(lv_color_t));
    if (!buf) return -1;
    comp->cbuf = lv_img_buf_alloc(canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, buf);
    if (!comp->cbuf) {
        lv_mem_free(buf);
        return -1;
    }
    comp->canvas = lv_canvas_create(parent);
    if (!comp->canvas) {
        lv_mem_free(buf);
        return -1;
    }
    lv_canvas_set_buffer(comp->canvas, buf, canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, 0);

    comp->initialized = true;
    return 0;
}

void nice_oled_screen_central_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas) {
        return;
    }
    draw_canvas_central(comp->canvas, comp->state);
}
```

**Note:** The actual draw helper implementations (`draw_background`, `draw_output_status`, etc.) are moved verbatim from screen.c. They read from `struct status_state` and draw onto the canvas. No logic changes — just relocation.

**Verification:** Run `rg -n "nice_oled_screen_central_init|nice_oled_screen_central_redraw" boards/shields/nice_oled` — expect matches only in this file (not yet included anywhere).

---

### Task 3: Create peripheral compositor implementation

**Files:**
- Create: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

Same pattern as central but for the peripheral path. The peripheral draw_canvas is simpler — it calls fewer helpers.

```c
/* boards/shields/nice_oled/display/render/screen_peripheral_render.c */
#include <zephyr/kernel.h>
#include <lvgl.h>
#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include "../model/peripheral_state.h"
#include "screen_common.h"
#include "../../widgets/util.h"
#include "../../widgets/battery.h"

static void draw_background(lv_obj_t *canvas, const struct status_state *state);
static void draw_output_status(lv_obj_t *canvas, const struct status_state *state);
static void draw_battery_status_peripheral(lv_obj_t *canvas, const struct status_state *state);
static void rotate_canvas(lv_obj_t *canvas);

static void draw_canvas_peripheral(lv_obj_t *canvas, const struct status_state *state) {
    draw_background(canvas, state);
    draw_output_status(canvas, state);
    draw_battery_status_peripheral(canvas, state);
    rotate_canvas(canvas);
}

int nice_oled_screen_peripheral_init(struct nice_oled_compositor *comp, lv_obj_t *parent) {
    comp->obj = parent;
    comp->cbuf = NULL;
    comp->canvas = NULL;
    comp->state = NULL;
    comp->dirty = NICE_OLED_DIRTY_NONE;
    comp->initialized = false;

    /* Canvas buffer setup — same pattern as central */
    int canvas_width = 64;
    int canvas_height = 64;
    lv_color_t *buf = lv_mem_alloc(canvas_width * canvas_height * sizeof(lv_color_t));
    if (!buf) return -1;
    comp->cbuf = lv_img_buf_alloc(canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, buf);
    if (!comp->cbuf) {
        lv_mem_free(buf);
        return -1;
    }
    comp->canvas = lv_canvas_create(parent);
    if (!comp->canvas) {
        lv_mem_free(buf);
        return -1;
    }
    lv_canvas_set_buffer(comp->canvas, buf, canvas_width, canvas_height, LV_IMG_CF_TRUE_COLOR, 0);

    comp->initialized = true;
    return 0;
}

void nice_oled_screen_peripheral_redraw(struct nice_oled_compositor *comp) {
    if (!comp || !comp->initialized || !comp->canvas || !comp->state) {
        return;
    }
    draw_canvas_peripheral(comp->canvas, comp->state);
}
```

**Verification:** Run `rg -n "nice_oled_screen_peripheral_init|nice_oled_screen_peripheral_redraw" boards/shields/nice_oled` — expect matches only in this file.

---

### Task 4: Move draw helpers from screen.c into screen_central.c as static functions

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

Move the following draw helper functions from screen.c into screen_central.c as `static` functions. They remain unchanged — same signatures, same logic, same access to `struct status_state`:

- `draw_background(lv_obj_t *canvas, const struct status_state *state)`
- `draw_output_status(lv_obj_t *canvas, const struct status_state *state)`
- `draw_battery_text_central(lv_obj_t *canvas, const struct status_state *state)` (the split-battery text variant used by central)
- `draw_wpm_status(lv_obj_t *canvas, const struct status_state *state)`
- `draw_profile_status(lv_obj_t *canvas, const struct status_state *state)`
- `draw_layer_status(lv_obj_t *canvas, const struct status_state *state)`
- `draw_hid_status(lv_obj_t *canvas, const struct status_state *state)`
- `draw_mods_status(lv_obj_t *canvas, const struct status_state *state)`

These become static in screen_central.c because they are only called from within the compositor's `draw_canvas_central()`. The old widget header files (`battery.h`, `output.h`, etc.) still declare non-static versions for any external callers that need them (e.g., tests or other widgets).

**Verification:** Run `rg -n "static void draw_background|static void draw_output_status" boards/shields/nice_oled/display/render/screen_central.c` — expect all 8 helpers present as static functions.

---

### Task 5: Refactor screen.c to use compositor + thin listener wrappers

**Files:**
- Modify: `boards/shields/nice_oled/widgets/screen.c`

This is the largest change but mechanically straightforward. Every event listener in screen.c follows this template:

```c
/* OLD (in screen.c): */
static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state) {
    widget->state.battery = state.level;
    widget->state.charging = state.charging;
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}

/* NEW (in screen.c): */
static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state) {
    const nice_oled_dirty_mask_t dirty =
        nice_oled_central_apply_battery_state(&widget->state.central.battery, state.level);
    if (dirty == NICE_OLED_DIRTY_NONE) return;
    widget->state.dirty |= dirty;
    nice_oled_status_state_sync_from_central(&widget->state);
    nice_oled_screen_central_redraw(&widget->compositor);
}
```

Changes to make in screen.c:
1. Add `struct nice_oled_compositor compositor;` field to the widget struct (or static local)
2. Replace every `draw_canvas(widget->obj, widget->cbuf, &widget->state)` call with `nice_oled_screen_central_redraw(&widget->compositor)`
3. Keep all draw helper calls (`draw_battery_text`, `draw_mods_status`, etc.) as-is — they are now in screen_central.c but still accessible via the same names
4. The `zmk_widget_screen_init()` function should call `nice_oled_screen_central_init(&widget->compositor, parent)` instead of creating canvas inline

**Key constraint:** Do NOT remove any draw helper functions from screen.c yet. They stay in screen_central.c and are called by the compositor's `draw_canvas_central()`. The listeners just stop calling `draw_canvas()` directly.

**Verification:** Run `rg -n "nice_oled_screen_central_redraw" boards/shields/nice_oled/widgets/screen.c` — expect every redraw path to call it instead of `draw_canvas`.

---

### Task 6: Refactor screen_peripheral.c similarly

**Files:**
- Modify: `boards/shields/nice_oled/widgets/screen_peripheral.c`

Same pattern as Task 5 but for the peripheral path. Replace `draw_canvas()` calls with `nice_oled_screen_peripheral_redraw()`. Add compositor field to widget struct. Call `nice_oled_screen_peripheral_init()` in init function.

**Verification:** Run `rg -n "nice_oled_screen_peripheral_redraw" boards/shields/nice_oled/widgets/screen_peripheral.c` — expect all redraw paths to call it.

---

### Task 7: Update CMakeLists.txt to compile new compositor files

**Files:**
- Modify: `boards/shields/nice_oled/CMakeLists.txt`

Add the new compositor sources under the existing conditional blocks:

```cmake
if(CONFIG_ZMK_DISPLAY AND CONFIG_NICE_OLED_WIDGET_STATUS)
    # ... existing model sources (central_state, peripheral_state, raw_hid_state) ...

    if(NOT CONFIG_ZMK_SPLIT || CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
        zephyr_library_sources(display/render/screen_common.c)       # NEW — inline impls
        zephyr_library_sources(display/render/screen_central.c)      # NEW — central compositor

        # Keep existing widget sources (screen.c now delegates to compositor)
        zephyr_library_sources(widgets/screen.c)
        # ... rest of existing sources unchanged ...
    else()
        zephyr_library_sources(display/render/screen_common.c)       # NEW — inline impls
        zephyr_library_sources(display/render/screen_peripheral_render.c)  # NEW

        # Keep existing peripheral sources
        zephyr_library_sources(widgets/screen_peripheral.c)
        # ... rest of existing sources unchanged ...
    endif()
endif()
```

Create `display/render/screen_common.c` (minimal — only non-inline helpers):
```c
/* boards/shields/nice_oled/display/render/screen_common.c */
#include "screen_common.h"
/* All public functions are declared as static inline in screen_common.h.
 * This file exists only to satisfy CMake's expectation that headers with
 * inline implementations have a corresponding .c when used across modules. */
```

**Verification:** Run `rg -n "screen_central|screen_peripheral_render" boards/shields/nice_oled/CMakeLists.txt` — expect both new sources listed in their respective conditional blocks.

---

### Task 8: Smoke build verification

**Files:** None (build only)

Run the smoke build to verify everything compiles:
```sh
source /Users/kervin/Coding/keyboard/zmk-nice-oled/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew
cd /Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/zmk-build-smoke/zmk
west build -p always -s app -d build/nice_oled -b nice_nano_v2 -- \
  -DSHIELD="corne_left nice_oled" \
  -DZMK_CONFIG=/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config \
  -DZMK_EXTRA_MODULES=/Users/kervin/Coding/keyboard/zmk-nice-oled
```

Expected: build passes with same or fewer warnings. No new errors.

---

### Task 9: Update tracker and handoff doc

**Files:**
- Modify: `docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md`
- Modify: `handoffs/2026-05-25-display-platform-migration-handoff.md`

Update tracker:
```markdown
### Phase 1: Structural Containment
- [x] Define widget/layout/theme boundaries in code   ← CHECK THIS
```

Add handoff note:
```markdown
### 5. Compositor Boundaries Defined (NEW)

Central and peripheral compositors now own canvas lifecycle:
- `display/render/screen_central.c` — central draw orchestration
- `display/render/screen_peripheral_render.c` — peripheral draw orchestration
- `display/render/screen_common.h` — shared compositor interface

Listeners call `nice_oled_screen_central_redraw()` instead of `draw_canvas()`.
Draw helpers moved into compositor files as static functions (same signatures, same logic).
`status_state` bridge preserved for backward compatibility.
```

---

## What This Completes

- **Phase 1 tracker item:** "Define widget/layout/theme boundaries in code" ✅
- **Handoff recommendation:** "Introduce explicit central and peripheral compositor modules" ✅
- **Spec Phase 1:** Structural containment — models extracted, compositors defined, renderers separated

## What Remains for Next Session

- **Phase 2:** Move fixed modifiers to persistent LVGL widgets (highest impact hot path)
- **Phase 2:** Move WPM value and RAW HID text fields to persistent object widgets
- **Phase 3:** Normalize ownership — remove duplicate modifier architecture, orphan paths
- **Phase 4:** Reduce full-screen redraw cost — dirty flags driving render policy

## Things To Be Careful Not To Do

- Do NOT delete `status_state` yet — all draw consumers still use it
- Do NOT change any visual behavior — this is a pure refactor with zero feature changes
- Do NOT remove the old widget files (`widgets/battery.c`, etc.) until all wrappers are verified
- Do NOT add dirty-flag-driven incremental redraw logic yet — that comes after compositors exist
