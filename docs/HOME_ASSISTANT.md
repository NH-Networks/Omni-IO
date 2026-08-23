# Home Assistant Integration Guide

Omni-IO features native **MQTT Discovery** for Home Assistant, providing zero-configuration setup for blinds, shutters, screens, and window openers.

---

## 1. Prerequisites

1. An operational **MQTT Broker** in Home Assistant (e.g., the official Mosquitto broker add-on).
2. Omni-IO connected to your local WiFi network.

---

## 2. Configuration in Omni-IO

1. Open the Omni-IO Web UI at **[http://omni-io.local](http://omni-io.local)**.
2. Navigate to **Settings → MQTT Settings**.
3. Fill in the following fields:
   * **MQTT Server / IP**: Your Home Assistant IP (e.g., `192.168.1.50`).
   * **Port**: `1883` (default).
   * **Username**: Your MQTT username.
   * **Password**: Your MQTT password.
   * **Discovery Prefix**: `homeassistant` (default).
4. Click **Save Settings**.

Once connected, Omni-IO will publish discovery payloads for all configured devices.

---

## 3. Discovered Entities

For each device added to Omni-IO (e.g., `Living Room Shutter` with ID `B60D1A`), Home Assistant automatically creates:

| Entity Type | Entity ID | Purpose |
| :--- | :--- | :--- |
| **Cover** | `cover.living_room_shutter` | Full Open / Close / Stop / Position percentage control |
| **Number** | `number.living_room_shutter_travel_time` | Runtime calibration of motor travel duration |
| **Button** | `button.living_room_shutter_pair` | Trigger motor pairing mode |
| **Button** | `button.living_room_shutter_add` | Send register controller command |
| **Button** | `button.living_room_shutter_remove` | Send unpair/remove command |

---

## 4. Example Dashboard Card (Lovelace)

### Tile Card Configuration
```yaml
type: tile
entity: cover.living_room_shutter
features:
  - type: cover-open-close
  - type: cover-position-slider
```

### Entities Card with Controls
```yaml
type: entities
title: Blinds & Shutters
entities:
  - entity: cover.living_room_shutter
    name: Living Room Shutter
  - entity: number.living_room_shutter_travel_time
    name: Travel Time (sec)
```

---

## 5. Automation Examples

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

---

## 6. MQTT Topics & Payloads

Omni-IO adheres to the standard `iown` topic tree:

* **State**: `iown/<ID>/state` (`OPEN`, `CLOSED`, `OPENING`, `CLOSING`, `STOP`)
* **Position Feedback**: `iown/<ID>/position` (`0` to `100`)
* **Command**: `iown/<ID>/set` (`OPEN`, `CLOSE`, `STOP`)
* **Position Set**: `iown/<ID>/position/set` (`0` to `100`)
* **Availability**: `iown/status` (`online` / `offline`)
