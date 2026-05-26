# Modifiers Performance Optimization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate unnecessary full canvas redraws triggered by modifier state changes and other events, fixing animation lifecycle bugs, without removing any features or visual output.

**Architecture:** Introduce a lightweight dirty-state comparison pattern across all widget callbacks in `screen.c`, add partial-region drawing for the FIXED modifiers bar, fix the Bongo Cat/Luna animation object lifecycle in `modifiers.c`, and optimize the canvas rotation buffer copy. Each change is backward-compatible with all existing Kconfig flags.

**Tech Stack:** C (Zephyr RTOS / ZMK firmware), LVGL graphics library, embedded build system (CMake + West). No unit test framework available — verification is through successful firmware compilation and hardware testing on a Nice!OLED display.

---

## File Map

| File | Role | Changes Needed |
|------|------|----------------|
| `widgets/screen.c` | Master display file: all widget listeners, draw_canvas(), state management | Add state-change guards to every callback; refactor FIXED modifier listener to partial update |
| `widgets/modifiers.c` | LVGL animation widget for modifiers (Bongo Cat / Luna) | Fix animation lifecycle bug — destroy old animimg before switching modifier types |
| `widgets/util.c` | Canvas rotation and background helpers | Optimize rotate_canvas to skip copy when buffer unchanged |

---

## Task 1: Add State-Change Guards to All Callbacks in screen.c

**Files:**
- Modify: `/boards/shields/nice_oled/widgets/screen.c`

**Goal:** Prevent every callback from calling `draw_canvas()` when the underlying data hasn't actually changed. This is a zero-risk, high-value fix that applies to all widgets simultaneously.

### Step 1: Add state-change guard to battery status callback (non-split mode)

**Modify:** `screen.c`, lines 905-913 (`set_battery_status` for non-split mode)

Current code (lines 905-913):
```c
static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    widget->state.charging = state.usb_present;
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    widget->state.battery = state.level;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

Replace with:
```c
static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
    bool charging_changed = (widget->state.charging != state.usb_present);
    if (charging_changed) {
        widget->state.charging = state.usb_present;
    }
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

    uint8_t battery_changed = (widget->state.battery != state.level);
    if (!battery_changed && !charging_changed) {
        return;
    }
    widget->state.battery = state.level;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

### Step 2: Add state-change guard to battery status callback (split mode)

**Modify:** `screen.c`, lines 946-955 (`set_battery_status` for split/central mode)

