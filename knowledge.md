# Omni-IO — Architecture, Development & Operational Knowledge Base

This document captures the comprehensive architectural decisions, protocol implementations, hardware quirks, and operational knowledge accumulated during the development and stabilization of the **Omni-IO** project, with special emphasis on the **ESPHome Native API**, **MQTT Integration**, **OLED Display Subsystem**, and **io-homecontrol Radio Protocol**.

---

## 1. System Architecture Overview

```
                                +-------------------------------------------+
                                |               Home Assistant              |
                                +--------------------+----------------------+
                                                     |
                         [ESPHome API (TCP 6053)]    |    [MQTT (Broker 1883)]
                         (Zero Broker / Instant)     |    (Topic Pub/Sub)
                                                     v
+----------------------------------------------------------------------------------------------------+
|                                      Omni-IO Gateway (ESP32)                                       |
|                                                                                                    |
|  +--------------------------------+   +---------------------------------+   +-------------------+  |
|  |     ESPHome Native Server      |   |          MQTT Handler           |   |   Async Web API   |  |
|  |     (FreeRTOS Task Core 1)     |   |      (AsyncMqttClient)          |   |    (ESPAsyncWeb)  |  |
|  +---------------+----------------+   +----------------+----------------+   +---------+---------+  |
|                  |                                     |                              |            |
|                  +------------------+------------------+------------------------------+            |
|                                     |                                                              |
|                                     v                                                              |
|                       +---------------------------+                                                |
|                       |   iohcRemote1W Controller |  <--->  NVS Storage (LittleFS / Preferences)   |
|                       +-------------+-------------+                                                |
|                                     |                                                              |
|                                     v                                                              |
|                       +---------------------------+                                                |
|                       |      iohcRadio Layer      |                                                |
|                       |     (SX1262 / CC1101)     |                                                |
|                       +-------------+-------------+                                                |
|                                     |                                                              |
|                                     v                                                              |
|                        OLED Display Subsystem (SSD1306 I2C 128x64)                                 |
+----------------------------------------------------------------------------------------------------+
                                      |
                                  868.95 MHz
                                      |
                                      v
                      [io-homecontrol 1W Blind / Motor]
```

---

## 2. ESPHome Native API Implementation

### 2.1 Protocol Specifications & Plaintext Framing
* **Transport**: Direct TCP connection on port `6053` (`esphome_port`).
* **Framing Structure**:
  ```
  [0x00] [varint payload_len] [varint msg_type] [protobuf_payload]
  ```
  * `0x00`: Plaintext framing indicator (ESPHome noise encryption is intentionally not mandated, enabling lightweight embedded C++ implementation without heavy crypto dependencies).
  * `varint payload_len`: Protobuf varint representing the length of `msg_type` varint + `protobuf_payload`.
  * `varint msg_type`: Unique protocol message ID (e.g. 1 = `HELLO_REQUEST`, 2 = `HELLO_RESPONSE`, etc.).
  * `protobuf_payload`: Serialized protobuf fields according to the ESPHome API protocol schema.

### 2.2 Core Message IDs
| ID | Name | Direction | Purpose |
| :--- | :--- | :--- | :--- |
| **1** | `HelloRequest` | HA $\rightarrow$ Omni | Handshake initiation containing client version & name |
| **2** | `HelloResponse` | Omni $\rightarrow$ HA | Server identification & protocol version (`1.10`) |
| **3** | `ConnectRequest` | HA $\rightarrow$ Omni | Password authentication check |
| **4** | `ConnectResponse` | Omni $\rightarrow$ HA | Authentication result (`invalid_password: bool`) |
| **5** | `DisconnectRequest` | HA $\rightarrow$ Omni | Client graceful disconnect |
| **6** | `DisconnectResponse` | Omni $\rightarrow$ HA | Server disconnect confirmation |
| **7** | `PingRequest` | Both | Keepalive ping (heartbeat) |
| **8** | `PingResponse` | Both | Keepalive pong |
| **9** | `DeviceInfoRequest` | HA $\rightarrow$ Omni | Query gateway hardware & firmware metadata |
| **10**| `DeviceInfoResponse` | Omni $\rightarrow$ HA | Friendly name, model (`Omni-IO Gateway`), MAC, version |
| **11**| `ListEntitiesRequest` | HA $\rightarrow$ Omni | Enumeration trigger for discovered entities |
| **12**| `ListEntitiesDoneResponse` | Omni $\rightarrow$ HA | Signal end of entity enumeration |
| **19**| `ListEntitiesCoverResponse` | Omni $\rightarrow$ HA | Cover entity schema definition |
| **20**| `CoverStateResponse` | Omni $\rightarrow$ HA | Current cover position / state broadcast |
| **21**| `CoverCommandRequest` | HA $\rightarrow$ Omni | Open, Close, Stop, or Absolute Position command |
| **26**| `ListEntitiesNumberResponse` | Omni $\rightarrow$ HA | Number entity schema (e.g. travel time slider) |
| **27**| `NumberStateResponse` | Omni $\rightarrow$ HA | Live number value broadcast |
| **28**| `NumberCommandRequest` | HA $\rightarrow$ Omni | User slider modification from HA |
| **29**| `ListEntitiesButtonResponse` | Omni $\rightarrow$ HA | Button entity schema (e.g. Pair / Add / Remove) |
| **31**| `ButtonCommandRequest` | HA $\rightarrow$ Omni | Button trigger from HA |
| **34**| `ListEntitiesSensorResponse` | Omni $\rightarrow$ HA | Sensor entity schema (RSSI, Free Heap, IP) |
| **35**| `SensorStateResponse` | Omni $\rightarrow$ HA | Numeric sensor state updates |
| **36**| `ListEntitiesTextSensorResponse` | Omni $\rightarrow$ HA | Text sensor entity schema |
| **37**| `TextSensorStateResponse` | Omni $\rightarrow$ HA | Text sensor state broadcast |
| **40**| `SubscribeStatesRequest` | HA $\rightarrow$ Omni | Subscription handshake: triggers full state dump |

