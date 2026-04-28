#!/bin/bash
#########################################################################################
#
#  Build script for the ProtoCentral Pulse Express library — Arduino Uno R4
#
#  Compiles every sketch under examples/ against the Uno R4 Minima target by
#  default, or the Uno R4 WiFi target with --wifi. Installs the Renesas core
#  on first run if it is not already present.
#
#  Copyright (c) 2025 ProtoCentral Electronics
#
#  This software is licensed under the MIT License (http://opensource.org/licenses/MIT).
#
#########################################################################################

set -euo pipefail

LIBRARY_PATH="$(cd "$(dirname "$0")" && pwd)"
EXAMPLES_DIR="$LIBRARY_PATH/examples"
BOARD_FQBN="arduino:renesas_uno:minima"
CORE_ID="arduino:renesas_uno"

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
Pulse Express build script — Arduino Uno R4

Usage: $0 [OPTIONS]

Options:
  --wifi          Build for Uno R4 WiFi (arduino:renesas_uno:unor4wifi)
                  instead of the default Minima target.
  --fqbn FQBN     Override the board FQBN entirely.
  -h, --help      Show this help.

The script compiles every directory found in examples/ that contains a .ino
file with a matching name. Exit code is non-zero if any sketch fails to build.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --wifi)
            BOARD_FQBN="arduino:renesas_uno:unor4wifi"
            shift
            ;;
        --fqbn)
            BOARD_FQBN="$2"
            shift 2
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

if ! command -v arduino-cli &> /dev/null; then
    print_error "arduino-cli not found"
    echo "Install from: https://arduino.github.io/arduino-cli/"
    exit 1
fi

if ! arduino-cli core list 2>/dev/null | awk 'NR>1 {print $1}' | grep -qx "$CORE_ID"; then
    print_info "Installing $CORE_ID core..."
    arduino-cli core update-index
    arduino-cli core install "$CORE_ID"
fi

if [[ ! -d "$EXAMPLES_DIR" ]]; then
    print_error "examples/ directory not found at $EXAMPLES_DIR"
    exit 1
fi

print_info "Target FQBN: $BOARD_FQBN"
print_info "Library:     $LIBRARY_PATH"
echo

declare -a SKETCHES=()
while IFS= read -r dir; do
    name=$(basename "$dir")
    if [[ -f "$dir/$name.ino" ]]; then
        SKETCHES+=("$dir")
    else
        print_warning "Skipping $name (no $name.ino inside)"
    fi
done < <(find "$EXAMPLES_DIR" -mindepth 1 -maxdepth 1 -type d | sort)

if [[ ${#SKETCHES[@]} -eq 0 ]]; then
    print_error "No example sketches found under $EXAMPLES_DIR"
    exit 1
fi

PASS=0
FAIL=0
FAILED_NAMES=()

for sketch in "${SKETCHES[@]}"; do
    name=$(basename "$sketch")
    print_info "Compiling $name ..."
    if arduino-cli compile \
            --fqbn "$BOARD_FQBN" \
            --library "$LIBRARY_PATH" \
            --warnings default \
            "$sketch" > /tmp/pulse-express-build.log 2>&1; then
        SIZE_LINE=$(grep -E "^Sketch uses" /tmp/pulse-express-build.log || true)
        print_success "$name  ${SIZE_LINE#Sketch uses }"
        PASS=$((PASS + 1))
    else
        print_error "$name failed:"
        sed 's/^/    /' /tmp/pulse-express-build.log
        FAIL=$((FAIL + 1))
        FAILED_NAMES+=("$name")
    fi
done

echo
echo "================================================================"
print_info "$PASS passed, $FAIL failed (out of ${#SKETCHES[@]} sketches)"
if [[ $FAIL -gt 0 ]]; then
    for n in "${FAILED_NAMES[@]}"; do
        echo "  - $n"
    done
    exit 1
fi
echo "================================================================"
