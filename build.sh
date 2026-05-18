#!/bin/bash

#build script file for windows (mingw)
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"

if [ "$1" == "--clean" ]; then
    echo "Cleaning..."
    rm -rf "$ROOT/build"
    rm -f "$ROOT/CMakeCache.txt" "$ROOT/Makefile" "$ROOT/cmake_install.cmake"
    rm -rf "$ROOT/CMakeFiles"
fi

cmake -S "$ROOT" -B "$ROOT/build" -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

mingw32-make -C "$ROOT/build" -j$(nproc)

echo ""
echo "[OK] Build successful."
echo ""

if [ "$1" != "--no-run" ]; then
    "$ROOT/build/Indium.exe"
fi
