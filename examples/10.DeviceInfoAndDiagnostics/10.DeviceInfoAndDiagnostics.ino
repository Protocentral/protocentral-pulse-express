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

    PulseExpressStatus s = hub.begin();
    if (s != PulseExpressStatus::Ok)
    {
        while (true) { Serial.print("hub.begin() failed: 0x");
                       Serial.println(uint8_t(s), HEX); delay(5000); }
    }

    Serial.println("=== ProtoCentral Pulse Express — device info ===");
    printVersion("Hub firmware:       ", hub.version());
    printVersion("Algorithm firmware: ", hub.algoVersion());
    Serial.print("Firmware supported: "); Serial.println(hub.firmwareSupported() ? "yes" : "no (legacy defaults)");
    Serial.print("MFIO pin:           "); Serial.println(hub.mfioPin());

    PulseExpressCaps c = hub.caps();
    Serial.println("--- derived capabilities ---");
    Serial.print("Calib vector bytes: "); Serial.println(c.calibVectorBytes);
    Serial.print("Sample bytes:       "); Serial.println(c.sampleBytes);
    Serial.print("Date format:        "); Serial.println(c.dateYYYYMMDD ? "YYYYMMDD" : "YYMMDD");
    Serial.print("Multi-point calib:  "); Serial.println(c.multiPointCalib ? "yes (40.5.0+)" : "no (legacy)");
    Serial.println("=================================================");

    s = hub.startRaw();
    if (s != PulseExpressStatus::Ok)
        Serial.println("startRaw() failed; status polling may stay idle.");
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
        Serial.print(" busy=");          Serial.println(st.deviceBusy);
    }

    // Drain the FIFO so overflow flags reflect current state rather than a
    // backlog we never read.
    PulseExpressRawSample buf[16];
    size_t n = 0;
    hub.readRaw(buf, 16, &n, /*wantRed=*/false);

    delay(500);
}
