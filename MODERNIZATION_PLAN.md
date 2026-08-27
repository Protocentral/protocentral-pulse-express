# Pulse Express — Modernisation Plan

Plan to bring the `protocentral-pulse-express` library in line with current
ProtoCentral Arduino library conventions, and to expand the examples so they
exercise the full capability set of the MAX32664D biometric sensor hub.

Reference library (newest, the canonical template): `protocentral_max30001_arduino_library`
(Nov 2025). Secondary reference for the dotted example style: `protocentral_st1vafe3bx_arduino`.

**Locked decisions**
- Public class name: **`PulseExpress`** (board-centric, breaking rename from `Max32664`).
- Example naming: **`01.Name/01.Name.ino`** (dotted, matches ST1VAFE3BX and the
  current sketches — lowest churn).
- Deliverable: this written plan only — no code changes until approved.

---

## 1. What the MAX32664D can actually do (capability inventory)

From UG6921 (bundled in `docs/`) and the existing `src/max32664.h`. The hub owns
the MAX30101 optical sensor over a private I2C bus; the host only ever talks to the
hub at `0x55`. Everything below is already supported by the driver internals — the
gap is that the **examples only surface 3 of these flows**.

| Capability | Hub output / mechanism | Surfaced today? |
|---|---|---|
| Raw PPG (IR + Red, 24-bit ADC counts) | Raw Data Collection mode | ✅ (IR only) |
| Heart rate (10× bpm) | Algorithm mode sample | ⚠️ only inside BPT estimation |
| SpO2 (10× %) + confidence + report flag | Algorithm mode sample | ⚠️ only inside BPT estimation |
| R-value (1000×) — used to calibrate SpO2 | Algorithm mode sample | ❌ |
| SpO2 coefficient calibration (a, b, c per AN6845) | `startEstimation(coeffs)` | ❌ (defaults only) |
| Systolic / diastolic BP (mmHg) | Algorithm mode sample | ✅ |
| BPT calibration → user calibration vector | `startCalibration` → `readCalibrationVector` | ✅ (not persisted) |
| Vector persistence + reload | `loadCalibrationVector` | ⚠️ in-RAM only, no NVM example |
| Multi-subject calibration (calIndex 0..4, FW ≥40.5.0) | `startCalibration(CalibrationRef)` | ❌ |
| Legacy single-shot calibration (FW <40.5.0) | `startCalibration(LegacyCalibrationRefs)` | ❌ |
| Inter-beat interval (IBI, ms, FW ≥40.5.0) → HRV | Algorithm mode sample | ❌ |
| BP status codes (24 states, Table 4) | `Max32664BpStatus` | ⚠️ minimal |
| Firmware/algo version + derived capability flags | `version()`, `algoVersion()`, `caps()` | ⚠️ printed, not explained |
| Hub status / FIFO overflow diagnostics | internal `HubStatus` | ❌ not exposed publicly |
| MFIO interrupt-driven reads vs polling | `mfioPin()` exposed | ❌ polling only |

**Implication:** the driver is already capable; modernisation is mostly
**packaging + examples**, not a rewrite.

---

## 2. Conventions to adopt (gap analysis vs. current state)

| Area | Current | Modern convention (target) |
|---|---|---|
| Class name | `Max32664` | `PulseExpress` |
| Main header | `src/max32664.h` | `src/protocentral_pulse_express.h` (+ keep thin `max32664.h` shim?) |
| License header | banner `//////` block | SPDX two-line header + MIT block (MAX30001 style) |
| `library.properties` | present, OK | add `includes=`, align `name=`, bump to keep 2.0.0 / 2.1.0 |
| `library.json` (PlatformIO) | none | none (siblings omit it) — leave out |
| `keywords.txt` | present, stale | regenerate for new class + all public symbols |
| README | board-focused, license-heavy | badges → buy → overview/key-caps → install → hardware → quick start → API → examples → license |
| Badges | Compile Examples only | + License: MIT, + Arduino Library |
| CI workflows | `compile-examples.yml`, `main.yml` (lint) | keep both; confirm `main.yml` = arduino-lint |
| `assets/` | has board photo + gif | keep; ensure README references resolve |
| `docs/` | 5 datasheets/PDFs | keep (sibling ADS1293 also ships a `docs/`) |
| Examples | 3 sketches | renumber + expand to ~9 (section 4) |
| `build-uno-r4.sh` | present, good | rename mentally as the "compile_all" equivalent; siblings call it `compile_all_examples.sh` — optionally add that as the canonical name |

