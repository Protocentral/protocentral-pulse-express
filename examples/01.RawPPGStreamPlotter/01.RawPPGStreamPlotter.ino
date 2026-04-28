//////////////////////////////////////////////////////////////////////////////////////////
//
//  Pulse Express — Raw PPG stream for the Arduino Serial Plotter
//
//  Streams the IR PPG counter from the MAX30101 (via the MAX32664D sensor hub)
//  one sample per line. Place a finger on the sensor and open Tools → Serial
//  Plotter at 57600 baud to view the waveform.
//
//  Hardware connections (default):
//    | MAX32664 pin | Arduino pin | Function       |
//    |--------------|-------------|----------------|
//    | SDA          | A4          | I2C data       |
//    | SCL          | A5          | I2C clock      |
//    | Vin          | 5V          | Power          |
//    | GND          | GND         |                |
//    | MFIO         | D2          | Data-ready int |
//    | RESET        | D4          | Hub reset      |
//
//  Original 2020 example: Joice Tm, Copyright (c) 2020 ProtoCentral
//  Modernised:            Copyright (c) 2025 ProtoCentral Electronics
//
//  This software is licensed under the MIT License (http://opensource.org/licenses/MIT).
//
/////////////////////////////////////////////////////////////////////////////////////////

#include <Wire.h>
#include "max32664.h"

#define RESET_PIN  4
#define MFIO_PIN   2
#define SAMPLE_CAP 32  // max samples to drain per loop iteration

Max32664 hub(RESET_PIN, MFIO_PIN);

void setup()
{
    Serial.begin(57600);
    Wire.begin();

    hub.setDebug(&Serial);

    Max32664Status s = hub.begin();
    if (s != Max32664Status::Ok)
    {
        Serial.print("hub.begin() failed: 0x");
        Serial.println(uint8_t(s), HEX);
        while (true) delay(5000);
    }
    Serial.print("Hub firmware ");
    Serial.print(hub.version().major); Serial.print('.');
    Serial.print(hub.version().minor); Serial.print('.');
    Serial.println(hub.version().patch);

    s = hub.startRaw();
    if (s != Max32664Status::Ok)
    {
        Serial.print("startRaw() failed: 0x");
        Serial.println(uint8_t(s), HEX);
        while (true) delay(5000);
    }
    delay(2000);
}

void loop()
{
    Max32664RawSample buf[SAMPLE_CAP];
    size_t n = 0;
    Max32664Status s = hub.readRaw(buf, SAMPLE_CAP, &n, /*wantRed=*/false);
    if (s != Max32664Status::Ok) return;

    for (size_t i = 0; i < n; ++i)
    {
        Serial.println(buf[i].ir);
        delay(3);
    }
}
