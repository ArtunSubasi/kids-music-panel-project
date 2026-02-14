#pragma once

#include "esp_err.h"

/**
 * @brief Initialize Music Assistant controller
 *
 * Subscribes to button events and forwards them to the Music Assistant client.
 *
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t music_assistant_controller_init(void);
