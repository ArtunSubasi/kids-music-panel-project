#include "buttons.h"
#include "board_pins.h"
#include "music_assistant/music_assistant_client.h"

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

/* Volume constants */
#define VOLUME_UPDATE_DEBOUNCE_MS 200  /* Min time between volume updates */

static const char *TAG = "BUTTONS";

ESP_EVENT_DEFINE_BASE(BUTTON_EVENT);

/* Rotary encoder volume control state */
static volatile int s_accumulated_steps = 0;  /* Accumulated encoder steps (negative = down, positive = up) */
static TimerHandle_t s_volume_update_timer = NULL;  /* Timer for deferred volume updates */
static TaskHandle_t s_volume_update_task = NULL;  /* Task for sending volume updates */

/* Debug: last seen encoder state for logging changes */
static uint8_t s_last_seen_a = 1;
static uint8_t s_last_seen_b = 1;

typedef struct {
    int pin;
    buttons_event_id_t event_id;
} button_pin_map_t;

static const button_pin_map_t s_button_pin_map[] = {
    { BOARD_BUTTON_ROTARY_LEFT,    BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED },
    { BOARD_BUTTON_ROTARY_CENTER,  BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED     },
    { BOARD_BUTTON_ROTARY_RIGHT,   BUTTON_EVENT_ID_NEXT_TRACK_PRESSED     },
};

static esp_event_loop_handle_t s_button_loop = NULL;

static bool get_button_event_id_for_pin(int pin, buttons_event_id_t *out_event_id)
{
    if (out_event_id == NULL) {
        return false;
    }

    for (size_t i = 0; i < (sizeof(s_button_pin_map) / sizeof(s_button_pin_map[0])); i++) {
        if (s_button_pin_map[i].pin == pin) {
            *out_event_id = s_button_pin_map[i].event_id;
            return true;
        }
    }
    return false;
}

static const char *event_id_to_name(buttons_event_id_t id)
{
    switch (id) {
        case BUTTON_EVENT_ID_PREVIOUS_TRACK_PRESSED:    return "PREVIOUS_TRACK_BUTTON_PRESSED";
        case BUTTON_EVENT_ID_PLAY_PAUSE_PRESSED:        return "PLAY_PAUSE_BUTTON_PRESSED";
        case BUTTON_EVENT_ID_NEXT_TRACK_PRESSED:        return "NEXT_TRACK_BUTTON_PRESSED";
        default:                                        return "UNKNOWN_BUTTON_EVENT";
    }
}

static void button_event_log_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;

    if (base != BUTTON_EVENT) {
        return;
    }

    buttons_event_id_t event_id = (buttons_event_id_t)id;
    int pin = -1;

    if (event_data != NULL) {
        const buttons_event_data_t *event = (const buttons_event_data_t *)event_data;
        pin = event->pin;
        if (event->button_id != event_id) {
            ESP_LOGW(TAG, "Mismatched button event payload id=%d payload=%d", (int)event_id, (int)event->button_id);
        }
    }

    ESP_LOGI(TAG, "Event: %s (id=%" PRId32 "), pin=%d", event_id_to_name(event_id), id, pin);
}

/**
 * @brief Task to handle volume updates with sufficient stack space for HTTP calls
 */
static void volume_update_task(void *pvParameters)
{
    (void)pvParameters;
    
    while (1) {
        /* Wait for notification from timer callback */
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        /* Get accumulated steps and reset counter */
        int steps = s_accumulated_steps;
        s_accumulated_steps = 0;
        
        ESP_LOGI(TAG, "Encoder accumulated steps: %d", steps);
        
        /* Send volume updates to Music Assistant - one call per accumulated step */
        if (steps > 0) {
            ESP_LOGI(TAG, "Volume UP (%d steps)", steps);
            for (int i = 0; i < steps; i++) {
                esp_err_t err = music_assistant_volume_up();
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to increase volume on step %d: %s", i+1, esp_err_to_name(err));
                }
            }
        } else if (steps < 0) {
            ESP_LOGI(TAG, "Volume DOWN (%d steps)", -steps);
            for (int i = 0; i < -steps; i++) {
                esp_err_t err = music_assistant_volume_down();
                if (err != ESP_OK) {
                    ESP_LOGW(TAG, "Failed to decrease volume on step %d: %s", i+1, esp_err_to_name(err));
                }
            }
        }
    }
}

/**
 * @brief Timer callback to trigger volume update task
 */
static void volume_update_timer_cb(TimerHandle_t xTimer)
{
    (void)xTimer;
    
    /* Notify the volume update task to send the update */
    if (s_volume_update_task != NULL) {
        xTaskNotifyGive(s_volume_update_task);
    }
}

/**
 * @brief Debug task to poll encoder pins and log any changes
 * This helps diagnose if the hardware is working
 */
