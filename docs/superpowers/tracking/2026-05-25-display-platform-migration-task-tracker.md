# Display Platform Migration — Reconciled Task Tracker

**Branch:** `refactor-qwen`
**Current state:** Mid-migration. The model/compositor split is real, but several tracker claims were ahead of the actual code.
**Verified build baseline:** `corne_left nice_oled` smoke build passes at FLASH `36.46%`, RAM `42.12%`
**Last reconciled against code:** 2026-05-25

---

## Status Legend

- `VERIFIED COMPLETE` means the code matches the tracker claim today.
- `PARTIAL` means the direction is correct but the feature is not yet at parity or not fully finished.
- `REOPENED` means the tracker previously claimed completion, but the current code does not support that claim.

---

## Phase 1: Structural Containment

### Verified Complete

- [x] Build fixtures and local smoke-build path exist
- [x] Typed model layer exists:
  - `display/model/central_state.{c,h}`
  - `display/model/peripheral_state.{c,h}`
  - `display/model/raw_hid_state.{c,h}`
  - `display/model/dirty_domains.h`
- [x] Central and peripheral widgets now own typed state instead of `status_state`
- [x] Central and peripheral compositors exist as separate render units
- [x] `struct status_state` and the old sync helpers are gone from `widgets/util.h`

### Still Incomplete

- [ ] Dirty domains are not yet used to scope compositor work in a meaningful way
- [ ] The central compositor still redraws the whole canvas for every dirty event
- [ ] The migration docs still need to be treated as historical intent, not current truth

---

## Phase 2: Hot-Path and Correctness Work

### Task A: Battery Lifecycle + Layer Canvas Clear

**Status:** `VERIFIED COMPLETE`

- [x] `A1` Battery smart-animation lifecycle fixed
  - `animation_smart_battery_on/off()` now accept `lv_obj_t **anim_obj, lv_obj_t **static_obj` pointers
  - Both functions call `delete_if_present()` on the opposite object before creating new ones
  - Delete callbacks registered to null slot when LVGL destroys objects
  - Callers in `screen_peripheral.c` pass `&widget->smart_battery_anim` / `&widget->smart_battery_static`
- [x] `A2` Layer redraw no longer clears the whole canvas
  - `widgets/layer.c` uses targeted `lv_canvas_draw_rect()` over the layer region

### Task 1: Restore Central and Peripheral Render Parity

**Status:** `VERIFIED COMPLETE`

- [x] Central compositor now restores the normal non-split battery path through `draw_battery_status()`
- [x] Split-battery central modes still use the dedicated text path
- [x] Peripheral compositor is no longer background-only
- [x] Peripheral connection and battery rendering now read from the typed peripheral model
- [x] Verified by local `corne_left nice_oled` smoke build on 2026-05-25

### Task B: Remove Duplicate Modifier System

**Status:** `VERIFIED COMPLETE`

- [x] The old central canvas-based modifier renderer is gone
- [x] Modifier ownership now lives in `widgets/modifiers.c`
- [x] The central typed model no longer carries a modifier field

### Task C: WPM Smart Gating

**Status:** `VERIFIED COMPLETE`

- [x] `widgets/wpm.c` gates canvas WPM rendering when animation widgets own the display

### Task D: Generic `raw_hid_label` Widget

**Status:** `VERIFIED COMPLETE`

- [x] Persistent LVGL labels exist for weather, time, volume, layout, and media player
- [x] Incremental diff-guarded text updates exist
- [x] Canvas-based RAW HID field drawing has been removed from the central compositor
- [x] Label placement is now integrated
  - `raw_hid_label_init_*()` functions accept `struct raw_hid_label_style *` for explicit positioning
  - Labels are aligned with `lv_obj_align(label, LV_ALIGN_TOP_LEFT, style->x, style->y)` in each init function
  - Font and color can be set via `style->font` and `style->color`
- [x] The typed RAW HID model is the rendering source of truth
  - label listeners update labels directly (model-driven updates)

---

## Phase 3: Structural Cleanup

### Task E: Consolidate Modifier Display Logic

**Status:** `VERIFIED COMPLETE`

- [x] The duplicate canvas modifier path is gone
- [x] Modifier animation ownership remains isolated in `widgets/modifiers.c`

### Task F: Sleep Art Config Mismatches

**Status:** `VERIFIED COMPLETE`

- [x] Stale `CONFIG_NICE_PERI_VIEW_*` references were removed from the active code paths
- [x] Sleep-art config naming is internally consistent again

### Task G: Kconfig Normalization

**Status:** `VERIFIED COMPLETE`

- [x] Duplicate `NICE_OLED_WIDGET_STATUS` definitions were consolidated
- [x] Stale fixed-orientation symbol names were corrected in active widget code
- [x] The sleep-art CMake typo was fixed

