/*
 * SPDX-FileCopyrightText: 2026 CloudAXS
 * SPDX-License-Identifier: LicenseRef-CloudAXS-Proprietary
 */

#ifndef SX126X_REGS_H
#define SX126X_REGS_H

#include <stdint.h>

// SX126x Operational Commands (Opcodes)
#define SX126X_CMD_SET_SLEEP                  0x84
#define SX126X_CMD_SET_STANDBY                0x80
#define SX126X_CMD_SET_FS                     0xC1
#define SX126X_CMD_SET_TX                     0x83
#define SX126X_CMD_SET_RX                     0x82
#define SX126X_CMD_STOP_TIMER_ON_PREAMBLE     0x9F
#define SX126X_CMD_SET_RX_TX_FALLBACK_MODE    0x93
#define SX126X_CMD_SET_CAD                    0xC5
#define SX126X_CMD_SET_TX_CONTINUOUS_WAVE     0xD1
#define SX126X_CMD_SET_TX_INFINITE_PREAMBLE   0xD2
#define SX126X_CMD_SET_REGULATOR_MODE         0x96
#define SX126X_CMD_CALIBRATE                  0x89
#define SX126X_CMD_CALIBRATE_IMAGE            0x98
#define SX126X_CMD_SET_PA_CONFIG              0x95
#define SX126X_CMD_WRITE_REGISTER             0x0D
#define SX126X_CMD_READ_REGISTER              0x1D
#define SX126X_CMD_WRITE_BUFFER               0x0E
#define SX126X_CMD_READ_BUFFER                0x1E
#define SX126X_CMD_SET_DIO_IRQ_PARAMS         0x08
#define SX126X_CMD_GET_IRQ_STATUS             0x12
#define SX126X_CMD_CLEAR_IRQ_STATUS           0x02
#define SX126X_CMD_SET_DIO2_AS_RF_SWITCH_CTRL 0x9D
#define SX126X_CMD_SET_DIO3_AS_TCXO_CTRL      0x97
#define SX126X_CMD_SET_RF_FREQUENCY           0x86
#define SX126X_CMD_SET_PACKET_TYPE            0x8A
#define SX126X_CMD_GET_PACKET_TYPE            0x11
#define SX126X_CMD_SET_TX_PARAMS              0x8E
#define SX126X_CMD_SET_MODULATION_PARAMS      0x8B
#define SX126X_CMD_SET_PACKET_PARAMS          0x8C
#define SX126X_CMD_GET_RX_BUFFER_STATUS       0x13
#define SX126X_CMD_GET_PACKET_STATUS          0x14
#define SX126X_CMD_GET_RSSI_INST              0x15
#define SX126X_CMD_GET_STATS                  0x10
#define SX126X_CMD_RESET_STATS                0x00
#define SX126X_CMD_GET_STATUS                 0xC0
#define SX126X_CMD_CLEAR_DEVICE_ERRORS        0x07
#define SX126X_CMD_GET_DEVICE_ERRORS          0x17
#define SX126X_CMD_SET_BUFFER_BASE_ADDRESS    0x8F

// Standby Modes
#define SX126X_STANDBY_RC                     0x00
#define SX126X_STANDBY_XOSC                   0x01

// Regulator Modes
#define SX126X_REGULATOR_LDO                  0x00
#define SX126X_REGULATOR_DC_DC                0x01

// Packet Types
#define SX126X_PACKET_TYPE_GFSK               0x00
#define SX126X_PACKET_TYPE_LORA               0x01

// IRQ Flags (16-bit)
#define SX126X_IRQ_TX_DONE                    0x0001
#define SX126X_IRQ_RX_DONE                    0x0002
#define SX126X_IRQ_PREAMBLE_DETECTED          0x0004
#define SX126X_IRQ_SYNCWORD_VALID             0x0008
#define SX126X_IRQ_HEADER_VALID               0x0010
#define SX126X_IRQ_HEADER_ERR                 0x0020
#define SX126X_IRQ_CRC_ERR                    0x0040
#define SX126X_IRQ_CAD_DONE                   0x0080
#define SX126X_IRQ_CAD_DETECTED               0x0100
#define SX126X_IRQ_TIMEOUT                    0x0200
#define SX126X_IRQ_ALL                        0x03FF

// GFSK Pulse Shaping
#define SX126X_GFSK_PULSE_SHAPE_OFF           0x00
#define SX126X_GFSK_PULSE_SHAPE_BT_0_3        0x08
#define SX126X_GFSK_PULSE_SHAPE_BT_0_5        0x09
#define SX126X_GFSK_PULSE_SHAPE_BT_0_7        0x0A
#define SX126X_GFSK_PULSE_SHAPE_BT_1_0        0x0B

