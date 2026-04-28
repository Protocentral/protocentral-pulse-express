//////////////////////////////////////////////////////////////////////////////////////////
//
//  Pulse Express — Blood Pressure Trending (BPT) calibration + estimation
//
//  Walks the full flow described in UG6921:
//    1. begin()                       -> reset hub, read firmware version
//    2. startCalibration()            -> upload reference cuff readings
//    3. readSample() polled for ~1 min while finger is on the sensor
//    4. readCalibrationVector()       -> persist the user-specific vector
//    5. loadCalibrationVector()       -> reload it into the hub
//    6. startEstimation()             -> begin live BPT/SpO2/HR readings
//    7. readSamples() in a loop       -> stream results over Serial
//
//  For firmware >= 40.5.0 the hub supports a multi-point calibration with up
//  to five subjects (calIndex 0..4). For brevity this example calibrates a
//  single subject at calIndex 0 only; production code would loop 0..4 with a
//  reference cuff measurement per subject and persist each vector in
//  non-volatile storage. The driver reports the firmware-derived behaviour
//  via hub.caps() — branch on caps().multiPointCalib if you support both.
//
//  WARNING: the SpO2 and reference BP values below are placeholders. For
//  meaningful readings you must:
//    - calibrate the SpO2 polynomial (a, b, c) against a reference oximeter
//      per Maxim AN6845.
//    - measure each subject's actual systolic / diastolic BP with a clinically
//      validated cuff at the moment of calibration.
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

// Reference cuff measurements taken with a clinically validated device at the
// moment of calibration. REPLACE WITH YOUR OWN VALUES.
#define REF_SYSTOLIC   120
#define REF_DIASTOLIC   80

// SpO2 calibration polynomial. REPLACE WITH YOUR OWN VALUES (Maxim AN6845).
#define SPO2_COEFF_A    1.5958422f
#define SPO2_COEFF_B  (-34.659664f)
#define SPO2_COEFF_C  112.68987f

// Calibration vector storage. The driver reports the size at runtime: 512
// bytes on firmware >= 40.5.0, 824 bytes on legacy. We allocate the larger
// of the two so the same sketch fits both. On AVR boards with 2 KB SRAM this
// uses ~40% of available memory.
#define CALIB_VECTOR_MAX  824
static uint8_t calibVector[CALIB_VECTOR_MAX];

Max32664 hub(RESET_PIN, MFIO_PIN);

static void haltWithError(const char *step, Max32664Status s)
{
    while (true)
    {
        Serial.print(step);
        Serial.print(" failed: 0x");
        Serial.println(uint8_t(s), HEX);
        delay(5000);
    }
}

static Max32664Status runCalibration()
{
    Max32664Status s;
    if (hub.caps().multiPointCalib)
    {
        Max32664CalibrationRef ref;
        ref.calIndex  = 0;
        ref.systolic  = REF_SYSTOLIC;
        ref.diastolic = REF_DIASTOLIC;
        s = hub.startCalibration(ref);
    }
    else
    {
        Max32664LegacyCalibrationRefs refs;
        refs.systolic[0]  = REF_SYSTOLIC;
        refs.systolic[1]  = REF_SYSTOLIC + 2;
        refs.systolic[2]  = REF_SYSTOLIC + 5;
        refs.diastolic[0] = REF_DIASTOLIC;
        refs.diastolic[1] = REF_DIASTOLIC + 1;
        refs.diastolic[2] = REF_DIASTOLIC + 2;
        s = hub.startCalibration(refs);
    }
    if (s != Max32664Status::Ok) return s;

    Serial.println("Place your finger on the sensor — hold still until 100%.");

    Max32664Sample sample;
    unsigned long startMs = millis();
    while (true)
    {
        Max32664Status r = hub.readSample(sample);
        if (r == Max32664Status::Ok)
        {
            Serial.print("progress: ");
            Serial.print(sample.progress);
            Serial.print("%  status: ");
            Serial.println(uint8_t(sample.bpStatus));

            if (sample.bpStatus == Max32664BpStatus::Success && sample.progress >= 100) break;
            if (sample.bpStatus == Max32664BpStatus::EstimationFailure ||
                sample.bpStatus == Max32664BpStatus::SubjectInitFailure ||
                sample.bpStatus == Max32664BpStatus::TooManyCalibrations)
            {
                return Max32664Status::UnknownHubError;
            }
        }
        if (millis() - startMs > 120000UL) return Max32664Status::Timeout;
        delay(40);
    }
    return Max32664Status::Ok;
}

void setup()
{
    Serial.begin(57600);
    while (!Serial && millis() < 3000) {}
    Wire.begin();

    Max32664Status s = hub.begin();
    if (s != Max32664Status::Ok) haltWithError("hub.begin()", s);

    Serial.print("Hub firmware ");
    Serial.print(hub.version().major); Serial.print('.');
    Serial.print(hub.version().minor); Serial.print('.');
    Serial.println(hub.version().patch);
    Serial.print("Calibration vector size: ");
    Serial.println(hub.caps().calibVectorBytes);

    s = runCalibration();
    if (s != Max32664Status::Ok) haltWithError("calibration", s);
    Serial.println("Calibration complete; reading vector.");

    size_t calibLen = 0;
    s = hub.readCalibrationVector(calibVector, sizeof(calibVector), &calibLen);
    if (s != Max32664Status::Ok) haltWithError("readCalibrationVector", s);
    Serial.print("Stored "); Serial.print(calibLen); Serial.println(" calibration bytes.");

    s = hub.stop();  // tear down calibration mode before re-enabling for estimation
    if (s != Max32664Status::Ok) haltWithError("hub.stop()", s);

    if (hub.caps().multiPointCalib)
        s = hub.loadCalibrationVector(/*calIndex=*/0, calibVector, calibLen);
    else
        s = hub.loadCalibrationVector(calibVector, calibLen);
    if (s != Max32664Status::Ok) haltWithError("loadCalibrationVector", s);

    Max32664Spo2Coeffs coeffs;
    coeffs.a = SPO2_COEFF_A;
    coeffs.b = SPO2_COEFF_B;
    coeffs.c = SPO2_COEFF_C;
    s = hub.startEstimation(coeffs);
    if (s != Max32664Status::Ok) haltWithError("startEstimation", s);

    Serial.println("Estimation running.");
    delay(1000);
}

void loop()
{
    Max32664Sample samples[8];
    size_t n = 0;
    Max32664Status s = hub.readSamples(samples, 8, &n);
    if (s != Max32664Status::Ok) return;

    for (size_t i = 0; i < n; ++i)
    {
        Serial.print("sys=");   Serial.print(samples[i].systolic);
        Serial.print(" dia=");  Serial.print(samples[i].diastolic);
        Serial.print(" hr=");   Serial.print(samples[i].heartRate(), 1);
        Serial.print(" spo2="); Serial.print(samples[i].spo2(), 1);
        Serial.print(" status=");
        Serial.println(uint8_t(samples[i].bpStatus));
    }
    delay(100);
}
