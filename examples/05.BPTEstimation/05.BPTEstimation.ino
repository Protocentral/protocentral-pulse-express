// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 05 — BPT estimation from a previously-saved calibration vector
//
// Loads a user calibration vector (produced by example 04) into the hub and
// streams live blood-pressure / heart-rate / SpO2 estimates — no recalibration.
// Paste the hex bytes printed by example 04 into calibVector[] below. Its
// length must match hub.caps().calibVectorBytes (512 on firmware >= 40.5.0,
// 824 on legacy); the sketch checks this and tells you if it does not.
//
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
#define CAL_INDEX   0   // multi-point firmware: subject slot this vector belongs to

// >>> PASTE the calibration vector from example 04 here (whitespace-separated
// hex bytes are fine after reformatting to 0x.. literals). The placeholder
// below is intentionally too short so the sketch halts until you replace it.
static const uint8_t calibVector[] = {
    0x00, 0x00, 0x00, 0x00  // <-- REPLACE with the full vector from example 04
};

PulseExpress hub(RESET_PIN, MFIO_PIN);

static void halt(const char *msg, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(msg); Serial.print(" (0x"); Serial.print(uint8_t(s), HEX);
        Serial.println(')');
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
    if (!hub.firmwareSupported())
        Serial.println("WARNING: firmware outside validated 40.x line.");

    if (sizeof(calibVector) != hub.caps().calibVectorBytes)
    {
        Serial.print("Calibration vector is ");
        Serial.print(sizeof(calibVector));
        Serial.print(" bytes but this firmware expects ");
        Serial.println(hub.caps().calibVectorBytes);
        halt("Paste a full vector from example 04", PulseExpressStatus::InvalidArgument);
    }

    if (hub.caps().multiPointCalib)
        s = hub.loadCalibrationVector(CAL_INDEX, calibVector, sizeof(calibVector));
    else
        s = hub.loadCalibrationVector(calibVector, sizeof(calibVector));
    if (s != PulseExpressStatus::Ok) halt("loadCalibrationVector", s);

    s = hub.startEstimation();
    if (s != PulseExpressStatus::Ok) halt("startEstimation", s);

    Serial.println("Estimation running. Place finger on the sensor.");
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
        Serial.print("sys=");   Serial.print(s.systolic);
        Serial.print(" dia=");  Serial.print(s.diastolic);
        Serial.print(" hr=");   Serial.print(s.heartRate(), 1);
        Serial.print(" spo2="); Serial.print(s.spo2(), 1);
        Serial.print(" status="); Serial.println(uint8_t(s.bpStatus));
    }
    delay(100);
}
