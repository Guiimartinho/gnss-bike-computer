#!/usr/bin/env python3
"""Confere a lista de nós do esquemático contra o devicetree da placa.

Um esquemático e um devicetree são a mesma informação escrita duas vezes.
Escritos duas vezes, eles divergem: alguém move um sinal de pino no
firmware, o esquemático fica para trás, e a divergência só aparece quando
a placa chega e o sinal não está onde o desenho diz.

Este script lê ``03-netlist.md`` e os arquivos de placa de
``zephyr_app/boards/gnss/gnssbike/`` e reclama quando os dois discordam.

O que ele confere:

1. **Pino do firmware que o esquemático não tem.** O devicetree usa um
   pino, a lista de nós não o menciona: o esquemático está incompleto e a
   placa sairia sem a ligação.
2. **Pino do esquemático que o firmware não usa.** O contrário: ou o
   firmware perdeu uma função, ou o esquemático inventou uma. Um pino
   marcado como reservado na lista não conta.
3. **Pino em dois nós.** Um pino com dois nomes de sinal é curto-circuito
   no desenho.
4. **Pino que não existe.** Fora do tamanho da porta do nRF54LM20A.
5. **Nome de nó repetido.** Dois nós com o mesmo nome são o mesmo nó, e
   quase nunca é isso que se quis dizer.

Uso:

    python hardware_gnssbike/net_check.py

Sai com 1 se achar problema.
"""

import io
import re
import sys
from collections import defaultdict
from pathlib import Path

# O console do Windows abre em cp1252 e engole os acentos
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
else:  # pragma: no cover - Python antigo
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding="utf-8", errors="replace")

ROOT = Path(__file__).resolve().parents[1]
NETLIST = Path(__file__).resolve().parent / "03-netlist.md"
BOARD = ROOT / "zephyr_app" / "boards" / "gnss" / "gnssbike"

# ngpios de cada porta (zephyr/dts/vendor/nordic/nrf54lm20_a_b.dtsi)
PORT_PINS = {0: 10, 1: 32, 2: 11, 3: 13}

PIN_RE = re.compile(r"\bP([0-3])\.(\d{2})\b")


def pin_name(port, pin):
    return f"P{port}.{pin:02d}"


def board_pins():
    """{pino: {dono}} do devicetree, como o board_check.py os lê."""
    text = ""
    for path in sorted(BOARD.glob("*.dts")) + sorted(BOARD.glob("*.dtsi")):
        text += path.read_text(encoding="utf-8") + "\n"

    used = defaultdict(set)

    for block in re.finditer(
        r"(\w+)_(default|sleep):\s*\w+\s*\{(.*?)\n\t\};", text, re.S
    ):
        owner, _state, body = block.group(1), block.group(2), block.group(3)
        for m in re.finditer(r"NRF_PSEL\((\w+),\s*(\d+),\s*(\d+)\)", body):
            fn, port, pin = m.group(1), int(m.group(2)), int(m.group(3))
            used[pin_name(port, pin)].add(f"{owner} {fn}")

    for m in re.finditer(r"([\w-]+)\s*=\s*<&gpio(\d)\s+(\d+)", text):
        prop, port, pin = m.group(1), int(m.group(2)), int(m.group(3))
        used[pin_name(port, pin)].add(prop)

    return used


def netlist_rows():
    """[(nó, pino, linha)] das tabelas que têm coluna 'Pino do MCU'."""
    rows = []
    header_has_pin = False

    for lineno, line in enumerate(NETLIST.read_text(encoding="utf-8").splitlines(), 1):
        line = line.strip()
        if not line.startswith("|"):
            continue

        cells = [c.strip() for c in line.strip("|").split("|")]
        if len(cells) < 2:
            continue

        # cabeçalho: liga ou desliga a leitura da tabela em curso
        if cells[0] in ("Nó", "No"):
            header_has_pin = cells[1].startswith("Pino do MCU")
            continue
        if set(cells[0]) <= set("- :"):
            continue
        if not header_has_pin:
            continue

        node = cells[0].strip("`")
        pin_cell = cells[1]
        m = PIN_RE.search(pin_cell)
        if m:
            rows.append((node, pin_name(int(m.group(1)), int(m.group(2))), lineno))
        elif "não populado" in pin_cell or "reservado" in pin_cell:
            rows.append((node, None, lineno))

    return rows


def main():
    if not NETLIST.is_file():
        print(f"net_check: {NETLIST} não existe")
        return 1
    if not BOARD.is_dir():
        print(f"net_check: {BOARD} não existe")
        return 1

    dt = board_pins()
    rows = netlist_rows()

    # um pino é "reservado" quando a lista o marca assim na coluna da nota
    reserved = set()
    text = NETLIST.read_text(encoding="utf-8")
    for line in text.splitlines():
        if "reservado" in line or "não populado" in line:
            m = PIN_RE.search(line)
            if m:
                reserved.add(pin_name(int(m.group(1)), int(m.group(2))))

    print(f"esquemático: {NETLIST.relative_to(ROOT)}")
    print(f"placa:       {BOARD.relative_to(ROOT)}")

    net_pins = defaultdict(list)
    names = defaultdict(int)
    for node, pin, lineno in rows:
        names[node] += 1
        if pin:
            net_pins[pin].append((node, lineno))

    print(f"{len(net_pins)} pinos no esquemático, {len(dt)} no devicetree\n")

    problems = []

    # 1. pino do firmware que o esquemático não tem
    for pin in sorted(set(dt) - set(net_pins)):
        problems.append(
            f"{pin} está no devicetree ({', '.join(sorted(dt[pin]))}) "
            f"e não aparece na lista de nós"
        )

    # 2. pino do esquemático que o firmware não usa
    for pin in sorted(set(net_pins) - set(dt)):
        if pin in reserved:
            continue
        nodes = ", ".join(n for n, _ in net_pins[pin])
        problems.append(
            f"{pin} está na lista de nós ({nodes}) e o devicetree não o usa"
        )

    # 3. pino em dois nós
    for pin, entries in sorted(net_pins.items()):
        if len(entries) > 1:
            where = ", ".join(f"{n} (linha {ln})" for n, ln in entries)
            problems.append(f"{pin} em dois nós: {where}")

    # 4. pino que não existe
    for pin in sorted(net_pins):
        port, num = int(pin[1]), int(pin[3:])
        if num >= PORT_PINS[port]:
            problems.append(f"{pin} não existe: P{port} tem {PORT_PINS[port]} pinos")

    # 5. nome de nó repetido
    for node, count in sorted(names.items()):
        if count > 1:
            problems.append(f"nó {node} aparece {count} vezes")

    if problems:
        print("problemas:")
        for p in problems:
            print(f"  - {p}")
        return 1

    if reserved:
        print(f"reservados no firmware: {', '.join(sorted(reserved))}")
    print("esquemático e devicetree batem")
    return 0


if __name__ == "__main__":
    sys.exit(main())
