# Comprehensive Refactor Plan

## Overview
Execute Phases 2–4 of the display platform migration to address performance audit findings. Conservative execution: smoke build after every task, two-stage review per task (spec compliance → code quality).

**Branch:** `refactor-qwen`  
**Smoke build baseline:** Central FLASH 36.37%, RAM 32.38%; Peripheral FLASH 30.93%, RAM 25.25%

---

## Deep Analysis Summary (Corrected)

### What the audit found vs. what actually exists:

| Audit Finding | Current State | Plan Task |
|---|---|---|
| Fixed modifiers force full redraw on every key press | **TWO systems running in parallel:** `widget_mods_status` listener → `nice_oled_central_apply_modifiers()` → canvas draw (screen.c + screen_central.c) AND `widget_modifiers` listener → persistent LVGL animimg objects (modifiers.c). Both fire on every modifier change. | B + E |
| WPM forces full redraw when animation active | `draw_wpm_status()` always draws to canvas even when Luna/BongoCat animation handles WPM display | C |
| RAW HID fields trigger full redraw | Weather, Spotify listeners call `nice_oled_screen_central_redraw()` on every update | D |
| Smart battery object lifecycle leak | `animation_smart_battery_on/off` in battery.c creates/destroys animimg objects but doesn't guard against repeated calls | A |
| Layer canvas fill_bg clears previously drawn widgets | `draw_layer_status()` fills entire canvas when responsive bongo cat is enabled | A |
| Rotation duplicates framebuffer + transforms every redraw | `rotate_canvas()` uses static scratch buffer of 160x160 for a 68x160 display | I |
| Square-buffer overhead 2.35x visible area | Canvas created as CANVAS_HEIGHT x CANVAS_HEIGHT (square) instead of native orientation | J |
| Stale Kconfig names in sleep_status.c | References `CONFIG_NICE_PERI_VIEW_SHOW_SLEEP_ART_ON_IDLE` / `...SLEEP` — should be `CONFIG_NICE_OLED_*` | F |
| Dead code not wired into build graph | weather.c, media_player.c exist but not compiled | H |

### Critical structural issue discovered:

**`struct status_state` is NOT dead code yet.** It's still the primary interface for ALL draw functions:
- `draw_battery_status(canvas, const struct status_state *state)` — reads `state->battery`, `state->charging` (legacy fields)
- `draw_layer_status(canvas, const struct status_state *state)` — reads `state->central.layer_label`, `state->central.layer_index` (typed model)
- `draw_wpm_status(canvas, const struct status_state *state)` — reads `state->central.wpm[]` (typed model)
- `draw_output_status(canvas, const struct status_state *state)` — reads `state->central.selected_endpoint`, etc. (typed model)
- `draw_profile_status(canvas, const struct status_state *state)` — reads `state->central.active_profile_index` (typed model)

**The typed models exist but draw functions don't use them directly.** Instead they receive `struct status_state *` which is a hybrid: it contains embedded typed models (`state->central.*`, `state->peripheral.*`) PLUS legacy wrapper fields (`state->battery`, `state->charging`).

**Listeners update BOTH paths simultaneously at the same memory address:**
```c
// In screen.c battery listener:
nice_oled_central_apply_battery_state(&widget->state.central, state.level, ...);  // typed model
widget->state.dirty |= dirty;
nice_oled_screen_central_redraw(&widget->compositor);  // passes &widget->state as status_state*
```

Since `&widget->state` IS a `struct status_state`, and it contains the typed models inside it, both paths point to the same memory. The redundant writes are wasted cycles but not incorrect — **except in screen_peripheral.c:68** where `widget->state.charging` is read from the legacy wrapper field instead of going through the typed model path. This means animation_smart_battery_on/off may use stale data since no sync mechanism bridges them.

### What needs to happen (corrected):

1. **Task B/E:** Remove duplicate modifier system — delete `widget_mods_status` listener and `draw_mods_status()` canvas rendering, keep only modifiers.c persistent LVGL objects
2. **Tasks A,C,D:** Fix correctness + add smart gating for hot-path widgets
3. **Task J (corrected):** Migrate ALL draw functions to accept typed models directly (`const struct nice_oled_central_state *`, `const struct nice_oled_peripheral_state *`) instead of `struct status_state *`. Then remove `struct status_state` entirely along with sync functions and legacy wrapper fields
4. **Tasks F,G,H,I:** Structural cleanup, Kconfig fixes, dead code quarantine, rotation optimization

