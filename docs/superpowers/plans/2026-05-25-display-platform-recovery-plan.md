# Display Platform Recovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bring the current mid-refactor display platform back to truthful feature parity, make native portrait the canonical central rendering path, and then finish the remaining performance work without losing any existing display capabilities.

**Architecture:** Treat the current code as a partially successful migration rather than a clean slate. First restore enough render parity to establish a trustworthy visual baseline, then move the central display off the square-buffer-plus-rotation model and onto a portrait-native path. Use a compatibility draw adapter where needed during migration so we do not spray coordinate conditionals across every widget.

**Tech Stack:** C, Zephyr, ZMK, LVGL, Kconfig, CMake, west, GitHub Actions

---

## File Map

- `boards/shields/nice_oled/display/render/screen_central.c`
  - Central canvas compositor orchestration
- `boards/shields/nice_oled/display/render/central_draw_compat.{c,h}`
  - Compatibility adapter for legacy square-space central draw helpers during portrait migration
- `boards/shields/nice_oled/display/render/screen_peripheral_render.c`
  - Peripheral canvas compositor orchestration
- `boards/shields/nice_oled/widgets/battery.c`
  - Smart battery animation lifecycle and central battery text/image helpers
- `boards/shields/nice_oled/widgets/layer.c`
  - Central layer drawing that still uses existing coordinate assumptions
- `boards/shields/nice_oled/widgets/profile.c`
  - Central profile drawing that still uses existing coordinate assumptions
- `boards/shields/nice_oled/widgets/raw_hid_label.{c,h}`
  - Persistent LVGL label widget for RAW HID fields
- `boards/shields/nice_oled/widgets/screen.c`
  - Central display controller, label initialization, event wiring
- `boards/shields/nice_oled/widgets/screen_peripheral.c`
  - Peripheral display controller and redraw triggers
- `boards/shields/nice_oled/widgets/util.c`
  - Rotation scratch path and canvas transform helper
- `boards/shields/nice_oled/widgets/wpm.c`
  - Central WPM graph/text drawing that still uses existing coordinate assumptions
- `boards/shields/nice_oled/src/raw_hid/usb_hid.c`
  - USB transport send path
- `boards/shields/nice_oled/src/raw_hid/hog.c`
  - BLE transport send path
- `docs/superpowers/tracking/2026-05-25-display-platform-migration-task-tracker.md`
  - Reconciled source-of-truth tracker

---

## Native Portrait Note

`CONFIG_NICE_OLED_NATIVE_PORTRAIT` already exists in Kconfig, but the central compositor still ignores it and always renders through the square rotation path. This plan now treats native portrait as a first-class recovery milestone rather than a later optional optimization.

---

### Task 1: Restore Central and Peripheral Render Parity

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`
- Modify: `boards/shields/nice_oled/widgets/battery.c`
- Modify: `boards/shields/nice_oled/widgets/output.h`
- Modify: `boards/shields/nice_oled/widgets/output.c`

- [ ] **Step 1: Record the intended renderer contract before changing code**

Add a short comment block in each compositor describing which responsibilities belong there:

```c
/* Central compositor responsibilities:
 * - background
 * - output/profile
 * - battery text/image for central mode
 * - layer and optional canvas WPM
 * Persistent widgets own RAW HID labels and animation-only objects.
 */
```

- [ ] **Step 2: Restore the normal central battery path**

Make `draw_canvas_central()` route the non-split battery configuration through `draw_battery_status()`:

```c
#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) && \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    draw_battery_status(canvas, state);
#else
    draw_battery_text_central(canvas, state);
#endif
```

- [ ] **Step 3: Restore peripheral status rendering explicitly**

Do not leave peripheral rendering as “background only.” Either redraw battery/connection on the canvas or move both into persistent widgets, but make the owner explicit:

```c
static void draw_canvas_peripheral(lv_obj_t *canvas,
                                   const struct nice_oled_peripheral_state *state) {
    draw_background(canvas);
    draw_peripheral_connection_status(canvas, state);
    draw_peripheral_battery_status(canvas, state);
}
```

- [ ] **Step 4: Run the smoke build**

Run:

```sh
source .venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew
cd /Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/zmk-build-smoke/zmk
west build -p always -s app -d build/nice_oled -b nice_nano_v2 -- \
  -DSHIELD="corne_left nice_oled" \
  -DZMK_CONFIG=/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config \
  -DZMK_EXTRA_MODULES=/Users/kervin/Coding/keyboard/zmk-nice-oled
