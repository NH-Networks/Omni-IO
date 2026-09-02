/*
 * SPDX-FileCopyrightText: 2026 CloudAXS
 * SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
 */

#include <Arduino.h>
#include <board-config.h>

#if defined(RADIO_SX126X)

#include <SX126xHelpers.h>
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

    static void initI2cExpander() {
#if defined(I2C_EXPANDER_ADDR) && defined(I2C_EXPANDER_SDA) && defined(I2C_EXPANDER_SCL)
        Wire.begin(I2C_EXPANDER_SDA, I2C_EXPANDER_SCL);

        // 1. Configure Port Direction: P5 (LNA_EN), P6 (ANT_SW), P7 (NRST) as OUTPUTS (0xE0)
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(0x01); // Configuration / Direction Register
        Wire.write(0xE0);
        Wire.endTransmission();

        // 2. Disable High-Impedance on output pins to enable active push-pull drive
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(0x05); // Output High-Impedance Register (0 = driven, 1 = Hi-Z)
        Wire.write(0x1F); // P7, P6, P5 active (0), P0..P4 high-Z inputs (1)
        Wire.endTransmission();

        // 3. Reset pulse: Drive SX_NRST (P7) LOW for 20ms while ANT_SW (P6) & LNA_EN (P5) are HIGH
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(0x03); // Output Port Register
        Wire.write(0x60); // P7=0 (reset), P6=1, P5=1
        Wire.endTransmission();
        delay(20);

        // 4. Release Reset: Drive SX_NRST (P7) HIGH
        Wire.beginTransmission(I2C_EXPANDER_ADDR);
        Wire.write(0x03); // Output Port Register
        Wire.write(0xE0); // P7=1 (run), P6=1, P5=1
        Wire.endTransmission();
        delay(20);
        Serial.println("PI4IOE5V6408 I2C expander initialized: SX_NRST released, ANT_SW enabled, LNA_EN active.");
#endif
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

    static void setPacketParamsInternal(uint16_t preambleLenBits, uint8_t maxPayloadLen = 0xFF) {
        currentPreambleLen = preambleLenBits;
        uint8_t params[9] = {
            (uint8_t)((preambleLenBits >> 8) & 0xFF),
            (uint8_t)(preambleLenBits & 0xFF),
            SX126X_GFSK_PREAMBLE_DETECT_16,  // 16-bit preamble detector
            16,                              // 16-bit (2 bytes) sync word
            SX126X_GFSK_ADDRESS_FILT_OFF,
            SX126X_GFSK_PACKET_VARIABLE,
            maxPayloadLen,
            SX126X_GFSK_CRC_2_BYTE,          // 2-byte CCITT CRC
            SX126X_GFSK_WHITENING_OFF
        };
        executeOpcode(SX126X_CMD_SET_PACKET_PARAMS, params, 9);
    }

    void initHardware() {
        Serial.println("SX1262 Hardware Init starting...");

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
        SPI.begin(RADIO_SCLK_PIN, RADIO_MISO_PIN, RADIO_MOSI_PIN, RADIO_CS_PIN);
#elif defined(ESP32)
        SPI.begin();
#endif

        // Put radio into Standby RC
        setStandby();

        // Configure DC-DC regulator mode for higher efficiency
        uint8_t regMode = SX126X_REGULATOR_DC_DC;
        executeOpcode(SX126X_CMD_SET_REGULATOR_MODE, &regMode, 1);

        // Configure DIO2 as RF Switch control
        uint8_t dio2Mode = 0x01; // Enable DIO2 RF switch
        executeOpcode(SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL, &dio2Mode, 1);

        // Set Buffer Base Address (TX base=0x00, RX base=0x00)
        uint8_t bufferBase[2] = {0x00, 0x00};
        executeOpcode(SX126X_CMD_SET_BUFFER_BASE_ADDRESS, bufferBase, 2);

        Serial.println("SX1262 Hardware Init completed.");
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
    }

    void initRegisters(uint8_t maxPayloadLength) {
        setStandby();

        // 1. Set Packet Type to GFSK
        uint8_t pktType = SX126X_PACKET_TYPE_GFSK;
        executeOpcode(SX126X_CMD_SET_PACKET_TYPE, &pktType, 1);

        // 2. Set Modulation Parameters (Bitrate 38400, Fdev 19200, BW 234.3 kHz)
        setModulationParamsInternal(38400, 19200, SX126X_GFSK_RX_BW_234300);

        // 3. Set Packet Parameters (32-bit preamble, 16-bit sync word, variable length, 2-byte CRC)
        setPacketParamsInternal(32, maxPayloadLength);

        // 4. Set Sync Word registers (0x06C0, 0x06C1)
        uint8_t syncBytes[2] = {SYNC_BYTE_1, SYNC_BYTE_2};
        writeRegistersRaw(SX126X_REG_SYNC_WORD_0, syncBytes, 2);

        // 5. Configure CCITT 16-bit CRC polynomial (0x1021) and initial value (0xFFFF)
        writeRegisterRaw(SX126X_REG_CRC_POLYNOMIAL_MSB, 0x10);
        writeRegisterRaw(SX126X_REG_CRC_POLYNOMIAL_LSB, 0x21);
        writeRegisterRaw(SX126X_REG_CRC_INITIAL_MSB, 0xFF);
        writeRegisterRaw(SX126X_REG_CRC_INITIAL_LSB, 0xFF);

        // 6. Set PA Config for SX1262 (+22dBm capable PA)
        uint8_t paConfig[4] = {0x04, 0x07, 0x00, 0x01}; // paDutyCycle=0x04, hpMax=0x07, deviceSel=0x00, paLut=0x01
        executeOpcode(SX126X_CMD_SET_PA_CONFIG, paConfig, 4);

        // 7. Set TX Parameters: +22dBm (0x16), 10us ramp
        uint8_t txParams[2] = {0x16, SX126X_RAMP_10_US};
        executeOpcode(SX126X_CMD_SET_TX_PARAMS, txParams, 2);

        // 8. Configure DIO1 IRQ parameters
        uint16_t irqMask = SX126X_IRQ_ALL;
        uint16_t dio1Mask = SX126X_IRQ_RX_DONE | SX126X_IRQ_TX_DONE |
                            SX126X_IRQ_PREAMBLE_DETECTED | SX126X_IRQ_SYNCWORD_VALID |
                            SX126X_IRQ_CRC_ERR;
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
        uint8_t timeout[3] = {0xFF, 0xFF, 0xFF}; // Continuous RX mode
        executeOpcode(SX126X_CMD_SET_RX, timeout, 3);
    }

    void setPreambleLength(uint16_t preambleLen) {
        // Convert symbols/bytes to bits if needed (default io-homecontrol passes symbol count)
        uint16_t preambleBits = preambleLen;
        if (preambleBits < 16) {
            preambleBits = 16;
        }
        setPacketParamsInternal(preambleBits, 0xFF);
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
        uint8_t rxStatus[2] = {0, 0};
        readOpcode(SX126X_CMD_GET_RX_BUFFER_STATUS, rxStatus, 2, true);
        uint8_t payloadLen = rxStatus[0];
        uint8_t rxStartOffset = rxStatus[1];

        if (payloadLen == 0) {
            size = 0;
            return;
        }

        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_READ_BUFFER);
        SPI.transfer(rxStartOffset);
        SPI.transfer(0x00); // NOP
        for (uint8_t i = 0; i < payloadLen; ++i) {
            buffer[i] = SPI.transfer(0x00);
        }
        spiEnd();
        waitBusy();

        size = payloadLen;
    }

    void writeTxBuffer(const uint8_t *buffer, uint8_t size) {
        waitBusy();
        spiBegin();
        SPI.transfer(SX126X_CMD_WRITE_BUFFER);
        SPI.transfer(0x00); // Start offset 0x00
        for (uint8_t i = 0; i < size; ++i) {
            SPI.transfer(buffer[i]);
        }
        spiEnd();
        waitBusy();

        // Update packet params with exact payload size
        setPacketParamsInternal(currentPreambleLen, size);
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

    void dump() {}
    void dumpReal() {}
    uint16_t readWord(uint8_t regAddr) { return 0; }
    void writeWord(uint8_t regAddr, uint16_t value) {}
}

#endif // RADIO_SX126X