### 2.1 Class rename (`Max32664` → `PulseExpress`)

Breaking change; bump `version` to **2.1.0** (still a 2.x clean-break line, README
already warns API is not 1.x-compatible). To keep churn low and avoid breaking the
existing 2.0.0 sketches in one shot:

- Rename the class and all `Max32664*` free types? **No** — keep the `Max32664Status`,
  `Max32664Sample`, etc. type names OR rename them too. Recommend renaming the class
  only and **type-aliasing** the struct/enum names to `PulseExpress…` (e.g.
  `using PulseExpressStatus = Max32664Status;`) so both spellings compile. Decide at
  implementation time; default: rename class, alias the rest, document the canonical
  names.
- Provide `src/protocentral_pulse_express.h` as the new primary include. Keep
  `src/max32664.h` as a one-line `#include "protocentral_pulse_express.h"` shim +
  `typedef PulseExpress Max32664;` for backward compatibility, marked deprecated.
- Update `library.properties` `includes=protocentral_pulse_express.h`.

### 2.2 License headers

Replace the banner block at the top of `src/*.{h,cpp}` and every `examples/**/*.ino`
with the MAX30001-style header:

```cpp
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
// SPDX-FileCopyrightText: Copyright (c) 2020 Maxim Integrated / Analog Devices (protocol)

/*
 * ProtoCentral Pulse Express — MAX30101 + MAX32664D Biometric Sensor Hub
 *
 * Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
 * ... full MIT text ...
 */
```

Preserve the original-author attribution ("Original 2020 driver: Joice Tm").

### 2.3 README rewrite (section order)

1. `# Protocentral Pulse Express ...` title
2. Badges: Compile Examples, License: MIT, Arduino Library
3. `## Don't have one? [Buy it here](...)` + board photo
4. `## Overview` + **Key Capabilities** bullet list (mirror section 1 table)
5. `## Important medical disclaimer` (R&D only — keep prominent)
6. `## Installation` (Library Manager + manual ZIP)
7. `## Hardware Setup` — pin table (already in README; extend with ESP32 column)
8. `## Quick Start` — minimal HR/SpO2 sketch
9. `## Firmware versions & capabilities` — explain the 40.x breakpoints + `caps()`;
   state the board **ships pre-flashed**, list the minimum/known-good version, and how
   `begin()`/`version()` report it. **No `.msbl` link.** One line: re-flashing is a
   factory/recovery operation (see `extras/`), image not redistributed.
10. `## Examples` — table mapping each sketch to the capability it demos
11. `## API Reference` — constructor, `begin()`, the three modes, status enum
12. `## License` block (current detailed block, trimmed)

---

## 3. Driver-level additions needed to support the new examples

Small, additive API surface (no behaviour change to existing methods):

1. **Public hub status accessor** — promote the private `HubStatus` read to a public
   `Max32664Status readStatus(...)` so a diagnostics example can show FIFO
   overflow / data-ready / busy. (Needed by example 08.)
2. **HRV helper is example-side** — compute SDNN/RMSSD from the existing `ibiMs`
   field in the sketch; no driver change. (Example 06.)
3. **Confirm `rValue()` and confidence fields** are populated in estimation samples
   (already in `Max32664Sample`). No change; just used by examples 03/05.
4. **Optional:** a convenience `begin(TwoWire&, sda, scl)` overload for ESP32 boards
   with remappable I2C, to make the multi-board story clean. Low priority.

If keeping the change set minimal, only item (1) touches the driver; the rest is
examples + docs.

---

## 4. Proposed example set (renumbered, `01.Name` convention)

Goal: one focused sketch per capability, ordered simplest → most advanced. Existing
sketches are renumbered/retitled; ✚ = new.

