@echo off
setlocal

if "%1"=="--clean" (
    echo Cleaning build directory...
    rmdir /s /q build 2>nul
)

if not exist build mkdir build

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    echo [ERROR] CMake configuration failed.
    exit /b 1
)

cmake --build build --config Release -j %NUMBER_OF_PROCESSORS%
if errorlevel 1 (
    echo [ERROR] Build failed.
    exit /b 1
)

echo.
echo [OK] Build successful.
echo.

if "%1"=="--no-run" exit /b 0

if exist build\Release\Indium.exe (
    build\Release\Indium.exe
) else if exist build\Indium.exe (
    build\Indium.exe
) else (
    echo [WARN] Executable not found - check build output above.
    exit /b 1
)
