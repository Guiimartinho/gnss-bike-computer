@echo off
REM ============================================================================
REM Flash script for stravaV10 - GNSS Bike Computer
REM Target: nRF52840DK
REM ============================================================================

set TOOLCHAIN_ROOT=C:\ncs\toolchains\b8b84efebd
set FIRMWARE=%~dp0zephyr_app\build\zephyr\zephyr.hex

echo ============================================================================
echo Flashing stravaV10 to nRF52840DK
echo ============================================================================
echo Firmware: %FIRMWARE%
echo ============================================================================

REM Check if firmware exists
if not exist "%FIRMWARE%" (
    echo ERROR: Firmware not found!
    echo Run build.bat first to compile the project.
    pause
    exit /b 1
)

REM List connected devices
echo.
echo Detecting devices...
"%TOOLCHAIN_ROOT%\nrfutil\bin\nrfutil.exe" device list

echo.
echo Flashing firmware...
"%TOOLCHAIN_ROOT%\nrfutil\bin\nrfutil.exe" device program --firmware "%FIRMWARE%" --options chip_erase_mode=ERASE_ALL,verify=VERIFY_READ,reset=RESET_SYSTEM

if %ERRORLEVEL% EQU 0 (
    echo ============================================================================
    echo FLASH SUCCESS!
    echo ============================================================================
) else (
    echo ============================================================================
    echo FLASH FAILED!
    echo ============================================================================
    echo If device is protected, run recover.bat first
)

pause