```

Expected: build passes, no new compile warnings from the touched files.

- [ ] **Step 5: Commit**

```sh
git add boards/shields/nice_oled/display/render/screen_central.c \
        boards/shields/nice_oled/display/render/screen_peripheral_render.c \
        boards/shields/nice_oled/widgets/battery.c \
        boards/shields/nice_oled/widgets/output.c \
        boards/shields/nice_oled/widgets/output.h
git commit -m "fix: restore display renderer parity"
```

---

### Task 2: Make Native Portrait the Canonical Central Path

**Files:**
- Create: `boards/shields/nice_oled/display/render/central_draw_compat.h`
- Create: `boards/shields/nice_oled/display/render/central_draw_compat.c`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/widgets/util.c`
- Modify: `boards/shields/nice_oled/widgets/util.h`
- Modify: `boards/shields/nice_oled/widgets/battery.c`
- Modify: `boards/shields/nice_oled/widgets/output.c`
- Modify: `boards/shields/nice_oled/widgets/layer.c`
- Modify: `boards/shields/nice_oled/widgets/profile.c`
- Modify: `boards/shields/nice_oled/widgets/wpm.c`

- [x] **Step 1: Introduce a compatibility draw adapter for legacy central coordinates**

Do not scatter `if (native_portrait)` logic through every widget. Add a migration layer that can translate legacy central draw primitives into portrait-canonical canvas operations:

```c
void nice_oled_central_draw_text_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t max_width,
                                        const lv_draw_label_dsc_t *dsc, const char *text);
void nice_oled_central_draw_img_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                       const void *src, const lv_draw_img_dsc_t *dsc);
void nice_oled_central_draw_rect_compat(lv_obj_t *canvas, lv_coord_t x, lv_coord_t y,
                                        lv_coord_t w, lv_coord_t h,
                                        const lv_draw_rect_dsc_t *dsc);
void nice_oled_central_draw_line_compat(lv_obj_t *canvas, const lv_point_t *points,
                                        uint32_t point_cnt, const lv_draw_line_dsc_t *dsc);
```

- [x] **Step 2: Make the central compositor honor `CONFIG_NICE_OLED_NATIVE_PORTRAIT`**

