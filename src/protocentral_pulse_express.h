// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
// SPDX-FileCopyrightText: Copyright (c) 2020 Maxim Integrated / Analog Devices (protocol)

/*
 * ProtoCentral Pulse Express — MAX30102 optical sensor + MAX32664D biometric
 * sensor hub. Public API for the PulseExpress driver.
 *
 * Implements the host-side procedure documented in Maxim/ADI UG6921 Rev 2
 * (11/20) — "Measuring Blood Pressure, Heart Rate, and SpO2 Using MAX32664D".
 * Supports MAX32664D firmware across the 40.x.y line, including the 40.5.0+
 * multi-point calibration / 512-byte vector / YYYYMMDD changes; the behaviour
 * set is selected at runtime by begin() once the hub firmware version is read.
 *
 * Original 2020 driver: Joice Tm, Copyright (c) 2020 ProtoCentral.
 *
 * Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
 * Email: support@protocentral.com
 *
 * This software is licensed under the MIT License.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef _PROTOCENTRAL_PULSE_EXPRESS_H_
#define _PROTOCENTRAL_PULSE_EXPRESS_H_

#include <Arduino.h>
#include <Wire.h>
#include <stdint.h>
#include <stddef.h>

/////////////////////////////////////////////////////////////////////////////////////////
// I2C Address
/////////////////////////////////////////////////////////////////////////////////////////

#define MAX32664_I2C_ADDR              0x55  // 7-bit form (0xAA/0xAB in 8-bit form)

/////////////////////////////////////////////////////////////////////////////////////////
// Hub status byte bits (Family 0x00 / Index 0x00, see UG6921 step 2.1)
/////////////////////////////////////////////////////////////////////////////////////////

#define MAX32664_STATUS_SENSOR_COMM    0x01
#define MAX32664_STATUS_DATA_READY     0x08
#define MAX32664_STATUS_FIFO_OUT_OVR   0x10
#define MAX32664_STATUS_FIFO_IN_OVR    0x20
#define MAX32664_STATUS_DEVICE_BUSY    0x40

/**
 * @brief Driver return codes.
 *
 * Values 0x00..0xFF mirror the sensor-hub status byte (UG6921 Table 1) so a
 * raw hub error can be returned to the caller without remapping. Values
 * starting at 0xE0 are synthesised by the host driver to surface conditions
 * the hub itself does not report (transport failure, programmer error).
 */
enum class PulseExpressStatus : uint8_t
{
    Ok                  = 0x00,
    IllegalIndex        = 0x01,
    IllegalByteCount    = 0x02,
    IllegalConfig       = 0x03,
    NotInAppMode        = 0x05,
    DeviceBusy          = 0xFE,
    UnknownHubError     = 0xFF,

    HostCommError       = 0xE0,
    UnsupportedFirmware = 0xE1,
    Timeout             = 0xE2,
    InvalidArgument     = 0xE3,
    BufferTooSmall      = 0xE4,
    NoDataAvailable     = 0xE5,
    NotConfigured       = 0xE6,
};

/**
 * @brief Hub or algorithm firmware version triplet (e.g. 40.6.0).
 */
struct PulseExpressVersion
{
    uint8_t major;
    uint8_t minor;
    uint8_t patch;

    /// True if (major, minor, patch) >= (M, m, p) lexicographically.
    bool atLeast(uint8_t M, uint8_t m, uint8_t p) const
    {
        if (major != M) return major > M;
        if (minor != m) return minor > m;
        return patch >= p;
    }
};

/**
 * @brief Capability flags derived from the hub firmware version.
 *
 * Populated by PulseExpress::begin() and exposed via PulseExpress::caps() so user
 * code can size buffers correctly and branch on what the hub will accept.
 *
 * Breakpoints from UG6921 Tables 2 & 3:
 *   - 40.2.2+ : BP-medication and Rest-mode setup steps deprecated.
 *   - 40.5.0+ : Calibration vector size 824 -> 512 bytes;
 *               sample size 23 -> 29 bytes;
 *               date format YYMMDD -> YYYYMMDD;
 *               multi-point user calibration (cal_index 0..4) replaces the
 *               legacy 3-systolic / 3-diastolic single-shot procedure.
 */
