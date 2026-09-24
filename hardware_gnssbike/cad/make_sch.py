#!/usr/bin/env python3
"""Write the hierarchical schematic: a root sheet and six block sheets.

The root is a block diagram: one box per sheet of 01-esquematico.md, the
signals that cross between them drawn as lines from box to box. Inside each
sheet the parts are placed on a grid with routing channels between the rows,
supplies and grounds are carried by power symbols next to the pin they feed,
and a signal that leaves the sheet ends at a hierarchical label on the
margin. That is the shape a schematic of this size has when a person draws
it, and it is what makes it readable: no page is wider than a sheet of paper,
and nothing is connected by a name that appears nowhere else.

Run:   python hardware_gnssbike/cad/make_sch.py
Check: python hardware_gnssbike/cad/check_sch.py
"""

from __future__ import annotations

import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import nets as N  # noqa: E402
import parts as P  # noqa: E402
import footprints as FPS  # noqa: E402
import sheets as S  # noqa: E402
from sch_lib import (GRID, HierLabel, PowerPort, Router, Schematic,  # noqa: E402
                     SheetSymbol, snap)

PROJETO = "gnssbike"
DATA = "2026-09-23"

# Standard paper, chosen per sheet by how much it has to hold.
PAPEIS = {"A4": (297.0, 210.0), "A3": (420.0, 297.0), "A2": (594.0, 420.0),
          "A1": (841.0, 594.0), "A0": (1189.0, 841.0)}
ORDEM = ["A4", "A3", "A2", "A1", "A0"]

MARGEM = 20.0
# Routing channels between parts. They were 24 and 30, which is a corridor
# wider than most of the symbols it separates: with 69 parts on the power
# sheet that alone forced A1 - 841 x 594 mm of paper for a block that fits
# on a third of it. A schematic sheet bigger than A4 is almost always a
# layout that was spread to fill the paper rather than paper chosen to hold
# the layout. 14 and 18 still leave more than a wire's width between any two
# symbols, and check_sch.py is what proves it: it fails if a wire crosses a
# component on any sheet.
COL_GAP = 16.0
ROW_GAP = 22.0
LABEL_X = 12.0     # how far inside the left and right margins a label sits


def escolher_papel(refs: list[str], n_labels: int) -> str:
    """The smallest standard sheet the drawing actually fits on.

    Measured, not estimated. The old version budgeted a fixed 30 x 18 mm cell
    plus the channels for EVERY part - 54 x 48 mm each, whether it was a
    0402 or an eighty pad module - and started from A3, so A4 was never even
    considered. Sixty-nine parts then "needed" 178.848 mm2 and landed on A1,
    where the real drawing occupies about a third of the sheet.

    This lays the parts out on the candidate sheet and asks whether the last
    row ended above the bottom margin. That is the same code that draws them,
    so the answer cannot drift from the drawing.
    """
    for nome in ORDEM:
        w, h = PAPEIS[nome]
        if (h - 2 * MARGEM) / (2 * GRID) < n_labels:
            continue
        if posicionar(refs, nome) <= h - MARGEM:
            return nome
    return "A0"


def ordenar_por_ligacao(refs: list[str]) -> list[str]:
    """Walk the sheet's parts by what connects to what, biggest part first.

    Filling the grid in this order puts the decoupling next to its chip and
    the divider next to the pin it divides, instead of sorting the page by
    pin count and scattering a circuit across it.
    """
    conjunto = set(refs)
    ligados: dict[str, set[str]] = {r: set() for r in refs}
    for nome, pinos in N.NETS.items():
        if nome in S.TRILHOS:          # a rail touches everything: it says nothing
            continue
        na_folha = {r for r, _p in pinos} & conjunto
        for a in na_folha:
            ligados[a] |= na_folha - {a}
    restantes = sorted(refs, key=lambda r: (-len(P.PARTS[r].pins), r))
    saida: list[str] = []
    vistos: set[str] = set()
    while restantes:
        raiz = next(r for r in restantes if r not in vistos)
        fila = [raiz]
        vistos.add(raiz)
        while fila:
            atual = fila.pop(0)
            saida.append(atual)
            for v in sorted(ligados[atual],
                            key=lambda r: (-len(P.PARTS[r].pins), r)):
                if v not in vistos:
                    vistos.add(v)
                    fila.append(v)
        restantes = [r for r in restantes if r not in vistos]
    return saida


