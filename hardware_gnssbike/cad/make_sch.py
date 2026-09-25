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

# folha -> redes que recebem PWR_FLAG nela. Preenchido em main().
FLAGS_AQUI: dict[str, set[str]] = {}


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
        # Quanto os pinos avancam para fora da caixa de cada lado.
        #
        # Nos simbolos deste projeto isso e sempre PIN_LEN e da na mesma. Nos
        # da biblioteca do KiCad nao: um `Conn_01x10_Socket` tem o desenho com
        # 1,27 mm de largura e os DEZ pinos 3,81 mm a esquerda dele. Sem
        # reservar esse avanco, o vizinho da esquerda encosta nos dez pinos e
        # os dez fios tem de se espremer no que sobra - dois deles nao
        # roteavam.
        esq, dire = part.avanco_dos_pinos()
        if x + esq + w + dire > x_lim and alt_linha > 0.0:
            x = MARGEM + LABEL_X
            y += alt_linha + ROW_GAP
            alt_linha = 0.0
        x += esq
        # `desloca_centro` e zero para os simbolos deste projeto e diferente
        # de zero para os da biblioteca do KiCad, que nao sao centrados na
        # origem. Sem descontar aqui, a peca sai da celula que a fila lhe deu.
        dx, dy = part.desloca_centro()
        part.x = snap(x + w / 2.0 - dx)
        part.y = snap(y + h / 2.0 - dy)
        x += w + dire + COL_GAP
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
    numero = next(q.number for q in part.pins
                  if q.name == pin_name or q.number == pin_name)
    if part.kicad:
        # Num simbolo da biblioteca do KiCad quem manda e o angulo do pino,
        # nao o lado que este projeto declarou em parts.py: o desenho ja
        # existe e o fio tem de sair na direcao em que o pino aponta. Angulo
        # 0 e um pino que aponta para a direita, ou seja, esta na ESQUERDA.
        ang = part.pin_local()[numero][2]
        return {0: "L", 180: "R", 90: "B", 270: "T"}.get(ang, "L")
    return next(q.side for q in part.pins
                if q.name == pin_name or q.number == pin_name)