struct PulseExpressCaps
{
    uint16_t calibVectorBytes = 824;   // 824 (legacy) | 512 (>=40.5.0)
    uint8_t  sampleBytes      = 23;    // 23 (legacy)  | 29 (>=40.5.0)
    bool     dateYYYYMMDD     = false; // false: YYMMDD legacy | true: >=40.5.0
    bool     multiPointCalib  = false; // false: 0x50/04/01+02 | true: 0x50/04/07+08
    bool     sendBpMedication = true;  // dropped in 40.2.2+
    bool     sendRestMode     = true;  // dropped in 40.2.2+
};

/**
 * @brief Wall-clock time supplied to the hub during configureCalibration() /
 *        configureEstimation().
 *
 * The driver re-encodes this as YYMMDD or YYYYMMDD on the wire depending on
 * PulseExpressCaps::dateYYYYMMDD; user code passes the same DateTime regardless
 * of firmware version.
 */
struct PulseExpressDateTime
{
    uint16_t year   = 2025;  // full 4-digit year, e.g. 2025
    uint8_t  month  = 1;     // 1..12
    uint8_t  day    = 1;     // 1..31
    uint8_t  hour   = 0;     // 0..23
    uint8_t  minute = 0;
    uint8_t  second = 0;
};

/**
 * @brief SpO2 calibration coefficients (per Maxim AN6845).
 *
 * Each coefficient is converted to round(10^5 * value) and uploaded as a
 * 32-bit signed integer. Defaults are the example values from UG6921 §2.1.
 */
struct PulseExpressSpo2Coeffs
{
    float a = 1.5958422f;
    float b = -34.659664f;
    float c = 112.68987f;
};

/**
 * @brief Multi-point reference cuff measurement (firmware 40.5.0+).
 *
 * One CalibrationRef is supplied per subject; calIndex 0..4. The firmware
 * stores up to five subject-specific calibrations in parallel.
 */
struct PulseExpressCalibrationRef
{
    uint8_t calIndex  = 0;    // 0..4
    uint8_t systolic  = 120;  // mmHg
    uint8_t diastolic = 80;   // mmHg
};

/**
 * @brief Legacy single-shot reference cuff ramp (firmware <40.5.0).
 *
 * Three systolic and three diastolic reference values, taken at three points
 * around the user's resting BP, uploaded via the deprecated 0x50/04/01 and
 * 0x50/04/02 commands.
 */
struct PulseExpressLegacyCalibrationRefs
{
    uint8_t systolic[3]  = {120, 122, 125};
    uint8_t diastolic[3] = {80, 81, 82};
};

/**
 * @brief BP status code from sample byte 12 (UG6921 Table 4).
 */
enum class PulseExpressBpStatus : uint8_t
{
    NoSignal              = 0,
    InProgress            = 1,
    Success               = 2,
    WeakSignal            = 3,
    Motion                = 4,
    EstimationFailure     = 5,
    CalibrationPartial    = 6,
    SubjectInitFailure    = 7,
    InitCompleted         = 8,
    RefBpTrendingError    = 9,
    RefInconsistency1     = 10,
    RefInconsistency2     = 11,
    RefInconsistency3     = 12,
    RefCountMismatch      = 13,
    RefOutOfLimits        = 14,
    TooManyCalibrations   = 15,
    PulsePressureOutRange = 16,
    HrOutOfRange          = 17,
    HrAboveResting        = 18,
    PerfusionOutOfRange   = 19,
    EstimationRetry       = 20,
    EstimateOutOfRefRange = 21,
    EstimateOutOfMaxLimit = 22,
    NoContact             = 23,
    NoFinger              = 24,
};

