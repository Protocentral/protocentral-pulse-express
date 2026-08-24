// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
// SPDX-FileCopyrightText: Copyright (c) 2020 Maxim Integrated / Analog Devices (protocol)

/*
 * ProtoCentral Pulse Express — MAX30102 + MAX32664D biometric sensor hub driver.
 *
 * See protocentral_pulse_express.h for the public API and UG6921 Rev 2
 * (Maxim/ADI) for the underlying I2C command set.
 *
 * Original 2020 driver: Joice Tm, Copyright (c) 2020 ProtoCentral.
 * Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics.
 *
 * This software is licensed under the MIT License. See LICENSE.md for the full
 * text. THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND.
 */

#include "protocentral_pulse_express.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////
// Internal constants
/////////////////////////////////////////////////////////////////////////////////////////

namespace
{

// Per-command CMD_DELAY values from UG6921 Tables 2/5/6, biased upward to
// match what the original 1.0.x driver shipped (uniform 45 ms + extra ad-hoc
// delays). UG6921 §1.1 quotes a 2 ms default; in practice the hub returns
// 0xFE (busy) on commands that proxy to the MAX30101 over its secondary I2C
// bus when the host polls that aggressively, and the doc itself recommends
// "Increase the CMD_DELAY" as the remedy. We err on the safe side here and
// rely on writeImpl()/readImpl() retry-on-0xFE for residual races.
constexpr uint16_t kDefaultCmdDelayMs       = 10;
constexpr uint16_t kSetDateTimeDelayMs      = 10;
constexpr uint16_t kSetCalIndexDelayMs      = 10;
constexpr uint16_t kSetSpo2CoeffsDelayMs    = 10;
constexpr uint16_t kCalibVectorChunkDelayMs = 30;
constexpr uint16_t kEnableAgcDelayMs        = 30;
constexpr uint16_t kEnableAfeDelayMs        = 60;
constexpr uint16_t kEnableBptDelayMs        = 600;
constexpr uint16_t kDisableAfeDelayMs       = 60;
constexpr uint16_t kDisableBptDelayMs       = 600;
constexpr uint16_t kDisableAgcDelayMs       = 30;
constexpr uint16_t kPostEnableSettleMs      = 200;
constexpr uint16_t kResetSettleMs           = 1500;
// Setting the operating mode (0x01/0x00/<mode>) restarts the hub application;
// it needs the same settling time as a hard reset before the sensor stack is
// ready to talk to the AFE. UG6921's application-mode flows never write this
// byte at all — the hub boots into application mode on its own when MFIO is
// high at reset — so we only send it when the hub reports otherwise.
constexpr uint16_t kEnterAppModeDelayMs     = 20;
constexpr uint16_t kModeSwitchSettleMs      = 1400;
// UG6921 Table 1: a 0xFF ("unknown error") from a sensor command is diagnosed
// by re-reading the AFE PART_ID. The hub can also answer 0xFF while its sensor
// stack is still coming up after a reset, so the AFE enable gets a few spaced
// retries before we give up.
constexpr uint8_t  kAfeEnableRetries        = 3;
constexpr uint16_t kAfeEnableRetryDelayMs   = 250;
// Upper bound on samples discarded by flushFifo() — the hub's output FIFO holds
// far fewer than this, so hitting it means something else is wrong.
constexpr uint16_t kFifoFlushMaxSamples     = 256;
constexpr uint8_t  kMax30101PartIdReg       = 0xFF;
constexpr uint8_t  kMax30101PartIdExpected  = 0x15;
constexpr uint16_t kRawModeSettleMs         = 250;
// LED-current writes (0x40/0x03/RegAddr/Val) proxy to the MAX30101 over the
// hub's secondary I2C bus — give them substantial headroom over the default.
constexpr uint16_t kLedCurrentDelayMs       = 25;

// Retry policy for 0xFE responses. UG6921 §1.1 says "Device is busy. Try
// again. Increase the CMD_DELAY." We retry up to kBusyRetryMax times and
// double the post-write delay each attempt (capped at kBusyMaxDelayMs).
constexpr uint8_t  kBusyRetryMax            = 4;
constexpr uint16_t kBusyMaxDelayMs          = 400;

// I2C chunk size for the long calibration-vector upload. Kept at 30 to match
// the AVR Wire BUFFER_LENGTH of 32 with two bytes of headroom; on platforms
// with larger buffers this still works since each chunk is its own I2C frame.
constexpr uint8_t  kCalibChunkBytes = 30;

// Read-side chunk: at most 30 payload bytes plus the 1-byte status header.
constexpr uint8_t  kReadChunkBytes = 30;

constexpr uint8_t kFifoIntrThreshold      = 0x0F;
constexpr uint8_t kOutputModeAlgo         = 0x03;
constexpr uint8_t kOutputModeSensorOnly   = 0x01;

// FIFO/status reads just return already-buffered data, so they do not need the
// long sensor-proxy CMD_DELAY. Keeping this short is critical: the FIFO must be
// drained faster than the hub fills it or samples are lost. (The hub only
// returns valid FIFO data in the first I2C read transaction after the command,
// so samples must be read one-per-command, not batched — the throughput win is
// purely from this shorter delay vs the 10 ms default.)
constexpr uint16_t kFifoReadDelayMs       = 2;

// LED current registers on the MAX30101: 0x0C (LED1/red) and 0x0D (LED2/IR).
// Half-scale (0x7F) per UG6921 Table 6 §1.8/1.9.
constexpr uint8_t kMax30101Led1RegAddr    = 0x0C;
constexpr uint8_t kMax30101Led2RegAddr    = 0x0D;
constexpr uint8_t kMax30101LedHalfScale   = 0x7F;

inline uint16_t pack16BE(const uint8_t *p)
{
    return uint16_t((uint16_t(p[0]) << 8) | uint16_t(p[1]));
}

inline void packU32BE(uint32_t v, uint8_t out[4])
{
    out[0] = uint8_t((v >> 24) & 0xFF);
    out[1] = uint8_t((v >> 16) & 0xFF);
    out[2] = uint8_t((v >>  8) & 0xFF);
    out[3] = uint8_t( v        & 0xFF);
}

}  // namespace

