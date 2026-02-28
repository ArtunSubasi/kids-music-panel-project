#include "music_assistant_controller.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "input/buttons.h"
#include "music_assistant/music_assistant_client.h"

static const char *TAG = "MUSIC_ASSISTANT_CTRL";

#define COMMAND_QUEUE_SIZE 10
#define WORKER_TASK_STACK_SIZE 8192
#define WORKER_TASK_PRIORITY 5

typedef enum {
    MA_CMD_PREVIOUS_TRACK,
    MA_CMD_PLAY_PAUSE,
    MA_CMD_NEXT_TRACK,
    MA_CMD_PLAY_MEDIA,
    MA_CMD_SEEK_TO_POSITION,
} ma_command_type_t;

typedef struct {
    ma_command_type_t type;
    union {
        char media_id[128];  // for MA_CMD_PLAY_MEDIA
        float seek_position; // for MA_CMD_SEEK_TO_POSITION
    };
} ma_command_t;

static bool s_handlers_registered = false;
static QueueHandle_t s_command_queue = NULL;
static TaskHandle_t s_worker_task_handle = NULL;

// Seek debounce state
static TimerHandle_t s_seek_debounce_timer = NULL;
static int s_seek_forward_clicks = 0;
static int s_seek_backward_clicks = 0;
static float s_initial_position = 0.0f;

#define SEEK_DEBOUNCE_MS 500
#define SEEK_SECONDS_PER_CLICK 10

static void music_assistant_worker_task(void *arg)
{
    (void)arg;
    ma_command_t cmd;

    ESP_LOGI(TAG, "Worker task started");

    while (1) {
        if (xQueueReceive(s_command_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            esp_err_t err = ESP_OK;

            switch (cmd.type) {
                case MA_CMD_PREVIOUS_TRACK:
                    err = music_assistant_previous_track();
                    break;
                case MA_CMD_PLAY_PAUSE:
                    err = music_assistant_play_pause();
                    break;
                case MA_CMD_NEXT_TRACK:
                    err = music_assistant_next_track();
                    break;
                case MA_CMD_PLAY_MEDIA:
                    err = music_assistant_play_media(cmd.media_id);
                    break;
                case MA_CMD_SEEK_TO_POSITION:
                    err = music_assistant_seek_to_position(cmd.seek_position);
                    break;
                default:
                    ESP_LOGW(TAG, "Unknown command type: %d", cmd.type);
                    continue;
            }

            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Failed to execute command type=%d: %s", cmd.type, esp_err_to_name(err));
            }
        }
    }
}

