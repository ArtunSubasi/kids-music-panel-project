#pragma once

#include "esp_event.h"

/**
 * @file app_events.h
 * @brief Application-wide Event System
 * 
 * This header defines all custom events used throughout the application.
 * It provides a centralized event architecture that enables:
 * - Decoupled communication between modules
 * - Easy addition of new features (buttons, BLE, parental control)
 * - Clear event flow for debugging and monitoring
 * 
 * Usage:
 *   1. Register for events: esp_event_handler_register(APP_EVENTS, APP_EVENT_CARD_DETECTED, handler, ctx)
 *   2. Post events:        esp_event_post(APP_EVENTS, APP_EVENT_CARD_DETECTED, data, size, portMAX_DELAY)
 */

/**
 * @brief Application event base - all app-specific events use this base
 */
ESP_EVENT_DECLARE_BASE(APP_EVENTS);

/**
 * @brief Application-specific event IDs
 */
typedef enum {
    /**
     * RFID Card Detection Events
     */
    
    /** Card has been detected and read
     * 
     * Event data: rc522_picc_t* pointer
     * Triggered: When RFID card transitions to ACTIVE state
     * Use case: Primary event for music playback requests
     */
    APP_EVENT_CARD_DETECTED = 0,
    
    /** Card has been removed
     * 
     * Event data: NULL
     * Triggered: When RFID card transitions from ACTIVE to IDLE state
     * Use case: Reset display or pause playback if needed
     */
    APP_EVENT_CARD_REMOVED,
    
    /**
     * WiFi Connection Events
     */
    
    /** WiFi connection attempt started
     * 
     * Event data: NULL
     * Triggered: WiFi STA mode started, attempting connection
     * Use case: Show connecting status on display
     */
    APP_EVENT_WIFI_CONNECTING,
    
    /** WiFi connection succeeded
     * 
     * Event data: ip_event_got_ip_t* pointer
     * Triggered: IP address obtained from AP
     * Use case: Show connected status, enable network features
     */
    APP_EVENT_WIFI_CONNECTED,
    
    /** WiFi connection failed
     * 
     * Event data: NULL
     * Triggered: Connection retries exhausted
     * Use case: Show error status, fall back to offline mode
     */
    APP_EVENT_WIFI_FAILED,
    
    /**
     * Future Feature Events (reserved for expansion)
     */
    
    /** Button pressed
     * 
     * Reserved for button controller module
     * Event data: button_event_t* with button_id and press_type
     * Use case: Volume control, skip track, etc.
     */
    APP_EVENT_BUTTON_PRESSED,
    
    /** Parental control time limit reached
     * 
     * Reserved for parental control module
     * Event data: NULL
     * Use case: Disable playback until limit resets
     */
    APP_EVENT_PARENTAL_LIMIT_REACHED,
    
    /** Bluetooth device connected
     * 
     * Reserved for BLE monitor module
     * Event data: ble_device_info_t* with device details
     * Use case: Log device connections, trigger monitoring
     */
    APP_EVENT_BLE_DEVICE_CONNECTED,
    
    /** System error or warning
     * 
     * Event data: app_error_event_t* with error code and message
     * Use case: Centralized error handling and logging
     */
    APP_EVENT_ERROR,
    
    /** Marker for event count (keep last)
     */
    APP_EVENTS_COUNT
} app_event_id_t;

/**
 * @brief Error event data structure
 */
typedef struct {
    int error_code;
    const char *error_message;
} app_error_event_t;