### 2.3 Stable Entity Keys (FNV-1a Hash)
* Home Assistant requires a persistent 32-bit unsigned integer `key` for every entity.
* If keys change across reboots, Home Assistant treats them as orphaned entities and creates duplicates (`_2`, `_3`).
* **Implementation**: Uses 32-bit FNV-1a hashing on stable strings:
  ```cpp
  uint32_t espHomeFnv1a(const std::string &str) {
      uint32_t hash = 2166136261u;
      for (unsigned char c : str) {
          hash ^= c;
          hash *= 16777619u;
      }
      return hash;
  }
  ```
* **Key Namespaces**:
  * Cover: `"cover_" + lowercase(deviceId)`
  * Travel Time: `"travel_time_" + lowercase(deviceId)`
  * Pair Button: `"btn_pair_" + lowercase(deviceId)`
  * Add Button: `"btn_add_" + lowercase(deviceId)`
  * Remove Button: `"btn_remove_" + lowercase(deviceId)`
  * Diagnostic Sensors: `"omni_wifi_rssi"`, `"omni_free_heap"`, `"omni_ip_address"`

### 2.4 Dynamic Discovery & Re-enumeration Architecture
* **The Problem**: In Home Assistant's `aioesphomeapi`, `client.list_entities_services()` is **only** invoked during the initial connection handshake. Spontaneous `ListEntities*` messages sent over an existing socket are ignored by the Python client.
* **The Solution (`syncEspHomeDevices()`)**:
  When a device is added, deleted, or renamed in Omni-IO:
  1. `syncEspHomeDevices()` iterates through all active client sessions (`MAX_CLIENTS = 4`).
  2. Gracefully closes their sockets.
  3. Home Assistant's client detects the disconnect and reconnects automatically within 1.0–1.5 seconds.
  4. During the reconnection handshake, HA calls `ListEntitiesRequest`, discovering newly added devices and purging deleted ones without restarting Home Assistant or reloading the integration manually.
* **Direct State Broadcasts (`notifyEspHomeTravelTime()`)**:
  When travel time is adjusted on a slider, it does **not** re-enumerate entities. Instead, it emits a `NumberStateResponse` over the active connection, providing zero-latency live updates in Home Assistant.

---

## 3. io-homecontrol (1W) Radio Commands & States

### 3.1 RF Command Frames
Commands transmitted via SX1262 / CC1101 on 868.95 MHz:
* **Open (100%)**: Command `0x00`
* **Close (0%)**: Command `0xC8`
* **Stop**: Dedicated STOP command sequence
* **Absolute Position**: Calculated dynamically based on `travelTime`:
  $$\Delta t = |\text{targetPosition} - \text{currentPosition}| \times \frac{\text{travelTime}}{100}$$
  The blind moves toward the target and stops automatically after $\Delta t$ seconds.
* **Pair Remote**: Command `0x2E`
* **Add Remote**: Command `0x30`
* **Remove Remote**: Command `0x39`

### 3.2 Security & Authentication
* Each 1W remote has a 3-byte unique ID and a 16-byte transfer key stored in NVS (`1W.json`).
* Messages include HMAC-MD5 signatures and sequential rolling counters (`sequence`) to prevent replay attacks.
* Counter increments on every transmitted command and persists across reboots.

---

## 4. OLED Display Subsystem (SSD1306 128x64)

