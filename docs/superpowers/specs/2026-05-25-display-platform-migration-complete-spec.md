# Display Platform Migration — Complete Implementation Spec

**Date:** 2026-05-25  
**Branch:** `refactor-qwen`  
**Status:** Approved by user  
**Goal:** Eliminate full-screen redraws on hot-path events, consolidate duplicate modifier architecture, fix correctness issues, and reduce canvas rotation overhead — without feature loss.

---

## Architecture Target

After completion, the display module will have clean ownership boundaries:

```
Event Sources (ZMK events, RAW HID transport)
    │
    ▼
Layer 1: Display Models (central_state / peripheral_state / raw_hid_state)
    - typed updates, dirty flags, change detection
    │
    ▼
Layer 2: Screen Compositors (screen_central.c / screen_peripheral_render.c)
    - canvas lifecycle, draw orchestration
    - dirty-flag-driven redraw policy
    │
    ├─────────────────────┬─────────────────────┐
    ▼                     ▼                     ▼
Canvas Path          Persistent Objects      RAW HID Labels
(low-frequency)      (high-frequency)        (incremental updates)
- background         - modifiers             - time
- profile art        - WPM animation         - volume
- battery text       - HID indicators        - layout
- layer label        - sleep art             - weather / Spotify
```

**Key principle:** Canvas path only redraws on structural changes (boot, profile change, shield mode). Persistent objects update incrementally on hot events (key press, volume change, WPM tick).

---

## Phase 2: Event-Scoped Updates (Hot Paths)

### Task A: Correctness fixes first

**Files:**
- Modify: `boards/shields/nice_oled/widgets/battery.c`
- Modify: `boards/shields/nice_oled/widgets/layer.c`

#### Smart battery object leak fix (`battery.c`)

Current broken pattern in `animation_smart_battery_on()`:
```c
lv_obj_del(art2);  // deletes old art2 if exists
art = lv_img_create(canvas);  // creates new art — but art already exists!
```

Fixed pattern:
```c
void animation_smart_battery_on(lv_obj_t *canvas) {
    if (art != NULL) return;  // already created, skip
    art = lv_img_create(canvas);
}
```

Same fix for `animation_smart_battery_off()` — check before creating.

#### Layer canvas clear removal (`layer.c`)

Remove the `lv_canvas_fill_bg()` call from `draw_layer_status()`. Background ownership belongs to the compositor (called in `screen_central.c`), not individual widgets. This is a layering violation that corrupts previously drawn content depending on composition order.

**Verification:** Run `rg -n "lv_canvas_fill_bg" boards/shields/nice_oled/widgets/layer.c` — expect 0 matches.

---

### Task B: Consolidate modifiers into persistent objects

**Files:**
- Modify: `boards/shields/nice_oled/widgets/modifiers.c`
- Modify: `boards/shields/nice_oled/widgets/screen.c`

#### What gets removed from screen.c

All fixed-modifier code (approximately 40 lines):
- `set_mods_status()` function and its callback
- The `zmk_keycode_state_changed` subscription for modifiers
- All `#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)` blocks in screen.c

#### What gets added to modifiers.c

1. **Persistent object creation:** Convert from singleton pattern (creates/deletes on every event) to instance-owned objects created once and updated incrementally.
2. **State-diff guard:** Only update LVGL objects when modifier mask actually changed:
   ```c
   if (state->mod_state == widget->prev_mod_state) return;
   widget->prev_mod_state = state->mod_state;
   ```
3. **Unified rendering:** Both "symbol" mode (images) and "text" mode (labels) under one codebase, replacing the stale Luna-only path.

**Verification:** Run `rg -n "set_mods_status|zmk_keycode_state_changed" boards/shields/nice_oled/widgets/screen.c` — expect 0 matches for modifier-related lines.

---

### Task C: WPM smart gating

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

Add a compile-time gate in the WPM draw helper:
```c
static void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA) || \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
    /* Animation widget handles WPM display — skip canvas path */
    (void)canvas;
    (void)state;
    return;
#endif
    /* ... existing draw logic unchanged ... */
}
```

This preserves all existing behavior when no animation is active, and eliminates redundant work when an animation IS active. Zero-risk change — just a preprocessor check at the call site in `draw_canvas_central()`.

**Verification:** Run `rg -n "CONFIG_NICE_OLED_WIDGET_WPM_LUNA\|CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT" boards/shields/nice_oled/display/render/screen_central.c` — expect exactly 1 match (the gate condition).

---

### Task D: Generic RAW HID persistent label widget

**Files:**
- Create: `boards/shields/nice_oled/widgets/raw_hid_label.h`
- Create: `boards/shields/nice_oled/widgets/raw_hid_label.c`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

