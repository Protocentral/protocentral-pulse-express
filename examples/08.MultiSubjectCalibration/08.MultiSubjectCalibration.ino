// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 08 — Multi-subject BPT calibration (firmware >= 40.5.0)
//
// Firmware 40.5.0+ stores up to five independent calibrations (calIndex 0..4),
// one per subject. This sketch walks through calibrating several subjects in
// turn, printing each subject's vector so you can persist it (see example 07).
// To keep RAM small enough for AVR boards it calibrates one subject at a time
// and reuses a single vector buffer rather than holding all five at once.
//
// Requires multi-point firmware; on older hubs it reports that and stops.
// Replace the placeholder cuff readings with real per-subject measurements.
// R&D use only — not a medical device.

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
    Serial.println(F("08.MultiSubjectCalibration needs more RAM than this board has:"));
    Serial.println(F("the BPT calibration vector alone is up to 824 bytes."));
    Serial.println(F("Use a Mega, UNO R4, ESP32, RP2040, STM32 or similar."));
}
void loop() {}

#else

#define RESET_PIN  4
#define MFIO_PIN   2
#define NUM_SUBJECTS 2          // up to 5 (calIndex 0..4)
#define CALIB_VECTOR_MAX  824

// Per-subject reference cuff readings — REPLACE with real measurements.
static const uint8_t kRefSys[5] = {120, 118, 130, 125, 122};
static const uint8_t kRefDia[5] = { 80,  79,  85,  82,  81};

static uint8_t calibVector[CALIB_VECTOR_MAX];
PulseExpress hub(RESET_PIN, MFIO_PIN);

static void halt(const char *msg, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(msg); Serial.print(F(" (0x")); Serial.print(uint8_t(s), HEX);
        Serial.println(')'); delay(5000);
    }
}

static void waitForKey(uint8_t subject)
{
    Serial.print(F("Subject ")); Serial.print(subject);
    Serial.println(F(": place finger on sensor, then send any character to start."));
    while (Serial.available()) Serial.read();
    while (!Serial.available()) delay(20);
    while (Serial.available()) Serial.read();
}

static PulseExpressStatus calibrateSubject(uint8_t calIndex, size_t &lenOut)
{
    PulseExpressCalibrationRef ref;
    ref.calIndex  = calIndex;
    ref.systolic  = kRefSys[calIndex];
    ref.diastolic = kRefDia[calIndex];

    PulseExpressStatus s = hub.startCalibration(ref);
    if (s != PulseExpressStatus::Ok) return s;

    PulseExpressSample sample;
    unsigned long startMs = millis();
    while (true)
    {
        if (hub.readSample(sample) == PulseExpressStatus::Ok)
        {
            Serial.print(F("  progress: ")); Serial.print(sample.progress); Serial.println('%');
            if (sample.bpStatus == PulseExpressBpStatus::Success && sample.progress >= 100) break;
        }
        if (millis() - startMs > 120000UL) return PulseExpressStatus::Timeout;
        delay(40);
    }
    s = hub.readCalibrationVector(calibVector, sizeof(calibVector), &lenOut);
    if (s != PulseExpressStatus::Ok) return s;
    hub.stop();
    return PulseExpressStatus::Ok;
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok) halt("hub.begin()", s);

    if (!hub.caps().multiPointCalib)
    {
        Serial.println(F("This hub firmware does not support multi-point calibration."));
        Serial.println(F("Multi-subject calibration requires firmware >= 40.5.0."));
        halt("unsupported firmware", PulseExpressStatus::UnsupportedFirmware);
    }

    for (uint8_t i = 0; i < NUM_SUBJECTS; ++i)
    {
        waitForKey(i);
        size_t len = 0;
        s = calibrateSubject(i, len);
        if (s != PulseExpressStatus::Ok) halt("calibrateSubject", s);
        Serial.print(F("Subject ")); Serial.print(i);
        Serial.print(F(" calibrated. Vector length: ")); Serial.println(len);
        Serial.println(F("  (persist calibVector[] now — see example 07)"));
    }

    Serial.println(F("All subjects calibrated."));
}

void loop()
{
    delay(1000);
}

#endif  // RAM guard
