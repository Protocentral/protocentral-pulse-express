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
        while (true) { Serial.print("hub.begin() failed: 0x");
                       Serial.println(uint8_t(s), HEX); delay(5000); }
    }

    Serial.println("=== ProtoCentral Pulse Express — device info ===");
    printVersion("Hub firmware:       ", hub.version());
    Serial.print("Algorithm firmware: ");
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
        Serial.println("unavailable (hub did not answer 0xFF/0x07)");
    }
    Serial.print("Firmware supported: "); Serial.println(hub.firmwareSupported() ? "yes" : "no (legacy defaults)");
    Serial.print("MFIO pin:           "); Serial.println(hub.mfioPin());

    PulseExpressCaps c = hub.caps();
    Serial.println("--- derived capabilities ---");
    Serial.print("Calib vector bytes: "); Serial.println(c.calibVectorBytes);
    Serial.print("Sample bytes:       "); Serial.println(c.sampleBytes);
    Serial.print("Date format:        "); Serial.println(c.dateYYYYMMDD ? "YYYYMMDD" : "YYMMDD");
    Serial.print("Multi-point calib:  "); Serial.println(c.multiPointCalib ? "yes (40.5.0+)" : "no (legacy)");
    Serial.println("=================================================");

    // UG6921 Table 1 prescribes this whenever a command returns 0xFF: read the
    // optical AFE's PART_ID (0x15 for MAX30101/MAX30102) to prove the hub can
    // reach the sensor over its secondary I2C bus.
    uint8_t partId = 0;
    PulseExpressStatus p = hub.readAfePartId(partId);
    Serial.print("AFE PART_ID:        ");
    if (p == PulseExpressStatus::Ok)
    {
        Serial.print("0x"); Serial.print(partId, HEX);
        Serial.println(partId == 0x15 ? " (MAX30101/2 OK)" : " (unexpected!)");
    }
    else
    {
        Serial.print("read failed 0x"); Serial.println(uint8_t(p), HEX);

        // The hub could not reach the AFE at the documented sensor index.
        // Sweep the other indices: if the part answers somewhere else, this
        // firmware image maps it differently and the fix is a driver change.
        // If nothing answers anywhere, the hub cannot see the sensor at all —
        // reflash the hub or suspect the board.
        Serial.println("Scanning sensor indices for a responding AFE...");
        bool found = false;
        for (uint8_t idx = 0; idx <= 0x07; ++idx)
        {
            uint8_t id = 0;
            if (hub.readAfePartId(id, idx) == PulseExpressStatus::Ok)
            {
                found = true;
                Serial.print("  index 0x"); Serial.print(idx, HEX);
                Serial.print(" answered PART_ID 0x"); Serial.println(id, HEX);
            }
        }
        if (!found)
            Serial.println("  no sensor index responded — hub cannot see the AFE.");
    }
    Serial.println("=================================================");

    s = hub.startRaw();
    if (s != PulseExpressStatus::Ok)
    {
        Serial.print("startRaw() failed: 0x");
        Serial.print(uint8_t(s), HEX);
        Serial.println(" — status polling may stay idle. See the trace above "
                       "for the command that failed.");
    }
    else
    {
        Serial.println("Raw acquisition started.");
    }
    delay(500);
}

void loop()
{
    PulseExpress::HubStatus st;
    if (hub.readStatus(st) == PulseExpressStatus::Ok)
    {
        Serial.print("dataReady=");      Serial.print(st.dataReady);
        Serial.print(" fifoOutOvr=");    Serial.print(st.fifoOutOverflow);
        Serial.print(" fifoInOvr=");     Serial.print(st.fifoInOverflow);
        Serial.print(" sensorCommErr="); Serial.print(st.sensorCommError);
        Serial.print(" busy=");          Serial.print(st.deviceBusy);
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

    Serial.print(" samples=");            Serial.print(total);
    if (total > 0) { Serial.print(" ir="); Serial.print(firstIr);
                     Serial.print(" red="); Serial.print(firstRed); }
    Serial.println();

    delay(100);
}
