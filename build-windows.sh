#!/usr/bin/env bash
set -e

# Requires MSYS2 with UCRT64 environment.
# Install dependencies once:
#   pacman -S mingw-w64-ucrt-x86_64-gcc \
#             mingw-w64-ucrt-x86_64-cmake \
#             mingw-w64-ucrt-x86_64-ninja \
#             mingw-w64-ucrt-x86_64-raylib

BUILD_DIR="build-windows"
BUILD_TYPE="${1:-Release}"

cmake -S . -B "$BUILD_DIR" \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++

cmake --build "$BUILD_DIR" --parallel

echo ""
echo "Build complete: $BUILD_DIR/Indium.exe"
