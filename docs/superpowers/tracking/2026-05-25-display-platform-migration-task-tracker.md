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

**Status:** `PARTIAL`

- [ ] `A1` Battery smart-animation lifecycle is still not fixed
  - `widgets/battery.c` still keeps separate `art` and `art2` globals
  - `animation_smart_battery_on()` and `animation_smart_battery_off()` only guard against duplicate creation of the same object
  - the opposite object is not deleted or swapped out cleanly
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

**Status:** `PARTIAL`

- [x] Persistent LVGL labels exist for weather, time, volume, layout, and media player
- [x] Incremental diff-guarded text updates exist
- [x] Canvas-based RAW HID field drawing has been removed from the central compositor
- [ ] Label placement is not integrated yet
  - `screen.c` creates the labels, but does not align them
  - `raw_hid_label.c` creates labels, but does not style or position them
- [ ] The typed RAW HID model is no longer the clear rendering source of truth
  - label listeners update labels directly instead of updating model state and letting one owner render

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

**Status:** `REOPENED`

- [ ] `widgets/util.c` still uses:
  - static square scratch buffer sized as `CANVAS_HEIGHT * CANVAS_HEIGHT`
  - full-buffer `memcpy`
  - square transform dimensions
- [ ] The claimed memory reduction has not landed in the current code

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

---

## Additional Gaps Discovered During Reconciliation

These are real code issues not represented clearly enough in the earlier tracker.

- [ ] RAW HID transmit-side safety still needs fixing
  - `src/raw_hid/usb_hid.c` and `src/raw_hid/hog.c` still `memcpy(..., len)` into fixed-size report buffers without clamping
- [ ] RAW HID labels need layout/theme ownership
  - they should be positioned and styled by layout/theme policy, not by ad hoc widget init
- [ ] The compositor split exists, but render ownership is still mixed
  - widgets, label listeners, and compositors all still participate in presentation logic
- [ ] The tracker baseline numbers were stale
  - previous RAM baseline listed `32.38%`
  - current verified smoke build is `42.12%`

---

## Current Recovery Priorities

1. Restore correctness and feature parity
   - enough visual parity to validate the portrait-native path safely

2. Finish correctness hardening and persistent-widget cleanup
   - smart battery animation lifecycle
   - RAW HID transmit clamping
   - raw_hid_label placement, styling, and ownership
   - renderer/widget responsibility cleanup

3. Finish the remaining performance work
   - shrink the remaining legacy rotation compatibility path
   - stop unconditional whole-canvas redraw behavior where persistent widgets already exist
   - make dirty domains drive real redraw decisions

4. Re-baseline and re-document
   - update memory numbers after each material render-path change
   - keep this tracker aligned with code after each completed task

---

## Progress Snapshot

- `Verified complete:` 9 major task areas
- `Partial:` 2 major task areas
- `Reopened:` 1 major task area
- `Additional uncovered gaps:` 4

**Bottom line:** the refactor is real and valuable, but it is not finished. Use this tracker as the source of truth instead of the earlier “12/12 complete” claim.
