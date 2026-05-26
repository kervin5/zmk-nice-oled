#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/display.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/usb_conn_state_changed.h>
#include <zmk/events/wpm_state_changed.h>
#include <zmk/keymap.h>
#include <zmk/usb.h>
#include <zmk/wpm.h>
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
#include <zmk/split/central.h>
#endif

#include <fonts.h>
#include "output.h"
#include "profile.h"
#include "screen.h"

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
#include <lvgl.h>
#include <raw_hid/hid.h>
#endif

#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) &&                    \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) &&                   \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

#include "battery.h"
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

/* draw_battery_text_central moved to screen_central.c (Task 4 of compositor boundaries plan) */
struct battery_state {
    uint8_t source;
    uint8_t level;
    bool usb_present;
};

#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
#include "layer.h"
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
#include "wpm.h"
#endif // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)

static sys_slist_t widgets = SYS_SLIST_STATIC_INIT(&widgets);


/**
 * Battery status
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE) ||                                         \
    IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
#include "sleep_status.h"
static struct zmk_widget_sleep_status sleep_status_widget;
#endif

/**
 * luna
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
#include "luna.h"
static struct zmk_widget_luna luna_widget;
#endif

/**
 * bongo cat
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
#include "bongo_cat.h"
static struct zmk_widget_wpm_bongo_cat wpm_bongo_cat_widget;
#endif

/**
 * responsive bongo cat
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
#include "responsive_bongo_cat.h"
static struct zmk_widget_responsive_bongo_cat responsive_bongo_cat_widget;
#endif

/**
 * modifiers
 **/
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA)
#include "modifiers.h"
static struct zmk_widget_modifiers modifiers_widget;
#endif

/**
 * raw hid
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID)

/* draw_hid_status moved to screen_central.c (Task 4 of compositor boundaries plan) */

#endif // CONFIG_NICE_OLED_WIDGET_RAW_HID

/* RAW HID label listeners — bridge ZMK notifications to persistent LVGL labels */
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID)
#include <zmk/events/raw_hid.h>
#include "raw_hid_label.h"

