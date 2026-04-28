//////////////////////////////////////////////////////////////////////////////////////////
//
//  Arduino library for the ProtoCentral Pulse Express breakout board
//  (MAX30102 optical sensor + MAX32664D biometric sensor hub).
//
//  Implements the host-side procedure documented in:
//    Maxim/ADI UG6921 Rev 2 (11/20) — "Measuring Blood Pressure, Heart Rate,
//    and SpO2 Using MAX32664D — A Quick Start Guide for Programmers".
//
//  Supports MAX32664D firmware revisions across the 40.x.y line, including
//  the 40.5.0+ multi-point calibration / 512-byte vector / YYYYMMDD date
//  changes. The exact behavioural set is selected at runtime via begin()
//  once the hub firmware version has been read back.
//
//  Original 2020 driver: Joice Tm, Copyright (c) 2020 ProtoCentral
//  Modernised rewrite:   Copyright (c) 2025 ProtoCentral Electronics
//
//  This software is licensed under the MIT License (http://opensource.org/licenses/MIT).
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
//  INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
//  PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
//  HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
//  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
//  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
//  For information on how to use, visit https://github.com/Protocentral/protocentral-pulse-express
//
/////////////////////////////////////////////////////////////////////////////////////////

#ifndef _MAX32664_H_
#define _MAX32664_H_

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
enum class Max32664Status : uint8_t
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
struct Max32664Version
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
 * Populated by Max32664::begin() and exposed via Max32664::caps() so user
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
struct Max32664Caps
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
 * Max32664Caps::dateYYYYMMDD; user code passes the same DateTime regardless
 * of firmware version.
 */
struct Max32664DateTime
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
struct Max32664Spo2Coeffs
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
struct Max32664CalibrationRef
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
struct Max32664LegacyCalibrationRefs
{
    uint8_t systolic[3]  = {120, 122, 125};
    uint8_t diastolic[3] = {80, 81, 82};
};

/**
 * @brief BP status code from sample byte 12 (UG6921 Table 4).
 */
enum class Max32664BpStatus : uint8_t
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
struct Max32664Sample
{
    Max32664BpStatus bpStatus       = Max32664BpStatus::NoSignal;
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
struct Max32664RawSample
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
class Max32664
{
public:
    Max32664(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus = Wire);

    /// Optional debug stream (e.g. &Serial). nullptr silences trace output.
    void setDebug(Print *dbg) { _dbg = dbg; }

    /// Cache the wall-clock time the hub will be initialised with on the next
    /// startCalibration()/startEstimation() call. Returns false on out-of-range.
    bool setDateTime(const Max32664DateTime &dt);

    /// Reset the hub, enter application mode, read firmware version, derive caps.
    Max32664Status begin();

    Max32664Version version()      const { return _hubVer; }
    Max32664Version algoVersion()  const { return _algoVer; }
    Max32664Caps    caps()         const { return _caps; }
    uint8_t         mfioPin()      const { return _mfioPin; }

    // ---------------- BPT calibration mode ----------------------------------

    /// Multi-point flow (firmware 40.5.0+). Repeat for calIndex 0..4.
    Max32664Status startCalibration(const Max32664CalibrationRef &ref);

    /// Legacy single-shot flow (firmware <40.5.0).
    Max32664Status startCalibration(const Max32664LegacyCalibrationRefs &refs);

    /// Read one sample if DataRdyInt is set. Returns NoDataAvailable if FIFO empty.
    Max32664Status readSample(Max32664Sample &out);

    /// Read the user calibration vector after BP status==Success / progress==100.
    /// `cap` must be at least caps().calibVectorBytes; `written` returns the
    /// actual byte count (always equal to caps().calibVectorBytes on success).
    Max32664Status readCalibrationVector(uint8_t *out, size_t cap, size_t *written);

    // ---------------- BPT estimation mode -----------------------------------

    /// Multi-point: load the previously-saved vector for one calIndex (0..4).
    Max32664Status loadCalibrationVector(uint8_t calIndex, const uint8_t *vec, size_t len);

    /// Legacy: load the single previously-saved vector.
    Max32664Status loadCalibrationVector(const uint8_t *vec, size_t len);

    Max32664Status startEstimation(const Max32664Spo2Coeffs &coeffs = Max32664Spo2Coeffs{});

