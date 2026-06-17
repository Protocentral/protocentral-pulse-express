// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
// SPDX-FileCopyrightText: Copyright (c) 2020 Maxim Integrated / Analog Devices (protocol)

/*
 * ProtoCentral Pulse Express — MAX32664 bootloader-mode API.
 * See pulse_express_bootloader.h. Command sequence from UG6806 Table 9.
 *
 * Implemented from spec; validate on hardware before production use.
 */

#include "pulse_express_bootloader.h"

#include <stdarg.h>
#include <stdio.h>

namespace
{
constexpr uint8_t  kI2cAddr           = 0x55;   // 7-bit (0xAA/0xAB 8-bit)
constexpr uint16_t kDefaultDelayMs    = 5;
constexpr uint16_t kEnterBlDelayMs    = 20;
constexpr uint16_t kSetCfgDelayMs     = 10;     // num pages / IV / auth
constexpr uint16_t kEraseDelayMs      = 1500;   // application erase is slow
constexpr uint16_t kPageWriteDelayMs  = 700;    // per-page erase+write
constexpr uint16_t kResetSettleMs     = 50;     // UG6806: 50 ms to reach BL mode
constexpr uint8_t  kBusyRetryMax      = 5;      // 0x80 = ERR_BTLDR_TRY_AGAIN
constexpr uint8_t  kI2cChunkBytes     = 30;     // fits AVR 32-byte Wire buffer
}

PulseExpressBootloader::PulseExpressBootloader(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus)
    : _bus(bus), _resetPin(resetPin), _mfioPin(mfioPin)
{
}

PulseExpressBootloader::Status PulseExpressBootloader::enterBootloader()
{
    // UG6806: hold MFIO LOW while releasing RSTN to select bootloader mode.
    pinMode(_resetPin, OUTPUT);
    pinMode(_mfioPin,  OUTPUT);
    digitalWrite(_mfioPin,  LOW);
    digitalWrite(_resetPin, LOW);
    delay(10);
    digitalWrite(_resetPin, HIGH);
    delay(kResetSettleMs);            // >= 50 ms to reach bootloader
    // MFIO may now be released; the enter-bootloader command must follow within
    // ~780 ms or a valid application would start instead.
    pinMode(_mfioPin, INPUT_PULLUP);

    Status s = setMode(0x08);         // 0x01 0x00 0x08 — enter bootloader
    if (s != Status::Ok) return s;

    uint8_t mode = 0xFF;
    s = readMode(mode);               // 0x02 0x00 — expect 0x08
    if (s != Status::Ok) return s;
    if (mode != 0x08) { trace("not in bootloader (mode=0x%02X)", mode); return Status::NotBootloader; }
    return Status::Ok;
}

