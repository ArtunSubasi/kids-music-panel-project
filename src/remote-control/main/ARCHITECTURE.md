# Kids Music Panel - RFID Remote Control System
## Architecture & Implementation Specification

---

## 1. System Overview

The Kids Music Panel is an ESP32-based RFID card reader system that allows children to play music by scanning NFC/RFID cards. The system integrates with a Music Assistant API to control media playback.

### Key Features
- **RFID Card Scanning**: RC522-based reader on SPI3
- **Visual Feedback**: SSD1306 OLED display on SPI2
- **WiFi Connectivity**: Connects to Music Assistant service
- **Media Mapping**: UID-to-media-ID database
- **Event-Driven Architecture**: FreeRTOS task-based

---

## 2. Hardware Architecture

### 2.1 SPI Bus Configuration

#### SPI2 Host (OLED Display)
| Component | Pin | Purpose |
|-----------|-----|---------|
| MOSI | GPIO 23 | Data output to display |
| CLK | GPIO 18 | Clock signal |
| CS | GPIO 5 | Chip select |
| DC | GPIO 17 | Data/Command control |
| RST | GPIO 16 | Reset signal |

#### SPI3 Host (RFID Reader)
| Component | Pin | Purpose |
|-----------|-----|---------|
| MISO | GPIO 12 | Data input from reader |
| MOSI | GPIO 13 | Data output to reader |
| SCLK | GPIO 14 | Clock signal |
| CS/SDA | GPIO 15 | Chip select |
| RST | GPIO 27 | Reset signal |

### 2.2 Component List
- **MCU**: ESP32
- **OLED**: SSD1306 (128x64, SPI)
- **RFID Reader**: RC522 (SPI)
- **WiFi**: Built-in ESP32 WiFi module

---

## 3. Software Architecture

### 3.1 Core Modules

#### A. WiFi Manager (`wifi_manager.h/c`)
**Responsibility**: Handle WiFi connectivity and state management
- Initialization with menuconfig credentials
- Connection state tracking (connected/disconnected/failed)
- Event-based handler for state changes
- Display updates during WiFi operations

**Current Status**: ✅ Implemented in main
**Required Changes**: Extract into separate module

#### B. Display Manager (`display_manager.h/c`)
**Responsibility**: Manage OLED display output
- Initialize SSD1306 on SPI2
- Queue-based display updates (thread-safe)
- Status screens:
  - Waiting for card
  - Card detected (type + UID)
  - WiFi connecting/connected/failed
  - Media playback confirmation

**Current Status**: ✅ Partially implemented
**Required Changes**: Create dedicated module with message queue

#### C. RFID Scanner (`rfid_scanner.h/c`)
**Responsibility**: Handle RC522 operations
- Initialize RC522 on SPI3
- Register event callbacks
- Process card state changes (inserted/removed)
- Extract UID and card type information

**Current Status**: ✅ Implemented in main
**Required Changes**: Extract into separate module

#### D. Media Mapper (`media_mapping.h/c`)
**Responsibility**: Map RFID UIDs to Media IDs
- Maintain UID↔MediaID lookup table
- Support multiple storage backends:
  - **Phase 1**: Hard-coded array
  - **Phase 2**: NVS (Non-Volatile Storage)
  - **Phase 3**: JSON configuration

**Current Status**: ✅ Header exists, implementation needed
**Required Changes**: Implement lookup and storage logic

#### E. Music Assistant Client (`music_assistant_client.h/c`)
**Responsibility**: HTTP communication with Music Assistant API
- Construct API requests
- Handle authentication (Bearer token)
- Send play_media requests
- Error handling and retry logic
- Response parsing

**Current Status**: ✅ Basic implementation in main
**Required Changes**: Extract and enhance with error handling

#### F. Configuration Manager (`config.h`)
**Responsibility**: Centralize all configurable values
- Menuconfig integration points
- Pin definitions
- API endpoints
- Timing constants

**Current Status**: ⚠️ Scattered throughout main
**Required Changes**: Consolidate into single header

### 3.2 Data Structures

```c
// RFID Card Info
typedef struct {
    rc522_picc_uid_t uid;
    rc522_picc_type_t type;
    char uid_str[RC522_PICC_UID_STR_BUFFER_SIZE_MAX];
} rfid_card_t;

// Media Mapping Entry
typedef struct {
    uint8_t uid_bytes[10];
    uint8_t uid_len;
    const char *media_id;
    const char *friendly_name;
} media_mapping_entry_t;

// Display Update Message
typedef struct {
    char line1[32];
    char line2[32];
    uint32_t duration_ms; // 0 = indefinite
} display_message_t;
```

