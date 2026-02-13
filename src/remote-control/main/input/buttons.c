#include "buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/gpio.h"

#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>

#define DEBOUNCE_MS 50

static const char *TAG = "BUTTONS";

ESP_EVENT_DEFINE_BASE(BUTTON_EVENT);

// Event IDs inside BUTTON_EVENT base
typedef enum {
    PREVIOUS_TRACK_BUTTON_PRESSED = 1,
    PLAY_PAUSE_BUTTON_PRESSED = 2,
    NEXT_TRACK_BUTTON_PRESSED = 3
} button_event_id_t;

typedef struct {
    int pin;
    button_event_id_t event_id;
} button_pin_map_t;

static const button_pin_map_t s_button_pin_map[] = {
    { PIN_PREVIOUS_TRACK, PREVIOUS_TRACK_BUTTON_PRESSED },
    { PIN_PLAY_PAUSE,     PLAY_PAUSE_BUTTON_PRESSED     },
    { PIN_NEXT_TRACK,     NEXT_TRACK_BUTTON_PRESSED     },
};

static esp_event_loop_handle_t s_button_loop = NULL;

static bool get_button_event_id_for_pin(int pin, button_event_id_t *out_event_id)
{
    for (size_t i = 0; i < (sizeof(s_button_pin_map) / sizeof(s_button_pin_map[0])); i++) {
        if (s_button_pin_map[i].pin == pin) {
            *out_event_id = s_button_pin_map[i].event_id;
            return true;
        }
    }
    return false;
}

static const char *event_id_to_name(button_event_id_t id)
{
    switch (id) {
        case PREVIOUS_TRACK_BUTTON_PRESSED: return "PREVIOUS_TRACK_BUTTON_PRESSED";
        case PLAY_PAUSE_BUTTON_PRESSED:     return "PLAY_PAUSE_BUTTON_PRESSED";
        case NEXT_TRACK_BUTTON_PRESSED:     return "NEXT_TRACK_BUTTON_PRESSED";
        default:                            return "UNKNOWN_BUTTON_EVENT";
    }
}

static void button_event_log_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;

    if (base != BUTTON_EVENT) {
        return;
    }

    button_event_id_t event_id = (button_event_id_t)id;
    int pin = (event_data != NULL) ? *(int *)event_data : -1;

    ESP_LOGI(TAG, "Event: %s (id=%" PRId32 "), pin=%d", event_id_to_name(event_id), id, pin);
}

static void debounce_timer_cb(TimerHandle_t xTimer)
{
    int pinNumber = (int)(intptr_t)pvTimerGetTimerID(xTimer);

    // Pull-up + active-low button: pressed only if still LOW after debounce
    if (gpio_get_level(pinNumber) == 0 && s_button_loop != NULL) {
        button_event_id_t event_id;
        if (!get_button_event_id_for_pin(pinNumber, &event_id)) {
            ESP_LOGW(TAG, "No event mapping found for GPIO %d", pinNumber);
            return;
        }

        esp_err_t err = esp_event_post_to(
            s_button_loop,
            BUTTON_EVENT,
            event_id,
            &pinNumber,
            sizeof(pinNumber),
            0
        );
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to post button event for GPIO %d: %s", pinNumber, esp_err_to_name(err));
        }
    }
}

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    TimerHandle_t debounce_timer = (TimerHandle_t)args;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(debounce_timer, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

esp_err_t buttons_init()
{
    if (s_button_loop != NULL) {
        return ESP_OK; // already initialized
    }

    esp_event_loop_args_t loop_with_task_args = {
        .queue_size = 10,
        .task_name = "button_event_loop_task",
        .task_priority = uxTaskPriorityGet(NULL),
        .task_stack_size = 3072,
        .task_core_id = tskNO_AFFINITY
    };

    ESP_ERROR_CHECK(esp_event_loop_create(&loop_with_task_args, &s_button_loop));

    ESP_ERROR_CHECK(esp_event_handler_register_with(
        s_button_loop,
        BUTTON_EVENT,
        ESP_EVENT_ANY_ID,
        button_event_log_handler,
        NULL
    ));

    ESP_ERROR_CHECK(gpio_install_isr_service(0));

    ESP_LOGI(TAG, "Buttons module initialized successfully");

    // Register all buttons declared in the pin->event map
    for (size_t i = 0; i < (sizeof(s_button_pin_map) / sizeof(s_button_pin_map[0])); i++) {
        ESP_ERROR_CHECK(buttons_register(s_button_pin_map[i].pin));
    }

    return ESP_OK;
}

esp_err_t buttons_register(int pinNumber)
{
    if (s_button_loop == NULL) {
        ESP_LOGE(TAG, "buttons_init() must be called before buttons_register()");
        return ESP_ERR_INVALID_STATE;
    }

    gpio_config_t io_conf_pullup_enabled = {
        .pin_bit_mask = (1ULL << pinNumber),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_NEGEDGE
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_pullup_enabled));

    TimerHandle_t debounce_timer = xTimerCreate(
        "btn_dbnc",
        pdMS_TO_TICKS(DEBOUNCE_MS),
        pdFALSE,
        (void *)(intptr_t)pinNumber,
        debounce_timer_cb
    );
    if (debounce_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create debounce timer for GPIO %d", pinNumber);
        return ESP_FAIL;
    }

    ESP_ERROR_CHECK(gpio_isr_handler_add(pinNumber, gpio_interrupt_handler, (void *)debounce_timer));
    ESP_LOGI(TAG, "Button %d registered", pinNumber);
    return ESP_OK;
}