| # | Folder | Demonstrates | Status |
|---|---|---|---|
| 01 | `01.RawPPGStreamPlotter` | Raw IR PPG → Arduino Serial Plotter | keep |
| 02 | `02.RawPPGStreamOpenView` | Raw IR/Red → ProtoCentral OpenView GUI | keep (was 03) |
| 03 | `03.HeartRateSpO2` | ✚ Algorithm mode: live HR + SpO2 + confidence, **no BP** — the simplest "vitals" demo | new |
| 04 | `04.BPTCalibration` | ✚ Run calibration only; dump the user calibration vector (hex) to Serial | new |
| 05 | `05.BPTEstimation` | Load a saved vector + stream BP/HR/SpO2 (the current "estimation" half) | refactor from current 02 |
| 06 | `06.BPTCalibrateAndEstimate` | Full end-to-end cal→estimate in one sketch (current 02 behaviour) | keep/rename |
| 07 | `07.SaveLoadCalibrationEEPROM` | ✚ Persist the vector to EEPROM/Preferences (size from `caps().calibVectorBytes`), reload on boot, then estimate | new |
| 08 | `08.MultiSubjectCalibration` | ✚ FW ≥40.5.0 multi-point calibration, calIndex 0..4, one vector per subject; branch on `caps().multiPointCalib` | new |
| 09 | `09.HeartRateVariability` | ✚ Derive SDNN/RMSSD from the IBI field (FW ≥40.5.0) | new |
| 10 | `10.DeviceInfoAndDiagnostics` | ✚ Print hub + algo firmware version, decoded `caps()`, and live hub status (FIFO overflow / busy) | new |

Factory / advanced (bootloader — see §5):
- `11.FirmwareFlash` — ✚ bootloader-mode flasher; streams a **user-supplied** `.msbl`
  over Serial (PC script) or reads it from SD. Documented as factory/recovery; image
  never shipped. Serial-stream variant uses no extra library so it stays in the CI matrix.

Optional / stretch (only if worthwhile):
- `12.SpO2Calibration` — upload custom SpO2 (a,b,c) coefficients and compare against
  defaults using the R-value output (AN6845 workflow).
- `13.InterruptDriven` — use the MFIO pin as a data-ready interrupt instead of polling.

