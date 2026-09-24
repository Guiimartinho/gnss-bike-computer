#!/usr/bin/env python3
"""Which sheet each part belongs to, and how each net leaves its sheet.

A schematic of this size is not drawn on one page. It is drawn the way a
board is built: one sheet per block, a root sheet that shows the blocks and
the signals between them, supply rails carried by power symbols instead of by
lines, and only the signals that really cross a block boundary leaving the
sheet at all.

Three kinds of net, and each is drawn differently:

  RAIL     a supply or a ground. It gets a power symbol wherever it touches a
           pin. KiCad joins every power symbol of the same name into one net
           across the whole design, so the ground does not become eighty
           lines converging on one rail.
  ENTRE    a signal whose pins sit on more than one sheet. It gets a
           hierarchical label on each sheet and a pin on each block of the
           root, and the root draws the line between the blocks.
  DENTRO   a signal whose pins are all on one sheet. It is only a wire.
"""

from __future__ import annotations

import nets as N
import parts as P

# The six sheets of 01-esquematico.md, in the order they are read.
FOLHAS: list[tuple[str, str, str]] = [
    ("1 Energia", "folha1-energia.kicad_sch", "2"),
    ("2 MCU e depuracao", "folha2-mcu.kicad_sch", "3"),
    ("3 GNSS", "folha3-gnss.kicad_sch", "4"),
    ("4 Display e luz", "folha4-display.kicad_sch", "5"),
    ("5 Memoria e sensores", "folha5-memoria-sensores.kicad_sch", "6"),
    ("6 Interface", "folha6-interface.kicad_sch", "7"),
]

# Reference prefixes to sheet, by the first digit of the number: 1xx is the
# power sheet, 2xx the MCU, and so on, which is how 05-materiais.md numbers
# them. This is the only rule; nothing is placed by hand.
def sheet_of(ref: str) -> str:
    digitos = "".join(c for c in ref if c.isdigit())
    n = int(digitos[0]) if digitos else 1
    return FOLHAS[min(max(n, 1), 6) - 1][0]


# Supplies and grounds. Everything here is drawn with power symbols.
TRILHOS: dict[str, bool] = {   # name -> is it a ground
    "GND": True,
    "VBUS": False, "VBUSOUT": False, "VBAT_CELULA": False, "VBAT": False,
    "VBAT_SYS": False, "VSYS": False, "1V8": False, "1V8_BLOCO": False,
    "1V8_GNSS": False, "3V0": False, "3V0_MOD": False, "3V0_SENS": False,
    "SD3V0": False, "SD3V0_FLASH": False, "3V3BL": False, "VBCKP": False,
    "VINT": False,
}


def classificar() -> tuple[dict[str, str], dict[str, set[str]]]:
    """Give each net its kind, and for the ones that leave, which sheets."""
    tipo: dict[str, str] = {}
    folhas_do_no: dict[str, set[str]] = {}
    for nome, pinos in N.NETS.items():
        folhas = {sheet_of(ref) for ref, _pin in pinos}
        folhas_do_no[nome] = folhas
        if nome in TRILHOS:
            tipo[nome] = "RAIL"
        elif len(folhas) > 1:
            tipo[nome] = "ENTRE"
        else:
            tipo[nome] = "DENTRO"
    return tipo, folhas_do_no


def por_folha() -> dict[str, list[str]]:
    out: dict[str, list[str]] = {nome: [] for nome, _f, _p in FOLHAS}
    for ref in P.PARTS:
        out[sheet_of(ref)].append(ref)
    for k in out:
        out[k].sort(key=lambda r: (-len(P.PARTS[r].pins), r))
    return out


if __name__ == "__main__":
    tipo, folhas = classificar()
    dist = por_folha()
    print(f"{len(P.PARTS)} posicoes em {len(FOLHAS)} folhas")
    for nome, _f, _p in FOLHAS:
        print(f"  {nome}: {len(dist[nome])} pecas")
    for t in ("RAIL", "ENTRE", "DENTRO"):
        quais = [n for n, k in tipo.items() if k == t]
        print(f"{t}: {len(quais)}")
        if t == "ENTRE":
            for n in sorted(quais):
                print(f"    {n}: {', '.join(sorted(folhas[n]))}")
