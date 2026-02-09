#pragma once

#include "esp_err.h"
#include "display.h"
#include "display/display_controller.h"

/**
 * Controls the display by subscribing to WiFi connection events and updating the display accordingly.
 */

/**
 * @brief Initialize event handlers for display updates
 * 
 * @param display Pointer to display handle for status messages during WiFi events
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t display_controller_init(display_t *display);