Recommend shipping **01–11**; treat 12–13 as fast-follows. Every new `.ino` gets the
SPDX header, the standard pin table comment block, and a top-of-file WARNING about
placeholder BP/SpO2 reference values (matching the current 02 example's tone).

**CI impact:** each new sketch is auto-picked up by `compile-examples.yml`
(`sketch-paths: - examples`) and `build-uno-r4.sh`. Examples that need EEPROM
(`07`) must guard for cores without `<EEPROM.h>` (ESP32 uses `Preferences`/emulated
EEPROM) — use `#if defined(...)` so the CI matrix keeps compiling across all ~30 boards.

---

## 5. Bootloader & firmware flashing (factory / recovery)

The hub ships **pre-flashed at the ProtoCentral factory**; end users never need the
`.msbl`. The flashing code nonetheless lives **in this library** as a factory/recovery
tool. The firmware *image* is treated as non-redistributable IP and is **never
committed** — the flasher consumes a user-supplied file.

### 5.1 Why this is a separate protocol

Everything the driver does today is **application mode**. Flashing is **bootloader
mode** — a distinct command set (reference: `docs/user-guide-6806-max32664.pdf`):

- **Entry:** hold **MFIO low** while releasing **RSTN** (application entry holds MFIO
  high). After settle, read operating mode (`0x02 0x00`) → expect `0x08` (bootloader).
- **Flash sequence (Family `0x80`):** set page count (`0x80 0x02`), upload init-vector
  + auth bytes parsed from the `.msbl` header (`0x80 0x00` / `0x80 0x01`), erase app
  (`0x80 0x03`), then stream each **8192-byte page + 16 check bytes** (`0x80 0x04`)
  with a long per-page `CMD_DELAY` (~340 ms). Bootloader status codes `0x80`–`0x83`
  (checksum/auth/invalid-app) surface here — distinct from the app-mode codes already
  in `Max32664Status`.
- **Exit:** reset with MFIO high → re-enters application mode; then the normal
  `begin()` path reads back the new version.

### 5.2 Image transport (the .msbl is too big to embed)

A typical `.msbl` is **>100 KB** — it cannot be a `const` array on an Uno. Two
supported transports, both keeping the file off-device-flash:

- **Serial-stream (primary):** a host PC script (Python) parses the `.msbl` and streams
  pages over USB-Serial to the flasher sketch, which writes them to the hub. **No extra
  Arduino library → compiles on the full CI matrix.** Recommended default.
- **SD card (optional):** sketch reads `firmware.msbl` from SD. Needs `<SD.h>`; guard
  with `#if __has_include(<SD.h>)` so CI keeps building on cores without it.

### 5.3 API additions (bootloader mode)

New, clearly separated from the app-mode API (own header section or a small
`PulseExpressBootloader` helper class to avoid bloating the hot-path driver):

- `Max32664Status enterBootloader()` / `exitBootloaderToApp()`
- `Max32664Status readBootloaderInfo(...)` — page size, mode confirm, BL version
- `Max32664Status beginFlash(uint16_t numPages, const uint8_t iv[11], const uint8_t auth[16])`
- `Max32664Status flashPage(const uint8_t *page8208)` — 8192 + 16 check bytes
- `Max32664Status finishFlash()`
- Reuse the existing `writeImpl()` retry-on-`0xFE` layer; add bootloader status decoding.

The `.msbl` **header parsing** (page count, IV, auth, page size) lives in the host
Python script and/or the example sketch — not baked into the driver.

### 5.4 Version-check on every boot (app side)

Independent of flashing: `begin()` already reads the version. Add a documented
**minimum/known-good firmware constant** and have examples warn (not hard-fail) when
the hub reports older firmware — this is the user-facing substitute for shipping a
`.msbl`.

### 5.5 Repo hygiene (do this regardless)

- `.gitignore`: add `*.msbl` and `*.bin` (neither is currently excluded — the image
  could be committed today by accident).
- No `.msbl` in the repo, README, or releases. The flasher docs say "supply your own
  image (factory-internal)"; the consumer README says only "ships pre-flashed."
- Add a `extras/firmware/README.md` placeholder explaining where the operator drops
  their local (gitignored) `.msbl`, without including one.

---

## 6. File-by-file change list

```
src/protocentral_pulse_express.h   NEW  primary header; class PulseExpress + types
src/protocentral_pulse_express.cpp NEW  (renamed from max32664.cpp)
src/pulse_express_bootloader.h/.cpp NEW bootloader-mode API (§5.3), kept separate
src/max32664.h                     EDIT shrink to compat shim (#include new + typedef)
src/max32664.cpp                   DEL  (content moved)
README.md                          REWRITE per §2.3 (incl. "ships pre-flashed" note)
keywords.txt                       REGEN for PulseExpress + bootloader + all symbols
library.properties                 EDIT name/includes/version=2.1.0
CHANGELOG.md                       NEW  document 2.1.0 rename + new examples
examples/01..11                    ADD/RENAME per §4, SPDX headers on all
extras/firmware/README.md          NEW  placeholder telling operator where the (gitignored) .msbl goes
extras/flash_tool/flash_msbl.py    NEW  host PC script: parse .msbl header + stream pages
.gitignore                         EDIT add *.msbl, *.bin
.github/workflows/main.yml         VERIFY arduino-lint passes with new layout
build-uno-r4.sh                    KEEP (optionally add compile_all_examples.sh alias)
CLAUDE.md                          UPDATE class name + example list + bootloader after rename
assets/                            KEEP; verify README image paths
```

Backward-compat note: the `src/max32664.h` shim means existing user sketches that
`#include "max32664.h"` and use `Max32664` keep compiling — important since 2.0.0
already shipped.

---

## 7. Suggested execution order (when approved)

1. **Driver:** add public `readStatus()` (§3.1); rename class + add header/shim +
   type aliases. Compile existing examples unchanged via the shim to prove
   back-compat. (`./build-uno-r4.sh`)
2. **Examples:** renumber existing, then add new sketches one capability at a time,
   compiling each. (`arduino-cli compile --fqbn arduino:renesas_uno:minima ...`)
3. **Metadata:** `library.properties`, `keywords.txt`, SPDX headers across all files.
4. **Docs:** README rewrite, CHANGELOG, CLAUDE.md update.
5. **CI:** push branch, confirm `compile-examples` matrix + `arduino-lint` are green.

Each step is independently compilable; nothing is merged until the full board matrix
passes.

---

## 8. Known regressions on earlier firmware (from the multi-version rewrite `f4f180b`)

1. **Hard version gate — FIXED (soft warning).** `begin()` used to
   `return UnsupportedFirmware` when `_hubVer.major != 40`, blocking earlier/other
   firmware outright. Now a soft warning: `begin()` proceeds with legacy capability
   defaults and exposes `firmwareSupported()` for callers to warn on.
   (`src/max32664.cpp` begin(), `src/max32664.h` accessor + `_fwSupported`.)

2. **Dead capability flags — OPEN.** `Max32664Caps::sendBpMedication` /
   `sendRestMode` are set by `capsFor()` but **never consumed** anywhere. For
   firmware **<40.2.2** these should be `true` and trigger extra calibration-setup
   commands that the rewrite never sends → legacy calibration is incomplete on the
   oldest hubs. Fix needs the older UG opcodes confirmed (`docs/user-guide-6806`,
   `docs/an6921`) before implementing — do **not** guess the bytes. Decide: implement
   the steps, or drop the flags and document <40.2.2 as unsupported.

3. **Legacy path likely never hardware-validated.** The 824-byte vector / 23-byte
   sample / single-point calibration branch was reimplemented from scratch; the only
   shipped example (`02`) exercises whichever path the test board reported. The
   `04`/`05` examples (calibration-only, estimation-only) should be run against a real
   pre-40.5.0 hub as part of this work.

**Diagnostic needed:** what does `begin()`'s trace print for the affected board
(hub + algo version), and at which call does it fail? `setDebug(&Serial)` enables it.
That single line tells us whether the symptom was the gate (#1, now fixed) or the
legacy-path gaps (#2/#3).

## 9. Open questions / risks

- **Type renames:** rename the `Max32664*` struct/enum names too, or alias them?
  Default plan: rename class only, alias the rest, document canonical `PulseExpress*`
  spellings. (Lower risk, slightly less "clean".)
- **EEPROM portability (example 07):** AVR/SAMD use `<EEPROM.h>`; ESP32 uses
  `Preferences` or `EEPROM.begin(size)`. Need `#if` guards so the 30-board CI matrix
  keeps compiling. Vector is up to 824 bytes (legacy) — **exceeds Uno's 1 KB EEPROM
  headroom only slightly**; fine on Uno (1 KB), tight on some AVRs — document it.
- **Multi-subject example (08):** requires FW ≥40.5.0 hardware to actually run;
  it must compile and degrade gracefully (print "requires 40.5.0+") on older hubs via
  `caps().multiPointCalib`.
- **Repo/folder name:** current `protocentral-pulse-express` (kebab) differs from the
  newest snake_case siblings; not worth renaming the repo. Leave as-is.
- **`.msbl` leakage:** the image must never reach git, the README, or a GitHub release.
  Mitigations: `.gitignore` `*.msbl`/`*.bin`, no download link, `extras/firmware/` ships
  only a placeholder README. Worth a one-time `git log -p` / history scan to confirm no
  image was committed before the ignore rule existed.
- **Bricking risk (flasher):** a failed/interrupted flash can leave the hub in
  bootloader mode. The flasher example must detect bootloader-on-boot and offer
  re-flash rather than assuming app mode. Document recovery (it's non-destructive —
  re-enter bootloader and retry).
- **Flasher CI:** the Serial-stream variant compiles everywhere (no deps); the SD
  variant must be `#if __has_include(<SD.h>)`-guarded so the 30-board matrix stays green.
- **Bootloader code placement:** keep it in a separate `PulseExpressBootloader` class /
  files so the common app-mode driver isn't enlarged on memory-tight AVRs that never flash.
