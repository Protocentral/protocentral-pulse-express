// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 09 — Heart-rate variability (HRV) from inter-beat intervals
//
// Firmware 40.5.0+ reports an inter-beat interval (IBI, in ms) per sample. This
// sketch collects a rolling window of IBI values and computes two common HRV
// metrics: SDNN (standard deviation of NN intervals) and RMSSD (root mean
// square of successive differences). Both are reported in milliseconds.
//
// IBI is only populated on firmware >= 40.5.0; on older hubs the sketch says so.
// R&D use only — not a medical device.
//
// Hardware connections (default): SDA=A4, SCL=A5, MFIO=D2, RESET=D4, Vin=5V.

#include <Wire.h>
#include "protocentral_pulse_express.h"

#define RESET_PIN  4
#define MFIO_PIN   2
#define WINDOW     32   // number of IBI samples per HRV computation

PulseExpress hub(RESET_PIN, MFIO_PIN);

static uint16_t ibis[WINDOW];
static uint8_t  count = 0;
static uint16_t lastIbi = 0;

static void halt(const char *msg, PulseExpressStatus s)
{
    while (true)
    {
        Serial.print(msg); Serial.print(" (0x"); Serial.print(uint8_t(s), HEX);
        Serial.println(')'); delay(5000);
    }
}

static void computeHrv()
{
    // Mean
    float mean = 0;
    for (uint8_t i = 0; i < count; ++i) mean += ibis[i];
    mean /= count;

    // SDNN
    float var = 0;
    for (uint8_t i = 0; i < count; ++i)
    {
        float d = ibis[i] - mean;
        var += d * d;
    }
    float sdnn = sqrt(var / count);

    // RMSSD
    float ss = 0;
    for (uint8_t i = 1; i < count; ++i)
    {
        float d = float(ibis[i]) - float(ibis[i - 1]);
        ss += d * d;
    }
    float rmssd = (count > 1) ? sqrt(ss / (count - 1)) : 0;

    Serial.print("HRV over "); Serial.print(count); Serial.print(" beats  ");
    Serial.print("mean IBI="); Serial.print(mean, 1); Serial.print(" ms  ");
    Serial.print("SDNN=");     Serial.print(sdnn, 1); Serial.print(" ms  ");
    Serial.print("RMSSD=");    Serial.print(rmssd, 1); Serial.println(" ms");
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok) halt("hub.begin()", s);

    // HRV here is derived entirely from the hub's inter-beat interval, which
    // only exists in the 29-byte sample (>= 40.5.0). On the 23-byte legacy
    // sample ibiMs is always zero, so this sketch would silently collect
    // nothing forever. Stop with an explanation instead.
    if (!hub.caps().extendedSampleFields)
    {
        while (true)
        {
            Serial.print("Hub firmware ");
            Serial.print(hub.version().major); Serial.print('.');
            Serial.print(hub.version().minor); Serial.print('.');
            Serial.println(hub.version().patch);
            Serial.println("does not report inter-beat intervals (needs >= 40.5.0),");
            Serial.println("so HRV cannot be computed. Use 03.HeartRateSpO2 for");
            Serial.println("heart rate on this firmware.");
            delay(5000);
        }
    }

    s = hub.startEstimation();
    if (s != PulseExpressStatus::Ok) halt("startEstimation", s);

    Serial.println("Collecting beats — keep finger still on the sensor.");
    delay(1000);
}

void loop()
{
    // Drain to empty; a hub FIFO left to overflow rejects every later read.
    PulseExpressSample samples[8];
    size_t n = 0;
    do
    {
        n = 0;
        if (hub.readSamples(samples, 8, &n) != PulseExpressStatus::Ok) return;

        for (size_t i = 0; i < n; ++i)
        {
            uint16_t ibi = samples[i].ibiMs;
            // Accept only fresh, plausible intervals (250..2000 ms ~ 30..240 bpm).
            if (ibi >= 250 && ibi <= 2000 && ibi != lastIbi)
            {
                lastIbi = ibi;
                ibis[count++] = ibi;
                if (count >= WINDOW)
                {
                    computeHrv();
                    count = 0;
                }
            }
        }
    } while (n == 8);
    delay(50);
}
