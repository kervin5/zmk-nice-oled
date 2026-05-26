# Portrait Layout Design — 68×160 Display

## Problem Statement

The display platform uses a legacy rendering path where widgets draw into a 160×160 square buffer, then `rotate_canvas()` rotates the entire buffer 90° CW around center (80,80) to produce the final portrait image on a 68×160 physical display. The native portrait path (`CONFIG_NICE_OLED_NATIVE_PORTRAIT=y`) skips rotation and draws directly into a 68×160 canvas buffer — but widget coordinates were designed for the rotated coordinate space, so they land at wrong positions (or off-screen) with no rotation to correct them.

The compat adapter `central_draw_compat.c` currently performs an identity mapping (`{x,y} → {x,y}`), which means native portrait renders widgets in completely wrong locations — effectively broken.

## Root Cause Analysis

Tracing through every widget coordinate and computing its physical position after the 90° CW rotation reveals that most hardcoded coordinates land off-screen even in legacy mode:

| Widget | Canvas Coords | Physical Position After Rotation | On-Screen? |
|--------|--------------|----------------------------------|------------|
| Battery text "X%" | (0, 50) | (50, **160**) | NO — phys_y=160 (off bottom edge) |
| USB icon | (0, 34) | (34, **160**) | NO — phys_y=160 |
| Layer text | (0, 146) | (**146**, **160**) | NO — both x and y off-screen |
| Profile icons | (0, 137) | (**137**, **160**) | NO — both off-screen |
| WPM grid | (0, 65) | (65, **160**) | NO — phys_y=160 |

Only a handful of elements reliably land on-screen after rotation:
- Battery bolt icon at `(25, 50)` → physical `(50, 135)` ✓
- BT icons at `(4, 32)` → physical `(32, 156)` ✓

The legacy path relies on LVGL clipping during `lv_canvas_transform()` to hide off-screen content. In native portrait mode there is no rotation and no clipping — widgets draw at their hardcoded coordinates directly into the buffer.

## Design Decision: Portrait Layout Redesign

Rather than attempting to reverse-map rotated coordinates (which would still place most content off-screen), we redesign all widget positions for direct 68×160 portrait rendering. The compat adapter stays as identity mapping since no transformation is needed — it becomes a pass-through wrapper that preserves the abstraction layer for future changes.

### Scope

- **Central screen:** Battery, connection status (BT/USB), layer name, profile indicator + text, WPM graph/gauge/speedometer/animation/modifiers
- **Peripheral screen:** Battery percentage + bolt icon, connection status, peripheral animations (crystal, cat, spaceman, pokemon, head)

### Asset Dimensions

| Widget | Element | Size (W×H) | Notes |
|--------|---------|-----------|-------|
| Central | Bongo Cat frames | 26×50 | Animated, centered via `lv_obj_center()` |
| Central | Luna dog frames | 24×32 | Animated, centered via `lv_obj_center()` |
| Central | Grid image (WPM graph bg) | 67×33 | Full width of display |
| Central | Gauge (speedometer bg) | 33×10 | Half-width strip |
| Peripheral | Crystal animation | 69×68 | Smart battery, full width |
| Peripheral | Cat/spaceman/pokemon/head | 69×68 | Animated, full width |

### Central Screen Layout (68×160)

```
┌──────────────────────┐ 68px wide
│ 72% ⚡               │ y=5    battery % + bolt icon
│ [BT] [USB]           │ y=30   connection status icons  
│                      │ y=45   spacer (15px)
│ Layer 3              │ y=60   layer name
│                      │ y=72   spacer (8px)
│ ●○○○○ Profile 2      │ y=80   profile indicator + text
│                      │ y=95   spacer (15px)
├──────────────────────┤ y=100  ── WPM mode splits here ──
│ [grid bg]            │ y=100  graph: grid image at y=100, 33px tall
│   /--\               │        line graph drawn inside grid area (y=100-132)
│  WPM:45              │ y=135  WPM number label below graph
├──────────────────────┤ y=148  ── Animation mode splits here ──
│ [Luna dog]           │ y=100  Luna centered vertically in remaining space (24x32)
│                      │        no other content when animation active
├──────────────────────┤ y=148  ── Bongo Cat mode splits here ──
│ [Bongo Cat]          │ y=95   Bongo Cat centered vertically in remaining space (26x50)
│                      │        no other content when animation active
├──────────────────────┤ y=148  ── Speedometer mode splits here ──
│ [gauge]              │ y=100  gauge strip at y=100 (33x10)
│   needle             │        needle drawn from center of gauge
│  WPM:45              │ y=120  WPM number below gauge
├──────────────────────┤ y=148  ── Modifiers mode splits here ──
│ [MODS]               │ y=130  modifiers indicators (compact)
│                      │        no graph when modifiers active
└──────────────────────┘ 160px tall
```

### Peripheral Screen Layout (68×160)

```
┌──────────────────────┐ 68px wide
│                      │ y=20   spacer
│ [animation]          │ y=45   animation (69x68) centered horizontally
│                      │        takes up ~43% of screen height
│ 72% ⚡               │ y=115  battery % + bolt icon below animation
│ [BT]                 │ y=140  connection status at bottom
└──────────────────────┘ 160px tall
```

### WPM Mode Priority (mutually exclusive on central)

The code already enforces this priority: `LUNA > BONGO_CAT > SPEEDOMETER > GRAPH`. Each mode occupies the area below y=95 and replaces all other WPM rendering. When an animation is active, no graph/gauge/label content renders.

### Kconfig Updates

Update `CONFIG_NICE_OLED_WIDGET_*_CUSTOM_X/Y` defaults for NICE_OLED_ON to portrait-friendly values:
- Battery: x=3, y=5 (was 0, 50)
- Output BT: x=4, y=32 (unchanged)
- Output USB: x=36, y=32 (was 0, 34 — move right of BT icon)
- Layer: x=0, y=60 (was 0, 146)
- Profile indicator: x=0, y=80 (was 0, 137)
- Profile text: x=25, y=80 (was 25, 32 — align with indicator)
- WPM graph grid: y=100 (was 65/95 depending on mode)
- WPM number label: y=135 (was 103/110)

### Compat Adapter

No changes needed. The identity mapping in `central_draw_compat.c` is correct for portrait rendering — it passes coordinates through unchanged, which is exactly what we want when all widgets use portrait-native coordinates.

## Files Changed

| File | Change |
|------|--------|
| `Kconfig.defconfig` | Update NICE_OLED_ON defaults for widget X/Y positions |
| `battery.c` | Use new Kconfig defaults (no code changes needed if Kconfig updated) |
| `output.c` | Same — relies on Kconfig defaults |
| `layer.c` | Same |
| `profile.c` | Same |
| `wpm.c` | Update hardcoded grid/label Y positions for portrait; update graph line coordinates to fit in 67×32 area below y=100 |
| `screen_peripheral_render.c` | Update peripheral draw functions for portrait layout (battery + connection below animation) |
| `luna.c`, `bongo_cat.c` | No changes — use `lv_obj_center()` which works in any buffer size |

## Success Criteria

- All widgets render at correct positions on 68×160 display with `CONFIG_NICE_OLED_NATIVE_PORTRAIT=y`
- Same visual output as legacy rotation path (all visible elements present)
- Smoke builds pass for both central and peripheral roles
- No regression in FLASH/RAM usage (>95% of baseline: FLASH 36.47%, RAM 42.12%)