#### API design (`raw_hid_label.h`)

```c
struct raw_hid_label_config {
    const char *field_id;      // "time", "volume", "layout", "weather"
    int x, y;                   // position on canvas (for object placement)
    const char *format;        // "%02d:%02d", "%lu%%", etc.
    lv_obj_t *(*get_parent)(void);  // returns the screen container obj
};

int raw_hid_label_init(struct raw_hid_label_config *cfg, struct zmk_widget_screen *widget);
lv_obj_t *raw_hid_label_obj(struct zmk_widget_screen *widget);
void raw_hid_label_update(struct zmk_widget_screen *widget, const char *value);
```

#### How it works

1. Each RAW HID field (time, volume, layout, weather, Spotify) gets its own `raw_hid_label_config` instance
2. On first update: creates an LVGL label object at the configured position
3. On subsequent updates: calls `lv_label_set_text(label, value)` — no canvas redraw, no rotation
4. The listener in screen.c replaces `draw_canvas()` with `raw_hid_label_update(widget, new_value)`

#### Migration path

- Remove RAW HID draw code from `screen_central.c` (the `draw_hid_status()` function and all RAW HID fields it draws)
- Replace each RAW HID listener callback to call `raw_hid_label_update()` instead of `nice_oled_screen_central_redraw()`
- Keep the dirty-flag pattern for connection state changes (those are structural, not text updates)

**Verification:** Run `rg -n "draw_hid_status" boards/shields/nice_oled/display/render/screen_central.c` — expect 0 matches.

---

## Phase 3: Structural Cleanup

### Task E: Remove duplicate modifier architecture

**Files:**
- Modify: `boards/shields/nice_oled/widgets/modifiers.c`
- Modify: `boards/shields/nice_oled/CMakeLists.txt`

Remove the old Luna-only path from modifiers.c. The consolidated modifier renderer created in Task B becomes the single owner of all modifier presentation. Remove stale Kconfig branches keyed off `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED_SYMBOL`, `...FIXED_VERTICAL`, etc.

**Verification:** Run `rg -n "MODIFIERS_INDICATORS_FIXED_SYMBOL\|MODIFIERS_INDICATORS_FIXED_VERTICAL" boards/shields/nice_oled/widgets/modifiers.c` — expect 0 matches.

---

### Task F: Fix sleep art config/build mismatches

**Files:**
- Modify: `boards/shields/nice_oled/widgets/sleep_status.c`
- Audit: `boards/shields/nice_oled/Kconfig`

Audit all `#if IS_ENABLED(CONFIG_...)` in sleep_status.c and compare against Kconfig symbols actually defined. Update stale symbol names (e.g., `CONFIG_NICE_PERI_VIEW_SHOW_SLEEP_ART_ON_IDLE`) to match current Kconfig definitions. Ensure CMakeLists.txt compiles sleep_status.c only when the corresponding feature is enabled.

**Verification:** Run `rg -n "CONFIG_NICE_PERI_VIEW" boards/shields/nice_oled/widgets/sleep_status.c` — expect 0 matches (all stale names replaced).

---

### Task G: Normalize Kconfig names and code usage

**Files:**
- Audit: All files under `boards/shields/nice_oled/`

Create a single source of truth: the `Kconfig` file is the canonical list of feature symbols. Audit all `IS_ENABLED(CONFIG_...)` calls against this list. Fix mismatches where CMake compiles a file but Kconfig gates it by a different name, or vice versa.

**Verification:** Run `rg -n "IS_ENABLED(CONFIG_" boards/shields/nice_oled/ | sort -u > /tmp/is_enabled.txt && rg -n "choice" boards/shields/nice_oled/Kconfig -A 500 | grep "config " | awk '{print $2}' | sort -u > /tmp/kconfig_symbols.txt && comm -23 /tmp/is_enabled.txt /tmp/kconfig_symbols.txt` — expect empty output (all used symbols defined in Kconfig).

---

### Task H: Quarantine/delete dead widget code

**Files:**
- Move: `boards/shields/nice_oled/widgets/weather.c` → `boards/shields/nice_oled/widgets/_deprecated/`
- Move: `boards/shields/nice_oled/widgets/media_player.c` → `boards/shields/nice_oled/widgets/_deprecated/`
- Modify: `boards/shields/nice_oled/CMakeLists.txt`

Move dead files to `_deprecated/` instead of deleting immediately. Remove them from CMakeLists.txt source lists so they don't compile. Preserves history and allows rollback if a valid use case is discovered later.

