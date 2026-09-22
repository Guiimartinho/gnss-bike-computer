#!/usr/bin/env python3
"""Confere o mapa de pinos de uma placa do projeto.

Um arquivo de devicetree compila com um pino em dois lugares, com o SCL de
um TWIM num pino que não é de clock e com o pad do NFC ocupado. Nada disso
aparece no build: aparece na bancada, meses depois, com o osciloscópio na
mão. Este script lê a placa e diz antes.

O que ele confere:

1. **Pino em dois lugares.** Um pino só pode ter uma função. Os estados
   ``_default`` e ``_sleep`` do mesmo periférico não contam: são o mesmo
   pino em dois momentos.
2. **Pinos de clock.** No nRF54LM20A o SCL de um TWIM e o SCK de um SPIM só
   funcionam nos pinos da tabela 79 da ficha 4539_001 v1.0.
3. **Pads do NFC.** P1.01 e P1.02 saem do reset como antena NFC, com a
   função de GPIO desligada. Quem os usa precisa de ``nfct-pins-as-gpios``
   no nó ``uicr``, e aí não há NFC.
4. **Cristal.** P1.20 e P1.21 levam o cristal de 32,768 kHz.
5. **Limite da porta.** P0 tem 10 pinos, P1 tem 32, P2 tem 11 e P3 tem 13.
6. **Blocos seriais.** Cada bloco (00, 20 a 24, 30) tem **um** periférico:
   um ``i2c30`` e um ``uart30`` ligados ao mesmo tempo não existem no
   silício.
7. **Apelidos.** Os serviços acham o hardware por apelido; um que falte
   deixa o serviço sem o dispositivo, em silêncio.

Uso:

    python tools/fw/board_check.py                  # a placa do projeto
    python tools/fw/board_check.py <pasta-da-placa> # outra

Sai com 1 se achar problema.
"""

import re
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_BOARD = ROOT / "zephyr_app" / "boards" / "gnss" / "gnssbike"

# Tabela 79 da ficha do nRF54LM20A
CLOCK_PINS = {
    "P0.03", "P0.04", "P0.06", "P0.07",
    "P1.03", "P1.04", "P1.07", "P1.13", "P1.14", "P1.17", "P1.18", "P1.23", "P1.24",
    "P2.01", "P2.06",
    "P3.03", "P3.04",
}

NFC_PINS = {"P1.01", "P1.02"}
LFXO_PINS = {"P1.20", "P1.21"}

# ngpios de cada porta (zephyr/dts/vendor/nordic/nrf54lm20_a_b.dtsi)
PORT_PINS = {0: 10, 1: 32, 2: 11, 3: 13}

# Apelidos que os serviços procuram (docs/16, Serviços)
REQUIRED_ALIASES = [
    "gnss", "baro0", "imu0", "mag0", "light0",
    "backlight", "backlight-supply", "fuel-gauge0",
    "pmic", "pmic-charger", "pmic-regulators", "solar-charger",
    "watchdog0",
]


def pin_name(port, pin):
    return f"P{port}.{pin:02d}"


def read_all(board_dir):
    """Todo o texto da placa, .dts e .dtsi juntos."""
    text = ""
    for path in sorted(board_dir.glob("*.dts")) + sorted(board_dir.glob("*.dtsi")):
        text += path.read_text(encoding="utf-8") + "\n"
    return text


def collect_pins(text):
    """{pino: {dono}}, com os estados _sleep somados ao periférico."""
    used = defaultdict(set)

    # Blocos de pinctrl: o rótulo do grupo diz de quem é o pino
    for block in re.finditer(
        r"(\w+)_(default|sleep):\s*\w+\s*\{(.*?)\n\t\};", text, re.S
    ):
        owner, _state, body = block.group(1), block.group(2), block.group(3)
        for m in re.finditer(r"NRF_PSEL\((\w+),\s*(\d+),\s*(\d+)\)", body):
            fn, port, pin = m.group(1), int(m.group(2)), int(m.group(3))
            used[pin_name(port, pin)].add(f"{owner} {fn}")

    # GPIOs dos nós: <&gpioN pino ...>
    for m in re.finditer(r"([\w-]+)\s*=\s*<&gpio(\d)\s+(\d+)", text):
        prop, port, pin = m.group(1), int(m.group(2)), int(m.group(3))
        used[pin_name(port, pin)].add(prop)

    return used


def collect_blocks(text):
    """{bloco: {periférico ligado}} dos nós com status okay."""
    blocks = defaultdict(set)
    for m in re.finditer(r"&(uart|i2c|spi)(\d\d)\s*\{([^}]*)", text):
        kind, num, body = m.group(1), m.group(2), m.group(3)
        if 'status = "okay"' in body:
            blocks[num].add(f"{kind}{num}")
    return blocks


def main():
    board_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_BOARD
    if not board_dir.is_dir():
        print(f"board_check: {board_dir} não é uma pasta")
        return 1

    text = read_all(board_dir)
    if not text.strip():
        print(f"board_check: nenhum .dts em {board_dir}")
        return 1

    used = collect_pins(text)
    problems = []

    print(f"placa: {board_dir.relative_to(ROOT)}")
    print(f"{len(used)} pinos usados de {sum(PORT_PINS.values())}\n")

    # 1. pino em dois lugares
    for pin, owners in sorted(used.items()):
        if len(owners) > 1:
            problems.append(f"{pin} em dois lugares: {', '.join(sorted(owners))}")

    # 2. pinos de clock
    for pin, owners in sorted(used.items()):
        for owner in owners:
            if ("TWIM_SCL" in owner or "SPIM_SCK" in owner) and pin not in CLOCK_PINS:
                problems.append(f"{pin} leva {owner} e não é pino de clock (tabela 79)")

    # 3 e 4. pads reservados
    for pin in sorted(NFC_PINS & used.keys()):
        problems.append(f"{pin} é pad do NFC e sai do reset sem GPIO: {used[pin]}")
    for pin in sorted(LFXO_PINS & used.keys()):
        problems.append(f"{pin} leva o cristal de 32,768 kHz: {used[pin]}")

    # 5. limite da porta
    for pin in sorted(used):
        port, num = int(pin[1]), int(pin[3:])
        if num >= PORT_PINS[port]:
            problems.append(f"{pin} não existe: P{port} tem {PORT_PINS[port]} pinos")

    # 6. um periférico por bloco serial
    for block, periphs in sorted(collect_blocks(text).items()):
        if len(periphs) > 1:
            problems.append(
                f"bloco {block} com mais de um periférico: {', '.join(sorted(periphs))}"
            )

    # 7. apelidos
    alias_block = re.search(r"aliases\s*\{(.*?)\n\t\};", text, re.S)
    aliases = set()
    if alias_block:
        aliases = set(re.findall(r"([\w-]+)\s*=\s*&", alias_block.group(1)))
    for name in REQUIRED_ALIASES:
        if name not in aliases:
            problems.append(f"falta o apelido {name}, que um serviço procura")

    # o que sobrou, para quem for desenhar a próxima revisão
    free = []
    for port, count in sorted(PORT_PINS.items()):
        for num in range(count):
            name = pin_name(port, num)
            if name not in used and name not in NFC_PINS and name not in LFXO_PINS:
                free.append(name)
    print(f"livres ({len(free)}): {', '.join(free)}")
    print(f"de clock ainda livres: {', '.join(sorted(CLOCK_PINS - used.keys()))}\n")

    if problems:
        print(f"{len(problems)} problema(s):")
        for p in problems:
            print(f"  {p}")
        return 1

    print("mapa de pinos coerente")
    return 0


if __name__ == "__main__":
    sys.exit(main())
