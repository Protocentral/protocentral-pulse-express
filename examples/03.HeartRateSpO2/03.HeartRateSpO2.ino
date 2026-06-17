// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 03 — Live heart rate + SpO2 (no blood-pressure calibration needed)
//
// The simplest "vitals" demo. The MAX32664D algorithm always reports heart
// rate, SpO2 and a per-reading confidence alongside the BPT block, so we can
// stream HR/SpO2 without ever running a BP calibration. Blood-pressure fields
// are NOT valid until you calibrate — see examples 04/05/06 for that.
//
// Place a finger on the sensor and open the Serial Monitor at 57600 baud.
// R&D use only — not a medical device.
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

PulseExpress hub(RESET_PIN, MFIO_PIN);

static void halt(const char *step, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(step);
        Serial.print(" failed: 0x");
        Serial.println(uint8_t(s), HEX);
        delay(5000);
    }
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok) halt("hub.begin()", s);

    Serial.print("Hub firmware ");
    Serial.print(hub.version().major); Serial.print('.');
    Serial.print(hub.version().minor); Serial.print('.');
    Serial.println(hub.version().patch);
    if (!hub.firmwareSupported())
        Serial.println("WARNING: firmware outside validated 40.x line.");

    // Estimation mode streams HR/SpO2 (and BP, once calibrated). We use the
    // default SpO2 coefficients here; calibrate them per AN6845 for accuracy.
    s = hub.startEstimation();
    if (s != PulseExpressStatus::Ok) halt("startEstimation", s);

    Serial.println("Place finger on the sensor...");
    delay(1000);
}

void loop()
{
    PulseExpressSample samples[8];
    size_t n = 0;
    if (hub.readSamples(samples, 8, &n) != PulseExpressStatus::Ok) return;

    for (size_t i = 0; i < n; ++i)
    {
        const PulseExpressSample &s = samples[i];
        Serial.print("HR: ");    Serial.print(s.heartRate(), 1); Serial.print(" bpm");
        Serial.print("  SpO2: "); Serial.print(s.spo2(), 1);      Serial.print(" %");
        Serial.print("  conf: "); Serial.print(s.spo2Confidence); Serial.print(" %");
        Serial.print("  status: "); Serial.println(uint8_t(s.bpStatus));
    }
    delay(100);
}
