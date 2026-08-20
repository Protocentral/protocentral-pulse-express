#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
# SPDX-FileCopyrightText: Copyright (c) 2025 Ashwin Whitchurch, Protocentral Electronics
#
# Host-side flasher for the ProtoCentral Pulse Express (MAX32664D).
#
# Parses a Maxim/ADI application image (.msbl) and streams it to the Arduino
# sketch examples/11.FirmwareFlash over USB-Serial, which relays the bootloader
# I2C commands to the hub. Field offsets follow UG6806 (Figures 9-12):
#
#   number of pages : byte 0x44
#   init vector     : bytes 0x28..0x32  (11 bytes)
#   auth bytes      : bytes 0x34..0x43  (16 bytes)
#   page data       : from 0x4C, 8208 bytes per page (8192 flash + 16 CRC)
#
# FACTORY / RECOVERY tool. The .msbl is Maxim/ADI IP and is NOT distributed with
# this library — supply your own. Implemented from spec; validate on hardware.
#
# Usage:
#   python3 flash_msbl.py --port /dev/ttyACM0 firmware.msbl
#   python3 flash_msbl.py --port COM5 --dry-run firmware.msbl   # parse only
#
# Requires: pyserial  (pip install pyserial)

import argparse
import sys
import time

MSBL_OFF_NUM_PAGES = 0x44
MSBL_OFF_INIT_VEC = 0x28
MSBL_INIT_VEC_LEN = 11
MSBL_OFF_AUTH = 0x34
MSBL_AUTH_LEN = 16
MSBL_OFF_PAGE_DATA = 0x4C
MSBL_OFF_PAGE_SIZE = 0x46   # uint16 LE: flash bytes per page (8192 in practice)
MSBL_PAGE_CRC_BYTES = 16    # appended to every page in the image
MSBL_PAGE_BYTES_MAX = 8208  # sketch's page buffer: 8192 + 16 CRC

# Status byte returned by the sketch (mirrors PulseExpressBootloader::Status).
STATUS_OK = 0x00

STATUS_HINTS = {
    0x02: " (incorrect byte count — the bootloader scores the page write as one"
          " command; a chunked --chunk-mode splits it and it counts short)",
    0x80: " (bootloader busy after retries)",
    0x81: " (checksum error — image not valid/keyed for this part)",
    0x82: " (authorization error — image not keyed for this part; a MAX32664A/B/C"
          " image on a D hub does this)",
    0x83: " (application invalid — flash did not complete; re-run)",
    0xE0: " (host I2C error)",
    0xE1: " (hub not in bootloader mode)",
    0xE2: " (timeout reading from the host)",
    0xE3: " (invalid argument)",
    0xE4: " (this host cannot issue an I2C transaction long enough for one page"
          " — use an ESP32 or RP2040 host; the UNO R4's Wire caps at 255 bytes)",
}

CHUNK_MODES = {"single": 0, "repeated-start": 1, "stop-each": 2}


def serial_error():
    """pyserial's SerialException, or a never-raised placeholder if pyserial is
    not installed (so --dry-run works without the dependency)."""
    try:
        import serial
        return serial.SerialException
    except ImportError:
        class _NoSerial(Exception):
            pass
        return _NoSerial


class Image:
    """A parsed .msbl."""

    def __init__(self, path, data, num_pages, page_size, init_vec, auth, pages):
        self.path = path
        self.magic = data[0:4].decode("ascii", errors="replace")
        self.target = data[0x08:0x10].rstrip(b"\x00").decode("ascii", errors="replace")
        self.crypto = data[0x18:0x20].rstrip(b"\x00").decode("ascii", errors="replace")
        self.size = len(data)
        self.num_pages = num_pages
        self.page_size = page_size
        self.payload_len = page_size + MSBL_PAGE_CRC_BYTES
        self.init_vec = init_vec
        self.auth = auth
        self.pages = pages

    def describe(self):
        print(f"Parsed {self.path}:")
        print(f"  magic      : {self.magic}   target: {self.target}   crypto: {self.crypto}")
        print(f"  pages      : {self.num_pages}")
        print(f"  page size  : {self.page_size} flash + {MSBL_PAGE_CRC_BYTES} CRC "
              f"= {self.payload_len} bytes per write")
        print(f"  init vector: {self.init_vec.hex(' ')}")
        print(f"  auth       : {self.auth.hex(' ')}")
        print(f"  page data  : {self.num_pages * self.payload_len} bytes "
              f"of a {self.size}-byte file")


