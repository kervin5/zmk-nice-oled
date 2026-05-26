# Post-Mortem: Display Platform Migration Refactor

**Branch:** `refactor-qwen`  
**Date:** 2026-05-25  
**Goal:** Eliminate full-screen redraws on hot-path events, consolidate duplicate modifier architecture, fix correctness issues, and reduce canvas rotation overhead — without feature loss.

---

## Executive Summary

| Metric | Result |
|--------|--------|
| Tasks planned | 10 (A–J) |
| Tasks fully completed | 7/10 (70%) |
| Tasks partially completed | 2/10 (20%) |
| Tasks not started | 1/10 (10%) |

**Overall: Partially achieved.** The refactor successfully eliminated `struct status_state`, consolidated Kconfig, quarantined dead code, and established clean typed model boundaries. However, two critical features were missed: battery lifecycle leak fix (Task A) and native orientation rendering path (Task J part 1). Additionally, the modifiers persistent object pattern (Task B) was only partially applied.

---

## Task-by-Task Audit

### ✅ Task A: Correctness fixes — PARTIALLY COMPLETE
**Spec:** Fix battery animation smart leak + remove `lv_canvas_fill_bg` from layer.c

| Subtask | Status | Notes |
|---------|--------|-------|
| Layer canvas clear removal | ✅ PASS | No `lv_canvas_fill_bg` in layer.c |
| Battery lifecycle fix | ❌ FAIL | `animation_smart_battery_on()` still deletes and recreates on every call. Spec wanted: `if (art != NULL) return;` guard. Current code: `if (art) { lv_obj_del(art); art = NULL; } art = lv_animimg_create(canvas);` — same leak pattern. |

**Impact:** Battery animation still leaks LVGL objects on repeated calls. This is a correctness bug that should have been fixed first as the spec stated "correctness fixes first."

---

### ✅ Task B: Consolidate modifiers into persistent objects — PARTIALLY COMPLETE
**Spec:** Remove duplicate modifier code from screen.c, add persistent objects with diff guards to modifiers.c

| Subtask | Status | Notes |
|---------|--------|-------|
| Remove from screen.c | ✅ PASS | No `set_mods_status` or `zmk_keycode_state_changed` in screen.c |
| Persistent object creation | ⚠️ PARTIAL | Label created once in `zmk_widget_modifiers_init()`, but animation objects inside `set_modifiers_text()` are still recreated on every event without diff guards. Spec wanted: `if (state->mod_state == widget->prev_mod_state) return;` |
| Unified rendering | ✅ PASS | Both symbol and text modes under one codebase |

**Impact:** Modifiers still trigger LVGL object recreation on every key event, but at least the label itself is persistent. The diff guard was not implemented.

---

### ⚠️ Task C: WPM smart gating — COMPLETE (with deviation)
**Spec:** Add compile-time gate in `screen_central.c` to skip canvas draw when animation handles WPM

| Subtask | Status | Notes |
|---------|--------|-------|
| Gate implementation | ✅ PASS | Gate exists at line 301-306 of wpm.c |
| Location per spec | ❌ DEVIATION | Spec said add gate in `draw_canvas_central()` call site. Instead added inside `draw_wpm_status()` function body. Functionally equivalent but deviates from plan. |

**Impact:** Zero functional impact. Gate works correctly — WPM canvas draw is skipped when LUNA or BONGO_CAT animation is active.

---

### ✅ Task D: Generic RAW HID persistent label widget — COMPLETE
**Spec:** Create `raw_hid_label.c/.h`, replace weather/spotify listeners with incremental label updates, remove `draw_hid_status()`

| Subtask | Status | Notes |
|---------|--------|-------|
| raw_hid_label.h/.c created | ✅ PASS | 190 lines implementing persistent LVGL labels for weather, time, volume, layout, media_player |
| ZMK listeners added | ✅ PASS | 5 listeners bridge RAW HID notifications → `raw_hid_label_update_*()` calls |
| draw_hid_status removed | ✅ PASS | No references in screen_central.c (~120 lines deleted) |
| CMakeLists.txt updated | ✅ PASS | raw_hid_label.c compiled under correct `CONFIG_NICE_OLED_WIDGET_RAW_HID` condition |

