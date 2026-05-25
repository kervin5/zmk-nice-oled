# Display Platform Migration Handoff

Date: `2026-05-25`
Branch: `refactor`
Status: `Phase 1 partially complete`

## Purpose

This handoff is for the next agent continuing the architecture-first migration of
`zmk-nice-oled` from a monolithic display implementation into a modular display
platform with:

- typed display models
- renderer/compositor separation
- persistent LVGL widgets for hot paths
- future layout flexibility
- future theme support
- no feature loss during migration

## Read These First

1. Architecture spec:
   [docs/superpowers/specs/2026-05-25-display-platform-migration-design.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/docs/superpowers/specs/2026-05-25-display-platform-migration-design.md)
2. Implementation plan:
   [docs/superpowers/plans/2026-05-25-display-platform-migration-plan.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/docs/superpowers/plans/2026-05-25-display-platform-migration-plan.md)
3. Tracker:
   [docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md)
4. Deep exploratory audit:
   [tmp/nice-oled-performance-audit-2026-05-25.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/nice-oled-performance-audit-2026-05-25.md)

The `tmp/` audit is intentionally more critical and more detailed than the
tracked docs. It contains the sharpest performance and architecture findings.

## What Was Completed

### 1. Verification Scaffold and Developer Environment

Completed earlier in this session:

- Added fixture config under:
  [tests/fixtures/zmk-config](/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config)
- Added GitHub Actions matrix:
  [.github/workflows/build-matrix.yml](/Users/kervin/Coding/keyboard/zmk-nice-oled/.github/workflows/build-matrix.yml)
- Added repo-local `uv` dependency tracking:
  [pyproject.toml](/Users/kervin/Coding/keyboard/zmk-nice-oled/pyproject.toml)
  [uv.lock](/Users/kervin/Coding/keyboard/zmk-nice-oled/uv.lock)
- Updated local build docs in:
  [README.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/README.md)

Result:

- local smoke build passes for `corne_left nice_oled`
- environment uses repo-local `.venv`
- toolchain verified with Homebrew `arm-none-eabi-gcc`, `cmake`, `ninja`

### 2. Phase 1 Model Extraction

Completed in this session:

- Added dirty-domain flags:
  [boards/shields/nice_oled/display/model/dirty_domains.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/dirty_domains.h)
- Added central display model:
  [boards/shields/nice_oled/display/model/central_state.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/central_state.h)
  [boards/shields/nice_oled/display/model/central_state.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/central_state.c)
- Added peripheral display model:
  [boards/shields/nice_oled/display/model/peripheral_state.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/peripheral_state.h)
  [boards/shields/nice_oled/display/model/peripheral_state.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/peripheral_state.c)
- Added RAW HID display model:
  [boards/shields/nice_oled/display/model/raw_hid_state.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/raw_hid_state.h)
  [boards/shields/nice_oled/display/model/raw_hid_state.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model/raw_hid_state.c)
- Wired model sources into:
  [boards/shields/nice_oled/CMakeLists.txt](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/CMakeLists.txt)

### 3. Compatibility Bridge

[boards/shields/nice_oled/widgets/util.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/util.h)
now acts as a temporary compatibility layer.

Important detail:

- `struct status_state` still exists
- it now embeds either `nice_oled_central_state` or `nice_oled_peripheral_state`
- it still exposes the legacy flattened fields used by old draw code
- helper sync functions keep legacy fields mirrored from the typed models

This was deliberate to keep the build green while changing ownership one step at
a time.

### 4. Event Path Migration

Existing listeners in:

- [boards/shields/nice_oled/widgets/screen.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/screen.c)
- [boards/shields/nice_oled/widgets/screen_peripheral.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/screen_peripheral.c)

now route state updates through typed apply helpers first.

This includes:

- battery
- split battery
- layer
- output/profile
- modifiers
- peripheral connection
- RAW HID connection/time/volume/layout/weather/media player

Current benefit:

- redraws are skipped when incoming events do not actually change display state
- this already removes some no-op redraw churn
- visuals were intentionally preserved

## What Is Still Not Fixed

These are the most important remaining issues.

### 1. `screen.c` Is Still a God File

Even after model extraction, central ownership is still concentrated in:

- [boards/shields/nice_oled/widgets/screen.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/screen.c)

It still owns:

- event listeners
- composite draw ordering
- RAW HID drawing
- fixed modifier drawing
- WPM redraw path
- initialization of several child widgets

The model layer reduced coupling, but the compositor boundary is still not
clean.

### 2. Dirty Flags Exist but Are Not Yet Driving Render Policy

`dirty` masks are now stored in `status_state`, but they are not yet used to:

- choose incremental redraw vs full redraw
- isolate redraw domains
- drive a renderer registry or widget registry

Right now dirty flags mainly serve as state-change detection and groundwork for
later rendering changes.