---

## Phase 2: Hot-Path Event Scoping (Tasks A–D)

### Task A: Correctness Fixes — Battery Lifecycle + Layer Canvas Clear
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks A1–A3  
**Files to modify:**
- `boards/shields/nice_oled/widgets/battery.c` (lines 38–60, animation_smart_battery_on/off)
- `boards/shields/nice_oled/widgets/layer.c` (line 23 in draw_layer_status)

**What to do:**
1. **Battery lifecycle fix:** In `animation_smart_battery_on()`, check if `art` already exists before creating it. If it exists, return early (don't create a second animation). Same for `animation_smart_battery_off()` — guard against creating duplicate static image objects. The current code deletes the old object but doesn't check if one already exists at creation time, causing duplicates on repeated calls.
2. **Layer canvas clear fix:** In `draw_layer_status()`, replace `lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER)` with a targeted approach: draw a filled rectangle over just the layer status area using `lv_canvas_draw_rect()` instead of filling the entire canvas. This prevents erasing other widgets that were drawn previously.
3. **Verification:** Run smoke build for central shield. Check that battery animations don't duplicate on repeated updates, and layer status redraw doesn't erase other widgets.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task B: Remove Duplicate Modifier System — Keep Only modifiers.c Persistent LVGL Objects
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks B1–B4  
**Files to modify:**
- `boards/shields/nice_oled/widgets/screen.c` (lines 118–168, widget_mods_status listener)
- `boards/shields/nice_oled/display/render/screen_central.c` (lines 24, 43, 167–379, draw_mods_status + its call site)

**What to do:**
1. **Delete the canvas-based modifier system from screen.c:** Remove lines 118–168 which define `struct mods_status_state`, `set_mods_status()`, `mods_status_update_cb()`, `mods_status_get_state()`, and the `ZMK_DISPLAY_WIDGET_LISTENER`/`ZMK_SUBSCRIPTION` macros. Also remove the `widget_mods_status_init()` call from `zmk_widget_screen_init()` (line 537).
2. **Delete draw_mods_status from screen_central.c:** Remove the forward declaration (line 24), the call in `draw_canvas_central()` (line 43), and the entire `draw_mods_status()` function implementation (lines 167–379 including both CONFIG branches).
3. **Remove nice_oled_central_apply_modifiers() from central_state.c:** Delete the function at lines 112–120 since it's no longer called by any listener.
4. **Keep modifiers.c as the single owner:** The existing `modifiers.c` already handles FIXED_SYMBOL, BONGO_CAT, and LUNA modes via persistent LVGL objects. No changes needed to modifiers.c itself — just remove the competing canvas-based system.
5. **Fix Bongo Cat/Luna memory leak in modifiers.c:** In the BONGO_CAT and LUNA paths (lines 196–300), before creating a new animimg object, destroy the old one if it exists:
   ```c
   // Replace:
   if (!bongo_imgs) { ... }
   
   // With:
   if (bongo_imgs) lv_obj_del(bongo_imgs);
   bongo_imgs = lv_animimg_create(label);
   ```
   Same pattern for `luna_imgs` in the LUNA path. This ensures only ONE animation object exists at a time when switching between modifiers.
6. **Verification:** Run smoke build for central shield. Verify modifier display works correctly via persistent LVGL objects (no canvas redraws), and no duplicate animimg objects are created when switching between modifiers.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task C: WPM Smart Gating — Skip Canvas Draw When Animation Active
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks C1–C3  
**Files to modify:**
- `boards/shields/nice_oled/widgets/wpm.c` (draw_wpm_status function, line 301)

**What to do:**
1. **Add animation state check at top of draw_wpm_status():** When Luna or BongoCat animation is enabled for WPM display, skip the canvas draw entirely and return early. The animation system handles WPM display visually, so drawing to canvas is redundant work.
2. **Implementation approach:** Check the Kconfig options that enable Luna/BongoCat animations:
   ```c
   void draw_wpm_status(lv_obj_t *canvas, const struct status_state *state) {
       // Skip canvas draw when animation handles WPM display
   #if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA) || \
       IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT) || \
       IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
       return;  // Animation widget handles WPM display
   #endif
   
       // ... rest of existing draw logic unchanged ...
   }
   ```
3. **Preserve all existing modes:** When no animation is active, behavior must be identical to current code — text mode shows "WPM: X", graph mode shows the bar, speedometer shows gauge+needle, both read from `state->central.wpm`.
4. **Verification:** Run smoke build for central shield. Verify WPM displays correctly when no animation active (text/graph/speedometer all work), and canvas draw is skipped when Luna/BongoCat IS active.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task D: Generic raw_hid_label Widget — One Parameterized Widget for All RAW HID Fields
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks D1–D4  
**Files to create/modify:**
- **Create:** `boards/shields/nice_oled/widgets/raw_hid_label.c` and `.h` (new file)
- **Modify:** `boards/shields/nice_oled/widgets/screen.c` (remove weather + spotify listeners, replace with generic widget calls)

**What to do:**
1. **Create raw_hid_label widget:** A single parameterized widget that handles all RAW HID field types through a config struct:
   ```c
   // raw_hid_label.h
   #include "../display/model/raw_hid_state.h"
   
   struct raw_hid_label_config {
       enum {
           RAW_HID_FIELD_TIME,
           RAW_HID_FIELD_VOLUME,
           RAW_HID_FIELD_LAYOUT,
           RAW_HID_FIELD_WEATHER_TEMP,
           RAW_HID_FIELD_MEDIA_PLAYER
       } field_id;
       int32_t x;
       int32_t y;
       const char *format;  // e.g., "%02d:%02d", "Vol: %d%%", "%s"
   };
   
   struct raw_hid_label_state {
       lv_obj_t *label;
       struct raw_hid_label_config config;
       int32_t last_value;  // for diff guard (stores encoded value)
   };
   
   int zmk_widget_raw_hid_label_init(struct raw_hid_label_state *state, 
                                     lv_obj_t *parent,
                                     const struct raw_hid_label_config *config);
   void raw_hid_label_update(struct raw_hid_label_state *state, 
                             const struct nice_oled_raw_hid_state *raw_hid);
   ```
2. **Replace screen.c listeners:** Remove `widget_weather_status` and `widget_spotify_status` listeners (lines 181–241). Replace with two `raw_hid_label_state` instances initialized in `zmk_widget_screen_init()` that call `raw_hid_label_update()` when their respective RAW HID events fire.
3. **Incremental updates:** The widget stores the last known value and only calls `lv_label_set_text()` when the new value differs (diff guard built into raw_hid_label.c). This eliminates full canvas redraws for these fields — instead of calling `nice_oled_screen_central_redraw()`, we just update a single LVGL label.
4. **For time/volume/layout:** These are already handled by existing widget listeners in screen.c (lines 542–556). Replace those with raw_hid_label instances too.
5. **Verification:** Run smoke build for central shield. Verify volume, time, layout, weather, and Spotify display correctly with incremental label updates instead of canvas redraws.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

## Phase 3: Structural Cleanup (Tasks E–H)

### Task E: Consolidate Modifier Display Logic (Follow-up to Task B)
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks E1–E3  
**Files to modify:**
- `boards/shields/nice_oled/widgets/modifiers.c` (Bongo Cat and Luna paths, lines 196–300)

**What to do:**
1. **Fix animimg lifecycle in BONGO_CAT path:** Before creating a new animimg for a different modifier state, destroy the existing one:
   ```c
   // In MODIFIERS_USE_BONGO_CAT block:
   if (bongo_imgs) lv_obj_del(bongo_imgs);  // destroy old first
   bongo_imgs = lv_animimg_create(label);    // create new one
   ```
2. **Fix animimg lifecycle in LUNA path:** Same pattern for `luna_imgs`:
   ```c
   // In MODIFIERS_USE_LUNA block:
   if (luna_imgs) lv_obj_del(luna_imgs);  // destroy old first
   luna_imgs = lv_animimg_create(label);   // create new one
   ```
3. **State-diff guard for FIXED_SYMBOL path:** The existing static image objects (`fixed_win`, `fixed_alt`, etc.) already use the singleton pattern correctly (check `if (!fixed_ctl)` before creating). No changes needed here — they only update their source image on state change, which is efficient.
4. **Verification:** Run smoke build for central shield. Verify no duplicate modifier animation objects are created when switching between modifiers rapidly.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task F: Fix Sleep Art Config Mismatches
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks F1–F3  
**Files to modify:**
- `boards/shields/nice_oled/widgets/sleep_status.c` (lines 33, 41)

**What to do:**
1. **Fix stale Kconfig names:** Replace `CONFIG_NICE_PERI_VIEW_SHOW_SLEEP_ART_ON_IDLE` with `CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE` and `CONFIG_NICE_PERI_VIEW_SHOW_SLEEP_ART_ON_SLEEP` with `CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP`. These are the actual Kconfig option names defined in the shield's Kconfig file.
2. **Verification:** Run smoke build for central shield. Verify sleep art displays correctly based on actual Kconfig values (no compile-time warnings about undefined config options).

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task G: Kconfig Normalization
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks G1–G3  
**Files to modify:**
- `boards/shields/nice_oled/Kconfig` (duplicate or conflicting option definitions)

**What to do:**
1. **Identify duplicates/conflicts:** Scan Kconfig files for duplicate option definitions or conflicting defaults that could cause build-time warnings or unexpected behavior.
2. **Consolidate definitions:** Merge duplicate options, resolve conflicts by keeping the most specific/accurate definition. Add comments explaining why certain defaults are set.
3. **Verification:** Run smoke build for central shield. Verify no Kconfig warnings during build and all expected options are available.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task H: Dead Code Quarantine
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks H1–H3  
**Files to modify/move:**
- `boards/shields/nice_oled/widgets/weather.c` (dead code)
- `boards/shields/nice_oled/widgets/media_player.c` (dead code)

**What to do:**
1. **Identify dead code:** Confirm that weather.c and media_player.c are not wired into the active build graph (not in CMakeLists.txt, no Kconfig option enables them).
2. **Quarantine, don't delete:** Move these files to a `quarantine/` subdirectory within widgets (e.g., `widgets/quarantine/weather.c`). Update any includes that reference them. Add a comment at the top of each file explaining why it was quarantined.
3. **Verification:** Run smoke build for central shield. Verify no build errors from missing files and that quarantine directory is excluded from build.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

## Phase 4: Render Cost Reduction (Tasks I–J)

### Task I: Eliminate Rotation Scratch Copy Overhead
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks I1–I3  
**Files to modify:**
- `boards/shields/nice_oled/widgets/util.c` (rotate_canvas function, lines 11–24)
- `boards/shields/nice_oled/widgets/util.h` (rotate_canvas declaration)

**What to do:**
1. **Remove static scratch buffer:** Delete the static scratch buffer in `rotate_canvas()` that causes unnecessary memory copies (`static lv_color_t cbuf_tmp[CANVAS_HEIGHT * CANVAS_HEIGHT]`). Replace with a direct approach: if rotation is needed, create a temporary canvas of the correct size, copy pixels directly without intermediate buffering.
2. **Eliminate redundant transforms:** If the display hardware supports rotation natively (check LVGL display driver config), remove software rotation entirely and let the driver handle it. If not, use LVGL's built-in rotation support instead of custom pixel manipulation.
3. **Verification:** Run smoke build for central shield. Verify rotated content displays correctly without visual artifacts or performance regression.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

### Task J: Native Orientation Rendering — Remove Square-Buffer Overhead + status_state Removal
**Spec:** `docs/superpowers/specs/2026-05-25-display-platform-migration-complete-spec.md` Tasks J1–J3  
**Files to modify:**
- **All draw function headers and implementations:** battery.h/.c, layer.h/.c, wpm.h/.c, output.h/.c, profile.h/.c
- **Compositor render files:** screen_central.c, screen_peripheral_render.c
- **Widget structs:** screen.h, screen_peripheral.h
- **Remove entirely:** `struct status_state` from util.h (lines 19–60), sync functions (lines 63–111)

**What to do:**
1. **Migrate ALL draw function signatures from `struct status_state *` to typed models:**
   - `draw_battery_status(canvas, const struct nice_oled_central_state *state)` — read battery from `state->battery`, charging from `state->charging`
   - `draw_layer_status(canvas, const struct nice_oled_central_state *state)` — already reads `state->central.layer_label` (change to just `state->layer_label`)
   - `draw_wpm_status(canvas, const struct nice_oled_central_state *state)` — already reads `state->central.wpm[]` (change to just `state->wpm[]`)
   - `draw_output_status(canvas, const struct nice_oled_central_state/peripheral_state *state)` — read from typed model directly
   - `draw_profile_status(canvas, const struct nice_oled_central_state *state)` — already reads `state->central.active_profile_index` (change to just `state->active_profile_index`)

2. **Update compositor draw functions:** In screen_central.c and screen_peripheral_render.c, change `draw_canvas_central()` and `draw_canvas_peripheral()` to accept typed model pointers instead of `struct status_state *`. Pass the appropriate typed model directly:
   ```c
   // Before:
   static void draw_canvas_central(lv_obj_t *canvas, const struct status_state *state) {
       draw_battery_status(canvas, state);  // state is status_state*
   }
   
   // After:
   static void draw_canvas_central(lv_obj_t *canvas, const struct nice_oled_central_state *state) {
       draw_battery_status(canvas, state);  // state is now typed model directly
   }
   ```

3. **Update compositor state pointer:** Change `comp->state` in screen_common.h from `const struct status_state *state` to a union or separate pointers for central/peripheral:
   ```c
   struct nice_oled_compositor {
       lv_obj_t *obj;
       lv_color_t *cbuf;
       lv_obj_t *canvas;
       // Remove: const struct status_state *state;
       const void *central_state;  // or separate central/peripheral pointers
       const void *peripheral_state;
       nice_oled_dirty_mask_t dirty;
       bool initialized;
   };
   ```

4. **Remove `struct status_state` entirely:** Delete the struct definition from util.h (lines 19–60), all sync functions (lines 63–111), and `nice_oled_status_state_init()` (line 101). Update screen.c and screen_peripheral.c to use typed model init directly:
   ```c
   // Before:
   nice_oled_status_state_init(&widget->state);
   
   // After:
   nice_oled_central_state_init(&widget->central_state);
   ```

5. **Fix screen_peripheral.c correctness bug:** Line 68 reads `widget->state.charging` from legacy wrapper — change to read from typed model: `widget->peripheral.battery.charging`.

6. **Native orientation:** Configure LVGL display driver with native rotation support instead of software-based square-buffer approach. This eliminates the 2.35x visible area overhead (from ~10KB to ~4.3KB framebuffer).

7. **Verification:** Run smoke build for central shield. Check memory usage improvement (expect ~6KB framebuffer reduction). Verify all display content renders correctly in native orientation with no references to removed struct.

**Smoke build command (central):**
```bash
cd /Users/kervin/Coding/keyboard/zmk-nice-oled && west build -p always -b nice60_proto_central -- -DBOARD_EXTRA_CFLAGS="-DNICE_OLED_DEBUG=1"
```

---

## Execution Order & Dependencies

| Task | Depends On | Phase | Estimated Effort |
|------|-----------|-------|-----------------|
| A    | —         | 2     | Small           |
| B    | —         | 2     | Medium          |
| C    | —         | 2     | Small           |
| D    | —         | 2     | Medium          |
| E    | B         | 3     | Small           |
| F    | —         | 3     | Small           |
| G    | —         | 3     | Small           |
| H    | —         | 3     | Small           |
| I    | —         | 4     | Medium          |
| J    | I, B      | 4     | Large           |

**Recommended order:** A → B → C → D → E → F → G → H → I → J  
(Each task verified with smoke build before proceeding to next)

---

## Verification Strategy

1. **Smoke build after every task:** Use `west build -p always -b nice60_proto_central` for central shield
2. **Memory comparison:** After each task, note FLASH/RAM usage and compare to baseline (Central: 36.37% FLASH, 32.38% RAM)
3. **Visual inspection:** Verify display content renders correctly after each task
4. **Two-stage review per task:** 
   - Stage 1: Spec compliance — does the change match the spec exactly?
   - Stage 2: Code quality — are there style issues, potential bugs, or improvements needed?

---

## Success Criteria

- All 10 tasks (A–J) completed and verified with smoke builds
- No regression in display functionality
- Measurable performance improvement (reduced redraws, lower memory usage)
- Clean code quality (no new warnings from clang-tidy/cppcheck)
- `struct status_state` fully removed — all draw consumers read from typed models directly
- Plan marked complete in tracker
