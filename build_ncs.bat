@echo off
REM ============================================================================
REM Build do zero (pristine) do zephyr_app. Atalho para: build.bat pristine
REM ============================================================================
call "%~dp0build.bat" pristine
exit /b %ERRORLEVEL%
