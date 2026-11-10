/*
 * SPDX-FileCopyrightText: 2026 CloudAXS
 * SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
 */

#include <Arduino.h>
#include <board-config.h>

#if defined(RADIO_SX126X)

#include <SX126xHelpers.h>
#include <iohcCryptoHelpers.h>
#include <string.h>
#include <SPI.h>
#include <Wire.h>
#include <map>
#include <esp_task_wdt.h>

namespace Radio {
    static SPISettings SpiSettings(8000000, MSBFIRST, SPI_MODE0);

    static uint32_t currentFrequency = 868950000;
    static uint32_t currentBitrate = 38400;
    static uint32_t currentDeviation = 19200;
    static uint16_t currentPreambleLen = 32; // in bits

    // On-air io-homecontrol sync word with start/stop bits applied (decoded: 0xFF 0x33).
    static constexpr uint8_t SYNC_ENC_1 = 0xFF;
    static constexpr uint8_t SYNC_ENC_2 = 0xB3;
    // Longest frame: 34 decoded bytes (31 + control + 2 CRC) * 10 bits + 4 bit offset = 44 bytes.
    static constexpr uint8_t RX_RAW_LEN = 44;
    static uint8_t currentPayloadLen = RX_RAW_LEN;

    static void IRAM_ATTR waitBusy() {
#if defined(RADIO_BUSY_PIN) && RADIO_BUSY_PIN >= 0
        uint32_t start = micros();
        while (digitalRead(RADIO_BUSY_PIN) == HIGH) {
            if (micros() - start > 50000) { // 50ms timeout
                break;
            }
            delayMicroseconds(10);
        }
#endif
    }

    static void IRAM_ATTR spiBegin() {
        SPI.beginTransaction(SpiSettings);
#if defined(RADIO_CS_PIN) && RADIO_CS_PIN >= 0
        digitalWrite(RADIO_CS_PIN, LOW);
#elif defined(RADIO_NSS) && RADIO_NSS >= 0
        digitalWrite(RADIO_NSS, LOW);
#endif
    }

    static void IRAM_ATTR spiEnd() {
#if defined(RADIO_CS_PIN) && RADIO_CS_PIN >= 0
        digitalWrite(RADIO_CS_PIN, HIGH);
#elif defined(RADIO_NSS) && RADIO_NSS >= 0
        digitalWrite(RADIO_NSS, HIGH);
#endif
        SPI.endTransaction();
    }

    static void executeOpcode(uint8_t opcode, const uint8_t *params = nullptr, uint8_t paramLen = 0) {
        waitBusy();
        spiBegin();
        SPI.transfer(opcode);
        for (uint8_t i = 0; i < paramLen; ++i) {
            SPI.transfer(params[i]);
        }
        spiEnd();
        waitBusy();
    }

    static void readOpcode(uint8_t opcode, uint8_t *data, uint8_t dataLen, bool hasNop = true) {
        waitBusy();
        spiBegin();
        SPI.transfer(opcode);
        if (hasNop) {
            SPI.transfer(0x00); // status / NOP byte
        }
        for (uint8_t i = 0; i < dataLen; ++i) {
            data[i] = SPI.transfer(0x00);
        }
        spiEnd();
        waitBusy();
    }

    static uint8_t readRegisterRaw(uint16_t address) {
        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_READ_REGISTER);
        SPI.transfer((address >> 8) & 0xFF);
        SPI.transfer(address & 0xFF);
        SPI.transfer(0x00); // NOP
        uint8_t val = SPI.transfer(0x00);
        spiEnd();
        waitBusy();
        return val;
    }

    static void writeRegisterRaw(uint16_t address, uint8_t value) {
        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_WRITE_REGISTER);
        SPI.transfer((address >> 8) & 0xFF);
        SPI.transfer(address & 0xFF);
        SPI.transfer(value);
        spiEnd();
        waitBusy();
    }

    static void writeRegistersRaw(uint16_t address, const uint8_t *data, uint8_t len) {
        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_WRITE_REGISTER);
        SPI.transfer((address >> 8) & 0xFF);
        SPI.transfer(address & 0xFF);
        for (uint8_t i = 0; i < len; ++i) {
            SPI.transfer(data[i]);
        }
        spiEnd();
        waitBusy();
    }

