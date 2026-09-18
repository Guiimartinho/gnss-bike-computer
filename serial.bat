@echo off
REM ============================================================================
REM Serial Monitor for stravaV10 - GNSS Bike Computer
REM Target: nRF52840DK
REM ============================================================================

set TOOLCHAIN_ROOT=C:\ncs\toolchains\b8b84efebd
set COMPORT=COM11
set BAUDRATE=115200

echo ============================================================================
echo Serial Monitor - stravaV10
echo ============================================================================
echo Port: %COMPORT%
echo Baud: %BAUDRATE%
echo Press Ctrl+C to exit
echo ============================================================================
echo.

"%TOOLCHAIN_ROOT%\opt\bin\python.exe" -m serial.tools.miniterm %COMPORT% %BAUDRATE%

pause