# Omni-IO Troubleshooting Guide

This guide provides solutions to common issues encountered during setup, WiFi connection, RF pairing, or Home Assistant integration.

---

## 1. WiFi & Captive Portal Issues

### The `iohc-setup` WiFi access point does not appear
* **Cause**: The ESP32 already has stored WiFi credentials and is attempting to connect to your network.
* **Solution**: 
  * If the network is not reachable, the fallback manager will start the AP after the configured retry count (default: 3 attempts).
  * You can force the device into AP mode by holding the boot button during startup or erasing NVS using `esptool.py erase_flash`.

### Cannot open `http://omni-io.local` (mDNS)
* **Cause**: Some mobile networks (especially Android) or local routers do not resolve multicast DNS (`.local` addresses).
* **Solution**:
  * Check the OLED display or your router's DHCP client table to find the device's IP address (e.g., `http://192.168.1.120`).
  * On Windows, ensure the **Bonjour Service** or mDNS resolution is enabled.

---

## 2. Radio & Pairing Issues

### The motor does not jog when pressing "Pair"
1. **Check Pairing Window**: When you press the **PROG** button on your physical remote, the motor must perform a physical up-and-down movement (*jog*). Once it jogs, you have a **2-minute window** to click **Pair** in Omni-IO.
2. **Check Antenna**: Ensure an 868 MHz antenna is firmly attached to the SMA connector.
3. **Verify Frequency**: Confirm your board has an **868 MHz** transceiver (not 433 MHz or 915 MHz).
4. **Range & Interference**: Position Omni-IO within 5 to 10 meters of the motor during initial pairing.

### Motor jogs twice or unpairs
* If you click **Unpair** or send an incorrect sequence number, the motor may clear its memory. Re-enter pairing mode with the physical remote and pair again.

---

## 3. OLED Display Issues

### The OLED display is black or does not turn on
1. **Screensaver / Screen-off**: Press any physical remote or send a command via the Web UI to wake the screen.
2. **Heltec LoRa32 Boards**: The Heltec board requires powering the `Vext` pin (GPIO 21 pulled LOW) to supply power to the OLED. Ensure you compiled with `-DHELTEC`.
3. **I2C Address**: Most SSD1306 0.96" screens use I2C address `0x3C`. If you have a custom screen using `0x3D`, verify board configurations.

---

## 4. Web Interface & LittleFS Issues

### Web UI shows a blank page or 404
* **Cause**: The LittleFS filesystem containing the HTML/JS/CSS assets has not been uploaded.
* **Solution**:
  * Upload `*_filesystem.bin` via the Web UI OTA update, or
  * Using PlatformIO: run `pio run --target uploadfs`.

---

## 5. Home Assistant MQTT Issues

### Devices are not appearing in Home Assistant
1. Verify Omni-IO shows **MQTT Connected** on the OLED screen and in the Web UI status bar.
2. Ensure the **Discovery Prefix** in Omni-IO matches the prefix configured in Home Assistant (default: `homeassistant`).
3. Check the Home Assistant MQTT integration logs under **Settings → Devices & Services → MQTT**.