def posicionar(refs: list[str], papel: str) -> float:
    """Grid placement with channels, biggest part first.

    Returns the y the last row ends at, which is what escolher_papel() asks
    to know whether the block fits on this sheet.
    """
    w_pag, h_pag = PAPEIS[papel]
    x_lim = w_pag - MARGEM - LABEL_X
    x = MARGEM + LABEL_X
    y = MARGEM + 14.0
    alt_linha = 0.0
    for ref in refs:
        part = P.PARTS[ref]
        w, h = part.size()
        if x + w > x_lim and alt_linha > 0.0:
            x = MARGEM + LABEL_X
            y += alt_linha + ROW_GAP
            alt_linha = 0.0
        part.x = snap(x + w / 2.0)
        part.y = snap(y + h / 2.0)
        x += w + COL_GAP
        alt_linha = max(alt_linha, h)
    return y + alt_linha


def pinos_do_no(nome: str, folha: str) -> list[tuple[str, str]]:
    return [(r, p) for r, p in N.NETS[nome] if S.sheet_of(r) == folha]


def ponto(ref: str, pin_name: str) -> tuple[float, float]:
    part = P.PARTS[ref]
    numero = next(q.number for q in part.pins
                  if q.name == pin_name or q.number == pin_name)
    return part.pin_sheet()[numero]


def lado_do_pino(ref: str, pin_name: str) -> str:
    part = P.PARTS[ref]
    return next(q.side for q in part.pins
                if q.name == pin_name or q.number == pin_name)


