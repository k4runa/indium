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

# Build raylib from source as a static library (once)
RAYLIB_PREFIX="$(pwd)/$RAYLIB_DIR/install"
if [ ! -f "$RAYLIB_PREFIX/lib/libraylib.a" ]; then
    echo "--- Building raylib (static)... ---"
    if [ ! -d "$RAYLIB_DIR" ]; then
        git clone --depth 1 --branch 5.5 https://github.com/raysan5/raylib.git "$RAYLIB_DIR"
    fi
    cmake -S "$RAYLIB_DIR" -B "$RAYLIB_DIR/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$RAYLIB_PREFIX" \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_EXAMPLES=OFF
    cmake --build "$RAYLIB_DIR/build" -j"$JOBS"
    cmake --install "$RAYLIB_DIR/build"
fi

echo "--- Configuring CMake ($BUILD_TYPE)... ---"
if ! cmake -S . -B "$BUILD_DIR" \
    -G "Ninja" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_C_COMPILER=gcc \
    -DCMAKE_CXX_COMPILER=g++ \
    -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++" \
    -DCMAKE_PREFIX_PATH="$RAYLIB_PREFIX"; then
    echo "--- Error: CMake configuration failed! ---"
    exit 1
fi

echo "--- Building ($JOBS cores)... ---"
if ! cmake --build "$BUILD_DIR" --parallel "$JOBS"; then
    echo "--- Error: Build failed! ---"
    exit 1
fi

if [ "$NO_RUN" -eq 1 ]; then
    echo "--- Build complete (--no-run, skipping launch). ---"
else
    echo "--- Running... ---"
    "$BUILD_DIR/Indium.exe"
fi