static void encoder_debug_task(void *pvParameters)
{
    (void)pvParameters;
    
    ESP_LOGI(TAG, "Encoder debug task started - will log pin changes every 100ms");
    
    while (1) {
        uint8_t a = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_A);
        uint8_t b = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_B);
        
        if (a != s_last_seen_a || b != s_last_seen_b) {
            ESP_LOGI(TAG, "PIN CHANGE: A=%d->%d, B=%d->%d", s_last_seen_a, a, s_last_seen_b, b);
            s_last_seen_a = a;
            s_last_seen_b = b;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/**
 * @brief Rotary encoder ISR handler
 * 
 * Simplified quadrature decoding - triggers on any edge of PHASE A,
 * samples PHASE B to determine direction
 */
static void IRAM_ATTR rotary_encoder_isr_handler(void *args)
{
    (void)args;
    
    /* Read both phases */
    uint8_t phase_a = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_A);
    uint8_t phase_b = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_B);
    
    /* Determine direction: when A falls (goes to 0), check B */
    if (phase_a == 0) {
        /* A just went low - if B is low = CW, if B is high = CCW */
        int8_t direction = (phase_b == 0) ? 1 : -1;
        s_accumulated_steps += direction;
    } else {
        /* A just went high - if B is high = CW, if B is low = CCW */
        int8_t direction = (phase_b == 1) ? 1 : -1;
        s_accumulated_steps += direction;
    }
    
    /* Always trigger the timer on any transition */
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (s_volume_update_timer != NULL) {
        xTimerResetFromISR(s_volume_update_timer, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR();
        }
    }
}

static void debounce_timer_cb(TimerHandle_t xTimer)
{
    int pinNumber = (int)(intptr_t)pvTimerGetTimerID(xTimer);

    // Pull-up + active-low button: pressed only if still LOW after debounce
    if (gpio_get_level(pinNumber) == 0 && s_button_loop != NULL) {
        buttons_event_id_t event_id;
        if (!get_button_event_id_for_pin(pinNumber, &event_id)) {
            ESP_LOGW(TAG, "No event mapping found for GPIO %d", pinNumber);
            return;
        }

        buttons_event_data_t event_data = {
            .pin = pinNumber,
            .button_id = event_id,
        };

        esp_err_t err = esp_event_post_to(
            s_button_loop,
            BUTTON_EVENT,
            event_id,
            &event_data,
            sizeof(event_data),
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

esp_err_t buttons_init(void)
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

    // Initialize rotary encoder for volume control
    ESP_LOGI(TAG, "Initializing rotary encoder on GPIO %d (PHASE A) and GPIO %d (PHASE B)",
             BOARD_BUTTON_ROTARY_PHASE_A, BOARD_BUTTON_ROTARY_PHASE_B);
    
    /* Create volume update task with sufficient stack for HTTP calls */
    BaseType_t ret = xTaskCreate(
        volume_update_task,
        "vol_update",
        4096,  /* Stack size - sufficient for HTTP operations */
        NULL,
        5,     /* Priority */
        &s_volume_update_task
    );
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create volume update task");
        return ESP_FAIL;
    }
    
    /* Create volume update timer */
    s_volume_update_timer = xTimerCreate(
        "vol_timer",
        pdMS_TO_TICKS(VOLUME_UPDATE_DEBOUNCE_MS),
        pdFALSE,  /* One-shot timer */
        NULL,
        volume_update_timer_cb
    );
    if (s_volume_update_timer == NULL) {
        ESP_LOGE(TAG, "Failed to create volume update timer");
        return ESP_FAIL;
    }
    
    /* Configure PHASE A with interrupt on any edge */
    gpio_config_t encoder_phase_a_config = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_ROTARY_PHASE_A),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE  /* Trigger on both rising and falling edges */
    };
    ESP_ERROR_CHECK(gpio_config(&encoder_phase_a_config));
    ESP_LOGI(TAG, "PHASE A (GPIO %d) configured with pull-up, interrupt on any edge", BOARD_BUTTON_ROTARY_PHASE_A);
    
    /* Configure PHASE B as input only (no interrupt) */
    gpio_config_t encoder_phase_b_config = {
        .pin_bit_mask = (1ULL << BOARD_BUTTON_ROTARY_PHASE_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE  /* No interrupt, just sampled */
    };
    ESP_ERROR_CHECK(gpio_config(&encoder_phase_b_config));
    ESP_LOGI(TAG, "PHASE B (GPIO %d) configured with pull-up, no interrupt", BOARD_BUTTON_ROTARY_PHASE_B);
    
    /* Read initial pin states */
    uint8_t initial_a = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_A);
    uint8_t initial_b = gpio_get_level(BOARD_BUTTON_ROTARY_PHASE_B);
    ESP_LOGI(TAG, "Initial encoder state: A=%d, B=%d", initial_a, initial_b);
    s_last_seen_a = initial_a;
    s_last_seen_b = initial_b;
    
    /* Create debug polling task */
    ret = xTaskCreate(
        encoder_debug_task,
        "enc_debug",
        2048,
        NULL,
        3,
        NULL
    );
    if (ret != pdPASS) {
        ESP_LOGW(TAG, "Failed to create encoder debug task (non-critical)");
    }
    
    /* Attach ISR handler only to PHASE A */
    ESP_ERROR_CHECK(gpio_isr_handler_add(BOARD_BUTTON_ROTARY_PHASE_A, rotary_encoder_isr_handler, NULL));
    ESP_LOGI(TAG, "ISR handler attached to PHASE A");
    
    ESP_LOGI(TAG, "Rotary encoder initialized for relative volume control");

    return ESP_OK;
}

esp_err_t buttons_subscribe(int32_t event_id, esp_event_handler_t handler, void *handler_arg)
{
    if (s_button_loop == NULL) {
        ESP_LOGE(TAG, "buttons_init() must be called before buttons_subscribe()");
        return ESP_ERR_INVALID_STATE;
    }

    if (handler == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    return esp_event_handler_register_with(
        s_button_loop,
        BUTTON_EVENT,
        event_id,
        handler,
        handler_arg
    );
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
