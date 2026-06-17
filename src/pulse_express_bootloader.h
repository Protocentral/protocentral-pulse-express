// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
// SPDX-FileCopyrightText: Copyright (c) 2020 Maxim Integrated / Analog Devices (protocol)

/*
 * ProtoCentral Pulse Express — MAX32664 bootloader-mode API (firmware flashing).
 *
 * This is a SEPARATE, FACTORY / RECOVERY tool, distinct from the application-
 * mode driver in protocentral_pulse_express.h. It implements the in-application
 * programming sequence from Maxim/ADI User Guide 6806, Table 9 ("Annotated I2C
 * Trace for Flashing the Application"):
 *
 *   1. enterBootloader()   - pin-sequence MFIO low at reset, set mode 0x08
 *   2. setNumPages()       - .msbl byte 0x44
 *   3. setInitVector()     - .msbl bytes 0x28..0x32 (11 bytes)
 *   4. setAuth()           - .msbl bytes 0x34..0x43 (16 bytes)
 *   5. eraseApplication()
 *   6. writePage() x N     - .msbl page data from 0x4C, 8208 bytes/page
 *   7. exitToApplication()
 *
 * IMPORTANT: Pulse Express boards ship pre-flashed at the factory. The firmware
 * image (.msbl) is Maxim/ADI IP and is NOT redistributed with this library —
 * supply your own. This code is implemented from the UG6806 spec and MUST be
 * validated on hardware before any production use; a failed flash leaves the
 * hub in bootloader mode (recoverable by re-running the flash).
 */

#ifndef _PULSE_EXPRESS_BOOTLOADER_H_
#define _PULSE_EXPRESS_BOOTLOADER_H_

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <stddef.h>

/// MAX32664 .msbl header field locations (UG6806, Figures 9-12). The values
/// change between firmware revisions but their offsets do not.
#define MSBL_OFFSET_NUM_PAGES   0x44
#define MSBL_OFFSET_INIT_VECTOR 0x28   // 11 bytes: 0x28..0x32
#define MSBL_OFFSET_AUTH        0x34   // 16 bytes: 0x34..0x43
#define MSBL_OFFSET_PAGE_DATA   0x4C   // first page; 8208 bytes each thereafter
#define MSBL_INIT_VECTOR_BYTES  11
#define MSBL_AUTH_BYTES         16
#define MSBL_PAGE_PAYLOAD_BYTES 8208   // 8192 flash + 16 CRC bytes per page

class PulseExpressBootloader
{
public:
    enum class Status : uint8_t
    {
        Ok            = 0x00,
        // 0x80..0x83 mirror the hub bootloader error byte (UG6806 status table).
        TryAgain      = 0x80,
        ChecksumError = 0x81,
        AuthError     = 0x82,
        InvalidApp    = 0x83,
        // Host-synthesised conditions.
        CommError     = 0xE0,
        NotBootloader = 0xE1,
        Timeout       = 0xE2,
        InvalidArg    = 0xE3,
    };

    PulseExpressBootloader(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus = Wire);

    void setDebug(Print *dbg) { _dbg = dbg; }

    /// Sequence RSTN/MFIO to power up in bootloader mode, then confirm via the
    /// operating-mode read (expects 0x08). UG6806 "MAX32664 Bootloader Mode".
    Status enterBootloader();

    Status readBootloaderVersion(uint8_t out[3]);   // 0x81 0x00
    Status readPageSize(uint16_t &sizeOut);         // 0x81 0x01

    Status setNumPages(uint16_t numPages);          // 0x80 0x02
    Status setInitVector(const uint8_t iv[MSBL_INIT_VECTOR_BYTES]);  // 0x80 0x00
    Status setAuth(const uint8_t auth[MSBL_AUTH_BYTES]);             // 0x80 0x01
    Status eraseApplication();                       // 0x80 0x03

    /// Write one page payload (must be MSBL_PAGE_PAYLOAD_BYTES). 0x80 0x04 +
    /// page bytes, chunked across I2C frames to fit small Wire buffers.
    Status writePage(const uint8_t *page, size_t len);

    /// Leave bootloader and start the (newly flashed) application. 0x01 0x00 0x00,
    /// then a reset with MFIO held high to select application mode.
    Status exitToApplication();

private:
    Status setMode(uint8_t mode);
    Status readMode(uint8_t &modeOut);
    Status writeFrame(const uint8_t *frame, size_t len, uint16_t cmdDelayMs);
    Status readFrame(const uint8_t *frame, size_t frameLen,
                     uint8_t *out, size_t outLen, uint16_t cmdDelayMs);
    void   trace(const char *fmt, ...) const;

    TwoWire &_bus;
    Print   *_dbg = nullptr;
    uint8_t  _resetPin;
    uint8_t  _mfioPin;
};

#endif  // _PULSE_EXPRESS_BOOTLOADER_H_
