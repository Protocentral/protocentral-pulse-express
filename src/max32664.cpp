//////////////////////////////////////////////////////////////////////////////////////////
//
//  Arduino library for the ProtoCentral Pulse Express breakout board
//  (MAX30102 optical sensor + MAX32664D biometric sensor hub).
//
//  See max32664.h for the public API and UG6921 Rev 2 (Maxim/ADI) for the
//  underlying I2C command set.
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

#include "max32664.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/////////////////////////////////////////////////////////////////////////////////////////
// Internal constants
/////////////////////////////////////////////////////////////////////////////////////////

namespace
{

// Per-command CMD_DELAY values from UG6921 Tables 2/5/6. Where a step is not
// explicitly tagged, the doc's default of 2 ms applies.
constexpr uint16_t kDefaultCmdDelayMs       = 2;
constexpr uint16_t kSetDateTimeDelayMs      = 5;
constexpr uint16_t kSetCalIndexDelayMs      = 5;
constexpr uint16_t kSetSpo2CoeffsDelayMs    = 5;
constexpr uint16_t kCalibVectorChunkDelayMs = 30;
constexpr uint16_t kEnableAgcDelayMs        = 20;
constexpr uint16_t kEnableAfeDelayMs        = 40;
constexpr uint16_t kEnableBptDelayMs        = 500;
constexpr uint16_t kDisableAfeDelayMs       = 40;
constexpr uint16_t kDisableBptDelayMs       = 500;
constexpr uint16_t kDisableAgcDelayMs       = 20;
constexpr uint16_t kPostEnableSettleMs      = 100;
constexpr uint16_t kResetSettleMs           = 1000;
constexpr uint16_t kEnterAppModeDelayMs     = 10;
constexpr uint16_t kRawModeSettleMs         = 180;

// I2C chunk size for the long calibration-vector upload. Kept at 30 to match
// the AVR Wire BUFFER_LENGTH of 32 with two bytes of headroom; on platforms
// with larger buffers this still works since each chunk is its own I2C frame.
constexpr uint8_t  kCalibChunkBytes = 30;

// Read-side chunk: at most 30 payload bytes plus the 1-byte status header.
constexpr uint8_t  kReadChunkBytes = 30;

constexpr uint8_t kFifoIntrThreshold      = 0x0F;
constexpr uint8_t kOutputModeAlgo         = 0x03;
constexpr uint8_t kOutputModeSensorOnly   = 0x01;

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

Max32664::Max32664(uint8_t resetPin, uint8_t mfioPin, TwoWire &bus)
    : _bus(bus), _resetPin(resetPin), _mfioPin(mfioPin)
{
}

bool Max32664::setDateTime(const Max32664DateTime &dt)
{
    if (dt.year < 2000 || dt.year > 2099)            return false;
    if (dt.month == 0 || dt.month > 12)              return false;
    if (dt.day == 0 || dt.day > 31)                  return false;
    if (dt.hour > 23 || dt.minute > 59 || dt.second > 59) return false;
    _now = dt;
    return true;
}

Max32664Status Max32664::begin()
{
    Max32664Status s = hardReset();
    if (s != Max32664Status::Ok) return s;

    s = enterAppMode();
    if (s != Max32664Status::Ok) return s;

    s = readFirmwareVersion(0x03, _hubVer);
    if (s != Max32664Status::Ok) return s;
    // Algorithm version is informational; ignore failure.
    readFirmwareVersion(0x07, _algoVer);

    _caps = capsFor(_hubVer);
    tracef("hub %u.%u.%u algo %u.%u.%u  vec=%u sample=%u %s%s",
           _hubVer.major, _hubVer.minor, _hubVer.patch,
           _algoVer.major, _algoVer.minor, _algoVer.patch,
           _caps.calibVectorBytes, _caps.sampleBytes,
           _caps.dateYYYYMMDD ? "YYYYMMDD " : "YYMMDD ",
           _caps.multiPointCalib ? "multi-point" : "single-point");

    // Anything outside the 40.x line is one of the A/B/C variants or a future
    // product — refuse rather than misdrive it.
    if (_hubVer.major != 40) return Max32664Status::UnsupportedFirmware;

    _began = true;
    return Max32664Status::Ok;
}

Max32664Status Max32664::hardReset()
{
    pinMode(_mfioPin,  OUTPUT);
    pinMode(_resetPin, OUTPUT);
    digitalWrite(_mfioPin,  HIGH);   // hold MFIO high to select application mode
    digitalWrite(_resetPin, LOW);
    delay(10);
    digitalWrite(_resetPin, HIGH);
    delay(kResetSettleMs);
    pinMode(_mfioPin, INPUT_PULLUP); // hub now drives MFIO as data-ready intr
    return Max32664Status::Ok;
}

Max32664Status Max32664::enterAppMode()
{
    // 0x01/0x00/0x00 — switch operating mode to application.
    Max32664Status s = writeCmd(0x01, 0x00, uint8_t(0x00), kEnterAppModeDelayMs);
    if (s != Max32664Status::Ok) return s;

    // 0x02/0x00 — read operating mode; expect 0x00 (application).
    uint8_t mode = 0xFF;
    s = readBytes(0x02, 0x00, &mode, 1);
    if (s != Max32664Status::Ok) return s;
    if (mode != 0x00) return Max32664Status::NotInAppMode;
    return Max32664Status::Ok;
}

Max32664Status Max32664::readFirmwareVersion(uint8_t indexByte, Max32664Version &out)
{
    uint8_t buf[3] = {0, 0, 0};
    Max32664Status s = readBytes(0xFF, indexByte, buf, sizeof(buf));
    if (s != Max32664Status::Ok) return s;
    out.major = buf[0];
    out.minor = buf[1];
    out.patch = buf[2];
    return Max32664Status::Ok;
}

Max32664Caps Max32664::capsFor(Max32664Version v) const
{
    Max32664Caps c;  // defaults are legacy values
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
    }
    return c;
}

