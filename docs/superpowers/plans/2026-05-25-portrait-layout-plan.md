# Implementation Plan: Portrait Layout Redesign

## Overview

Redesign all widget coordinates for direct 68×160 portrait rendering on `CONFIG_NICE_OLED_NATIVE_PORTRAIT=y`. The compat adapter stays as identity mapping. All changes are coordinate updates — no new abstractions or complex logic.

**Branch:** `refactor-qwen`
**Base SHA:** `c353a08` (latest commit before this work)

## Task 1: Update Kconfig Defaults for NICE_OLED_ON Widget Positions

**File:** `boards/shields/nice_oled/Kconfig.defconfig`

Update the following defaults under `NICE_OLED_ON`:

| Config | Old Value | New Value | Reason |
|--------|-----------|-----------|--------|
| `BATTERY_CUSTOM_X` | 0 | 3 | Center battery text in portrait |
| `BATTERY_CUSTOM_Y` | 50 | 5 | Top of screen, leave room for nothing above |
| `OUTPUT_USB_CUSTOM_X` | 0 | 36 | Right side of display (BT icon at x=4) |
| `OUTPUT_USB_CUSTOM_Y` | 34 | 32 | Align with BT icon vertically |
| `LAYER_CUSTOM_X` | 0 | 0 | Left-aligned, no change needed |
| `LAYER_CUSTOM_Y` | 146 | 60 | Move up to make room for WPM below |
| `PROFILE_CUSTOM_X` | 0 | 0 | Left-aligned, no change needed |
| `PROFILE_CUSTOM_Y` | 137 | 80 | Move up; profile indicator at y=80 |
| `PROFILE_TEXT_CUSTOM_X` | 25 | 25 | No change — aligns with dots |
| `PROFILE_TEXT_CUSTOM_Y` | 32 | 80 | Align with profile indicator (was 32) |
| `WPM_GAUGE_CUSTOM_X` | 0 | 17 | Center gauge (68-33)/2 ≈ 17 |
| `WPM_GAUGE_CUSTOM_Y` | 70 | 100 | Below profile area |
| `WPM_NEEDLE_CENTER_CUSTOM_X` | 12 | 34 | Center of gauge (17+33/2) |
| `WPM_NEEDLE_CENTER_CUSTOM_Y` | 90 | 105 | Center of gauge vertically |
| `LUNA_CUSTOM_X` | 65 | 22 | Center Luna (68-24)/2 ≈ 22 |
| `LUNA_CUSTOM_Y` | 0 | 100 | Below profile area |
| `BONGO_CAT_CUSTOM_X` | 64 | 21 | Center Bongo Cat (68-26)/2 ≈ 21 |
| `BONGO_CAT_CUSTOM_Y` | -9 | 95 | Below profile area, centered vertically in remaining space |

Also update the following for NICE_OLED_ON:
- `HID_INDICATORS_CUSTOM_X`: 36 → 0 (modifiers at left)
- `HID_INDICATORS_CUSTOM_Y`: 0 → 148 (bottom of screen)
- `RAW_HID_WEATHER_CUSTOM_Y`: 62 → 100
- `RAW_HID_TIME_CUSTOM_Y`: 74 → 112
- `RAW_HID_VOLUME_CUSTOM_Y`: 98 → 124
- `RAW_HID_LAYOUT_CUSTOM_Y`: 86 → 136
- `RAW_HID_MEDIA_PLAYER_CUSTOM_Y`: 110 → 148

## Task 2: Update WPM Graph Coordinates for Portrait Space

**File:** `boards/shields/nice_oled/widgets/wpm.c`

### Grid image position
Change from legacy rotated coordinates to portrait:
```c
// Line ~23 (non-LUNA/BONGO_CAT path) and line ~180 (LUNA/BONGO_CAT path)
nice_oled_central_draw_img_compat(canvas, 0, 100, &grid, &img_dsc);
// Was: canvas(0, 65) or canvas(-1, 95) — both off-screen in portrait
```

### Graph line coordinates
The grid image is 67×33. The graph should fill the area from y=100 to y=132 (grid height). Update `draw_graph()` to use this coordinate space:

```c
// Fixed range path (line ~43-44):
points[i].x = i * 6.7;           // 0..60 across grid width (was 0 + i*7.4)
points[i].y = 128 - (value * 28 / max);  // y=128 down to ~100 (was 97 - value*32/max)

// Non-fixed range path (line ~65-66):
points[i].x = i * 6.7;
points[i].y = 128 - (state->wpm[i] - min) * 28 / range;
```

