# Display Platform Migration Design

**Project:** `zmk-nice-oled`

**Date:** 2026-05-25

## Goal

Migrate the current `nice_oled` / `nice_epaper` / `nice_custom` display module from a feature-rich but monolithic implementation into a modular display platform that:

- preserves all current user-visible features
- materially reduces avoidable redraw, buffer-copy, and object-churn costs
- supports adding new widgets without turning the compositor into a god file
- supports multiple layouts without duplicating widget logic
- supports a real theme system without hardcoding styling into widget logic

## Non-Goals

- removing current widgets or user-visible features
- redesigning the visuals for aesthetic reasons alone
- switching away from LVGL or ZMK
- rewriting all existing rendering code in one pass
- changing shield IDs, public module identity, or installation flow during this migration

## Current-State Summary

The current module has strong feature coverage, but the architecture has drifted into a mixed model:

- `widgets/screen.c` acts as the central model, renderer, event hub, and feature host
- many event listeners trigger full-screen redraws even for tiny state changes
- the render path stores and rotates a square framebuffer on every redraw
- widgets are split across:
  - canvas draw helpers
  - dedicated LVGL object widgets
  - stale or partially orphaned widget files
- Kconfig, CMake, and docs no longer describe exactly the same system

This produces three practical problems:

1. Performance cost is paid in the hottest paths.
2. Ownership boundaries are unclear.
3. Future extensibility is expensive because layout, theme, and widget logic are entangled.

## Design Principles

### 1. Migration over rewrite

The target architecture should be introduced incrementally while preserving existing features and visuals.

### 2. Separate state from presentation

Display-facing state should be updated independently from how it is drawn.

### 3. Keep full-screen redraws rare

Whole-canvas redraw and rotation should be reserved for structural changes, not high-frequency status updates.

### 4. Make widget, layout, and theme independent concerns

- widgets decide what to render
- layouts decide where widgets go
- themes decide how widgets look

### 5. Prefer declarative capability mapping

Support constraints such as central/peripheral-only widgets, RAW HID requirements, and screen-geometry rules through explicit metadata, not scattered `#ifdef` branches.

## Target Architecture

## Layer 1: Display Models

Introduce explicit model owners:

- `display/model/central_state.h`
- `display/model/central_state.c`
- `display/model/peripheral_state.h`
- `display/model/peripheral_state.c`
- `display/model/raw_hid_state.h`
- `display/model/raw_hid_state.c`

Responsibilities:

- store display-facing state
- accept typed updates from ZMK events and RAW HID protocol decode
- compare previous and next values
- emit dirty domains or change flags

This layer must not depend on LVGL drawing APIs.

## Layer 2: Screen Compositors

Split compositors by role:

- `display/render/screen_central.c`
- `display/render/screen_peripheral.c`
- `display/render/screen_common.h`

Responsibilities:

- create the root canvas and object graph
- load the active layout definition
- load the active theme definition
- decide draw order for low-frequency canvas content
- instantiate persistent object widgets for high-frequency content
- trigger full redraw only when required by dirty domains

This layer must not parse RAW HID packets or contain widget-specific business logic.

## Layer 3: Feature Renderers

Renderers should own low-frequency draw helpers:

- `display/render/render_background.c`
- `display/render/render_battery.c`
- `display/render/render_output.c`
- `display/render/render_profile.c`
- `display/render/render_layer.c`
- `display/render/render_wpm_graph.c`
- `display/render/render_wpm_label.c`
- `display/render/render_raw_hid_status.c`

Renderer contract:

- consume explicit state structs
- avoid global mutable state
- never clear the whole canvas from within a widget renderer
- never subscribe directly to unrelated events
- declare which layout slots and theme tokens they support

## Layer 4: Persistent Object Widgets

High-frequency and stateful visuals should be persistent LVGL object widgets:

- modifiers
- WPM number
- WPM animations
- HID indicators
- RAW HID text fields
- sleep art

Object-widget contract:

- `init(instance, parent, theme, layout_slot)`
- `apply_state(instance, state)`
- `obj(instance)`

These widgets should update existing objects, not create/delete objects on hot paths.

## Layer 5: Layout System

Add an explicit layout layer:

- `display/layout/layout_registry.h`
- `display/layout/layout_registry.c`
- `display/layout/layout_defs/*.c`

Layout responsibilities:

- define widget ordering
- map widgets to slots or regions
- define anchors, offsets, alignment, and visibility rules
- provide fallbacks when widgets are disabled or unsupported
- support multiple layouts without duplicating widget logic

Initial layouts to support:

- dense central status
- animation-first central
- minimal/power-saving central
- peripheral animation
- peripheral static-art
- custom geometry layout

Kconfig coordinate overrides can remain, but only as an override layer, not the primary authoring model.

## Layer 6: Theme System

Add an explicit theme layer:

- `display/theme/theme_registry.h`
- `display/theme/theme_registry.c`
- `display/theme/theme_defs/*.c`

Theme responsibilities:

- define foreground/background colors
- define font roles and sizing
- define icon-family choices
- define spacing and density tokens
- define framing, box, and alignment defaults
- optionally define animation timing presets

Themes must not own transport logic, widget state logic, or event subscriptions.

Initial themes to support:

- classic monochrome dense
- minimal status
- playful animation-first
- ePaper readability theme
- high-contrast accessibility theme