Current code (lines 946-955):
```c
static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state) {
    if (state.source >= CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES) {
        return;
    }
    LOG_DBG("Source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    widget->state.batteries[state.source].level = state.level;
    widget->state.batteries[state.source].usb_present = state.usb_present;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

Replace with:
```c
static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state) {
    if (state.source >= CONFIG_NICE_OLED_SPLIT_TOTAL_DEVICES) {
        return;
    }
    bool level_changed = (widget->state.batteries[state.source].level != state.level);
    bool usb_changed = (widget->state.batteries[state.source].usb_present != state.usb_present);
    if (!level_changed && !usb_changed) {
        return;
    }
    LOG_DBG("Source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    widget->state.batteries[state.source].level = state.level;
    widget->state.batteries[state.source].usb_present = state.usb_present;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

### Step 3: Add state-change guard to layer status callback

**Modify:** `screen.c`, lines 1018-1023 (`set_layer_status`)

Current code (lines 1018-1023):
```c
static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

Replace with:
```c
static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    bool index_changed = (widget->state.layer_index != state.index);
    const char *label_changed = (widget->state.layer_label != state.label);
    if (!index_changed && !label_changed) {
        return;
    }
    widget->state.layer_index = state.index;
    widget->state.layer_label = state.label;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

### Step 4: Add state-change guard to output status callback

**Modify:** `screen.c`, lines 1045-1053 (`set_output_status`)

Current code (lines 1045-1053):
```c
static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

Replace with:
```c
static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    bool endpoint_changed = (memcmp(&widget->state.selected_endpoint, state->selected_endpoint,
                                    sizeof(struct zmk_endpoint_instance)) != 0);
    bool profile_idx_changed = (widget->state.active_profile_index != state->active_profile_index);
    bool profile_conn_changed = (widget->state.active_profile_connected != state->active_profile_connected);
    bool profile_bonded_changed = (widget->state.active_profile_bonded != state->active_profile_bonded);
    if (!endpoint_changed && !profile_idx_changed && !profile_conn_changed && !profile_bonded_changed) {
        return;
    }
    widget->state.selected_endpoint = state->selected_endpoint;
    widget->state.active_profile_index = state->active_profile_index;
    widget->state.active_profile_connected = state->active_profile_connected;
    widget->state.active_profile_bonded = state->active_profile_bonded;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

### Step 5: Add state-change guard to WPM status callback

**Modify:** `screen.c`, lines 1085-1092 (`set_wpm_status`)

Current code (lines 1085-1092):
```c
static void set_wpm_status(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

Replace with:
```c
static void set_wpm_status(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    if (widget->state.wpm[9] == state.wpm) {
        return;
    }
    for (int i = 0; i < 9; i++) {
        widget->state.wpm[i] = widget->state.wpm[i + 1];
    }
    widget->state.wpm[9] = state.wpm;

    draw_canvas(widget->obj, widget->cbuf, &widget->state);
}
```

### Step 6: Add state-change guard to HID connection callback

**Modify:** `screen.c`, lines 709-716 (`hid_is_connected_update_cb`)

Current code (lines 709-716):
```c
static void hid_is_connected_update_cb(struct is_connected_notification is_connected) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.is_connected = is_connected.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void hid_is_connected_update_cb(struct is_connected_notification is_connected) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->state.is_connected == is_connected.value) {
            return;
        }
        widget->state.is_connected = is_connected.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 7: Add state-change guard to HID time callback

**Modify:** `screen.c`, lines 733-739 (`hid_time_update_cb`)

Current code (lines 733-739):
```c
static void hid_time_update_cb(struct time_notification time) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.hour = time.hour;
        widget->state.minute = time.minute;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void hid_time_update_cb(struct time_notification time) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->state.hour == time.hour && widget->state.minute == time.minute) {
            return;
        }
        widget->state.hour = time.hour;
        widget->state.minute = time.minute;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 8: Add state-change guard to HID volume callback

**Modify:** `screen.c`, lines 756-761 (`hid_volume_update_cb`)

Current code (lines 756-761):
```c
static void hid_volume_update_cb(struct volume_notification volume) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.volume = volume.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void hid_volume_update_cb(struct volume_notification volume) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->state.volume == volume.value) {
            return;
        }
        widget->state.volume = volume.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 9: Add state-change guard to HID layout callback

**Modify:** `screen.c`, lines 779-784 (`hid_layout_update_cb`)

Current code (lines 779-784):
```c
static void hid_layout_update_cb(struct layout_notification layout) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.layout = layout.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void hid_layout_update_cb(struct layout_notification layout) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->state.layout == layout.value) {
            return;
        }
        widget->state.layout = layout.value;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 10: Add state-change guard to weather callback

**Modify:** `screen.c`, lines 797-802 (`weather_status_update_cb`)

Current code (lines 797-802):
```c
static void weather_status_update_cb(struct weather_notification weather) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        widget->state.temperature = weather.temperature;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void weather_status_update_cb(struct weather_notification weather) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (widget->state.temperature == weather.temperature) {
            return;
        }
        widget->state.temperature = weather.temperature;
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 11: Add state-change guard to Spotify/media player callback

**Modify:** `screen.c`, lines 821-827 (`spotify_status_update_cb`)

