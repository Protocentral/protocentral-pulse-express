# Changelog

All notable changes to the ProtoCentral Pulse Express library.

## [2.2.0] - 2026-08-27

### Fixed
- **`startRaw()` failed on 40.2.2 hubs, leaving the hub idle** (`dataReady` never
  asserting). Two causes:
  - The MAX30101 LED-current writes sent a 5-byte frame
    (`0x40 0x03 <reg> <val> 0x00`); the hub expects exactly 4 bytes and rejects
    the padded form.
  - Raw sensor-only mode required "enable BPT in estimation mode"
    (`0x52 0x04 0x02`) to succeed. That command is rejected on 40.2.2+ until a
    calibration vector / date-time / SpO2 coefficients have been loaded, so a
    freshly-booted board could never stream raw PPG. The algorithm contributes
    nothing in sensor-only output mode, so the step is now best-effort: its
    status is traced and raw streaming continues.
- **`begin()` restarted the hub application unnecessarily.** `enterAppMode()`
  unconditionally wrote `0x01 0x00 0x00` (set operating mode) and waited only
  20 ms. That command restarts the hub's application; the following commands
  then landed while its sensor stack was still initialising and the AFE enable
  answered `0xFF`. UG6921's application-mode flows never write the mode byte —
  the hub boots into application mode when MFIO is high at reset — so
  `enterAppMode()` now *reads* the mode first (`0x02 0x00`) and only switches,
  with a full 1.4 s settling delay, if the hub reports something other than
  application mode.
- `enableAfe(true)` retries a `0xFF` up to 3 times at 250 ms spacing (UG6921
  Table 1: "insert delay and resend"), and on final failure traces the AFE
  `PART_ID` read so the log distinguishes a sequencing fault from a hub that
  genuinely cannot reach the sensor.
- `startRaw()` traces the status of every sub-step so a failure names the
  offending command, and now follows UG6921 Table 6's documented ordering
  (AFE -> algorithm -> AGC off).

- **Misleading "Algorithm firmware: 0.0.0".** UG6921 documents `AA FF 03` (hub
  version) but not `AA FF 07`; some 40.x builds reject the latter with status
  0x02 (incorrect byte count for the family). `begin()` ignored that failure and
  left the version at 0.0.0, which reads like a broken hub in a diagnostic log.
  The result is now tracked via `algoVersionValid()`.

- **Hub output-FIFO overflow wedged raw streaming.** `readRaw()`/`readSamples()`
  return at most `cap` samples per call; a caller that polls slowly (as
  `10.DeviceInfoAndDiagnostics` did, one 16-sample call per 500 ms) lets the
  backlog grow until the hub's output FIFO overflows, after which every
  `0x12 0x01` read is rejected with `0xFF` and streaming never recovers. Both
  readers now flush the FIFO on a failed sample read, report how many samples
  they did retrieve, and trace a warning when the overflow flag is set. The
  example drains the FIFO to empty each pass and polls at 100 ms.

### Fixed (legacy 40.2.2 firmware support)
- **`03.HeartRateSpO2` printed a fabricated SpO2 confidence of 0 %** on the
  23-byte legacy sample, which does not carry that field. It now prints `n/a`.
- **`09.HeartRateVariability` silently did nothing on legacy firmware.** HRV is
  derived from the hub's inter-beat interval, which only exists in the 29-byte
  sample (>= 40.5.0); the sketch warned once and then collected zeros forever.
  It now stops with an explanation and points at `03.HeartRateSpO2`.
- Examples `01`, `03`, `05`, `06`, `07`, `09` drain the hub FIFO to empty each
  pass instead of taking one capped read per loop. An overflowed output FIFO
  rejects every later read until emptied, which stops streaming permanently.

### Added
- `PulseExpressCaps::extendedSampleFields` — true when the hub reports the
  29-byte sample carrying IBI, SpO2 confidence and the two report flags
  (>= 40.5.0). Examples and `parseSample()` now branch on this named capability
  rather than testing `sampleBytes` directly.
