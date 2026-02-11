#include "buttons.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "ssd1306.h"
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"

#include <string.h>
#include <stdint.h>

#define PULLUP_ENABLED_PIN 22
#define DEBOUNCE_MS 50

static const char *TAG = "BUTTONS";

static QueueHandle_t interruptQueue = NULL;

static void debounce_timer_cb(TimerHandle_t xTimer)
{
    int pinNumber = (int)(intptr_t)pvTimerGetTimerID(xTimer);
    // For pull-up + active-low button: only enqueue if still LOW (pressed)
    int level = gpio_get_level(pinNumber);
    if (level == 0) {
        xQueueSend(interruptQueue, &pinNumber, 0);
    }
}

static void IRAM_ATTR gpio_interrupt_handler(void *args)
{
    TimerHandle_t debounce_timer = (TimerHandle_t)args;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(debounce_timer, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

static void LED_Control_Task(void *params)
{
    int pinNumber = (int)(intptr_t)params;
    while (true)
    {
        if (xQueueReceive(interruptQueue, &pinNumber, portMAX_DELAY))
        {
            printf("GPIO %d was pressed\n", pinNumber);
        }
    }
}

esp_err_t buttons_init() {
    // Create queue for interrupt handling
    interruptQueue = xQueueCreate(10, sizeof(int));
    if (interruptQueue == NULL) {
        printf("Failed to create interrupt queue\n");
        return ESP_FAIL;
    }

    // Register interrupt handler
    gpio_install_isr_service(0);

    ESP_LOGI(TAG, "Buttons initialized successfully");
    return ESP_OK;
}

esp_err_t buttons_register(int pinNumber) {
    // Configure Input GPIO with Pull-up
    gpio_config_t io_conf_pullup_enabled = {
        .pin_bit_mask = (1ULL << pinNumber),    // Select GPIO 4
        .mode = GPIO_MODE_INPUT,                // Set as input
        .pull_up_en = GPIO_PULLUP_ENABLE,       // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable pull-down
        .intr_type = GPIO_INTR_NEGEDGE        
    };
    gpio_config(&io_conf_pullup_enabled);

    // Create a one-shot FreeRTOS timer for debounce. ISR will reset/start it.
    TimerHandle_t debounce_timer = xTimerCreate("debounce", pdMS_TO_TICKS(DEBOUNCE_MS), pdFALSE, (void*)(intptr_t)pinNumber, debounce_timer_cb);
    if (debounce_timer == NULL)
    {
        printf("Failed to create debounce timer\n");
        return ESP_FAIL;
    }

    xTaskCreate(LED_Control_Task, "LED_Control_Task", 2048, (void*)(intptr_t)pinNumber, 1, NULL);

    // Register interrupt handler
    gpio_isr_handler_add(pinNumber, gpio_interrupt_handler, (void*)debounce_timer);
    ESP_LOGI(TAG, "Button %d initialized successfully", pinNumber);
    return ESP_OK;
}
