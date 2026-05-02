#include "music_assistant_client.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_err.h"
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "common/config.h"

static const char *TAG = "MUSIC_ASSISTANT_CLIENT";

#define MAX_HTTP_RESPONSE_BUFFER 512

static esp_err_t music_assistant_post_service(const char *service_path, const char *payload)
{
    const char *host_cfg = CONFIG_MUSIC_ASSISTANT_HOST;
    const char *api_key = CONFIG_MUSIC_ASSISTANT_API_KEY;

    if (service_path == NULL || payload == NULL) {
        ESP_LOGE(TAG, "service_path/payload is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (host_cfg == NULL || strlen(host_cfg) == 0) {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_HOST not set in menuconfig");
        return ESP_FAIL;
    }

    char url[320];
    if (strncmp(host_cfg, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/api/services/%s", host_cfg, service_path);
    } else {
        snprintf(url, sizeof(url), "http://%s/api/services/%s", host_cfg, service_path);
    }

    // Buffer to capture response body
    char *response_buffer = malloc(MAX_HTTP_RESPONSE_BUFFER);
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    memset(response_buffer, 0, MAX_HTTP_RESPONSE_BUFFER);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS,
        .buffer_size = MAX_HTTP_RESPONSE_BUFFER,
        .user_data = response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(response_buffer);
        return ESP_FAIL;
    }

    char auth_header[256] = {0};
    if (api_key && strlen(api_key) > 0) {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
        esp_http_client_set_header(client, "Authorization", auth_header);
    } else {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_API_KEY not set; proceeding without Authorization header");
    }
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, payload, strlen(payload));

    // Log the request details
    ESP_LOGI(TAG, "POST %s", url);
    ESP_LOGI(TAG, "Payload: %s", payload);

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP POST request failed for service '%s': %s", service_path, esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(response_buffer);
        return ESP_FAIL;
    }

    int status = esp_http_client_get_status_code(client);
    int content_length = esp_http_client_get_content_length(client);
    
    // Read response body
    int data_read = esp_http_client_read_response(client, response_buffer, MAX_HTTP_RESPONSE_BUFFER - 1);
    if (data_read >= 0) {
        response_buffer[data_read] = '\0';
    }

    ESP_LOGI(TAG, "Service '%s' completed, status=%d, len=%d", service_path, status, content_length);

    // Log response body on error
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "HTTP %d Error Response: %s", status, response_buffer);
        esp_http_client_cleanup(client);
        free(response_buffer);
        return ESP_FAIL;
    }

    // Log success response for debugging
    if (data_read > 0) {
        ESP_LOGD(TAG, "Response body: %s", response_buffer);
    }

    esp_http_client_cleanup(client);
    free(response_buffer);
    return ESP_OK;
}

static esp_err_t music_assistant_send_player_command(const char *command)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (command == NULL || strlen(command) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"command\":\"%s\"}",
             device_id, command);

    return music_assistant_post_service("music_assistant/player_command", payload);
}

static esp_err_t music_assistant_call_media_player_service(const char *service_name)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (service_name == NULL || strlen(service_name) == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    char service_path[96];
    snprintf(service_path, sizeof(service_path), "media_player/%s", service_name);

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\"}",
             device_id);

    return music_assistant_post_service(service_path, payload);
}

esp_err_t music_assistant_client_init(void)
{
    ESP_LOGI(TAG, "Music Assistant client initialized");
    return ESP_OK;
}

esp_err_t music_assistant_play_media(const char *media_id)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (!media_id) {
        ESP_LOGE(TAG, "media_id is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    /* Construct JSON payload */
    char payload[512];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"media_id\":\"%s\",\"enqueue\":\"replace\"}",
             device_id, media_id);

    return music_assistant_post_service("music_assistant/play_media", payload);
}

esp_err_t music_assistant_previous_track(void)
{
    return music_assistant_call_media_player_service("media_previous_track");
}

esp_err_t music_assistant_play_pause(void)
{
    return music_assistant_call_media_player_service("media_play_pause");
}

esp_err_t music_assistant_next_track(void)
{
    return music_assistant_call_media_player_service("media_next_track");
}

esp_err_t music_assistant_set_volume(int volume_level)
{

    if (volume_level < 0 || volume_level > 100) {
        ESP_LOGE(TAG, "Invalid volume level: %d (must be 0-100)", volume_level);
        return ESP_ERR_INVALID_ARG;
    }

    /* Convert 0-100 to 0.0-1.0 float as Home Assistant/Music Assistant expects */
    float volume_float = volume_level / 100.0f;

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"entity_id\":\"all\",\"volume_level\":%.2f}", volume_float);

    return music_assistant_post_service("media_player/volume_set", payload);
}

esp_err_t music_assistant_volume_up(void)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\"}",
             device_id);

    return music_assistant_post_service("media_player/volume_up", payload);
}

esp_err_t music_assistant_volume_down(void)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\"}",
             device_id);

    return music_assistant_post_service("media_player/volume_down", payload);
}

esp_err_t music_assistant_seek_forward(int seconds)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    if (seconds <= 0) {
        seconds = 10;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"seek_position\":%d}",
             device_id, seconds);

    return music_assistant_post_service("media_player/media_seek", payload);
}