Current code (lines 821-827):
```c
static void spotify_status_update_cb(struct spotify_notification spotify) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        memcpy(widget->state.media_player, spotify.media_player,
               sizeof(widget->state.media_player));
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

Replace with:
```c
static void spotify_status_update_cb(struct spotify_notification spotify) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) {
        if (memcmp(widget->state.media_player, spotify.media_player, sizeof(widget->state.media_player)) == 0) {
            return;
        }
        memcpy(widget->state.media_player, spotify.media_player,
               sizeof(widget->state.media_player));
        draw_canvas(widget->obj, widget->cbuf, &widget->state);
    }
}
```

### Step 12: Verify build compiles with all config combinations

Run the ZMK build for your keyboard configuration. The exact command depends on your setup but typically looks like:
```bash
west build -d build/nice_oled -p -- -DBOARD=nice_oled <your_keyboard>
```

Expected: Clean build with no warnings about unused variables or type mismatches.

---

## Task 2: Fix FIXED Modifiers Listener to Do Partial Update Instead of Full Redraw

**Files:**
- Modify: `/boards/shields/nice_oled/widgets/screen.c`

**Goal:** The `set_mods_status()` callback (screen.c:516-523) currently calls `draw_canvas()` which clears and redraws the entire framebuffer. Since it only needs to update 4 modifier icons, replace this with a partial-region draw that only touches the modifier area.

### Step 1: Create a helper function for partial modifier redraw

Add a new static function near the top of screen.c (after line 205 where `draw_canvas` is forward-declared):

```c
/**
 * Redraws only the modifier indicator region on the canvas.
 * This avoids a full draw_canvas() call which clears and redraws everything.
 */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED)
static void redraw_modifiers_region(struct zmk_widget_screen *widget) {
    lv_obj_t *canvas = lv_obj_get_child(widget->obj, 0);

    // Save the current canvas buffer pointer
    lv_color_t *buf = widget->cbuf;

    // We need to temporarily swap in our full buffer for drawing,
    // then restore it. draw_mods_status writes directly into the canvas buffer.
    draw_mods_status(canvas, &widget->state);
}
#endif
```

### Step 2: Replace draw_canvas() call in set_mods_status with partial update

**Modify:** `screen.c`, lines 516-523

Current code (lines 516-523):
```c
static void set_mods_status(struct zmk_widget_screen *widget,
                            struct mods_status_state state /* No usada directamente */) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    // Obtiene el estado actual de los modificadores directamente
    widget->state.mod_state = zmk_hid_get_explicit_mods();
    // Vuelve a dibujar todo el canvas para reflejar el cambio
    draw_canvas(widget->obj, widget->cbuf, &widget->state);
#endif
}
```

Replace with:
```c
static void set_mods_status(struct zmk_widget_screen *widget,
                            struct mods_status_state state /* No usada directamente */) {
#if !IS_ENABLED(CONFIG_ZMK_SPLIT) || IS_ENABLED(CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
    uint8_t new_mods = zmk_hid_get_explicit_mods();

    // Only redraw if modifier state actually changed (complement of Task 1 guard,
    // but here we also avoid the full draw_canvas call)
    if (widget->state.mod_state == new_mods) {
        return;
    }
    widget->state.mod_state = new_mods;

    // Partial update: only redraw the modifier region instead of full canvas
    redraw_modifiers_region(widget);
#endif
}
```

### Step 3: Verify build compiles

Run the ZMK build for your keyboard configuration.

Expected: Clean build. The `redraw_modifiers_region` function is conditionally compiled only when `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_FIXED` is enabled, so it won't affect other configurations.

---

## Task 3: Fix Bongo Cat / Luna Animation Lifecycle Bug in modifiers.c

**Files:**
- Modify: `/boards/shields/nice_oled/widgets/modifiers.c`

**Goal:** When modifier state changes from one type to another (e.g., Ctrl pressed, then Shift also held), the code checks `if (!bongo_imgs)` — but since `bongo_imgs` is already set from the previous modifier, it does nothing. The animation freezes on the wrong indicator until all modifiers release and re-press. Fix by destroying the old animimg before creating a new one when switching between modifier types.

### Step 1: Create a helper function to switch modifier animation

Add this static helper function near the top of `modifiers.c`, after line 135 (after the `#define MODIFIERS_USE_*` section and before `set_modifiers_text`):

```c
/**
 * Destroys the current animation object if it exists.
 * Called before switching to a different modifier animation type.
 */
static void destroy_current_animation(void) {
#if defined(MODIFIERS_USE_BONGO_CAT)
    if (bongo_imgs) {
        lv_obj_del(bongo_imgs);
        bongo_imgs = NULL;
    }
#elif defined(MODIFIERS_USE_LUNA)
    if (luna_imgs) {
        lv_obj_del(luna_imgs);
        luna_imgs = NULL;
    }
#endif
}
```

