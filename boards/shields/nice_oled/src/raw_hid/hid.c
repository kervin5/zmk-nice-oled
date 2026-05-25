#include <raw_hid/hid.h>
#include <raw_hid/events.h>
#include <raw_hid/protocol_decode.h>

#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

ZMK_EVENT_IMPL(is_connected_notification);
ZMK_EVENT_IMPL(time_notification);
ZMK_EVENT_IMPL(volume_notification);
ZMK_EVENT_IMPL(weather_notification);
#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
ZMK_EVENT_IMPL(spotify_notification);
#endif
#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
ZMK_EVENT_IMPL(layout_notification);
#endif

static bool is_connected = false;

static void on_disconnect_timer(struct k_timer *dummy) {
    LOG_INF("raise_connection_notification: false");
    is_connected = false;
    raise_is_connected_notification((struct is_connected_notification){.value = false});
}

K_TIMER_DEFINE(disconnect_timer, on_disconnect_timer, NULL);

static uint8_t last_hid_volume = 0;
static uint8_t last_raised_volume = 0;

static void on_volume_timer(struct k_timer *dummy) {
    if (last_raised_volume != last_hid_volume) {
        last_raised_volume = last_hid_volume;
        LOG_INF("raise_volume_notification %i", last_hid_volume);
        raise_volume_notification((struct volume_notification){.value = last_hid_volume});
    }
}

K_TIMER_DEFINE(volume_timer, on_volume_timer, NULL);

static void process_raw_hid_data(uint8_t *data, uint8_t length) {
    struct nice_oled_raw_hid_message message;
    if (!nice_oled_raw_hid_decode(data, length, &message)) {
        LOG_WRN("Invalid RAW HID packet");
        return;
    }

    LOG_INF("display_process_raw_hid_data - received data_type %u", (unsigned)message.kind);

    k_timer_start(&disconnect_timer, K_SECONDS(65), K_NO_WAIT);
    if (!is_connected) {
        LOG_INF("raise_connection_notification: true");
        is_connected = true;
        raise_is_connected_notification((struct is_connected_notification){.value = true});
    }

    switch (message.kind) {
    case NICE_OLED_RAW_HID_TIME:
        raise_time_notification((struct time_notification){
            .hour = message.time.hour, .minute = message.time.minute});
        break;

    case NICE_OLED_RAW_HID_VOLUME:
        last_hid_volume = message.volume.value;
        if (k_timer_status_get(&volume_timer) > 0 || k_timer_remaining_get(&volume_timer) == 0) {
            k_timer_start(&volume_timer, K_MSEC(150), K_NO_WAIT);
            on_volume_timer(&volume_timer);
        }
        break;

#ifdef CONFIG_NICE_OLED_WIDGET_RAW_HID_LAYOUT
    case NICE_OLED_RAW_HID_LAYOUT:
        raise_layout_notification((struct layout_notification){.value = message.layout.value});
        break;
#endif

    case NICE_OLED_RAW_HID_WEATHER:
        raise_weather_notification((struct weather_notification){
            .temperature = message.weather.temperature});
        break;

#if IS_ENABLED(CONFIG_NICE_OLED_WIDGET_RAW_HID_MEDIA_PLAYER_SPOTIFY_MACOS)
    case NICE_OLED_RAW_HID_SPOTIFY: {
        struct spotify_notification notification;
        memcpy(notification.media_player, message.spotify.media_player,
               sizeof(notification.media_player));
        notification.media_player[sizeof(notification.media_player) - 1] = '\0';
        raise_spotify_notification(notification);
        break;
    }
#endif

    default:
        break;
    }
}

static int raw_hid_received_event_listener(const zmk_event_t *eh) {
    struct raw_hid_received_event *event = as_raw_hid_received_event(eh);
    if (event) {
        process_raw_hid_data(event->data, event->length);
    }

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(process_raw_hid_event, raw_hid_received_event_listener);
ZMK_SUBSCRIPTION(process_raw_hid_event, raw_hid_received_event);