### 4.1 Header / Status Bar Layout (Y: 0 to 10)
```
+-----------------------------------------------------------------------+
| [Omni-IO Logo] Omni-IO       [CPU: 45C]         [HA Icon]  [WiFi Bars]|
| (x=1, y=1)     (x=20, y=4)   (x=62/74, y=4)     (x=105)    (x=119)    |
+-----------------------------------------------------------------------+
|                                                                       |
|                     (Main Content / Log Area)                         |
|                     Lines: y=20 to y=55                               |
|                                                                       |
+-----------------------------------------------------------------------+
| IP: 10.10.33.15 / http://omni-io.local                       (y=56)   |
+-----------------------------------------------------------------------+
```

### 4.2 Pixel Art Bitmaps & Dimensions
1. **Omni-IO Logo (`omniIoLogo`)**:
   - Position: `x = 1, y = 1`
   - Size: 16×10 pixels
   - Visual: A house with the letters "IO" in the doorway (original CRIDP project brand).
2. **ESPHome / Home Assistant Status Icon (`espHomeIcons`)**:
   - Position: `x = 105, y = 3` (left of WiFi bars at `x = 119`, vertically aligned `y=3..9`).
   - Size: 11×7 pixels
   - **Connected State (`clients > 0`)**: Solid, filled house with chimney and doorway:
     ```
     ....##..#..  (chimney on right)
     ...####.#..
     ..#######..
     .#########.  (roof eave)
     ..##...##..  (walls + door opening)
     ..##...##..
     ..#######..  (base)
     ```
   - **Waiting State (`clients == 0`)**: Outline house with chimney:
     ```
     ....##..#..
     ...#..#.#..
     ..#....##..
     .#########.
     ..#.....#..
     ..#.....#..
     ..#######..
     ```
3. **MQTT Status Icon (`mqttIcons`)**:
   - Size: 16×5 pixels
   - Visual: Chain links (0 = connecting, 1 = connected, 2 = broken/disconnected).
   - Dynamic Offset: When both MQTT and ESPHome are enabled, ESPHome shifts to `x=89` and CPU temp moves to `x=62`, preventing any visual overlap.
4. **WiFi Signal Bars (`wifiIcons`)**:
   - Position: `x = 119, y = 3`
   - Size: 8×7 pixels (4 signal levels: 0%, 25%, 50%, 75%, 100%).

### 4.3 Power Management & Screensaver Modes
* `displayEnabled`: Global toggle in NVS (`NVS_KEY_DISPLAY_ENABLED`).
* `screensaverTimeout`: Default 60 seconds (`NVS_KEY_DISPLAY_SCREENSAVER`).
  * When no radio data or user interaction occurs for 60s, `drawLogo()` enters screensaver mode (dimmed display, floating logo + IP at random screen coordinates).
* `screenOffTimeout`: Default 3600 seconds. Shuts display panel off completely.
* `wakeDisplay()` & Inactivity Timing:
  * Inactivity timer (`lastDataTime`): Reset on radio TX / RX frame events (`displayCustomMessage()`) and interactive Web API commands (`_jsonPost`, `WS_EVT_CONNECT`, `WS_EVT_DATA`).
  * Background protection: Automatic browser WebSocket `WS_EVT_PONG` heartbeats, read-only HTTP GET requests, and ESPHome client connects/disconnects do NOT wake the display (they call `updateDisplayStatus()` to refresh icons without resetting sleep timers).
  * Hardware I2C Contrast: Set via `setDisplayContrast(contrast)` in a single I2C transmission (`0x00`, `0x81`, `contrast`), preventing SSD1306 command decoder aborts caused by split transactions.
  * Dim levels: Level 0 = `1` (low/dark room), Level 1 = `32` (medium), Level 2 = `80` (high). Active screensavers immediately adapt to runtime dim level changes.

---

## 5. NVS Configuration Keys & Web Endpoints

### 5.1 NVS Preference Keys
| Key | Type | Default | Description |
| :--- | :--- | :--- | :--- |
| `mqtt_en` | bool | `true` | Enable / disable MQTT client subsystem |
| `mqtt_server` | string | `""` | MQTT Broker hostname / IP |
| `mqtt_port` | u16 | `1883` | MQTT Broker port |
| `mqtt_user` | string | `"mosquitto"` | MQTT Username |
| `mqtt_pwd` | string | `""` | MQTT Password |
| `mqtt_topic` | string | `"homeassistant"` | MQTT Auto-Discovery prefix topic |
| `esphome_en` | bool | `true` | Enable / disable ESPHome native API server |
| `esphome_pwd` | string | `""` | Optional API password |
| `disp_en` | bool | `true` | Enable / disable OLED display task |
| `disp_ss` | u16 | `60` | Screensaver timeout (seconds) |
| `disp_off` | u16 | `3600` | Screen off timeout (seconds) |
| `disp_temp` | bool | `true` | Show on-chip CPU temperature in header |