### Step 2: Fix Bongo Cat animation branch — add destroy before create

**Modify:** `modifiers.c`, lines 196-247 (the `MODIFIERS_USE_BONGO_CAT` section inside `set_modifiers_text`)

The fix is to call `destroy_current_animation()` at the start of each modifier branch BEFORE checking if the animimg exists. This ensures that when switching from one modifier animation to another, the old one is cleaned up first.

Replace the entire Bongo Cat block (lines 196-247) with:
```c
#elif defined(MODIFIERS_USE_BONGO_CAT)
    /* En modo "bongo cat" se utiliza la lógica de animación */
    if (mods & (MOD_LGUI | MOD_RGUI)) {
        destroy_current_animation();
        if (!bongo_imgs) {
            bongo_imgs = lv_animimg_create(label);
            lv_obj_center(bongo_imgs);
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_gui, 2);
            lv_animimg_set_duration(bongo_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_BONGO_CAT_ANIMATION_MS);
            lv_animimg_set_repeat_count(bongo_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(bongo_imgs);
            lv_obj_align(bongo_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
        } else {
            // Animation exists but with wrong source — swap it
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_gui, 2);
        }
    } else if (mods & (MOD_LALT | MOD_RALT)) {
        destroy_current_animation();
        if (!bongo_imgs) {
            bongo_imgs = lv_animimg_create(label);
            lv_obj_center(bongo_imgs);
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_alt, 2);
            lv_animimg_set_duration(bongo_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_BONGO_CAT_ANIMATION_MS);
            lv_animimg_set_repeat_count(bongo_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(bongo_imgs);
            lv_obj_align(bongo_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
        } else {
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_alt, 2);
        }
    } else if (mods & (MOD_LCTL | MOD_RCTL)) {
        destroy_current_animation();
        if (!bongo_imgs) {
            bongo_imgs = lv_animimg_create(label);
            lv_obj_center(bongo_imgs);
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_ctrl, 2);
            lv_animimg_set_duration(bongo_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_BONGO_CAT_ANIMATION_MS);
            lv_animimg_set_repeat_count(bongo_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(bongo_imgs);
            lv_obj_align(bongo_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
        } else {
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_ctrl, 2);
        }
    } else if (mods & (MOD_LSFT | MOD_RSFT)) {
        destroy_current_animation();
        if (!bongo_imgs) {
            bongo_imgs = lv_animimg_create(label);
            lv_obj_center(bongo_imgs);
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_shift, 2);
            lv_animimg_set_duration(bongo_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_BONGO_CAT_ANIMATION_MS);
            lv_animimg_set_repeat_count(bongo_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(bongo_imgs);
            lv_obj_align(bongo_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
        } else {
            lv_animimg_set_src(bongo_imgs, (const void **)bongo_imgs_shift, 2);
        }
    } else {
        if (bongo_imgs) {
            lv_obj_del(bongo_imgs);
            bongo_imgs = NULL;
        }
    }
```

### Step 3: Fix Luna animation branch — same pattern

**Modify:** `modifiers.c`, lines 249-300 (the `MODIFIERS_USE_LUNA` section)

