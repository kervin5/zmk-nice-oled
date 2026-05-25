# Display Platform Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Migrate `zmk-nice-oled` from its current monolithic display implementation into a modular, migration-safe display platform with incremental updates for hot-path widgets, layout/theme abstraction, and explicit transport/model/render boundaries.

**Architecture:** The plan introduces a layered model where ZMK and RAW HID events update typed display models, compositors decide whether a full redraw is required, high-frequency content moves to persistent LVGL object widgets, and layout/theme policy is separated from widget logic. The migration is phased to preserve current features and visuals while removing the largest redraw and ownership bottlenecks first.

**Tech Stack:** C, Zephyr, ZMK, LVGL, Kconfig, CMake, GitHub Actions

---

### Task 1: Establish Verification Fixtures and CI Safety Net

**Files:**
- Create: `tests/fixtures/zmk-config/west.yml`
- Create: `tests/fixtures/zmk-config/build.yaml`
- Create: `tests/fixtures/zmk-config/config/base.conf`
- Create: `tests/fixtures/zmk-config/config/nice_oled.conf`
- Create: `tests/fixtures/zmk-config/config/nice_epaper.conf`
- Create: `tests/fixtures/zmk-config/config/nice_custom.conf`
- Create: `tests/fixtures/zmk-config/config/raw_hid.conf`
- Create: `tests/fixtures/zmk-config/config/corne.keymap`
- Create: `.github/workflows/build-matrix.yml`
- Modify: `README.md`

- [x] **Step 1: Write the failing CI workflow and fixture references**

```yaml
name: build-matrix

on:
  pull_request:
  push:
    branches: ["main"]

jobs:
  build:
    runs-on: ubuntu-latest
    strategy:
      fail-fast: false
      matrix:
        target:
          - nice_oled
          - nice_epaper
          - nice_custom
          - nice_oled_raw_hid
```

- [x] **Step 2: Run the workflow locally at the config level to verify fixtures are still missing**

Run: `rg -n "tests/fixtures/zmk-config|build-matrix" .github README.md tests`
Expected: no matching fixture files yet, or workflow references missing

- [x] **Step 3: Define minimal fixture configuration files**

```conf
CONFIG_ZMK_DISPLAY=y
CONFIG_ZMK_DISPLAY_STATUS_SCREEN_CUSTOM=y
```

```yaml
---
include:
  - board: nice_nano_v2
    shield: corne_left nice_oled
```

```dts
/ {
    keymap {
        compatible = "zmk,keymap";
    };
};
```

- [x] **Step 4: Wire the GitHub Actions workflow to build the matrix**

```yaml
- name: Build fixture
  working-directory: ${{ runner.temp }}/zmk-workspace
  run: |
    mkdir -p "$RUNNER_TEMP/zmk-workspace"
    git clone --depth 1 --branch v0.3.0 \
      https://github.com/zmkfirmware/zmk.git \
      "$RUNNER_TEMP/zmk-workspace/zmk"
    west init -l zmk/app
    cd zmk
    west update
    west build -s app -d build/${{ matrix.target }} -b nice_nano_v2 -- \
      -DZMK_CONFIG="$GITHUB_WORKSPACE/tests/fixtures/zmk-config/config" \
      -DZMK_EXTRA_MODULES="$GITHUB_WORKSPACE"
```

- [x] **Step 5: Run static verification locally**

Run: `rg -n "build-matrix|tests/fixtures/zmk-config" .github README.md tests`
Expected: workflow and fixture paths present

### Task 2: Extract Display Model Types and Dirty Domains

**Files:**
- Create: `boards/shields/nice_oled/display/model/central_state.h`
- Create: `boards/shields/nice_oled/display/model/central_state.c`
- Create: `boards/shields/nice_oled/display/model/peripheral_state.h`
- Create: `boards/shields/nice_oled/display/model/peripheral_state.c`
- Create: `boards/shields/nice_oled/display/model/raw_hid_state.h`
- Create: `boards/shields/nice_oled/display/model/raw_hid_state.c`
- Create: `boards/shields/nice_oled/display/model/dirty_domains.h`
- Modify: `boards/shields/nice_oled/widgets/util.h`

