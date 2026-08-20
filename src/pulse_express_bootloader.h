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
#define MSBL_OFFSET_PAGE_DATA   0x4C   // first page; page_size + 16 bytes each
#define MSBL_OFFSET_PAGE_SIZE   0x46   // uint16 LE: flash bytes per page (8192)
#define MSBL_INIT_VECTOR_BYTES  11
#define MSBL_AUTH_BYTES         16
#define MSBL_PAGE_CRC_BYTES     16     // appended to every page in the image
/// Largest page payload the sketch buffers: 8192 flash + 16 CRC. Images declare
/// their own page size at MSBL_OFFSET_PAGE_SIZE; this is the buffer bound.
#define MSBL_PAGE_PAYLOAD_BYTES 8208

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
        /// This host's I2C stack cannot issue a transaction long enough to
        /// carry one page. Reported by configureForPageSize() BEFORE the
        /// application is erased, so the board is left bootable.
        PageTooLarge  = 0xE4,
    };

    PulseExpressBootloader(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus = Wire);

    void setDebug(Print *dbg) { _dbg = dbg; }

    /// Sequence RSTN/MFIO to power up in bootloader mode, then confirm via the
    /// operating-mode read (expects 0x08). UG6806 "MAX32664 Bootloader Mode".
    Status enterBootloader();

    Status readBootloaderVersion(uint8_t out[3]);   // 0x81 0x00

    /// Bootloader's reported page size (0x81 0x01), raw and unscaled.
    ///
    /// CAUTION: this is NOT necessarily the number of bytes a page write must
    /// carry. UG6806's trace (bootloader 3.0.0) shows `0x20 0x00` = 8192, which
    /// is the byte count; bootloader 8.0.0 answers 2048 for the same 8192-byte
    /// flash page — i.e. it reports 32-bit words, matching the `4` stored at
    /// .msbl offset 0x48. Always size page writes from the image header
    /// (MSBL_OFFSET_PAGE_SIZE), not from this value, and treat a reported size
    /// of `header/4` as normal rather than as a mismatch.
    Status readPageSize(uint16_t &sizeOut);         // 0x81 0x01

    Status setNumPages(uint16_t numPages);          // 0x80 0x02
    Status setInitVector(const uint8_t iv[MSBL_INIT_VECTOR_BYTES]);  // 0x80 0x00
    Status setAuth(const uint8_t auth[MSBL_AUTH_BYTES]);             // 0x80 0x01
    Status eraseApplication();                       // 0x80 0x03

    /// Write one page payload (page_size + 16 CRC bytes, at most
    /// MSBL_PAGE_PAYLOAD_BYTES). 0x80 0x04 + page bytes, chunked across I2C
    /// frames because the payload dwarfs any Arduino Wire buffer.
    Status writePage(const uint8_t *page, size_t len);

    /// How writePage() puts a page on the wire.
    enum class ChunkMode : uint8_t
    {
        /// One I2C transaction carrying `0x80 0x04` + the whole page. This is
        /// what the bootloader expects (UG6806 Table 9) and the default. It
        /// requires a Wire implementation whose buffer can hold the entire
        /// page — see configureForPageSize().
        SingleTransaction = 0,
        /// Split into kI2cChunkBytes frames, holding the transaction open
        /// between them (repeated START, STOP only on the last). A fallback for
        /// hosts that cannot buffer a whole page; whether the bootloader
        /// accepts it is hub-dependent.
        RepeatedStart     = 1,
        /// Split into frames with a STOP after each. What this driver did
        /// originally; the MAX32664D answers 0x02 (incorrect byte count),
        /// since it scores each frame as a separate command. Kept for A/B
        /// testing only.
        StopEachChunk     = 2,
    };
    void setChunkMode(ChunkMode m) { _chunkMode = m; }
    ChunkMode chunkMode() const { return _chunkMode; }

    /// Prepare the I2C stack to send `payloadLen`-byte pages, growing the Wire
    /// buffer where the core supports it (ESP32, RP2040: WIRE_HAS_BUFFER_SIZE).
    ///
    /// Call BEFORE eraseApplication(): a host that cannot carry a whole page
    /// must fail while the existing firmware is still intact. Returns Ok when
    /// SingleTransaction mode is viable, PageTooLarge when it is not.
    ///
    /// On cores that only honour Wire::setBufferSize() before Wire::begin()
    /// (RP2040), the sketch must pre-size the buffer itself — see
    /// examples/11.FirmwareFlash.
    ///
    /// The Arduino UNO R4's Renesas Wire caps a transaction at 255 bytes
    /// (I2C_BUFFER_LENGTH, and write_to() takes a uint8_t length), so it cannot
    /// flash 8208-byte pages at all. Use an ESP32 or RP2040 host.
    Status configureForPageSize(size_t payloadLen);

    /// Largest single I2C transaction this host can issue, after any
    /// configureForPageSize() growth.
    size_t maxTransactionBytes() const { return _maxTxnBytes; }

    /// Last failure detail recorded by the driver, "" if none. Populated even
    /// when no debug stream is attached, so a sketch driving a binary protocol
    /// can still report why something failed.
    const char *lastError() const { return _lastError; }

    /// Leave bootloader and start the (newly flashed) application. 0x01 0x00 0x00,
    /// then a reset with MFIO held high to select application mode.
    Status exitToApplication();

private:
    Status setMode(uint8_t mode);
    Status readMode(uint8_t &modeOut);
    Status writeFrame(const uint8_t *frame, size_t len, uint16_t cmdDelayMs);
    Status writePageSingle(const uint8_t *page, size_t len);
    Status writePageChunked(const uint8_t *page, size_t len, bool stopEachChunk);
    size_t platformMaxTransactionBytes() const;
    Status readFrame(const uint8_t *frame, size_t frameLen,
                     uint8_t *out, size_t outLen, uint16_t cmdDelayMs);
    void   trace(const char *fmt, ...) const;

    TwoWire  &_bus;
    ChunkMode _chunkMode = ChunkMode::SingleTransaction;
    size_t    _maxTxnBytes = 0;
    mutable char _lastError[96] = {0};   // written by the const trace()
    Print    *_dbg = nullptr;
    uint8_t  _resetPin;
    uint8_t  _mfioPin;
};

#endif  // _PULSE_EXPRESS_BOOTLOADER_H_
