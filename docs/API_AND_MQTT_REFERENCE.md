# Omni-IO REST API & MQTT Reference

This document details the REST endpoints, WebSocket event streams, and MQTT topics exposed by the **Omni-IO** firmware.

---

## 1. System & Information Endpoints

### `GET /api/info`
Returns general firmware and hardware information.
* **Response `200 OK`**:
```json
{
  "version": "1.0.2",
  "branch": "master",
  "chip": "ESP32-D0WDQ6",
  "flash_size": 4194304,
  "free_heap": 182340,
  "uptime": 3600
}
```

### `POST /api/restart`
Triggers an immediate soft restart of the ESP32.
* **Response `200 OK`**: `{"message":"Restarting"}`

---

## 2. Device Management Endpoints

### `GET /api/devices`
Returns the list of all registered 1W devices.
* **Response `200 OK`**:
```json
[
  {
    "id": "B60D1A",
    "name": "Living Room Shutter",
    "travel_time": 18,
    "position": 100,
    "paired": true
  }
]
```

### `POST /api/action`
Trigger an action on a specific device.
* **Request Body**:
```json
{
  "deviceId": "B60D1A",
  "action": "open" // "open", "close", "stop", "vent"
}
```

### `POST /api/command`
Executes an interactive terminal command.
* **Request Body**:
```json
{
  "deviceId": "B60D1A",
  "command": "position 50"
}
```

---

## 3. Remote Mapping Endpoints

### `GET /api/remotes`
Lists all learned physical remotes and their device associations.
* **Response `200 OK`**:
```json
[
  {
    "id": "4A1F9C",
    "name": "Wall Switch 1",
    "devices": ["B60D1A"]
  }
]
```

### `GET /api/lastaddr`
Returns the most recently intercepted 3-byte RF node address.
* **Response `200 OK`**: `{"address":"4A1F9C"}`

---

## 4. Network & WiFi Endpoints

### `GET /api/wifi`
Returns the current WiFi configuration.

### `POST /api/wifi`
Updates WiFi SSID and password.
* **Request Body**:
```json
{
  "ssid": "MyHomeNetwork",
  "password": "SecretPassword123"
}
```

### `GET /api/wifi-scan`
Triggers an asynchronous WiFi network scan and returns detected SSIDs and RSSI levels.

### `GET /api/network` & `POST /api/network`
Manages static IP configuration, DHCP toggle, hostname, SNTP server, and timezone settings.

---

## 5. Display & Screensaver Endpoints

### `GET /api/display`
Returns display configuration.
```json
{
  "enabled": true,
  "screensaverTimeout": 300,
  "screenOffTimeout": 1800,
  "dimLevel": 1,
  "showCpuTemp": true
}
```

### `POST /api/display`
Updates display preferences and stores them persistently in ESP32 NVS.

---

## 6. Backup & Restore Endpoints

### `GET /api/download/backup`
Downloads complete device list and remote mappings as an `omni-io-backup.json` file.

### `POST /api/upload/backup`
Uploads and applies a previously saved backup file.

---

## 7. WebSocket Stream (`/ws`)

Omni-IO exposes a WebSocket server at `ws://<device_ip>/ws` or `ws://omni-io.local/ws`.

* **Live Logs**: Serial & radio activity frames are broadcast as text messages.
* **Position Updates**: Broadcast in JSON format:
```json
{
  "type": "position",
  "id": "B60D1A",
  "position": 75
}
```
* **MQTT Status Updates**: Broadcast in real-time on broker connect/disconnect:
```json
{
  "type": "mqtt_status",
  "connected": true,
  "enabled": true,
  "state": "connected"
}
```