/////////////////////////////////////////////////////////////////////////////////////////
// BPT calibration mode
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::startCalibration(const Max32664CalibrationRef &ref)
{
    if (!_began)                 return Max32664Status::NotConfigured;
    if (!_caps.multiPointCalib)  return Max32664Status::UnsupportedFirmware;
    if (ref.calIndex > 4)        return Max32664Status::InvalidArgument;

    Max32664Status s = sendDateTime();
    if (s != Max32664Status::Ok) return s;
    s = sendCalibrationRef(ref);
    if (s != Max32664Status::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != Max32664Status::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != Max32664Status::Ok) return s;
    s = enableAgc(true);                           if (s != Max32664Status::Ok) return s;
    s = enableAfe(true);                           if (s != Max32664Status::Ok) return s;
    s = enableBpt(0x01);                           if (s != Max32664Status::Ok) return s;
    delay(kPostEnableSettleMs);
    return Max32664Status::Ok;
}

Max32664Status Max32664::startCalibration(const Max32664LegacyCalibrationRefs &refs)
{
    if (!_began)                 return Max32664Status::NotConfigured;
    // Multi-point firmware rejects the deprecated 0x50/04/01 + 0x50/04/02 path.
    if (_caps.multiPointCalib)   return Max32664Status::UnsupportedFirmware;

    Max32664Status s = sendDateTime();
    if (s != Max32664Status::Ok) return s;
    s = sendLegacyCalibrationRefs(refs);
    if (s != Max32664Status::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != Max32664Status::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != Max32664Status::Ok) return s;
    s = enableAgc(true);                           if (s != Max32664Status::Ok) return s;
    s = enableAfe(true);                           if (s != Max32664Status::Ok) return s;
    s = enableBpt(0x01);                           if (s != Max32664Status::Ok) return s;
    delay(kPostEnableSettleMs);
    return Max32664Status::Ok;
}

Max32664Status Max32664::readSample(Max32664Sample &out)
{
    HubStatus st;
    Max32664Status s = readHubStatus(st);
    if (s != Max32664Status::Ok) return s;
    if (!st.dataReady) return Max32664Status::NoDataAvailable;

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != Max32664Status::Ok) return s;
    if (nn == 0) return Max32664Status::NoDataAvailable;

    uint8_t buf[32];
    if (_caps.sampleBytes > sizeof(buf)) return Max32664Status::BufferTooSmall;
    s = readFifoSample(buf, _caps.sampleBytes);
    if (s != Max32664Status::Ok) return s;
    parseSample(buf, out);
    return Max32664Status::Ok;
}

