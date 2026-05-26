# Fix Tracker: Post-Mortem Remediation

**Branch:** `refactor-qwen`  
**Date:** 2026-05-25  
**Goal:** Address all "must fix" items from post-mortem audit before merge.

---

## Tasks

### Task A1: Battery Lifecycle Leak Fix
**Spec:** Add early-return guard to `animation_smart_battery_on()` and `animation_smart_battery_off()` so LVGL objects are created once, not recreated on every call.

**Before (leaky):**
```c
void animation_smart_battery_on(lv_obj_t *canvas) {
    if (art) { lv_obj_del(art); art = NULL; }  // deletes existing!
    if (art2) { lv_obj_del(art2); art2 = NULL; }
    art = lv_animimg_create(canvas);  // creates new — leak on every call
    ...
}
```

**After (fixed):**
```c
void animation_smart_battery_on(lv_obj_t *canvas) {
    if (art != NULL) return;  // already created, skip
    art = lv_animimg_create(canvas);
    ...
}
```

Same pattern for `animation_smart_battery_off()`.

**Files:** `widgets/battery.c`  
**Verification:** `rg -n "if \(art\) { lv_obj_del" boards/shields/nice_oled/widgets/battery.c` — expect 0 matches (no delete-before-create)

---

### Task B1: Modifiers Diff Guard
**Spec:** Add state-diff guard in `set_modifiers_text()` so LVGL animation objects are only recreated when the modifier mask actually changed.

**Current:** Every key event triggers recreation of bongo_cat/luna animation objects even if modifiers didn't change.

**Fix:** Track previous modifier state and return early if unchanged:
```c
static uint8_t s_prev_mod_state = 0;

void set_modifiers_text(lv_obj_t *label, struct modifiers_state state) {
    if (state.modifiers == s_prev_mod_state) return;
    s_prev_mod_state = state.modifiers;
    ...
}
```

**Files:** `widgets/modifiers.c`  
**Verification:** Check that `set_modifiers_text()` has a diff guard before animation object creation.

---

### Task J1: Native Portrait Kconfig Option + Remove Dead rotate_canvas
**Spec:** Add `CONFIG_NICE_OLED_NATIVE_PORTRAIT` compile-time option to skip rotation entirely for panels that support it natively. Also remove dead `rotate_canvas()` function (declared in util.h, defined in util.c, never called).

**Fix 1 — Kconfig:** Add option in `Kconfig.defconfig`:
```kconfig
config NICE_OLED_NATIVE_PORTRAIT
    bool "Enable native portrait mode (skip rotation)"
    default n
    help
      Panels that natively support portrait orientation can enable this
      to skip the 90-degree canvas rotation at runtime.
```

**Fix 2 — Remove dead code:** Delete `rotate_canvas()` from util.c and its declaration from util.h. It's never called anywhere in active code.

**Files:** `Kconfig.defconfig`, `widgets/util.c`, `widgets/util.h`  
**Verification:** 
- `rg -n "CONFIG_NICE_OLED_NATIVE_PORTRAIT" boards/shields/nice_oled/Kconfig.defconfig` — expect 1 match
- `rg -n "rotate_canvas" boards/shields/nice_oled/widgets/util.c` — expect 0 matches (removed)

---

## Progress

| Task | Description | Status |
|------|-------------|--------|
| A1 | Battery lifecycle leak fix | ✅ COMPLETE |
| B1 | Modifiers diff guard | ✅ COMPLETE |
| J1 | Native portrait Kconfig + remove dead rotate_canvas | ✅ COMPLETE |

---

## Verification Results

### Task A1: Battery Leak Fix
- `rg -n "lv_obj_del" boards/shields/nice_oled/widgets/battery.c` — 0 matches (no delete-before-create)
- Early-return guards present at lines 39 and 51 of battery.c

### Task B1: Modifiers Diff Guard  
- `s_prev_mods` static variable added to `set_modifiers_text()`
- Diff guard at line 141: `if (mods == s_prev_mods) return;`
- Prevents unnecessary LVGL object recreation on every key event

### Task J1: Native Portrait + Dead Code Removal
- `CONFIG_NICE_OLED_NATIVE_PORTRAIT` added to Kconfig.defconfig at line 83
- `rotate_canvas()` removed from util.c (was never called)
- `rotate_canvas()` declaration removed from util.h
