# Omni-IO Command Reference

This document provides a comprehensive list of all serial console and web interface terminal commands supported by the **Omni-IO** firmware.

---

## 1. 1-Way (1W) Device Commands

> [!NOTE]
> For 1W commands, use the device description/name as configured in `1W.json` or the web interface.

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `open` | `<device>` | Open device (blind/shutter) |
| `close` | `<device>` | Close device (blind/shutter) |
| `stop` | `<device>` | Stop motion |
| `position` | `<device> <0-100>` | Move to relative position percentage (0 = closed, 100 = open) |
| `absolute` | `<device> <0-100>` | Move to absolute position percentage |
| `vent` | `<device>` | Set ventilation position |
| `force` | `<device>` | Force device open |
| `pair` | `<device>` | Put device into pairing mode (updates `paired: true` in `1W.json`) |
| `add` | `<device>` | Add controller to device |
| `remove` | `<device>` | Remove controller from device (updates `paired: false` in `1W.json`) |
| `mode1` / `mode2` / `mode3` / `mode4` | `<device>` | Send special mode commands |
| `new1W` | `<name>` | Create a new 1W device entry (supports spaces in names) |
| `del1W` | `<device>` | Delete a 1W device entry |
| `edit1W` | `<device> <new_name>` | Rename a 1W device |
| `time1W` | `<device> <seconds>` | Set travel time in seconds for position tracking |
| `list1W` | | List all configured 1W devices |

---

## 2. Remote Mapping Commands

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `newRemote` | `<id> <name>` | Register a physical remote ID with a friendly name |
| `linkRemote` | `<remote_id> <device_id>` | Link a physical remote to a virtual/controlled device |
| `unlinkRemote` | `<remote_id> <device_id>` | Unlink a remote from a device |
| `delRemote` | `<remote_id>` | Delete a remote mapping entry |

---

## 3. 2-Way (2W) Commands (Sauter / Atlantic / Thermor / HVAC)

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `powerOn` | | Retrieve paired devices |
| `setTemp` | `<7.0-28.0>` (or `0`) | Set target temperature (or get current temperature with `0`) |
| `setMode` | `<auto\|prog\|manual\|off>` | Set HVAC mode (or `FF` to read mode) |
| `setPresence` | `<on\|off>` | Set presence status |
| `setWindow` | `<open\|close>` | Set window open/closed status |
| `midnight` | | Trigger midnight synchronization |
| `associate` | | Synchronize paired 2W devices |
| `discovery` | | Broadcast 2W discovery packet |
| `getName` | `<device>` | Request name of a 2W device |
| `scanMode` | | Toggle 2W scan mode |
| `scanDump` | | Dump 2W scan results to console |
| `pairMode` | | Toggle 2W pairing mode |
| `custom` / `custom60` | `<args>` | Test experimental 2W command frames |

---

## 4. System & Network Management

| Command | Arguments | Description |
| :--- | :--- | :--- |
| `lastAddr` | | Display the last received RF node address |
| `verbose` | | Toggle verbose debug logging for RF packets |
| `help` | | Display available commands |
| `ls` | | List files in LittleFS filesystem |
| `cat` | `<filename>` | Print file content to console |
| `rm` | `<filename>` | Delete file from filesystem |
| `mqttIp` | `<ip>` | Configure MQTT broker IP address |
| `mqttUser` | `<username>` | Configure MQTT username |
| `mqttPass` | `<password>` | Configure MQTT password |
| `mqttDiscovery` | `<topic>` | Configure Home Assistant discovery prefix |