Max32664Status Max32664::readCalibrationVector(uint8_t *out, size_t cap, size_t *written)
{
    if (!_began)                       return Max32664Status::NotConfigured;
    if (!out)                          return Max32664Status::InvalidArgument;
    if (cap < _caps.calibVectorBytes)  return Max32664Status::BufferTooSmall;

    // 0x51/0x04/0x03 — read user calibration vector.
    Max32664Status s = readBytes(0x51, 0x04, 0x03, out, _caps.calibVectorBytes);
    if (s != Max32664Status::Ok) return s;
    if (written) *written = _caps.calibVectorBytes;
    return Max32664Status::Ok;
}

/////////////////////////////////////////////////////////////////////////////////////////
// BPT estimation mode
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::loadCalibrationVector(uint8_t calIndex, const uint8_t *vec, size_t len)
{
    if (!_began)                                       return Max32664Status::NotConfigured;
    if (!_caps.multiPointCalib)                        return Max32664Status::UnsupportedFirmware;
    if (calIndex > 4 || vec == nullptr || len != _caps.calibVectorBytes)
                                                       return Max32664Status::InvalidArgument;

    Max32664Status s = setCalIndex(calIndex);
    if (s != Max32664Status::Ok) return s;
    return sendCalibrationVectorChunked(vec, len);
}

Max32664Status Max32664::loadCalibrationVector(const uint8_t *vec, size_t len)
{
    if (!_began)                                       return Max32664Status::NotConfigured;
    if (_caps.multiPointCalib)                         return Max32664Status::UnsupportedFirmware;
    if (vec == nullptr || len != _caps.calibVectorBytes)
                                                       return Max32664Status::InvalidArgument;
    return sendCalibrationVectorChunked(vec, len);
}

Max32664Status Max32664::startEstimation(const Max32664Spo2Coeffs &coeffs)
{
    if (!_began) return Max32664Status::NotConfigured;

    Max32664Status s = sendDateTime();
    if (s != Max32664Status::Ok) return s;
    s = sendSpo2Coeffs(coeffs);
    if (s != Max32664Status::Ok) return s;

    s = setOutputMode(kOutputModeAlgo);            if (s != Max32664Status::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != Max32664Status::Ok) return s;
    s = enableAgc(true);                           if (s != Max32664Status::Ok) return s;
    s = enableAfe(true);                           if (s != Max32664Status::Ok) return s;
    s = enableBpt(0x02);                           if (s != Max32664Status::Ok) return s;
    delay(kPostEnableSettleMs);
    return Max32664Status::Ok;
}

Max32664Status Max32664::readSamples(Max32664Sample *out, size_t cap, size_t *count)
{
    if (count) *count = 0;
    if (!_began || out == nullptr) return Max32664Status::InvalidArgument;

    HubStatus st;
    Max32664Status s = readHubStatus(st);
    if (s != Max32664Status::Ok) return s;
    if (!st.dataReady) return Max32664Status::Ok;  // empty FIFO is not an error

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != Max32664Status::Ok) return s;
    if (nn == 0) return Max32664Status::Ok;
    if (nn > cap) nn = uint8_t(cap);

    uint8_t buf[32];
    if (_caps.sampleBytes > sizeof(buf)) return Max32664Status::BufferTooSmall;

    for (uint8_t i = 0; i < nn; ++i)
    {
        s = readFifoSample(buf, _caps.sampleBytes);
        if (s != Max32664Status::Ok) return s;
        parseSample(buf, out[i]);
    }
    if (count) *count = nn;
    return Max32664Status::Ok;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Raw PPG mode
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::startRaw()
{
    if (!_began) return Max32664Status::NotConfigured;

    // Per UG6921 Table 6: output mode = sensor-only, then enable AFE, then
    // enable BPT in estimation mode (the algorithm runs but does not affect
    // PPG), then disable AGC so LED currents stay where the user puts them.
    Max32664Status s = setOutputMode(kOutputModeSensorOnly);
    if (s != Max32664Status::Ok) return s;
    s = setFifoIntrThreshold(kFifoIntrThreshold);  if (s != Max32664Status::Ok) return s;
    s = enableAfe(true);                           if (s != Max32664Status::Ok) return s;
    s = enableBpt(0x02);                           if (s != Max32664Status::Ok) return s;
    s = enableAgc(false);                          if (s != Max32664Status::Ok) return s;
    delay(kRawModeSettleMs);

    // LED1 (red) and LED2 (IR) currents to half-scale. Must come AFTER the
    // algorithm enable or the values are overwritten during init.
    s = writeCmd3(0x40, 0x03, kMax30101Led1RegAddr, kMax30101LedHalfScale, 0,
                  kDefaultCmdDelayMs);
    if (s != Max32664Status::Ok) return s;
    return writeCmd3(0x40, 0x03, kMax30101Led2RegAddr, kMax30101LedHalfScale, 0,
                     kDefaultCmdDelayMs);
}

