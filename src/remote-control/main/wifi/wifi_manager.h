#pragma once

#include "esp_err.h"
#include "display/display.h"

/**
 * @file wifi_manager.h
 * @brief WiFi Connection Manager Module
 * 
 * This module encapsulates all WiFi initialization and connection management.
 * It handles:
 * - WiFi event loop setup
 * - Station mode configuration from menuconfig
 * - Connection retry logic
 * - Display updates via event callbacks
 */

/**
 * @brief Initialize and start WiFi connection
 * 
 * Sets up the WiFi event loop, registers event handlers, and starts connecting
 * to the configured SSID. Display updates are provided via the event callbacks.
 * 
 * @param display Pointer to display handle for status messages during WiFi events
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t wifi_manager_init(display_t *display);

/**
 * @brief Check if WiFi is currently connected
 * 
 * Non-blocking check of the current WiFi connection state.
 * 
 * @return true if connected and IP obtained, false otherwise
 */
bool wifi_manager_is_connected(void);
