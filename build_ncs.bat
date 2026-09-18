@echo off
REM =============================================================================
REM Build script for gnss-bike-computer using NCS 3.1.0
REM =============================================================================

set NCS_VERSION=v3.1.0
set NCS_PATH=C:\ncs\%NCS_VERSION%
set TOOLCHAIN_PATH=C:\ncs\toolchains\b8b84efebd

REM Set Zephyr environment
set ZEPHYR_BASE=%NCS_PATH%\zephyr
set ZEPHYR_SDK_INSTALL_DIR=%TOOLCHAIN_PATH%\opt\zephyr-sdk
set ZEPHYR_TOOLCHAIN_VARIANT=zephyr

REM Add toolchain to PATH (prepend to override system tools)
set PATH=%TOOLCHAIN_PATH%\opt\bin;%TOOLCHAIN_PATH%\mingw64\bin;%TOOLCHAIN_PATH%\opt\zephyr-sdk\arm-zephyr-eabi\bin;%PATH%

REM Change to NCS directory (required for west to find modules)
cd /d %NCS_PATH%

REM Run west build
"%TOOLCHAIN_PATH%\opt\bin\python.exe" -m west build ^
    --build-dir "C:\Users\AORUS-Desktop\Documents\88.Personal\1.Projects\gnss-bike-computer\build" ^
    "C:\Users\AORUS-Desktop\Documents\88.Personal\1.Projects\gnss-bike-computer\zephyr_app" ^
    --pristine ^
    --board nrf52840dk/nrf52840 ^
    -- ^
    -DCMAKE_MAKE_PROGRAM="%TOOLCHAIN_PATH%\opt\bin\ninja.exe"

REM pause