Max32664Status Max32664::readRaw(Max32664RawSample *out, size_t cap, size_t *count, bool wantRed)
{
    if (count) *count = 0;
    if (!_began || out == nullptr) return Max32664Status::InvalidArgument;

    HubStatus st;
    Max32664Status s = readHubStatus(st);
    if (s != Max32664Status::Ok) return s;
    if (!st.dataReady) return Max32664Status::Ok;

    uint8_t nn = 0;
    s = readNumFifoSamples(nn);
    if (s != Max32664Status::Ok) return s;
    if (nn == 0) return Max32664Status::Ok;
    if (nn > cap) nn = uint8_t(cap);

    // Per UG6921 Table 4 a sensor-only sample is 12 bytes (4 LEDs * 3 bytes).
    // Only LED1 (IR, bytes 0-2) and LED2 (Red, bytes 3-5) are wired up on the
    // MAX30101 in this product; LED3/4 are reported as zero.
    uint8_t buf[16];
    for (uint8_t i = 0; i < nn; ++i)
    {
        s = readFifoSample(buf, 12);
        if (s != Max32664Status::Ok) return s;
        out[i].ir  = (uint32_t(buf[0]) << 16) | (uint32_t(buf[1]) << 8) | uint32_t(buf[2]);
        out[i].red = wantRed
                       ? ((uint32_t(buf[3]) << 16) | (uint32_t(buf[4]) << 8) | uint32_t(buf[5]))
                       : 0u;
    }
    if (count) *count = nn;
    return Max32664Status::Ok;
}

Max32664Status Max32664::stop()
{
    if (!_began) return Max32664Status::Ok;
    // Disable order from UG6921 Tables 5 & 6 §3.
    Max32664Status s1 = enableAfe(false);
    Max32664Status s2 = enableBpt(0x00);
    Max32664Status s3 = enableAgc(false);
    if (s1 != Max32664Status::Ok) return s1;
    if (s2 != Max32664Status::Ok) return s2;
    return s3;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Sub-procedures (private)
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::sendDateTime()
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

Max32664Status Max32664::sendSpo2Coeffs(const Max32664Spo2Coeffs &c)
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

Max32664Status Max32664::sendCalibrationRef(const Max32664CalibrationRef &r)
{
    // 0x50/0x04/0x07 — set cal_index + reference systolic + reference
    // diastolic (introduced in 40.5.0+).
    uint8_t payload[3] = {r.calIndex, r.systolic, r.diastolic};
    return writeCmd(0x50, 0x04, 0x07, payload, sizeof(payload), kSetCalIndexDelayMs);
}

Max32664Status Max32664::sendLegacyCalibrationRefs(const Max32664LegacyCalibrationRefs &r)
{
    // 0x50/0x04/0x01 — three systolic refs.
    Max32664Status s = writeCmd(0x50, 0x04, 0x01, r.systolic, 3, kDefaultCmdDelayMs);
    if (s != Max32664Status::Ok) return s;
    // 0x50/0x04/0x02 — three diastolic refs.
    return writeCmd(0x50, 0x04, 0x02, r.diastolic, 3, kDefaultCmdDelayMs);
}

Max32664Status Max32664::setCalIndex(uint8_t idx)
{
    if (idx > 4) return Max32664Status::InvalidArgument;
    // 0x50/0x04/0x08 — select cal_index for subsequent vector load (40.5.0+).
    uint8_t payload[1] = {idx};
    return writeCmd(0x50, 0x04, 0x08, payload, 1, kSetCalIndexDelayMs);
}

Max32664Status Max32664::sendCalibrationVectorChunked(const uint8_t *vec, size_t len)
{
    if (vec == nullptr || len == 0) return Max32664Status::InvalidArgument;

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
        if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
        delay(kCalibVectorChunkDelayMs);
    }

    // Final status read confirms the upload was accepted by the hub.
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t status = uint8_t(_bus.read());
    if (status != 0x00) return Max32664Status(status);
    return Max32664Status::Ok;
}