### Alternative graph path (LUNA/BONGO_CAT disabled, RAW_HID enabled)
Update line ~202-203:
```c
points[i].x = i * 6.7;           // was -36 + i*7.4
points[i].y = 128 - (value * 28 / max);  // was 127 - value*32/max
```

### WPM number label position
Update `DRAW_LABEL_WMP_Y` from 103 to 135:
```c
#define DRAW_LABEL_WMP_Y 135  // Below graph area
```

For FIXED_VER mode, update `DRAW_LABEL_WMP_Y` from 110 to 148.

### Speedometer gauge position
The gauge image is 33×10. Center it horizontally:
- `WPM_GAUGE_CUSTOM_X = 17` (center of 68px display minus half gauge width)
- `WPM_GAUGE_CUSTOM_Y = 100`

Needle center should be at the center of the gauge image:
- `WPM_NEEDLE_CENTER_CUSTOM_X = 34` (17 + 33/2)
- `WPM_NEEDLE_CENTER_CUSTOM_Y = 105` (100 + 10/2)

## Task 3: Update Peripheral Render Functions for Portrait Layout

**File:** `boards/shields/nice_oled/display/render/screen_peripheral_render.c`

### Battery drawing position
Update `draw_canvas_peripheral()` to call battery and connection functions with portrait coordinates. The Kconfig defaults handle most positioning, but verify the draw function doesn't override them:
- Battery text at `(BATTERY_CUSTOM_X, BATTERY_CUSTOM_Y)` → `(3, 5)` via new Kconfig default
- BT icon at `(OUTPUT_BT_CUSTOM_X, OUTPUT_BT_CUSTOM_Y)` → `(4, 32)` (unchanged)

### Animation positioning for peripheral
Peripheral animations (crystal, cat, spaceman, pokemon, head) are all 69×68. They should be centered horizontally and positioned above the battery/connection info:
- Horizontal center: `(68 - 69) / 2 ≈ 0` (essentially full-width, slight overflow clipped)
- Vertical position: y=45 (leaves room for battery at y=115 and connection at y=140)

The animation is already created via `lv_animimg_create()` or `lv_img_create()` in `screen_peripheral.c` with alignment set by `CONFIG_NICE_OLED_WIDGET_ANIMATION_PERIPHERAL_CUSTOM_X/Y`. Update these Kconfig defaults:
- `ANIMATION_PERIPHERAL_CUSTOM_X`: 18 → 0 (center horizontally)
- `ANIMATION_PERIPHERAL_CUSTOM_Y`: -18 → 45 (position above battery info)

## Task 4: Verify RAW HID Label Positions in Portrait Mode

**File:** `boards/shields/nice_oled/widgets/screen.c`

The RAW HID labels were already given explicit style ownership in a previous commit. Update the Y positions to stack below the profile area and above the bottom edge:
- Weather: y=100 (top of WPM graph area)
- Time: y=112
- Volume: y=124
- Layout: y=136
- Media Player: y=148

These positions should be set in the `raw_hid_label_style` structs passed to each init function.

## Task 5: Build Verification

Run smoke builds for both roles and capture memory numbers:

```bash
# Central build
west build -p always -s app -d build/nice_oled_portrait \
    -b nice_nano_v2 -- \
    -DSHIELD="corne_left nice_oled" \
    -DZMK_CONFIG=/path/to/zmk-config/config \
    -DZMK_EXTRA_MODULES=/path/to/zmk-nice-oled

# Peripheral build  
west build -p always -s app -d build/nice_oled_portrait_peri \
    -b nice_nano_v2 -- \
    -DSHIELD="corne_right nice_oled" \
    -DZMK_CONFIG=/path/to/zmk-config/config \
    -DZMK_EXTRA_MODULES=/path/to/zmk-nice-oled
```

Expected: both builds pass with no warnings from touched files. FLASH/RAM within 5% of baseline (FLASH ≤38%, RAM ≤44%).

## Task 6: Commit

Single commit with all changes:
```bash
git add boards/shields/nice_oled/Kconfig.defconfig \
        boards/shields/nice_oled/widgets/wpm.c \
        boards/shields/nice_oled/display/render/screen_peripheral_render.c \
        boards/shields/nice_oled/widgets/screen.c \
        docs/superpowers/specs/2026-05-25-portrait-layout-design.md \
        docs/superpowers/plans/2026-05-25-portrait-layout-plan.md \
        docs/superpowers/tracking/2026-05-25-display-platform-migration-task-tracker.md

git commit -m "feat: redesign all widget coordinates for native portrait layout"
```

## Task 7: Update Tracker and Recovery Plan

Update the recovery plan to mark this as a new task (Task 8) and update the tracker with verification results.