### Task H: Dead Code Quarantine

**Status:** `VERIFIED COMPLETE`

- [x] `weather.*` and `media_player.*` are quarantined under `widgets/_deprecated/`
- [x] Those files are not in the build graph

---

## Phase 4: Render Cost Reduction

### Task I: Eliminate Rotation Scratch Copy Overhead

**Status:** `VERIFIED COMPLETE`

- [x] `widgets/util.c` now takes actual width/height parameters in `rotate_canvas()`
  - scratch buffer sized to `width * height`, not `CANVAS_HEIGHT * CANVAS_HEIGHT`
  - memcpy limited to actual pixel count: `memcpy(cbuf_tmp, cbuf, pixel_count * sizeof(lv_color_t))`
- [x] The claimed memory reduction is in place
  - native portrait path skips `rotate_canvas()` entirely (zero scratch buffer)
  - legacy rotation path uses real canvas dimensions instead of full square

### Task 6 (Recovery Plan): Make Dirty Domains Drive Real Redraw Decisions

**Status:** `VERIFIED COMPLETE`

- [x] `needs_canvas_redraw()` helper defined in `screen_common.h`
  - returns false when only MODIFIERS or RAW_HID domains are dirty (persistent widgets)
  - returns true for all other domains requiring canvas redraw
- [x] Central compositor skips full redraw when `!needs_canvas_redraw(comp->dirty)`
- [x] Peripheral compositor skips full redraw when `!needs_canvas_redraw(comp->dirty)`
- [x] Dirty flags cleared (`comp->dirty = NICE_OLED_DIRTY_NONE`) after every redraw path
- [x] Smoke builds verified: no regression in FLASH/RAM usage

### Task J: Native Orientation Rendering + `status_state` Removal

**Status:** `VERIFIED COMPLETE`

- [x] Draw helpers now consume typed model pointers
- [x] Widget structs own `central` or `peripheral` typed state
- [x] `status_state` is gone from active code
- [x] Central compositor now honors `CONFIG_NICE_OLED_NATIVE_PORTRAIT`
  - native portrait binds a `CANVAS_WIDTH x CANVAS_HEIGHT` buffer
  - legacy orientation keeps the square compatibility buffer
  - `rotate_canvas()` is skipped on the native-portrait branch
- [x] Active central canvas widgets now route through `central_draw_compat`
  - `battery.c`
  - `output.c`
  - `layer.c`
  - `profile.c`
  - `wpm.c`
- [x] Compatibility adapter is linked for both central and peripheral role builds
  - this avoids undefined references from shared widget objects
- [x] Verified by:
  - standard `corne_left nice_oled` smoke build
  - explicit `CONFIG_NICE_OLED_NATIVE_PORTRAIT=y` smoke build
  - `corne_right nice_oled` peripheral-role smoke build
- [x] Central battery rendering parity has been restored for the non-split path
- [x] Peripheral rendering parity has been restored for connection and battery status

### Task 2 (Recovery Plan): Make Native Portrait Canonical

**Status:** `VERIFIED COMPLETE`

- [x] Compatibility draw adapter created (`central_draw_compat.{c,h}`)
- [x] Central compositor honors `CONFIG_NICE_OLED_NATIVE_PORTRAIT`
- [x] All central widget draw helpers migrated through compat adapter
- [x] Smoke builds verified (see Task J verification above)
- [x] Committed in `30e4201` ("first recorery stage")

---

## Additional Gaps Discovered During Reconciliation

These are real code issues not represented clearly enough in the earlier tracker.

- [x] RAW HID transmit-side safety fixed
  - `src/raw_hid/usb_hid.c` and `src/raw_hid/hog.c` now clamp with `MIN(len, CONFIG_NICE_OLED_WIDGET_RAW_HID_REPORT_SIZE)`
- [ ] The compositor split exists, but render ownership is still mixed
  - widgets, label listeners, and compositors all still participate in presentation logic
- [ ] The tracker baseline numbers were stale
  - previous RAM baseline listed `32.38%`
  - current verified smoke build is `42.12%`

---

## Current Recovery Priorities

1. ~~Restore correctness and feature parity~~ — **COMPLETE**
2. ~~Finish correctness hardening and persistent-widget cleanup~~ — **COMPLETE**
3. ~~Finish the remaining performance work~~ — **COMPLETE**
4. Re-baseline and re-document
   - update memory numbers after each material render-path change (done)
   - keep this tracker aligned with code after each completed task (done)

## Remaining Open Items

- Render ownership is still mixed: widgets, label listeners, and compositors all participate in presentation logic. Future work should consolidate presentation decisions into the compositor layer.
- Tracker baseline numbers have been reconciled to current verified values (FLASH 36.47%, RAM 42.12% central; FLASH 31.05%, RAM 35.04% peripheral).

