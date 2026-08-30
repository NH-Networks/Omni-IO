<!--
SPDX-FileCopyrightText: 2026 CloudAXS
SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
-->
# Home Assistant Integration Guide

Omni-IO provides two seamless integration paths into **Home Assistant**:
1. **ESPHome Native API (Recommended)** — Instant, zero-broker, direct TCP socket with automatic mDNS discovery.
2. **MQTT Auto-Discovery** — Traditional MQTT discovery via an external broker (e.g. Mosquitto).

---

## 🚀 Option A: ESPHome Native API (Recommended — Zero-Broker)

Starting with **Omni-IO v3.0.0**, the firmware includes a native embedded **ESPHome API server** running directly on TCP port `6053`.

### Key Advantages
* **Zero External Dependencies**: No MQTT broker (Mosquitto) required.
* **Instantaneous Latency**: Direct bidirectional TCP socket between Home Assistant and the ESP32.
* **Automatic Discovery**: Discovered automatically in Home Assistant via mDNS (`_esphomelib._tcp.local.`).
* **Unified Device Structure**: All covers, sliders, buttons, and diagnostic sensors are cleanly grouped under a single **`Omni-IO Gateway`** device in Home Assistant.
* **Dynamic Re-enumeration**: Adding, editing, or deleting a device in the Omni-IO web interface automatically updates Home Assistant within 1.5 seconds without restarting HA or reloading integrations.

### Setup Instructions
1. Ensure Omni-IO and your Home Assistant server are connected to the same local network.
2. In Home Assistant, navigate to **Settings → Devices & Services**.
3. Omni-IO will appear in the **Discovered** section as an **ESPHome** device (`omni-io`).
4. Click **Configure** and then **Submit**. (If you set an API password in Omni-IO Web Settings, enter it when prompted).
5. All devices and entities will populate immediately!

> [!TIP]
> If mDNS discovery is blocked by your router or cross-VLAN firewall, click **Add Integration → ESPHome** in Home Assistant and enter your Omni-IO device's IP address (e.g. `10.10.33.15`) with port `6053`.

### Discovered Entities (ESPHome)

For every paired screen/shutter (e.g. `Living Room Shutter` with ID `B60D1A`), Home Assistant automatically exposes:

| Entity Type | Entity ID | Purpose |
| :--- | :--- | :--- |
| **Cover** | `cover.living_room_shutter` | Open, Close, Stop, and percentage position control (`device_class: shutter`) |
| **Number** | `number.living_room_shutter_travel_time` | Interactive slider to adjust motor travel time in seconds |
| **Button** | `button.living_room_shutter_pair` | Sends the 1W PROG/Pair command (`0x2E`) to the physical motor |
| **Button** | `button.living_room_shutter_add` | Sends the 1W Add Remote command (`0x30`) |
| **Button** | `button.living_room_shutter_remove` | Sends the 1W Remove Remote command (`0x39`) |

#### Gateway Diagnostics & Controls:
| Entity Type | Entity ID | Purpose |
| :--- | :--- | :--- |
| **Sensor** | `sensor.omni_wifi_rssi` | Gateway WiFi Signal Strength (dBm) |
| **Sensor** | `sensor.omni_free_heap` | Gateway Free Heap Memory (Bytes) |
| **Text Sensor** | `sensor.omni_ip_address` | Gateway Local IP Address |
| **Button** | `button.omni_io_restart` | Triggers a soft reboot of the Omni-IO Gateway |

---

## 📡 Option B: MQTT Auto-Discovery

If your smart-home architecture relies on an existing MQTT broker, Omni-IO can publish standard Home Assistant MQTT discovery topics.

### Prerequisites
1. An active **MQTT Broker** (e.g. Mosquitto add-on in Home Assistant).
2. Omni-IO connected to your local WiFi network.

### Configuration in Omni-IO
1. Open the Omni-IO Web UI at **[http://omni-io.local](http://omni-io.local)**.
2. Go to **Settings → MQTT Settings**.
3. Enable MQTT and configure:
   * **MQTT Server / IP**: Your Home Assistant or MQTT broker IP.
   * **Port**: `1883` (default).
   * **Username / Password**: Your broker credentials.
   * **Discovery Prefix**: `homeassistant` (default).
4. Click **Save Settings**.

### MQTT Topics & Payloads
Omni-IO follows the standard `iown` topic hierarchy:
* **State**: `iown/<ID>/state` (`OPEN`, `CLOSED`, `OPENING`, `CLOSING`, `STOP`)
* **Position Feedback**: `iown/<ID>/position` (`0` to `100`)
* **Command**: `iown/<ID>/set` (`OPEN`, `CLOSE`, `STOP`)
* **Position Set**: `iown/<ID>/position/set` (`0` to `100`)
* **Availability**: `iown/status` (`online` / `offline`)

---

## 🔄 Migrating from MQTT to ESPHome Native API

To switch from MQTT to the new ESPHome Native API without duplicate or orphaned entities:

1. **Disable MQTT in Omni-IO**:
   * In the Web UI under **Settings → MQTT Settings**, toggle **Enable MQTT** to **Off** and click **Save Settings**.
2. **Remove Old MQTT Entities in Home Assistant**:
   * Go to **Settings → Devices & Services → MQTT**.
   * Locate the previous Omni-IO devices/covers and select **Delete**.
3. **Add the ESPHome Device**:
   * Go to **Settings → Devices & Services**.
   * Accept the discovered **ESPHome** device (`omni-io`).
4. All entities will populate cleanly under the unified `Omni-IO Gateway` device with zero stale entries!

---

## 🎛️ Example Dashboard Cards (Lovelace)

### Modern Tile Card
```yaml
type: tile
entity: cover.living_room_shutter
features:
  - type: cover-open-close
  - type: cover-position-slider
```

### Entities Card with Travel Time Calibration
```yaml
type: entities
title: Living Room Shutter
show_header_toggle: false
entities:
  - entity: cover.living_room_shutter
    name: Shutter Position
  - entity: number.living_room_shutter_travel_time
    name: Travel Time (seconds)
  - type: divider
  - entity: button.living_room_shutter_pair
    name: Pair Motor (Prog)
```

---

## ⚡ Automation Examples

### Automation 1: Close all shutters at sunset
```yaml
alias: "Shutters: Close at Sunset"
trigger:
  - platform: sun
    event: sunset
    offset: "00:15:00"
action:
  - service: cover.close_cover
    target:
      entity_id:
        - cover.living_room_shutter
        - cover.bedroom_shutter
```

### Automation 2: Open blinds at 07:30 on weekdays
```yaml
alias: "Shutters: Weekday Morning Open"
trigger:
  - platform: time
    at: "07:30:00"
condition:
  - condition: time
    weekday:
      - mon
      - tue
      - wed
      - thu
      - fri
action:
  - service: cover.open_cover
    target:
      entity_id: cover.living_room_shutter
```

