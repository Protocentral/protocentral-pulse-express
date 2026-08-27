// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 10 — Device info and live hub diagnostics
//
// Prints the sensor-hub and algorithm firmware versions and the capability set
// the driver derived from them (vector size, sample size, date format,
// multi-point support). Then it starts raw acquisition and polls the hub status
// byte so you can watch the data-ready / FIFO-overflow / sensor-comm flags in
// real time — handy when bringing up new hardware.
//
// Hardware connections (default): SDA=A4, SCL=A5, MFIO=D2, RESET=D4, Vin=5V.

#include <Wire.h>
#include "protocentral_pulse_express.h"

#define RESET_PIN  4
#define MFIO_PIN   2

PulseExpress hub(RESET_PIN, MFIO_PIN);

static void printVersion(const char *label, PulseExpressVersion v)
{
    Serial.print(label);
    Serial.print(v.major); Serial.print('.');
    Serial.print(v.minor); Serial.print('.');
    Serial.println(v.patch);
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    // Trace every hub command (family/index/status) — this is the diagnostics
    // sketch, so we want the failing command named, not just a return code.
    hub.setDebug(&Serial);

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok)
    {
        while (true) { Serial.print(F("hub.begin() failed: 0x"));
                       Serial.println(uint8_t(s), HEX); delay(5000); }
    }

    Serial.println(F("=== ProtoCentral Pulse Express — device info ==="));
    printVersion("Hub firmware:       ", hub.version());
    Serial.print(F("Algorithm firmware: "));
    if (hub.algoVersionValid())
    {
        PulseExpressVersion a = hub.algoVersion();
        Serial.print(a.major); Serial.print('.');
        Serial.print(a.minor); Serial.print('.');
        Serial.println(a.patch);
    }
    else
    {
        // Not every 40.x build answers 0xFF/0x07 — this is normal, not a fault.
        Serial.println(F("unavailable (hub did not answer 0xFF/0x07)"));
    }
    Serial.print(F("Firmware supported: "));
    if (hub.firmwareSupported()) Serial.println(F("yes"));
    else                         Serial.println(F("no (legacy defaults)"));
    Serial.print(F("MFIO pin:           ")); Serial.println(hub.mfioPin());

    PulseExpressCaps c = hub.caps();
    Serial.println(F("--- derived capabilities ---"));
    Serial.print(F("Calib vector bytes: ")); Serial.println(c.calibVectorBytes);
    Serial.print(F("Sample bytes:       ")); Serial.println(c.sampleBytes);
    Serial.print(F("Date format:        "));
    if (c.dateYYYYMMDD) Serial.println(F("YYYYMMDD"));
    else                Serial.println(F("YYMMDD"));
    Serial.print(F("Multi-point calib:  "));
    if (c.multiPointCalib) Serial.println(F("yes (40.5.0+)"));
    else                   Serial.println(F("no (legacy)"));
    Serial.println(F("================================================="));

    // UG6921 Table 1 prescribes this whenever a command returns 0xFF: read the
    // optical AFE's PART_ID (0x15 for MAX30101/MAX30102) to prove the hub can
    // reach the sensor over its secondary I2C bus.
    uint8_t partId = 0;
    PulseExpressStatus p = hub.readAfePartId(partId);
    Serial.print(F("AFE PART_ID:        "));
    if (p == PulseExpressStatus::Ok)
    {
        Serial.print(F("0x")); Serial.print(partId, HEX);
        if (partId == 0x15) Serial.println(F(" (MAX30101/2 OK)"));
        else                Serial.println(F(" (unexpected!)"));
    }
    else
    {
        Serial.print(F("read failed 0x")); Serial.println(uint8_t(p), HEX);

        // The hub could not reach the AFE at the documented sensor index.
        // Sweep the other indices: if the part answers somewhere else, this
        // firmware image maps it differently and the fix is a driver change.
        // If nothing answers anywhere, the hub cannot see the sensor at all —
        // reflash the hub or suspect the board.
        Serial.println(F("Scanning sensor indices for a responding AFE..."));
        bool found = false;
        for (uint8_t idx = 0; idx <= 0x07; ++idx)
        {
            uint8_t id = 0;
            if (hub.readAfePartId(id, idx) == PulseExpressStatus::Ok)
            {
                found = true;
                Serial.print(F("  index 0x")); Serial.print(idx, HEX);
                Serial.print(F(" answered PART_ID 0x")); Serial.println(id, HEX);
            }
        }
        if (!found)
            Serial.println(F("  no sensor index responded — hub cannot see the AFE."));
    }
    Serial.println(F("================================================="));

    s = hub.startRaw();
    if (s != PulseExpressStatus::Ok)
    {
        Serial.print(F("startRaw() failed: 0x"));
        Serial.print(uint8_t(s), HEX);
        Serial.println(F(" — status polling may stay idle. See the trace above "
                         "for the command that failed."));
    }
    else
    {
        Serial.println(F("Raw acquisition started."));
    }
    delay(500);
}

void loop()
{
    PulseExpress::HubStatus st;
    if (hub.readStatus(st) == PulseExpressStatus::Ok)
    {
        Serial.print(F("dataReady="));      Serial.print(st.dataReady);
        Serial.print(F(" fifoOutOvr="));    Serial.print(st.fifoOutOverflow);
        Serial.print(F(" fifoInOvr="));     Serial.print(st.fifoInOverflow);
        Serial.print(F(" sensorCommErr=")); Serial.print(st.sensorCommError);
        Serial.print(F(" busy="));          Serial.print(st.deviceBusy);
    }

    // Drain the FIFO *completely* each pass. readRaw() returns at most `cap`
    // samples per call, so a single call plus a long delay lets the backlog
    // grow until the hub's output FIFO overflows — after which the hub rejects
    // FIFO reads with 0xFF until it is emptied (UG6921 Table 1). Loop until a
    // pass comes back empty.
    PulseExpressRawSample buf[16];
    size_t total = 0, n = 0;
    uint32_t firstIr = 0, firstRed = 0;
    do
    {
        n = 0;
        if (hub.readRaw(buf, 16, &n, /*wantRed=*/true) != PulseExpressStatus::Ok) break;
        if (total == 0 && n > 0) { firstIr = buf[0].ir; firstRed = buf[0].red; }
        total += n;
    } while (n == 16);

    Serial.print(F(" samples="));            Serial.print(total);
    if (total > 0) { Serial.print(F(" ir=")); Serial.print(firstIr);
                     Serial.print(F(" red=")); Serial.print(firstRed); }
    Serial.println();

    delay(100);
}
