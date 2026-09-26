#!/usr/bin/env python3
"""Functional blocks of the schematic: who goes with whom, and where.

A readable schematic is drawn the way the circuit works, not the way the
netlist lists it. This module turns each sheet of sheets.FOLHAS into a row
of functional blocks - "USB-C and its protection", "nPM1300: input, charge
and the two bucks", "solar harvesting" - and lays each block out around its
own anchor: the chip or connector in the middle, every passive next to the
pin it serves, on the side that pin faces, and the decoupling in a shelf
underneath. The blocks then go on the page left to right in the order the
signal flows, each inside a dashed rectangle with its title.

What decides membership, in this order:

  1. the BLOCOS table below: the anchors of each block, by hand, because
     "what is a functional block" is a design statement, not a graph
     property;
  2. make_pcb.JUNTO, the table that already says which chip each passive
     belongs to on the board - a decoupling capacitor touches only rails,
     so no netlist walk could ever tell whose it is;
  3. the netlist, rails excluded: a part joins the block of the nearest
     anchor it reaches through signal nets (the divider of the thermal
     cut-off reaches the comparator in one hop, the NTC in two).

Conventions applied, and where they come from: signal flow left to right
and blocks in that order; supplies drawn as power symbols above the pin and
grounds below (KiCad Eeschema manual, "power symbols make global
connections"); a net that leaves its block is a local label, never a wire
across the page (same manual: "local labels connect items located in the
same sheet"; SparkFun, "give a net a name and label it, rather than
routing a wire all over the schematic"); a net that leaves the sheet is a
hierarchical label at the pin; every symbol and wire on the 50 mil grid
(manual: "wires connect ... only if their ends coincide exactly").
"""

from __future__ import annotations

import math
from dataclasses import dataclass, field

import nets as N
import parts as P
import sheets as S
from sch_lib import GRID, snap

# sheet -> list of (title, anchors). The order is the order on the page.
BLOCOS: dict[str, list[tuple[str, list[str]]]] = {
    "1 Energia": [
        ("USB-C e protecao", ["J101", "D101", "D102"]),
        ("nPM1300: entrada, carga e os dois bucks", ["U101"]),
        ("Medidor de bateria e celula", ["U102", "J102"]),
        ("Backup do receptor GNSS", ["U104"]),
        ("Colheita solar ADP5091", ["J103", "U103"]),
        ("Corte termico da carga solar", ["U105"]),
    ],
    "2 MCU e depuracao": [
        ("Modulo ME54BS13 (nRF54LM20A)", ["U201"]),
        ("Depuracao SWD e console", ["J201"]),
    ],
    "3 GNSS": [
        ("Receptor MAX-F10S", ["U301"]),
        ("Antena de chip e rede pi", ["E301", "JP301"]),
    ],
    "4 Display e luz": [
        ("Painel JDI e conector de sinal", ["J401", "DS401"]),
        ("Luz do painel", ["J402", "Q401"]),
    ],
    "5 Memoria e sensores": [
        ("Flash NOR MX25R6435F", ["U501"]),
        ("IMU BMI270", ["U503"]),
        ("Barometro BMP585", ["U502"]),
        ("Magnetometro MMC5603", ["U504"]),
        ("Luz ambiente OPT3001", ["U505"]),
    ],
    "6 Interface": [
        ("Teclas", ["SW601", "SW602", "SW603"]),
        ("LED RGB", ["D601", "Q601", "Q602", "Q603"]),
        ("Buzzer piezo", ["LS601"]),
    ],
}

GAP = 2 * GRID            # pin tip to neighbour's pin tip
FOLGA = 2 * GRID          # between two bodies of the same block
MARGEM_BLOCO = 5 * GRID   # inside the dashed box, room for power symbols
ENTRE_BLOCOS = 6 * GRID   # between two dashed boxes


@dataclass
class Bloco:
    titulo: str
    ancoras: list[str]
    membros: list[str] = field(default_factory=list)   # anchors first
    # bounding box on the sheet, set by posicionar()
    caixa: tuple[float, float, float, float] = (0.0, 0.0, 0.0, 0.0)


def _vizinhos_por_sinal(refs: list[str]) -> dict[str, set[str]]:
    """Who touches whom through a net that is not a rail, on this sheet."""
    conjunto = set(refs)
    viz: dict[str, set[str]] = {r: set() for r in refs}
    for nome, pinos in N.NETS.items():
        if nome in S.TRILHOS:
            continue
        na_folha = {r for r, _p in pinos} & conjunto
        for a in na_folha:
            viz[a] |= na_folha - {a}
    return viz


