#!/bin/bash
#########################################################################################
#
#  Hub firmware flasher for the ProtoCentral Pulse Express (MAX32664D)
#
#  FACTORY / RECOVERY TOOL. Pulse Express boards ship pre-flashed; you normally
#  never need this. Use it to recover a hub stuck in bootloader mode, or to move
#  a board to a different MAX32664D firmware version.
#
#  Two stages, both driven from here:
#    1. Compile + upload examples/11.FirmwareFlash to the Arduino host board.
#       That sketch puts the hub into bootloader mode and relays page writes.
#    2. Run extras/flash_tool/flash_msbl.py, which parses the .msbl and streams
#       its pages to the sketch over USB-Serial.
#
#  The .msbl image is Maxim/ADI intellectual property and is NOT distributed with
#  this library — supply your own (see extras/firmware/README.md).
#
#  Copyright (c) 2025 ProtoCentral Electronics
#
#  This software is licensed under the MIT License (http://opensource.org/licenses/MIT).
#
#########################################################################################

set -euo pipefail

LIBRARY_PATH="$(cd "$(dirname "$0")" && pwd)"
SKETCH_DIR="$LIBRARY_PATH/examples/11.FirmwareFlash"
FLASH_TOOL="$LIBRARY_PATH/extras/flash_tool/flash_msbl.py"
BOARD_FQBN="arduino:renesas_uno:minima"
BAUD=115200
CHUNK_MODE="single"
PAGE_SIZE=""
PORT=""
MSBL=""
SKETCH_ONLY=0
DRY_RUN=0
SKIP_UPLOAD=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

print_info()    { echo -e "${BLUE}[INFO]${NC} $1"; }
print_success() { echo -e "${GREEN}[OK]${NC}   $1"; }
print_warning() { echo -e "${YELLOW}[WARN]${NC} $1"; }
print_error()   { echo -e "${RED}[ERROR]${NC} $1"; }

show_help() {
    cat <<EOF
Pulse Express hub firmware flasher (factory / recovery)

Usage: $0 [OPTIONS] [firmware.msbl]

Stage 1 uploads examples/11.FirmwareFlash to the Arduino host board.
Stage 2 streams the .msbl to it with extras/flash_tool/flash_msbl.py.

Options:
  --port PORT     Serial port of the Arduino host board (e.g. /dev/cu.usbmodem1101,
                  /dev/ttyACM0, COM5). Auto-detected if omitted.
  --fqbn FQBN     Board FQBN. Default: $BOARD_FQBN
  --wifi          Shorthand for --fqbn arduino:renesas_uno:unor4wifi
  --esp32         Shorthand for --fqbn esp32:esp32:esp32
  --baud RATE     Serial baud for the host script. Default: $BAUD
  --chunk-mode M  How the sketch puts a page on the wire: 'single' (default,
                  one I2C transaction per page — what the bootloader expects),
                  'repeated-start', or 'stop-each'. The fallbacks split the page
                  into 30-byte frames; the MAX32664D rejects that with 0x02.
  --page-size N   Override the flash bytes/page from the image header. Rarely
                  needed — the header is authoritative, and the bootloader's own
                  reported page size is NOT a byte count on BL 8.0.0.
  --sketch-only   Stage 1 only: upload the flasher sketch, then stop.
  --skip-upload   Stage 2 only: assume the sketch is already on the board.
  --dry-run       Parse the .msbl and report its header; touch no hardware.
  -h, --help      Show this help.

Examples:
  $0 --dry-run extras/firmware/MAX32664D_BPT_40.6.0.msbl
  $0 --port /dev/cu.usbmodem1101 extras/firmware/MAX32664D_BPT_40.6.0.msbl
  $0 --wifi --sketch-only

HOST BOARD: a page write is 8210 bytes in ONE I2C transaction, so the host's
Wire buffer must hold all of it. Use an ESP32 (--esp32) or RP2040 — their Wire
buffer is resizable. The Arduino UNO R4 CANNOT flash: its Renesas Wire caps a
transaction at 255 bytes. Small AVRs lack the RAM entirely. The flasher checks
this and stops before erasing anything, so an unsuitable host is harmless.

WARNING: the flashing sequence is implemented from Maxim/ADI UG6806 Table 9 and
has NOT been hardware-validated. A failed or interrupted flash leaves the hub in
bootloader mode — re-run this script to recover.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --port)        PORT="$2"; shift 2 ;;
        --fqbn)        BOARD_FQBN="$2"; shift 2 ;;
        --wifi)        BOARD_FQBN="arduino:renesas_uno:unor4wifi"; shift ;;
        --esp32)       BOARD_FQBN="esp32:esp32:esp32"; shift ;;
        --baud)        BAUD="$2"; shift 2 ;;
        --chunk-mode)  CHUNK_MODE="$2"; shift 2 ;;
        --page-size)   PAGE_SIZE="$2"; shift 2 ;;
        --sketch-only) SKETCH_ONLY=1; shift ;;
        --skip-upload) SKIP_UPLOAD=1; shift ;;
        --dry-run)     DRY_RUN=1; shift ;;
        -h|--help)     show_help; exit 0 ;;
        -*)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
        *)
            if [[ -n "$MSBL" ]]; then
                print_error "More than one .msbl given: '$MSBL' and '$1'"
                exit 1
            fi
            MSBL="$1"
            shift
            ;;
    esac
