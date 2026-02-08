#include "music_assistant_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <string.h>
#include "common/config.h"

static const char *TAG = "MUSIC_ASSISTANT_CLIENT";

esp_err_t music_assistant_client_init(void)
{
    ESP_LOGI(TAG, "Music Assistant client initialized");
    return ESP_OK;
}

esp_err_t music_assistant_play_media(const char *media_id)
{
    const char *host_cfg = CONFIG_MUSIC_ASSISTANT_HOST;
    const char *api_key = CONFIG_MUSIC_ASSISTANT_API_KEY;
    const char *device_id = CONFIG_DEVICE_ID;

    if (!media_id) {
        ESP_LOGE(TAG, "media_id is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (host_cfg == NULL || strlen(host_cfg) == 0) {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_HOST not set in menuconfig");
        return ESP_FAIL;
    }

    /* Construct URL from configured host */
    char url[256];
    if (strncmp(host_cfg, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/api/services/music_assistant/play_media", host_cfg);
    } else {
        snprintf(url, sizeof(url), "http://%s/api/services/music_assistant/play_media", host_cfg);
    }

    /* Initialize HTTP client */
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return ESP_FAIL;
    }

    /* Set up Authorization header if API key is configured */
    char auth_header[256] = {0};
    if (api_key && strlen(api_key) > 0) {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
        esp_http_client_set_header(client, "Authorization", auth_header);
    } else {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_API_KEY not set; proceeding without Authorization header");
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");

    /* Construct JSON payload */
    char payload[512];
    snprintf(payload, sizeof(payload), 
             "{\"device_id\": \"%s\", \"media_id\": \"%s\", \"enqueue\": \"replace\"}", 
             device_id, media_id);

    esp_http_client_set_post_field(client, payload, strlen(payload));

    /* Send request */
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        int len = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP POST completed, status=%d, len=%d", status, len);
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    
    return (err == ESP_OK) ? ESP_OK : ESP_FAIL;
}