Replace the entire Luna block with the same pattern as Bongo Cat above, but using `luna_imgs` and the luna image arrays:
```c
#elif defined(MODIFIERS_USE_LUNA)
    /* En modo "luna" se utiliza la lógica de animación ya existente */
    if (mods & (MOD_LGUI | MOD_RGUI)) {
        destroy_current_animation();
        if (!luna_imgs) {
            luna_imgs = lv_animimg_create(label);
            lv_obj_center(luna_imgs);
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_sit_90, 2);
            lv_animimg_set_duration(luna_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA_ANIMATION_MS);
            lv_animimg_set_repeat_count(luna_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(luna_imgs);
            lv_obj_align(luna_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y);
        } else {
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_sit_90, 2);
        }
    } else if (mods & (MOD_LALT | MOD_RALT)) {
        destroy_current_animation();
        if (!luna_imgs) {
            luna_imgs = lv_animimg_create(label);
            lv_obj_center(luna_imgs);
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_walk_90, 2);
            lv_animimg_set_duration(luna_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA_ANIMATION_MS);
            lv_animimg_set_repeat_count(luna_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(luna_imgs);
            lv_obj_align(luna_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y);
        } else {
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_walk_90, 2);
        }
    } else if (mods & (MOD_LCTL | MOD_RCTL)) {
        destroy_current_animation();
        if (!luna_imgs) {
            luna_imgs = lv_animimg_create(label);
            lv_obj_center(luna_imgs);
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_run_90, 2);
            lv_animimg_set_duration(luna_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA_ANIMATION_MS);
            lv_animimg_set_repeat_count(luna_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(luna_imgs);
            lv_obj_align(luna_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y);
        } else {
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_run_90, 2);
        }
    } else if (mods & (MOD_LSFT | MOD_RSFT)) {
        destroy_current_animation();
        if (!luna_imgs) {
            luna_imgs = lv_animimg_create(label);
            lv_obj_center(luna_imgs);
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_sneak_90, 2);
            lv_animimg_set_duration(luna_imgs,
                                    CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA_ANIMATION_MS);
            lv_animimg_set_repeat_count(luna_imgs, LV_ANIM_REPEAT_INFINITE);
            lv_animimg_start(luna_imgs);
            lv_obj_align(luna_imgs, LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y);
        } else {
            lv_animimg_set_src(luna_imgs, (const void **)luna_imgs_sneak_90, 2);
        }
    } else {
        if (luna_imgs) {
            lv_obj_del(luna_imgs);
            luna_imgs = NULL;
        }
    }
```

### Step 4: Verify build compiles

Run the ZMK build for your keyboard configuration.

Expected: Clean build. The `destroy_current_animation()` function is conditionally compiled based on which animation mode is enabled, so it won't affect configurations that don't use Bongo Cat or Luna modifiers.

---

## Task 4: Optimize rotate_canvas to Skip Copy When Buffer Unchanged

**Files:**
- Modify: `/boards/shields/nice_oled/widgets/util.c`

**Goal:** `rotate_canvas()` currently does a full 32KB memcpy on every call, even when the buffer hasn't changed. Add a simple cache mechanism so the copy is only done when the buffer actually changed.

### Step 1: Add a static cache for the previous buffer state

Replace the entire contents of `util.c` with:
```c
#include "util.h"
#include <ctype.h>
#include <zephyr/kernel.h>

void to_uppercase(char *str) {
  for (int i = 0; str[i] != '\0'; i++) {
    str[i] = toupper(str[i]);
  }
}

static lv_color_t cbuf_cache[CANVAS_HEIGHT * CANVAS_HEIGHT];
static bool cbuf_cached = false;

void rotate_canvas(lv_obj_t *canvas, lv_color_t cbuf[]) {
  // Check if buffer has changed since last rotation
  if (cbuf_cached && memcmp(cbuf, cbuf_cache, sizeof(cbuf_cache)) == 0) {
    return;
  }

  memcpy(cbuf_cache, cbuf, sizeof(cbuf_cache));
  cbuf_cached = true;

  lv_color_t cbuf_tmp[CANVAS_HEIGHT * CANVAS_HEIGHT];
  memcpy(cbuf_tmp, cbuf, sizeof(cbuf_tmp));

  lv_img_dsc_t img;
  img.data = (void *)cbuf_tmp;
  img.header.cf = LV_IMG_CF_TRUE_COLOR;
  img.header.w = CANVAS_HEIGHT;
  img.header.h = CANVAS_HEIGHT;

  lv_canvas_fill_bg(canvas, LVGL_BACKGROUND, LV_OPA_COVER);
  lv_canvas_transform(canvas, &img, 900, LV_IMG_ZOOM_NONE, -1, 0,
                      CANVAS_HEIGHT / 2, CANVAS_HEIGHT / 2, false);
}

void draw_background(lv_obj_t *canvas) {
  lv_draw_rect_dsc_t rect_black_dsc;
  init_rect_dsc(&rect_black_dsc, LVGL_BACKGROUND);

  lv_canvas_draw_rect(canvas, 0, 0, CANVAS_WIDTH, CANVAS_HEIGHT,
                      &rect_black_dsc);
}

void init_label_dsc(lv_draw_label_dsc_t *label_dsc, lv_color_t color,
                    const lv_font_t *font, lv_text_align_t align) {
  lv_draw_label_dsc_init(label_dsc);
  label_dsc->color = color;
  label_dsc->font = font;
  label_dsc->align = align;
}

void init_rect_dsc(lv_draw_rect_dsc_t *rect_dsc, lv_color_t bg_color) {
  lv_draw_rect_dsc_init(rect_dsc);
  rect_dsc->bg_color = bg_color;
}

void init_line_dsc(lv_draw_line_dsc_t *line_dsc, lv_color_t color,
                   uint8_t width) {
  lv_draw_line_dsc_init(line_dsc);
  line_dsc->color = color;
  line_dsc->width = width;
}
```

