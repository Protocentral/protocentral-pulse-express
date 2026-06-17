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
MSBL_PAGE_BYTES = 8208  # 8192 + 16 CRC

# Status byte returned by the sketch (mirrors PulseExpressBootloader::Status).
STATUS_OK = 0x00


def parse_msbl(path):
    with open(path, "rb") as f:
        data = f.read()
    if len(data) < MSBL_OFF_PAGE_DATA + MSBL_PAGE_BYTES:
        raise ValueError("File too small to be a valid .msbl")

    num_pages = data[MSBL_OFF_NUM_PAGES]
    init_vec = data[MSBL_OFF_INIT_VEC:MSBL_OFF_INIT_VEC + MSBL_INIT_VEC_LEN]
    auth = data[MSBL_OFF_AUTH:MSBL_OFF_AUTH + MSBL_AUTH_LEN]

    expected = MSBL_OFF_PAGE_DATA + num_pages * MSBL_PAGE_BYTES
    if len(data) < expected:
        raise ValueError(
            f"Header says {num_pages} pages ({expected} bytes) but file is {len(data)} bytes"
        )

    pages = []
    for i in range(num_pages):
        start = MSBL_OFF_PAGE_DATA + i * MSBL_PAGE_BYTES
        pages.append(data[start:start + MSBL_PAGE_BYTES])
    return num_pages, init_vec, auth, pages


def expect_ack(ser, what):
    b = ser.read(1)
    if len(b) != 1:
        raise RuntimeError(f"{what}: no ack (timeout)")
    if b[0] != STATUS_OK:
        raise RuntimeError(f"{what}: hub returned status 0x{b[0]:02X}")


def main():
    ap = argparse.ArgumentParser(description="Flash a .msbl to Pulse Express (MAX32664D)")
    ap.add_argument("msbl", help="path to the .msbl firmware image")
    ap.add_argument("--port", help="serial port (e.g. /dev/ttyACM0 or COM5)")
    ap.add_argument("--baud", type=int, default=115200)
    ap.add_argument("--dry-run", action="store_true", help="parse the .msbl only; no serial I/O")
    args = ap.parse_args()

    num_pages, init_vec, auth, pages = parse_msbl(args.msbl)
    print(f"Parsed {args.msbl}:")
    print(f"  pages      : {num_pages}")
    print(f"  init vector: {init_vec.hex(' ')}")
    print(f"  auth       : {auth.hex(' ')}")
    print(f"  page bytes : {MSBL_PAGE_BYTES} each, {num_pages * MSBL_PAGE_BYTES} total")

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
    while time.time() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            print(f"  [board] {line}")
        if line == "READY":
            break
    else:
        raise RuntimeError("Board did not report READY")

    ser.write(b"N" + bytes([(num_pages >> 8) & 0xFF, num_pages & 0xFF]))
    expect_ack(ser, "set num pages")
    ser.write(b"I" + bytes(init_vec))
    expect_ack(ser, "set init vector")
    ser.write(b"A" + bytes(auth))
    expect_ack(ser, "set auth")

    print("Erasing application...")
    ser.write(b"E")
    expect_ack(ser, "erase")

    for i, pg in enumerate(pages):
        ser.write(b"P" + pg)
        expect_ack(ser, f"page {i + 1}/{num_pages}")
        print(f"\r  flashed page {i + 1}/{num_pages}", end="", flush=True)
    print()

    ser.write(b"X")
    expect_ack(ser, "exit to application")
    print("Done. Hub restarted into application mode.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