def parse_msbl(path, page_size_override=None):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < MSBL_OFF_PAGE_DATA:
        raise ValueError("File too small to hold an .msbl header")
    if data[0:4] != b"msbl":
        raise ValueError(f"missing 'msbl' magic (found {data[0:4]!r})")

    num_pages = data[MSBL_OFF_NUM_PAGES]
    init_vec = data[MSBL_OFF_INIT_VEC:MSBL_OFF_INIT_VEC + MSBL_INIT_VEC_LEN]
    auth = data[MSBL_OFF_AUTH:MSBL_OFF_AUTH + MSBL_AUTH_LEN]

    # Page size is declared in the header, little-endian, at 0x46. Do NOT take
    # it from the bootloader's 0x81/0x01 answer: BL 8.0.0 reports 2048 (32-bit
    # words) for the same 8192-byte page, and sending 2064 bytes instead of the
    # image's 8208 earns status 0x02 (incorrect byte count) on the first page.
    page_size = page_size_override
    if page_size is None:
        page_size = int.from_bytes(data[MSBL_OFF_PAGE_SIZE:MSBL_OFF_PAGE_SIZE + 2], "little")
    if page_size <= 0:
        raise ValueError(f"header declares a page size of {page_size}")

    payload_len = page_size + MSBL_PAGE_CRC_BYTES
    if payload_len > MSBL_PAGE_BYTES_MAX:
        raise ValueError(
            f"page payload {payload_len} exceeds the sketch's {MSBL_PAGE_BYTES_MAX}-byte buffer"
        )

    expected = MSBL_OFF_PAGE_DATA + num_pages * payload_len
    if len(data) < expected:
        raise ValueError(
            f"header says {num_pages} pages x {payload_len} bytes (>= {expected} total) "
            f"but the file is {len(data)} bytes"
        )

    pages = []
    for i in range(num_pages):
        start = MSBL_OFF_PAGE_DATA + i * payload_len
        pages.append(data[start:start + payload_len])
    return Image(path, data, num_pages, page_size, init_vec, auth, pages)


def read_board_error(ser):
    """Ask the sketch for its last recorded failure detail ('D' -> len + text)."""
    try:
        ser.write(b"D")
        n = ser.read(1)
        if len(n) != 1 or n[0] == 0:
            return ""
        return ser.read(n[0]).decode(errors="replace").strip()
    except Exception:
        return ""


def expect_ack(ser, what):
    b = ser.read(1)
    if len(b) != 1:
        raise RuntimeError(f"{what}: no ack (timeout)")
    if b[0] != STATUS_OK:
        hint = STATUS_HINTS.get(b[0], "")
        detail = read_board_error(ser)
        detail = f"\n  board says: {detail}" if detail else ""
        raise RuntimeError(f"{what}: hub returned status 0x{b[0]:02X}{hint}{detail}")


