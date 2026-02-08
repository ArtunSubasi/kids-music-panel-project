#ifndef RFID_SCANNER_H
#define RFID_SCANNER_H

#include "esp_err.h"
#include "rc522.h"

/**
 * @file rfid_scanner.h
 * @brief Public interface for RFID scanner management
 * 
 * This module encapsulates all RC522 RFID reader operations.
 * Low-level SPI driver initialization is handled internally.
 */

typedef struct {
    rc522_driver_handle_t driver;
    rc522_handle_t scanner;
} rfid_scanner_t;

/**
 * Initialize the RC522 RFID scanner on SPI3
 * 
 * @param scanner Pointer to rfid_scanner_t structure to initialize
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t rfid_scanner_init(rfid_scanner_t *scanner);

/**
 * Start scanning for RFID cards
 * 
 * Registers event callbacks and begins card detection.
 * 
 * @param scanner Pointer to initialized rfid_scanner_t structure
 * @param event_handler Callback function to handle RFID events
 */
void rfid_scanner_start(rfid_scanner_t *scanner, void (*event_handler)(void *, const char *, int32_t, void *));

#endif /* RFID_SCANNER_H */