static int raw_hid_weather_listener(const zmk_event_t *eh) {
    const struct weather_notification *ev = as_weather_notification(eh);
    if (ev) {
        raw_hid_label_update_weather(ev->temperature);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_weather, raw_hid_weather_listener);
ZMK_SUBSCRIPTION(raw_hid_weather, weather_notification);

static int raw_hid_time_listener(const zmk_event_t *eh) {
    const struct time_notification *ev = as_time_notification(eh);
    if (ev) {
        raw_hid_label_update_time(ev->hour, ev->minute);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_time, raw_hid_time_listener);
ZMK_SUBSCRIPTION(raw_hid_time, time_notification);

static int raw_hid_volume_listener(const zmk_event_t *eh) {
    const struct volume_notification *ev = as_volume_notification(eh);
    if (ev) {
        raw_hid_label_update_volume(ev->value);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_volume, raw_hid_volume_listener);
ZMK_SUBSCRIPTION(raw_hid_volume, volume_notification);

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
static int raw_hid_layout_listener(const zmk_event_t *eh) {
    const struct layout_notification *ev = as_layout_notification(eh);
    if (ev) {
        raw_hid_label_update_layout(ev->value, NULL);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_layout, raw_hid_layout_listener);
ZMK_SUBSCRIPTION(raw_hid_layout, layout_notification);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
static int raw_hid_spotify_listener(const zmk_event_t *eh) {
    const struct spotify_notification *ev = as_spotify_notification(eh);
    if (ev) {
        raw_hid_label_update_media_player(ev->media_player);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(raw_hid_spotify, raw_hid_spotify_listener);
ZMK_SUBSCRIPTION(raw_hid_spotify, spotify_notification);
#endif
#endif // CONFIG_NICE_OLED_WIDGET_RAW_HID

/**
 * hid indicators
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
#include "hid_indicators.h"
static struct zmk_widget_hid_indicators hid_indicators_widget;
#endif


/**
 * Battery status
 **/

#if !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) &&                    \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) &&                   \
    !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

static void set_battery_status(struct zmk_widget_screen *widget,
                               struct battery_status_state state) {
    const nice_oled_dirty_mask_t dirty = nice_oled_central_apply_battery_state(
        &widget->central, state.level,
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        state.usb_present
#else
        false
#endif
    );

    if (dirty == NICE_OLED_DIRTY_NONE) {
        return;
    }

    widget->central.dirty |= dirty;
    nice_oled_screen_central_redraw(&widget->compositor);
}

static void battery_status_update_cb(struct battery_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

static struct battery_status_state battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);

    return (struct battery_status_state){
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_status_state,
                            battery_status_update_cb, battery_status_get_state);

ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */

#endif // !IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)

static void set_battery_status(struct zmk_widget_screen *widget, struct battery_state state) {
    LOG_DBG("Source: %d, level: %d, usb: %d", state.source, state.level, state.usb_present);
    const nice_oled_dirty_mask_t dirty = nice_oled_central_apply_split_battery_state(
        &widget->central, state.source, state.level, state.usb_present);

    if (dirty == NICE_OLED_DIRTY_NONE) {
        return;
    }

    widget->central.dirty |= dirty;
    nice_oled_screen_central_redraw(&widget->compositor);
}

void battery_status_update_cb(struct battery_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_battery_status(widget, state); }
}

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
static struct battery_state peripheral_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_peripheral_battery_state_changed *ev =
        as_zmk_peripheral_battery_state_changed(eh);
    return (struct battery_state){
        .source = ev->source + 1,
        .level = ev->state_of_charge,
    };
}
#endif

static struct battery_state central_battery_status_get_state(const zmk_event_t *eh) {
    const struct zmk_battery_state_changed *ev = as_zmk_battery_state_changed(eh);
    return (struct battery_state){
        .source = 0,
        .level = (ev != NULL) ? ev->state_of_charge : zmk_battery_state_of_charge(),
#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
        .usb_present = zmk_usb_is_powered(),
#endif /* IS_ENABLED(CONFIG_USB_DEVICE_STACK) */
    };
}

static struct battery_state battery_status_get_state(const zmk_event_t *eh) {
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
    if (as_zmk_peripheral_battery_state_changed(eh) != NULL) {
        return peripheral_battery_status_get_state(eh);
    } else {
        return central_battery_status_get_state(eh);
    }
#else
    return central_battery_status_get_state(eh);
#endif
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_battery_status, struct battery_state, battery_status_update_cb,
                            battery_status_get_state)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ALL) ||                     \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_ONLY) ||                    \
    IS_ENABLED(CONFIG_NICE_OLED_WIDGET_CENTRAL_SHOW_BATTERY_PERIPHERAL_AND_CENTRAL)
ZMK_SUBSCRIPTION(widget_battery_status, zmk_peripheral_battery_state_changed);
#endif
ZMK_SUBSCRIPTION(widget_battery_status, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(widget_battery_status, zmk_usb_conn_state_changed);
#endif
// TODO: batt END

/**
 * Layer status
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
static void set_layer_status(struct zmk_widget_screen *widget, struct layer_status_state state) {
    const nice_oled_dirty_mask_t dirty =
        nice_oled_central_apply_layer(&widget->central, state.index, state.label);

    if (dirty == NICE_OLED_DIRTY_NONE) {
        return;
    }

    widget->central.dirty |= dirty;
    nice_oled_screen_central_redraw(&widget->compositor);
}

static void layer_status_update_cb(struct layer_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_layer_status(widget, state); }
}

static struct layer_status_state layer_status_get_state(const zmk_event_t *eh) {
    uint8_t index = zmk_keymap_highest_layer_active();
    return (struct layer_status_state){.index = index, .label = zmk_keymap_layer_name(index)};
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_layer_status, struct layer_status_state, layer_status_update_cb,
                            layer_status_get_state)

ZMK_SUBSCRIPTION(widget_layer_status, zmk_layer_state_changed);
#endif

/**
 * Output status
 **/

static void set_output_status(struct zmk_widget_screen *widget,
                              const struct output_status_state *state) {
    const nice_oled_dirty_mask_t dirty = nice_oled_central_apply_output(
        &widget->central, &state->selected_endpoint, state->active_profile_index,
        state->active_profile_connected, state->active_profile_bonded);

    if (dirty == NICE_OLED_DIRTY_NONE) {
        return;
    }

    widget->central.dirty |= dirty;
    nice_oled_screen_central_redraw(&widget->compositor);
}

static void output_status_update_cb(struct output_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_output_status(widget, &state); }
}

static struct output_status_state output_status_get_state(const zmk_event_t *_eh) {
    return (struct output_status_state){
        .selected_endpoint = zmk_endpoints_selected(),
        .active_profile_index = zmk_ble_active_profile_index(),
        .active_profile_connected = zmk_ble_active_profile_is_connected(),
        .active_profile_bonded = !zmk_ble_active_profile_is_open(),
    };
}

ZMK_DISPLAY_WIDGET_LISTENER(widget_output_status, struct output_status_state,
                            output_status_update_cb, output_status_get_state)
ZMK_SUBSCRIPTION(widget_output_status, zmk_endpoint_changed);

#if IS_ENABLED(CONFIG_USB_DEVICE_STACK)
ZMK_SUBSCRIPTION(widget_output_status, zmk_usb_conn_state_changed);
#endif
#if defined(CONFIG_ZMK_BLE)
ZMK_SUBSCRIPTION(widget_output_status, zmk_ble_active_profile_changed);
#endif

/**
 * WPM status
 **/

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
static void set_wpm_status(struct zmk_widget_screen *widget, struct wpm_status_state state) {
    const nice_oled_dirty_mask_t dirty =
        nice_oled_central_apply_wpm(&widget->central, state.wpm);

    widget->central.dirty |= dirty;
    nice_oled_screen_central_redraw(&widget->compositor);
}

static void wpm_status_update_cb(struct wpm_status_state state) {
    struct zmk_widget_screen *widget;
    SYS_SLIST_FOR_EACH_CONTAINER(&widgets, widget, node) { set_wpm_status(widget, state); }
}

struct wpm_status_state wpm_status_get_state(const zmk_event_t *eh) {
    return (struct wpm_status_state){.wpm = zmk_wpm_get_state()};
};

ZMK_DISPLAY_WIDGET_LISTENER(widget_wpm_status, struct wpm_status_state, wpm_status_update_cb,
                            wpm_status_get_state)
ZMK_SUBSCRIPTION(widget_wpm_status, zmk_wpm_state_changed);
#endif // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)

/**
 * Initialization
 **/

int zmk_widget_screen_init(struct zmk_widget_screen *widget, lv_obj_t *parent) {
    widget->obj = lv_obj_create(parent);
    lv_obj_set_size(widget->obj, CANVAS_HEIGHT, CANVAS_WIDTH);
    nice_oled_central_state_init(&widget->central);

    if (nice_oled_screen_central_init(&widget->compositor, widget->obj) != 0) {
        return -1;
    }
    lv_obj_t *canvas = widget->compositor.canvas;

    sys_slist_append(&widgets, &widget->node);

    widget_battery_status_init();

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_LAYER)
    widget_layer_status_init();
#endif
    widget_output_status_init();
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)
    widget_wpm_status_init();
#endif // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
    zmk_widget_luna_init(&luna_widget, canvas);
    lv_obj_align(zmk_widget_luna_obj(&luna_widget), LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_LUNA_CUSTOM_Y);
       // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_LUNA)
#elif IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
    zmk_widget_wpm_bongo_cat_init(&wpm_bongo_cat_widget, canvas);
    lv_obj_align(zmk_widget_wpm_bongo_cat_obj(&wpm_bongo_cat_widget), LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_BONGO_CAT_CUSTOM_Y);
       // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM_BONGO_CAT)
#endif

#endif // IS_ENABLED(CONFIG_NICE_OLED_WIDGET_WPM)

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT)
    zmk_widget_responsive_bongo_cat_init(&responsive_bongo_cat_widget, canvas);
    lv_obj_align(zmk_widget_responsive_bongo_cat_obj(&responsive_bongo_cat_widget),
                 LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_RESPONSIVE_BONGO_CAT_CUSTOM_Y);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_HID_INDICATORS)
    zmk_widget_hid_indicators_init(&hid_indicators_widget, canvas);
