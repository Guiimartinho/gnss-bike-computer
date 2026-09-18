@echo off
setlocal
REM ============================================================================
REM Compila o zephyr_app (stravaV10 em Zephyr) com o nRF Connect SDK.
REM
REM   build.bat            build incremental em zephyr_app\build
REM   build.bat pristine   apaga zephyr_app\build e compila do zero
REM
REM Alvo: nrf52840dk/nrf52840 com os pinos da placa myStravaB
REM (zephyr_app\boards\nrf52840_strava.overlay), com sysbuild.
REM Versao do SDK e do toolchain: tools\fw\ncs_env.bat.
REM Defina BUILD_DIR para compilar em outra pasta.
REM Defina NOPAUSE=1 para nao esperar tecla no fim.
REM ============================================================================

call "%~dp0tools\fw\ncs_env.bat"
if errorlevel 1 goto :fail

set "APP_DIR=%~dp0zephyr_app"
if not defined BUILD_DIR set "BUILD_DIR=%APP_DIR%\build"
set "PRISTINE=auto"
if /i "%~1"=="pristine" set "PRISTINE=always"

echo ============================================================================
echo Compilando %APP_DIR%
echo SDK: NCS %NCS_VERSION% (toolchain %NCS_TOOLCHAIN%)
echo Build: %BUILD_DIR%
echo ============================================================================

REM O west precisa rodar no drive do projeto: com o NCS em C: e o projeto
REM em F:, rodar a partir de C: quebra o os.path.relpath do west.
cd /d "%APP_DIR%"
python -m west build -p %PRISTINE% -b nrf52840dk/nrf52840 -d "%BUILD_DIR%" --sysbuild "%APP_DIR%"
if errorlevel 1 goto :fail

echo ============================================================================
echo BUILD OK
echo Firmware: %BUILD_DIR%\zephyr_app\zephyr\zephyr.hex
echo ============================================================================
set "RC=0"
goto :end

:fail
echo ============================================================================
echo BUILD FALHOU
echo ============================================================================
set "RC=1"

:end
if not defined NOPAUSE pause
exit /b %RC%
