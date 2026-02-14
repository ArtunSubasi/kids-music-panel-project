#include "music_assistant_controller.h"

#include <stdbool.h>

#include "esp_log.h"
#include "input/buttons.h"
#include "music_assistant/music_assistant_client.h"

static const char *TAG = "MUSIC_ASSISTANT_CTRL";

static bool s_handlers_registered = false;

static void music_assistant_button_event_handler(void *arg,
                                                 esp_event_base_t event_base,
                                                 int32_t event_id,
                                                 void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base != BUTTON_EVENT) {
        return;
    }

    esp_err_t err = ESP_OK;
    switch ((buttons_event_id_t)event_id) {
        case BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED:
            err = music_assistant_previous_track();
            break;
        case BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED:
            err = music_assistant_play_pause();
            break;
        case BUTTON_EVENT_ID_NEXT_TRACK_PRESSED:
            err = music_assistant_next_track();
            break;
        default:
            ESP_LOGW(TAG, "Unsupported button event id=%ld", (long)event_id);
            return;
    }

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to handle button event id=%ld: %s", (long)event_id, esp_err_to_name(err));
    }
}

esp_err_t music_assistant_controller_init(void)
{
    if (s_handlers_registered) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(buttons_subscribe(
        BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED,
        music_assistant_button_event_handler,
        NULL
    ));

    ESP_ERROR_CHECK(buttons_subscribe(
        BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED,
        music_assistant_button_event_handler,
        NULL
    ));

    ESP_ERROR_CHECK(buttons_subscribe(
        BUTTON_EVENT_ID_NEXT_TRACK_PRESSED,
        music_assistant_button_event_handler,
        NULL
    ));

    s_handlers_registered = true;
    ESP_LOGI(TAG, "Music Assistant controller initialized");
    return ESP_OK;
}