#endif

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_MODIFIERS_INDICATORS_LUNA)
    zmk_widget_modifiers_init(&modifiers_widget, canvas); // Inicializar el widget de modifiers
#endif

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_WEATHER)
    raw_hid_label_init_weather(canvas);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_TIME)
    raw_hid_label_init_time(canvas);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_VOLUME)
    raw_hid_label_init_volume(canvas);
#endif
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
    raw_hid_label_init_layout(canvas);
#endif
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    raw_hid_label_init_media_player(canvas);
#endif
#endif // CONFIG_NICE_OLED_WIDGET_RAW_HID

    // tiene que estar siempre al final por la sobre exposicion!!
#if IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_IDLE) ||                                         \
    IS_ENABLED(CONFIG_NICE_OLED_SHOW_SLEEP_ART_ON_SLEEP)
    zmk_widget_sleep_status_init(&sleep_status_widget, canvas);
    lv_obj_align(zmk_widget_sleep_status_obj(&sleep_status_widget), LV_ALIGN_TOP_LEFT, CONFIG_NICE_OLED_WIDGET_SLEEP_STATUS_CUSTOM_X, CONFIG_NICE_OLED_WIDGET_SLEEP_STATUS_CUSTOM_Y);
#endif

    return 0;
}

lv_obj_t *zmk_widget_screen_obj(struct zmk_widget_screen *widget) { return widget->obj; }
