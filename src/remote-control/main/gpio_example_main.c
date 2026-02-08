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

static const char *TAG = "MAIN_APP";

// --- OLED Konfiguration (SPI2) ---
// (Deine Pins vom letzten Mal)
#define OLED_MOSI 23
#define OLED_CLK  18
#define OLED_CS   5
#define OLED_DC   17
#define OLED_RST  16

// --- RFID Konfiguration (SPI3) ---
// (Neue Pins für den zweiten Bus)
#define RC522_SPI_BUS_GPIO_MISO    (12)
#define RC522_SPI_BUS_GPIO_MOSI    (13)
#define RC522_SPI_BUS_GPIO_SCLK    (14)
#define RC522_SPI_SCANNER_GPIO_SDA (15)
#define RC522_SCANNER_GPIO_RST     (27) // soft-reset

// Globale Variable für das Display-Handle, damit wir aus dem Scanner darauf zugreifen können
ssd1306_handle_t oled_dev = NULL;

// Hilfsfunktion: Schreibt Text auf das OLED
void update_display(const char *line1, const char *line2) {
    if (oled_dev) {
        ssd1306_clear(oled_dev);
        ssd1306_draw_text(oled_dev, 0, 0,  "RFID SCANNER", true);
        ssd1306_draw_text(oled_dev, 0, 20, line1, true);
        ssd1306_draw_text(oled_dev, 0, 40, line2, true);
        ssd1306_display(oled_dev);
    }
}

static rc522_spi_config_t driver_config = {
    .host_id = SPI3_HOST,
    .bus_config = &(spi_bus_config_t){
        .miso_io_num = RC522_SPI_BUS_GPIO_MISO,
        .mosi_io_num = RC522_SPI_BUS_GPIO_MOSI,
        .sclk_io_num = RC522_SPI_BUS_GPIO_SCLK,
    },
    .dev_config = {
        .spics_io_num = RC522_SPI_SCANNER_GPIO_SDA,
    },
    .rst_io_num = RC522_SCANNER_GPIO_RST,
};

static rc522_driver_handle_t driver;
static rc522_handle_t scanner;

/* Hard-coded test IDs (kept as requested) */
static const char *TEST_DEVICE_ID = "250c3cfed7cfa1bc079d3b7fe26417f2";

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
        update_display("connecting...", " ");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_CONNECT_MAX_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
            ESP_LOGI(TAG,"connect to the AP fail");
            update_display("conn. failed", "");
        }
        ESP_LOGI(TAG, "connect fail, reason:%d", ((wifi_event_sta_disconnected_t*)event_data)->reason);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        //ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        //ESP_LOGI(TAG, "got ip:%s", esp_ip4addr_ntoa(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
        update_display("connected.", "");
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
        update_display(type_name ? type_name : "Unknown", uid_str);
        
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
        update_display("Warte auf", "Karte...");
    }
}

void app_main(void) {
    // ---------------------------------------------------------
    // 1. OLED INITIALISIERUNG (Dein funktionierender Code)
    // ---------------------------------------------------------
    spi_bus_config_t buscfg_oled = {
        .mosi_io_num = OLED_MOSI,
        .sclk_io_num = OLED_CLK,
        .miso_io_num = -1,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST, &buscfg_oled, SPI_DMA_CH_AUTO));

    ssd1306_config_t cfg_oled = {
        .bus = SSD1306_SPI,
        .width = 128,
        .height = 64,
        .iface.spi = {
            .host = SPI2_HOST,
            .cs_gpio = OLED_CS,
            .dc_gpio = OLED_DC,
            .rst_gpio = OLED_RST,
            .clk_hz = 8000000,
        },
    };
    ESP_ERROR_CHECK(ssd1306_new_spi(&cfg_oled, &oled_dev));
    
    // Startbildschirm
    update_display("Warte auf", "Karte...");

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
    // 2. RFID INITIALISIERUNG (Neuer Code auf SPI3)
    // ---------------------------------------------------------
    // Hinweis: Der RC522-Treiber "abobija" kümmert sich intern um die 
    // Bus-Initialisierung, wenn wir ihm die Pins geben.
    rc522_spi_create(&driver_config, &driver);
    rc522_driver_install(driver);

    rc522_config_t scanner_config = {
        .driver = driver,
    };

    rc522_create(&scanner_config, &scanner);
    rc522_register_events(scanner, RC522_EVENT_PICC_STATE_CHANGED, on_rfid_tag_scanned, NULL);
    rc522_start(scanner);
    
    ESP_LOGI(TAG, "System läuft. Halte eine Karte an den Leser.");
}