def montar_folha(nome: str, arquivo: str, pagina: str, root_uuid: str,
                 sheet_uuid: str, tipo: dict[str, str],
                 papel_forcado: str = "") -> tuple[Schematic, list[str], int]:
    refs = ordenar_por_ligacao(S.por_folha()[nome])
    entre = sorted(n for n, k in tipo.items()
                   if k == "ENTRE" and any(S.sheet_of(r) == nome for r, _p in N.NETS[n]))
    papel = papel_forcado or escolher_papel(refs, len(entre))
    posicionar(refs, papel)
    w_pag, h_pag = PAPEIS[papel]

    sch = Schematic(PROJETO, papel, name=nome, root_uuid=root_uuid,
                    title=f"GNSS Bike Computer - {nome}", rev="A", date=DATA)
    sch.sheet_symbol_uuid = sheet_uuid
    sch.parts = [P.PARTS[r] for r in refs]

    router = Router(sch.parts)
    router.build_obstacles()

    # De quem e cada celula de pino desta folha.
    #
    # O roteador sabe QUE uma celula e de pino, mas nao DE QUEM - e o
    # colocador de simbolos de alimentacao, que roda antes de qualquer fio,
    # so olhava `router.blocked`. Uma celula de pino nao esta em `blocked`,
    # esta em `pin_cells`: nada impedia um simbolo de GND de pousar em cima
    # do pino do vizinho, ou o talo dele de atravessar o pino alheio. Dois
    # fios que se encostam sao um no so para o KiCad, e nada no desenho diz
    # isso - foi assim que o MPPT do ADP5091, que fica na borda de baixo
    # junto dos tres pinos de terra, virou terra.
    dono_da_celula: dict[tuple[int, int], set[str]] = {}
    for rede_n, pinos_n in N.NETS.items():
        for ref_n, pin_n in pinos_n:
            if S.sheet_of(ref_n) != nome:
                continue
            peca = P.PARTS[ref_n]
            num = next(q.number for q in peca.pins
                       if q.name == pin_n or q.number == pin_n)
            px_n, py_n = peca.pin_sheet()[num]
            ang_n = peca.pin_local()[num][2]
            dx_n, dy_n = {0: (1, 0), 90: (0, -1),
                          180: (-1, 0), 270: (0, 1)}.get(ang_n, (0, 0))
            bx0, by0, bx1, by1 = peca.box()
            cx_n, cy_n = px_n, py_n
            for _ in range(9):
                dono_da_celula.setdefault(router.key(cx_n, cy_n),
                                          set()).add(rede_n)
                if bx0 <= cx_n <= bx1 and by0 <= cy_n <= by1:
                    break
                cx_n += dx_n * GRID
                cy_n += dy_n * GRID

    def _de_outro(cel, rede: str) -> bool:
        return bool(dono_da_celula.get(cel, set()) - {rede})

    # ---- supplies and grounds, one power symbol per pin ----
    # A power symbol is a symbol AND a label, and only the symbol was being
    # given room: two ground pins two grid steps apart each got their own
    # symbol, the symbols did not collide, and the words printed straight
    # over each other - "GNDGNDGND" across the bottom of every chip. What is
    # reserved now is the whole thing, the name included.
    ocupados: set[tuple[float, float]] = set()
    caixas: list[tuple[float, float, float, float]] = []

    def _cabe(qx: float, qy: float, rede: str) -> bool:
        w = len(rede) * 0.9 + 0.8
        h = 4.2                     # the symbol plus its name under it
        a = (qx - w / 2, qy - h / 2, qx + w / 2, qy + h / 2)
        return not any(a[2] > b[0] and b[2] > a[0] and a[3] > b[1] and b[3] > a[1]
                       for b in caixas)

    for rede, terra in S.TRILHOS.items():
        if rede not in N.NETS:
            continue
        for ref, pin_name in pinos_do_no(rede, nome):
            px, py = ponto(ref, pin_name)
            lado = lado_do_pino(ref, pin_name)
            dx, dy = {"L": (-1, 0), "R": (1, 0), "T": (0, -1), "B": (0, 1)}[lado]
            # First pass: a spot where the NAME also fits. Second: any free
            # spot at all. A long stub to save a label is worse than the
            # label - at fourteen grid steps one of them ran across a
            # component and split a net.
            passo = None
            for exigir_rotulo in (True, False):
                for tentativa in range(2, 9):
                    qx, qy = (px + dx * tentativa * GRID,
                              py + dy * tentativa * GRID)
                    if (qx, qy) in ocupados:
                        continue
                    if router.key(qx, qy) in router.blocked:
                        continue
                    # nem o simbolo nem o talo dele podem encostar num pino
                    # de OUTRA rede: seria um curto que o desenho nao mostra
                    if any(_de_outro(router.key(px + dx * k * GRID,
                                                py + dy * k * GRID), rede)
                           for k in range(1, tentativa + 1)):
                        continue
                    if exigir_rotulo and not _cabe(qx, qy, rede):
                        continue
                    passo = tentativa
                    break
                if passo is not None:
                    break
            if passo is None:
                passo = 2
            qx, qy = px + dx * passo * GRID, py + dy * passo * GRID
            ocupados.add((qx, qy))
            w_r = len(rede) * 0.9 + 0.8
            caixas.append((qx - w_r / 2, qy - 2.1, qx + w_r / 2, qy + 2.1))
            router.pin_cells.add(router.key(qx, qy))
            sch.powers.append(PowerPort(rede, qx, qy, ground=terra,
                                        ref=f"#PWR{len(sch.powers) + 1:03d}"))
            sch.wires.append(((px, py), (qx, qy)))
            for k in range(min(passo, 14) + 1):
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
    # A ORDEM importa, e muito. Um pino no MEIO de uma coluna de dez - o
    # pino 5 do FPC do display - fica cercado pelos fios dos vizinhos assim
    # que eles saem, e quem chega por ultimo nao acha caminho. Roteando
    # primeiro quem tem menos espaco em volta, o apertado passa e o folgado
    # da a volta, que e o que uma pessoa faz.
    #
    # "Espaco em volta" aqui e quantas celulas livres ha ao redor dos pinos
    # da rede, contadas no raio de tres passos de grade.
    def _aperto(rede: str) -> int:
        livre = 0
        for r, p in pinos_do_no(rede, nome):
            kx, ky = router.key(*ponto(r, p))
            for ax in range(-3, 4):
                for ay in range(-3, 4):
                    c = (kx + ax, ky + ay)
                    if c not in router.blocked and c not in router.pin_cells:
                        livre += 1
        return livre

    for rede in sorted(dentro, key=lambda n: (_aperto(n), n)) + sorted(entre):
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

    # ---- as duas bandeiras que o ERC do KiCad cobra ----
    #
    # Sem elas o ERC acusa 52 erros que nao sao erros, e um relatorio com 52
    # falsos positivos e um relatorio que ninguem le - foi por isso que este
    # projeto passou meses sem nunca ter rodado o ERC.
    #
    # 1. Pino aberto DE PROPOSITO leva bandeira de "sem conexao". Sao os
    #    NC do supressor, os GPIO que ninguem usa, o SDA e o SCL do receptor
    #    (que fala por UART), o SWO do gravador. `nets.py` ja e a fonte da
    #    verdade sobre o que esta aberto: um pino que nao aparece em rede
    #    nenhuma esta aberto, e ponto.
    # Uma rede de UM pino so nao liga nada: e um pino aberto com nome. O
    # R111, que era o pull-up do IRQ do colhedor antigo, virou isso quando a
    # peca mudou - e o ERC, com razao, chama de pino sem ligacao.
    com_no = {(r, q.number) for rede, pinos_r in N.NETS.items()
              if len(pinos_r) > 1
              for r, p in pinos_r if S.sheet_of(r) == nome
              for q in P.PARTS[r].pins if q.name == p or q.number == p}
    for ref in refs:
        peca = P.PARTS[ref]
        folha_pinos = peca.pin_sheet()
        # Pontos onde JA existe um pino ligado. Simbolo da biblioteca do
        # KiCad empilha pinos - os quatro VBUS do USB-C num ponto so, o pad
        # exposto do colhedor sobre o AGND -, e por um desses pontos a rede
        # ja passa. Uma bandeira de "sem conexao" ali seria mentira, e o ERC
        # a chama de `no_connect_connected`.
        ligados = {folha_pinos[n] for n in folha_pinos if (ref, n) in com_no}
        for num, xy in folha_pinos.items():
            if (ref, num) not in com_no and xy not in ligados:
                sch.no_connects.append(xy)
                ligados.add(xy)      # um so por ponto, nunca dois empilhados

    # 2. Trilho alimentado atraves de peca passiva nao tem fonte que o ERC
    #    enxergue: o 1V8_GNSS vem do 1V8 por um ferrite, o SD3V0_FLASH vem
    #    do SD3V0 por um jumper, o VIN do colhedor vem do painel. O PWR_FLAG
    #    e o simbolo que existe para dizer "esta alimentado, eu respondo por
    #    isso".
    #
    #    UMA por rede NO PROJETO INTEIRO. Duas bandeiras na mesma rede sao
    #    duas saidas de alimentacao ligadas entre si, que e outro erro de
    #    ERC - e uma rede que atravessa folhas ganharia uma em cada. Por isso
    #    `precisa_de_flag` e calculado uma vez, fora daqui, e cada folha so
    #    coloca as que lhe couberem.
    for rede in sorted(FLAGS_AQUI.get(nome, ())):
        px, py = ponto(*pinos_do_no(rede, nome)[0])
        sch.powers.append(PowerPort("PWR_FLAG", px, py, ground=False,
                                    ref=f"#FLG{len(sch.powers) + 1:03d}"))

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

    # Quais redes precisam de PWR_FLAG, e em que folha cada uma recebe a sua.
    #
    # Precisa quem tem pino de ENTRADA de alimentacao e nenhum de saida em
    # lugar nenhum do projeto: um trilho que chega por ferrite, por jumper ou
    # de fora da placa. A folha escolhida e a primeira, em ordem, que tenha
    # um pino daquela rede - assim a bandeira fica perto de onde a rede vive.
    FLAGS_AQUI.clear()
    # Duas redes que compartilham um PINO sao uma so para o KiCad. Acontece
    # de proposito nos conectores que atravessam sinal - o do display leva o
    # FPC_VSS de um lado e o GND do outro pelo mesmo contato -, e sem juntar
    # antes de contar, cada lado ganharia a sua bandeira e as duas brigariam
    # como duas saidas de alimentacao ligadas entre si.
    grupo: dict[str, str] = {}

    def _raiz(n: str) -> str:
        while grupo.get(n, n) != n:
            n = grupo[n]
        return n

    por_pino: dict[tuple[str, str], str] = {}
    for rede, pinos_r in N.NETS.items():
        grupo.setdefault(rede, rede)
        for r, pi in pinos_r:
            num = next(q.number for q in P.PARTS[r].pins
                       if q.name == pi or q.number == pi)
            outro = por_pino.setdefault((r, num), rede)
            if _raiz(outro) != _raiz(rede):
                grupo[_raiz(rede)] = _raiz(outro)

    tipos_do_grupo: dict[str, list[str]] = {}
    for rede, pinos_r in N.NETS.items():
        g = _raiz(rede)
        tipos_do_grupo.setdefault(g, []).extend(
            P.PARTS[r].etype_de(
                next(q.number for q in P.PARTS[r].pins
                     if q.name == pi or q.number == pi)) for r, pi in pinos_r)

    feitos: set[str] = set()
    for rede, pinos_r in N.NETS.items():
        g = _raiz(rede)
        if g in feitos:
            continue
        tipos = tipos_do_grupo[g]
        if "power_out" in tipos or "output" in tipos:
            continue
        # Um trilho SEMPRE tem entrada de alimentacao, mesmo quando todos os
        # pinos de peca nele sao passivos: o proprio simbolo de alimentacao
        # que o carrega e um pino `power_in`. Era o caso do VBAT_CELULA, que
        # so toca o conector da bateria e um jumper, e do 1V8_BLOCO, entre o
        # jumper e o ferrite.
        if "power_in" not in tipos and rede not in S.TRILHOS:
            continue
        for folha_n, _a, _pg in S.FOLHAS:
            if any(S.sheet_of(r) == folha_n for r, _pi in pinos_r):
                FLAGS_AQUI.setdefault(folha_n, set()).add(rede)
                feitos.add(g)
                break

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
