#!/usr/bin/env bash
set -e

# Requires MSYS2 with UCRT64 environment.
# Install dependencies once:
#   pacman -S mingw-w64-ucrt-x86_64-gcc \
#             mingw-w64-ucrt-x86_64-cmake \
#             mingw-w64-ucrt-x86_64-ninja \
#             mingw-w64-ucrt-x86_64-raylib

BUILD_DIR="build-windows"
BUILD_TYPE="Release"
NO_RUN=0
RAYLIB_DIR="_raylib"

for arg in "$@"; do
    case "$arg" in
        --clean)
            echo "--- Removing old $BUILD_DIR folder... ---"
            rm -rf "$BUILD_DIR"
            ;;
        --no-run)
            NO_RUN=1
            ;;
        Debug|Release|MinSizeRel|RelWithDebInfo)
            BUILD_TYPE="$arg"
            ;;
        *)
            echo "Unknown option: $arg"
            echo "Usage: $0 [Debug|Release|MinSizeRel|RelWithDebInfo] [--clean] [--no-run]"
            exit 1
            ;;
    esac
done

# Platform-independent core count
if command -v nproc &>/dev/null; then
    JOBS=$(nproc)
elif command -v sysctl &>/dev/null; then
    JOBS=$(sysctl -n hw.logicalcpu)
else
    JOBS=4
fi

# Build raylib from source as a SHARED library (once).
#
# Why shared and not static: the editor supports C++ gameplay scripts compiled
# into plugin DLLs at runtime. On Windows a plugin DLL and the host exe must
# share ONE C++ runtime and ONE copy of raylib so that cross-module RTTI works
# (the engine does dynamic_cast<NativeScript*> on objects created inside the
# script DLL). Statically linking raylib / libstdc++ into the exe would give the
# script DLL a *separate* runtime and break those casts silently at runtime.
# Shipping raylib + the MinGW runtime as DLLs is the standard plugin layout.
RAYLIB_PREFIX="$(pwd)/$RAYLIB_DIR/install"
if [ ! -f "$RAYLIB_PREFIX/bin/libraylib.dll" ]; then
    echo "--- Building raylib (shared)... ---"
    rm -rf "$RAYLIB_DIR/build"   # ensure a clean switch if a static build exists
    if [ ! -d "$RAYLIB_DIR" ]; then
        git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
    fi
    cmake -S "$RAYLIB_DIR" -B "$RAYLIB_DIR/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$RAYLIB_PREFIX" \
        -DBUILD_SHARED_LIBS=ON \
        -DBUILD_EXAMPLES=OFF
    cmake --build "$RAYLIB_DIR/build" -j"$JOBS"
    cmake --install "$RAYLIB_DIR/build"
fi

echo "--- Configuring CMake ($BUILD_TYPE)... ---"
# No -static: exe and script DLLs must share the MinGW C++ runtime (see above).
if ! cmake -S . -B "$BUILD_DIR" \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_PREFIX_PATH="$RAYLIB_PREFIX"; then
    echo "--- Error: CMake configuration failed! ---"
    exit 1
fi

echo "--- Building ($JOBS cores)... ---"
if ! cmake --build "$BUILD_DIR" --parallel "$JOBS"; then
    echo "--- Error: Build failed! ---"
    exit 1
fi

# --- Bundle runtime DLLs next to the exe (automatic, never manual) ---
# raylib.dll + its import lib (scripts link against the latter), plus every
# MinGW runtime DLL the exe actually depends on (discovered via ldd, so we copy
# exactly what's needed instead of hardcoding fragile names).
echo "--- Bundling runtime DLLs... ---"
cp "$RAYLIB_PREFIX/bin/libraylib.dll"    "$BUILD_DIR/"
cp "$RAYLIB_PREFIX/lib/libraylib.dll.a"  "$BUILD_DIR/"   # import lib for script linking
ldd "$BUILD_DIR/Indium.exe" \
    | grep -iE '=> /(ucrt64|mingw64)/bin/' \
    | awk '{print $3}' \
    | while read -r dll; do cp -u "$dll" "$BUILD_DIR/"; done

if [ "$NO_RUN" -eq 1 ]; then
    echo "--- Build complete (--no-run, skipping launch). ---"
else
    echo "--- Running... ---"
    "$BUILD_DIR/Indium.exe"
fi
