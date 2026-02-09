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
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t wifi_manager_init();
