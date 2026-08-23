# Omni-IO — Open-Source ESP32 io-homecontrol® Gateway

<div align="center">
  <img src="extras/web_interface_data/img/logo.png" alt="Omni-IO Logo" width="100"/>
  <br/>
  <strong>Next-Generation Open-Source 868MHz Gateway for io-homecontrol® Devices</strong>
  <br/><br/>

  [![Release](https://img.shields.io/github/v/release/NH-Networks/Omni-IO?style=flat-square&color=blue)](https://github.com/NH-Networks/Omni-IO/releases)
  [![PlatformIO](https://img.shields.io/badge/PlatformIO-ESP32-orange?style=flat-square)](https://platformio.org/)
  [![Home Assistant](https://img.shields.io/badge/Home%20Assistant-MQTT%20Discovery-41BDF5?style=flat-square&logo=home-assistant)](https://www.home-assistant.io/)
  [![License](https://img.shields.io/badge/License-CC%20BY--NC--ND%204.0-lightgrey?style=flat-square)](https://creativecommons.org/licenses/by-nc-nd/4.0/)
  [![BuyMeACoffee](https://img.shields.io/badge/Support-Buy%20Me%20A%20Coffee-yellow?style=flat-square&logo=buy-me-a-coffee)](https://buymeacoffee.com/dyna_mite)
</div>

---

## 🌟 Acknowledgments & Credits

**Omni-IO** stands on the shoulders of giants. This project is built upon the pioneering research, reverse engineering, and codebase contributions of several key members of the open-source home automation community:

* **[Velocet](https://github.com/Velocet)** — Author of [Velocet/iown-homecontrol](https://github.com/Velocet/iown-homecontrol): The groundbreaking reverse engineering, protocol specifications, CRC algorithms, and AES cryptography implementations for the io-homecontrol® protocol.
* **[cridp](https://github.com/cridp)** — Author of [cridp/iown-homecontrol-esp32sx1276](https://github.com/cridp/iown-homecontrol-esp32sx1276): Hardware adaptation and timing optimizations for the ESP32 and Semtech SX1276 radio transceiver.
* **[djbenbe](https://github.com/djbenbe)** — Core firmware development, UI concept, graphics, and initial integration.
* **[rspaargaren](https://github.com/rspaargaren)** — Community documentation, troubleshooting guides, and [io-homecontrol Wiki](https://github.com/rspaargaren/iohomecontrol/wiki).
* **[CloudAXS](https://github.com/CloudAXS)** — Web interface architecture, modern responsive design, and multi-language engine.

*All respective trademarks and copyrights belong to their respective owners.*

---

## ✨ Features

* 📡 **868.95 MHz io-homecontrol® Radio Engine**:
  * Native 1-Way (1W) control for shutters, blinds, screens, and Velux/Somfy window openers.
  * 2-Way (2W) protocol frame sniffing, device discovery, and temperature/mode controls for compatible HVAC systems (Atlantic / Sauter / Thermor).
* 🌐 **Modern Built-in Web UI**:
  * Fully responsive mobile & desktop web interface served directly from LittleFS.
  * Real-time WebSocket connection for instant feedback and live RF traffic log stream.
  * Multi-language support with instant live switching (**Dutch**, **English**, **German**, and **French**).
  * Direct device pairing, unpairing, renaming, and travel-time configuration.
  * Physical remote controller map manager.
* 🏠 **Home Assistant Auto-Discovery via MQTT**:
  * Automatic discovery for cover entities (blinds, screens, shutters).
  * Smooth percentage-based position control with travel time tracking.
  * Dedicated pairing and maintenance button entities.
  * Real-time state reporting (`OPEN`, `CLOSED`, `OPENING`, `CLOSING`, `STOP`).
  * Availability tracking with MQTT Last Will and Testament (LWT).
* 🖥️ **OLED Display & Advanced Screen Manager**:
  * Status display showing device name, WiFi signal, IP / mDNS address, MQTT state, and optional CPU temperature.
  * Runtime configurable screensaver timeout, screen-off timeout, and 3 dimming levels (Low, Medium, High).
* 📜 **Syslog & Remote Logging**:
  * Sends log messages directly to remote Syslog servers (RFC3164 / RFC5424 compliant).
* 💾 **Backup & Restore**:
  * One-click JSON backup and restore for device configurations and remote mappings.
  * Full backward-compatibility with legacy backup files.
* 🔄 **OTA (Over-The-Air) & Partitioning**:
  * Built-in OTA updates for both firmware and LittleFS filesystem from the web interface.

---

## 🛠️ Supported Hardware

| Hardware Board | Chip | Flash | Frequency | Notes |
| :--- | :--- | :--- | :--- | :--- |
| **LilyGo T-Beam v1.2** | ESP32 | 4MB | 868 MHz (SX1276) | Recommended / Full support |
| **LilyGo LoRa32 v2.1 (T3 v1.6.1)** | ESP32 | 4MB | 868 MHz (SX1276) | Integrated OLED |
| **Heltec LoRa32 v2** | ESP32 | 8MB | 868 MHz (SX1276) | Integrated OLED |
| **LilyGo T3-S3** | ESP32-S3 | 4MB | 868 MHz (SX1262/SX1276) | High-performance S3 SoC |

---

## 🚀 Quick Start Guide

### 1. Flash the Firmware
Download the latest ready-to-flash binaries from the [Releases](https://github.com/NH-Networks/Omni-IO/releases) page:
* Use the **merged single-file binary** (`<Board>.bin`) with [ESP Web Tools](https://esphome.github.io/esp-web-tools/) or `esptool.py`.

### 2. Connect to WiFi
1. On first boot, the ESP32 creates a setup Access Point named **`iohc-setup`**.
2. Connect to this WiFi network from your phone or PC.
3. The captive portal will open automatically. Select your home WiFi network and enter your password.

### 3. Open the Web Interface
Once connected to your network, open your web browser and navigate to:
👉 **[http://omni-io.local](http://omni-io.local)**  
*(or use the device IP address shown on the OLED screen / Serial monitor)*

---

## 🏠 Home Assistant Integration

When MQTT is enabled, Omni-IO automatically exposes your blinds and covers to Home Assistant via MQTT Discovery:

1. In the Web UI, go to **Settings → MQTT**.
2. Enter your MQTT Broker IP, port, username, and password.
3. Click **Save Settings**.
4. Home Assistant will immediately discover the covers and their control buttons!

### MQTT Topics Structure
* **Command Topic**: `iown/<ID>/set` (`OPEN`, `CLOSE`, `STOP`)
* **Position Set Topic**: `iown/<ID>/position/set` (`0` to `100`)
* **State Topic**: `iown/<ID>/state` (`OPEN`, `CLOSED`, `OPENING`, `CLOSING`, `STOP`)
* **Position Feedback**: `iown/<ID>/position` (`0` to `100`)
* **Availability**: `iown/status` (`online` / `offline`)

---

## 📖 Terminal & Serial Commands

For a full reference of commands available via the Serial Monitor and Web Terminal, see [COMMANDS.md](COMMANDS.md).

---

## ⚠️ Disclaimer

This tool is designed for educational, testing, and smart-home integration purposes and is provided "as is" without warranty of any kind. The creators and contributors are not responsible for any misuse, damage, or malfunction caused by using this software or hardware.

---

## 📄 License & Attribution

* Code licensed under open-source terms.
* Documentation & Graphic assets © 2025–2026 by [djbenbe](https://creativecommons.org) and contributors, licensed under [CC BY-NC-ND 4.0](https://creativecommons.org/licenses/by-nc-nd/4.0/).