## Layer 7: RAW HID Transport and Protocol

Split transport from decode:

- `raw_hid/transport_usb.c`
- `raw_hid/transport_ble.c`
- `raw_hid/protocol_types.h`
- `raw_hid/protocol_decode.c`
- `raw_hid/model_bridge.c`

Responsibilities:

- transport files move bytes
- decoder validates length and payload structure
- model bridge updates `raw_hid_state`

The display layer should only consume typed state, never raw buffers.

## Ownership Rules

### Widgets own

- local visuals
- local object state
- their own update logic

### Widgets must not own

- whole-screen redraw policy
- shield-wide layout rules
- theme selection
- unrelated event fan-out

### Layouts own

- placement
- visibility
- ordering
- fallback composition

### Themes own

- look and feel
- typography
- spacing
- icon styling

### Compositors own

- canvas/object graph lifecycle
- layout application
- theme application
- draw scheduling

## Render Strategy

Use a hybrid rendering model.

### Canvas content

Canvas composition is appropriate for:

- background fills
- static framing
- low-frequency graphics
- large precomposed layout elements

Expected triggers:

- boot
- layout change
- theme change
- profile change
- endpoint change
- structural screen-mode change

### Object content

Persistent LVGL objects are appropriate for:

- modifiers
- WPM text
- HID indicators
- RAW HID values
- sleep-state overlays
- animation widgets

Expected triggers:

- key and modifier transitions
- WPM updates
- host-side status changes
- sleep/idle changes

## Extensibility Requirements

The redesign is not complete unless it enables all of the following:

### Add a widget without rewriting the compositor

A new widget should be addable through:

- one widget implementation
- one metadata registration entry
- one or more layout placements
- optional theme-token mapping

### Add a layout without duplicating widget logic

The same widget implementations should be reusable across layouts with different placement and visibility rules.

### Add a theme without rewriting widget code

Most visual variation should be expressed through theme tokens rather than per-widget conditionals.

### Express capability rules declaratively

Support constraints such as:

- central-only
- peripheral-only
- RAW HID required
- minimum display height or width
- animation capable

through metadata and layout evaluation.

## Build and Configuration Strategy

## Kconfig

Regroup Kconfig by concern:

1. shield target and geometry
2. central widgets
3. peripheral widgets
4. RAW HID features
5. animation presets
6. layout preset selection
7. theme preset selection
8. performance/memory tuning
9. developer/experimental features

Kconfig should remain the source of compile-time feature selection and default preset choice, but not the main authoring surface for layout policy.

## CMake

The build graph should become explicit:

- stop broad always-on asset inclusion where avoidable
- gate assets under their true owning feature
- compile only live widget implementations
- use one consistent conditional-source style

## Documentation Strategy

Tracked docs should mirror the architecture:

- `docs/architecture.md`
- `docs/configuration.md`
- `docs/raw-hid.md`
- `docs/performance.md`
- `docs/layouts.md`
- `docs/themes.md`
- `docs/nice-custom.md`

The root README should remain the front door, not the complete specification.

## Verification Strategy

Introduce a verification surface before large refactors:

- CI compile matrix for:
  - `nice_oled`
  - `nice_epaper`
  - `nice_custom`
  - one RAW HID-enabled configuration
- unit tests for:
  - RAW HID protocol decode
  - layout registry resolution
  - theme token resolution
  - dirty-domain/state-diff helpers
- targeted visual verification for critical layout/theme combinations

## Migration Strategy

### Phase 0: Freeze behavior

- document supported feature combinations
- document current visuals for key configurations
- define the migration support matrix

### Phase 1: Structural containment

- extract display models from `screen.c`
- add dirty-domain tracking
- define widget/layout/theme boundaries

### Phase 2: Migrate hottest paths

- move fixed modifiers to persistent objects
- move RAW HID labels to persistent objects
- move WPM value to a persistent widget

### Phase 3: Normalize ownership

- remove duplicate modifier architecture
- remove stale branches and orphan widgets
- move placement rules into layouts
- move styling defaults into themes

### Phase 4: Reduce full-screen work

- reserve full redraw for structural changes
- reduce square-buffer dependence where feasible
- investigate native-orientation rendering for supported geometries

### Phase 5: Enable growth

- add additional layouts using the registry
- add additional themes using theme tokens
- verify widget reuse across layouts/themes

## Risks

### Over-fragmentation

Do not split files arbitrarily. Split by responsibility.

### Hidden feature regressions

The current Kconfig surface is broad enough that the migration must use an explicit support matrix.

### Visual drift

Moving to object-based updates can change clipping, alignment, and layering. Visual compatibility must be treated as a real requirement.

## Success Criteria

The migration is successful when all of the following are true:

- no hot-path feature relies on unconditional whole-screen redraw unless structurally necessary
- modifiers, WPM text, and RAW HID text update incrementally
- central and peripheral screens have explicit compositors
- widget, layout, and theme responsibilities are separated
- at least two layouts and two themes can be expressed without duplicating widget logic
- CI can compile the supported shield matrix
- dead widget paths and stale config branches are removed or quarantined

## Recommendation

The project should be repositioned internally as:

> A layered display platform with explicit model, render, widget, layout, theme, and transport boundaries.

That is the architecture that supports:

- predictable performance
- safer refactoring
- easier widget growth
- easier layout experimentation
- theme application without widget rewrites
- lower long-term maintenance cost