Switch the central canvas buffer to portrait dimensions when native portrait is enabled, and skip the full-buffer rotation path on that branch:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_NATIVE_PORTRAIT)
    lv_canvas_set_buffer(comp->canvas, cbuf, CANVAS_WIDTH, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
#else
    lv_canvas_set_buffer(comp->canvas, cbuf, CANVAS_HEIGHT, CANVAS_HEIGHT, LV_IMG_CF_TRUE_COLOR);
#endif
```

```c
#if !IS_ENABLED(CONFIG_NICE_OLED_NATIVE_PORTRAIT)
    rotate_canvas(comp->canvas, (lv_color_t *)comp->raw_cbuf);
#endif
```

- [x] **Step 3: Migrate central widget draw helpers onto the compatibility adapter**

Update the central draw helpers that still assume square-space coordinates so they stop calling raw `lv_canvas_draw_*` directly:

```c
nice_oled_central_draw_text_compat(canvas, CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_X,
                                   CONFIG_NICE_OLED_WIDGET_BATTERY_CUSTOM_Y, 42,
                                   &label_right_dsc, text);
```

This task owns the active central draw call sites in:
- `widgets/battery.c`
- `widgets/output.c`
- `widgets/layer.c`
- `widgets/profile.c`
- `widgets/wpm.c`

- [x] **Step 4: Verify central native portrait is now a real path**

Run:

```sh
rg -n "NICE_OLED_NATIVE_PORTRAIT|lv_canvas_set_buffer\\(|rotate_canvas\\(" \
  boards/shields/nice_oled/display/render/screen_central.c \
  boards/shields/nice_oled/display/render/central_draw_compat.c \
  boards/shields/nice_oled/widgets/{battery,output,layer,profile,wpm}.c
```

Observed:
- central compositor has distinct native-portrait and legacy-rotation branches
- migrated central widget draw helpers route through the compatibility adapter

- [x] **Step 5: Re-run the smoke build**

Run the same `west build` command from Task 1.  
Observed:
- `corne_left nice_oled` build passes at FLASH `36.46%`, RAM `42.12%`
- explicit `CONFIG_NICE_OLED_NATIVE_PORTRAIT=y` build passes at FLASH `36.36%`, RAM `32.35%`
- `corne_right nice_oled` peripheral-role build passes at FLASH `31.04%`, RAM `35.04%`
- reviewer-found split-role linkage regression was fixed by moving `central_draw_compat.c` out of the central-only CMake branch

- [ ] **Step 6: Commit**

```sh
git add boards/shields/nice_oled/display/render/central_draw_compat.h \
        boards/shields/nice_oled/display/render/central_draw_compat.c \
        boards/shields/nice_oled/display/render/screen_central.c \
        boards/shields/nice_oled/widgets/util.c \
        boards/shields/nice_oled/widgets/util.h \
        boards/shields/nice_oled/widgets/battery.c \
        boards/shields/nice_oled/widgets/output.c \
        boards/shields/nice_oled/widgets/layer.c \
        boards/shields/nice_oled/widgets/profile.c \
        boards/shields/nice_oled/widgets/wpm.c
git commit -m "feat: make native portrait canonical for central rendering"
```

**Task 2 outcome:** Native portrait is now a real, verified central rendering path. The compatibility layer currently preserves the existing portrait-oriented coordinates rather than remapping them, which is acceptable for the current layout model and keeps future remaps centralized.

---

### Task 3: Fix Smart Battery Lifecycle and RAW HID TX Safety

**Files:**
- Modify: `boards/shields/nice_oled/widgets/battery.c`
- Modify: `boards/shields/nice_oled/src/raw_hid/usb_hid.c`
- Modify: `boards/shields/nice_oled/src/raw_hid/hog.c`

- [ ] **Step 1: Normalize smart battery object ownership**

Replace the dual-global-object pattern with a single visible owner or a delete-before-create swap:

```c
static void delete_if_present(lv_obj_t **obj) {
    if (*obj != NULL) {
        lv_obj_del(*obj);
        *obj = NULL;
    }
}
```

- [ ] **Step 2: Make on/off transitions mutually exclusive**

```c
void animation_smart_battery_on(lv_obj_t *canvas) {
    delete_if_present(&art2);
    delete_if_present(&art);
    art = lv_animimg_create(canvas);
    ...
}
```

```c
void animation_smart_battery_off(lv_obj_t *canvas) {
    delete_if_present(&art);
    delete_if_present(&art2);
    art2 = lv_img_create(canvas);
    ...
}
```

- [ ] **Step 3: Clamp RAW HID send length on both transports**

```c
uint8_t copy_len = MIN(len, CONFIG_NICE_OLED_WIDGET_RAW_HID_REPORT_SIZE);
memcpy(report, data, copy_len);
```

- [ ] **Step 4: Run targeted static verification**

Run:

```sh
rg -n "memcpy\\(report, data, len\\)" boards/shields/nice_oled/src/raw_hid
rg -n "delete_if_present|animation_smart_battery_on|animation_smart_battery_off" boards/shields/nice_oled/widgets/battery.c
```

Expected: no remaining unclamped `memcpy(report, data, len)` matches.

- [ ] **Step 5: Re-run the smoke build**

Run the same `west build` command from Task 1.  
Expected: build passes.

- [ ] **Step 6: Commit**

```sh
git add boards/shields/nice_oled/widgets/battery.c \
        boards/shields/nice_oled/src/raw_hid/usb_hid.c \
        boards/shields/nice_oled/src/raw_hid/hog.c
git commit -m "fix: harden battery and raw hid ownership"
```

---

### Task 4: Finish the RAW HID Persistent-Widget Migration

**Files:**
- Modify: `boards/shields/nice_oled/widgets/raw_hid_label.h`
- Modify: `boards/shields/nice_oled/widgets/raw_hid_label.c`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/display/model/raw_hid_state.c`
- Modify: `boards/shields/nice_oled/display/model/raw_hid_state.h`

- [ ] **Step 1: Give RAW HID labels explicit placement and style inputs**

Expand the init API so layout/theme policy can place the labels:

```c
struct raw_hid_label_style {
    lv_coord_t x;
    lv_coord_t y;
    const lv_font_t *font;
    lv_color_t color;
};

lv_obj_t *raw_hid_label_init_time(lv_obj_t *parent,
                                  const struct raw_hid_label_style *style);
```

- [ ] **Step 2: Stop treating label updates as a side channel**

Update the model first, then let one owner decide whether to update the label:

```c
if (nice_oled_raw_hid_apply_time(&widget->central.raw_hid, ev->hour, ev->minute)) {
    raw_hid_label_update_time(ev->hour, ev->minute);
}
```

- [ ] **Step 3: Align each created label explicitly**

```c
lv_obj_set_style_text_font(label, style->font, LV_PART_MAIN);
lv_obj_set_style_text_color(label, style->color, LV_PART_MAIN);
lv_obj_align(label, LV_ALIGN_TOP_LEFT, style->x, style->y);
```

- [ ] **Step 4: Run static verification**

Run:

```sh
rg -n "raw_hid_label_init_|lv_obj_align|lv_obj_set_style_text_" boards/shields/nice_oled/widgets/raw_hid_label.c boards/shields/nice_oled/widgets/screen.c
```

Expected: each label type has explicit placement and style code.

- [ ] **Step 5: Re-run the smoke build**

Run the same `west build` command from Task 1.  
Expected: build passes.

- [ ] **Step 6: Commit**

```sh
git add boards/shields/nice_oled/widgets/raw_hid_label.h \
        boards/shields/nice_oled/widgets/raw_hid_label.c \
        boards/shields/nice_oled/widgets/screen.c \
        boards/shields/nice_oled/display/model/raw_hid_state.h \
        boards/shields/nice_oled/display/model/raw_hid_state.c
git commit -m "refactor: finish raw hid label ownership"
```

---

### Task 5: Contain the Remaining Rotation Cost to Compatibility Paths

**Files:**
- Modify: `boards/shields/nice_oled/widgets/util.c`
- Modify: `boards/shields/nice_oled/widgets/util.h`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

- [ ] **Step 1: Change `rotate_canvas()` to take actual dimensions**

```c
void rotate_canvas(lv_obj_t *canvas, lv_color_t *cbuf, lv_coord_t width, lv_coord_t height);
```

- [ ] **Step 2: Shrink the legacy rotation path to the real active image size**

Use one reusable scratch buffer sized to `width * height`, not `CANVAS_HEIGHT * CANVAS_HEIGHT`.

```c
size_t pixel_count = (size_t)width * (size_t)height;
memcpy(cbuf_tmp, cbuf, pixel_count * sizeof(lv_color_t));
```

- [ ] **Step 3: Pass real dimensions from both compositors**

```c
rotate_canvas(comp->canvas, (lv_color_t *)comp->raw_cbuf, CANVAS_WIDTH, CANVAS_HEIGHT);
```

- [ ] **Step 4: Run static verification**

Run:

```sh
rg -n "CANVAS_HEIGHT \\* CANVAS_HEIGHT|rotate_canvas\\(" boards/shields/nice_oled/widgets/util.c boards/shields/nice_oled/display/render
```

Expected: no square scratch-buffer sizing remains in the active rotation path.

- [ ] **Step 5: Re-run the smoke build and capture memory numbers**

Run the same `west build` command from Task 1.  
Expected: build passes; record FLASH/RAM output into the tracker. The central native portrait path should now avoid `rotate_canvas()` entirely.

- [ ] **Step 6: Commit**

```sh
git add boards/shields/nice_oled/widgets/util.c \
        boards/shields/nice_oled/widgets/util.h \
        boards/shields/nice_oled/display/render/screen_central.c \
        boards/shields/nice_oled/display/render/screen_peripheral_render.c \
        docs/superpowers/tracking/2026-05-25-display-platform-migration-task-tracker.md
git commit -m "perf: remove square rotation scratch overhead"
```

---

### Task 6: Make Dirty Domains Drive Real Redraw Decisions

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_common.h`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/widgets/screen_peripheral.c`

- [ ] **Step 1: Define which dirty domains require canvas redraw versus persistent widget update only**

Add a helper:

```c
static bool needs_canvas_redraw(nice_oled_dirty_mask_t dirty);
```

- [ ] **Step 2: Skip full compositor redraws when only persistent-widget-owned domains changed**

```c
if (!needs_canvas_redraw(widget->compositor.dirty)) {
    return;
}
nice_oled_screen_central_redraw(&widget->compositor);
```

- [ ] **Step 3: Clear dirty flags after redraw**

```c
nice_oled_screen_central_redraw(&widget->compositor);
widget->compositor.dirty = NICE_OLED_DIRTY_NONE;
```

- [ ] **Step 4: Run targeted verification**

Run:

```sh
rg -n "needs_canvas_redraw|dirty = NICE_OLED_DIRTY_NONE|compositor\\.dirty \\|=" boards/shields/nice_oled/display/render boards/shields/nice_oled/widgets/screen*.c
```

Expected: redraw gating and dirty reset logic are present.

- [ ] **Step 5: Re-run the smoke build**

Run the same `west build` command from Task 1.  
Expected: build passes.

- [ ] **Step 6: Commit**

```sh
git add boards/shields/nice_oled/display/render/screen_common.h \
        boards/shields/nice_oled/display/render/screen_central.c \
        boards/shields/nice_oled/display/render/screen_peripheral_render.c \
        boards/shields/nice_oled/widgets/screen.c \
        boards/shields/nice_oled/widgets/screen_peripheral.c
git commit -m "perf: gate redraws by dirty domain"
```

---

### Task 7: Reconcile Documentation After Each Real Milestone

**Files:**
- Modify: `docs/superpowers/tracking/2026-05-25-display-platform-migration-task-tracker.md`
- Modify: `handoffs/2026-05-25-display-platform-migration-handoff.md`

- [ ] **Step 1: Update the tracker only after verifying code and build state**

Never mark a task complete until:
- the code path exists
- static checks match the claim
- the smoke build passes

- [ ] **Step 2: Update the handoff with the new baseline and next blocker**

Add:

```md
## Latest verified state
- Smoke build: PASS
- FLASH: XX.XX%
- RAM: YY.YY%
- Next priority: <task name>
```

- [ ] **Step 3: Run a final doc consistency check**

Run:

```sh
rg -n "100%|12/12 complete|fixed animation object lifecycle leak|reduced rotation scratch buffer size by 57%" docs/superpowers handoffs
```

Expected: no stale success claims remain after the docs are updated.

- [ ] **Step 4: Commit**

```sh
git add docs/superpowers/tracking/2026-05-25-display-platform-migration-task-tracker.md \
        handoffs/2026-05-25-display-platform-migration-handoff.md
git commit -m "docs: reconcile migration status with code"
```

---

## Recommended Execution Order

1. Task 1: Restore Central and Peripheral Render Parity
2. Task 2: Make Native Portrait the Canonical Central Path
3. Task 3: Fix Smart Battery Lifecycle and RAW HID TX Safety
4. Task 4: Finish the RAW HID Persistent-Widget Migration
5. Task 5: Contain the Remaining Rotation Cost to Compatibility Paths
6. Task 6: Make Dirty Domains Drive Real Redraw Decisions
7. Task 7: Reconcile Documentation After Each Real Milestone

## Success Criteria

- Central and peripheral display behavior match pre-regression feature expectations
- Central native portrait is a real, verified code path instead of a dormant Kconfig promise
- RAW HID transport is length-safe on both receive and transmit paths
- Persistent widgets have explicit placement and style ownership
- The old full-buffer square rotation path is no longer on the hot central redraw path
- Dirty domains influence real redraw behavior instead of serving as bookkeeping only
- Tracker and handoff docs describe the real repo state without overstating completion
