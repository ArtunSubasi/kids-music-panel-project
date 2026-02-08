#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/**
 * @file config.h
 * @brief Centralized configuration constants for the Kids Music Panel
 * 
 * This file consolidates all configurable values including timing constants
 * and display messages. Menuconfig values are referenced directly from
 * sdkconfig.h (already included via esp_log.h and other IDF headers).
 */

/* ========== Device Configuration ========== */
#define CONFIG_DEVICE_ID                "250c3cfed7cfa1bc079d3b7fe26417f2"

/* ========== Timing Constants ========== */
#define WIFI_CONNECT_MAX_RETRY          5
#define WIFI_RECONNECT_DELAY_MS         1000
#define HTTP_REQUEST_TIMEOUT_MS         5000
#define DISPLAY_UPDATE_TIMEOUT_MS       100

/* ========== Display Messages ========== */
#define DISPLAY_MSG_WAITING             "Warte auf", "Karte..."
#define DISPLAY_MSG_CONNECTING_WIFI     "WLAN-Verbindung", "wird hergestellt..."
#define DISPLAY_MSG_WIFI_CONNECTED      "WLAN-Verbindung", "erfolgreich"
#define DISPLAY_MSG_WIFI_FAILED         "WLAN-Verbindung", "fehlgeschlagen"

#endif /* APP_CONFIG_H */