Max32664Status Max32664::setOutputMode(uint8_t mode)
{
    return writeCmd(0x10, 0x00, mode, kDefaultCmdDelayMs);
}

Max32664Status Max32664::setFifoIntrThreshold(uint8_t th)
{
    return writeCmd(0x10, 0x01, th, kDefaultCmdDelayMs);
}

Max32664Status Max32664::enableAfe(bool on)
{
    return writeCmd(0x44, 0x03, uint8_t(on ? 0x01 : 0x00),
                    on ? kEnableAfeDelayMs : kDisableAfeDelayMs);
}

Max32664Status Max32664::enableAgc(bool on)
{
    return writeCmd(0x52, 0x00, uint8_t(on ? 0x01 : 0x00),
                    on ? kEnableAgcDelayMs : kDisableAgcDelayMs);
}

Max32664Status Max32664::enableBpt(uint8_t mode)
{
    // mode: 0=disabled, 1=calibration, 2=estimation.
    if (mode > 2) return Max32664Status::InvalidArgument;
    return writeCmd(0x52, 0x04, mode,
                    mode == 0 ? kDisableBptDelayMs : kEnableBptDelayMs);
}

/////////////////////////////////////////////////////////////////////////////////////////
// FIFO / sample I/O
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::readHubStatus(HubStatus &out)
{
    uint8_t bits = 0;
    Max32664Status s = readBytes(0x00, 0x00, &bits, 1);
    if (s != Max32664Status::Ok) return s;
    out.sensorCommError = (bits & MAX32664_STATUS_SENSOR_COMM)  != 0;
    out.dataReady       = (bits & MAX32664_STATUS_DATA_READY)   != 0;
    out.fifoOutOverflow = (bits & MAX32664_STATUS_FIFO_OUT_OVR) != 0;
    out.fifoInOverflow  = (bits & MAX32664_STATUS_FIFO_IN_OVR)  != 0;
    out.deviceBusy      = (bits & MAX32664_STATUS_DEVICE_BUSY)  != 0;
    return Max32664Status::Ok;
}

Max32664Status Max32664::readNumFifoSamples(uint8_t &nn)
{
    return readBytes(0x12, 0x00, &nn, 1);
}

Max32664Status Max32664::readFifoSample(uint8_t *out, size_t len)
{
    return readBytes(0x12, 0x01, out, len);
}

void Max32664::parseSample(const uint8_t *buf, Max32664Sample &s) const
{
    // Layout per UG6921 Table 4. Bytes 0..11 are MAX30101 PPG counters in
    // algorithm mode; byte 12 onward is the BPT block.
    s.bpStatus     = Max32664BpStatus(buf[12]);
    s.progress     = buf[13];
    s.heartRate10x = pack16BE(&buf[14]);
    s.systolic     = buf[16];
    s.diastolic    = buf[17];
    s.spo210x      = pack16BE(&buf[18]);
    s.rValue1000x  = pack16BE(&buf[20]);
    s.pulseFlag    = buf[22];

    // 40.5.0+ extension fields. Zeroed when caps reports a smaller sample
    // size so user code can rely on a consistent struct layout.
    s.ibiMs          = (_caps.sampleBytes >= 25) ? pack16BE(&buf[23]) : 0;
    s.spo2Confidence = (_caps.sampleBytes >= 26) ? buf[25] : 0;
    s.bptReportFlag  = (_caps.sampleBytes >= 27) ? buf[26] : 0;
    s.spo2ReportFlag = (_caps.sampleBytes >= 28) ? buf[27] : 0;
}