def montar_folha(nome: str, arquivo: str, pagina: str, root_uuid: str,
                 sheet_uuid: str, tipo: dict[str, str]) -> tuple[Schematic, list[str], int]:
    refs = ordenar_por_ligacao(S.por_folha()[nome])
    entre = sorted(n for n, k in tipo.items()
                   if k == "ENTRE" and any(S.sheet_of(r) == nome for r, _p in N.NETS[n]))
    papel = escolher_papel(refs, len(entre))
    posicionar(refs, papel)
    w_pag, h_pag = PAPEIS[papel]

    sch = Schematic(PROJETO, papel, name=nome, root_uuid=root_uuid,
                    title=f"GNSS Bike Computer - {nome}", rev="A", date=DATA)
    sch.sheet_symbol_uuid = sheet_uuid
    sch.parts = [P.PARTS[r] for r in refs]

    router = Router(sch.parts)
    router.build_obstacles()

    # ---- supplies and grounds, one power symbol per pin ----
    ocupados: set[tuple[float, float]] = set()
    for rede, terra in S.TRILHOS.items():
        if rede not in N.NETS:
            continue
        for ref, pin_name in pinos_do_no(rede, nome):
            px, py = ponto(ref, pin_name)
            lado = lado_do_pino(ref, pin_name)
            dx, dy = {"L": (-1, 0), "R": (1, 0), "T": (0, -1), "B": (0, 1)}[lado]
            passo = 2
            while passo < 8:
                qx, qy = px + dx * passo * GRID, py + dy * passo * GRID
                if (qx, qy) not in ocupados and router.key(qx, qy) not in router.blocked:
                    break
                passo += 1
            ocupados.add((qx, qy))
            router.pin_cells.add(router.key(qx, qy))
            sch.powers.append(PowerPort(rede, qx, qy, ground=terra,
                                        ref=f"#PWR{len(sch.powers) + 1:03d}"))
            sch.wires.append(((px, py), (qx, qy)))
            for k in range(min(passo, 8) + 1):
                cel = router.key(px + dx * k * GRID, py + dy * k * GRID)
                router.used.setdefault(cel, set()).add(rede)
                d = router.dirs.setdefault(cel, {}).setdefault(rede, set())
                d.add("H" if dy == 0 else "V")

    # ---- signals that leave the sheet: a label on the nearest margin ----
    alvos_label: dict[str, tuple[float, float]] = {}
    esq = [r for r in entre if _mais_a_esquerda(r, nome, w_pag)]
    dir_ = [r for r in entre if r not in esq]
    for lista, x_lab, ang in ((esq, MARGEM + LABEL_X - 6.0, 180),
                              (dir_, w_pag - MARGEM - LABEL_X + 6.0, 0)):
        n = len(lista)
        if not n:
            continue
        passo = max(2 * GRID, snap((h_pag - 2 * MARGEM - 20.0) / max(n, 1)))
        for i, rede in enumerate(sorted(lista)):
            ly = snap(MARGEM + 14.0 + i * passo)
            sch.labels.append(HierLabel(rede, snap(x_lab), ly, angle=ang))
            alvos_label[rede] = (snap(x_lab), ly)
            # a label is a connection point: a wire of another net that runs
            # over it joins that signal, silently
            router.pin_cells.add(router.key(snap(x_lab), ly))

    # ---- routing ----
    falhas: list[str] = []
    dentro = [n for n, k in tipo.items()
              if k == "DENTRO" and any(S.sheet_of(r) == nome for r, _p in N.NETS[n])]
    for rede in sorted(dentro) + sorted(entre):
        pts = [ponto(r, p) for r, p in pinos_do_no(rede, nome)]
        if rede in alvos_label:
            pts.append(alvos_label[rede])
        if len(pts) < 2:
            continue
        feito = {router.key(*pts[0])}
        router.used.setdefault(router.key(*pts[0]), set()).add(rede)
        for pt in pts[1:]:
            if router.key(*pt) in feito:
                continue
            caminho = None
            for folga in (60, 200, 700):
                caminho = router.route(rede, pt, feito, folga)
                if caminho is not None:
                    break
            if caminho is None:
                falhas.append(f"{nome}: {rede} ate {pt}")
                continue
            router.add_path(rede, caminho)
            feito |= set(caminho)

    _fechar(sch, router)
    sch.text(MARGEM, MARGEM - 8.0,
             f"{nome} - {len(refs)} posicoes. NADA MONTADO NEM MEDIDO.", 2.5)
    return sch, falhas, len(refs)


def _mais_a_esquerda(rede: str, folha: str, w_pag: float) -> bool:
    pts = [ponto(r, p) for r, p in pinos_do_no(rede, folha)]
    if not pts:
        return True
    return sum(p[0] for p in pts) / len(pts) < w_pag / 2.0


def _fechar(sch: Schematic, router: Router) -> None:
    """Cut every run at the points its own net touches, then dot the branches.

    KiCad joins two wires only where they share an END: a run that stops in
    the middle of another one does not connect, junction dot or not.
    """
    segs = list(router.segs)
    nos: dict[str, set[tuple[float, float]]] = {}
    for a, b, n in segs:
        nos.setdefault(n, set()).update({(round(a[0], 3), round(a[1], 3)),
                                         (round(b[0], 3), round(b[1], 3))})
    partidos = []
    for a, b, n in segs:
        pontos = [a, b]
        for k in nos.get(n, ()):
            if abs(a[1] - b[1]) < 1e-6 and abs(k[1] - a[1]) < 1e-6 \
                    and min(a[0], b[0]) < k[0] < max(a[0], b[0]):
                pontos.append(k)
            elif abs(a[0] - b[0]) < 1e-6 and abs(k[0] - a[0]) < 1e-6 \
                    and min(a[1], b[1]) < k[1] < max(a[1], b[1]):
                pontos.append(k)
        pontos.sort(key=lambda p: (p[0], p[1]))
        for i in range(len(pontos) - 1):
            if pontos[i] != pontos[i + 1]:
                partidos.append((pontos[i], pontos[i + 1], n))
    sch.wires += [(a, b) for a, b, _n in partidos]

    contagem: dict[tuple[float, float], dict[str, int]] = {}
    for a, b, n in partidos:
        for p in (a, b):
            k = (round(p[0], 3), round(p[1], 3))
            contagem.setdefault(k, {}).setdefault(n, 0)
            contagem[k][n] += 1
    for k, donos in contagem.items():
        if any(c >= 3 for c in donos.values()) and \
                len(router.used.get(router.key(*k), set())) == 1:
            sch.junctions.append(k)


