#ifndef BOARD_PINS_H
#define BOARD_PINS_H

/**
 * @file board_pins.h
 * @brief Centralized GPIO and SPI pin definitions for the Kids Music Panel
 * 
 * This file defines all hardware pin assignments to avoid duplication
 * and make it easy to adapt to different board revisions.
 */

/* ==================== OLED Display (SPI2) ==================== */
#define BOARD_OLED_SPI_HOST         SPI2_HOST
#define BOARD_OLED_SPI_CLK          18
#define BOARD_OLED_SPI_MOSI         23
#define BOARD_OLED_SPI_MISO         -1      /* Not used */
#define BOARD_OLED_PIN_CS           5
#define BOARD_OLED_PIN_DC           17
#define BOARD_OLED_PIN_RST          16
#define BOARD_OLED_SPI_FREQ_HZ      8000000

/* ==================== RFID Scanner (SPI3) ==================== */
#define BOARD_RFID_SPI_HOST         SPI3_HOST
#define BOARD_RFID_SPI_CLK          14
#define BOARD_RFID_SPI_MOSI         13
#define BOARD_RFID_SPI_MISO         12
#define BOARD_RFID_PIN_CS           15      /* Also called SDA in RC522 context */
#define BOARD_RFID_PIN_RST          27      /* Soft-reset */

#endif /* BOARD_PINS_H */
