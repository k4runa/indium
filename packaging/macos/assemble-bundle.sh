#!/usr/bin/env bash
#
# Assemble a self-contained Indium.app from a plain CMake build.
#
# The bundle is built by ALLOWLIST — only named items are copied into an empty
# skeleton, so repo content (website/, docs/, tests/, assets/, .git/, build/, …)
# is excluded by construction. The complete contents are:
#
#   Indium.app/Contents/
#     Info.plist                  # generated from Info.plist.template
#     MacOS/Indium                # editor binary (statically linked raylib)
#     MacOS/IndiumPlayer          # player runtime (File > Export Game needs it)
#     MacOS/sdk/{core,2D,include} # script-compile HEADERS ONLY (.h/.hpp/.inl)
#     Resources/Indium.icns       # icon (only if present)
#
# Usage:
#   assemble-bundle.sh <version> <build-dir> <repo-root> <output-app> [icns-path]
#
# Env:
#   RAYLIB_INCLUDE_DIR   dir holding raylib.h (default: /usr/local/include)

set -euo pipefail

VERSION="${1:?version required (e.g. v1.0.20 or 1.0.20)}"
BUILD_DIR="${2:?build dir required}"
REPO_ROOT="${3:?repo root required}"
APP="${4:?output .app path required}"
ICNS="${5:-}"
RAYLIB_INCLUDE_DIR="${RAYLIB_INCLUDE_DIR:-/usr/local/include}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Strip a leading 'v' for the plist's version strings.
PLIST_VERSION="${VERSION#v}"

echo "==> Assembling $APP (version $PLIST_VERSION)"
rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

# ── Binaries ──────────────────────────────────────────────────────────────
for bin in Indium IndiumPlayer; do
    if [[ ! -x "$BUILD_DIR/$bin" ]]; then
        echo "ERROR: $BUILD_DIR/$bin missing or not executable" >&2
        exit 1
    fi
    cp "$BUILD_DIR/$bin" "$APP/Contents/MacOS/$bin"
done

# ── Info.plist ────────────────────────────────────────────────────────────
sed "s|__VERSION__|$PLIST_VERSION|g" \
    "$SCRIPT_DIR/Info.plist.template" > "$APP/Contents/Info.plist"

# ── Script SDK (headers only) ─────────────────────────────────────────────
# Rule: copy every .h/.hpp/.inl, skip .cpp. The macOS script compile uses
# `-dynamiclib -undefined dynamic_lookup` (no -l/-L), so only headers are
# needed; dropping imgui_*.cpp / rlImGui.cpp is the size win.
SDK="$APP/Contents/MacOS/sdk"
copy_headers() {
    # copy_headers <src-dir> <dst-dir>
    rsync -a \
        --include='*/' \
        --include='*.h' --include='*.hpp' --include='*.inl' \
        --exclude='*' \
        "$1/" "$2/"
}
mkdir -p "$SDK/core" "$SDK/2D" "$SDK/include"
copy_headers "$REPO_ROOT/core"    "$SDK/core"
copy_headers "$REPO_ROOT/2D"      "$SDK/2D"
copy_headers "$REPO_ROOT/include" "$SDK/include"

# raylib's public headers (resolved first via appDir/sdk/include at runtime).
for h in raylib.h raymath.h rlgl.h; do
    if [[ -f "$RAYLIB_INCLUDE_DIR/$h" ]]; then
        cp "$RAYLIB_INCLUDE_DIR/$h" "$SDK/include/$h"
    else
        echo "WARNING: $RAYLIB_INCLUDE_DIR/$h not found — scripting may break" >&2
    fi
done

# Sanity: GetScriptSdkRoot() keys off sdk/core existing.
if [[ ! -d "$SDK/core" ]] || [[ -z "$(find "$SDK/core" -name '*.hpp' -print -quit)" ]]; then
    echo "ERROR: sdk/core has no headers — script compilation would fail" >&2
    exit 1
fi

# ── Icon (optional) ───────────────────────────────────────────────────────
if [[ -n "$ICNS" && -f "$ICNS" ]]; then
    cp "$ICNS" "$APP/Contents/Resources/Indium.icns"
    echo "==> Bundled icon: $ICNS"
else
    echo "==> No .icns provided — shipping with the generic app icon"
fi

echo "==> Bundle contents:"
find "$APP/Contents" -maxdepth 2 -mindepth 1 | sort | sed 's/^/    /'
du -sh "$APP" || true
echo "==> Done: $APP"