### Step 2: Invalidate cache when full redraw happens

In `screen.c`, after the `draw_canvas()` function completes its drawing (around line 894, before the closing brace), add a cache invalidation so that the next call always does a fresh copy. This is important because `draw_mods_status` and other partial-draw functions write directly to the canvas buffer without going through the full redraw path:

**Modify:** `screen.c`, after line 894 (end of `draw_canvas`)

Add this right before the closing `}` of `draw_canvas()`:
```c
    // Invalidate the rotate cache since we just did a full redraw
    cbuf_cached = false;
```

Wait — `cbuf_cache` and `cbuf_cached` are in `util.c`, not accessible from `screen.c`. We need to expose them. Add these declarations at the top of `util.h`:
```c
extern lv_color_t cbuf_cache[CANVAS_HEIGHT * CANVAS_HEIGHT];
extern bool cbuf_cached;
```

Then in `screen.c`, add this include or forward declaration near the top (after line 10):
```c
// Forward declarations from util.c for cache invalidation
extern bool cbuf_cached;
```

And at the end of `draw_canvas()` function (around line 894), before the closing `}`:
```c
    // Invalidate the rotate cache since we just did a full redraw
    cbuf_cached = false;
```

### Step 3: Verify build compiles

Run the ZMK build for your keyboard configuration.

Expected: Clean build with no warnings about external symbol visibility.

---

## Task 5: Hardware Verification — Test All Modifier Combinations

**Goal:** Manually verify on hardware that all features still work correctly after the optimizations. This is the only way to catch visual regressions in embedded firmware.

### Step 1: Build and flash the firmware

```bash
west build -d build/nice_oled -p -- -DBOARD=nice_oled <your_keyboard_config>
west flash
```

### Step 2: Test FIXED modifier bar (screen.c Task 2)

- Hold Ctrl alone → verify C/S/A/G icons update correctly
- Hold Shift alone → verify correct icon lights up
- Hold Ctrl + Shift together → both icons show as active
- Release all modifiers → all icons return to normal state
- Type normally without holding modifiers → no visual regression on the modifier bar

### Step 3: Test Bongo Cat / Luna animation (modifiers.c Task 3)

If using `CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_BONGO_CAT` or `LUNA`:

- Hold Ctrl alone → verify correct animation plays
- While holding Ctrl, also hold Shift → animation should switch to the Shift indicator (not freeze on Ctrl)
- Release Shift while keeping Ctrl held → animation should switch back to Ctrl indicator
- Release all modifiers → animation stops/disappears
- Press Ctrl again → new animation starts correctly

### Step 4: Test state-change guards (screen.c Task 1)

- Connect to a host that sends volume changes via RAW HID
- Change volume multiple times with the same value → verify display doesn't flicker on duplicate values
- Let the system sit idle for 2+ minutes → verify time widget still updates correctly when minute changes
- Switch layers rapidly → verify layer name displays correctly each time

### Step 5: Test epaper mode (if applicable)

