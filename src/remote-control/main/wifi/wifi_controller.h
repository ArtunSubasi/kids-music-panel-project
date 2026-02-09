#pragma once

#include "esp_err.h"

/**
 * Controls the WLAN connection by subscribing to WiFi events.
 */

/**
 * @brief control WiFi connection
 * 
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t wifi_controller_init();
