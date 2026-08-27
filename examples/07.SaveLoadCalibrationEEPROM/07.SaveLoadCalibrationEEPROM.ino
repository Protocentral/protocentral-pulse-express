// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 07 — Persist the BPT calibration vector to EEPROM, reload on boot
//
// First run: no valid vector in EEPROM, so the sketch calibrates and stores the
// resulting vector (plus a small header) to EEPROM. Every subsequent boot it
// finds the stored vector, loads it into the hub, and goes straight to
// estimation — no recalibration needed.
//
// NOTE: the calibration vector is up to 824 bytes. Classic ATmega328 boards
// have 1 KB of EEPROM, so it just fits; ESP32/ESP8266 use emulated EEPROM and
// must call EEPROM.begin()/commit() (handled below). Boards without an EEPROM
// library are reported at build time.
//
// R&D use only — not a medical device.

#include <Wire.h>
#include "protocentral_pulse_express.h"

// EEPROM is present on most cores but not all (Arduino mbed and SAM/Due cores
// lack it). Guard on architecture so the full CI board matrix still compiles;
// unsupported boards build a tiny stub instead. A plain #include (not
// __has_include) is required so arduino-cli adds the EEPROM library path.
// NOTE: the Nano RP2040 Connect defines ARDUINO_ARCH_RP2040 *and*
// ARDUINO_ARCH_MBED, but the Arduino mbed core ships no EEPROM library — only
// the earlephilhower rp2040 core does. Exclude mbed explicitly or this guard
// lets that board through and the build dies on a missing EEPROM.h.
#if (defined(__AVR__) || defined(ARDUINO_ARCH_MEGAAVR) || defined(ARDUINO_ARCH_RENESAS) || \
     defined(ESP32) || defined(ESP8266) || defined(ARDUINO_ARCH_RP2040) || \
     defined(ARDUINO_ARCH_STM32) || defined(ARDUINO_ARCH_APOLLO3)) && \
    !defined(ARDUINO_ARCH_MBED) && \
    !(defined(__AVR__) && (RAMEND < 0x1000))

#include <EEPROM.h>

#define RESET_PIN  4
#define MFIO_PIN   2
#define CAL_INDEX   0

#define REF_SYSTOLIC   120   // REPLACE with a real cuff reading at calibration
#define REF_DIASTOLIC   80

#define EE_MAGIC   0x42505445UL   // "BPTE"
#define EE_ADDR    0              // base EEPROM address
#define CALIB_VECTOR_MAX  824

struct EeHeader { uint32_t magic; uint16_t len; };

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

static void eeBegin(size_t bytes)
{
#if defined(ESP32) || defined(ESP8266)
    EEPROM.begin(bytes);
#else
    (void)bytes;
#endif
}

static void eeCommit()
{
#if defined(ESP32) || defined(ESP8266)
    EEPROM.commit();
#endif
}

static bool loadFromEeprom(size_t expectedLen)
{
    EeHeader h;
    EEPROM.get(EE_ADDR, h);
    if (h.magic != EE_MAGIC || h.len != expectedLen) return false;
    for (size_t i = 0; i < h.len; ++i)
        calibVector[i] = EEPROM.read(EE_ADDR + sizeof(EeHeader) + i);
    return true;
}

static void saveToEeprom(size_t len)
{
    EeHeader h = { EE_MAGIC, uint16_t(len) };
    EEPROM.put(EE_ADDR, h);
    for (size_t i = 0; i < len; ++i)
        EEPROM.write(EE_ADDR + sizeof(EeHeader) + i, calibVector[i]);
    eeCommit();
}

static PulseExpressStatus calibrate(size_t &lenOut)
{
    PulseExpressStatus s;
    if (hub.caps().multiPointCalib)
    {
        PulseExpressCalibrationRef ref;
        ref.calIndex = CAL_INDEX; ref.systolic = REF_SYSTOLIC; ref.diastolic = REF_DIASTOLIC;
        s = hub.startCalibration(ref);
    }
    else
    {
        PulseExpressLegacyCalibrationRefs refs;
        refs.systolic[0]=REF_SYSTOLIC; refs.systolic[1]=REF_SYSTOLIC+2; refs.systolic[2]=REF_SYSTOLIC+5;
        refs.diastolic[0]=REF_DIASTOLIC; refs.diastolic[1]=REF_DIASTOLIC+1; refs.diastolic[2]=REF_DIASTOLIC+2;
        s = hub.startCalibration(refs);
    }
    if (s != PulseExpressStatus::Ok) return s;

    Serial.println(F("Calibrating — finger on sensor, hold still until 100%."));
    PulseExpressSample sample;
    unsigned long startMs = millis();
    while (true)
    {
        if (hub.readSample(sample) == PulseExpressStatus::Ok)
        {
            Serial.print(F("progress: ")); Serial.print(sample.progress); Serial.println('%');
            if (sample.bpStatus == PulseExpressBpStatus::Success && sample.progress >= 100) break;
        }
        if (millis() - startMs > 120000UL) return PulseExpressStatus::Timeout;
        delay(40);
    }
    return hub.readCalibrationVector(calibVector, sizeof(calibVector), &lenOut);
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

    const size_t vecLen = hub.caps().calibVectorBytes;
    eeBegin(sizeof(EeHeader) + vecLen);

    if (loadFromEeprom(vecLen))
    {
        Serial.println(F("Loaded calibration vector from EEPROM."));
    }
    else
    {
        Serial.println(F("No stored vector — running calibration once."));
        size_t len = 0;
        s = calibrate(len);
        if (s != PulseExpressStatus::Ok) halt("calibration", s);
        saveToEeprom(len);
        Serial.println(F("Calibration saved to EEPROM."));
        hub.stop();
    }

    if (hub.caps().multiPointCalib)
        s = hub.loadCalibrationVector(CAL_INDEX, calibVector, vecLen);
    else
        s = hub.loadCalibrationVector(calibVector, vecLen);
    if (s != PulseExpressStatus::Ok) halt("loadCalibrationVector", s);

    s = hub.startEstimation();
    if (s != PulseExpressStatus::Ok) halt("startEstimation", s);
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
        if (hub.readSamples(samples, 8, &n) != PulseExpressStatus::Ok) return;
        for (size_t i = 0; i < n; ++i)
        {
            Serial.print(F("sys="));   Serial.print(samples[i].systolic);
            Serial.print(F(" dia="));  Serial.print(samples[i].diastolic);
            Serial.print(F(" hr="));   Serial.print(samples[i].heartRate(), 1);
            Serial.print(F(" spo2=")); Serial.println(samples[i].spo2(), 1);
        }
    } while (n == 8);
    delay(100);
}

#else  // no EEPROM library on this board

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Serial.println(F("This board has no EEPROM library, or too little RAM for the"));
    Serial.println(F("824-byte calibration vector. See example 05 to load a vector"));
    Serial.println(F("from a code array instead."));
}
void loop() {}

#endif  // EEPROM-capable architecture