### 3. Full-Canvas Redraw Strategy Still Exists

The biggest architectural performance problem is still present:

- `draw_canvas(...)` still repaints the full composition
- `rotate_canvas(...)` still runs on the full canvas
- hot content still lives inside the monolithic canvas path

No-op redraws were reduced, but real updates still trigger the same heavy
presentation path.

### 4. Fixed Modifiers Are Still Architecturally Wrong

Modifiers are still drawn inside the main canvas path and still subscribed via:

- `zmk_keycode_state_changed`

This means:

- every actual modifier change still causes a full redraw
- the feature still belongs to the wrong rendering layer
- the duplicate modifier architecture still exists in the repo

This should likely be the next hot-path refactor.

### 5. WPM Is Still Doing Too Much Work

WPM still pushes through the central canvas redraw path in addition to any
animation widgets that also react to WPM. The current extraction intentionally
did not “optimize” WPM beyond moving its state ownership into the model, because
the right fix is structural:

- persistent object widget for numeric WPM
- eventually separate graph/value/animation concerns

### 6. RAW HID Transport/Decode Is Still Unsafe

The architectural and safety issue found earlier is still open:

- RAW HID packet validation is still not split into protocol decoding
- transport layers still feed data straight into event/update flows
- packet length checking and report clamping still need a proper decoder layer

This is planned as Task 3, not done yet.

### 7. `util.h` Is Transitional and Should Not Become Permanent

The compatibility wrapper is useful now, but it duplicates the same state in:

- embedded typed model structs
- flattened legacy fields

That duplication is acceptable only as a migration device. The next agent should
avoid building more permanent architecture on top of the flattened fields.

## Verified Build State

The latest verified local smoke build command was:

```sh
source /Users/kervin/Coding/keyboard/zmk-nice-oled/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew
cd /Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/zmk-build-smoke/zmk
west build -p always -s app -d build/nice_oled -b nice_nano_v2 -- \
  -DSHIELD="corne_left nice_oled" \
  -DZMK_CONFIG=/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config \
  -DZMK_EXTRA_MODULES=/Users/kervin/Coding/keyboard/zmk-nice-oled
```

Result:

- build passed
- `zmk.elf` and `zmk.uf2` generated

Observed non-blocking warnings:

- deprecated `NRF_STORE_REBOOT_TYPE_GPREGRET`
- linker warning about RWX segment

## Current Changed Files

At the time of handoff, the main uncommitted work is in:

- [boards/shields/nice_oled/CMakeLists.txt](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/CMakeLists.txt)
- [boards/shields/nice_oled/widgets/screen.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/screen.c)
- [boards/shields/nice_oled/widgets/screen_peripheral.c](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/screen_peripheral.c)
- [boards/shields/nice_oled/widgets/util.h](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/widgets/util.h)
- [boards/shields/nice_oled/display/model](/Users/kervin/Coding/keyboard/zmk-nice-oled/boards/shields/nice_oled/display/model)
- [docs/superpowers/plans/2026-05-25-display-platform-migration-plan.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/docs/superpowers/plans/2026-05-25-display-platform-migration-plan.md)
- [docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md](/Users/kervin/Coding/keyboard/zmk-nice-oled/docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md)

There is no commit for this work yet.

## Recommended Next Step

The strongest next move is:

### Finish Phase 1: Define Widget and Compositor Boundaries

Do this before trying to “optimize” more hot paths blindly.

Recommended order:

1. Introduce explicit central and peripheral compositor modules.
2. Move pure draw helpers toward renderer-style ownership.
3. Keep `status_state` compatibility only long enough to cross that boundary.
4. Once compositor seams exist, start migrating the hottest path:
   fixed modifiers.

## Recommended Refactor After That

After compositor boundaries are in place, attack hot paths in this order:

1. Move fixed modifiers to a persistent LVGL widget.
2. Move numeric WPM to a persistent LVGL widget.
3. Move RAW HID text fields to persistent LVGL labels.
4. Restrict full canvas redraw to structural domains only.
5. Split RAW HID transport/decode/model bridge properly.

## Things The Next Agent Should Be Careful Not To Do

- Do not delete `status_state` yet without first replacing all draw consumers.
- Do not treat dirty flags as “done”; they are groundwork, not the final render policy.
- Do not claim WPM or modifiers are optimized yet.
- Do not merge more feature logic into `screen.c`; push new ownership outward.
- Do not ignore the future layout/theme direction when defining renderer boundaries.

## Short Executive Summary

The migration is off to a solid start:

- build and tooling are reproducible
- model ownership exists
- no-op redraws are reduced
- the firmware still builds

But the real architectural win is still ahead:

- compositor separation
- persistent hot-path widgets
- proper render policy
- layout/theme registries

This handoff should let the next agent continue from a verified, migration-safe
checkpoint instead of redoing discovery.
