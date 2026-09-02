/*
 * SPDX-FileCopyrightText: 2026 CloudAXS
 * SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
 */

#ifndef SX126XHELPERS_H
#define SX126XHELPERS_H

#include <sx126xRegs.h>
#include <board-config.h>

#if defined(ESP32)
    #include "mbedtls/aes.h"
#endif

#define LSBFIRST 0
#define MSBFIRST 1

#define KHz     *1000
#define MHz     (KHz *1000)
#define FXOSC   32000000

#define RF_PACKETCONFIG2_IOHOME_POWERFRAME  0x10

namespace Radio {
    enum class Carrier {
        Frequency,
        Deviation,
        Bandwidth,
        Bitrate,
        Modulation
    };

    enum Modulation:uint32_t {
        OOK = 0x00,
        FSK,
        LoRa
    };

    struct WorkingParams {
        uint32_t    carrierFrequency;
        uint8_t     rfOpMode;
        uint32_t    bitRate;
        uint32_t    deviation;
        uint8_t     seqConf[2];
    };

    struct regBandWidth {
        uint8_t     Mant;
        uint8_t     Exp;
    };

    void initHardware();
    void initRegisters(uint8_t maxPayloadLength = 0xff);
    void calibrate();
    void setStandby();
    void setTx();
    void setRx();
    void setPreambleLength(uint16_t preambleLen);
    void clearBuffer();
    void clearFlags();
    bool preambleDetected();
    bool syncedAddress();
    bool dataAvail();
    bool crcOk();
    uint8_t readByte(uint8_t regAddr);
    void readBytes(uint8_t regAddr, uint8_t *out, uint8_t len);
    bool writeByte(uint8_t regAddr, uint8_t data, bool check = false);
    bool writeBytes(uint8_t regAddr, uint8_t *in, uint8_t len, bool check = false);
    bool inStdbyOrSleep();
    bool setParams();
    bool setCarrier(Carrier param, uint32_t value);
    regBandWidth bwRegs(uint8_t bandwidth);
    void dump();
    void dumpReal();
    int dump_fsk_registers(const uint8_t *regs);

    uint16_t readWord(uint8_t regAddr);
    void writeWord(uint8_t regAddr, uint16_t value);

    // SX126x specific extensions
    uint16_t getIrqStatus();
    void clearIrqStatus(uint16_t mask = SX126X_IRQ_ALL);
    void setDioIrqParams(uint16_t irqMask, uint16_t dio1Mask, uint16_t dio2Mask = 0, uint16_t dio3Mask = 0);
    uint8_t getRssiInst();
    void readRxBuffer(uint8_t *buffer, uint8_t &size);
    void writeTxBuffer(const uint8_t *buffer, uint8_t size);
}

#endif // SX126XHELPERS_H
