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
#include "input/buttons.h"

static const char *TAG = "MAIN_APP";

/* Global display handle */
static display_t g_display = {0};

/* Global RFID scanner handle */
static rfid_scanner_t g_rfid_scanner = {0};

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
    
    ESP_ERROR_CHECK(buttons_init());

    ESP_LOGI(TAG, "System ready. Waiting for RFID cards...");
}
