#pragma once

#include "esp_err.h"

/**
 * @file music_assistant_client.h
 * @brief Music Assistant HTTP Client Module
 * 
 * This module encapsulates all HTTP communication with the Music Assistant API.
 * It handles:
 * - URL construction from config
 * - Authentication header setup
 * - Media playback requests
 * - Error handling and logging
 */

/**
 * @brief Initialize the Music Assistant client
 * 
 * This function prepares any necessary state for Music Assistant communication.
 * Currently minimal setup is needed, but provided for future enhancements
 * (e.g., connection pooling, retry logic).
 * 
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t music_assistant_client_init(void);

/**
 * @brief Send a media playback request to Music Assistant
 * 
 * Constructs an HTTP POST request with the given media_id and sends it to
 * the configured Music Assistant host. Uses the device_id and API key from
 * menuconfig for authentication.
 * 
 * @param media_id The Music Assistant media ID (e.g., "radiobrowser://radio/...")
 * @return ESP_OK on HTTP 2xx response, ESP_ERR_INVALID_ARG if media_id is NULL,
 *         ESP_FAIL if HTTP request failed or host/API key not configured
 */
esp_err_t music_assistant_play_media(const char *media_id);

/**
 * @brief Send a previous-track command to Music Assistant
 *
 * @return ESP_OK on HTTP 2xx response, ESP_FAIL otherwise
 */
esp_err_t music_assistant_previous_track(void);

/**
 * @brief Send a play/pause toggle command to Music Assistant
 *
 * @return ESP_OK on HTTP 2xx response, ESP_FAIL otherwise
 */
esp_err_t music_assistant_play_pause(void);

/**
 * @brief Send a next-track command to Music Assistant
 *
 * @return ESP_OK on HTTP 2xx response, ESP_FAIL otherwise
 */
esp_err_t music_assistant_next_track(void);