def main():
    ap = argparse.ArgumentParser(description="Flash a .msbl to Pulse Express (MAX32664D)")
    ap.add_argument("msbl", help="path to the .msbl firmware image")
    ap.add_argument("--port", help="serial port (e.g. /dev/ttyACM0 or COM5)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dry-run", action="store_true", help="parse the .msbl only; no serial I/O")
    ap.add_argument("--page-size", type=int, default=None, metavar="N",
                    help="override the flash bytes per page from the header (rarely needed; "
                         "16 CRC bytes are added to get the write length)")
    ap.add_argument("--chunk-mode", choices=tuple(CHUNK_MODES),
                    default="single",
                    help="how the sketch puts a page on the wire. 'single' (default) "
                         "sends the whole page in one I2C transaction, which is what "
                         "the bootloader expects but needs a host that can buffer it "
                         "(ESP32/RP2040). 'repeated-start' and 'stop-each' split it "
                         "into 30-byte frames as fallbacks")
    args = ap.parse_args()

    try:
        img = parse_msbl(args.msbl, args.page_size)
    except FileNotFoundError:
        print(f"error: no such file: {args.msbl}", file=sys.stderr)
        return 2
    except ValueError as e:
        print(f"error: {args.msbl} is not a valid .msbl: {e}", file=sys.stderr)
        return 2

    img.describe()

    if args.dry_run:
        print("Dry run — not flashing.")
        return 0
    if not args.port:
        print("error: --port is required unless --dry-run", file=sys.stderr)
        return 2

    import serial  # imported here so --dry-run works without pyserial

    ser = serial.Serial(args.port, args.baud, timeout=20)
    time.sleep(2.0)  # allow the board to reset and enter bootloader

    # Wait for the sketch's READY line.
    deadline = time.time() + 15
    max_txn = None
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            print(f"  [board] {line}")
        if line.startswith("maxTxn="):
            try:
                max_txn = int(line.split("=", 1)[1])
            except ValueError:
                pass
        if line == "READY":
            break
    else:
        raise RuntimeError("Board did not report READY")

    need = img.payload_len + 2   # 0x80 0x04 + payload
    if args.chunk_mode == "single" and max_txn is not None and max_txn < need:
        raise RuntimeError(
            f"this host can only issue {max_txn}-byte I2C transactions but one page "
            f"write is {need} bytes. The bootloader scores a page as a single "
            f"command, so use an ESP32 or RP2040 host (their Wire buffer is "
            f"resizable). Nothing was erased."
        )

    # Tell the sketch the page length and chunking mode before anything else, so
    # both ends agree on how many bytes a 'P' command carries.
    # Chunk mode first: 'S' sizes the I2C stack for the selected mode and is the
    # gate that fails a too-small host before anything is erased.
    ser.write(b"M" + bytes([CHUNK_MODES[args.chunk_mode]]))
    expect_ack(ser, f"set chunk mode ({args.chunk_mode})")
    ser.write(b"S" + img.payload_len.to_bytes(2, "big"))
    expect_ack(ser, f"set page length ({img.payload_len})")

    ser.write(b"N" + img.num_pages.to_bytes(2, "big"))
    expect_ack(ser, "set num pages")
    ser.write(b"I" + bytes(img.init_vec))
    expect_ack(ser, "set init vector")
    ser.write(b"A" + bytes(img.auth))
    expect_ack(ser, "set auth")

    print("Erasing application...")
    ser.write(b"E")
    expect_ack(ser, "erase")

    for i, pg in enumerate(img.pages):
        ser.write(b"P" + pg)
        expect_ack(ser, f"page {i + 1}/{img.num_pages}")
        print(f"\r  flashed page {i + 1}/{img.num_pages}", end="", flush=True)
    print()

    ser.write(b"X")
    expect_ack(ser, "exit to application")
    print("Done. Hub restarted into application mode.")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        # An interrupted flash leaves the hub in bootloader mode; say so rather
        # than dumping a traceback, since the remedy is simply to re-run.
        print("\ninterrupted — hub may be left in bootloader mode; re-run to recover",
              file=sys.stderr)
        sys.exit(130)
    except RuntimeError as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
    except serial_error() as e:  # pyserial failures (port busy, gone, denied)
        print(f"error: serial port: {e}", file=sys.stderr)
        sys.exit(1)
