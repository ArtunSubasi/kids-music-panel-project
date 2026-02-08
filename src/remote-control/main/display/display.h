#ifndef DISPLAY_H
#define DISPLAY_H

#include "esp_err.h"
#include "ssd1306.h"

/**
 * @file display.h
 * @brief Public interface for OLED display management
 * 
 * This module encapsulates all SSD1306 display operations.
 * Initialization and low-level SPI configuration are hidden.
 */

typedef struct {
    ssd1306_handle_t handle;
} display_t;

/**
 * Initialize the OLED display on SPI2
 * 
 * @param display Pointer to display_t structure to initialize
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t display_init(display_t *display);

/**
 * Show two lines of text on the display
 * 
 * Displays:
 * - Header: "RFID SCANNER"
 * - Line 1: First parameter
 * - Line 2: Second parameter
 * 
 * @param display Pointer to initialized display_t structure
 * @param line1 Text for first line (max 32 chars recommended)
 * @param line2 Text for second line (max 32 chars recommended)
 */
void display_show(display_t *display, const char *line1, const char *line2);

/**
 * Clear the display completely
 * 
 * @param display Pointer to initialized display_t structure
 */
void display_clear(display_t *display);

#endif /* DISPLAY_H */