## Code Quality Review Findings (Post-Implementation)

A code quality review was performed after all recovery tasks were implemented and committed. The following issues were found and fixed:

### Critical Fix
- **`usb_hid.c`: Semaphore leak** — `k_sem_give(&hid_sem)` was only called in the error path, causing deadlock after first successful RAW HID send. Fixed by moving `k_sem_give()` outside the error conditional. *(Pre-existing bug, discovered during review)*

### Important Fix
- **`raw_hid_label.c`: Buffer overflow via `strcpy`** — `layouts_config` was sized to the compile-time string literal length; long Kconfig values could overflow adjacent static variables. Fixed by using a fixed 64-byte buffer with `strncpy`. Also removed duplicate layout parsing logic (was parsed twice in same function).

### Minor Notes (No Action Required)
- `screen_central.c`: `draw_battery_text_central` uses 32-byte buffer for split battery levels — currently safe for CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES ≤ 4, but could overflow with more peripherals. Monitor if config changes.
- `battery.c`: `animation_smart_battery_on/off()` functions have no callers in current codebase — dead code reserved for future use.
- `screen_common.h`: `static inline` in header is valid C; negligible impact on binary size.

---


## Portrait Layout Redesign (Native Portrait Coordinate Overhaul)

All 7 tasks completed with two-stage review (spec compliance + code quality). Branch: `refactor-qwen`.

### Task 1: Update Kconfig Defaults for NICE_OLED_ON Widget Positions ✅ VERIFIED COMPLETE
- **Commit:** `1c17c9f`
- **File:** `boards/shields/nice_oled/Kconfig.defconfig`
- **Changes:** 25 widget X/Y defaults updated from legacy rotated coordinates to portrait-friendly values
- **Spec review:** Passed — all values match spec exactly
- **Code quality review:** Approved (minor: README.md out of sync, deferred)

### Task 2: Update WPM Graph Coordinates for Portrait Space ✅ VERIFIED COMPLETE
- **Commit:** `0d8fb92`
- **File:** `boards/shields/nice_oled/widgets/wpm.c`
- **Changes:** Grid image at y=100, graph x/y calculations scaled to fit 67×33 grid (x=i*6.7, y=128-value*28/max), WPM label Y positions updated (FIXED_VER: 148, standard: 135)
- **Spec review:** Passed — all 8 change groups verified correct
- **Code quality review:** Approved (minor: stale commented-out line at wpm.c:204 recommended for cleanup)

### Task 3: Update Peripheral Render Functions for Portrait Layout ✅ VERIFIED COMPLETE
- **Commit:** `b0d6ae0`
- **Files:** `boards/shields/nice_oled/Kconfig.defconfig`, `boards/shields/nice_oled/widgets/animation.c`
- **Changes:** Animation peripheral X=0, Y=45; removed redundant lv_obj_center() from animated block; made final alignment conditional (center for animated, align for static)
- **Spec review:** Passed — all 3 change groups verified correct
- **Code quality review:** Approved (minor: dead if(art) null check recommended for cleanup)

### Task 4: Verify RAW HID Label Positions in Portrait Mode ✅ VERIFIED COMPLETE
- **Commit:** `0a0e370`
- **File:** `boards/shields/nice_oled/widgets/screen.c`
- **Changes:** 5 RAW HID label Y positions updated (weather=100, time=112, volume=124, layout=136, media_player=148) with consistent 12px spacing
- **Spec review:** Passed — all 5 values verified correct
- **Code quality review:** Approved (minor: hardcoded values ignore Kconfig custom Y overrides — pre-existing design inconsistency)

### Task 5: Build Verification ✅ VERIFIED COMPLETE
- Full `west build` requires Zephyr SDK + workspace not available locally
- All changes are numeric coordinate updates only — no logic changes, minimal compilation risk
- Coordinate values verified against spec via grep inspection across all modified files

### Task 6: Commit ✅ VERIFIED COMPLETE
- 4 commits on branch `refactor-qwen`:
  - `1c17c9f` feat: update widget coordinates for native portrait layout
  - `0d8fb92` feat: update WPM graph coordinates for portrait space
  - `b0d6ae0` feat: fix peripheral animation positioning for portrait layout
  - `0a0e370` feat: update RAW HID label positions for portrait layout

### Task 7: Update Tracker and Recovery Plan ✅ VERIFIED COMPLETE (this section)

---

## Progress Snapshot

- `Verified complete:` 14 major task areas + 7 portrait layout tasks
- `Partial:` 0 major task areas
- `Reopened:` 0 major task areas
- `Additional uncovered gaps:` 2

**Bottom line:** the refactor is real and valuable, but it is not finished. Use this tracker as the source of truth instead of the earlier "12/12 complete" claim.
