#include "buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_err.h"
#include "driver/gpio.h"

#include <stdint.h>

#define DEBOUNCE_MS 50

static const char *TAG = "BUTTONS";

ESP_EVENT_DEFINE_BASE(BUTTON_EVENT);

// Event IDs inside BUTTON_EVENT base
typedef enum {
    BUTTON_EVENT_PRESSED = 1,
} button_event_id_t;

static esp_event_loop_handle_t s_button_loop = NULL;

static void button_event_log_handler(void *arg, esp_event_base_t base, int32_t id, void *event_data)
{
    (void)arg;
    if (base == BUTTON_EVENT && id == BUTTON_EVENT_PRESSED && event_data != NULL) {
        int pin = *(int *)event_data;
        ESP_LOGI(TAG, "GPIO %d was pressed", pin);
    }
}

static void debounce_timer_cb(TimerHandle_t xTimer)
{
    int pinNumber = (int)(intptr_t)pvTimerGetTimerID(xTimer);

    // Pull-up + active-low button: pressed only if still LOW after debounce
    if (gpio_get_level(pinNumber) == 0 && s_button_loop != NULL) {
        esp_err_t err = esp_event_post_to(
            s_button_loop,
            BUTTON_EVENT,
            BUTTON_EVENT_PRESSED,
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

    esp_err_t isr_err = gpio_install_isr_service(0);
    if (isr_err != ESP_OK && isr_err != ESP_ERR_INVALID_STATE) {
        return isr_err;
    }

    ESP_LOGI(TAG, "Buttons initialized successfully");
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