- [x] **Step 1: Define dirty-domain flags and typed display state**

```c
enum nice_oled_dirty_domain {
    NICE_OLED_DIRTY_NONE = 0,
    NICE_OLED_DIRTY_LAYOUT = BIT(0),
    NICE_OLED_DIRTY_BATTERY = BIT(1),
    NICE_OLED_DIRTY_OUTPUT = BIT(2),
    NICE_OLED_DIRTY_LAYER = BIT(3),
    NICE_OLED_DIRTY_WPM = BIT(4),
    NICE_OLED_DIRTY_MODIFIERS = BIT(5),
    NICE_OLED_DIRTY_RAW_HID = BIT(6),
    NICE_OLED_DIRTY_SLEEP = BIT(7),
};
```

- [x] **Step 2: Run a symbol check to verify the new model layer does not exist yet**

Run: `rg -n "NICE_OLED_DIRTY_|central_state|peripheral_state|raw_hid_state" boards/shields/nice_oled`
Expected: no matches before implementation

- [x] **Step 3: Move display-facing state out of the monolithic `status_state` into explicit model structs**

```c
struct nice_oled_central_state {
    uint8_t battery_percent;
    bool charging;
    uint8_t layer_index;
    const char *layer_label;
    uint8_t wpm_history[10];
    uint8_t modifiers;
    struct nice_oled_raw_hid_state raw_hid;
};
```

- [x] **Step 4: Add change-application helpers that return dirty flags**

```c
uint32_t nice_oled_central_apply_wpm(struct nice_oled_central_state *state, uint8_t wpm);
uint32_t nice_oled_central_apply_modifiers(struct nice_oled_central_state *state, uint8_t mods);
```

- [x] **Step 5: Verify model-layer symbols are discoverable**

Run: `rg -n "nice_oled_central_apply_|NICE_OLED_DIRTY_" boards/shields/nice_oled/display/model`
Expected: model update helpers and dirty flags present

### Task 3: Split RAW HID Transport, Decode, and Model Bridge

**Files:**
- Create: `boards/shields/nice_oled/raw_hid/protocol_types.h`
- Create: `boards/shields/nice_oled/raw_hid/protocol_decode.c`
- Create: `boards/shields/nice_oled/raw_hid/model_bridge.c`
- Move/Modify: `boards/shields/nice_oled/src/raw_hid/hid.c`
- Move/Modify: `boards/shields/nice_oled/src/raw_hid/usb_hid.c`
- Move/Modify: `boards/shields/nice_oled/src/raw_hid/hog.c`
- Create: `tests/unit/raw_hid_protocol/test_raw_hid_protocol.c`

- [ ] **Step 1: Write failing protocol tests for payload length validation**

```c
ZTEST(raw_hid_protocol, test_rejects_short_time_packet) {
    uint8_t packet[] = {0xAA, 0x10};
    assert_false(nice_oled_raw_hid_decode(packet, sizeof(packet), &message));
}
```

- [ ] **Step 2: Run the test discovery command to confirm the test target is not present yet**

Run: `rg -n "test_rejects_short_time_packet|nice_oled_raw_hid_decode" tests boards/shields/nice_oled`
Expected: no decode test implementation yet

- [ ] **Step 3: Define typed protocol messages**

```c
enum nice_oled_raw_hid_kind {
    NICE_OLED_RAW_HID_TIME,
    NICE_OLED_RAW_HID_VOLUME,
    NICE_OLED_RAW_HID_LAYOUT,
    NICE_OLED_RAW_HID_WEATHER,
    NICE_OLED_RAW_HID_SPOTIFY,
};
```

- [ ] **Step 4: Implement decoder with strict length checks and report-size clamps**

```c
bool nice_oled_raw_hid_decode(const uint8_t *data, size_t len,
                              struct nice_oled_raw_hid_message *out);
```