def blocos_da_folha(nome: str, refs: list[str]) -> list[Bloco]:
    """Split the sheet's parts into its functional blocks."""
    from make_pcb import JUNTO
    blocos = [Bloco(t, [a for a in ancs if a in refs])
              for t, ancs in BLOCOS.get(nome, [])]
    blocos = [b for b in blocos if b.ancoras]
    dono: dict[str, Bloco] = {}
    for b in blocos:
        for a in b.ancoras:
            dono[a] = b
    viz = _vizinhos_por_sinal(refs)

    # 1. make_pcb.JUNTO and DECOPLA: the board already knows whose each
    #    passive is - the decoupling table by capacitor, JUNTO by anchor
    from make_pcb import DECOPLA
    for r in refs:
        if r in dono:
            continue
        anc = S.SEGUE.get(r) or DECOPLA.get(r) or JUNTO.get(r)
        # follow the chain (C132 -> U106 -> U301) until an anchor of a block
        vistos = set()
        while anc and anc not in dono and anc not in vistos:
            vistos.add(anc)
            anc = S.SEGUE.get(anc) or DECOPLA.get(anc) or JUNTO.get(anc)
        if anc in dono:
            dono[r] = dono[anc]

    # 2. the netlist: nearest anchor by hops through signal nets
    fila = [(a, 0) for b in blocos for a in b.ancoras]
    dist: dict[str, int] = {a: 0 for a, _d in fila}
    origem: dict[str, Bloco] = {a: dono[a] for a, _d in fila}
    while fila:
        atual, d = fila.pop(0)
        for v in sorted(viz[atual]):
            if v not in dist:
                dist[v] = d + 1
                origem[v] = origem[atual]
                fila.append((v, d + 1))
    for r in refs:
        if r not in dono and r in origem:
            dono[r] = origem[r]

    # 2b. a part that touches only rails, but one of them is a rail of ONE
    #     block - 3V0_GNSS, 3V0_MOD, SD3V0_FLASH, VBCKP - joins the block of
    #     the anchor on that rail. GND and the rails that feed everything say
    #     nothing about membership and are skipped.
    GLOBAIS = {"GND", "3V0", "VSYS", "VBUS", "VBUSOUT", "VBAT", "VBAT_SYS",
               "VBAT_CELULA"}
    for r in refs:
        if r in dono:
            continue
        contagem: dict[int, int] = {}
        por_id: dict[int, Bloco] = {}
        for nome_n, pinos in N.NETS.items():
            if nome_n not in S.TRILHOS or nome_n in GLOBAIS:
                continue
            if not any(rr == r for rr, _p in pinos):
                continue
            for rr, _p in pinos:
                if rr in dono and rr != r:
                    b = dono[rr]
                    contagem[id(b)] = contagem.get(id(b), 0) + 1
                    por_id[id(b)] = b
        if contagem:
            dono[r] = por_id[max(contagem, key=lambda k: contagem[k])]

    # 3. whatever is left: a block of its own, so nothing disappears
    sobras = [r for r in refs if r not in dono]
    if sobras:
        b = Bloco("Outros", [])
        blocos.append(b)
        for r in sobras:
            dono[r] = b

    for b in blocos:
        b.membros = list(b.ancoras) + sorted(
            (r for r in refs if dono[r] is b and r not in b.ancoras),
            key=lambda r: (dist.get(r, 9), -len(P.PARTS[r].pins), r))
    return blocos


# ------------------------------------------------------------------ layout
def _caixa(ref: str, x: float, y: float) -> tuple[float, float, float, float]:
    """Body box of a part placed at (x, y), pins included."""
    p = P.PARTS[ref]
    old = (p.x, p.y)
    p.x, p.y = x, y
    bx0, by0, bx1, by1 = p.box()
    esq, dire = p.avanco_dos_pinos()
    if p.kicad or not __import__("sch_lib").classe_do_simbolo(p):
        # pins stick out of the body on every side that has them
        pl = p.pin_local()
        for _n, (px, py, _a) in pl.items():
            bx0 = min(bx0, x + px)
            bx1 = max(bx1, x + px)
            by0 = min(by0, y - py)
            by1 = max(by1, y - py)
    else:
        bx0 -= esq
        bx1 += dire
        if p.eixo() == "v":
            by0 -= PIN_LEN_V
            by1 += PIN_LEN_V
            # room for the reference and the value beside a vertical part
            bx0 -= 1.5 * GRID
            bx1 += 1.5 * GRID
        else:
            # ...and above and below a horizontal one: two parts on
            # neighbouring pins at 2,54 mm of pitch printed their values on
            # top of each other
            by0 -= 1.5 * GRID
            by1 += 1.5 * GRID
    p.x, p.y = old
    return (bx0, by0, bx1, by1)


PIN_LEN_V = 2.54


def _sobrepoe(a, b, folga: float) -> bool:
    return (a[2] + folga > b[0] and b[2] + folga > a[0]
            and a[3] + folga > b[1] and b[3] + folga > a[1])