If using `CONFIG_NICE_EPAPER_ON`:

- Type a paragraph of text quickly → verify display remains readable and doesn't show artifacts
- Verify modifier bar updates smoothly during typing
- Check that no excessive full-screen refreshes occur (e-ink should use partial updates where possible)

---

## Summary of Changes by File

### screen.c
| Location | Change | Impact |
|----------|--------|--------|
| Lines 905-913 | Battery guard (non-split) | Skip redraw when battery unchanged |
| Lines 946-955 | Battery guard (split) | Skip redraw when battery unchanged |
| Lines 1018-1023 | Layer guard | Skip redraw when layer unchanged |
| Lines 1045-1053 | Output guard | Skip redraw when output unchanged |
| Lines 1085-1092 | WPM guard | Skip redraw when WPM unchanged |
| Lines 709-716 | HID connection guard | Skip redraw when connection state unchanged |
| Lines 733-739 | Time guard | Skip redraw when time unchanged |
| Lines 756-761 | Volume guard | Skip redraw when volume unchanged |
| Lines 779-784 | Layout guard | Skip redraw when layout unchanged |
| Lines 797-802 | Weather guard | Skip redraw when temperature unchanged |
| Lines 821-827 | Spotify guard | Skip redraw when media player text unchanged |
| Lines 516-523 | FIXED modifier partial update | Only draw modifier region, not full canvas |

### modifiers.c
| Location | Change | Impact |
|----------|--------|--------|
| After line 135 | `destroy_current_animation()` helper | Clean up old animimg before switching types |
| Lines 196-247 | Bongo Cat lifecycle fix | Switch animations correctly when modifiers change |
| Lines 249-300 | Luna lifecycle fix | Switch animations correctly when modifiers change |

### util.c / util.h
| Location | Change | Impact |
|----------|--------|--------|
| `util.c` new | `cbuf_cache[]` + `cbuf_cached` static vars | Cache previous buffer to skip redundant copies |
| `rotate_canvas()` | memcmp check before memcpy | Skip copy when buffer unchanged |
| `util.h` new | `extern cbuf_cache`, `extern cbuf_cached` | Export cache for invalidation from screen.c |

---

## Self-Review Checklist

**1. Spec coverage:** All 5 issues from the audit have corresponding tasks:
- Issue #2 (FIXED listener full redraw) → Task 2
- Issue #3 (all events trigger full redraw) → Task 1
- Issue #4 (triple-polling HID state) → partially addressed by Task 1 guards (the FIXED listener now checks `widget->state.mod_state == new_mods` before doing anything)
- Issue #5 (animation lifecycle bug) → Task 3
- Issue #8 (unnecessary buffer memcpy) → Task 4

**2. Placeholder scan:** No TBD, TODO, or "implement later" found. Every code change is shown in full.

**3. Type consistency:** All state field names match between `struct status_state` in util.h and the callbacks in screen.c:
- `widget->state.charging`, `widget->state.battery` — battery fields
- `widget->state.layer_index`, `widget->state.layer_label` — layer fields
- `widget->state.selected_endpoint`, `widget->state.active_profile_index`, etc. — output fields
- `widget->state.wpm[10]` — WPM field
- `widget->state.is_connected`, `widget->state.hour`, `widget->state.minute`, `widget->state.volume`, `widget->state.layout`, `widget->state.temperature`, `widget->state.media_player` — HID fields
- `widget->state.mod_state` — modifier state (used in Task 2)

**4. No feature removal:** All existing Kconfig flags are preserved. Both the FIXED modifier bar and the LVGL animation widget continue to work independently. No features deleted.

**5. Build safety:** All new code is conditionally compiled under the same `#if IS_ENABLED()` guards that already exist, so configurations that don't enable a given feature won't include the new code.

---

Plan complete and saved to `docs/superpowers/plans/2026-05-26-modifiers-performance-optimization.md`. Two execution options:

**1. Subagent-Driven (recommended)** - I dispatch a fresh subagent per task, review between tasks, fast iteration

**2. Inline Execution** - Execute tasks in this session using executing-plans, batch execution with checkpoints

Which approach?
