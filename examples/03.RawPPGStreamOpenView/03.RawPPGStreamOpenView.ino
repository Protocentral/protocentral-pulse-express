//////////////////////////////////////////////////////////////////////////////////////////
//
//  Pulse Express — Raw PPG stream for the ProtoCentral OpenView desktop app
//
//  Streams IR + Red PPG counters in the OpenView packet framing protocol over
//  the USB serial link.
//  OpenView GUI: https://github.com/Protocentral/protocentral_openview
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
#define SAMPLE_CAP 32

// OpenView packet framing
#define CES_CMDIF_PKT_START_1  0x0A
#define CES_CMDIF_PKT_START_2  0xFA
#define CES_CMDIF_TYPE_DATA    0x02
#define CES_CMDIF_PKT_STOP     0x0B
#define DATA_LEN               9

const uint8_t DataPacketHeader[5] = {CES_CMDIF_PKT_START_1, CES_CMDIF_PKT_START_2,
                                     DATA_LEN, 0, CES_CMDIF_TYPE_DATA};
const uint8_t DataPacketFooter[2] = {0, CES_CMDIF_PKT_STOP};

Max32664 hub(RESET_PIN, MFIO_PIN);

static void sendDataThroughUart(int16_t ir, int16_t red)
{
    uint8_t payload[DATA_LEN] = {0};
    payload[0] = uint8_t(ir       & 0xFF);
    payload[1] = uint8_t((ir >> 8) & 0xFF);
    payload[2] = uint8_t(red       & 0xFF);
    payload[3] = uint8_t((red >> 8) & 0xFF);

    for (int i = 0; i < 5;        ++i) Serial.write(DataPacketHeader[i]);
    for (int i = 0; i < DATA_LEN; ++i) Serial.write(payload[i]);
    for (int i = 0; i < 2;        ++i) Serial.write(DataPacketFooter[i]);
}

void setup()
{
    Serial.begin(57600);
    Wire.begin();

    if (hub.begin() != Max32664Status::Ok)
    {
        while (true) { Serial.println("hub.begin() failed"); delay(5000); }
    }
    if (hub.startRaw() != Max32664Status::Ok)
    {
        while (true) { Serial.println("startRaw() failed"); delay(5000); }
    }
    delay(2000);
}

void loop()
{
    Max32664RawSample buf[SAMPLE_CAP];
    size_t n = 0;
    if (hub.readRaw(buf, SAMPLE_CAP, &n, /*wantRed=*/true) != Max32664Status::Ok) return;

    for (size_t i = 0; i < n; ++i)
    {
        // OpenView expects 16-bit signed values; the hub reports 24-bit
        // counters that we scale down to fit the legacy packet format.
        int16_t ir  = int16_t(buf[i].ir  / 10);
        int16_t red = int16_t(buf[i].red / 10);
        sendDataThroughUart(ir, red);
    }
}