done

# ----------------------------------------------------------------------------
# Preconditions
# ----------------------------------------------------------------------------

if [[ ! -f "$FLASH_TOOL" ]]; then
    print_error "Host flash tool not found at $FLASH_TOOL"
    exit 1
fi

PYTHON=""
for candidate in python3 python; do
    if command -v "$candidate" &> /dev/null; then PYTHON="$candidate"; break; fi
done
if [[ -z "$PYTHON" ]]; then
    print_error "python3 not found — required to run flash_msbl.py"
    exit 1
fi

if [[ $SKETCH_ONLY -eq 0 ]]; then
    if [[ -z "$MSBL" ]]; then
        print_error "No .msbl given."
        echo
        echo "Pulse Express boards ship pre-flashed, and the MAX32664D image is"
        echo "Maxim/ADI IP that is not distributed with this library. Place your"
        echo "licensed image under extras/firmware/ and pass it as an argument:"
        echo
        echo "    $0 --port <PORT> extras/firmware/<image>.msbl"
        echo
        echo "Run '$0 --sketch-only' to upload just the flasher sketch, or"
        echo "'$0 --help' for all options."
        exit 1
    fi
    if [[ ! -f "$MSBL" ]]; then
        print_error "No such file: $MSBL"
        exit 1
    fi
fi

# --dry-run never touches hardware: parse the image and stop.
TOOL_ARGS=(--chunk-mode "$CHUNK_MODE")
[[ -n "$PAGE_SIZE" ]] && TOOL_ARGS+=(--page-size "$PAGE_SIZE")

if [[ $DRY_RUN -eq 1 ]]; then
    print_info "Dry run — parsing $MSBL, no hardware access"
    exec "$PYTHON" "$FLASH_TOOL" --dry-run "${TOOL_ARGS[@]}" "$MSBL"
fi

if ! command -v arduino-cli &> /dev/null; then
    print_error "arduino-cli not found"
    echo "Install from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

if ! "$PYTHON" -c "import serial" 2>/dev/null; then
    print_error "pyserial not installed (needed to talk to the board)"
    echo "Install with: $PYTHON -m pip install pyserial"
    exit 1
fi

# ----------------------------------------------------------------------------
# Port discovery
# ----------------------------------------------------------------------------

detect_port() {
    # `arduino-cli board list` prints one row per port, with the port in column
    # 1 and the FQBN of any identified board later in the row. Machines have
    # plenty of serial ports that are not Arduinos (Bluetooth, debug consoles),
    # so never just take the first row: prefer a board matching the target
    # FQBN, then any identified board, and otherwise give up rather than guess.
    local listing
    listing=$(arduino-cli board list 2>/dev/null) || return 0

    local detected
    detected=$(echo "$listing" | awk -v fqbn="$BOARD_FQBN" \
        '$0 ~ fqbn && $1 ~ /^(\/dev\/|COM)/ {print $1; exit}')
    if [[ -n "$detected" ]]; then echo "$detected"; return 0; fi

    # Any row that identified a board at all (i.e. not "Unknown").
    detected=$(echo "$listing" | awk \
        'NR>1 && $1 ~ /^(\/dev\/|COM)/ && $0 !~ /Unknown/ {print $1; exit}')
    echo "$detected"
}