**Impact:** RAW HID fields now use incremental label updates instead of full canvas redraws. This is the core performance improvement of the refactor.

---

### ✅ Task E: Remove duplicate modifier architecture — COMPLETE
**Spec:** Remove stale Kconfig branches keyed off `...FIXED_SYMBOL`, `...FIXED_VERTICAL` from modifiers.c

| Subtask | Status | Notes |
|---------|--------|-------|
| Stale symbol names fixed | ✅ PASS | No `MODIFIERS_INDICATORS_FIXED_SYMBOL\|MODIFIERS_INDICATORS_FIXED_VERTICAL` in modifiers.c. All references updated to `FIXED_VER`. |

**Impact:** Clean Kconfig symbol usage. No stale references remain.

---

### ✅ Task F: Fix sleep art config/build mismatches — COMPLETE
**Spec:** Audit and fix stale `CONFIG_NICE_PERI_VIEW` symbols in sleep_status.c

| Subtask | Status | Notes |
|---------|--------|-------|
| Stale symbol replacement | ✅ PASS | No `CONFIG_NICE_PERI_VIEW` references in sleep_status.c |

**Impact:** Sleep art config is now consistent with Kconfig definitions.

---

### ✅ Task G: Normalize Kconfig names — COMPLETE
**Spec:** Audit all `IS_ENABLED(CONFIG_...)` calls against Kconfig symbols, fix mismatches

| Subtask | Status | Notes |
|---------|--------|-------|
| Cross-reference check | ✅ PASS | All nice_oled-specific CONFIG symbols are defined in Kconfig.defconfig. (Zephyr/ZMK core configs like `CONFIG_USB_DEVICE_STACK`, `CONFIG_ZMK_SPLIT` are expected to be undefined locally.) |
| Duplicate NICE_OLED_WIDGET_STATUS consolidated | ✅ PASS | 3 definitions → 1 canonical definition |
| CMakeLists.txt typo fixed | ✅ PASS | Line 52: duplicate `CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE` → fixed to check both idle and sleep |

**Impact:** Kconfig is now a single source of truth. No build-time conflicts from duplicate definitions.

---

### ✅ Task H: Quarantine dead widget code — COMPLETE
**Spec:** Move weather.c/h, media_player.c/h to `_deprecated/`, remove from CMakeLists.txt

| Subtask | Status | Notes |
|---------|--------|-------|
| Files quarantined | ✅ PASS | All 4 files in `widgets/_deprecated/` with quarantine comments |
| Removed from build | ✅ PASS | No references in CMakeLists.txt |
| _deprecated preserved | ✅ PASS | Directory kept for potential rollback (per spec "Things To Be Careful Not To Do") |

**Impact:** Dead code removed from build without permanent deletion. Flash savings from not compiling unused files.

---

### ⚠️ Task I: Eliminate rotation scratch copy — PARTIALLY COMPLETE
**Spec:** Remove manual second-buffer allocation for rotation, use LVGL's built-in support

| Subtask | Status | Notes |
|---------|--------|-------|
| Scratch buffer removed from renderers | ✅ PASS | No `lv_mem_alloc`/`lv_mem_free` in screen_central.c or screen_peripheral_render.c |
| rotate_canvas calls removed | ✅ PASS | Not called anywhere in active code |
| Dead function definition remains | ⚠️ PARTIAL | `rotate_canvas()` still declared in util.h and defined in util.c but never called. Should be removed to avoid confusion. |

**Impact:** Rotation scratch allocation eliminated from renderers (the main goal). Minor cleanup needed: remove dead `rotate_canvas` declaration/definition.

---

### ❌ Task J: Native orientation rendering path — PARTIALLY COMPLETE
**Spec:** Add `CONFIG_NICE_OLED_NATIVE_PORTRAIT` Kconfig option, skip rotation on native panels + migrate draw functions to typed models and remove `struct status_state`