    /// Read up to `cap` samples; `*count` returns the number actually written.
    Max32664Status readSamples(Max32664Sample *out, size_t cap, size_t *count);

    // ---------------- Raw PPG mode ------------------------------------------

    Max32664Status startRaw();
    Max32664Status readRaw(Max32664RawSample *out, size_t cap, size_t *count, bool wantRed = true);

    // ---------------- Teardown ----------------------------------------------

    /// Disable AFE, BPT algorithm, and AGC (UG6921 Tables 5 & 6 §3).
    Max32664Status stop();

private:
    struct HubStatus
    {
        bool sensorCommError;
        bool dataReady;
        bool fifoOutOverflow;
        bool fifoInOverflow;
        bool deviceBusy;
    };

    // I2C primitives. Each thin wrapper builds a small command frame on the
    // stack and routes it through writeImpl()/readImpl(), which apply the
    // CMD_DELAY, validate the hub status byte, and retry on 0xFE.
    Max32664Status writeCmd(uint8_t fam, uint8_t idx, uint16_t cmdDelayMs = 0);
    Max32664Status writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint16_t cmdDelayMs = 0);
    Max32664Status writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1, uint16_t cmdDelayMs = 0);
    Max32664Status writeCmd3(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1, uint8_t v2, uint16_t cmdDelayMs = 0);
    Max32664Status writeCmd(uint8_t fam, uint8_t idx, const uint8_t *payload, size_t len, uint16_t cmdDelayMs);
    Max32664Status writeCmd(uint8_t fam, uint8_t idx, uint8_t sub, const uint8_t *payload, size_t len, uint16_t cmdDelayMs);

    Max32664Status readBytes(uint8_t fam, uint8_t idx, uint8_t *out, size_t len, uint16_t cmdDelayMs = 0);
    Max32664Status readBytes(uint8_t fam, uint8_t idx, uint8_t sub, uint8_t *out, size_t len, uint16_t cmdDelayMs = 0);

    // Underlying send-then-status helpers with retry-on-0xFE. `frame` holds
    // the command bytes (fam, idx, optional sub, optional payload) — the
    // caller must build it on the stack before invoking these.
    Max32664Status writeImpl(const uint8_t *frame, size_t frameLen, uint16_t cmdDelayMs);
    Max32664Status readImpl(const uint8_t *frame, size_t frameLen,
                            uint8_t *out, size_t outLen, uint16_t cmdDelayMs);

    // Procedural pieces from UG6921 Tables 2/5/6.
    Max32664Status hardReset();
    Max32664Status enterAppMode();
    Max32664Status readFirmwareVersion(uint8_t indexByte, Max32664Version &out);
    Max32664Caps   capsFor(Max32664Version v) const;

    Max32664Status sendDateTime();
    Max32664Status sendSpo2Coeffs(const Max32664Spo2Coeffs &c);
    Max32664Status sendCalibrationRef(const Max32664CalibrationRef &r);
    Max32664Status sendLegacyCalibrationRefs(const Max32664LegacyCalibrationRefs &r);
    Max32664Status setCalIndex(uint8_t idx);
    Max32664Status sendCalibrationVectorChunked(const uint8_t *vec, size_t len);
    Max32664Status setOutputMode(uint8_t mode);
    Max32664Status setFifoIntrThreshold(uint8_t th);
    Max32664Status enableAfe(bool on);
    Max32664Status enableAgc(bool on);
    Max32664Status enableBpt(uint8_t mode);  // 0=disabled, 1=calib, 2=estimation

    Max32664Status readHubStatus(HubStatus &out);
    Max32664Status readNumFifoSamples(uint8_t &nn);
    Max32664Status readFifoSample(uint8_t *out, size_t len);
    void           parseSample(const uint8_t *buf, Max32664Sample &s) const;

    void trace(const char *msg) const;
    void tracef(const char *fmt, ...) const;

    TwoWire         &_bus;
    Print           *_dbg     = nullptr;
    uint8_t          _resetPin;
    uint8_t          _mfioPin;
    Max32664Version  _hubVer  = {0, 0, 0};
    Max32664Version  _algoVer = {0, 0, 0};
    Max32664Caps     _caps;
    Max32664DateTime _now;
    bool             _began   = false;
};

#endif  // _MAX32664_H_