if [[ -z "$PORT" ]]; then
    PORT="$(detect_port)"
    if [[ -z "$PORT" ]]; then
        print_error "Could not auto-detect a serial port."
        echo "Plug the board in and pass it explicitly: --port <PORT>"
        echo "Ports currently visible to arduino-cli:"
        arduino-cli board list || true
        exit 1
    fi
    print_info "Auto-detected port: $PORT"
fi

print_info "Board FQBN:  $BOARD_FQBN"
print_info "Port:        $PORT"
[[ -n "$MSBL" ]] && print_info "Firmware:    $MSBL"
echo

# ----------------------------------------------------------------------------
# Stage 1 — upload the relay sketch
# ----------------------------------------------------------------------------

if [[ $SKIP_UPLOAD -eq 1 ]]; then
    print_info "Skipping stage 1 (--skip-upload); assuming 11.FirmwareFlash is loaded"
else
    print_info "Stage 1/2: compiling 11.FirmwareFlash ..."
    if ! arduino-cli compile --fqbn "$BOARD_FQBN" --library "$LIBRARY_PATH" \
            "$SKETCH_DIR" > /tmp/pulse-express-flash.log 2>&1; then
        print_error "Compile failed:"
        sed 's/^/    /' /tmp/pulse-express-flash.log
        exit 1
    fi
    print_success "compiled  $(grep -E '^Sketch uses' /tmp/pulse-express-flash.log || true)"

    print_info "Stage 1/2: uploading to $PORT ..."
    if ! arduino-cli upload --fqbn "$BOARD_FQBN" --port "$PORT" \
            "$SKETCH_DIR" > /tmp/pulse-express-flash.log 2>&1; then
        print_error "Upload failed:"
        sed 's/^/    /' /tmp/pulse-express-flash.log
        exit 1
    fi
    print_success "uploaded 11.FirmwareFlash"

    # The board re-enumerates after upload; give the port time to come back.
    print_info "Waiting for the board to re-enumerate ..."
    sleep 3
    if [[ ! -e "$PORT" ]]; then
        NEW_PORT="$(detect_port)"
        if [[ -n "$NEW_PORT" && "$NEW_PORT" != "$PORT" ]]; then
            print_warning "Port changed after upload: $PORT -> $NEW_PORT"
            PORT="$NEW_PORT"
        fi
    fi
fi

if [[ $SKETCH_ONLY -eq 1 ]]; then
    echo
    print_success "Flasher sketch is on the board."
    echo "Stream an image to it with:"
    echo "    $PYTHON $FLASH_TOOL --port $PORT <firmware>.msbl"
    exit 0
fi

# ----------------------------------------------------------------------------
# Stage 2 — stream the .msbl
# ----------------------------------------------------------------------------

echo
print_warning "Do not unplug the board or interrupt this step."
print_info "Stage 2/2: streaming $(basename "$MSBL") to the hub ..."
echo

if "$PYTHON" "$FLASH_TOOL" --port "$PORT" --baud "$BAUD" "${TOOL_ARGS[@]}" "$MSBL"; then
    echo
    print_success "Hub firmware flashed."
    echo "Verify with example 10:"
    echo "    arduino-cli compile --fqbn $BOARD_FQBN --library $LIBRARY_PATH \\"
    echo "        --upload --port $PORT examples/10.DeviceInfoAndDiagnostics"
    echo "It should report the new hub version and 'AFE PART_ID: 0x15'."
else
    echo
    print_error "Flashing failed."
    echo "The hub is most likely still in bootloader mode — that is recoverable:"
    echo "re-run this script. If the failure was status 0xE4 (or 'maxTxn=' above"
    echo "is smaller than 8210), this host cannot carry a page — flash from an"
    echo "ESP32 or RP2040 instead:"
    echo "    $0 --esp32 --port <PORT> $MSBL"
    echo "See extras/flash_tool/README.md for the rest of the troubleshooting notes."
    exit 1
fi