def _lado_e_ponta(ref: str, pin_name: str) -> tuple[str, tuple[float, float]]:
    """Side a pin faces and its tip, for the part where it currently is."""
    p = P.PARTS[ref]
    num = next(q.number for q in p.pins if q.name == pin_name or q.number == pin_name)
    px, py, ang = p.pin_local()[num]
    lado = {0: "L", 180: "R", 90: "B", 270: "T"}[ang] if p.kicad else \
        next(q.side for q in p.pins if q.number == num)
    if not p.kicad and __import__("sch_lib").classe_do_simbolo(p):
        # the two-terminal symbols: left/right or top/bottom by position
        if p.eixo() == "v":
            lado = "T" if py > 0 else "B"
        else:
            lado = "L" if px < 0 else "R"
    return lado, (p.x + px, p.y - py)


def _pino_compartilhado(ref: str, colocado: str) -> tuple[str, str] | None:
    """(pin of `colocado`, pin of `ref`) on a common signal net, if any."""
    melhor = None
    for nome, pinos in N.NETS.items():
        if nome in S.TRILHOS:
            continue
        de_col = [p for r, p in pinos if r == colocado]
        de_ref = [p for r, p in pinos if r == ref]
        if de_col and de_ref:
            cand = (de_col[0], de_ref[0], len(pinos))
            if melhor is None or cand[2] < melhor[2]:
                melhor = cand
    return None if melhor is None else (melhor[0], melhor[1])