- **`flash-firmware.sh`** — one-command hub firmware flashing: compiles and
  uploads `11.FirmwareFlash` to the Arduino host, then runs
  `extras/flash_tool/flash_msbl.py` against it. Auto-detects the board's serial
  port (preferring one matching the target FQBN), with `--dry-run`,
  `--sketch-only`, `--skip-upload`, `--port`, `--fqbn`/`--wifi`/`--esp32`.
- **`extras/flash_tool/README.md`** — full `flash_msbl.py` usage: how the sketch
  and host script split the work, options, a sample run, troubleshooting table,
  and the `.msbl` layout the parser expects.
- `flash_msbl.py` reports malformed images, missing files, serial errors and
  Ctrl-C as one-line messages with distinct exit codes instead of tracebacks,
  validates the `msbl` magic, and prints the target part and crypto from the
  header. Bootloader status bytes come with a plain-English hint.

### Fixed (firmware flashing)
- **Page size was hard-coded to 8208 bytes.** It now comes from the image header
  (uint16 LE at offset `0x46`, plus 16 CRC bytes) and is sent to the sketch,
  which sizes each `P` command from it. Overridable with `--page-size`.
  The bootloader's own `0x81 0x01` answer must NOT be used as a byte count:
  UG6806's trace (BL 3.0.0) returns 8192 there, but **BL 8.0.0 returns 2048 for
  the same 8192-byte page** — 32-bit words, matching the `4` at `.msbl` offset
  `0x48`. The sketch now prints that value as information only.
- **Page writes were split across I2C transactions; the bootloader requires one.**
  `writePage()` sent the 8208-byte payload in 30-byte `Wire` frames with a STOP
  after each, and the MAX32664D answered `0x02` (incorrect byte count) because it
  scores each frame as a separate command. It now sends `0x80 0x04` + the whole
  page in a **single transaction** (`ChunkMode::SingleTransaction`, the default).
  The two chunked strategies remain selectable via `setChunkMode()` /
  `--chunk-mode {repeated-start,stop-each}` for A/B testing.
- **The host board must be able to buffer a whole page.** New
  `configureForPageSize()` grows the Wire buffer on cores that support it
  (`WIRE_HAS_BUFFER_SIZE`: ESP32, RP2040 — the sketch pre-sizes before
  `Wire.begin()`, which RP2040 requires) and returns the new
  `Status::PageTooLarge` (0xE4) when the host cannot. The check runs **before**
  the erase step, so an unsuitable host leaves the hub's firmware intact.
  **The Arduino UNO R4 cannot flash the hub** — its Renesas Wire caps a
  transaction at 255 bytes. The sketch prints `maxTxn=` and the host script
  refuses to start when it is too small.
- `writePage()` checks every `Wire.write()` return value; on a platform whose TX
  buffer is smaller than the chunk size it now reports a comm error instead of
  silently dropping bytes and sending an under-length page.
- **Flash failures were undiagnosable.** The sketch runs with debug output off
  (the serial link carries a binary protocol), so driver traces went nowhere.
  `trace()` now always records into a buffer exposed by `lastError()`, the sketch
  serves it via a new `D` command, and `flash_msbl.py` prints it under any error
  (`board says: ...`).
- `flushFifo()` — discard the hub's output FIFO (bounded), the documented
  recovery from an overflow.
- `algoVersionValid()` — false when the hub did not answer the algorithm-version
  read, so callers can print "unavailable" instead of a fabricated 0.0.0.
- `readAfePartId(uint8_t&, uint8_t sensorIdx = kAfeSensorIndex)` — reads a
  sensor's `PART_ID` register (`0x41 0x03 0xFF`, expect `0x15`), the diagnostic
  UG6921 Table 1 prescribes for `0xFF` responses. The index argument lets a
  caller sweep the sensor slots to find where a given firmware image maps the
  AFE; `10.DeviceInfoAndDiagnostics` does this automatically when the default
  index does not answer.

### Changed
- `10.DeviceInfoAndDiagnostics` enables `setDebug(&Serial)`, prints the AFE
  `PART_ID` and the `startRaw()` status code, and reports FIFO sample count plus
  the first IR/Red counters each poll.

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
