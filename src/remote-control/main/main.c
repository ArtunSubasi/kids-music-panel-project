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
#include "media_mapping.h"

/* Centralized configuration headers */
#include "common/board_pins.h"
#include "common/config.h"
#include "display/display.h"
#include "display/display_controller.h"
#include "rfid/rfid_scanner.h"
#include "music_assistant/music_assistant_client.h"
#include "wifi/wifi_manager.h"
#include "wifi/wifi_controller.h"

#define PULLUP_ENABLED_PIN 22
#define DEBOUNCE_MS 50

static const char *TAG = "MAIN_APP";

/* Global display handle */
static display_t g_display = {0};

/* Global RFID scanner handle */
static rfid_scanner_t g_rfid_scanner = {0};


QueueHandle_t interruptQueue = NULL;
int led_on = 0;
static TimerHandle_t debounce_timer = NULL;

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
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xTimerResetFromISR(debounce_timer, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken)
    {
        portYIELD_FROM_ISR();
    }
}

void LED_Control_Task(void *params)
{
    int pinNumber, count = 0;
    while (true)
    {
        if (xQueueReceive(interruptQueue, &pinNumber, portMAX_DELAY))
        {
            led_on = !led_on;
            int level = gpio_get_level(PULLUP_ENABLED_PIN);
            printf("GPIO %d was pressed %d times. The state is %d\n", pinNumber, count++, level);
        }
    }
}


// TODO move this somewhere more appropriate, maybe rfid_scanner.c or a new app_events.c?
static void on_rfid_tag_scanned(void *arg, esp_event_base_t base, int32_t event_id, void *data)
{
    rc522_picc_state_changed_event_t *event = (rc522_picc_state_changed_event_t *)data;
    rc522_picc_t *picc = event->picc;

    if (picc->state == RC522_PICC_STATE_ACTIVE) {
        rc522_picc_print(picc);
        const char *type_name = rc522_picc_type_name(picc->type);
        char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX] = {0};
        if (rc522_picc_uid_to_str(&picc->uid, uid_str, sizeof(uid_str)) != ESP_OK) {
            snprintf(uid_str, sizeof(uid_str), "UID-error");
        }
        display_show(&g_display, type_name ? type_name : "Unknown", uid_str);
        
        /* Look up media ID from UID mapping and request playback */
        const char *media_id = media_mapping_get_media_id(&picc->uid);
        if (media_id) {
            music_assistant_play_media(media_id);
        } else {
            ESP_LOGW(TAG, "No media mapping found for this UID, skipping playback request");
        }
    }
    else if (picc->state == RC522_PICC_STATE_IDLE && event->old_state >= RC522_PICC_STATE_ACTIVE) {
        ESP_LOGI(TAG, "Card has been removed");
        display_show(&g_display, DISPLAY_MSG_WAITING);
    }
}

void app_main(void) {

    // Create the default event loop before initializing any components that rely on it
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // ---------------------------------------------------------
    // 1. OLED INITIALIZATION (via display module)
    // ---------------------------------------------------------
    ESP_ERROR_CHECK(display_controller_init(&g_display));
    ESP_ERROR_CHECK(display_init(&g_display));
    
    // Show initial screen
    display_show(&g_display, DISPLAY_MSG_WAITING);

    // ---------------------------------------------------------
    // 2. WiFi INITIALIZATION (via wifi_manager module)
    // ---------------------------------------------------------
    // Start WiFi using credentials from menuconfig; display will be updated by handlers
    ESP_ERROR_CHECK(wifi_controller_init());
    ESP_ERROR_CHECK(wifi_manager_init());

    // ---------------------------------------------------------
    // 3. RFID INITIALIZATION (via rfid_scanner module)
    // ---------------------------------------------------------
    ESP_ERROR_CHECK(rfid_scanner_init(&g_rfid_scanner));
    rfid_scanner_start(&g_rfid_scanner, on_rfid_tag_scanned);
    
    ESP_LOGI(TAG, "System ready. Waiting for RFID cards...");

    // Configure Input GPIO with Pull-up
    gpio_config_t io_conf_pullup_enabled = {
        .pin_bit_mask = (1ULL << PULLUP_ENABLED_PIN),   // Select GPIO 4
        .mode = GPIO_MODE_INPUT,                // Set as input
        .pull_up_en = GPIO_PULLUP_ENABLE,       // Enable internal pull-up
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  // Disable pull-down
        .intr_type = GPIO_INTR_NEGEDGE        
    };
    gpio_config(&io_conf_pullup_enabled);

    // Create queue for interrupt handling
    interruptQueue = xQueueCreate(10, sizeof(int));
    xTaskCreate(LED_Control_Task, "LED_Control_Task", 2048, NULL, 1, NULL);

    // Create a one-shot FreeRTOS timer for debounce. ISR will reset/start it.
    debounce_timer = xTimerCreate("debounce", pdMS_TO_TICKS(DEBOUNCE_MS), pdFALSE, (void*)(intptr_t)PULLUP_ENABLED_PIN, debounce_timer_cb);
    if (debounce_timer == NULL)
    {
        printf("Failed to create debounce timer\n");
    }

    // Register interrupt handler
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PULLUP_ENABLED_PIN, gpio_interrupt_handler, (void*)(intptr_t)PULLUP_ENABLED_PIN);
}
