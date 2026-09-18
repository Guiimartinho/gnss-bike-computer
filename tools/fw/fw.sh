#!/usr/bin/env bash
# Comandos do firmware Zephyr no Git Bash, com o mesmo fluxo dos .bat da raiz.
#
#   tools/fw/fw.sh build [pristine]   compila zephyr_app (sysbuild)
#   tools/fw/fw.sh flash [keep]       grava no DK pelo J-Link
#   tools/fw/fw.sh recover            desbloqueia um chip protegido (apaga tudo)
#   tools/fw/fw.sh devices            lista as placas conectadas
#   tools/fw/fw.sh size               memória por região e maiores símbolos de RAM/flash
#
# Variáveis: BUILD_DIR (padrão: zephyr_app/build), BOARD (padrão:
# nrf52840dk/nrf52840; o nRF54LM20 DK é nrf54lm20dk/nrf54lm20a/cpuapp),
# FAMILY (família do nrfutil, deduzida da BOARD: nrf52 ou nrf54l),
# NRF_SERIAL (número de série do J-Link) e as de tools/fw/ncs_env.sh
# (NCS_ROOT, NCS_VERSION, NCS_TOOLCHAIN).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
# shellcheck source=tools/fw/ncs_env.sh
source "$ROOT/tools/fw/ncs_env.sh"

APP_DIR="$ROOT/zephyr_app"
BUILD_DIR="${BUILD_DIR:-$APP_DIR/build}"
BOARD="${BOARD:-nrf52840dk/nrf52840}"
case "$BOARD" in
    *nrf54l*) DEFAULT_FAMILY=nrf54l ;;
    *) DEFAULT_FAMILY=nrf52 ;;
esac
FAMILY="${FAMILY:-$DEFAULT_FAMILY}"

select_probe() {
    if [ -n "${NRF_SERIAL:-}" ]; then
        echo "--serial-number $NRF_SERIAL"
    else
        echo "--traits jlink"
    fi
}

firmware_hex() {
    local hex
    for hex in "$BUILD_DIR/merged.hex" "$BUILD_DIR/zephyr_app/zephyr/zephyr.hex" "$BUILD_DIR/zephyr/zephyr.hex"; do
        if [ -f "$hex" ]; then
            echo "$hex"
            return 0
        fi
    done
    echo "fw.sh: firmware não encontrado em $BUILD_DIR; rode tools/fw/fw.sh build" >&2
    return 1
}

firmware_elf() {
    local elf
    for elf in "$BUILD_DIR/zephyr_app/zephyr/zephyr.elf" "$BUILD_DIR/zephyr/zephyr.elf"; do
        if [ -f "$elf" ]; then
            echo "$elf"
            return 0
        fi
    done
    echo "fw.sh: zephyr.elf não encontrado em $BUILD_DIR" >&2
    return 1
}

cmd="${1:-build}"
shift || true

case "$cmd" in
    build)
        pristine=auto
        if [ "${1:-}" = "pristine" ]; then
            pristine=always
        fi
        # O west precisa rodar no drive do projeto (F:), não no do NCS (C:).
        cd "$APP_DIR"
        python -m west build -p "$pristine" -b "$BOARD" -d "$BUILD_DIR" --sysbuild "$APP_DIR"
        ;;
    flash)
        erase=ERASE_ALL
        if [ "${1:-}" = "keep" ]; then
            erase=ERASE_RANGES_TOUCHED_BY_FIRMWARE
        fi
        hex="$(firmware_hex)"
        echo "Gravando $hex (apagamento: $erase)"
        # shellcheck disable=SC2046
        nrfutil device program $(select_probe) --family "$FAMILY" --firmware "$hex" \
            --options "chip_erase_mode=$erase,verify=VERIFY_READ,reset=RESET_SYSTEM"
        ;;
    recover)
        # shellcheck disable=SC2046
        nrfutil device recover $(select_probe) --family "$FAMILY"
        ;;
    devices)
        nrfutil device list
        ;;
    size)
        elf="$(firmware_elf)"
        # head fecha o pipe cedo; sem isto o SIGPIPE do sort aborta o script.
        set +o pipefail
        arm-zephyr-eabi-size -A "$elf" | grep -E "^(section|text|rodata|datas|bss|noinit)"
        echo "--- maiores símbolos de RAM (bytes) ---"
        arm-zephyr-eabi-nm --size-sort -S -t d "$elf" | awk '$3 ~ /[bBdD]/ {print $2+0, $4}' | sort -rn | head -20
        echo "--- maiores símbolos de flash (bytes) ---"
        arm-zephyr-eabi-nm --size-sort -S -t d "$elf" | awk '$3 ~ /[tTrR]/ {print $2+0, $4}' | sort -rn | head -20
        ;;
    *)
        sed -n '2,13p' "$0"
        exit 2
        ;;
esac
