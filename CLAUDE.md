# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ESP32 firmware (C/ESP-IDF) for a kids' music panel. Children scan RFID cards to trigger music playback; the ESP32 communicates with a Home Assistant / Music Assistant server over WiFi. Audio streams via a separate ESP32 Squeezelite client to hard-wired speakers.

Only the remote control firmware lives in this repo (`src/remote-control/`). Home Assistant configuration and the Squeezelite client are external.

## Build & Flash Commands

All commands run from `src/remote-control/`:

```bash
idf.py menuconfig       # Configure WiFi credentials, MA host/API key
idf.py build            # Compile firmware
idf.py flash            # Flash to connected ESP32
idf.py monitor          # Serial output (Ctrl+] to exit)
idf.py build flash monitor  # Common all-in-one dev command
idf.py fullclean        # Clean build artifacts
```

Required menuconfig entries before first build:
- `WiFi Configuration` → SSID and password
- `Music Assistant Configuration` → host URL and API key

A Docker DevContainer (`.devcontainer/`) provides ESP-IDF pre-installed; open in VSCode and use the ESP-IDF extension.

## Architecture

### Event-Driven Design

The firmware uses FreeRTOS event loops throughout. Modules post events; controllers subscribe and act. Direct function calls between modules are avoided — events decouple timing.

```
[RC522 RFID ISR] → rfid_scanned event → music_assistant_controller → HTTP POST → Music Assistant API
[Button GPIO ISR] → button event → music_assistant_controller → HTTP POST
[WiFi events]    → wifi_controller / display_controller → display updates
```

### Module Structure (`main/`)

| Directory | Role |
|-----------|------|
| `common/` | Shared headers: `config.h` (constants), `board_pins.h` (all GPIO/SPI pin defs), `app_events.h` (event IDs) |
| `display/` | SSD1306 driver (`display.c`) + event subscriber that maps WiFi/app events to screen text (`display_controller.c`) |
| `rfid/` | RC522 driver init and event registration (`rfid_scanner.c`) |
| `music_assistant/` | HTTP client for MA API (`music_assistant_client.c`) + translates button/RFID events into API calls (`music_assistant_controller.c`) |
| `wifi/` | Connection state machine (`wifi_manager.c`) + event handler (`wifi_controller.c`) |
| `input/` | Button GPIO with debounce (`buttons.c`), potentiometer ADC (`potentiometer.c`) |
| `soft_power/` | GPIO-21 power management (`soft_power.c`) |
| `media_mapping.c/h` | Static UID→Media ID lookup table |

`main.c` is a thin entry point: initializes NVS/event loop/netif, then calls each module's init function.

### Adding a New RFID Card Mapping

Edit `main/media_mapping.c`. Add an entry to the static array with the card's UID string (format: `"AA BB CC DD"`) and the Music Assistant media URI. The UID is logged to serial when an unknown card is scanned.

### Pin Assignments

Defined in `main/common/board_pins.h`. Two SPI buses are used:
- **SPI2**: OLED display (MOSI=23, CLK=18, CS=5, DC=17, RST=16)
- **SPI3**: RC522 RFID (MISO=12, MOSI=13, CLK=14, CS=15, RST=27)
- Buttons: GPIO 22 (Prev), 25 (Play/Pause), 19 (Next)
- Potentiometer: GPIO 33 (ADC1_CH5)
- Soft power: GPIO 21

### Application Constants

`main/common/config.h` holds the MA device ID, media player entity name, HTTP timeout, display update interval, and WiFi retry settings. Change these before adding new features that depend on timing or MA entity names.
