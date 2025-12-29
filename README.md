# Kids Music Panel Project

## Introduction

This project is a **DIY music panel for children** designed to allow kids to play Spotify playlists safely and interactively while providing parental controls.  
The system consists of a **portable ESP32-based panel** with colorful arcade buttons, LEDs, small control buttons (Prev / Start / Next), and a monochrome OLED display showing remaining playtime.  

The backend is handled by **Home Assistant (HA)** running on a **Raspberry Pi** in the kids’ room, which streams Spotify playlists to **Bluetooth speakers**.  
Parental controls enforce a **daily time limit** for music playback, and the panel provides intuitive visual feedback with LEDs and pegs for playlist identification.

---

## Architecture

### Overview

                   ┌───────────────────────────┐
                   │        ESP32 Panel        │
                   │  - Sends button events    │
                   │  - Receives remaining time│
                   └─────────▲─────────────────┘
                             │
                           Wi-Fi
                             │
                   ┌─────────▼────────────────┐
                   │      Home Assistant      │
                   │  - Spotify integration   │
                   │  - Playtime tracking     │
                   │  - Parental controls     │
                   └──────────────────────────┘
                             │
                             │ Wi-Fi
                             ▼
                   ┌──────────────────────────┐
                   │      Raspberry Pi        │
                   │  - Spotify client        │
                   │  - Bluetooth connection  │
                   └─────────▲────────────────┘
                             │
                    Bluetooth│
                             ▼
                   ┌──────────────────────────┐
                   │    Bluetooth Speakers    │
                   └──────────────────────────┘

### Component Descriptions

- **ESP32 Panel:**  
  Portable, user-facing frontend. Handles button input, LED feedback, OLED display, battery monitoring, and deep sleep. Sends button events to HA and receives remaining time updates for display and LED control.

- **Home Assistant (HA):**  
  Central brain of the system. Receives button events, triggers Spotify playback on the Raspberry Pi, enforces parental control limits, and updates remaining time for display on the panel.

- **Raspberry Pi:**  
  Runs Spotify client. Connects to Bluetooth speakers for playback. Receives commands from HA.

- **Bluetooth Speakers:**  
  Bluetooth speakers for audio output. Playlists are streamed from the Raspberry Pi via Bluetooth.

---

## Design Notes

- **Frontend / Backend separation:**  
  The ESP32 panel is fully wireless and portable, acting as the frontend, while HA + Raspberry Pi handle backend logic and Spotify playback. This makes the system modular and easier to test.

- **Bidirectional communication:**  
  Button events are sent from the panel to HA, and HA pushes remaining time updates back to the panel via ESPHome native API.

- **Battery & deep sleep:**  
  The panel can operate on a USB-C power bank, using deep sleep when idle to conserve energy. Any button press wakes the panel.

- **Arcade buttons & pegs:**  
  Big colorful buttons for playlists are paired with LEDs above and wooden pegs below to visually associate playlist pictures. Small black buttons handle Prev / Start / Next functionality.

- **Parental control integration:**  
  HA enforces a daily playtime limit. The remaining time is displayed on the OLED to give kids immediate feedback.

- **Simplicity & safety:**  
  All components are kid-friendly, mounted on a smooth wooden panel. Wiring uses a common ground and screw terminals for maintainability.

---

## Project Plan

| Module | Task | Difficulty | Time Estimate | Required Items | Recommended Items |
|--------|------|------------|---------------|----------------|-----------------|
| **1. Raspberry Pi (Spotify)** | Set up Raspberry Pi OS Lite | Medium | 2–3 h | Raspberry Pi Zero 2 W, SD card 32GB, power supply | Case, heatsink |
| | Pair Bluetooth speakers | Easy | 1 h | |  |
| | Integrate Spotify with HA | Medium | 1–2 h | Spotify Family account | None |
| | Test HA scripts: play/stop/prev/next | Medium | 1 h | None | None |
| | Optional: simulate controller via HA UI | Easy | 1 h | None | Lovelace dashboard on tablet/PC |
| **2. ESP32 Controller (Input & Display)** | Select ESP32 board | Easy | 30 min | ESP32 WROOM/WROVER | None |
| | Connect I²C OLED (monochrome) | Medium | 1 h | 0.96" SSD1306 OLED | Optional: extra I²C cables, headers |
| | Wire buttons (5 arcade + 3 small) | Medium | 2 h | 5 arcade buttons, 3 small buttons, diodes, resistors, wires | Arcade wiring harness, screw terminals |
| | Wire LEDs for each button | Medium | 1–2 h | 5 LEDs, resistors | WS2812 RGB LEDs if desired |
| | Battery monitoring (ADC) | Medium | 1 h | USB-C power bank, voltage divider resistors | Multimeter for testing |
| | Implement wake from deep sleep | Medium | 1–2 h | None | None |
| **3. Physical Construction** | Design/cut wooden panel | Medium | 2–3 h | Wood board, saw, ruler, pencil | Laser cutter (optional) |
| | Drill holes for buttons, pegs | Medium | 1–2 h | Drill, 24–30 mm hole saw | Template for alignment |
| | Engrave icons for buttons & LEDs | Medium | 1–2 h | Woodburning tool or engraving pen | Templates/icons printed |
| | Install arcade buttons & LEDs | Medium | 1–2 h | Screws/nuts, washers | None |
| | Install small control buttons | Easy | 30 min | Screws/nuts | None |
| | Install wooden pegs below arcade buttons | Easy | 30 min | Pegs (wood), glue/screws | Pre-finished pegs for smooth edges |
| | Wire all components to ESP32 | Medium | 2–3 h | Wires, soldering tools | Labeling tape or heat shrink |
| **4. Software Integration** | Define ESPHome config (buttons, LEDs, OLED) | Medium | 2–3 h | ESP32, USB cable | None |
| | Subscribe to HA sensors (remaining time, battery) | Medium | 1–2 h | None | None |
| | Implement button automations to HA scripts | Medium | 2–3 h | None | None |
| | Test wake from deep sleep + all buttons | Medium | 1–2 h | None | None |
| | Test parental control limit integration | Medium | 1–2 h | None | None |
| **5. Finishing Touches** | Smooth panel edges & finish | Easy | 1–2 h | Sandpaper, varnish | Wood polish |
| | Test full system end-to-end | Medium | 2–3 h | All assembled components | Notebook for troubleshooting |
| | Create/print playlist images for pegs | Easy | 1 h | Paper, printer | Laminate sheets for durability |
| | Adjust brightness/LEDs/OLED display | Easy | 1–2 h | None | Optional diffusers for LEDs |

---

## Shopping List Summary

### Electronics
- Raspberry Pi Zero 2 W + SD card + power supply  
- ESP32 WROOM/WROVER  
- USB-C power bank (≥5,000 mAh)  
- 5 large arcade buttons (colorful, LED-compatible)  
- 3 small control buttons (Prev / Start/Stop / Next)  
- LEDs for each arcade button + resistors  
- Diodes for wake line OR logic  
- Wires, headers, optional screw terminals  
- 0.96" monochrome OLED display (I²C)  
- Voltage divider resistors for battery monitoring  

### Mechanical / Wood
- Wooden board for panel  
- Wooden pegs for playlist pictures  
- Screws/nuts/washers  
- Drill + hole saw (24–30 mm)  
- Engraving tool / woodburning pen  
- Sandpaper & varnish / polish  

### Miscellaneous
- Labeling tape / heat shrink for wires  
- Multimeter for testing voltages  