- [ ] **Step 5: Make transport listeners forward decoded messages into the model bridge**

```c
if (nice_oled_raw_hid_decode(event->data, event->length, &message)) {
    nice_oled_raw_hid_apply_message(&central_state, &message);
}
```

- [ ] **Step 6: Run symbol verification**

Run: `rg -n "nice_oled_raw_hid_decode|nice_oled_raw_hid_apply_message" boards/shields/nice_oled tests`
Expected: decoder and bridge symbols present

### Task 4: Introduce Layout and Theme Registries

**Files:**
- Create: `boards/shields/nice_oled/display/layout/layout_registry.h`
- Create: `boards/shields/nice_oled/display/layout/layout_registry.c`
- Create: `boards/shields/nice_oled/display/layout/layout_dense_central.c`
- Create: `boards/shields/nice_oled/display/layout/layout_peripheral_anim.c`
- Create: `boards/shields/nice_oled/display/theme/theme_registry.h`
- Create: `boards/shields/nice_oled/display/theme/theme_registry.c`
- Create: `boards/shields/nice_oled/display/theme/theme_classic_mono.c`
- Create: `boards/shields/nice_oled/display/theme/theme_epaper_readable.c`
- Create: `tests/unit/layout_registry/test_layout_registry.c`

- [ ] **Step 1: Write failing tests for layout lookup and capability gating**

```c
ZTEST(layout_registry, test_dense_central_exposes_modifier_slot) {
    const struct nice_oled_layout *layout = nice_oled_layout_get(NICE_OLED_LAYOUT_DENSE_CENTRAL);
    assert_not_null(layout->slots[NICE_OLED_SLOT_MODIFIERS]);
}
```

- [ ] **Step 2: Verify layout and theme registries do not exist yet**

Run: `rg -n "nice_oled_layout_get|nice_oled_theme_get|NICE_OLED_LAYOUT_" boards/shields/nice_oled tests`
Expected: no registry symbols yet

- [ ] **Step 3: Define layout slot, widget capability, and theme token types**

```c
enum nice_oled_slot_id {
    NICE_OLED_SLOT_OUTPUT,
    NICE_OLED_SLOT_BATTERY,
    NICE_OLED_SLOT_MODIFIERS,
    NICE_OLED_SLOT_RAW_HID_TIME,
};
```

```c
struct nice_oled_theme_tokens {
    lv_color_t fg;
    lv_color_t bg;
    const lv_font_t *font_primary;
    const lv_font_t *font_compact;
};
```

- [ ] **Step 4: Implement one central layout and one ePaper-friendly theme**

```c
const struct nice_oled_layout *nice_oled_layout_get(enum nice_oled_layout_id id);
const struct nice_oled_theme_tokens *nice_oled_theme_get(enum nice_oled_theme_id id);
```

- [ ] **Step 5: Run symbol verification**

Run: `rg -n "nice_oled_layout_get|nice_oled_theme_get" boards/shields/nice_oled/display tests`
Expected: layout and theme registries present

### Task 5: Move Fixed Modifiers to Persistent Object Widgets