/////////////////////////////////////////////////////////////////////////////////////////
// Construction & lifecycle
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpress::PulseExpress(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus)
    : _bus(bus), _resetPin(resetPin), _mfioPin(mfioPin)
{
}

bool PulseExpress::setDateTime(const PulseExpressDateTime &dt)
{
    if (dt.year < 2000 || dt.year > 2099)            return false;
    if (dt.month == 0 || dt.month > 12)              return false;
    if (dt.day == 0 || dt.day > 31)                  return false;
    if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) return false;
    _now = dt;
    return true;
}

PulseExpressStatus PulseExpress::begin()
{
    PulseExpressStatus s = hardReset();
    if (s != PulseExpressStatus::Ok) return s;

    s = enterAppMode();
    if (s != PulseExpressStatus::Ok) return s;

    s = readFirmwareVersion(0x03, _hubVer);
    if (s != PulseExpressStatus::Ok) return s;
    // Algorithm version is informational; ignore failure. Not every 40.x build
    // answers 0xFF/0x07 (some return 0x02, wrong byte count for the family), so
    // track whether the read actually succeeded instead of reporting 0.0.0.
    _algoVerValid = (readFirmwareVersion(0x07, _algoVer) == PulseExpressStatus::Ok);
    if (!_algoVerValid) _algoVer = PulseExpressVersion{0, 0, 0};

    _caps = capsFor(_hubVer);
    tracef("hub %u.%u.%u algo %u.%u.%u  vec=%u sample=%u %s%s",
           _hubVer.major, _hubVer.minor, _hubVer.patch,
           _algoVer.major, _algoVer.minor, _algoVer.patch,
           _caps.calibVectorBytes, _caps.sampleBytes,
           _caps.dateYYYYMMDD ? "YYYYMMDD " : "YYMMDD ",
           _caps.multiPointCalib ? "multi-point" : "single-point");

    // Soft version check: the 40.x line is what this driver is validated
    // against. Earlier/other firmware (A/B/C variants, older BPT builds) may
    // not be fully supported, but we no longer hard-fail on it — begin()
    // proceeds with legacy capability defaults and flags the condition via
    // firmwareSupported() so callers can warn. (Was a hard UnsupportedFirmware
    // return prior to this; that blocked earlier-firmware boards outright.)
    _fwSupported = (_hubVer.major == 40);
    if (!_fwSupported)
        tracef("WARNING: hub firmware %u.%u.%u is outside the validated 40.x "
               "line; proceeding with legacy capability defaults",
               _hubVer.major, _hubVer.minor, _hubVer.patch);

    _began = true;
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::hardReset()
{
    pinMode(_mfioPin,  OUTPUT);
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_mfioPin,  HIGH);   // hold MFIO high to select application mode
    digitalWrite(_resetPin, LOW);
    delay(10);
    digitalWrite(_resetPin, HIGH);
    delay(kResetSettleMs);
    pinMode(_mfioPin, INPUT_PULLUP); // hub now drives MFIO as data-ready intr
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::enterAppMode()
{
    // 0x02/0x00 — read operating mode; 0x00 is application mode.
    //
    // After hardReset() with MFIO held high the hub boots straight into
    // application mode, which is why UG6921's flows (Tables 5 & 6) only ever
    // *read* the mode and never write it. Writing 0x01/0x00/0x00 restarts the
    // hub application, and issuing the next command ~20 ms later caught the
    // sensor stack mid-init — the AFE enable then answered 0xFF. So: read
    // first, and only switch (with a full settling delay) if we have to.
    uint8_t mode = 0xFF;
    PulseExpressStatus s = readBytes(0x02, 0x00, &mode, 1);
    if (s != PulseExpressStatus::Ok) return s;
    if (mode == 0x00) return PulseExpressStatus::Ok;

    tracef("hub in mode 0x%02X, switching to application", mode);
    s = writeCmd(0x01, 0x00, uint8_t(0x00), kEnterAppModeDelayMs);
    if (s != PulseExpressStatus::Ok) return s;
    delay(kModeSwitchSettleMs);

    mode = 0xFF;
    s = readBytes(0x02, 0x00, &mode, 1);
    if (s != PulseExpressStatus::Ok) return s;
    if (mode != 0x00) return PulseExpressStatus::NotInAppMode;
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readFirmwareVersion(uint8_t indexByte, PulseExpressVersion &out)
{
    uint8_t buf[3] = {0, 0, 0};
    PulseExpressStatus s = readBytes(0xFF, indexByte, buf, sizeof(buf));
    if (s != PulseExpressStatus::Ok) return s;
    out.major = buf[0];
    out.minor = buf[1];
    out.patch = buf[2];
    return PulseExpressStatus::Ok;
}

PulseExpressCaps PulseExpress::capsFor(PulseExpressVersion v) const
{
    PulseExpressCaps c;  // defaults are legacy values
    if (v.atLeast(40, 2, 2))
    {
        c.sendBpMedication = false;
        c.sendRestMode     = false;
    }
    if (v.atLeast(40, 5, 0))
    {
        c.calibVectorBytes = 512;
        c.sampleBytes      = 29;
        c.dateYYYYMMDD     = true;
        c.multiPointCalib  = true;
        c.extendedSampleFields = true;
    }
    return c;
}

/////////////////////////////////////////////////////////////////////////////////////////
// BPT calibration mode
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpressStatus PulseExpress::startCalibration(const PulseExpressCalibrationRef &ref)
{
    if (!_began)                 return PulseExpressStatus::NotConfigured;
    if (!_caps.multiPointCalib)  return PulseExpressStatus::UnsupportedFirmware;
    if (ref.calIndex > 4)        return PulseExpressStatus::InvalidArgument;

    PulseExpressStatus s = sendDateTime();
    if (s != PulseExpressStatus::Ok) return s;
    s = sendCalibrationRef(ref);
    if (s != PulseExpressStatus::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != PulseExpressStatus::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != PulseExpressStatus::Ok) return s;
    s = enableAgc(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableAfe(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableBpt(0x01);                           if (s != PulseExpressStatus::Ok) return s;
    delay(kPostEnableSettleMs);
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::startCalibration(const PulseExpressLegacyCalibrationRefs &refs)
{
    if (!_began)                 return PulseExpressStatus::NotConfigured;
    // Multi-point firmware rejects the deprecated 0x50/04/01 + 0x50/04/02 path.
    if (_caps.multiPointCalib)   return PulseExpressStatus::UnsupportedFirmware;

    PulseExpressStatus s = sendDateTime();
    if (s != PulseExpressStatus::Ok) return s;
    s = sendLegacyCalibrationRefs(refs);
    if (s != PulseExpressStatus::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != PulseExpressStatus::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != PulseExpressStatus::Ok) return s;
    s = enableAgc(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableAfe(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableBpt(0x01);                           if (s != PulseExpressStatus::Ok) return s;
    delay(kPostEnableSettleMs);
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readSample(PulseExpressSample &out)
{
    HubStatus st;
    PulseExpressStatus s = readHubStatus(st);
    if (s != PulseExpressStatus::Ok) return s;
    if (!st.dataReady) return PulseExpressStatus::NoDataAvailable;

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != PulseExpressStatus::Ok) return s;
    if (nn == 0) return PulseExpressStatus::NoDataAvailable;

    uint8_t buf[32];
    if (_caps.sampleBytes > sizeof(buf)) return PulseExpressStatus::BufferTooSmall;
    s = readFifoSample(buf, _caps.sampleBytes);
    if (s != PulseExpressStatus::Ok) return s;
    parseSample(buf, out);
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readCalibrationVector(uint8_t *out, size_t cap, size_t *written)
{
    if (!_began)                       return PulseExpressStatus::NotConfigured;
    if (!out)                          return PulseExpressStatus::InvalidArgument;
    if (cap < _caps.calibVectorBytes)  return PulseExpressStatus::BufferTooSmall;

    // 0x51/0x04/0x03 — read user calibration vector.
    PulseExpressStatus s = readBytes(0x51, 0x04, 0x03, out, _caps.calibVectorBytes);
    if (s != PulseExpressStatus::Ok) return s;
    if (written) *written = _caps.calibVectorBytes;
    return PulseExpressStatus::Ok;
}

/////////////////////////////////////////////////////////////////////////////////////////
// BPT estimation mode
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpressStatus PulseExpress::loadCalibrationVector(uint8_t calIndex, const uint8_t *vec, size_t len)
{
    if (!_began)                                       return PulseExpressStatus::NotConfigured;
    if (!_caps.multiPointCalib)                        return PulseExpressStatus::UnsupportedFirmware;
    if (calIndex > 4 || vec == nullptr || len != _caps.calibVectorBytes)
                                                       return PulseExpressStatus::InvalidArgument;

    PulseExpressStatus s = setCalIndex(calIndex);
    if (s != PulseExpressStatus::Ok) return s;
    return sendCalibrationVectorChunked(vec, len);
}

PulseExpressStatus PulseExpress::loadCalibrationVector(const uint8_t *vec, size_t len)
{
    if (!_began)                                       return PulseExpressStatus::NotConfigured;
    if (_caps.multiPointCalib)                         return PulseExpressStatus::UnsupportedFirmware;
    if (vec == nullptr || len != _caps.calibVectorBytes)
                                                       return PulseExpressStatus::InvalidArgument;
    return sendCalibrationVectorChunked(vec, len);
}

PulseExpressStatus PulseExpress::startEstimation(const PulseExpressSpo2Coeffs &coeffs)
{
    if (!_began) return PulseExpressStatus::NotConfigured;

    PulseExpressStatus s = sendDateTime();
    if (s != PulseExpressStatus::Ok) return s;
    s = sendSpo2Coeffs(coeffs);
    if (s != PulseExpressStatus::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != PulseExpressStatus::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != PulseExpressStatus::Ok) return s;
    s = enableAgc(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableAfe(true);                           if (s != PulseExpressStatus::Ok) return s;
    s = enableBpt(0x02);                           if (s != PulseExpressStatus::Ok) return s;
    delay(kPostEnableSettleMs);
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readSamples(PulseExpressSample *out, size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (!_began || out == nullptr) return PulseExpressStatus::InvalidArgument;

    HubStatus st;
    PulseExpressStatus s = readHubStatus(st);
    if (s != PulseExpressStatus::Ok) return s;
    if (!st.dataReady) return PulseExpressStatus::Ok;  // empty FIFO is not an error

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != PulseExpressStatus::Ok) return s;
    if (nn == 0) return PulseExpressStatus::Ok;
    if (nn > cap) nn = uint8_t(cap);

    // One sample per 0x12 0x01 command: each read is a single I2C transaction
    // (<= 30 bytes), which the hub returns reliably. The throughput win over the
    // original comes from the short kFifoReadDelayMs (see readFifoSample), NOT
    // from batching — the hub only returns valid FIFO data in the first read
    // transaction after the command, so a multi-chunk burst read corrupts.
    uint8_t buf[32];
    if (_caps.sampleBytes > sizeof(buf)) return PulseExpressStatus::BufferTooSmall;

    for (uint8_t i = 0; i < nn; ++i)
    {
        s = readFifoSample(buf, _caps.sampleBytes);
        if (s != PulseExpressStatus::Ok)
        {
            tracef("readSamples: FIFO read failed 0x%02X after %u samples; flushing",
                   uint8_t(s), i);
            flushFifo();
            if (count) *count = i;
            return s;
        }
        parseSample(buf, out[i]);
    }
    if (count) *count = nn;
    return PulseExpressStatus::Ok;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Raw PPG mode
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpressStatus PulseExpress::startRaw()
{
    if (!_began) return PulseExpressStatus::NotConfigured;

    // UG6921 Table 6 §1.2-1.9, in the documented order: sensor-only output
    // mode, FIFO threshold, enable AFE, enable algorithm, disable AGC (so the
    // LED currents set below stay put), settle, then the LED currents.
    //
    // NOTE: the 1.0.x driver also issued "enable BPT in estimation mode"
    // (0x52/04/02) here. In sensor-only output mode the algorithm contributes
    // nothing — the FIFO carries raw MAX30101 counters either way — and on
    // 40.2.2+ that command is rejected unless a calibration vector, date/time
    // and SpO2 coefficients have already been loaded. Making it fatal meant a
    // freshly-booted board could never stream raw PPG. It is now best-effort:
    // we try it, trace the result and carry on.
    PulseExpressStatus s = setOutputMode(kOutputModeSensorOnly);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: output mode failed 0x%02X", uint8_t(s)); return s; }
    s = setFifoIntrThreshold(kFifoIntrThreshold);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: fifo threshold failed 0x%02X", uint8_t(s)); return s; }
    s = enableAfe(true);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: AFE enable failed 0x%02X", uint8_t(s)); return s; }

    PulseExpressStatus bpt = enableBpt(0x02);
    if (bpt != PulseExpressStatus::Ok)
        tracef("startRaw: BPT estimation enable returned 0x%02X (optional in "
               "sensor-only mode; continuing)", uint8_t(bpt));

    s = enableAgc(false);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: AGC disable failed 0x%02X", uint8_t(s)); return s; }
    delay(kRawModeSettleMs);

    // LED1 (red) and LED2 (IR) currents to half-scale. Must come AFTER the AFE
    // (and algorithm, when it starts) is enabled or the values are overwritten
    // during init. Frame is exactly 4 bytes — family, sensor index, MAX30101
    // register address, register value (UG6921 Table 6 §1.8/1.9); a trailing
    // pad byte makes the hub reject the command. The 0x40/03 family proxies to
    // the MAX30101 over the secondary I2C bus, so it needs more headroom than
    // the trivial-command default.
    s = writeCmd(0x40, 0x03, kMax30101Led1RegAddr, kMax30101LedHalfScale,
                 kLedCurrentDelayMs);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: LED1 current failed 0x%02X", uint8_t(s)); return s; }
    s = writeCmd(0x40, 0x03, kMax30101Led2RegAddr, kMax30101LedHalfScale,
                 kLedCurrentDelayMs);
    if (s != PulseExpressStatus::Ok) { tracef("startRaw: LED2 current failed 0x%02X", uint8_t(s)); return s; }
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readRaw(PulseExpressRawSample *out, size_t cap, size_t *count, bool wantRed)
{
    if (count) *count = 0;
    if (!_began || out == nullptr) return PulseExpressStatus::InvalidArgument;

    HubStatus st;
    PulseExpressStatus s = readHubStatus(st);
    if (s != PulseExpressStatus::Ok) return s;
    if (st.fifoOutOverflow)
        trace("readRaw: hub output FIFO overflowed — poll faster or drain the "
              "whole FIFO each pass (loop until count reports 0)");
    if (!st.dataReady) return PulseExpressStatus::Ok;

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != PulseExpressStatus::Ok) return s;
    if (nn == 0) return PulseExpressStatus::Ok;
    if (nn > cap) nn = uint8_t(cap);

    // Per UG6921 Table 4 a sensor-only sample is 12 bytes (4 LEDs * 3 bytes).
    // Only LED1 (IR, bytes 0-2) and LED2 (Red, bytes 3-5) are wired up on the
    // MAX30101 in this product; LED3/4 are reported as zero. One sample per
    // 0x12 0x01 command (single I2C transaction); the speed-up over the original
    // is the short kFifoReadDelayMs, not batching.
    uint8_t buf[16];
    for (uint8_t i = 0; i < nn; ++i)
    {
        s = readFifoSample(buf, 12);
        if (s != PulseExpressStatus::Ok)
        {
            // A FIFO that has overflowed rejects further reads until it is
            // emptied (UG6921 Table 1). Drain it so the next call recovers,
            // and hand back the samples we did get alongside the error.
            tracef("readRaw: FIFO read failed 0x%02X after %u samples; flushing",
                   uint8_t(s), i);
            flushFifo();
            if (count) *count = i;
            return s;
        }
        out[i].ir  = (uint32_t(buf[0]) << 16) | (uint32_t(buf[1]) << 8) | uint32_t(buf[2]);
        out[i].red = wantRed
                       ? ((uint32_t(buf[3]) << 16) | (uint32_t(buf[4]) << 8) | uint32_t(buf[5]))
                       : 0u;
    }
    if (count) *count = nn;
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::stop()
{
    if (!_began) return PulseExpressStatus::Ok;
    // Disable order from UG6921 Tables 5 & 6 §3.
    PulseExpressStatus s1 = enableAfe(false);
    PulseExpressStatus s2 = enableBpt(0x00);
    PulseExpressStatus s3 = enableAgc(false);
    if (s1 != PulseExpressStatus::Ok) return s1;
    if (s2 != PulseExpressStatus::Ok) return s2;
    return s3;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Sub-procedures (private)
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpressStatus PulseExpress::sendDateTime()
{
    // Two 32-bit values: date then time. UG6921 Table 3 0x04 describes the
    // wire format as little-endian, and the worked example for the default
    // value (0xFE 0xA1 0x33 0x01 / 0x7C 0xD9 0x01 0x00 -> 20180101 / 121212)
    // confirms little-endian byte order.
    uint32_t date;
    if (_caps.dateYYYYMMDD)
    {
        date = uint32_t(_now.year)  * 10000u
             + uint32_t(_now.month) * 100u
             + uint32_t(_now.day);
    }
    else
    {
        // Legacy YYMMDD: take the last two digits of the year.
        uint16_t yy = _now.year % 100;
        date = uint32_t(yy)           * 10000u
             + uint32_t(_now.month)   * 100u
             + uint32_t(_now.day);
    }
    uint32_t time = uint32_t(_now.hour)   * 10000u
                  + uint32_t(_now.minute) * 100u
                  + uint32_t(_now.second);

    uint8_t payload[8];
    payload[0] = uint8_t( date        & 0xFF);
    payload[1] = uint8_t((date >>  8) & 0xFF);
    payload[2] = uint8_t((date >> 16) & 0xFF);
    payload[3] = uint8_t((date >> 24) & 0xFF);
    payload[4] = uint8_t( time        & 0xFF);
    payload[5] = uint8_t((time >>  8) & 0xFF);
    payload[6] = uint8_t((time >> 16) & 0xFF);
    payload[7] = uint8_t((time >> 24) & 0xFF);

    // 0x50/0x04/0x04 — set date/time. The trailing 0x04 is the configuration
    // index; payload follows.
    return writeCmd(0x50, 0x04, 0x04, payload, sizeof(payload), kSetDateTimeDelayMs);
}

PulseExpressStatus PulseExpress::sendSpo2Coeffs(const PulseExpressSpo2Coeffs &c)
{
    // Each coefficient is round(10^5 * value) packed as a 32-bit signed
    // integer in MSB-first byte order (per the UG6921 worked example for A,
    // B, C in Table 5 step 1.5).
    auto toFixed = [](float v) -> int32_t {
        double scaled = double(v) * 100000.0;
        return int32_t(scaled >= 0 ? (scaled + 0.5) : (scaled - 0.5));
    };
    int32_t Ai = toFixed(c.a);
    int32_t Bi = toFixed(c.b);
    int32_t Ci = toFixed(c.c);

    uint8_t payload[12];
    packU32BE(uint32_t(Ai), &payload[0]);
    packU32BE(uint32_t(Bi), &payload[4]);
    packU32BE(uint32_t(Ci), &payload[8]);

    // 0x50/0x04/0x06 — set SpO2 calibration coefficients.
    return writeCmd(0x50, 0x04, 0x06, payload, sizeof(payload), kSetSpo2CoeffsDelayMs);
}

PulseExpressStatus PulseExpress::sendCalibrationRef(const PulseExpressCalibrationRef &r)
{
    // 0x50/0x04/0x07 — set cal_index + reference systolic + reference
    // diastolic (introduced in 40.5.0+).
    uint8_t payload[3] = {r.calIndex, r.systolic, r.diastolic};
    return writeCmd(0x50, 0x04, 0x07, payload, sizeof(payload), kSetCalIndexDelayMs);
}

PulseExpressStatus PulseExpress::sendLegacyCalibrationRefs(const PulseExpressLegacyCalibrationRefs &r)
{
    // 0x50/0x04/0x01 — three systolic refs.
    PulseExpressStatus s = writeCmd(0x50, 0x04, 0x01, r.systolic, 3, kDefaultCmdDelayMs);
    if (s != PulseExpressStatus::Ok) return s;
    // 0x50/0x04/0x02 — three diastolic refs.
    return writeCmd(0x50, 0x04, 0x02, r.diastolic, 3, kDefaultCmdDelayMs);
}

PulseExpressStatus PulseExpress::setCalIndex(uint8_t idx)
{
    if (idx > 4) return PulseExpressStatus::InvalidArgument;
    // 0x50/0x04/0x08 — select cal_index for subsequent vector load (40.5.0+).
    uint8_t payload[1] = {idx};
    return writeCmd(0x50, 0x04, 0x08, payload, 1, kSetCalIndexDelayMs);
}

PulseExpressStatus PulseExpress::sendCalibrationVectorChunked(const uint8_t *vec, size_t len)
{
    if (vec == nullptr || len == 0) return PulseExpressStatus::InvalidArgument;

    // The cal-vector upload is 0x50/0x04/0x03 followed by `len` raw bytes.
    // On most Arduino platforms the I2C buffer is too small to hold the whole
    // payload in one beginTransmission/endTransmission (AVR is 32 bytes,
    // ESP32 is 128, etc.), so we split the payload across multiple I2C frames.
    // The first frame carries the 3-byte command header; subsequent frames
    // are raw payload continuations that the hub firmware accumulates until
    // it has received `_caps.calibVectorBytes` bytes total and the host
    // issues a status read to terminate the transaction.
    size_t cursor = 0;
    bool   first  = true;
    while (cursor < len)
    {
        _bus.beginTransmission(MAX32664_I2C_ADDR);
        uint8_t inFrame = 0;
        if (first)
        {
            _bus.write(uint8_t(0x50));
            _bus.write(uint8_t(0x04));
            _bus.write(uint8_t(0x03));
            inFrame = 3;
            first   = false;
        }
        while (inFrame < kCalibChunkBytes && cursor < len)
        {
            _bus.write(vec[cursor++]);
            ++inFrame;
        }
        if (_bus.endTransmission() != 0) return PulseExpressStatus::HostCommError;
        delay(kCalibVectorChunkDelayMs);
    }

    // Final status read confirms the upload was accepted by the hub.
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return PulseExpressStatus::HostCommError;
    uint8_t status = uint8_t(_bus.read());
    if (status != 0x00) return PulseExpressStatus(status);
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::setOutputMode(uint8_t mode)
{
    return writeCmd(0x10, 0x00, mode, kDefaultCmdDelayMs);
}

PulseExpressStatus PulseExpress::setFifoIntrThreshold(uint8_t th)
{
    return writeCmd(0x10, 0x01, th, kDefaultCmdDelayMs);
}

PulseExpressStatus PulseExpress::enableAfe(bool on)
{
    // UG6921 Table 6 §1.4 / §3.1: AA 44 03 <on>, CMD_DELAY 40 ms. Index 0x03 is
    // the optical AFE (MAX30101/MAX30102).
    if (!on)
        return writeCmd(0x44, 0x03, uint8_t(0x00), kDisableAfeDelayMs);

    // Enabling makes the hub configure the AFE over its secondary I2C bus. If
    // the sensor stack is still initialising the hub answers 0xFF ("unknown
    // error"), whose documented remedy (Table 1) is to check AFE comms and
    // insert delay before resending. Space out a few attempts before failing.
    PulseExpressStatus s = PulseExpressStatus::Ok;
    for (uint8_t attempt = 0; attempt <= kAfeEnableRetries; ++attempt)
    {
        s = writeCmd(0x44, 0x03, uint8_t(0x01), kEnableAfeDelayMs);
        if (s == PulseExpressStatus::Ok) return s;
        if (attempt == kAfeEnableRetries) break;
        tracef("AFE enable returned 0x%02X, retry %u of %u in %u ms",
               uint8_t(s), attempt + 1, kAfeEnableRetries, kAfeEnableRetryDelayMs);
        delay(kAfeEnableRetryDelayMs);
    }

    // Out of retries: run the diagnostic UG6921 Table 1 asks for, so the trace
    // says whether the hub can reach the AFE at all.
    uint8_t partId = 0;
    PulseExpressStatus p = readAfePartId(partId);
    if (p == PulseExpressStatus::Ok)
        tracef("AFE PART_ID=0x%02X (expected 0x%02X) — hub<->AFE bus OK, so the "
               "0x%02X is a sequencing/timing fault",
               partId, kMax30101PartIdExpected, uint8_t(s));
    else
        tracef("AFE PART_ID read also failed (0x%02X) — hub cannot reach the "
               "optical AFE; check the sensor's power and secondary I2C bus",
               uint8_t(p));
    return s;
}

PulseExpressStatus PulseExpress::readAfePartId(uint8_t &partId, uint8_t sensorIdx)
{
    // UG6921 Table 5 §1.13: AA 41 03 FF -> AB 00 15.
    return readBytes(0x41, sensorIdx, kMax30101PartIdReg, &partId, 1);
}

PulseExpressStatus PulseExpress::flushFifo()
{
    // Read and discard until the hub reports an empty output FIFO. Bounded so a
    // hub that keeps reporting samples can never wedge the caller's loop.
    uint8_t scratch[16];
    for (uint16_t guard = 0; guard < kFifoFlushMaxSamples; ++guard)
    {
        uint8_t nn = 0;
        PulseExpressStatus s = readNumFifoSamples(nn);
        if (s != PulseExpressStatus::Ok) return s;
        if (nn == 0) return PulseExpressStatus::Ok;
        for (uint8_t i = 0; i < nn && guard < kFifoFlushMaxSamples; ++i, ++guard)
        {
            s = readFifoSample(scratch, 12);
            if (s != PulseExpressStatus::Ok) return s;
        }
    }
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::enableAgc(bool on)
{
    return writeCmd(0x52, 0x00, uint8_t(on ? 0x01 : 0x00),
                    on ? kEnableAgcDelayMs : kDisableAgcDelayMs);
}

PulseExpressStatus PulseExpress::enableBpt(uint8_t mode)
{
    // mode: 0=disabled, 1=calibration, 2=estimation.
    if (mode > 2) return PulseExpressStatus::InvalidArgument;
    return writeCmd(0x52, 0x04, mode,
                    mode == 0 ? kDisableBptDelayMs : kEnableBptDelayMs);
}

/////////////////////////////////////////////////////////////////////////////////////////
// FIFO / sample I/O
/////////////////////////////////////////////////////////////////////////////////////////

PulseExpressStatus PulseExpress::readHubStatus(HubStatus &out)
{
    uint8_t bits = 0;
    PulseExpressStatus s = readBytes(0x00, 0x00, &bits, 1, kFifoReadDelayMs);
    if (s != PulseExpressStatus::Ok) return s;
    out.sensorCommError = (bits & MAX32664_STATUS_SENSOR_COMM)  != 0;
    out.dataReady       = (bits & MAX32664_STATUS_DATA_READY)   != 0;
    out.fifoOutOverflow = (bits & MAX32664_STATUS_FIFO_OUT_OVR) != 0;
    out.fifoInOverflow  = (bits & MAX32664_STATUS_FIFO_IN_OVR)  != 0;
    out.deviceBusy      = (bits & MAX32664_STATUS_DEVICE_BUSY)  != 0;
    return PulseExpressStatus::Ok;
}

PulseExpressStatus PulseExpress::readNumFifoSamples(uint8_t &nn)
{
    return readBytes(0x12, 0x00, &nn, 1, kFifoReadDelayMs);
}

PulseExpressStatus PulseExpress::readFifoSample(uint8_t *out, size_t len)
{
    return readBytes(0x12, 0x01, out, len, kFifoReadDelayMs);
}

void PulseExpress::parseSample(const uint8_t *buf, PulseExpressSample &s) const
{
    // Layout per UG6921 Table 4. Bytes 0..11 are MAX30101 PPG counters in
    // algorithm mode; byte 12 onward is the BPT block.
    s.bpStatus     = PulseExpressBpStatus(buf[12]);
    s.progress     = buf[13];
    s.heartRate10x = pack16BE(&buf[14]);
    s.systolic     = buf[16];
    s.diastolic    = buf[17];
    s.spo210x      = pack16BE(&buf[18]);
    s.rValue1000x  = pack16BE(&buf[20]);
    s.pulseFlag    = buf[22];

    // 40.5.0+ extension fields. Zeroed when caps reports a smaller sample
    // size so user code can rely on a consistent struct layout.
    const bool ext   = _caps.extendedSampleFields;
    s.ibiMs          = ext ? pack16BE(&buf[23]) : 0;
    s.spo2Confidence = ext ? buf[25] : 0;
    s.bptReportFlag  = ext ? buf[26] : 0;
    s.spo2ReportFlag = ext ? buf[27] : 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Low-level I2C primitives
/////////////////////////////////////////////////////////////////////////////////////////

// All command transactions go through writeImpl()/readImpl(), which:
//   - send the pre-built command frame on the I2C bus,
//   - delay CMD_DELAY,
//   - read the hub status byte (and any payload, for the read variant),
//   - retry up to kBusyRetryMax times on 0xFE (busy), doubling the delay
//     each attempt up to kBusyMaxDelayMs.
// Per UG6921 §1.1: "0xFE: Device is busy. Try again. Increase the CMD_DELAY."

PulseExpressStatus PulseExpress::writeImpl(const uint8_t *frame, size_t frameLen, uint16_t cmdDelayMs)
{
    uint16_t actualDelay = cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs;

    for (uint8_t attempt = 0; attempt <= kBusyRetryMax; ++attempt)
    {
        _bus.beginTransmission(MAX32664_I2C_ADDR);
        for (size_t i = 0; i < frameLen; ++i) _bus.write(frame[i]);
        if (_bus.endTransmission() != 0)
        {
            tracef("write fam=0x%02X idx=0x%02X i2c-nack",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0);
            return PulseExpressStatus::HostCommError;
        }
        delay(actualDelay);
        if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
            return PulseExpressStatus::HostCommError;
        uint8_t st = uint8_t(_bus.read());

        if (st == 0x00) return PulseExpressStatus::Ok;
        if (st != 0xFE)
        {
            tracef("write fam=0x%02X idx=0x%02X status=0x%02X",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0, st);
            return PulseExpressStatus(st);
        }
        if (attempt == kBusyRetryMax)
        {
            tracef("write fam=0x%02X idx=0x%02X 0xFE busy after %u retries",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0, attempt);
            return PulseExpressStatus::DeviceBusy;
        }
        actualDelay = (actualDelay < kBusyMaxDelayMs / 2)
                          ? uint16_t(actualDelay * 2u)
                          : kBusyMaxDelayMs;
    }
    return PulseExpressStatus::DeviceBusy;
}

PulseExpressStatus PulseExpress::readImpl(const uint8_t *frame, size_t frameLen,
                                  uint8_t *out, size_t outLen, uint16_t cmdDelayMs)
{
    uint16_t actualDelay = cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs;

    for (uint8_t attempt = 0; attempt <= kBusyRetryMax; ++attempt)
    {
        _bus.beginTransmission(MAX32664_I2C_ADDR);
        for (size_t i = 0; i < frameLen; ++i) _bus.write(frame[i]);
        if (_bus.endTransmission() != 0)
        {
            tracef("read fam=0x%02X idx=0x%02X i2c-nack",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0);
            return PulseExpressStatus::HostCommError;
        }
        delay(actualDelay);

        // Drain (1 + outLen) bytes across as many requestFrom() calls as the
        // platform's Wire buffer requires. The leading byte is the status; on
        // 0xFE we discard the rest of the chunk and retry the whole command.
        size_t   remaining   = outLen;
        uint8_t *cursor      = out;
        bool     firstChunk  = true;
        uint8_t  status      = 0;
        bool     transient   = false;

        while (remaining > 0 || firstChunk)
        {
            uint16_t want = uint16_t((firstChunk ? 1u : 0u) + remaining);
            if (want > kReadChunkBytes) want = kReadChunkBytes;
            uint8_t got = _bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(want));
            if (got != want) return PulseExpressStatus::HostCommError;

            if (firstChunk)
            {
                status = uint8_t(_bus.read());
                --got;
                firstChunk = false;
                if (status != 0x00)
                {
                    while (got--) _bus.read();
                    transient = (status == 0xFE);
                    break;
                }
            }
            while (got-- && remaining > 0)
            {
                *cursor++ = uint8_t(_bus.read());
                --remaining;
            }
            if (remaining == 0) break;
        }

        if (status == 0x00) return PulseExpressStatus::Ok;
        if (!transient)
        {
            tracef("read fam=0x%02X idx=0x%02X status=0x%02X",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0, status);
            return PulseExpressStatus(status);
        }
        if (attempt == kBusyRetryMax)
        {
            tracef("read fam=0x%02X idx=0x%02X 0xFE busy after %u retries",
                   frameLen > 0 ? frame[0] : 0, frameLen > 1 ? frame[1] : 0, attempt);
            return PulseExpressStatus::DeviceBusy;
        }
        actualDelay = (actualDelay < kBusyMaxDelayMs / 2)
                          ? uint16_t(actualDelay * 2u)
                          : kBusyMaxDelayMs;
    }
    return PulseExpressStatus::DeviceBusy;
}

PulseExpressStatus PulseExpress::writeCmd(uint8_t fam, uint8_t idx, uint16_t cmdDelayMs)
{
    uint8_t frame[2] = {fam, idx};
    return writeImpl(frame, 2, cmdDelayMs);
}

PulseExpressStatus PulseExpress::writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint16_t cmdDelayMs)
{
    uint8_t frame[3] = {fam, idx, v0};
    return writeImpl(frame, 3, cmdDelayMs);
}

PulseExpressStatus PulseExpress::writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1,
                                  uint16_t cmdDelayMs)
{
    uint8_t frame[4] = {fam, idx, v0, v1};
    return writeImpl(frame, 4, cmdDelayMs);
}

PulseExpressStatus PulseExpress::writeCmd3(uint8_t fam, uint8_t idx,
                                   uint8_t v0, uint8_t v1, uint8_t v2,
                                   uint16_t cmdDelayMs)
{
    uint8_t frame[5] = {fam, idx, v0, v1, v2};
    return writeImpl(frame, 5, cmdDelayMs);
}

PulseExpressStatus PulseExpress::writeCmd(uint8_t fam, uint8_t idx,
                                  const uint8_t *payload, size_t len,
                                  uint16_t cmdDelayMs)
{
    // Inline assemble: max payload across callers is 12 bytes (SpO2 coeffs).
    uint8_t frame[16];
    if (2 + len > sizeof(frame)) return PulseExpressStatus::BufferTooSmall;
    frame[0] = fam;
    frame[1] = idx;
    for (size_t i = 0; i < len; ++i) frame[2 + i] = payload[i];
    return writeImpl(frame, 2 + len, cmdDelayMs);
}

PulseExpressStatus PulseExpress::writeCmd(uint8_t fam, uint8_t idx, uint8_t sub,
                                  const uint8_t *payload, size_t len,
                                  uint16_t cmdDelayMs)
{
    uint8_t frame[16];
    if (3 + len > sizeof(frame)) return PulseExpressStatus::BufferTooSmall;
    frame[0] = fam;
    frame[1] = idx;
    frame[2] = sub;
    for (size_t i = 0; i < len; ++i) frame[3 + i] = payload[i];
    return writeImpl(frame, 3 + len, cmdDelayMs);
}

PulseExpressStatus PulseExpress::readBytes(uint8_t fam, uint8_t idx,
                                   uint8_t *out, size_t len, uint16_t cmdDelayMs)
{
    uint8_t frame[2] = {fam, idx};
    return readImpl(frame, 2, out, len, cmdDelayMs);
}

PulseExpressStatus PulseExpress::readBytes(uint8_t fam, uint8_t idx, uint8_t sub,
                                   uint8_t *out, size_t len, uint16_t cmdDelayMs)
{
    uint8_t frame[3] = {fam, idx, sub};
    return readImpl(frame, 3, out, len, cmdDelayMs);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////////////////////////

void PulseExpress::trace(const char *msg) const
{
    if (_dbg) _dbg->println(msg);
}

void PulseExpress::tracef(const char *fmt, ...) const
{
    if (!_dbg) return;
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    _dbg->println(buf);
}
