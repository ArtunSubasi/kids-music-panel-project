#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "driver/spi_master.h"
#include "ssd1306.h"
#include "rc522.h"
#include "driver/rc522_spi.h"
#include "rc522_picc.h"

#include "esp_http_client.h"
#include <string.h>
#include "media_mapping.h"

/* Centralized configuration headers */
#include "common/board_pins.h"
#include "common/config.h"
#include "display/display.h"
#include "rfid/rfid_scanner.h"

static const char *TAG = "MAIN_APP";

/* Global display handle */
static display_t g_display = {0};

/* Global RFID scanner handle */
static rfid_scanner_t g_rfid_scanner = {0};

/* Hard-coded test IDs (kept as requested) */
static const char *TEST_DEVICE_ID = CONFIG_DEVICE_ID;

static void send_play_media_request(const char *media_id)
{
    const char *host_cfg = CONFIG_MUSIC_ASSISTANT_HOST;
    const char *api_key = CONFIG_MUSIC_ASSISTANT_API_KEY;

    if (!media_id) {
        ESP_LOGE(TAG, "media_id is NULL");
        return;
    }

    if (host_cfg == NULL || strlen(host_cfg) == 0) {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_HOST not set in menuconfig");
        return;
    }

    char url[256];
    if (strncmp(host_cfg, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/api/services/music_assistant/play_media", host_cfg);
    } else {
        snprintf(url, sizeof(url), "http://%s/api/services/music_assistant/play_media", host_cfg);
    }

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return;
    }

    /* Authorization header */
    char auth_header[256] = {0};
    if (api_key && strlen(api_key) > 0) {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
        esp_http_client_set_header(client, "Authorization", auth_header);
    } else {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_API_KEY not set; proceeding without Authorization header");
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");

    char payload[512];
    snprintf(payload, sizeof(payload), "{\"device_id\": \"%s\", \"media_id\": \"%s\", \"enqueue\": \"replace\"}", TEST_DEVICE_ID, media_id);

    esp_http_client_set_post_field(client, payload, strlen(payload));

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP POST completed, status=%d, len=%d", status, len);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}



/* Queue used by GPIO ISR (match existing code's name) */
static QueueHandle_t interputQueue = NULL;

/* WiFi connection state handling */
#define WIFI_CONNECT_MAX_RETRY 5
static EventGroupHandle_t s_wifi_event_group;
const int WIFI_CONNECTED_BIT = BIT0;
const int WIFI_FAIL_BIT = BIT1;

static int s_retry_num = 0;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        display_show(&g_display, DISPLAY_MSG_CONNECTING_WIFI);
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_CONNECT_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG,"connect to the AP fail");
            display_show(&g_display, DISPLAY_MSG_WIFI_FAILED);
        }
        ESP_LOGI(TAG, "connect fail, reason:%d", ((wifi_event_sta_disconnected_t*)event_data)->reason);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        //ESP_LOGI(TAG, "got ip:%s", esp_ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        display_show(&g_display, DISPLAY_MSG_WIFI_CONNECTED);
    }
}

static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
#ifdef CONFIG_WIFI_SSID
            .ssid = CONFIG_WIFI_SSID,
#else
            .ssid = "",
#endif
#ifdef CONFIG_WIFI_PASSWORD
            .password = CONFIG_WIFI_PASSWORD,
#else
            .password = "",
#endif
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}



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
        
        /* Look up media ID from UID mapping */
        const char *media_id = media_mapping_get_media_id(&picc->uid);
        if (media_id) {
            send_play_media_request(media_id);
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
    // ---------------------------------------------------------
    // 1. OLED INITIALIZATION (via display module)
    // ---------------------------------------------------------
    ESP_ERROR_CHECK(display_init(&g_display));
    
    // Show initial screen
    display_show(&g_display, DISPLAY_MSG_WAITING);

    // Start WiFi using credentials from menuconfig; display will be updated by handlers
    wifi_init_sta();

    // Create queue used by GPIO ISR (if present elsewhere in project)
    interputQueue = xQueueCreate(10, sizeof(uint32_t));
    if (interputQueue == NULL) {
        ESP_LOGE(TAG, "Failed to create interputQueue");
    } else {
        ESP_LOGI(TAG, "interputQueue created");
    }


    // ---------------------------------------------------------
    // 2. RFID INITIALIZATION (via rfid_scanner module)
    // ---------------------------------------------------------
    ESP_ERROR_CHECK(rfid_scanner_init(&g_rfid_scanner));
    rfid_scanner_start(&g_rfid_scanner, on_rfid_tag_scanned);
    
    ESP_LOGI(TAG, "System ready. Waiting for RFID cards...");
}