/**
 * @brief One algorithm-mode FIFO sample (UG6921 Table 4).
 *
 * Heart rate, SpO2 and R are reported by the hub as fixed-point integers; the
 * helper accessors below return the equivalent floats. Fields beyond pulseFlag
 * are zero on firmware <40.5.0.
 */
struct PulseExpressSample
{
    PulseExpressBpStatus bpStatus       = PulseExpressBpStatus::NoSignal;
    uint8_t          progress       = 0;  // % calibration progress
    uint16_t         heartRate10x   = 0;  // 10x bpm
    uint8_t          systolic       = 0;  // mmHg
    uint8_t          diastolic      = 0;  // mmHg
    uint16_t         spo210x        = 0;  // 10x SpO2 percent
    uint16_t         rValue1000x    = 0;  // 1000x R (used by SpO2 calibration)
    uint8_t          pulseFlag      = 0;  // 1 = R-peak detected
    uint16_t         ibiMs          = 0;  // inter-beat interval, ms (40.5.0+)
    uint8_t          spo2Confidence = 0;  // % (40.5.0+)
    uint8_t          bptReportFlag  = 0;  // 1 = BPT estimation updated (40.5.0+)
    uint8_t          spo2ReportFlag = 0;  // 1 = SpO2 estimation updated (40.5.0+)

    float heartRate() const { return heartRate10x / 10.0f; }
    float spo2()      const { return spo210x      / 10.0f; }
    float rValue()    const { return rValue1000x  / 1000.0f; }
};

/**
 * @brief One raw-mode FIFO sample (24-bit IR + Red ADC counters).
 */
struct PulseExpressRawSample
{
    uint32_t ir  = 0;
    uint32_t red = 0;
};

/////////////////////////////////////////////////////////////////////////////////////////
// Driver Class
/////////////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Driver for the MAX32664D biometric sensor hub.
 *
 * Lifecycle:
 *   1. Construct with the RSTN and MFIO host pin numbers (and optional TwoWire
 *      bus reference for boards with multiple I2C peripherals).
 *   2. From setup(): call Wire.begin(), then begin(). begin() resets the hub,
 *      reads the firmware version, and selects the matching capability set.
 *   3. Enter one of the three modes:
 *        - startCalibration() + readSample() loop -> readCalibrationVector()
 *        - loadCalibrationVector() + startEstimation() + readSamples()
 *        - startRaw() + readRaw()
 *   4. stop() to disable AFE / algorithm / AGC.
 */
class PulseExpress
{
public:
    PulseExpress(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus = Wire);

    /// Optional debug stream (e.g. &Serial). nullptr silences trace output.
    void setDebug(Print *dbg) { _dbg = dbg; }

    /// Cache the wall-clock time the hub will be initialised with on the next
    /// startCalibration()/startEstimation() call. Returns false on out-of-range.
    bool setDateTime(const PulseExpressDateTime &dt);

    /// Reset the hub, enter application mode, read firmware version, derive caps.
    PulseExpressStatus begin();

    PulseExpressVersion version()      const { return _hubVer; }
    /// Algorithm firmware version. Only meaningful when algoVersionValid() is
    /// true: UG6921 documents `AA FF 03` (hub version) but not `AA FF 07`, and
    /// some builds reject the latter with status 0x02 (wrong byte count). When
    /// that happens this stays {0,0,0} — report it as unavailable rather than
    /// as a real "0.0.0", which reads like a broken hub.
    PulseExpressVersion algoVersion()  const { return _algoVer; }
    bool            algoVersionValid() const { return _algoVerValid; }
    PulseExpressCaps    caps()         const { return _caps; }
    uint8_t         mfioPin()      const { return _mfioPin; }

    /// True if the hub firmware is on the validated 40.x line. begin() now
    /// proceeds even when this is false (soft warning), driving the hub with
    /// legacy capability defaults; user code can branch on this to warn.
    bool            firmwareSupported() const { return _fwSupported; }