/////////////////////////////////////////////////////////////////////////////////////////
// Low-level I2C primitives
/////////////////////////////////////////////////////////////////////////////////////////

Max32664Status Max32664::writeCmd(uint8_t fam, uint8_t idx, uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

Max32664Status Max32664::writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    _bus.write(v0);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

Max32664Status Max32664::writeCmd(uint8_t fam, uint8_t idx, uint8_t v0, uint8_t v1,
                                  uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    _bus.write(v0);
    _bus.write(v1);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

Max32664Status Max32664::writeCmd3(uint8_t fam, uint8_t idx,
                                   uint8_t v0, uint8_t v1, uint8_t v2,
                                   uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    _bus.write(v0);
    _bus.write(v1);
    _bus.write(v2);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

Max32664Status Max32664::writeCmd(uint8_t fam, uint8_t idx,
                                  const uint8_t *payload, size_t len,
                                  uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    for (size_t i = 0; i < len; ++i) _bus.write(payload[i]);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

Max32664Status Max32664::writeCmd(uint8_t fam, uint8_t idx, uint8_t sub,
                                  const uint8_t *payload, size_t len,
                                  uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    _bus.write(sub);
    for (size_t i = 0; i < len; ++i) _bus.write(payload[i]);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    if (_bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(1)) != 1)
        return Max32664Status::HostCommError;
    uint8_t st = uint8_t(_bus.read());
    return st == 0x00 ? Max32664Status::Ok : Max32664Status(st);
}

// Internal helper for the read primitives below: pulls (1 + len) bytes off
// the bus across as many requestFrom() calls as the platform's Wire buffer
// requires, validates the leading status byte, and writes the payload bytes
// to `out`.
namespace
{
Max32664Status drainResponse(TwoWire &bus, uint8_t *out, size_t len)
{
    size_t   remaining = len;
    uint8_t *cursor    = out;
    bool     firstChunk = true;

    while (remaining > 0 || firstChunk)
    {
        uint16_t want = uint16_t((firstChunk ? 1u : 0u) + remaining);
        if (want > kReadChunkBytes) want = kReadChunkBytes;
        uint8_t got = bus.requestFrom(uint8_t(MAX32664_I2C_ADDR), uint8_t(want));
        if (got != want) return Max32664Status::HostCommError;

        if (firstChunk)
        {
            uint8_t st = uint8_t(bus.read());
            --got;
            firstChunk = false;
            if (st != 0x00)
            {
                while (got--) bus.read();
                return Max32664Status(st);
            }
        }
        while (got-- && remaining > 0)
        {
            *cursor++ = uint8_t(bus.read());
            --remaining;
        }
        if (remaining == 0) break;
    }
    return Max32664Status::Ok;
}
}  // namespace

Max32664Status Max32664::readBytes(uint8_t fam, uint8_t idx,
                                   uint8_t *out, size_t len, uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    return drainResponse(_bus, out, len);
}

Max32664Status Max32664::readBytes(uint8_t fam, uint8_t idx, uint8_t sub,
                                   uint8_t *out, size_t len, uint16_t cmdDelayMs)
{
    _bus.beginTransmission(MAX32664_I2C_ADDR);
    _bus.write(fam);
    _bus.write(idx);
    _bus.write(sub);
    if (_bus.endTransmission() != 0) return Max32664Status::HostCommError;
    delay(cmdDelayMs ? cmdDelayMs : kDefaultCmdDelayMs);
    return drainResponse(_bus, out, len);
}

/////////////////////////////////////////////////////////////////////////////////////////
// Tracing
/////////////////////////////////////////////////////////////////////////////////////////

void Max32664::trace(const char *msg) const
{
    if (_dbg) _dbg->println(msg);
}

void Max32664::tracef(const char *fmt, ...) const
{
    if (!_dbg) return;
    char buf[128];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    _dbg->println(buf);
}
