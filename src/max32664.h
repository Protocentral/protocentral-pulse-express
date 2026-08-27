// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics

/*
 * Backward-compatibility shim for ProtoCentral Pulse Express library 2.0.x.
 *
 * The 2.0.x API named the driver class `Max32664` and prefixed its types
 * `Max32664*`. As of 2.1.0 the canonical names are `PulseExpress` /
 * `PulseExpress*` (see protocentral_pulse_express.h). This header keeps the old
 * spelling compiling via aliases. New sketches should include
 * "protocentral_pulse_express.h" directly.
 *
 * DEPRECATED: prefer #include "protocentral_pulse_express.h".
 */

#ifndef _MAX32664_H_
#define _MAX32664_H_

#include "protocentral_pulse_express.h"

using Max32664                     = PulseExpress;
using Max32664Status               = PulseExpressStatus;
using Max32664Version              = PulseExpressVersion;
using Max32664Caps                 = PulseExpressCaps;
using Max32664DateTime             = PulseExpressDateTime;
using Max32664Spo2Coeffs           = PulseExpressSpo2Coeffs;
using Max32664CalibrationRef       = PulseExpressCalibrationRef;
using Max32664LegacyCalibrationRefs = PulseExpressLegacyCalibrationRefs;
using Max32664BpStatus             = PulseExpressBpStatus;
using Max32664Sample               = PulseExpressSample;
using Max32664RawSample            = PulseExpressRawSample;

#endif  // _MAX32664_H_