    // ---------------- BPT calibration mode ----------------------------------

    /// Multi-point flow (firmware 40.5.0+). Repeat for calIndex 0..4.
    PulseExpressStatus startCalibration(const PulseExpressCalibrationRef &ref);

    /// Legacy single-shot flow (firmware <40.5.0).
    PulseExpressStatus startCalibration(const PulseExpressLegacyCalibrationRefs &refs);

    /// Read one sample if DataRdyInt is set. Returns NoDataAvailable if FIFO empty.
    PulseExpressStatus readSample(PulseExpressSample &out);

    /// Read the user calibration vector after BP status==Success / progress==100.
    /// `cap` must be at least caps().calibVectorBytes; `written` returns the
    /// actual byte count (always equal to caps().calibVectorBytes on success).
    PulseExpressStatus readCalibrationVector(uint8_t *out, size_t cap, size_t *written);

    // ---------------- BPT estimation mode -----------------------------------

    /// Multi-point: load the previously-saved vector for one calIndex (0..4).
    PulseExpressStatus loadCalibrationVector(uint8_t calIndex, const uint8_t *vec, size_t len);

    /// Legacy: load the single previously-saved vector.
    PulseExpressStatus loadCalibrationVector(const uint8_t *vec, size_t len);

    PulseExpressStatus startEstimation(const PulseExpressSpo2Coeffs &coeffs = PulseExpressSpo2Coeffs{});

    /// Read up to `cap` samples; `*count` returns the number actually written.
    PulseExpressStatus readSamples(PulseExpressSample *out, size_t cap, size_t *count);

    // ---------------- Raw PPG mode ------------------------------------------

    PulseExpressStatus startRaw();
    PulseExpressStatus readRaw(PulseExpressRawSample *out, size_t cap, size_t *count, bool wantRed = true);

    // ---------------- Diagnostics -------------------------------------------

    /// Decoded sensor-hub status byte (UG6921 Table 1). Surfaced so user code
    /// can detect FIFO overflow / sensor-comm errors during streaming.
    struct HubStatus
    {
        bool sensorCommError;
        bool dataReady;
        bool fifoOutOverflow;
        bool fifoInOverflow;
        bool deviceBusy;
    };

    /// Read and decode the hub status byte (Family 0x00 / Index 0x00).
    PulseExpressStatus readStatus(HubStatus &out) { return readHubStatus(out); }

    /// Sensor index of the optical AFE in the 0x40/0x41/0x44 command families.
    static constexpr uint8_t kAfeSensorIndex = 0x03;

    /// Read a sensor's PART_ID register (UG6921 Table 5 §1.13: `AA 41 03 FF`
    /// -> 0x15 for MAX30101/MAX30102). This is the diagnostic UG6921 Table 1
    /// prescribes when a command returns 0xFF ("Unknown error. Verify that the
    /// communications to the AFE ... are correct by reading the PART_ID"): a
    /// good read proves the hub can reach the AFE over its secondary I2C bus,
    /// so a 0xFF elsewhere is a sequencing/timing problem rather than a broken
    /// sensor. `sensorIdx` defaults to the optical AFE; pass other indices to
    /// probe which slot (if any) a given firmware image maps the sensor to.
    PulseExpressStatus readAfePartId(uint8_t &partId, uint8_t sensorIdx = kAfeSensorIndex);

    /// Discard everything in the hub's output FIFO. An overflowed FIFO makes
    /// subsequent 0x12/0x01 reads fail with 0xFF until it is emptied (UG6921
    /// Table 1, NAK row: "empty the FIFO by reading all the data or reduce the
    /// report rate"), so readRaw()/readSamples() call this to recover.
    PulseExpressStatus flushFifo();

    // ---------------- Teardown ----------------------------------------------