### 3.3 Event Flow

```
[Card Scanned] 
    ↓
[RC522 ISR] → [on_rfid_tag_scanned event]
    ↓
[Extract UID & Type]
    ↓
[Display Update: Show Card Info]
    ↓
[Lookup Media ID in Mapping]
    ↓
[If Found] → [HTTP POST to Music Assistant]
    ↓
[Update Display: Playing / Error]
```

---

## 4. Implementation Roadmap

### Phase 1: Modularization (Current)
- [ ] Extract WiFi logic → `wifi_manager.c`
- [ ] Extract Display logic → `display_manager.c`
- [ ] Extract RFID logic → `rfid_scanner.c`
- [ ] Extract HTTP logic → `music_assistant_client.c`
- [ ] Implement basic `media_mapping.c`
- [ ] Create `config.h` header

### Phase 2: Message Queue Integration
- [ ] Add display message queue (FreeRTOS queue)
- [ ] Implement async display updates
- [ ] Add RFID event queue for debouncing
- [ ] Thread-safe logging

### Phase 3: Error Handling & Resilience
- [ ] HTTP timeout handling
- [ ] WiFi reconnection logic
- [ ] Display error codes
- [ ] Graceful degradation

### Phase 4: NVS Storage
- [ ] Migrate media mappings to NVS
- [ ] Support hot-reloading of mappings
- [ ] Backup to flash

### Phase 5: JSON Configuration
- [ ] Support JSON config file
- [ ] SPIFFS or LittleFS integration
- [ ] Web-based mapping editor

---

## 5. Code Organization

```
src/remote-control/
├── main/
│   └── main.c         (entry point)
├── components/
│   ├── config/
│   │   ├── config.h
│   │   └── config.c
│   ├── wifi_manager/
│   │   ├── wifi_manager.h
│   │   └── wifi_manager.c
│   ├── display_manager/
│   │   ├── display_manager.h
│   │   └── display_manager.c
│   ├── rfid_scanner/
│   │   ├── rfid_scanner.h
│   │   └── rfid_scanner.c
│   ├── music_assistant/
│   │   ├── music_assistant_client.h
│   │   └── music_assistant_client.c
│   └── media_mapping/
│       ├── media_mapping.h
│       └── media_mapping.c
└── docs/
    ├── ARCHITECTURE.md             (this file)
    ├── API.md
    └── BUILD.md
```

---

## 6. Configuration (menuconfig)

Required configuration points:
```
MUSIC_ASSISTANT_HOST        (string) - API endpoint
MUSIC_ASSISTANT_API_KEY     (string) - Bearer token
WIFI_SSID                   (string) - WiFi network
WIFI_PASSWORD               (string) - WiFi password
DEVICE_ID                   (string) - Unique device identifier
```

---

## 7. Testing Strategy

### Unit Tests
- Media mapping lookup
- UID-to-string conversion
- Configuration parsing

### Integration Tests
- WiFi connection flow
- RFID card detection
- HTTP request sending
- Display message queue

### Manual Tests
- Card scanning with valid mapping
- Card scanning with invalid UID
- WiFi disconnect/reconnect
- Display visibility in different lighting

---

## 8. Dependencies

- **FreeRTOS**: Task management, queues, event groups
- **ESP-IDF**: WiFi, NVS, HTTP client, logging
- **RC522 Driver**: "abobija" rc522-rfid component
- **SSD1306 Driver**: SPI-based OLED library
- **cJSON** (future): JSON parsing for config

---

## 9. Performance Targets

| Metric | Target |
|--------|--------|
| Card detection latency | < 1s |
| HTTP request timeout | 5s |
| Display update latency | < 100ms |
| WiFi reconnection time | < 10s |
| Memory footprint | < 500KB (heap) |

---

## 10. Security Considerations

- ✅ Bearer token authentication for API
- ⚠️ WiFi credentials in plaintext (menuconfig)
- TODO: UID validation before API call
- TODO: Rate limiting on API requests
- TODO: Encrypted storage for sensitive data

---

## Revision History

| Date | Version | Author | Changes |
|------|---------|--------|---------|
| 2025-01-XX | 0.1 | Initial | Architecture specification |

