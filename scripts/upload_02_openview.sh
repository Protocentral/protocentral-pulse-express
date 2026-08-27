#!/bin/bash
#########################################################################################
#
#  Compile + upload example 02 (RawPPGStreamOpenView) to a connected Arduino Uno R4.
#
#  Usage:
#    ./scripts/upload_02_openview.sh                 # auto-detect port, Uno R4 Minima
#    ./scripts/upload_02_openview.sh --wifi          # Uno R4 WiFi target
#    PORT=/dev/cu.usbmodemXXXX ./scripts/upload_02_openview.sh
#    FQBN=arduino:renesas_uno:unor4wifi ./scripts/upload_02_openview.sh
#
#  Copyright (c) 2025 ProtoCentral Electronics — MIT License.
#
#########################################################################################

set -euo pipefail

SKETCH="examples/02.RawPPGStreamOpenView"
LIB_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FQBN="${FQBN:-arduino:renesas_uno:minima}"

for arg in "$@"; do
    case "$arg" in
        --wifi) FQBN="arduino:renesas_uno:unor4wifi" ;;
        -h|--help)
            sed -n '4,14p' "$0"; exit 0 ;;
        *) echo "Unknown option: $arg" >&2; exit 1 ;;
    esac
done

if ! command -v arduino-cli >/dev/null 2>&1; then
    echo "ERROR: arduino-cli not found. Install: https://arduino.github.io/arduino-cli/" >&2
    exit 1
fi

# Auto-detect the Uno R4 (Renesas) port unless PORT is provided.
if [[ -z "${PORT:-}" ]]; then
    PORT="$(arduino-cli board list 2>/dev/null | awk '/renesas_uno/ {print $1; exit}')"
fi
if [[ -z "${PORT:-}" ]]; then
    echo "ERROR: could not auto-detect a Uno R4 (no 'renesas_uno' board in 'arduino-cli board list')." >&2
    echo "Connected boards:" >&2
    arduino-cli board list >&2
    echo "Re-run with the port explicitly, e.g.:  PORT=/dev/cu.usbmodemXXXX $0" >&2
    exit 1
fi

echo "[INFO] Sketch: $SKETCH"
echo "[INFO] FQBN:   $FQBN"
echo "[INFO] Port:   $PORT"

echo "[INFO] Compiling..."
arduino-cli compile --fqbn "$FQBN" --library "$LIB_ROOT" "$LIB_ROOT/$SKETCH"

echo "[INFO] Uploading..."
arduino-cli upload --fqbn "$FQBN" --port "$PORT" "$LIB_ROOT/$SKETCH"

echo "[OK] Uploaded $SKETCH to $PORT"