def _colocar_bloco(bloco: Bloco) -> tuple[float, float]:
    """Lay the block out around its anchors, at origin (0, 0). Returns (w, h).

    Coordinates are relative; posicionar() shifts the whole block later.
    """
    caixas: dict[str, tuple[float, float, float, float]] = {}
    # every placed pin tip with its net: a candidate whose tip lands within
    # two grid steps of another net's tip is refused, because the power
    # symbol or the label stub that pin will need has nowhere else to go
    pontas: dict[tuple[float, float], str] = {}

    def rede_do_pino(ref: str, numero: str) -> str:
        p = P.PARTS[ref]
        nome_p = next(q.name for q in p.pins if q.number == numero)
        for nome_n, pinos in N.NETS.items():
            if any(r == ref and (pi == nome_p or pi == numero) for r, pi in pinos):
                return nome_n
        return ""

    def pontas_de(ref: str, x: float, y: float):
        p = P.PARTS[ref]
        old = (p.x, p.y)
        p.x, p.y = x, y
        saida = [((snap(tx), snap(ty)), rede_do_pino(ref, n))
                 for n, (tx, ty) in p.pin_sheet().items()]
        p.x, p.y = old
        return saida

    def cabe(ref: str, x: float, y: float) -> bool:
        c = _caixa(ref, x, y)
        if any(_sobrepoe(c, o, FOLGA) for r, o in caixas.items() if r != ref):
            return False
        for (tx, ty), rede in pontas_de(ref, x, y):
            for (ox, oy), outra in pontas.items():
                if outra != rede and math.hypot(tx - ox, ty - oy) < 2.5 * GRID:
                    return False
        return True

    def por(ref: str, x: float, y: float) -> None:
        p = P.PARTS[ref]
        p.x, p.y = snap(x), snap(y)
        caixas[ref] = _caixa(ref, p.x, p.y)
        for ponta, rede in pontas_de(ref, p.x, p.y):
            pontas[ponta] = rede

    # the anchors, in a row
    x = 0.0
    for a in bloco.ancoras:
        w, h = P.PARTS[a].size()
        esq, dire = P.PARTS[a].avanco_dos_pinos()
        dx, dy = P.PARTS[a].desloca_centro()
        por(a, x + esq + w / 2.0 - dx, h / 2.0 - dy)
        x += esq + w + dire + 8 * GRID

    # the members, each beside the pin it serves
    prateleira: list[str] = []
    for ref in bloco.membros:
        if ref in caixas:
            continue
        alvo = None
        for colocado in list(bloco.ancoras) + [r for r in caixas if r not in bloco.ancoras]:
            par = _pino_compartilhado(ref, colocado)
            if par:
                alvo = (colocado, par)
                break
        if alvo is None:
            prateleira.append(ref)
            continue
        colocado, (pin_col, pin_ref) = alvo
        lado, (tx, ty) = _lado_e_ponta(colocado, pin_col)
        dvec = {"L": (-1, 0), "R": (1, 0), "T": (0, -1), "B": (0, 1)}[lado]
        # where this part's own pin tip is, relative to its origin. For a
        # two-terminal part the pin that serves the chip has to be the one
        # FACING it: a part to the left of the chip presents its right pin.
        # The symbol cannot be rotated here, but it can be flipped.
        p = P.PARTS[ref]
        num = next(q.number for q in p.pins if q.name == pin_ref or q.number == pin_ref)
        p.espelho = False
        rx, ry, _a = p.pin_local()[num]
        if __import__("sch_lib").classe_do_simbolo(p) and len(p.pins) == 2:
            if p.eixo() == "h" and lado in ("L", "R"):
                # left of the chip (dvec -1): the serving pin must be at +x
                if (dvec[0] < 0 and rx < 0) or (dvec[0] > 0 and rx > 0):
                    p.espelho = True
            elif p.eixo() == "v" and lado in ("T", "B"):
                # above the chip (dvec y -1 on the sheet): the serving pin is
                # the lower one, py < 0 in symbol space (y up)
                if (dvec[1] < 0 and ry > 0) or (dvec[1] > 0 and ry < 0):
                    p.espelho = True
            rx, ry, _a = p.pin_local()[num]
        # first try: the part's pin tip GAP away from the placed pin tip
        base = (tx + dvec[0] * GAP - rx, ty + dvec[1] * GAP + ry)
        achou = False
        for passo in range(0, 60):
            # push outward along the pin's direction, then sideways
            for lateral in (0, 1, -1, 2, -2, 3, -3, 4, -4):
                ox = base[0] + dvec[0] * passo * GRID + (dvec[1] * lateral * 2 * GRID)
                oy = base[1] + dvec[1] * passo * GRID + (dvec[0] * lateral * 2 * GRID)
                if cabe(ref, snap(ox), snap(oy)):
                    por(ref, ox, oy)
                    achou = True
                    break
            if achou:
                break
        if not achou:
            prateleira.append(ref)

    # the shelf: parts that touch only rails, under the anchors
    if prateleira:
        # a block with no anchor ("Outros") starts its shelf at the origin.
        # The shelf wraps: a row is never wider than the anchors above it
        # (or 70 mm), so forty decoupling capacitors do not become a
        # single 400 mm line.
        y_base = (max(c[3] for c in caixas.values()) + 7 * GRID) if caixas else 0.0
        x_ini = min(c[0] for c in caixas.values()) if caixas else 0.0
        largura = max(70.0, (max(c[2] for c in caixas.values()) - x_ini) if caixas else 0.0)
        x = x_ini
        alt = 0.0
        for ref in prateleira:
            w, h = P.PARTS[ref].size()
            esq, dire = P.PARTS[ref].avanco_dos_pinos()
            if x > x_ini and x + esq + w + dire > x_ini + largura:
                x = x_ini
                y_base += alt + 6 * GRID
                alt = 0.0
            cx, cy = x + esq + w / 2.0, y_base + h / 2.0
            k = 0
            while not cabe(ref, snap(cx), snap(cy)) and k < 60:
                cx += 2 * GRID
                k += 1
            por(ref, cx, cy)
            x = caixas[ref][2] + 3 * GRID
            alt = max(alt, caixas[ref][3] - caixas[ref][1])

    x0 = min(c[0] for c in caixas.values())
    y0 = min(c[1] for c in caixas.values())
    x1 = max(c[2] for c in caixas.values())
    y1 = max(c[3] for c in caixas.values())
    # normalise to the origin
    for ref in caixas:
        P.PARTS[ref].x = snap(P.PARTS[ref].x - x0 + MARGEM_BLOCO)
        P.PARTS[ref].y = snap(P.PARTS[ref].y - y0 + MARGEM_BLOCO)
    return (snap(x1 - x0 + 2 * MARGEM_BLOCO), snap(y1 - y0 + 2 * MARGEM_BLOCO))


def posicionar(blocos: list[Bloco], papel: tuple[float, float],
               margem: float, topo: float) -> float:
    """Blocks on the page, left to right, shelf by shelf. Returns the y the
    last shelf ends at (what escolher_papel() asks)."""
    w_pag, _h_pag = papel
    x_lim = w_pag - margem
    x, y = margem, topo
    alt_linha = 0.0
    for b in blocos:
        bw, bh = _colocar_bloco(b)
        if x + bw > x_lim and alt_linha > 0.0:
            x = margem
            y += alt_linha + ENTRE_BLOCOS
            alt_linha = 0.0
        for ref in b.membros:
            P.PARTS[ref].x = snap(P.PARTS[ref].x + x)
            P.PARTS[ref].y = snap(P.PARTS[ref].y + y)
        b.caixa = (snap(x), snap(y), snap(x + bw), snap(y + bh))
        x += bw + ENTRE_BLOCOS
        alt_linha = max(alt_linha, bh)
    return y + alt_linha


def bloco_de(blocos: list[Bloco]) -> dict[str, Bloco]:
    return {r: b for b in blocos for r in b.membros}