def main() -> int:
    # The footprint field is what ties the schematic to the board: without it
    # KiCad cannot update the layout from the schematic at all.
    for ref, (nome, _origem, _nota) in FPS.FP.items():
        P.PARTS[ref].footprint = nome
    for ref in FPS.FORA_DA_PLACA:
        P.PARTS[ref].note = (P.PARTS[ref].note + " | FORA DA PLACA: "
                             "vive na caixa e chega por contato ou cabo").strip(" |")

    tipo, _folhas = S.classificar()

    # How big each block has to be, and therefore how big the sheet is. The
    # blocks were a fixed 190 x 240 mm on a 370 x 350 mm pitch, which spans
    # 990 x 680 and forces A0 - for a diagram of six rectangles and the lines
    # between them. A block only has to hold its title and its pins: the pins
    # go two per row at 4 grid steps, and the widest pin NAME decides the
    # width. Measured, the diagram fits on A2.
    entre_raiz = sorted(n for n, k in tipo.items() if k == "ENTRE")
    pinos_por_folha: dict[str, list[str]] = {n: [] for n, _f, _p in S.FOLHAS}
    for rede in entre_raiz:
        for folha in sorted({S.sheet_of(r) for r, _p in N.NETS[rede]}):
            pinos_por_folha[folha].append(rede)
    n_max = max(len(v) for v in pinos_por_folha.values())
    letra = max((len(r) for v in pinos_por_folha.values() for r in v),
                default=8)
    # Two grid steps between pins, not four. Four is a 5,08 mm slot for a
    # 1,27 mm label, and on the MCU block - 33 crossing signals - it alone
    # made the block 116 mm tall and pushed the diagram off A3 onto A2.
    PASSO_PINO = 2 * GRID
    bloco_h = snap(20.0 + ((n_max + 1) // 2) * PASSO_PINO + 10.0)
    # the pin name is drawn inside the block, from each side
    bloco_w = snap(max(70.0, 2 * (letra * 2.1) + 24.0))
    passo_x = bloco_w + 34.0
    passo_y = bloco_h + 30.0
    largura = 30.0 + 2 * passo_x + bloco_w + 30.0
    altura = 40.0 + passo_y + bloco_h + 20.0
    papel_raiz = "A0"
    for nome_p in ORDEM:
        w, h = PAPEIS[nome_p]
        if largura <= w and altura <= h:
            papel_raiz = nome_p
            break

    raiz = Schematic(PROJETO, papel_raiz,
                     title="GNSS Bike Computer - diagrama de blocos",
                     rev="A", date=DATA)

    # the six blocks, two rows of three
    blocos: list[SheetSymbol] = []
    for i, (nome, arquivo, pagina) in enumerate(S.FOLHAS):
        col, lin = i % 3, i // 3
        blocos.append(SheetSymbol(nome, arquivo,
                                  x=snap(30.0 + col * passo_x),
                                  y=snap(40.0 + lin * passo_y),
                                  w=bloco_w, h=bloco_h, page=pagina))
    raiz.sheets = blocos
    por_nome = {b.name: b for b in blocos}

    # one pin per crossing signal on each block it touches
    entre = sorted(n for n, k in tipo.items() if k == "ENTRE")
    contador: dict[str, int] = {b.name: 0 for b in blocos}
    for rede in entre:
        for folha in sorted({S.sheet_of(r) for r, _p in N.NETS[rede]}):
            b = por_nome[folha]
            i = contador[folha]
            contador[folha] += 1
            lado = 180 if (i % 2 == 0) else 0
            py = snap(b.y + 10.0 + (i // 2) * PASSO_PINO)
            b.pins.append((rede, py, lado, "bidirectional"))

    todas: list[tuple[str, Schematic]] = []
    falhas: list[str] = []
    for nome, arquivo, pagina in S.FOLHAS:
        sch, f, _n = montar_folha(nome, arquivo, pagina, raiz.uuid,
                                  por_nome[nome].uuid, tipo)
        todas.append((arquivo, sch))
        falhas += f

    # the root's own wiring: a line between the two blocks each signal joins
    r_router = Router([])
    r_router.build_obstacles()
    for b in blocos:
        for kx in range(r_router.key(b.x, 0)[0], r_router.key(b.x + b.w, 0)[0] + 1):
            for ky in range(r_router.key(0, b.y)[1], r_router.key(0, b.y + b.h)[1] + 1):
                r_router.blocked.add((kx, ky))
    # A sheet pin is a connection point like any other: a wire that merely
    # passes over one joins that signal. They have to be reachable and, to
    # every other net, closed.
    for b in blocos:
        for n, py, ang, _s in b.pins:
            k = r_router.key(*b.pin_pos(n))
            r_router.blocked.discard(k)
            r_router.pin_cells.add(k)
    # Fan-out. Every sheet pin gets a short straight stub before the router
    # takes over, and each stub is a different length, so the turns happen in
    # different columns. Without it the first net to turn takes the corner
    # cell next to the block and the pin behind it can no longer be reached.
    stubs: dict[tuple[str, str], tuple[float, float]] = {}
    for b in blocos:
        for i, (n, py, ang, _s) in enumerate(b.pins):
            px, _ = b.pin_pos(n)
            dx = -1 if ang == 180 else 1
            comp = 2 + (i % 10)
            fim = (px + dx * comp * GRID, py)
            raiz.wires.append(((px, py), fim))
            for k in range(comp + 1):
                cel = r_router.key(px + dx * k * GRID, py)
                r_router.used.setdefault(cel, set()).add(n)
                r_router.dirs.setdefault(cel, {}).setdefault(n, set()).add("H")
                r_router.blocked.discard(cel)
            stubs[(b.name, n)] = fim

    for rede in entre:
        pontos = [stubs[(por_nome[f].name, rede)]
                  for f in sorted({S.sheet_of(r) for r, _p in N.NETS[rede]})]
        feito = {r_router.key(*pontos[0])}
        r_router.used.setdefault(r_router.key(*pontos[0]), set()).add(rede)
        for pt in pontos[1:]:
            caminho = None
            for folga in (120, 400, 1200):
                caminho = r_router.route(rede, pt, feito, folga)
                if caminho is not None:
                    break
            if caminho is None:
                falhas.append(f"raiz: {rede}")
                continue
            r_router.add_path(rede, caminho)
            feito |= set(caminho)
    _fechar(raiz, r_router)
    raiz.text(25.0, 22.0,
              "Trilhos de alimentacao e terra viajam por simbolo de "
              "alimentacao, nao por linha. NADA FOI MONTADO NEM MEDIDO.", 2.2)

    (HERE / "gnssbike.kicad_sch").write_text(raiz.render(), encoding="utf-8", newline="\n")
    for arquivo, sch in todas:
        (HERE / arquivo).write_text(sch.render(), encoding="utf-8", newline="\n")

    print(f"raiz + {len(todas)} folhas")
    for (arquivo, sch), (nome, _f, _p) in zip(todas, S.FOLHAS):
        print(f"  {arquivo}: {sch.paper}, {len(sch.parts)} pecas, "
              f"{len(sch.powers)} simbolos de alimentacao, "
              f"{len(sch.labels)} rotulos, {len(sch.wires)} fios")
    if falhas:
        print(f"  NAO ROTEADOS: {len(falhas)}")
        for f in falhas[:10]:
            print(f"    {f}")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
