// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 04 — BPT calibration only (produce a user calibration vector)
//
// Runs a single BPT calibration against a reference cuff reading, then prints
// the resulting user calibration vector as hex over Serial. Copy that vector
// into example 05 (BPTEstimation) to estimate blood pressure later WITHOUT
// recalibrating — or see example 07 to persist it to EEPROM automatically.
//
// WARNING: REF_SYSTOLIC / REF_DIASTOLIC are placeholders. Measure the subject's
// real BP with a clinically validated cuff at the moment of calibration, or the
// vector is meaningless. R&D use only — not a medical device.
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

// The BPT calibration vector is up to 824 bytes, which does not fit alongside
// the Serial buffers and locals in 2 KB of SRAM. Build a stub on those parts
// rather than failing with "data section exceeds available space".
#if defined(__AVR__) && (RAMEND < 0x1000)

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Serial.println(F("04.BPTCalibration needs more RAM than this board has:"));
    Serial.println(F("the BPT calibration vector alone is up to 824 bytes."));
    Serial.println(F("Use a Mega, UNO R4, ESP32, RP2040, STM32 or similar."));
}
void loop() {}

#else

#define RESET_PIN  4
#define MFIO_PIN   2

// Reference cuff measurement — REPLACE WITH YOUR OWN VALUES.
#define REF_SYSTOLIC   120
#define REF_DIASTOLIC   80
#define CAL_INDEX        0   // multi-point firmware: subject slot 0..4

// Allocate the larger of the two possible vector sizes (824 legacy / 512 new).
#define CALIB_VECTOR_MAX  824
static uint8_t calibVector[CALIB_VECTOR_MAX];

PulseExpress hub(RESET_PIN, MFIO_PIN);

static void halt(const char *step, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(step); Serial.print(F(" failed: 0x"));
        Serial.println(uint8_t(s), HEX);
        delay(5000);
    }
}

static PulseExpressStatus startCalibration()
{
    if (hub.caps().multiPointCalib)
    {
        PulseExpressCalibrationRef ref;
        ref.calIndex  = CAL_INDEX;
        ref.systolic  = REF_SYSTOLIC;
        ref.diastolic = REF_DIASTOLIC;
        return hub.startCalibration(ref);
    }
    PulseExpressLegacyCalibrationRefs refs;
    refs.systolic[0]  = REF_SYSTOLIC;     refs.diastolic[0] = REF_DIASTOLIC;
    refs.systolic[1]  = REF_SYSTOLIC + 2; refs.diastolic[1] = REF_DIASTOLIC + 1;
    refs.systolic[2]  = REF_SYSTOLIC + 5; refs.diastolic[2] = REF_DIASTOLIC + 2;
    return hub.startCalibration(refs);
}

static void dumpHex(const uint8_t *buf, size_t len)
{
    Serial.println(F("---- BEGIN CALIBRATION VECTOR ----"));
    for (size_t i = 0; i < len; ++i)
    {
        if (buf[i] < 0x10) Serial.print('0');
        Serial.print(buf[i], HEX);
        if ((i & 0x1F) == 0x1F) Serial.println();
        else                    Serial.print(' ');
    }
    Serial.println();
    Serial.println(F("---- END CALIBRATION VECTOR ----"));
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok) halt("hub.begin()", s);
    if (!hub.firmwareSupported())
        Serial.println(F("WARNING: firmware outside validated 40.x line."));

    s = startCalibration();
    if (s != PulseExpressStatus::Ok) halt("startCalibration", s);

    Serial.println(F("Place your finger on the sensor — hold still until 100%."));
    PulseExpressSample sample;
    unsigned long startMs = millis();
    while (true)
    {
        if (hub.readSample(sample) == PulseExpressStatus::Ok)
        {
            Serial.print(F("progress: ")); Serial.print(sample.progress);
            Serial.print(F("%  status: ")); Serial.println(uint8_t(sample.bpStatus));
            if (sample.bpStatus == PulseExpressBpStatus::Success && sample.progress >= 100)
                break;
        }
        if (millis() - startMs > 120000UL) halt("calibration timeout", PulseExpressStatus::Timeout);
        delay(40);
    }

    size_t len = 0;
    s = hub.readCalibrationVector(calibVector, sizeof(calibVector), &len);
    if (s != PulseExpressStatus::Ok) halt("readCalibrationVector", s);

    Serial.print(F("Calibration complete. Vector length: "));
    Serial.println(len);
    dumpHex(calibVector, len);

    hub.stop();
    Serial.println(F("Done. Save the vector above for use with example 05."));
}

void loop()
{
    delay(1000);
}

#endif  // RAM guard
