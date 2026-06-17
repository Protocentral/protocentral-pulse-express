# Changelog

All notable changes to the ProtoCentral Pulse Express library.

## [2.1.0] - 2026-06-17

### Changed
- **Primary class renamed `Max32664` -> `PulseExpress`**, and the public types
  from `Max32664*` to `PulseExpress*`, to match modern ProtoCentral library
  conventions. The 2.0.x names still compile: `#include "max32664.h"` provides
  `using` aliases. New sketches should `#include "protocentral_pulse_express.h"`.
- Source files renamed to `protocentral_pulse_express.{h,cpp}`; `max32664.h` is
  now a thin backward-compatibility shim.
- License headers switched to the SPDX format across `src/` and `examples/`.
- `library.properties`: `version=2.1.0`, added `includes=`.

### Fixed
- **Earlier-firmware regression:** `begin()` no longer hard-fails (returned
  `UnsupportedFirmware`) when the hub reports a major version other than 40.
  Version checking is now a **soft warning** — `begin()` proceeds with legacy
  capability defaults and exposes `firmwareSupported()` for callers to branch on.

### Added
- `firmwareSupported()` and a public `readStatus(HubStatus&)` for hub diagnostics.
- Examples expanded from 3 to 11 (renumbered, `01.Name` convention):
  `03.HeartRateSpO2`, `04.BPTCalibration`, `05.BPTEstimation`,
  `06.BPTCalibrateAndEstimate`, `07.SaveLoadCalibrationEEPROM`,
  `08.MultiSubjectCalibration`, `09.HeartRateVariability`,
  `10.DeviceInfoAndDiagnostics`, `11.FirmwareFlash`.
- **Bootloader / firmware flashing (factory / recovery):**
  `PulseExpressBootloader` class (`src/pulse_express_bootloader.{h,cpp}`),
  the `11.FirmwareFlash` sketch, and a host script `extras/flash_tool/flash_msbl.py`.
  Implemented from Maxim/ADI UG6806 Table 9 — validate on hardware before
  production. The `.msbl` firmware image is non-redistributable and is never
  committed (`*.msbl`/`*.bin` gitignored).

### Known issues
- `Max32664Caps::sendBpMedication` / `sendRestMode` are derived from the firmware
  version but not yet consumed; calibration on firmware <40.2.2 may be missing
  setup steps. Tracked for a follow-up once the older opcodes are confirmed.

## [2.0.0]

- Clean-break rewrite from 1.0.x: runtime multi-firmware support across the
  MAX32664D 40.x line, raw PPG / BPT calibration / BPT estimation modes.