    /// Disable AFE, BPT algorithm, and AGC (UG6921 Tables 5 & 6 §3).
    PulseExpressStatus stop();

private:
    // I2C primitives. Each thin wrapper builds a small command frame on the
    // stack and routes it through writeImpl()/readImpl(), which apply the
    // CMD_DELAY, validate the hub status byte, and retry on 0xFE.
    PulseExpressStatus writeCmd(uint8_t fam, uint8_t idx, uint16_t cmdDelayMs = 0);
    PulseExpressStatus writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint16_t cmdDelayMs = 0);
    PulseExpressStatus writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1, uint16_t cmdDelayMs = 0);
    PulseExpressStatus writeCmd3(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1, uint8_t v2, uint16_t cmdDelayMs = 0);
    PulseExpressStatus writeCmd(uint8_t fam, uint8_t idx, const uint8_t *payload, size_t len, uint16_t cmdDelayMs);
    PulseExpressStatus writeCmd(uint8_t fam, uint8_t idx, uint8_t sub, const uint8_t *payload, size_t len, uint16_t cmdDelayMs);

    PulseExpressStatus readBytes(uint8_t fam, uint8_t idx, uint8_t *out, size_t len, uint16_t cmdDelayMs = 0);
    PulseExpressStatus readBytes(uint8_t fam, uint8_t idx, uint8_t sub, uint8_t *out, size_t len, uint16_t cmdDelayMs = 0);

    // Underlying send-then-status helpers with retry-on-0xFE. `frame` holds
    // the command bytes (fam, idx, optional sub, optional payload) — the
    // caller must build it on the stack before invoking these.
    PulseExpressStatus writeImpl(const uint8_t *frame, size_t frameLen, uint16_t cmdDelayMs);
    PulseExpressStatus readImpl(const uint8_t *frame, size_t frameLen,
                            uint8_t *out, size_t outLen, uint16_t cmdDelayMs);

    // Procedural pieces from UG6921 Tables 2/5/6.
    PulseExpressStatus hardReset();
    PulseExpressStatus enterAppMode();
    PulseExpressStatus readFirmwareVersion(uint8_t indexByte, PulseExpressVersion &out);
    PulseExpressCaps   capsFor(PulseExpressVersion v) const;

    PulseExpressStatus sendDateTime();
    PulseExpressStatus sendSpo2Coeffs(const PulseExpressSpo2Coeffs &c);
    PulseExpressStatus sendCalibrationRef(const PulseExpressCalibrationRef &r);
    PulseExpressStatus sendLegacyCalibrationRefs(const PulseExpressLegacyCalibrationRefs &r);
    PulseExpressStatus setCalIndex(uint8_t idx);
    PulseExpressStatus sendCalibrationVectorChunked(const uint8_t *vec, size_t len);
    PulseExpressStatus setOutputMode(uint8_t mode);
    PulseExpressStatus setFifoIntrThreshold(uint8_t th);
    PulseExpressStatus enableAfe(bool on);
    PulseExpressStatus enableAgc(bool on);
    PulseExpressStatus enableBpt(uint8_t mode);  // 0=disabled, 1=calib, 2=estimation

    PulseExpressStatus readHubStatus(HubStatus &out);
    PulseExpressStatus readNumFifoSamples(uint8_t &nn);
    PulseExpressStatus readFifoSample(uint8_t *out, size_t len);
    void           parseSample(const uint8_t *buf, PulseExpressSample &s) const;

    void trace(const char *msg) const;
    void tracef(const char *fmt, ...) const;

    TwoWire         &_bus;
    Print           *_dbg     = nullptr;
    uint8_t          _resetPin;
    uint8_t          _mfioPin;
    PulseExpressVersion  _hubVer  = {0, 0, 0};
    PulseExpressVersion  _algoVer = {0, 0, 0};
    bool                 _algoVerValid = false;
    PulseExpressCaps     _caps;
    PulseExpressDateTime _now;
    bool             _began        = false;
    bool             _fwSupported  = false;
};

#endif  // _PROTOCENTRAL_PULSE_EXPRESS_H_