### 5.2 REST Web API Endpoints
* `GET /api/info`: Comprehensive device metadata, heap, WiFi RSSI, MQTT status, ESPHome status.
* `GET /api/devices`: List all configured 1W remotes, travel times, and sequence counters.
* `POST /api/mqtt`: Update MQTT config (`{"enabled": bool, "server": "...", "port": ...}`).
* `POST /api/esphome`: Update ESPHome config (`{"enabled": bool, "password": "...", "name": "..."}`).
* `POST /api/firmware`: Multipart OTA firmware binary upload (`..._firmware.bin`).
* `POST /api/filesystem`: Multipart OTA LittleFS binary upload (`..._filesystem.bin`).
* `GET /api/logs`: In-memory rolling circular buffer containing recent system log lines.

### 5.3 WebSocket Events (`/ws`)
* `esphome_status`: Broadcasts live ESPHome server state (`enabled`, `running`, `clients`, `port`, `connected`, `state`). Controls the `#esphome-status-pill` in the web navbar.
* `mqtt_status`: Broadcasts live MQTT client state (`connected`, `enabled`, `state`). Controls the `#mqtt-status-pill` in the web navbar.
* `position`: Real-time position tracking (`{"id": "...", "position": ...}`).
* `deviceaction`: Radio activity log (`action`, `position`, `target`, `source`).
* `log`: System trace and debug lines.

### 5.4 Web Navigation Bar Status Pills
* **MQTT Pill (`#mqtt-status-pill`)**: Shows dot status (green = connected, yellow = connecting, grey = disabled, red = disconnected). Clicking opens Settings → MQTT.
* **ESPHome Pill (`#esphome-status-pill`)**: Shows dot status (green pulsating = connected with client count, yellow = listening on port, grey = disabled, red = disconnected). Clicking opens Settings → Integration.

---

## 6. Home Assistant Integration Guidance

### 6.1 ESPHome Native vs MQTT Comparison
| Feature | ESPHome Native API | MQTT Auto-Discovery |
| :--- | :--- | :--- |
| **Broker Requirement** | None (Direct TCP connection) | Requires external Mosquitto broker |
| **Latency** | Instantaneous (direct socket) | Network hop via MQTT broker |
| **Device Representation** | 1 Device (`Omni-IO Gateway`) with child entities | Split into separate devices per blind |
| **Entity Types** | Cover, Number, Button, Sensors | Cover, Number, Sensor |
| **Authentication** | Optional API password | Username / Password |
| **Entity Cleanup** | Dynamic on reconnect | Stale MQTT discovery retained until retained topic cleared |

### 6.2 Preventing Entity Conflicts
When switching a running device from MQTT to ESPHome:
1. In the Omni-IO Web UI under **Settings**, toggle **MQTT: Disabled** first (or `POST /api/mqtt {"enabled": false}`).
2. In Home Assistant, delete the old MQTT devices/entities under **Settings $\rightarrow$ Devices & Services $\rightarrow$ MQTT**.
3. Add the **ESPHome** integration pointing to the gateway's IP address (`10.10.33.15`) on port `6053`.
4. All entities populate cleanly under a single unified `Omni-IO Gateway` device.

---

## 7. Multi-Target Hardware Environments

| Environment | Board Definition | Target MCU | Radio Module | Primary Flash Pins |
| :--- | :--- | :--- | :--- | :--- |
| `LilyGoT3S3` | `lilygo-t3-s3` | ESP32-S3 | SX1262 | Standard S3 Octal/Quad SPI |
| `LilyGoTBeamV12ESP32` | `ttgo-t-beam` | ESP32-D0WDQ6 | SX1262 / SX1276 | SPI (SCK: 5, MISO: 19, MOSI: 27, CS: 18) |
| `LilyGoLoraESP32` | `ttgo-lora32-v21`| ESP32-PICO-D4 | SX1276 | SPI (SCK: 5, MISO: 19, MOSI: 27, CS: 18) |
| `HeltecLoraV2ESP32` | `heltec_wifi_lora_32_V2` | ESP32-D0WDQ6 | SX1276 | Integrated 0.96" OLED on I2C `0x3C` |

### 7.1 OTA Firmware Update Script Pattern
Always dynamically discover the newest binary via `glob` to avoid uploading stale cached builds:
```python
import glob, urllib.request

firmware_files = sorted(glob.glob(r"scratch/ota_binaries/*_firmware.bin"))
filesystem_files = sorted(glob.glob(r"scratch/ota_binaries/*_filesystem.bin"))

latest_firmware = firmware_files[-1]
latest_filesystem = filesystem_files[-1]
```
Upload sequence:
1. Upload firmware to `POST /api/firmware` $\rightarrow$ wait for device reboot (~7s).
2. Upload filesystem to `POST /api/filesystem` $\rightarrow$ wait for device reboot (~7s).
3. Query `GET /api/info` to verify clean version reporting and client reconnection.
