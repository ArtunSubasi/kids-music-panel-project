#pragma once

#include "esp_err.h"
#include "display/display.h"

/**
 * @file wifi_manager.h
 * @brief WiFi Connection Manager Module
 * 
 * This module encapsulates all WiFi initialization.
 */

/**
 * @brief Initialize and start WiFi connection
 * 
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t wifi_manager_init();