**Files:**
- Create: `boards/shields/nice_oled/display/widgets/widget_modifiers.c`
- Create: `boards/shields/nice_oled/display/widgets/widget_modifiers.h`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/widgets/modifiers.c`
- Modify: `boards/shields/nice_oled/CMakeLists.txt`
- Create: `tests/unit/modifiers_widget/test_modifiers_diff.c`

- [ ] **Step 1: Write failing tests for modifier-mask diff behavior**

```c
ZTEST(modifiers_diff, test_no_redraw_when_mask_unchanged) {
    assert_equal(0, nice_oled_modifiers_apply(&widget, 0x02));
    assert_equal(0, nice_oled_modifiers_apply(&widget, 0x02));
}
```

- [ ] **Step 2: Verify the current fixed-modifier path still lives in `screen.c`**

Run: `rg -n "widget_mods_status|draw_mods_status|zmk_keycode_state_changed" boards/shields/nice_oled/widgets/screen.c boards/shields/nice_oled/widgets/modifiers.c`
Expected: fixed modifier logic still split across two files

- [ ] **Step 3: Create a persistent modifier widget with four child objects**

```c
struct nice_oled_modifiers_widget {
    lv_obj_t *container;
    lv_obj_t *icons[4];
    uint8_t last_mask;
};
```

- [ ] **Step 4: Early-return when the mask is unchanged and update only icon state when it changes**

```c
if (widget->last_mask == mods) {
    return NICE_OLED_DIRTY_NONE;
}
widget->last_mask = mods;
```

- [ ] **Step 5: Remove full-screen redraw coupling from the fixed modifier listener path**

```c
/* old path removed from screen.c */
```

- [ ] **Step 6: Run symbol verification**

Run: `rg -n "last_mask|widget_mods_status|draw_mods_status" boards/shields/nice_oled`
Expected: new widget owns modifier updates; old canvas-coupled path removed or quarantined

### Task 6: Move WPM Value and RAW HID Text to Persistent Widgets

**Files:**
- Create: `boards/shields/nice_oled/display/widgets/widget_wpm_value.c`
- Create: `boards/shields/nice_oled/display/widgets/widget_wpm_value.h`
- Create: `boards/shields/nice_oled/display/widgets/widget_raw_hid_text.c`
- Create: `boards/shields/nice_oled/display/widgets/widget_raw_hid_text.h`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/widgets/wpm.c`

- [ ] **Step 1: Write failing tests for value-diff updates**

```c
ZTEST(wpm_value, test_updates_only_when_value_changes) {
    assert_false(nice_oled_wpm_value_apply(&widget, 42));
    assert_true(nice_oled_wpm_value_apply(&widget, 43));
}
```

- [ ] **Step 2: Verify the current WPM and RAW HID paths still force full-screen redraws**

Run: `rg -n "widget_wpm_status|widget_time|widget_volume|widget_layout|widget_weather_status|draw_canvas" boards/shields/nice_oled/widgets/screen.c`
Expected: listeners call back into the monolithic draw path

- [ ] **Step 3: Create persistent label widgets for WPM and RAW HID fields**

```c
struct nice_oled_raw_hid_widget {
    lv_obj_t *time_label;
    lv_obj_t *volume_label;
    lv_obj_t *layout_label;
    lv_obj_t *weather_label;
};
```

- [ ] **Step 4: Make listener updates mutate object text instead of calling the whole-screen compositor**

```c
lv_label_set_text_fmt(widget->time_label, "%02u:%02u", hour, minute);
```

- [ ] **Step 5: Keep graph/speedometer rendering under canvas only when that visual is actually active**

```c
if (layout->show_wpm_canvas_graph) {
    render_wpm_graph(...);
}
```

- [ ] **Step 6: Run symbol verification**

Run: `rg -n "widget_raw_hid_text|widget_wpm_value|lv_label_set_text_fmt" boards/shields/nice_oled`
Expected: text-based hot paths use persistent widgets

### Task 7: Refactor Central and Peripheral Compositors Around the New Boundaries

