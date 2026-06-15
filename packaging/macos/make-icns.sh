#!/usr/bin/env bash
#
# Build Indium.icns from a single 1024x1024 PNG logo, using the macOS-native
# sips + iconutil toolchain. Optional: if no source PNG exists, the bundle ships
# with the generic app icon (assemble-bundle.sh handles its absence).
#
# Usage:
#   make-icns.sh <source-1024.png> <output.icns>

set -euo pipefail

SRC="${1:?source 1024x1024 PNG required}"
OUT="${2:?output .icns path required}"

if [[ ! -f "$SRC" ]]; then
    echo "ERROR: source PNG not found: $SRC" >&2
    exit 1
fi

WORK="$(mktemp -d)/Indium.iconset"
mkdir -p "$WORK"

# Apple's required iconset sizes (1x + 2x for each base size).
gen() { sips -z "$2" "$2" "$SRC" --out "$WORK/icon_${1}.png" >/dev/null; }
gen 16x16        16
gen 16x16@2x     32
gen 32x32        32
gen 32x32@2x     64
gen 128x128      128
gen 128x128@2x   256
gen 256x256      256
gen 256x256@2x   512
gen 512x512      512
gen 512x512@2x   1024

iconutil -c icns "$WORK" -o "$OUT"
echo "==> Wrote $OUT"
