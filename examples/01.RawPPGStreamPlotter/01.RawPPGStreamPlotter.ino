// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 01 — Raw PPG stream for the Arduino Serial Plotter
//
// Streams the IR PPG counter from the MAX30102 (via the MAX32664D sensor hub)
// one sample per line. Place a finger on the sensor and open Tools -> Serial
// Plotter at 57600 baud to view the waveform.
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
#define SAMPLE_CAP 32  // max samples to drain per loop iteration

PulseExpress hub(RESET_PIN, MFIO_PIN);

void setup()
{
    Serial.begin(57600);
    Wire.begin();

    hub.setDebug(&Serial);

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok)
    {
        Serial.print("hub.begin() failed: 0x");
        Serial.println(uint8_t(s), HEX);
        while (true) delay(5000);
    }
    Serial.print("Hub firmware ");
    Serial.print(hub.version().major); Serial.print('.');
    Serial.print(hub.version().minor); Serial.print('.');
    Serial.println(hub.version().patch);

    s = hub.startRaw();
    if (s != PulseExpressStatus::Ok)
    {
        Serial.print("startRaw() failed: 0x");
        Serial.println(uint8_t(s), HEX);
        while (true) delay(5000);
    }
    delay(2000);
}

void loop()
{
    // readRaw() returns at most SAMPLE_CAP samples per call, so keep calling it
    // until a pass comes back short. If the host drains slower than the hub
    // fills, the hub's output FIFO overflows and then rejects every FIFO read
    // until it is emptied — streaming would stop for good.
    PulseExpressRawSample buf[SAMPLE_CAP];
    size_t n = 0;
    do
    {
        n = 0;
        if (hub.readRaw(buf, SAMPLE_CAP, &n, /*wantRed=*/false) != PulseExpressStatus::Ok) return;
        for (size_t i = 0; i < n; ++i) Serial.println(buf[i].ir);
    } while (n == SAMPLE_CAP);
}