**Files:**
- Create: `boards/shields/nice_oled/display/render/screen_central.c`
- Create: `boards/shields/nice_oled/display/render/screen_peripheral.c`
- Create: `boards/shields/nice_oled/display/render/screen_common.h`
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/widgets/screen_peripheral.c`
- Modify: `boards/shields/nice_oled/widgets/layer.c`
- Modify: `boards/shields/nice_oled/widgets/util.c`

- [ ] **Step 1: Write the failing grep checks that confirm the monolithic compositor is still central**

Run: `wc -l boards/shields/nice_oled/widgets/screen.c && rg -n "draw_canvas|lv_canvas_set_buffer|rotate_canvas" boards/shields/nice_oled/widgets/screen.c boards/shields/nice_oled/widgets/screen_peripheral.c`
Expected: central and peripheral compositors still anchored in legacy widget files

- [ ] **Step 2: Introduce new compositor entry points and move canvas ownership there**

```c
int nice_oled_screen_central_init(struct nice_oled_screen_central *screen, lv_obj_t *parent);
int nice_oled_screen_peripheral_init(struct nice_oled_screen_peripheral *screen, lv_obj_t *parent);
```

- [ ] **Step 3: Remove full-canvas clearing from individual widget renderers**

```c
/* remove lv_canvas_fill_bg(...) from render_layer path */
```

- [ ] **Step 4: Make compositors render only dirty domains**

```c
if (dirty & NICE_OLED_DIRTY_LAYOUT) {
    nice_oled_redraw_canvas(screen);
}
```

- [ ] **Step 5: Keep the current square-buffer rotation path temporarily, but isolate it behind one compositor helper**

```c
void nice_oled_present_canvas(struct nice_oled_canvas *canvas);
```

- [ ] **Step 6: Run symbol verification**

Run: `rg -n "nice_oled_screen_central_init|nice_oled_present_canvas|lv_canvas_fill_bg" boards/shields/nice_oled`
Expected: compositors own presentation; layer renderer no longer clears whole canvas

### Task 8: Tighten Asset Gating, Remove Orphan Paths, and Refresh Documentation

**Files:**
- Modify: `boards/shields/nice_oled/CMakeLists.txt`
- Modify: `boards/shields/nice_oled/Kconfig.defconfig`
- Modify: `README.md`
- Create: `docs/architecture.md`
- Create: `docs/configuration.md`
- Create: `docs/layouts.md`
- Create: `docs/themes.md`
- Create: `docs/performance.md`
- Modify/Delete/Archive: orphan widget files after dependency review

- [ ] **Step 1: Write a failing inventory command for always-on assets and stale symbols**

Run: `rg -n "luna_images|bongo_cat_images|FIXED_SYMBOL_VERTICAL|NICE_PERI_VIEW_SHOW_SLEEP_ART" boards/shields/nice_oled`
Expected: broad asset inclusion and stale symbols still present

- [ ] **Step 2: Gate large assets under their owning features**

```cmake
target_sources_ifdef(CONFIG_NICE_OLED_WIDGET_WPM_LUNA app PRIVATE assets/luna_images.c)
target_sources_ifdef(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT app PRIVATE assets/bongo_cat_images.c)
```

- [ ] **Step 3: Normalize stale config names and quarantine or delete orphan widget files**

```c
/* remove references to obsolete FIXED_SYMBOL_VERTICAL-style symbols */
```

- [ ] **Step 4: Publish architecture/config/layout/theme/performance docs from the implemented structure**

```markdown
# Layouts

This document describes supported layout profiles and their slot maps.
```

- [ ] **Step 5: Run final static verification**

Run: `rg -n "FIXED_SYMBOL_VERTICAL|NICE_PERI_VIEW_SHOW_SLEEP_ART" boards/shields/nice_oled docs README.md`
Expected: no stale symbol references remain in active paths

### Task 9: Final Verification and Handoff

**Files:**
- Modify: `docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md`

- [ ] **Step 1: Run the verification matrix**

Run: `rg -n "nice_oled|nice_epaper|nice_custom|raw_hid" .github/workflows/build-matrix.yml tests/fixtures/zmk-config`
Expected: all matrix targets and fixture references present

- [ ] **Step 2: Run final architecture sanity checks**

Run: `rg -n "widget_mods_status|draw_mods_status|NICE_PERI_VIEW_SHOW_SLEEP_ART|FIXED_SYMBOL_VERTICAL" boards/shields/nice_oled`
Expected: legacy hot-path and stale-symbol references removed from active code

- [ ] **Step 3: Update the tracker to reflect completed phases and open follow-ups**

```markdown
- [x] Phase 1: Structural containment
- [x] Phase 2: Hot-path migration
- [ ] Phase 5: Add second central theme preset
```

- [ ] **Step 4: Hand off with residual risk notes**

```markdown
Residual risks:
- visual alignment drift on custom layouts
- missing fixture for one rare Kconfig combination
```