PulseExpressBootloader::Status PulseExpressBootloader::setMode(uint8_t mode)
{
    uint8_t f[3] = {0x01, 0x00, mode};
    return writeFrame(f, 3, kEnterBlDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::readMode(uint8_t &modeOut)
{
    uint8_t f[2] = {0x02, 0x00};
    return readFrame(f, 2, &modeOut, 1, kDefaultDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::readBootloaderVersion(uint8_t out[3])
{
    uint8_t f[2] = {0x81, 0x00};
    return readFrame(f, 2, out, 3, kDefaultDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::readPageSize(uint16_t &sizeOut)
{
    uint8_t f[2] = {0x81, 0x01};
    uint8_t buf[2] = {0, 0};
    Status s = readFrame(f, 2, buf, 2, kDefaultDelayMs);
    if (s != Status::Ok) return s;
    sizeOut = uint16_t((uint16_t(buf[0]) << 8) | buf[1]);   // big-endian
    return Status::Ok;
}

PulseExpressBootloader::Status PulseExpressBootloader::setNumPages(uint16_t numPages)
{
    uint8_t f[4] = {0x80, 0x02, uint8_t(numPages >> 8), uint8_t(numPages & 0xFF)};
    return writeFrame(f, 4, kSetCfgDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::setInitVector(const uint8_t iv[MSBL_INIT_VECTOR_BYTES])
{
    uint8_t f[2 + MSBL_INIT_VECTOR_BYTES] = {0x80, 0x00};
    for (uint8_t i = 0; i < MSBL_INIT_VECTOR_BYTES; ++i) f[2 + i] = iv[i];
    return writeFrame(f, sizeof(f), kSetCfgDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::setAuth(const uint8_t auth[MSBL_AUTH_BYTES])
{
    uint8_t f[2 + MSBL_AUTH_BYTES] = {0x80, 0x01};
    for (uint8_t i = 0; i < MSBL_AUTH_BYTES; ++i) f[2 + i] = auth[i];
    return writeFrame(f, sizeof(f), kSetCfgDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::eraseApplication()
{
    uint8_t f[2] = {0x80, 0x03};
    return writeFrame(f, 2, kEraseDelayMs);
}

PulseExpressBootloader::Status PulseExpressBootloader::writePage(const uint8_t *page, size_t len)
{
    if (page == nullptr || len != MSBL_PAGE_PAYLOAD_BYTES) return Status::InvalidArg;

    // 0x80 0x04 followed by the page payload. The payload is far larger than any
    // Arduino Wire buffer, so it is split across I2C frames: the first frame
    // carries the 2-byte command header, the rest are raw continuations the hub
    // accumulates. A single status read terminates the transaction. Retried as a
    // whole on 0x80 (ERR_BTLDR_TRY_AGAIN).
    for (uint8_t attempt = 0; attempt <= kBusyRetryMax; ++attempt)
    {
        size_t cursor = 0;
        bool   first  = true;
        bool   ioOk   = true;
        while (cursor < len)
        {
            _bus.beginTransmission(kI2cAddr);
            uint8_t inFrame = 0;
            if (first) { _bus.write(uint8_t(0x80)); _bus.write(uint8_t(0x04)); inFrame = 2; first = false; }
            while (inFrame < kI2cChunkBytes && cursor < len) { _bus.write(page[cursor++]); ++inFrame; }
            if (_bus.endTransmission() != 0) { ioOk = false; break; }
        }
        if (!ioOk) { trace("writePage i2c-nack"); return Status::CommError; }

        delay(kPageWriteDelayMs);
        if (_bus.requestFrom(kI2cAddr, uint8_t(1)) != 1) return Status::CommError;
        uint8_t st = uint8_t(_bus.read());
        if (st == 0x00) return Status::Ok;
        if (st != 0x80) { trace("writePage status=0x%02X", st); return Status(st); }
        if (attempt == kBusyRetryMax) return Status::TryAgain;
        delay(kPageWriteDelayMs);     // busy: wait and resend the whole page
    }
    return Status::TryAgain;
}

PulseExpressBootloader::Status PulseExpressBootloader::exitToApplication()
{
    Status s = setMode(0x00);         // 0x01 0x00 0x00 — exit bootloader
    // Hard reset back into application mode (MFIO high during reset).
    pinMode(_mfioPin,  OUTPUT);
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_mfioPin,  HIGH);
    digitalWrite(_resetPin, LOW);
    delay(10);
    digitalWrite(_resetPin, HIGH);
    delay(1000);
    pinMode(_mfioPin, INPUT_PULLUP);
    return s;
}

PulseExpressBootloader::Status PulseExpressBootloader::writeFrame(const uint8_t *frame, size_t len,
                                                                  uint16_t cmdDelayMs)
{
    for (uint8_t attempt = 0; attempt <= kBusyRetryMax; ++attempt)
    {
        _bus.beginTransmission(kI2cAddr);
        for (size_t i = 0; i < len; ++i) _bus.write(frame[i]);
        if (_bus.endTransmission() != 0) { trace("write i2c-nack fam=0x%02X", frame[0]); return Status::CommError; }
        delay(cmdDelayMs ? cmdDelayMs : kDefaultDelayMs);
        if (_bus.requestFrom(kI2cAddr, uint8_t(1)) != 1) return Status::CommError;
        uint8_t st = uint8_t(_bus.read());
        if (st == 0x00) return Status::Ok;
        if (st != 0x80) { trace("write status=0x%02X (fam=0x%02X)", st, frame[0]); return Status(st); }
        if (attempt == kBusyRetryMax) return Status::TryAgain;
        delay(cmdDelayMs ? cmdDelayMs : kDefaultDelayMs);
    }
    return Status::TryAgain;
}

PulseExpressBootloader::Status PulseExpressBootloader::readFrame(const uint8_t *frame, size_t frameLen,
                                                                 uint8_t *out, size_t outLen,
                                                                 uint16_t cmdDelayMs)
{
    _bus.beginTransmission(kI2cAddr);
    for (size_t i = 0; i < frameLen; ++i) _bus.write(frame[i]);
    if (_bus.endTransmission() != 0) return Status::CommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultDelayMs);

    uint8_t want = uint8_t(1 + outLen);
    if (_bus.requestFrom(kI2cAddr, want) != want) return Status::CommError;
    uint8_t st = uint8_t(_bus.read());
    for (size_t i = 0; i < outLen; ++i) out[i] = uint8_t(_bus.read());
    if (st == 0x00) return Status::Ok;
    return Status(st);
}

void PulseExpressBootloader::trace(const char *fmt, ...) const
{
    if (!_dbg) return;
    char buf[80];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    _dbg->println(buf);
}
