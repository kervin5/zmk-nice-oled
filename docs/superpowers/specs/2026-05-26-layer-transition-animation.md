# Layer Transition Animation Design Spec

**Date:** 2026-05-26  
**Status:** Approved

## Overview

Replace the static layer name display with an animated slide-in/slide-out effect that triggers on layer press/release events. Adds subtle background color flash for visual polish.

## Requirements

### Trigger
- Only fires on layer press/release (not every keypress)
- Uses ZMK `zmk_key_press` / `zmk_key_release` events filtered by layer activation

### Animation Behavior
- Old name slides out left, new name slides in from right
- Default duration: 250ms (configurable via Kconfig)
- If animation is mid-progress when another layer change occurs, reset to new target
- Background briefly flashes different color at transition start, restores after completion

### Visual Design
- Text slides horizontally across existing layer name area (~68px wide canvas)
- Position offset starts at +20 (off-screen right), animates to 0 (centered) for slide-in
- Slide-out: offset goes from 0 to -20, then text swaps and resets to +20 before slide-in

### Kconfig Options
```kconfig
config NICE_OLED_WIDGET_LAYER_ANIMATION_MS
    int "Layer transition animation duration in ms"
    default 250
    range 100 500

config NICE_OLED_WIDGET_LAYER_COLOR_FLASH
    bool "Enable background color flash on layer change"
    default y
```

## Architecture

### State Machine (per layer event)
```
IDLE → SLIDE_OUT → SWAP_TEXT → SLIDE_IN → IDLE
```

- **IDLE:** Display current layer name at offset 0
- **SLIDE_OUT:** Animate offset from 0 to -20 over ~50% of duration
- **SWAP_TEXT:** Swap text to new layer name, reset offset to +20
- **SLIDE_IN:** Animate offset from +20 to 0 over remaining ~50%

### Data Structures
```c
struct layer_state {
    const char *current_name;      // Current displayed layer name
    const char *pending_name;      // New name waiting for swap
    int16_t offset_x;              // Horizontal text offset (for animation)
    enum { LAYER_IDLE, LAYER_SLIDE_OUT, LAYER_SLIDE_IN } state;
    uint32_t anim_start_time;      // Timestamp when animation started
    bool flash_active;             // Whether background flash is active
};
```

### Integration Points
- `draw_layer_status()` in screen.c becomes animated version
- Layer press/release events stored as pending name, trigger animation start
- Animation advances each frame based on elapsed time since `anim_start_time`
- Background color toggled during flash period (~100ms)

## Files Modified
- `boards/shields/nice_oled/widgets/layer.c` — New animated layer widget implementation
- `boards/shields/nice_oled/Kconfig.defconfig` — Add animation Kconfig options
- `boards/shields/nice_oled/widgets/screen.c` — Replace static draw call with animated version

## Testing Strategy
- Verify animation triggers only on layer changes (not keypresses)
- Test rapid layer switches (animation resets mid-flight correctly)
- Verify no visual artifacts when animation is interrupted
- Confirm Kconfig options compile correctly and respect defaults