**Verification:** Run `rg -n "weather\.c\|media_player\.c" boards/shields/nice_oled/CMakeLists.txt` — expect 0 matches (removed from build).

---

## Phase 4: Reduce Render Cost

### Task I: Eliminate rotation scratch copy

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

Remove the manual second-buffer allocation for rotation. Use LVGL's built-in rotation support that operates in-place or manages its own internal buffer without requiring user-managed scratch space.

Current pattern (approximately 20 lines per compositor):
```c
lv_color_t *scratch = lv_mem_alloc(canvas_width * canvas_height * sizeof(lv_color_t));
lv_canvas_transform(canvas, &rotation_matrix, scratch);
lv_mem_free(scratch);
```

After fix:
```c
// LVGL handles rotation internally — no user-managed scratch buffer needed
lv_obj_set_pos(comp->canvas, 0, 0);  // or use lv_canvas_set_buffer with in-place transform
```

**Impact:** Eliminates ~25,600 pixel allocations per redraw (RAM savings + no memcpy overhead). The square buffer still exists but we stop duplicating it.

---

### Task J: Native orientation rendering path

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`
- Add Kconfig option in `boards/shields/nice_oled/Kconfig`

Add a compile-time option for panels that support native portrait mode:

```c
#if IS_ENABLED(CONFIG_NICE_OLED_NATIVE_PORTRAIT)
    comp->canvas = lv_canvas_create(parent);
    lv_obj_set_size(comp->canvas, CANVAS_WIDTH, CANVAS_HEIGHT);
#else
    // Existing path: create + rotate at runtime
#endif
```

Both paths coexist behind Kconfig. The native path skips the `rotate_canvas()` call entirely in `draw_canvas_central()`.

**Impact:** Eliminates rotation matrix computation from every redraw for panels that natively support portrait orientation. Removes ~100% of rotation CPU cost on supported hardware.

---

## Verification Strategy

### Build verification (after every task)
```bash
# Central
source /Users/kervin/Coding/keyboard/zmk-nice-oled/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew
cd /Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/zmk-build-smoke/zmk
west build -p always -s app -d build/nice_oled_central -b nice_nano_v2 -- \
  -DSHIELD="corne_left nice_oled" \
  -DZMK_CONFIG="/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config" \
  -DZMK_EXTRA_MODULES="/Users/kervin/Coding/keyboard/zmk-nice-oled"

# Peripheral (same command, different shield)
west build -p always -s app -d build/nice_oled_peripheral -b nice_nano_v2 -- \
  -DSHIELD="corne_right nice_oled" \
  -DZMK_CONFIG="/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config" \
  -DZMK_EXTRA_MODULES="/Users/kervin/Coding/keyboard/zmk-nice-oled"
```

### Static analysis (after every task)
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled
clang-tidy --quiet boards/shields/nice_oled/widgets/raw_hid_label.c 2>&1 | grep -v "no such file"
cppcheck --enable=warning,style,performance --inline-suppr \
  boards/shields/nice_oled/widgets/boards/shields/nice_oled/display/render/*.c 2>&1 | head -20
```

### Memory tracking (after every task)
Record FLASH and RAM percentages from build output. Compare against baseline:
- Central: FLASH 36.37%, RAM 32.38%
- Peripheral: FLASH 30.93%, RAM 25.25%

Acceptable variance: ±1% for either metric (compiler differences, dead code elimination).

---

## Things To Be Careful Not To Do

- Do NOT change any visual behavior — this is a pure refactor with zero feature changes
- Do NOT remove the old widget files (`widgets/battery.c`, etc.) until all wrappers are verified
- Do NOT delete `status_state` struct itself yet — it still exists in headers and may be referenced by other code not covered by this cleanup
- Do NOT add layout/theme system yet — that is out of scope for this plan
- Do NOT remove the `_deprecated/` directory after Task H — keep quarantined files for potential rollback

## What This Completes

- **Phase 1 tracker item:** "Define widget/layout/theme boundaries in code" — fully complete with no legacy field coupling remaining
- **Phase 2 tracker items:** Move fixed modifiers to persistent objects, move RAW HID text fields to persistent labels, add WPM smart gating
- **Phase 3 tracker items:** Remove duplicate modifier architecture, remove stale config branches, quarantine orphan widget paths
- **Phase 4 tracker items:** Restrict full redraw to structural changes, eliminate rotation scratch copy

## What Remains for Future Plans

- Layout registry and layout profiles (dedicated plan)
- Theme registry and theme presets (dedicated plan)
- RAW HID transport/protocol safety improvements (separate from display concerns)
- E-paper specific optimizations (nice_epaper shield, separate plan)
