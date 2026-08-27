// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 06 — Blood Pressure Trending (BPT): calibrate AND estimate, end-to-end
//
// Walks the full flow described in UG6921 in a single sketch:
//   1. begin()                 -> reset hub, read firmware version
//   2. startCalibration()      -> upload reference cuff readings
//   3. readSample() polled      until calibration progress reaches 100%
//   4. readCalibrationVector() -> obtain the user-specific vector
//   5. loadCalibrationVector() -> reload it into the hub
//   6. startEstimation()       -> begin live BPT/SpO2/HR readings
//   7. readSamples() in a loop -> stream results over Serial
//
// See examples 04 and 05 to split calibration and estimation into separate
// sketches with the vector persisted in between (07 shows EEPROM persistence).
//
// WARNING: the SpO2 and reference BP values below are PLACEHOLDERS. For
// meaningful readings calibrate the SpO2 polynomial (a, b, c) per Maxim AN6845
// and measure each subject's real systolic/diastolic BP with a validated cuff
// at the moment of calibration. R&D use only — not a medical device.
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
//
// Original 2020 example: Joice Tm, Copyright (c) 2020 ProtoCentral.

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
    Serial.println(F("06.BPTCalibrateAndEstimate needs more RAM than this board has:"));
    Serial.println(F("the BPT calibration vector alone is up to 824 bytes."));
    Serial.println(F("Use a Mega, UNO R4, ESP32, RP2040, STM32 or similar."));
}
void loop() {}

#else

#define RESET_PIN  4
#define MFIO_PIN   2

// Reference cuff measurements taken with a clinically validated device at the
// moment of calibration. REPLACE WITH YOUR OWN VALUES.
#define REF_SYSTOLIC   120
#define REF_DIASTOLIC   80

// SpO2 calibration polynomial. REPLACE WITH YOUR OWN VALUES (Maxim AN6845).
#define SPO2_COEFF_A    1.5958422f
#define SPO2_COEFF_B  (-34.659664f)
#define SPO2_COEFF_C  112.68987f

// Calibration vector storage. The driver reports the size at runtime: 512
// bytes on firmware >= 40.5.0, 824 bytes on legacy. We allocate the larger
// of the two so the same sketch fits both. On AVR boards with 2 KB SRAM this
// uses ~40% of available memory.
#define CALIB_VECTOR_MAX  824
static uint8_t calibVector[CALIB_VECTOR_MAX];

PulseExpress hub(RESET_PIN, MFIO_PIN);

static void haltWithError(const char *step, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(step);
        Serial.print(F(" failed: 0x"));
        Serial.println(uint8_t(s), HEX);
        delay(5000);
    }
}

static PulseExpressStatus runCalibration()
{
    PulseExpressStatus s;
    if (hub.caps().multiPointCalib)
    {
        PulseExpressCalibrationRef ref;
        ref.calIndex  = 0;
        ref.systolic  = REF_SYSTOLIC;
        ref.diastolic = REF_DIASTOLIC;
        s = hub.startCalibration(ref);
    }
    else
    {
        PulseExpressLegacyCalibrationRefs refs;
        refs.systolic[0]  = REF_SYSTOLIC;
        refs.systolic[1]  = REF_SYSTOLIC + 2;
        refs.systolic[2]  = REF_SYSTOLIC + 5;
        refs.diastolic[0] = REF_DIASTOLIC;
        refs.diastolic[1] = REF_DIASTOLIC + 1;
        refs.diastolic[2] = REF_DIASTOLIC + 2;
        s = hub.startCalibration(refs);
    }
    if (s != PulseExpressStatus::Ok) return s;

    Serial.println(F("Place your finger on the sensor — hold still until 100%."));

    PulseExpressSample sample;
    unsigned long startMs = millis();
    while (true)
    {
        PulseExpressStatus r = hub.readSample(sample);
        if (r == PulseExpressStatus::Ok)
        {
            Serial.print(F("progress: "));
            Serial.print(sample.progress);
            Serial.print(F("%  status: "));
            Serial.println(uint8_t(sample.bpStatus));

            if (sample.bpStatus == PulseExpressBpStatus::Success && sample.progress >= 100) break;
            if (sample.bpStatus == PulseExpressBpStatus::EstimationFailure ||
                sample.bpStatus == PulseExpressBpStatus::SubjectInitFailure ||
                sample.bpStatus == PulseExpressBpStatus::TooManyCalibrations)
            {
                return PulseExpressStatus::UnknownHubError;
            }
        }
        if (millis() - startMs > 120000UL) return PulseExpressStatus::Timeout;
        delay(40);
    }
    return PulseExpressStatus::Ok;
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok) haltWithError("hub.begin()", s);

    Serial.print(F("Hub firmware "));
    Serial.print(hub.version().major); Serial.print('.');
    Serial.print(hub.version().minor); Serial.print('.');
    Serial.println(hub.version().patch);
    Serial.print(F("Calibration vector size: "));
    Serial.println(hub.caps().calibVectorBytes);

    s = runCalibration();
    if (s != PulseExpressStatus::Ok) haltWithError("calibration", s);
    Serial.println(F("Calibration complete; reading vector."));

    size_t calibLen = 0;
    s = hub.readCalibrationVector(calibVector, sizeof(calibVector), &calibLen);
    if (s != PulseExpressStatus::Ok) haltWithError("readCalibrationVector", s);
    Serial.print(F("Stored ")); Serial.print(calibLen); Serial.println(F(" calibration bytes."));

    s = hub.stop();  // tear down calibration mode before re-enabling for estimation
    if (s != PulseExpressStatus::Ok) haltWithError("hub.stop()", s);

    if (hub.caps().multiPointCalib)
        s = hub.loadCalibrationVector(/*calIndex=*/0, calibVector, calibLen);
    else
        s = hub.loadCalibrationVector(calibVector, calibLen);
    if (s != PulseExpressStatus::Ok) haltWithError("loadCalibrationVector", s);

    PulseExpressSpo2Coeffs coeffs;
    coeffs.a = SPO2_COEFF_A;
    coeffs.b = SPO2_COEFF_B;
    coeffs.c = SPO2_COEFF_C;
    s = hub.startEstimation(coeffs);
    if (s != PulseExpressStatus::Ok) haltWithError("startEstimation", s);

    Serial.println(F("Estimation running."));
    delay(1000);
}

void loop()
{
    PulseExpressSample samples[8];
    size_t n = 0;
    // Drain to empty; a hub FIFO left to overflow rejects every later read.
    do
    {
        n = 0;
        PulseExpressStatus s = hub.readSamples(samples, 8, &n);
        if (s != PulseExpressStatus::Ok) return;

        for (size_t i = 0; i < n; ++i)
        {
            Serial.print(F("sys="));   Serial.print(samples[i].systolic);
            Serial.print(F(" dia="));  Serial.print(samples[i].diastolic);
            Serial.print(F(" hr="));   Serial.print(samples[i].heartRate(), 1);
            Serial.print(F(" spo2=")); Serial.print(samples[i].spo2(), 1);
            Serial.print(F(" status="));
            Serial.println(uint8_t(samples[i].bpStatus));
        }
    } while (n == 8);
    delay(100);
}

#endif  // RAM guard
