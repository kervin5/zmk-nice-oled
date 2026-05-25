# Compositor Boundary Cleanup — Code Quality & Full Migration

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **CRITICAL — Skill mapping per role (DO NOT USE GENERIC CODE REVIEWERS):**
> - **Controller:** `superpowers:subagent-driven-development` — dispatches fresh subagents, runs two-stage review (spec compliance → code quality)
> - **Implementer subagents:** `superpowers:test-driven-development` — write failing tests first, implement minimal code, verify
> - **Spec reviewer subagents:** `superpowers:subagent-driven-development/spec-reviewer-prompt.md` — verify implementation matches spec exactly
> - **Code quality reviewer subagents:** `superpowers:subagent-driven-development/code-quality-reviewer-prompt.md` — verify clean, maintainable code
> - **Final merge:** `superpowers:finishing-a-development-branch` — when all tasks complete
>
> **DO NOT** use generic code reviewer agents or tools. Only use the superpowers skills listed above.

**Goal:** Fix code quality issues from Tasks 4–9 and fully migrate remaining draw helpers to typed models so the `status_state` compatibility wrapper becomes dead code.

**Architecture:** The compositor refactor (Tasks 1–7) moved 3 of 8 draw helpers into compositors but left 4 others as external functions that still read legacy fields from `struct status_state`. This leaves the sync bridge serving an inconsistent half-system. This cleanup fully migrates all remaining draw helpers to typed models, removes dead sync calls, and fixes code quality issues (#pragma once on .c files, duplicate includes).

**Tech Stack:** C, Zephyr, LVGL, Kconfig

---

### Task 1: Fix code quality issues in compositor .c files

**Files:**
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

Fix three code quality issues introduced during Tasks 4–9:

1. Remove `#pragma once` from `.c` files (only headers should have this)
2. Remove duplicate `<lvgl.h>` include in screen_central.c line 20 (already at line 5)
3. Add traditional include guard to screen_peripheral_render.c if it doesn't have one

**Verification:** Run `rg -n "#pragma once" boards/shields/nice_oled/display/render/*.c` — expect 0 matches. Run `rg -n "#include <lvgl.h>" boards/shields/nice_oled/display/render/screen_central.c` — expect exactly 1 match.

---

### Task 2: Migrate draw_output_status to typed models

**Files:**
- Modify: `boards/shields/nice_oled/widgets/output.c`
- Modify: `boards/shields/nice_oled/widgets/output.h`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`
- Modify: `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

The current `draw_output_status(canvas, const struct status_state *state)` reads legacy fields from `status_state`:
- `state->selected_endpoint.transport` (line 65) → available as `state->central.selected_endpoint.transport` in typed model
- `state->active_profile_bonded` (line 71) → available as `state->central.active_profile_bonded`
- `state->active_profile_connected` (line 72) → available as `state->central.active_profile_connected`
- `state->connected` (line 83) → available as `state->peripheral.connected`

**Step A: Update output.h declaration**

Change the function signature to accept typed models. Since both central and peripheral paths need this function, create two versions or use a union approach. The simplest is to keep one function that takes `const struct status_state *` but reads from the embedded typed model fields directly (e.g., `state->central.selected_endpoint.transport`). This requires no signature change — just verify the current code already does this.

Actually, looking at output.c:65-83, it already accesses `state->selected_endpoint.transport`, `state->active_profile_bonded`, etc. These are legacy fields on `status_state`. The typed model equivalents live inside `state->central` and `state->peripheral`.

**Step B: Update output.c to read from typed models directly**

Replace all legacy field accesses with typed model paths:
- `state->selected_endpoint.transport` → `state->central.selected_endpoint.transport`
- `state->active_profile_bonded` → `state->central.active_profile_bonded`
- `state->active_profile_connected` → `state->central.active_profile_connected`
- `state->connected` → `state->peripheral.connected`

**Step C: Update callers in screen_central.c and screen_peripheral_render.c**

The call sites already pass `const struct status_state *state`, so no signature change needed. The typed model fields are embedded inside the same struct, so this is a pure field path update.

**Verification:** Run `rg -n "state->selected_endpoint|state->active_profile_bonded|state->active_profile_connected|state->connected" boards/shields/nice_oled/widgets/output.c` — expect only accesses through `state->central.` or `state->peripheral.` (not bare `state->`).

---

### Task 3: Migrate draw_wpm_status to typed models

**Files:**
- Modify: `boards/shields/nice_oled/widgets/wpm.c`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

The current `draw_wpm_status(canvas, const struct status_state *state)` reads `state->wpm[9]`, `state->wpm[i]` (lines 30, 37-38, etc.). These are legacy fields. The typed model equivalent is `state->central.wpm`.

**Step A: Update all wpm.c draw functions to read from typed models**

Replace `state->wpm[...]` with `state->central.wpm[...]` in all static functions within wpm.c that receive `struct status_state *`:
- `draw_gauge` (line 30)
- `draw_needle` (lines 37-38, etc.)
- `draw_graph` (lines 85, 97-101, 112, etc.)
- `draw_label` (line 153)
- All variants for different WPM widget types

**Step B: Update draw_wpm_status signature and body**

Change from `const struct status_state *state` to read `state->central.wpm[...]`.

**Verification:** Run `rg -n "state->wpm\[" boards/shields/nice_oled/widgets/wpm.c` — expect only accesses through `state->central.wpm[` (not bare `state->wpm[`).

---

### Task 4: Migrate draw_layer_status to typed models

**Files:**
- Modify: `boards/shields/nice_oled/widgets/layer.c`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

The current `draw_layer_status(canvas, const struct status_state *state)` reads:
- `state->layer_label == NULL` (line 15) → `state->central.layer_label`
- `state->layer_index` (line 16) → `state->central.layer_index`
- `state->layer_label` (line 18) → `state->central.layer_label`

**Step A: Update layer.c to read from typed models**

Replace all legacy field accesses with `state->central.` paths.

**Verification:** Run `rg -n "state->layer_index|state->layer_label" boards/shields/nice_oled/widgets/layer.c` — expect only accesses through `state->central.` (not bare `state->`).

---

### Task 5: Migrate draw_profile_status to typed models

**Files:**
- Modify: `boards/shields/nice_oled/widgets/profile.c`
- Modify: `boards/shields/nice_oled/display/render/screen_central.c`

The current `draw_profile_status(canvas, const struct status_state *state)` reads legacy fields. Check what it accesses and replace with typed model paths (`state->central.selected_endpoint`, `state->central.active_profile_index`, etc.).

**Step A: Read profile.c to identify all legacy field accesses**
Run: `rg -n "state->[a-z_]+" boards/shields/nice_oled/widgets/profile.c`

**Step B: Replace with typed model paths**
Replace each bare `state->field` with the appropriate `state->central.field`.

**Verification:** Run `rg -n "state->[a-z_]+" boards/shields/nice_oled/widgets/profile.c` — expect only accesses through `state->central.` (not bare `state->`).

---

### Task 6: Remove dead sync calls from screen.c and screen_peripheral.c

**Files:**
- Modify: `boards/shields/nice_oled/widgets/screen.c`
- Modify: `boards/shields/nice_oled/widgets/screen_peripheral.c`

After Tasks 2–5, all draw helpers read directly from typed models. The sync functions (`nice_oled_status_state_sync_from_central`, `nice_oled_status_state_sync_from_peripheral`) are now dead code — they copy typed model data into legacy fields that nobody reads anymore.

**Step A: Remove all `nice_oled_status_state_sync_*` calls from screen.c**

Remove these lines (approximately 8 occurrences):
```c
// REMOVE: nice_oled_status_state_sync_from_central(&widget->state);
```

**Step B: Remove all `nice_oled_status_state_sync_*` calls from screen_peripheral.c**

Remove these lines (2 occurrences at lines 62 and 124):
```c
// REMOVE: nice_oled_status_state_sync_from_peripheral(&widget->state);
```

**Verification:** Run `rg -n "nice_oled_status_state_sync" boards/shields/nice_oled/widgets/screen.c boards/shields/nice_oled/widgets/screen_peripheral.c` — expect 0 matches.

---

### Task 7: Smoke build verification for both central and peripheral

**Files:** None (build only)

Run smoke builds to verify everything compiles after all changes:

```sh
# Central
source /Users/kervin/Coding/keyboard/zmk-nice-oled/.venv/bin/activate
export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
export GNUARMEMB_TOOLCHAIN_PATH=/opt/homebrew
cd /Users/kervin/Coding/keyboard/zmk-nice-oled/tmp/zmk-build-smoke/zmk
west build -p always -s app -d build/nice_oled_central -b nice_nano_v2 -- \
  -DSHIELD="corne_left nice_oled" \
  -DZMK_CONFIG=/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config \
  -DZMK_EXTRA_MODULES=/Users/kervin/Coding/keyboard/zmk-nice-oled

# Peripheral
west build -p always -s app -d build/nice_oled_peripheral -b nice_nano_v2 -- \
  -DSHIELD="corne_right nice_oled" \
  -DZMK_CONFIG=/Users/kervin/Coding/keyboard/zmk-nice-oled/tests/fixtures/zmk-config/config \
  -DZMK_EXTRA_MODULES=/Users/kervin/Coding/keyboard/zmk-nice-oled
```

Expected: both builds pass with same or fewer warnings. No new errors. Memory usage should be similar or slightly improved (dead code removed).

---

### Task 8: Update tracker and handoff docs

**Files:**
- Modify: `docs/superpowers/tracking/2026-05-25-display-platform-migration-tracker.md`
- Modify: `handoffs/2026-05-25-display-platform-migration-handoff.md`

Update tracker — add new section under Phase 1 or create a "Compositor Cleanup" subsection noting that the compatibility wrapper sync calls have been removed and all draw helpers now use typed models directly.

Add handoff note:
```markdown
### 6. Compositor Boundary Cleanup (NEW)

All draw helpers now read from typed models directly — no more legacy field access through `struct status_state`. The `nice_oled_status_state_sync_*` functions are dead code and have been removed from widget listeners. Code quality issues fixed (#pragma once on .c files, duplicate includes).
```

---

## What This Completes

- **Phase 1 tracker item:** "Define widget/layout/theme boundaries in code" — fully complete with no legacy field coupling remaining
- **Handoff recommendation:** Compositor boundaries are now clean — all draw consumers use typed models directly
- **Spec Phase 1:** Structural containment — models extracted, compositors defined, renderers separated, compatibility bridge removed

## What Remains for Next Session

- **Phase 2:** Move fixed modifiers to persistent LVGL widgets (highest impact hot path)
- **Phase 2:** Move WPM value and RAW HID text fields to persistent object widgets
- **Phase 3:** Normalize ownership — remove duplicate modifier architecture, orphan paths
- **Phase 4:** Reduce full-screen redraw cost — dirty flags driving render policy

## Things To Be Careful Not To Do

- Do NOT delete `status_state` struct itself yet — it still exists in headers and may be referenced by other code not covered by this cleanup
- Do NOT change any visual behavior — this is a pure refactor with zero feature changes
- Do NOT remove the old widget files (`widgets/battery.c`, etc.) until all wrappers are verified
