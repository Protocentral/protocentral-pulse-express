// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
//
// ProtoCentral Pulse Express (MAX30102 + MAX32664D)
// Example 11 — Firmware flasher (FACTORY / RECOVERY tool)
//
// Pulse Express boards ship PRE-FLASHED. You normally never need this. It exists
// for factory programming and recovery, and drives the MAX32664 bootloader to
// write a Maxim/ADI application image (.msbl) over I2C.
//
// The .msbl is Maxim/ADI IP and is NOT distributed with this library — supply
// your own. Because a page payload is 8208 bytes, this sketch needs a host
// board with ample RAM (ESP32, UNO R4, RP2040, STM32, Portenta, ...). On small
// AVR boards it builds a stub.
//
// HOW IT WORKS: this sketch puts the hub in bootloader mode, then takes commands
// over USB-Serial from the host script extras/flash_tool/flash_msbl.py, which
// parses the .msbl and streams the pages. Run:
//     python3 extras/flash_tool/flash_msbl.py --port <PORT> firmware.msbl
//
// WARNING: implemented from UG6806 Table 9; VALIDATE ON HARDWARE before
// production. A failed/interrupted flash leaves the hub in bootloader mode —
// just re-run the flash to recover.

#include <Wire.h>
#include "pulse_express_bootloader.h"

#define RESET_PIN  4
#define MFIO_PIN   2

#if defined(__AVR__)

void setup()
{
    Serial.begin(115200);
    while (!Serial && millis() < 3000) {}
    Serial.println("11.FirmwareFlash needs a board with >= ~16 KB RAM for the");
    Serial.println("8208-byte page buffer (ESP32, UNO R4, RP2040, STM32, ...).");
    Serial.println("This AVR board is too small. Build for a larger host.");
}
void loop() {}

#else

using BL = PulseExpressBootloader;
static BL bl(RESET_PIN, MFIO_PIN);
static uint8_t page[MSBL_PAGE_PAYLOAD_BYTES];

// Read exactly n bytes from Serial with a timeout; returns false on timeout.
static bool readExact(uint8_t *buf, size_t n, unsigned long timeoutMs = 5000)
{
    size_t got = 0;
    unsigned long start = millis();
    while (got < n)
    {
        int c = Serial.read();
        if (c < 0) { if (millis() - start > timeoutMs) return false; continue; }
        buf[got++] = uint8_t(c);
        start = millis();
    }
    return true;
}

static void ack(BL::Status s) { Serial.write(uint8_t(s)); Serial.flush(); }

void setup()
{
    Serial.begin(115200);
    while (!Serial) {}
    Wire.begin();
#if defined(WIRE_BUFFER_SIZE) || defined(ESP32)
    Wire.setClock(400000);
#endif

    bl.setDebug(nullptr);   // keep the serial channel clean for the binary protocol
    BL::Status s = bl.enterBootloader();
    if (s != BL::Status::Ok)
    {
        Serial.print("ERR enterBootloader 0x"); Serial.println(uint8_t(s), HEX);
        while (true) delay(1000);
    }

    uint8_t ver[3] = {0, 0, 0};
    uint16_t pageSize = 0;
    bl.readBootloaderVersion(ver);
    bl.readPageSize(pageSize);
    Serial.print("BL "); Serial.print(ver[0]); Serial.print('.');
    Serial.print(ver[1]); Serial.print('.'); Serial.print(ver[2]);
    Serial.print(" pageSize="); Serial.println(pageSize);
    Serial.println("READY");   // host script waits for this line
}

void loop()
{
    int cmd = Serial.read();
    if (cmd < 0) return;

    switch (cmd)
    {
        case 'N': {                          // set number of pages (2 bytes BE)
            uint8_t b[2];
            if (!readExact(b, 2)) { ack(BL::Status::Timeout); break; }
            ack(bl.setNumPages(uint16_t((b[0] << 8) | b[1])));
            break;
        }
        case 'I': {                          // init vector (11 bytes)
            uint8_t iv[MSBL_INIT_VECTOR_BYTES];
            if (!readExact(iv, sizeof(iv))) { ack(BL::Status::Timeout); break; }
            ack(bl.setInitVector(iv));
            break;
        }
        case 'A': {                          // auth (16 bytes)
            uint8_t au[MSBL_AUTH_BYTES];
            if (!readExact(au, sizeof(au))) { ack(BL::Status::Timeout); break; }
            ack(bl.setAuth(au));
            break;
        }
        case 'E':                            // erase application
            ack(bl.eraseApplication());
            break;
        case 'P': {                          // one page (8208 bytes)
            if (!readExact(page, sizeof(page), 15000)) { ack(BL::Status::Timeout); break; }
            ack(bl.writePage(page, sizeof(page)));
            break;
        }
        case 'X':                            // exit to application
            ack(bl.exitToApplication());
            break;
        default:
            break;                           // ignore stray bytes / newlines
    }
}

#endif  // RAM guard