#if defined(I2C_EXPANDER_ADDR) && defined(I2C_EXPANDER_SDA) && defined(I2C_EXPANDER_SCL)
    // PI4IOE5V6408 register map
    static constexpr uint8_t PI4IO_REG_CHIP_RESET = 0x01;
    static constexpr uint8_t PI4IO_REG_IO_DIR     = 0x03;
    static constexpr uint8_t PI4IO_REG_OUT_SET    = 0x05;
    static constexpr uint8_t PI4IO_REG_OUT_H_IM   = 0x07;

    static bool expanderWrite(uint8_t reg, uint8_t value) {
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(reg);
        Wire.write(value);
        return Wire.endTransmission() == 0;
    }

    static int expanderRead(uint8_t reg) {
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(reg);
        if (Wire.endTransmission() != 0 || Wire.requestFrom((int)I2C_EXPANDER_ADDR, 1) != 1) {
            return -1;
        }
        return Wire.read();
    }
#endif

    static void initI2cExpander() {
#if defined(I2C_EXPANDER_ADDR) && defined(I2C_EXPANDER_SDA) && defined(I2C_EXPANDER_SCL)
        // P7 = SX1262 NRST, P6 = RF switch (SX_ANT_SW), P5 = LNA enable
        Wire.begin(I2C_EXPANDER_SDA, I2C_EXPANDER_SCL);

        bool ok = expanderWrite(PI4IO_REG_CHIP_RESET, 0xFF);
        delay(10);
        ok &= expanderWrite(PI4IO_REG_IO_DIR, 0xE0);   // P5..P7 outputs
        ok &= expanderWrite(PI4IO_REG_OUT_H_IM, 0x1C); // P5..P7 actively driven
        // Hold the radio in reset while the RF switch and LNA are enabled
        ok &= expanderWrite(PI4IO_REG_OUT_SET, 0x60);
        delay(20);
        ok &= expanderWrite(PI4IO_REG_OUT_SET, 0xE0);  // release NRST
        delay(20);

        const int dir = expanderRead(PI4IO_REG_IO_DIR);
        const int out = expanderRead(PI4IO_REG_OUT_SET);
        ets_printf("PI4IOE5V6408 @0x%02x: %s dir=0x%02x out=0x%02x (expected dir=0xE0 out=0xE0)\n",
                      I2C_EXPANDER_ADDR, ok ? "ack" : "NACK", dir & 0xFF, out & 0xFF);
        if (!ok || dir != 0xE0 || out != 0xE0) {
            ets_printf("WARNING: PI4IOE5V6408 init failed, SX1262 is likely held in reset or has no antenna path\n");
        }
#endif
    }

    static uint8_t reverseByte(uint8_t b) {
        b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
        b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
        b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
        return b;
    }

    // io-homecontrol sends every byte as start bit (0), 8 data bits LSB first, stop bit (1).
    // The SX1276 strips this in hardware (IoHome mode); the SX1262 hands us the raw bit stream.
    // The first start bit sits at bit 3 after the 16 bit sync word.
    // Returns decoded length including the 2 CRC bytes, or -1.
    static int16_t decodeIoHomeFrame(const uint8_t *raw, uint8_t rawLen, uint8_t *out, uint8_t outMax) {
        uint16_t decodedLen = 0xFFFF;
        for (uint16_t idx = 0; idx < decodedLen; ++idx) {
            const uint16_t bitPos = 4 + 10 * idx;
            const uint16_t bytePos = bitPos >> 3;
            const uint8_t shift = bitPos & 7;
            if (bytePos + 1 >= rawLen || idx >= outMax) {
                return -1;
            }
            const uint8_t value = (uint8_t)((raw[bytePos] << shift) | (raw[bytePos + 1] >> (8 - shift)));
            out[idx] = reverseByte(value);
            if (idx == 0) {
                decodedLen = (out[0] & 0x1F) + 1 + 2; // control byte + payload + CRC
            }
        }
        return (int16_t)decodedLen;
    }

    // Inverse of decodeIoHomeFrame: appends the CRC-16 (low byte first), frames every byte with
    // start/stop bits and prepends the 3 bits that follow the sync word. Returns the byte count.
    static uint8_t encodeIoHomeFrame(const uint8_t *in, uint8_t len, uint8_t *out, uint8_t outMax) {
        const uint16_t crc = iohcCrypto::radioPacketComputeCrc(const_cast<uint8_t *>(in), len);
        const uint16_t totalBits = 3 + 10 * (len + 2);
        const uint8_t outLen = (totalBits + 7) / 8 + 1; // one trailing idle byte flushes the last stop bit
        if (outLen > outMax) {
            return 0;
        }
        memset(out, 0xFF, outLen); // idle line is high
        uint16_t bitPos = 0;
        auto putBit = [&](bool bit) {
            if (!bit) {
                out[bitPos >> 3] &= ~(0x80 >> (bitPos & 7));
            }
            ++bitPos;
        };
        putBit(0); putBit(0); putBit(1);
        for (uint8_t i = 0; i < len + 2u; ++i) {
            const uint8_t value = i < len ? in[i] : (i == len ? (crc & 0xFF) : (crc >> 8));
            putBit(0);
            for (uint8_t b = 0; b < 8; ++b) {
                putBit((value >> b) & 1);
            }
            putBit(1);
        }
        return outLen;
    }

    static void setRfFrequency(uint32_t frequency) {
        currentFrequency = frequency;
        uint32_t freqRaw = (uint32_t)((double)frequency / (double)FXOSC * (double)(1 << 25));
        uint8_t params[4] = {
            (uint8_t)((freqRaw >> 24) & 0xFF),
            (uint8_t)((freqRaw >> 16) & 0xFF),
            (uint8_t)((freqRaw >> 8) & 0xFF),
            (uint8_t)(freqRaw & 0xFF)
        };
        executeOpcode(SX126X_CMD_SET_RF_FREQUENCY, params, 4);
    }

    static void setModulationParamsInternal(uint32_t bitrate, uint32_t deviation, uint8_t rxBw = SX126X_GFSK_RX_BW_234300) {
        currentBitrate = bitrate;
        currentDeviation = deviation;

        uint32_t brRaw = (uint32_t)(32.0 * (double)FXOSC / (double)bitrate);
        uint32_t fdevRaw = (uint32_t)((double)deviation / (double)FXOSC * (double)(1 << 25));

        uint8_t params[8] = {
            (uint8_t)((brRaw >> 16) & 0xFF),
            (uint8_t)((brRaw >> 8) & 0xFF),
            (uint8_t)(brRaw & 0xFF),
            SX126X_GFSK_PULSE_SHAPE_OFF, // No shaping for standard io-homecontrol FSK
            rxBw,                        // Bandwidth
            (uint8_t)((fdevRaw >> 16) & 0xFF),
            (uint8_t)((fdevRaw >> 8) & 0xFF),
            (uint8_t)(fdevRaw & 0xFF)
        };
        executeOpcode(SX126X_CMD_SET_MODULATION_PARAMS, params, 8);
    }

    // Fixed length packets without hardware CRC: the io-homecontrol length byte, start/stop bits
    // and CRC-16 are handled in software (see decodeIoHomeFrame).
    static void setPacketParamsInternal(uint16_t preambleLenBits, uint8_t payloadLen) {
        currentPreambleLen = preambleLenBits;
        currentPayloadLen = payloadLen;
        uint8_t params[9] = {
            (uint8_t)((preambleLenBits >> 8) & 0xFF),
            (uint8_t)(preambleLenBits & 0xFF),
            SX126X_GFSK_PREAMBLE_DETECT_16,  // 16-bit preamble detector
            16,                              // 16-bit (2 bytes) sync word
            SX126X_GFSK_ADDRESS_FILT_OFF,
            SX126X_GFSK_PACKET_KNOWN_LENGTH,
            payloadLen,
            SX126X_GFSK_CRC_OFF,
            SX126X_GFSK_WHITENING_OFF
        };
        executeOpcode(SX126X_CMD_SET_PACKET_PARAMS, params, 9);
    }

    void initHardware() {
        ets_printf("SX1262 Hardware Init starting...\n");

        // Initialize I2C port expander if present (e.g. M5Stack C6L)
        initI2cExpander();

#if defined(RADIO_BUSY_PIN) && RADIO_BUSY_PIN >= 0
        pinMode(RADIO_BUSY_PIN, INPUT);
#endif
#if defined(RADIO_DIO1_PIN) && RADIO_DIO1_PIN >= 0
        pinMode(RADIO_DIO1_PIN, INPUT);
#endif
#if defined(RADIO_CS_PIN) && RADIO_CS_PIN >= 0
        pinMode(RADIO_CS_PIN, OUTPUT);
        digitalWrite(RADIO_CS_PIN, HIGH);
#elif defined(RADIO_NSS) && RADIO_NSS >= 0
        pinMode(RADIO_NSS, OUTPUT);
        digitalWrite(RADIO_NSS, HIGH);
#endif

#if defined(RADIO_SCLK_PIN) && defined(RADIO_MISO_PIN) && defined(RADIO_MOSI_PIN) && defined(RADIO_CS_PIN)
        // SPI.begin() is a no-op when the bus is already running (e.g. started by the OLED
        // driver with default pins), so restart it to be sure the radio pins are routed.
        SPI.end();
        SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
#elif defined(ESP32)
        SPI.begin();
#endif

        // Put radio into Standby RC
        setStandby();

#if defined(RADIO_TCXO_VOLTAGE)
        // The XTAL is a TCXO powered from DIO3; without this the SX1262 has no clock.
        // Delay = 5 ms in 15.625 us steps (320 = 0x000140).
        uint8_t tcxo[4] = {RADIO_TCXO_VOLTAGE, 0x00, 0x01, 0x40};
        executeOpcode(SX126X_CMD_SET_DIO3_AS_TCXO_CTRL, tcxo, 4);
        delay(10);
        uint8_t clearErr[2] = {0x00, 0x00};
        executeOpcode(SX126X_CMD_CLEAR_DEVICE_ERRORS, clearErr, 2);
#endif

        // Configure DC-DC regulator mode for higher efficiency
        uint8_t regMode = SX126X_REGULATOR_DC_DC;
        executeOpcode(SX126X_CMD_SET_REGULATOR_MODE, &regMode, 1);

        // Configure DIO2 as RF Switch control
        uint8_t dio2Mode = 0x01; // Enable DIO2 RF switch
        executeOpcode(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &dio2Mode, 1);

        // Set Buffer Base Address (TX base=0x00, RX base=0x00)
        uint8_t bufferBase[2] = {0x00, 0x00};
        executeOpcode(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, bufferBase, 2);

        ets_printf("SX1262 Hardware Init completed.\n");
    }

    void calibrate() {
        // Standby RC mode
        setStandby();

        // Calibrate image for 863MHz - 870MHz band (0xD7, 0xDB)
        uint8_t imgCal[2] = {0xD7, 0xDB};
        executeOpcode(SX126X_CMD_CALIBRATE_IMAGE, imgCal, 2);

        // Full RC and ADC calibration (0x7F)
        uint8_t calAll = 0x7F;
        executeOpcode(SX126X_CMD_CALIBRATE, &calAll, 1);
        delay(5);

        uint8_t errors[2] = {0, 0};
        readOpcode(SX126X_CMD_GET_DEVICE_ERRORS, errors, 2, true);
        uint8_t status = 0;
        readOpcode(SX126X_CMD_GET_STATUS, &status, 1, false);
        ets_printf("SX1262 status=0x%02x deviceErrors=0x%02x%02x (0 = clock/PLL/PA OK)\n",
                      status, errors[0], errors[1]);
    }

    void initRegisters(uint8_t maxPayloadLength) {
        setStandby();

        // 1. Set Packet Type to GFSK
        uint8_t pktType = SX126X_PACKET_TYPE_GFSK;
        executeOpcode(SX126X_CMD_SET_PACKET_TYPE, &pktType, 1);

        // 2. Set Modulation Parameters (Bitrate 38400, Fdev 19200, BW 234.3 kHz)
        setModulationParamsInternal(38400, 19200, SX126X_GFSK_RX_BW_234300);

        // 3. Set Packet Parameters (32-bit preamble, 16-bit sync word, fixed length, no hardware CRC)
        setPacketParamsInternal(32, RX_RAW_LEN);

        // 4. Set Sync Word registers (0x06C0, 0x06C1)
        uint8_t syncBytes[2] = {SYNC_ENC_1, SYNC_ENC_2};
        writeRegistersRaw(SX126X_REG_SYNC_WORD_0, syncBytes, 2);

        // 5. RX boosted gain for better sensitivity
        writeRegisterRaw(SX126X_REG_RX_GAIN, SX126X_RX_GAIN_BOOSTED);

        // 6. Set PA Config for SX1262 (+22dBm capable PA)
        uint8_t paConfig[4] = {0x04, 0x07, 0x00, 0x01}; // paDutyCycle=0x04, hpMax=0x07, deviceSel=0x00, paLut=0x01
        executeOpcode(SX126X_CMD_SET_PA_CONFIG, paConfig, 4);

        // 7. Set TX Parameters: +22dBm (0x16), 10us ramp
        uint8_t txParams[2] = {0x16, SX126X_RAMP_10_US};
        executeOpcode(SX126X_CMD_SET_TX_PARAMS, txParams, 2);

        // 8. Configure DIO1 IRQ parameters. The ISR treats every DIO1 rising edge as
        // "packet done", so only RX_DONE and TX_DONE may drive the pin.
        uint16_t irqMask = SX126X_IRQ_ALL;
        uint16_t dio1Mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE;
        setDioIrqParams(irqMask, dio1Mask, 0, 0);

        // 9. Set default carrier frequency
        setRfFrequency(currentFrequency);

        clearFlags();
        clearBuffer();
    }

    void setStandby() {
        uint8_t mode = SX126X_STANDBY_RC;
        executeOpcode(SX126X_CMD_SET_STANDBY, &mode, 1);
    }

    void setTx() {
        uint8_t timeout[3] = {0x00, 0x00, 0x00}; // No timeout
        executeOpcode(SX126X_CMD_SET_TX, timeout, 3);
    }

    void setRx() {
        // A previous TX leaves its own payload length in the packet params
        if (currentPayloadLen != RX_RAW_LEN) {
            setPacketParamsInternal(currentPreambleLen, RX_RAW_LEN);
        }
        uint8_t timeout[3] = {0xFF, 0xFF, 0xFF}; // Continuous RX mode
        executeOpcode(SX126X_CMD_SET_RX, timeout, 3);
    }

    void setPreambleLength(uint16_t preambleLen) {
        // Callers pass bytes, the SX126x wants bits
        uint16_t preambleBits = preambleLen * 8;
        if (preambleBits < 16) {
            preambleBits = 16;
        }
        setPacketParamsInternal(preambleBits, currentPayloadLen);
    }

    void clearBuffer() {
        uint8_t base[2] = {0x00, 0x00};
        executeOpcode(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, base, 2);
    }

    void clearFlags() {
        clearIrqStatus(SX126X_IRQ_ALL);
    }

    uint16_t getIrqStatus() {
        uint8_t status[2] = {0, 0};
        readOpcode(SX126X_CMD_GET_IRQ_STATUS, status, 2, true);
        return ((uint16_t)status[0] << 8) | status[1];
    }

    void clearIrqStatus(uint16_t mask) {
        uint8_t params[2] = {(uint8_t)((mask >> 8) & 0xFF), (uint8_t)(mask & 0xFF)};
        executeOpcode(SX126X_CMD_CLEAR_IRQ_STATUS, params, 2);
    }

    void setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask, uint16_t dio3Mask) {
        uint8_t params[8] = {
            (uint8_t)((irqMask >> 8) & 0xFF),  (uint8_t)(irqMask & 0xFF),
            (uint8_t)((dio1Mask >> 8) & 0xFF), (uint8_t)(dio1Mask & 0xFF),
            (uint8_t)((dio2Mask >> 8) & 0xFF), (uint8_t)(dio2Mask & 0xFF),
            (uint8_t)((dio3Mask >> 8) & 0xFF), (uint8_t)(dio3Mask & 0xFF)
        };
        executeOpcode(SX126X_CMD_SET_DIO_IRQ_PARAMS, params, 8);
    }

    bool preambleDetected() {
        uint16_t irq = getIrqStatus();
        return (irq & SX126X_IRQ_PREAMBLE_DETECTED) != 0;
    }

    bool syncedAddress() {
        uint16_t irq = getIrqStatus();
        return (irq & SX126X_IRQ_SYNCWORD_VALID) != 0;
    }

    bool dataAvail() {
        uint16_t irq = getIrqStatus();
        return (irq & SX126X_IRQ_RX_DONE) != 0;
    }

    bool crcOk() {
        uint16_t irq = getIrqStatus();
        return (irq & SX126X_IRQ_CRC_ERR) == 0;
    }

    uint8_t getRssiInst() {
        uint8_t rssiRaw = 0;
        readOpcode(SX126X_CMD_GET_RSSI_INST, &rssiRaw, 1, true);
        return rssiRaw;
    }

    void readRxBuffer(uint8_t *buffer, uint8_t &size) {
        size = 0;
        uint8_t rxStatus[2] = {0, 0};
        readOpcode(SX126X_CMD_GET_RX_BUFFER_STATUS, rxStatus, 2, true);
        uint8_t payloadLen = rxStatus[0];
        const uint8_t rxStartOffset = rxStatus[1];

        if (payloadLen == 0) {
            return;
        }
        if (payloadLen > RX_RAW_LEN) {
            payloadLen = RX_RAW_LEN;
        }

        uint8_t raw[RX_RAW_LEN];
        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_READ_BUFFER);
        SPI.transfer(rxStartOffset);
        SPI.transfer(0x00); // NOP
        for (uint8_t i = 0; i < payloadLen; ++i) {
            raw[i] = SPI.transfer(0x00);
        }
        spiEnd();
        waitBusy();

        uint8_t decoded[RX_RAW_LEN];
        const int16_t decodedLen = decodeIoHomeFrame(raw, payloadLen, decoded, sizeof(decoded));
        if (decodedLen < 3 || iohcCrypto::radioPacketComputeCrc(decoded, (uint8_t)decodedLen) != 0) {
            static uint32_t lastLogMs = 0;
            if (millis() - lastLogMs > 2000) {
                lastLogMs = millis();
                ets_printf("SX1262 RX frame rejected (decodedLen=%d) raw:", decodedLen);
                for (uint8_t i = 0; i < 16 && i < payloadLen; ++i) {
                    ets_printf(" %02x", raw[i]);
                }
                ets_printf("\n");
            }
            return; // not a valid io-homecontrol frame; reported as empty
        }

        // Strip the CRC, like the SX1276 does in hardware
        size = (uint8_t)(decodedLen - 2);
        memcpy(buffer, decoded, size);
    }

    void writeTxBuffer(const uint8_t *buffer, uint8_t size) {
        uint8_t encoded[RX_RAW_LEN + 2];
        const uint8_t encodedLen = encodeIoHomeFrame(buffer, size, encoded, sizeof(encoded));
        if (encodedLen == 0) {
            return;
        }

        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_WRITE_BUFFER);
        SPI.transfer(0x00); // Start offset 0x00
        for (uint8_t i = 0; i < encodedLen; ++i) {
            SPI.transfer(encoded[i]);
        }
        spiEnd();
        waitBusy();

        setPacketParamsInternal(currentPreambleLen, encodedLen);
    }

    uint8_t readByte(uint8_t regAddr) {
        // Map common virtual query addresses
        if (regAddr == 0x01) { // OPMODE query
            uint8_t status = 0;
            readOpcode(SX126X_CMD_GET_STATUS, &status, 1, false);
            return status;
        }
        if (regAddr == 0x1B) { // RSSI query
            return getRssiInst();
        }
        return readRegisterRaw((uint16_t)regAddr);
    }

    void readBytes(uint8_t regAddr, uint8_t *out, uint8_t len) {
        if (regAddr == 0x12) { // IRQ Flags
            uint16_t irq = getIrqStatus();
            if (len >= 2) {
                out[0] = (uint8_t)((irq >> 8) & 0xFF);
                out[1] = (uint8_t)(irq & 0xFF);
            } else if (len == 1) {
                out[0] = (uint8_t)(irq & 0xFF);
            }
            return;
        }
        for (uint8_t i = 0; i < len; ++i) {
            out[i] = readRegisterRaw((uint16_t)(regAddr + i));
        }
    }

    bool writeByte(uint8_t regAddr, uint8_t data, bool check) {
        writeRegisterRaw((uint16_t)regAddr, data);
        return true;
    }

    bool writeBytes(uint8_t regAddr, uint8_t *in, uint8_t len, bool check) {
        if (regAddr == 0x00) { // FIFO write
            writeTxBuffer(in, len);
            return true;
        }
        writeRegistersRaw((uint16_t)regAddr, in, len);
        return true;
    }

    bool inStdbyOrSleep() {
        uint8_t status = 0;
        readOpcode(SX126X_CMD_GET_STATUS, &status, 1, false);
        uint8_t chipMode = (status >> 4) & 0x07;
        return (chipMode == 0x00 || chipMode == 0x02 || chipMode == 0x03);
    }

    bool setParams() {
        return true;
    }

    bool setCarrier(Carrier param, uint32_t value) {
        switch (param) {
            case Carrier::Frequency:
                setRfFrequency(value);
                break;
            case Carrier::Deviation:
                setModulationParamsInternal(currentBitrate, value);
                break;
            case Carrier::Bitrate:
                setModulationParamsInternal(value, currentDeviation);
                break;
            case Carrier::Bandwidth:
                setModulationParamsInternal(currentBitrate, currentDeviation, SX126X_GFSK_RX_BW_234300);
                break;
            case Carrier::Modulation:
                // FSK mode is standard
                break;
        }
        return true;
    }

    regBandWidth bwRegs(uint8_t bandwidth) {
        return {0x00, 0x01};
    }

    void dump() {
        uint8_t status = 0, errors[2] = {0, 0}, pktType = 0xFF, rssi = 0;
        readOpcode(SX126X_CMD_GET_STATUS, &status, 1, false);
        readOpcode(SX126X_CMD_GET_DEVICE_ERRORS, errors, 2, true);
        readOpcode(SX126X_CMD_GET_PACKET_TYPE, &pktType, 1, true);
        rssi = getRssiInst();
        const uint8_t sync0 = readRegisterRaw(SX126X_REG_SYNC_WORD_0);
        const uint8_t sync1 = readRegisterRaw(SX126X_REG_SYNC_WORD_1);
        const uint8_t gain = readRegisterRaw(SX126X_REG_RX_GAIN);
        ets_printf("SX1262 dump: status=0x%02x (mode=%d cmd=%d; 5=RX 2=STDBY_RC 3=STDBY_XOSC) errors=0x%02x%02x pktType=%d "
                   "sync=%02x%02x rxGain=0x%02x irq=0x%04x rssi=-%d.%ddBm busy=%d dio1=%d freq=%d\n",
                   status, (status >> 4) & 7, (status >> 1) & 7, errors[0], errors[1], pktType,
                   sync0, sync1, gain, getIrqStatus(), rssi / 2, (rssi & 1) * 5,
#if defined(RADIO_BUSY_PIN) && RADIO_BUSY_PIN >= 0
                   digitalRead(RADIO_BUSY_PIN),
#else
                   -1,
#endif
                   digitalRead(RADIO_DIO1_PIN), (int)currentFrequency);
    }
    void dumpReal() {}
    uint16_t readWord(uint8_t regAddr) { return 0; }
    void writeWord(uint8_t regAddr, uint16_t value) {}
}

#endif // RADIO_SX126X