esp_err_t music_assistant_seek_backward(int seconds)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    if (seconds <= 0) {
        seconds = 10;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"seek_position\":-%d}",
             device_id, seconds);

    return music_assistant_post_service("media_player/media_seek", payload);
}
esp_err_t music_assistant_get_media_position(float *position)
{
    const char *host_cfg = CONFIG_MUSIC_ASSISTANT_HOST;
    const char *api_key = CONFIG_MUSIC_ASSISTANT_API_KEY;
    const char *entity_id = CONFIG_MEDIA_PLAYER_ENTITY_ID;

    if (position == NULL) {
        ESP_LOGE(TAG, "position pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    if (host_cfg == NULL || strlen(host_cfg) == 0) {
        ESP_LOGW(TAG, "MUSIC_ASSISTANT_HOST not set in menuconfig");
        return ESP_FAIL;
    }

    if (entity_id == NULL || strlen(entity_id) == 0) {
        ESP_LOGE(TAG, "CONFIG_MEDIA_PLAYER_ENTITY_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    char url[320];
    if (strncmp(host_cfg, "http", 4) == 0) {
        snprintf(url, sizeof(url), "%s/api/states/%s", host_cfg, entity_id);
    } else {
        snprintf(url, sizeof(url), "http://%s/api/states/%s", host_cfg, entity_id);
    }

    // Allocate larger buffer for state response (can be quite large with all attributes)
    const int STATE_BUFFER_SIZE = 2048;
    char *response_buffer = malloc(STATE_BUFFER_SIZE);
    if (!response_buffer) {
        ESP_LOGE(TAG, "Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    memset(response_buffer, 0, STATE_BUFFER_SIZE);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = HTTP_REQUEST_TIMEOUT_MS,
        .buffer_size = STATE_BUFFER_SIZE,
        .user_data = response_buffer,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        free(response_buffer);
        return ESP_FAIL;
    }

    char auth_header[256] = {0};
    if (api_key && strlen(api_key) > 0) {
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", api_key);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(response_buffer);
        return ESP_FAIL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    
    int data_read = 0;
    if (status >= 200 && status < 300 && content_length > 0) {
        data_read = esp_http_client_read(client, response_buffer, STATE_BUFFER_SIZE - 1);
        if (data_read >= 0) {
            response_buffer[data_read] = '\0';
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "HTTP %d Error getting state", status);
        free(response_buffer);
        return ESP_FAIL;
    }

    // Log response for debugging
    ESP_LOGI(TAG, "State response (%d bytes): %s", data_read, response_buffer);

    // Parse JSON to extract media_position and media_position_updated_at
    char *pos_str = strstr(response_buffer, "\"media_position\":");
    char *updated_str = strstr(response_buffer, "\"media_position_updated_at\":\"");
    
    if (!pos_str) {
        ESP_LOGW(TAG, "media_position not found in response");
        free(response_buffer);
        return ESP_FAIL;
    }
    
    // Parse base position
    pos_str += strlen("\"media_position\":");
    float base_position = atof(pos_str);
    
    // If we have the updated_at timestamp, calculate actual current position
    if (updated_str) {
        updated_str += strlen("\"media_position_updated_at\":\"");
        
        // Parse ISO 8601 timestamp: "2026-02-28T15:21:09.014396+00:00"
        struct tm timeinfo = {0};
        char timestamp_str[64];
        sscanf(updated_str, "%63[^\"]", timestamp_str);
        
        // Parse the timestamp (ignoring microseconds and timezone for simplicity)
        if (strptime(timestamp_str, "%Y-%m-%dT%H:%M:%S", &timeinfo) != NULL) {
            time_t updated_time = mktime(&timeinfo);
            
            // Get current time
            struct timeval now;
            gettimeofday(&now, NULL);
            time_t current_time = now.tv_sec;
            
            // Calculate elapsed time since the position was updated
            double elapsed = difftime(current_time, updated_time);
            
            // Calculate actual current position
            *position = base_position + (float)elapsed;
            
            ESP_LOGI(TAG, "Base position: %.1fs, elapsed: %.1fs, current position: %.1fs", 
                     base_position, elapsed, *position);
        } else {
            // Fallback to base position if timestamp parsing fails
            ESP_LOGW(TAG, "Failed to parse timestamp, using base position: %.1fs", base_position);
            *position = base_position;
        }
    } else {
        // No timestamp available, use base position
        ESP_LOGI(TAG, "No timestamp found, using base position: %.1fs", base_position);
        *position = base_position;
    }
    
    free(response_buffer);
    return ESP_OK;
}

esp_err_t music_assistant_seek_to_position(float position)
{
    const char *device_id = CONFIG_DEVICE_ID;

    if (device_id == NULL || strlen(device_id) == 0) {
        ESP_LOGE(TAG, "DEVICE_ID not set");
        return ESP_ERR_INVALID_STATE;
    }

    if (position < 0) {
        position = 0;
    }

    char payload[256];
    snprintf(payload, sizeof(payload),
             "{\"device_id\":\"%s\",\"seek_position\":%.1f}",
             device_id, position);

    return music_assistant_post_service("media_player/media_seek", payload);
}