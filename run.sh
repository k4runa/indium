#!/bin/bash

set -e  # Exit immediately if any command fails

NO_RUN=0

for arg in "$@"; do
    case "$arg" in
        --clean)
            echo "--- Removing old build folder... ---"
            rm -rf build
            ;;
        --no-run)
            NO_RUN=1
            ;;
    esac
done

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

if [ "$NO_RUN" -eq 1 ]; then
    echo "--- Build complete (--no-run, skipping launch). ---"
else
    echo "--- Running... ---"
    ./Indium
fi
