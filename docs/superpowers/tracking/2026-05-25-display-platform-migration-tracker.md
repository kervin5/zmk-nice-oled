# Display Platform Migration Tracker

Use this file as the execution ledger for the migration. The architecture spec defines the destination. The implementation plan defines the sequence. This tracker records actual progress.

## Status Key

- `[ ]` Not started
- `[~]` In progress
- `[x]` Completed
- `[-]` Deferred

## Current Focus

- `[ ]` Phase 1 planning review

## Phases

### Phase 0: Freeze Behavior

- `[ ]` Document supported feature combinations
- `[ ]` Capture current visual/reference behavior for key shield/layout combinations
- `[ ]` Define migration support matrix

### Phase 1: Structural Containment

- `[ ]` Add build fixtures and CI matrix
- `[ ]` Extract central/peripheral/raw HID model types
- `[ ]` Add dirty-domain flags
- `[ ]` Define widget/layout/theme boundaries in code

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

- `[ ]` `central_state`
- `[ ]` `peripheral_state`
- `[ ]` `raw_hid_state`
- `[ ]` `dirty_domains`

### Renderers

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
