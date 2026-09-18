@echo off
REM ============================================================================
REM Build script for stravaV10 - GNSS Bike Computer
REM Target: nRF52840DK
REM ============================================================================

set TOOLCHAIN_ROOT=C:\ncs\toolchains\b8b84efebd
set ZEPHYR_BASE=C:\ncs\v3.1.0\zephyr
set ZEPHYR_TOOLCHAIN_VARIANT=zephyr
set ZEPHYR_SDK_INSTALL_DIR=%TOOLCHAIN_ROOT%\opt\zephyr-sdk

set PATH=%TOOLCHAIN_ROOT%\opt\bin;%TOOLCHAIN_ROOT%\mingw64\bin;%TOOLCHAIN_ROOT%\bin;%TOOLCHAIN_ROOT%\opt\zephyr-sdk\arm-zephyr-eabi\bin;%PATH%

set PROJECT_DIR=%~dp0zephyr_app
set BUILD_DIR=%PROJECT_DIR%\build

echo ============================================================================
echo Building stravaV10 for nRF52840DK
echo ============================================================================
echo Project: %PROJECT_DIR%
echo Build:   %BUILD_DIR%
echo ============================================================================

cd /d C:\ncs\v3.1.0

"%TOOLCHAIN_ROOT%\opt\bin\python.exe" -m west build -b nrf52840dk/nrf52840 -d "%BUILD_DIR%" --no-sysbuild "%PROJECT_DIR%" -- -DCMAKE_MAKE_PROGRAM="%TOOLCHAIN_ROOT%\opt\bin\ninja.exe"

if %ERRORLEVEL% EQU 0 (
    echo ============================================================================
    echo BUILD SUCCESS!
    echo ============================================================================
    echo Firmware: %BUILD_DIR%\zephyr\zephyr.hex
    echo ============================================================================
) else (
    echo ============================================================================
    echo BUILD FAILED!
    echo ============================================================================
)

pause
