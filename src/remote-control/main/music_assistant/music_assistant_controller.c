#include "music_assistant_controller.h"

#include <stdbool.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
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
} ma_command_type_t;

typedef struct {
    ma_command_type_t type;
    char media_id[128]; // only used for MA_CMD_PLAY_MEDIA
} ma_command_t;

static bool s_handlers_registered = false;
static QueueHandle_t s_command_queue = NULL;
static TaskHandle_t s_worker_task_handle = NULL;

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

    s_handlers_registered = true;
    ESP_LOGI(TAG, "Music Assistant controller initialized (queue=%d, stack=%d)", 
             COMMAND_QUEUE_SIZE, WORKER_TASK_STACK_SIZE);
    return ESP_OK;
}