// GFSK RX Bandwidth values (DSB in kHz)
#define SX126X_GFSK_RX_BW_4800                0x1F
#define SX126X_GFSK_RX_BW_5800                0x17
#define SX126X_GFSK_RX_BW_7300                0x0F
#define SX126X_GFSK_RX_BW_9700                0x1E
#define SX126X_GFSK_RX_BW_11700               0x16
#define SX126X_GFSK_RX_BW_14600               0x0E
#define SX126X_GFSK_RX_BW_19500               0x1D
#define SX126X_GFSK_RX_BW_23400               0x15
#define SX126X_GFSK_RX_BW_29300               0x0D
#define SX126X_GFSK_RX_BW_39000               0x1C
#define SX126X_GFSK_RX_BW_46900               0x14
#define SX126X_GFSK_RX_BW_58600               0x0C
#define SX126X_GFSK_RX_BW_78200               0x1B
#define SX126X_GFSK_RX_BW_93800               0x13
#define SX126X_GFSK_RX_BW_117300              0x0B
#define SX126X_GFSK_RX_BW_156200              0x1A
#define SX126X_GFSK_RX_BW_187200              0x12
#define SX126X_GFSK_RX_BW_234300              0x0A
#define SX126X_GFSK_RX_BW_312000              0x19
#define SX126X_GFSK_RX_BW_373600              0x11
#define SX126X_GFSK_RX_BW_467000              0x09

// GFSK Preamble Detector Lengths
#define SX126X_GFSK_PREAMBLE_DETECT_OFF       0x00
#define SX126X_GFSK_PREAMBLE_DETECT_8         0x04
#define SX126X_GFSK_PREAMBLE_DETECT_16        0x05
#define SX126X_GFSK_PREAMBLE_DETECT_24        0x06
#define SX126X_GFSK_PREAMBLE_DETECT_32        0x07

// GFSK Address Filtering
#define SX126X_GFSK_ADDRESS_FILT_OFF          0x00
#define SX126X_GFSK_ADDRESS_FILT_NODE         0x01
#define SX126X_GFSK_ADDRESS_FILT_NODE_BROAD   0x02

// GFSK Packet Length Type
#define SX126X_GFSK_PACKET_KNOWN_LENGTH       0x00
#define SX126X_GFSK_PACKET_VARIABLE           0x01

// GFSK CRC Types
#define SX126X_GFSK_CRC_OFF                   0x01
#define SX126X_GFSK_CRC_1_BYTE                0x00
#define SX126X_GFSK_CRC_2_BYTE                0x02
#define SX126X_GFSK_CRC_1_BYTE_INV            0x04
#define SX126X_GFSK_CRC_2_BYTE_INV            0x06

// GFSK Whitening
#define SX126X_GFSK_WHITENING_OFF             0x00
#define SX126X_GFSK_WHITENING_ON              0x01

// TX Ramp Times
#define SX126X_RAMP_10_US                     0x00
#define SX126X_RAMP_20_US                     0x01
#define SX126X_RAMP_40_US                     0x02
#define SX126X_RAMP_80_US                     0x03
#define SX126X_RAMP_200_US                    0x04
#define SX126X_RAMP_800_US                    0x05
#define SX126X_RAMP_1700_US                   0x06
#define SX126X_RAMP_3400_US                   0x07

// Internal Direct Register Addresses
#define SX126X_REG_WHITENING_INITIAL_MSB      0x06B8
#define SX126X_REG_WHITENING_INITIAL_LSB      0x06B9
#define SX126X_REG_CRC_INITIAL_MSB            0x06BC
#define SX126X_REG_CRC_INITIAL_LSB            0x06BD
#define SX126X_REG_CRC_POLYNOMIAL_MSB         0x06BE
#define SX126X_REG_CRC_POLYNOMIAL_LSB         0x06BF
#define SX126X_REG_SYNC_WORD_0                0x06C0
#define SX126X_REG_SYNC_WORD_1                0x06C1
#define SX126X_REG_SYNC_WORD_2                0x06C2
#define SX126X_REG_SYNC_WORD_3                0x06C3
#define SX126X_REG_SYNC_WORD_4                0x06C4
#define SX126X_REG_SYNC_WORD_5                0x06C5
#define SX126X_REG_SYNC_WORD_6                0x06C6
#define SX126X_REG_SYNC_WORD_7                0x06C7
#define SX126X_REG_NODE_ADDRESS               0x06CD
#define SX126X_REG_BROADCAST_ADDRESS          0x06CE
#define SX126X_REG_IQ_POLARITY_SETUP          0x0736
#define SX126X_REG_LORA_SYNC_WORD_MSB         0x0740
#define SX126X_REG_LORA_SYNC_WORD_LSB         0x0741
#define SX126X_REG_RANDOM_NUMBER_0            0x0819
#define SX126X_REG_TX_MODULATION              0x0889
#define SX126X_REG_RX_GAIN                    0x08AC
#define SX126X_REG_TX_CLAMP_CONFIG            0x08D8
#define SX126X_REG_OCP_CONFIGURATION          0x08E7
#define SX126X_REG_RTC_CONTROL                0x0902
#define SX126X_REG_XTA_TRIM                   0x0911
#define SX126X_REG_XTB_TRIM                   0x0912
#define SX126X_REG_DIO3_OUTPUT_VOLTAGE_CTRL   0x0920
#define SX126X_REG_EVENT_MASK                 0x0944

#endif // SX126X_REGS_H
