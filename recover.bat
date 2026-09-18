@echo off
REM ============================================================================
REM Recover script for nRF52840DK
REM Use this to unlock a protected device before flashing
REM ============================================================================

set TOOLCHAIN_ROOT=C:\ncs\toolchains\b8b84efebd

echo ============================================================================
echo Recovering nRF52840DK (removing readback protection)
echo ============================================================================
echo WARNING: This will erase ALL data on the device!
echo ============================================================================
echo.

"%TOOLCHAIN_ROOT%\nrfutil\bin\nrfutil.exe" device recover

if %ERRORLEVEL% EQU 0 (
    echo ============================================================================
    echo RECOVER SUCCESS! Device is now unlocked.
    echo ============================================================================
    echo You can now run flash.bat
) else (
    echo ============================================================================
    echo RECOVER FAILED!
    echo ============================================================================
)

pause
