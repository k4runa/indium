#!/bin/bash

set -e  # Exit immediately if any command fails

# If --clean flag is provided, wipe the build folder; otherwise do an incremental build
if [ "$1" == "--clean" ]; then
    echo "--- Removing old build folder... ---"
    rm -rf build
fi

# Create build folder if it doesn't exist
mkdir -p build && cd build

# Platform-independent core count
if command -v nproc &>/dev/null; then
    JOBS=$(nproc)                          # Linux
elif command -v sysctl &>/dev/null; then
    JOBS=$(sysctl -n hw.logicalcpu)        # macOS and BSD
else
    JOBS=4                                 # Fallback
fi

echo "--- Configuring CMake... ---"
if ! cmake .. ; then
    echo "--- Error: CMake configuration failed! ---"
    exit 1
fi

echo "--- Building ($JOBS cores)... ---"
if ! make -j"$JOBS" ; then
    echo "--- Error: Build failed! ---"
    exit 1
fi

echo "--- Running... ---"
./Indium
