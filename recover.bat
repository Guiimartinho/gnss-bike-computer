@echo off
setlocal
REM ============================================================================
REM Recupera um nRF52840 protegido (APPROTECT): apaga TODA a flash e a UICR
REM e libera a porta de debug. Use antes do flash.bat quando a gravacao falhar
REM por protecao.
REM
REM Com mais de um J-Link conectado, defina NRF_SERIAL.
REM Defina NOPAUSE=1 para nao esperar tecla no fim.
REM ============================================================================

call "%~dp0tools\fw\ncs_env.bat"
if errorlevel 1 goto :fail

set "SELECT=--traits jlink"
if defined NRF_SERIAL set "SELECT=--serial-number %NRF_SERIAL%"

echo ============================================================================
echo ATENCAO: isto apaga TODO o conteudo do chip.
echo ============================================================================
nrfutil device recover %SELECT% --family nrf52
if errorlevel 1 goto :fail

echo ============================================================================
echo RECOVER OK: a placa esta desbloqueada. Rode flash.bat.
echo ============================================================================
set "RC=0"
goto :end

:fail
echo ============================================================================
echo RECOVER FALHOU
echo ============================================================================
set "RC=1"

:end
if not defined NOPAUSE pause
exit /b %RC%
