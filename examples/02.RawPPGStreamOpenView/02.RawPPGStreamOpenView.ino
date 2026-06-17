// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 02 — Raw PPG stream for the ProtoCentral OpenView desktop app
//
// Streams IR + Red PPG counters in the OpenView packet framing protocol over
// the USB serial link.
// OpenView GUI: https://github.com/Protocentral/protocentral_openview
//
// Original 2020 example: Joice Tm, Copyright (c) 2020 ProtoCentral.
//
// Hardware connections (default):
//   | MAX32664 pin | Arduino pin | Function       |
//   |--------------|-------------|----------------|
//   | SDA          | A4          | I2C data       |
//   | SCL          | A5          | I2C clock      |
//   | Vin          | 5V          | Power          |
//   | GND          | GND         |                |
//   | MFIO         | D2          | Data-ready int |
//   | RESET        | D4          | Hub reset      |

#include <Wire.h>
#include "protocentral_pulse_express.h"

#define RESET_PIN  4
#define MFIO_PIN   2
#define SAMPLE_CAP 32

// OpenView packet framing
#define CES_CMDIF_PKT_START_1  0x0A
#define CES_CMDIF_PKT_START_2  0xFA
#define CES_CMDIF_TYPE_DATA    0x02
#define CES_CMDIF_PKT_STOP     0x0B
#define DATA_LEN               9

const uint8_t DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2,
                                     DATA_LEN, 0, CES_CMDIF_TYPE_DATA};
const uint8_t DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};

PulseExpress hub(RESET_PIN, MFIO_PIN);

// Scale a raw counter down to a signed 16-bit range without wrapping.
static int16_t scaleToInt16(uint32_t v)
{
    uint32_t s = v / 10;
    if (s > 32767u) s = 32767u;
    return int16_t(s);
}

static void sendDataThroughUart(int16_t ir, int16_t red)
{
    uint8_t payload[DATA_LEN] = {0};
    payload[0] = uint8_t(ir       & 0xFF);
    payload[1] = uint8_t((ir >> 8) & 0xFF);
    payload[2] = uint8_t(red       & 0xFF);
    payload[3] = uint8_t((red >> 8) & 0xFF);

    for (int i = 0; i < 5;        ++i) Serial.write(DataPacketHeader[i]);
    for (int i = 0; i < DATA_LEN; ++i) Serial.write(payload[i]);
    for (int i = 0; i < 2;        ++i) Serial.write(DataPacketFooter[i]);
}

void setup()
{
    Serial.begin(57600);
    Wire.begin();

    if (hub.begin() != PulseExpressStatus::Ok)
    {
        while (true) { Serial.println("hub.begin() failed"); delay(5000); }
    }
    if (hub.startRaw() != PulseExpressStatus::Ok)
    {
        while (true) { Serial.println("startRaw() failed"); delay(5000); }
    }
    delay(2000);
}

void loop()
{
    PulseExpressRawSample buf[SAMPLE_CAP];
    size_t n = 0;
    if (hub.readRaw(buf, SAMPLE_CAP, &n, /*wantRed=*/true) != PulseExpressStatus::Ok) return;

    for (size_t i = 0; i < n; ++i)
    {
        // Non-blocking send: only emit a packet if the USB-CDC TX buffer has
        // room for a whole one. If the host (OpenView) momentarily stops
        // reading, Serial.write would otherwise block here and let the sensor
        // FIFO back up and overflow — losing a burst of samples and desyncing.
        // Dropping the odd packet during a host stall keeps the read loop fast
        // and the stream in sync, so it recovers cleanly when the host resumes.
        if (Serial.availableForWrite() < (5 + DATA_LEN + 2)) break;

        // OpenView expects 16-bit signed values; the hub reports up-to-24-bit
        // counters. Scale down and CLAMP — without a finger the sensor can rail
        // at a high count whose unclamped cast would wrap and look like a square
        // wave riding on the baseline.
        sendDataThroughUart(scaleToInt16(buf[i].ir), scaleToInt16(buf[i].red));
    }
}
