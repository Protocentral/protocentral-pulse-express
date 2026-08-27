# `flash_msbl.py` — host-side hub firmware flasher

Streams a Maxim/ADI MAX32664D application image (`.msbl`) to the sensor hub.

> **Factory / recovery only.** Pulse Express boards ship pre-flashed. Use this to
> recover a hub stuck in bootloader mode, or to move a board between MAX32664D
> firmware versions.
>
> The flashing sequence is implemented from **UG6806 Table 9** and has **not been
> hardware-validated** — verify before relying on it in production. R&D use only,
> not a medical device.

## How the two pieces fit together

The Arduino can't parse an 800 KB firmware image, and the host PC has no I2C bus
to the hub. So the work is split:

```
   PC                          Arduino host board              Pulse Express
┌──────────────────┐  USB   ┌────────────────────────┐  I2C  ┌──────────────┐
│ flash_msbl.py    │ ─────► │ 11.FirmwareFlash sketch │ ────► │  MAX32664D   │
│ parses .msbl,    │ serial │ relays each command as  │ 0x55  │  bootloader  │
│ streams pages    │ ◄───── │ a bootloader I2C write  │ ◄──── │              │
└──────────────────┘  ack   └────────────────────────┘       └──────────────┘
```

The sketch holds MFIO low across reset to enter the bootloader, prints `READY`,
then services one-letter commands from the script: `M` (I2C chunking mode),
`S` (page payload length, which also sizes the I2C buffer and is the gate that
rejects an unsuitable host), `N` (page count), `I` (init vector), `A` (auth
bytes), `E` (erase), `P` (one page), `X` (exit to application). Each returns a
single status byte, `0x00` on success. `D` returns the board's last failure
detail as a length-prefixed string, which the host prints after any error.

## Requirements

- **A host whose I2C stack can issue one ~8.2 KB transaction.** This is the
  binding constraint — see below. In practice: **ESP32 or RP2040**.
- ≥ ~16 KB free RAM for the page buffer.
- The **Arduino UNO R4 cannot flash the hub**: its Renesas `Wire` caps a
  transaction at 255 bytes (`I2C_BUFFER_LENGTH`, and `write_to()` takes a
  `uint8_t` length), with no way to grow it. Small AVRs lack the RAM entirely
  and build a stub. The flasher detects this and stops **before** erasing
  anything, so pointing it at the wrong host is harmless.
- `arduino-cli` on `PATH`, and `pyserial` (`pip install pyserial`).
- Your own licensed `.msbl`. It is Maxim/ADI IP, is not distributed here, and
  `*.msbl` is gitignored — never commit one. See [`../firmware/README.md`](../firmware/README.md).

## The easy path — `flash-firmware.sh`

The repo-root script does both stages (upload the sketch, then stream the image):

```bash
# Check the image parses — no hardware touched, pyserial not needed
./flash-firmware.sh --dry-run extras/firmware/MAX32664D_BPT_40.6.0.msbl

# Full flash, auto-detecting the board's port
./flash-firmware.sh extras/firmware/MAX32664D_BPT_40.6.0.msbl

# Explicit port and a non-default host board
./flash-firmware.sh --port /dev/cu.usbmodem1401 --wifi extras/firmware/image.msbl

# Just put the flasher sketch on the board and stop
./flash-firmware.sh --sketch-only
```

`--help` lists every option (`--fqbn`, `--esp32`, `--baud`, `--skip-upload`).

## The manual path — two commands

Equivalent to the above, if you'd rather drive each stage yourself:

```bash
# 1. Put the relay sketch on the Arduino
arduino-cli compile --fqbn arduino:renesas_uno:minima --library . \
    --upload --port /dev/cu.usbmodem1401 examples/11.FirmwareFlash

# 2. Stream the image to it
python3 extras/flash_tool/flash_msbl.py \
    --port /dev/cu.usbmodem1401 extras/firmware/MAX32664D_BPT_40.6.0.msbl
```

### `flash_msbl.py` options

| Option | Meaning |
|--------|---------|
| `msbl` (positional) | Path to the firmware image. Required. |
| `--port PORT` | Serial port of the Arduino. Required unless `--dry-run`. |
| `--baud RATE` | Serial baud. Default `115200`, matching the sketch. |
| `--dry-run` | Parse the image, print its header, exit. No serial I/O, no pyserial needed. |
| `--chunk-mode M` | How the sketch splits a page across I2C frames: `repeated-start` (default) or `stop-each`. See below. |
| `--page-size N` | Override the flash bytes/page from the image header. Rarely needed. |

### Page size: trust the image, not the bootloader

The `.msbl` header declares its flash page size at offset `0x46` (uint16 LE);
every page write carries that many bytes plus 16 CRC bytes. For the MAX32664D
images seen so far that is 8192 + 16 = **8208 bytes per write**.

