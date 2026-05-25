# Display Platform Migration Tracker

Use this file as the execution ledger for the migration. The architecture spec defines the destination. The implementation plan defines the sequence. This tracker records actual progress.

## Status Key

- `[ ]` Not started
- `[~]` In progress
- `[x]` Completed
- `[-]` Deferred

## Current Focus

- `[~]` Phase 1 compositor boundaries (Tasks 4–9)

## Phases

### Phase 0: Freeze Behavior

- `[ ]` Document supported feature combinations
- `[ ]` Capture current visual/reference behavior for key shield/layout combinations
- `[~]` Define migration support matrix

### Phase 1: Structural Containment

- `[x]` Add build fixtures and CI matrix
- `[x]` Extract central/peripheral/raw HID model types
- `[x]` Add dirty-domain flags
- `[x]` Define widget/layout/theme boundaries in code

### Phase 2: Migrate Hottest Paths

- `[ ]` Move fixed modifiers to persistent object widgets
- `[ ]` Move WPM value to persistent object widget
- `[ ]` Move RAW HID text fields to persistent object widgets
- `[ ]` Remove unconditional redraws from those paths

### Phase 3: Normalize Ownership

- `[ ]` Split central compositor ownership out of `widgets/screen.c`
- `[ ]` Split peripheral compositor ownership out of `widgets/screen_peripheral.c`
- `[ ]` Remove duplicate modifier architecture
- `[ ]` Remove stale config branches
- `[ ]` Quarantine or delete orphan widget paths

### Phase 4: Reduce Render Cost

- `[ ]` Restrict full redraw to structural changes
- `[ ]` Isolate rotation/presentation path behind one helper
- `[ ]` Investigate reducing square-buffer overhead
- `[ ]` Fix object-churn and leak-prone widget paths

### Phase 5: Enable Growth

- `[ ]` Add layout registry
- `[ ]` Implement first central layout profile
- `[ ]` Implement first peripheral layout profile
- `[ ]` Add theme registry
- `[ ]` Implement first non-default theme
- `[ ]` Verify widget reuse across layouts/themes

## Subsystem Checklist

### Models

- `[x]` `central_state`
- `[x]` `peripheral_state`
- `[x]` `raw_hid_state`
- `[x]` `dirty_domains`

### Renderers

- `[x]` `screen_central` (compositor + draw orchestration)
- `[x]` `screen_peripheral_render` (compositor + draw orchestration)
- `[ ]` `render_background`
- `[ ]` `render_battery`
- `[ ]` `render_output`
- `[ ]` `render_profile`
- `[ ]` `render_layer`
- `[ ]` `render_wpm_graph`
- `[ ]` `render_wpm_label`
- `[ ]` `render_raw_hid_status`

### Object Widgets

- `[ ]` `widget_modifiers`
- `[ ]` `widget_wpm_value`
- `[ ]` `widget_hid_status`
- `[ ]` `widget_sleep_art`
- `[ ]` `widget_raw_hid_text`

### Layouts

- `[ ]` `dense_central`
- `[ ]` `animation_first_central`
- `[ ]` `minimal_central`
- `[ ]` `peripheral_anim`
- `[ ]` `peripheral_static`
- `[ ]` `custom_geometry`

### Themes

- `[ ]` `classic_mono`
- `[ ]` `minimal_status`
- `[ ]` `playful_anim`
- `[ ]` `epaper_readable`
- `[ ]` `high_contrast`

## Risks to Recheck During Execution

- `[ ]` No feature regressions across shield matrix
- `[ ]` No stale Kconfig symbols left in active paths
- `[ ]` No widget clears the full canvas outside the compositor
- `[ ]` No hot-path widget recreates LVGL objects unnecessarily
- `[ ]` No RAW HID decoder path reads beyond packet length

## Notes

### 2026-05-25

- Architecture-first migration approved.
- Spec, implementation plan, and tracker created.
- Task 1 started with a real fixture strategy instead of documentation-only placeholders.
- The verification scaffold now includes a minimal Corne keymap and a CI path that builds the checked-out module via `ZMK_EXTRA_MODULES`.
- Static verification passed for the new fixture paths and workflow references.
- Local smoke build now passes for `corne_left nice_oled` using a repo-local `uv`-managed `.venv`, `west`, Homebrew `cmake`/`ninja`, and `GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew`.
- Python-side build dependencies are now tracked in `pyproject.toml` and `uv.lock` instead of being installed out-of-band.
- Phase 1 now has typed `central_state`, `peripheral_state`, `raw_hid_state`, and `dirty_domains` modules under `boards/shields/nice_oled/display/model`.
- `widgets/util.h` is now a compatibility wrapper over the extracted models so the old draw paths still compile while event ownership shifts into explicit apply helpers.
- Central and peripheral screen listeners now route updates through change-detection helpers and skip redraws when an event does not change the underlying display model.
- Local smoke build still passes for `corne_left nice_oled` after the model extraction pass.

### 2026-05-25 (Compositor Boundaries)

- Tasks 1–7 completed: shared compositor header, central/peripheral render modules created, CMakeLists.txt updated
- Draw helpers moved from screen.c into screen_central.c as static functions (battery_text, mods_status, hid_status)
- screen.c refactored to use compositor + thin listener wrappers; draw_canvas() removed entirely
- Tasks 8–9 completed: smoke build passes for both central and peripheral
- Memory improved: FLASH 36.38%→36.38%, RAM 42.14%→32.38% (central); peripheral also improved
- Phase 1 tracker item "Define widget/layout/theme boundaries in code" marked complete