| Subtask | Status | Notes |
|---------|--------|-------|
| Draw functions migrated to typed models | ✅ PASS | All 5 draw functions use `const struct nice_oled_central_state *` instead of `struct status_state *` |
| struct status_state removed | ✅ PASS | Deleted from util.h (~90 lines). No references in active code. |
| Compositor updated | ✅ PASS | Single `comp->state` → separate `central_state` + `peripheral_state` pointers |
| Native portrait Kconfig option | ❌ FAIL | `CONFIG_NICE_OLED_NATIVE_PORTRAIT` not added. Rotation still exists as dead code but no compile-time path to skip it. |

**Impact:** The typed model migration is a significant architectural improvement — all draw consumers now read from typed models directly. However, the native orientation rendering path (the performance optimization for portrait panels) was not implemented. This should be addressed in a follow-up task.

---

## What Was Achieved

### Architectural Improvements
1. **Clean model separation** — `nice_oled_central_state` and `nice_oled_peripheral_state` are now the single source of truth for display state
2. **Typed draw function signatures** — All 5 draw functions read from typed models directly, no more hybrid `status_state` struct
3. **Persistent RAW HID labels** — Weather, time, volume, layout, and media_player use incremental LVGL label updates instead of canvas redraws
4. **Kconfig normalization** — Single source of truth, no duplicate definitions, all symbols properly scoped

### Code Quality Improvements
1. **Dead code quarantine** — weather.c/h, media_player.c/h moved to `_deprecated/` with rollback capability
2. **Stale symbol cleanup** — All `FIXED_SYMBOL_VERTICAL`, `NICE_PERI_VIEW` references removed
3. **Compositor boundary clarity** — Central and peripheral compositors have separate state pointers

### Performance Improvements
1. **Eliminated full canvas redraws on hot events** — RAW HID fields update incrementally via persistent labels
2. **Removed rotation scratch allocation from renderers** — No more `lv_mem_alloc`/`memcpy` per redraw in central/peripheral compositors
3. **WPM smart gating** — Canvas draw skipped when animation widget handles WPM display

---

## What Was Missed (Must Fix)

### Critical
1. **Task A: Battery lifecycle leak** — `animation_smart_battery_on()` still deletes and recreates LVGL objects on every call. Should add early-return guard: `if (art != NULL) return;`

### Important
2. **Task J part 1: Native orientation path** — `CONFIG_NICE_OLED_NATIVE_PORTRAIT` Kconfig option not implemented. Rotation still exists as dead code but no compile-time optimization for portrait panels.
3. **Task B part 2: Modifiers diff guard** — `set_modifiers_text()` in modifiers.c recreates animation objects on every event without checking if modifier mask changed. Should add: `if (state->mod_state == widget->prev_mod_state) return;`

### Minor Cleanup
4. **Dead rotate_canvas function** — Declared in util.h, defined in util.c, never called. Remove to avoid confusion.

---

## Recommendations

1. **Fix battery leak immediately** — This is a correctness bug that causes LVGL object leaks on repeated battery state changes
2. **Add native portrait Kconfig option** — Implement the `CONFIG_NICE_OLED_NATIVE_PORTRAIT` path in screen_central.c to skip rotation entirely for panels that support it natively
3. **Add modifiers diff guard** — Prevent unnecessary animation recreation by checking if modifier mask changed before updating LVGL objects
4. **Remove dead rotate_canvas** — Clean up the unused function declaration and definition

---

## Conclusion

The refactor achieved its primary goal of eliminating `struct status_state` and establishing clean typed model boundaries, which is a significant architectural improvement. The RAW HID persistent label widget successfully replaces canvas-based redraws with incremental updates for 5 field types. However, the battery lifecycle leak (Task A) was missed despite being marked as "correctness fixes first," and the native orientation rendering path (Task J part 1) was not implemented at all. These should be addressed before merging to ensure correctness and complete the planned optimization scope.