Do **not** size page writes from the bootloader's own `0x81 0x01` answer. UG6806's
trace (bootloader 3.0.0) shows it returning `0x20 0x00` = 8192, a byte count — but
**bootloader 8.0.0 answers 2048 for the same 8192-byte page**, i.e. 32-bit words,
matching the `4` stored at `.msbl` offset `0x48`. The sketch prints the raw value
for information only; a hub reporting exactly one quarter of the header's page
size is normal, not a mismatch.

### Why the host board matters

The bootloader treats a page write as **one** command — `0x80 0x04` plus the
whole 8208-byte payload — and validates its byte count. Splitting it across I2C
frames does not work: with a STOP after each frame the hub scores each frame as
its own command and answers `0x02` (incorrect byte count).

So the host must buffer all 8210 bytes and send them in a single transaction.
Cores that define `WIRE_HAS_BUFFER_SIZE` (ESP32, RP2040) can grow their Wire
buffer to fit; the sketch does so **before** `Wire.begin()`, because the RP2040
core ignores `setBufferSize()` once the bus is running. Cores with a fixed buffer
cannot flash at all.

On startup the sketch prints its capability, which the host script checks against
the image before erasing anything:

```
  [board] BL 8.0.0 pageSize=2048
  [board] maxTxn=8210        <- must be >= page payload + 2
```

`--chunk-mode repeated-start` and `--chunk-mode stop-each` split the page into
30-byte frames as fallbacks for experimentation. Neither is expected to work on
the MAX32664D; they exist so the behaviour can be A/B tested without a rebuild.

Exit codes: `0` success, `1` flashing/serial failure, `2` bad arguments or a
malformed image, `130` interrupted.

### What a run looks like

```
$ python3 extras/flash_tool/flash_msbl.py --port /dev/cu.usbmodem1401 image.msbl
Parsed image.msbl:
  magic      : msbl   target: MAX32660   crypto: AES-192
  pages      : 21
  page size  : 8192 flash + 16 CRC = 8208 bytes per write
  init vector: c3 2e 24 88 73 5e e6 b7 a8 ea 19
  auth       : 2b b6 94 be 0a 62 11 bf 32 81 01 1c b7 b5 e6 d8
  page data  : 172368 bytes of a 172448-byte file
  [board] BL 8.0.0 pageSize=2048
  [board] READY
Erasing application...
  flashed page 21/21
Done. Hub restarted into application mode.
```

(`pageSize=2048` from the board is the bootloader's word count, not a byte
count — see "Page size" above.)

Then confirm with example 10 — it should report the new hub version and
`AFE PART_ID: 0x15`:

```bash
arduino-cli compile --fqbn arduino:renesas_uno:minima --library . \
    --upload --port /dev/cu.usbmodem1401 examples/10.DeviceInfoAndDiagnostics
```

## Troubleshooting

| Symptom | Cause and fix |
|---------|---------------|
| `Board did not report READY` | The sketch isn't running, or the hub didn't enter bootloader mode. Confirm `11.FirmwareFlash` is the loaded sketch, check the RESET/MFIO pin numbers at the top of it match your wiring, and close any Serial Monitor holding the port. |
| `ERR enterBootloader 0x..` on the board | The hub didn't respond to the mode switch. Check I2C wiring and that the board is powered. |
| `status 0xE4` / `this host can only issue N-byte I2C transactions` | The host board cannot carry a whole page. Flash from an ESP32 or RP2040. Nothing was erased. |
| `page 1/N: hub returned status 0x02` | Incorrect byte count for the page write — the page reached the hub split across transactions. Use `--chunk-mode single` (the default) on a host that can buffer it. |
| `page 1/N: ... 0xE0 (host I2C error)` | The host's own I2C call failed; the board's `D` response is printed underneath and names the exact step. Usually a buffer too small for the chosen chunk mode. |
| `... hub returned status 0x81 / 0x82` | Bootloader checksum / authorization error — the `.msbl` is not a valid keyed image for this part (e.g. a MAX32664**A/B/C** image on a **D** hub). |
| `set num pages: no ack (timeout)` | Baud mismatch, or something else has the port open. |
| `error: serial port: ...` | Port busy, gone, or permission denied. On Linux add yourself to `dialout`. |
| Flash fails partway | The hub stays in bootloader mode, which is recoverable — just re-run. It will not brick. |
| Sketch prints "needs a board with >= ~16 KB RAM" | The host board is too small; use a UNO R4 / ESP32 / RP2040 / STM32. |

## Image layout the parser expects

Offsets from UG6806 (Figures 9–12):

| Field | Offset | Size |
|-------|--------|------|
| Magic `msbl` | `0x00` | 4 bytes |
| Target part (e.g. `MAX32660`) | `0x08` | 8 bytes |
| Crypto (e.g. `AES-192`) | `0x18` | 8 bytes |
| Initialization vector | `0x28` | 11 bytes |
| Authentication bytes | `0x34` | 16 bytes |
| Number of pages | `0x44` | 1 byte |
| Flash bytes per page | `0x46` | uint16 LE (8192) |
| Page data | `0x4C` | page size + 16 CRC bytes per page |

`--dry-run` prints all of these, so it is the quickest way to sanity-check an
image before touching hardware.