static void seek_debounce_timer_callback(TimerHandle_t timer)
{
    (void)timer;
    
    int forward_clicks = s_seek_forward_clicks;
    int backward_clicks = s_seek_backward_clicks;
    float initial_pos = s_initial_position;
    
    // Reset counters
    s_seek_forward_clicks = 0;
    s_seek_backward_clicks = 0;
    s_initial_position = 0.0f;
    
    // Calculate net offset
    int net_clicks = forward_clicks - backward_clicks;
    if (net_clicks == 0) {
        ESP_LOGW(TAG, "Seek cancelled (forward=%d, backward=%d)", forward_clicks, backward_clicks);
        return;
    }
    
    float offset = net_clicks * SEEK_SECONDS_PER_CLICK;
    float new_position = initial_pos + offset;
    
    if (new_position < 0) {
        new_position = 0;
    }
    
    ESP_LOGI(TAG, "Seeking: initial=%.1fs, clicks=%d, offset=%.1fs, target=%.1fs",
             initial_pos, net_clicks, offset, new_position);
    
    // Queue seek command to worker task (don't make HTTP call from timer context)
    ma_command_t cmd = {0};
    cmd.type = MA_CMD_SEEK_TO_POSITION;
    cmd.seek_position = new_position;
    
    if (xQueueSend(s_command_queue, &cmd, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue seek command (queue full)");
    }
}

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

    ma_command_t cmd = {0};
    
    switch ((buttons_event_id_t)event_id) {
        case BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED:
            cmd.type = MA_CMD_PREVIOUS_TRACK;
            break;
        case BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED:
            cmd.type = MA_CMD_PLAY_PAUSE;
            break;
        case BUTTON_EVENT_ID_NEXT_TRACK_PRESSED:
            cmd.type = MA_CMD_NEXT_TRACK;
            break;
        case BUTTON_EVENT_ID_SEEK_FORWARD_PRESSED:
        case BUTTON_EVENT_ID_SEEK_BACKWARD_PRESSED:
            {
                // Check if this is the first click
                bool is_first_click = (s_seek_forward_clicks == 0 && s_seek_backward_clicks == 0);
                
                if (is_first_click) {
                    // Fetch current position
                    esp_err_t err = music_assistant_get_media_position(&s_initial_position);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to get media position, using 0");
                        s_initial_position = 0.0f;
                    }
                }
                
                // Increment appropriate counter
                if (event_id == BUTTON_EVENT_ID_SEEK_FORWARD_PRESSED) {
                    s_seek_forward_clicks++;
                    ESP_LOGI(TAG, "Seek forward click %d", s_seek_forward_clicks);
                } else {
                    s_seek_backward_clicks++;
                    ESP_LOGI(TAG, "Seek backward click %d", s_seek_backward_clicks);
                }
                
                // Start or reset debounce timer
                if (s_seek_debounce_timer != NULL) {
                    xTimerReset(s_seek_debounce_timer, 0);
                } else {
                    ESP_LOGW(TAG, "Seek debounce timer is NULL");
                }
                
                return;  // Don't queue command, timer callback will handle seek
            }
        default:
            ESP_LOGW(TAG, "Unsupported button event id=%ld", (long)event_id);
            return;
    }

    // Post command to queue (non-blocking with short timeout)
    if (xQueueSend(s_command_queue, &cmd, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Failed to queue command type=%d (queue full)", cmd.type);
    }
}

esp_err_t music_assistant_controller_init(void)
{
    if (s_handlers_registered) {
        return ESP_OK;
    }

    // Create seek debounce timer
    s_seek_debounce_timer = xTimerCreate(
        "seek_debounce",
        pdMS_TO_TICKS(SEEK_DEBOUNCE_MS),
        pdFALSE,  // One-shot timer
        NULL,
        seek_debounce_timer_callback
    );
    
    if (s_seek_debounce_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create seek debounce timer");
        return ESP_ERR_NO_MEM;
    }

    // Create command queue
    s_command_queue = xQueueCreate(COMMAND_QUEUE_SIZE, sizeof(ma_command_t));
    if (s_command_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create command queue");
        return ESP_ERR_NO_MEM;
    }

    // Create worker task
    BaseType_t ret = xTaskCreate(
        music_assistant_worker_task,
        "ma_worker",
        WORKER_TASK_STACK_SIZE,
        NULL,
        WORKER_TASK_PRIORITY,
        &s_worker_task_handle
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create worker task");
        vQueueDelete(s_command_queue);
        s_command_queue = NULL;
        return ESP_ERR_NO_MEM;
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

    ESP_ERROR_CHECK(buttons_subscribe(
        BUTTON_EVENT_ID_SEEK_FORWARD_PRESSED,
        music_assistant_button_event_handler,
        NULL
    ));

    ESP_ERROR_CHECK(buttons_subscribe(
        BUTTON_EVENT_ID_SEEK_BACKWARD_PRESSED,
        music_assistant_button_event_handler,
        NULL
    ));

    s_handlers_registered = true;
    ESP_LOGI(TAG, "Music Assistant controller initialized (queue=%d, stack=%d)", 
             COMMAND_QUEUE_SIZE, WORKER_TASK_STACK_SIZE);
    return ESP_OK;
}
