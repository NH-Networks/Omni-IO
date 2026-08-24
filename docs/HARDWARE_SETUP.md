# Omni-IO Hardware Setup & Pinout Guide

This guide covers the supported ESP32 + Semtech LoRa/FSK transceiver boards, pinouts, antenna specifications, and wiring considerations for **Omni-IO**.

---

## Supported Hardware Boards

Omni-IO is optimized for 868.95 MHz io-homecontrol® communication using ESP32 microcontrollers paired with Semtech transceivers (SX1276 / SX1262).

| Board | Microcontroller | Radio Transceiver | Display | Storage |
| :--- | :--- | :--- | :--- | :--- |
| **LilyGo T-Beam v1.2** | ESP32-D0WDQ6 (Dual Core 240MHz) | SX1276 (868 MHz) | External I2C OLED (Optional) | 4MB Flash |
| **LilyGo LoRa32 v2.1 (T3 v1.6.1)** | ESP32-PICO-D4 (Dual Core 240MHz) | SX1276 (868 MHz) | Integrated 0.96" SSD1306 OLED | 4MB Flash |
| **Heltec LoRa32 v2** | ESP32 (Dual Core 240MHz) | SX1276 (868 MHz) | Integrated 0.96" SSD1306 OLED | 8MB Flash |
| **LilyGo T3-S3** | ESP32-S3 (Dual Core 240MHz) | SX1262 / SX1276 (868 MHz) | Integrated 0.96" OLED | 4MB Flash / 2MB PSRAM |

---

## 1. Board Pinout Mappings

The firmware automatically configures the appropriate SPI and GPIO pins based on the selected build environment in PlatformIO:

### LilyGo T-Beam v1.2 (`-DLILYGO_TBEAM_V12`)
* **Radio SPI**:
  * `SCK`: GPIO 5
  * `MISO`: GPIO 19
  * `MOSI`: GPIO 27
  * `NSS / CS`: GPIO 18
  * `RST`: GPIO 23
  * `DIO0` (IRQ): GPIO 26
* **I2C OLED (Optional)**:
  * `SDA`: GPIO 21
  * `SCL`: GPIO 22
* **Power & Battery**: AXP192 / AXP2101 power management IC with integrated 18650 battery holder.

---

### LilyGo LoRa32 v2.1 (`-DLILYGO`)
* **Radio SPI**:
  * `SCK`: GPIO 5
  * `MISO`: GPIO 19
  * `MOSI`: GPIO 27
  * `NSS / CS`: GPIO 18
  * `RST`: GPIO 23
  * `DIO0` (IRQ): GPIO 26
* **Integrated SSD1306 OLED (I2C)**:
  * `SDA`: GPIO 21
  * `SCL`: GPIO 22
  * `RST`: GPIO 16

---

### Heltec WiFi LoRa 32 v2 (`-DHELTEC`)
* **Radio SPI**:
  * `SCK`: GPIO 5
  * `MISO`: GPIO 19
  * `MOSI`: GPIO 27
  * `NSS / CS`: GPIO 18
  * `RST`: GPIO 14
  * `DIO0` (IRQ): GPIO 26
* **Integrated SSD1306 OLED (I2C)**:
  * `SDA`: GPIO 4
  * `SCL`: GPIO 15
  * `RST`: GPIO 16
  * `Vext Power Control`: GPIO 21 (Pulled LOW to power display and external sensors)

---

### LilyGo T3-S3 (`-DLILYGO_T3S3`)
* **Radio SPI**:
  * `SCK`: GPIO 5
  * `MISO`: GPIO 3
  * `MOSI`: GPIO 6
  * `NSS / CS`: GPIO 7
  * `RST`: GPIO 8
  * `DIO0 / BUSY`: GPIO 33
* **Integrated OLED (I2C)**:
  * `SDA`: GPIO 18
  * `SCL`: GPIO 17

---

## 2. RF & Antenna Recommendations

> [!WARNING]
> **Never operate the ESP32 radio module without an antenna connected!** Transmitting RF frames without a load can permanently damage the power amplifier stage of the Semtech transceiver.

* **Frequency**: io-homecontrol operates on **868.95 MHz** (Channel 1W) and frequency-hopping channels around 868-870 MHz.
* **Antenna Type**:
  * A dedicated tuned 868 MHz whip or dipole antenna with SMA connector is strongly recommended for best range.
  * For DIY wire antennas, use a quarter-wave length of copper wire:
    $$\text{Length} = \frac{c}{4 \times f} \approx \frac{300}{4 \times 868.95} \approx 8.6\text{ cm (3.38 inches)}$$
* **Placement**: Position the gateway away from large metal objects, thick concrete walls, or high-power 2.4GHz WiFi access points to maximize reception sensitivity.

---

## 3. Power Supply

* Provide a clean and stable 5V / 1A power supply via USB-C or Micro-USB.
* During RF transmission bursts, the radio transceiver can draw peaks up to 120-150mA. A capacitor (100µF–470µF) across 3.3V and GND is beneficial on custom breadboard setups.
