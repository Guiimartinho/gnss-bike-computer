#!/usr/bin/env python3
"""A writer for KiCad 8 schematics, and a router that keeps wires off the parts.

The owner asked for a schematic drawn with real connection lines and no net
labels, with the wires separated so that none runs across a component. That is
two jobs: emitting the file, which is mechanical, and routing, which is not.

Emitting. A KiCad schematic carries its own symbol definitions in lib_symbols,
so nothing here depends on a library being installed. Each part becomes a
rectangle with its pins on the four sides, at the pitch KiCad uses, 1.27 mm.
Symbol space has y upward and sheet space has y downward, so a pin at symbol
(px, py) on a part placed at (X, Y) lands on the sheet at (X + px, Y - py).

Routing. Every net is a tree grown pin by pin with an A* on the 1.27 mm grid:

  - a part's rectangle is an obstacle, so no wire crosses a component;
  - a wire already drawn for the SAME net is free to join, which is what makes
    a branch instead of a second parallel run, and where it joins mid-wire a
    junction dot is written;
  - a wire of ANOTHER net may be CROSSED, at a right angle, because two wires
    crossing without a dot is how a schematic says "these do not connect", and
    forbidding it would make most sheets unroutable. What is forbidden is
    sharing: running along the same line as another net, ending on it, or
    crossing at a point where it turns. KiCad joins collinear wires into one
    net, so a shared run is a short circuit that nothing on the drawing shows;
  - turning costs a little, so runs come out straight instead of staircased.
"""

from __future__ import annotations

import hashlib
import heapq
import math
from dataclasses import dataclass, field

GRID = 1.27
PIN_LEN = 2.54
TEXT = 1.27


def uid(*parts: object) -> str:
    h = hashlib.sha256("|".join(str(p) for p in parts).encode()).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-8{h[17:20]}-{h[20:32]}"


def snap(v: float) -> float:
    return round(v / GRID) * GRID


def esc(s: str) -> str:
    """Quote a string for the s-expression.

    One unescaped quote inside a note and KiCad refuses the whole file, while
    the file still reads as balanced parentheses, so nothing warns about it.
    The notes on this sheet quote the documents, and the documents use
    quotation marks, so this is not a theoretical case: it happened.
    """
    return s.replace("\\", "\\\\").replace('"', '\\"')


# --- simbolos ---------------------------------------------------------------
# A block with pin names down its sides is how an INTEGRATED CIRCUIT is drawn,
# and only an integrated circuit. A resistor drawn as a block is a block, and a
# sheet of blocks joined by lines does not show a circuit - which is what this
# generator produced: 127 parts, every one of them a rectangle, resistors and
# ceramic capacitors and diodes and the TVS alike.
#
# Everything below is the standard symbol for its class, drawn in the
# symbol's own frame with the lead axis on x, the body between -1.27 and
# +1.27, and the pins at +-3.81. A part whose two pins are on TOP and BOTTOM
# gets the same art turned a quarter turn.
CORPO_2T = 1.27


def _traco(pts, largura=0.254, preenche="none") -> str:
    p = " ".join(f"(xy {x:.4f} {y:.4f})" for x, y in pts)
    return (f'\t\t\t\t(polyline\n\t\t\t\t\t(pts {p})\n'
            f'\t\t\t\t\t(stroke (width {largura}) (type default))\n'
            f'\t\t\t\t\t(fill (type {preenche}))\n\t\t\t\t)')


def _circulo(x, y, r, largura=0.254, preenche="none") -> str:
    return (f'\t\t\t\t(circle\n\t\t\t\t\t(center {x:.4f} {y:.4f})'
            f'\n\t\t\t\t\t(radius {r:.4f})\n'
            f'\t\t\t\t\t(stroke (width {largura}) (type default))\n'
            f'\t\t\t\t\t(fill (type {preenche}))\n\t\t\t\t)')


def _arco(x0, y0, xm, ym, x1, y1, largura=0.254) -> str:
    return (f'\t\t\t\t(arc\n\t\t\t\t\t(start {x0:.4f} {y0:.4f})'
            f'\n\t\t\t\t\t(mid {xm:.4f} {ym:.4f})'
            f'\n\t\t\t\t\t(end {x1:.4f} {y1:.4f})\n'
            f'\t\t\t\t\t(stroke (width {largura}) (type default))\n'
            f'\t\t\t\t\t(fill (type none))\n\t\t\t\t)')


def classe_do_simbolo(p) -> str | None:
    """Which symbol this part gets, or None to keep the block."""
    pre = "".join(c for c in p.ref if c.isalpha())
    lados = {q.side for q in p.pins}
    if len(p.pins) == 1 and pre == "TP":
        return "tp"
    if len(p.pins) == 3 and pre == "Q":
        return None          # o MOSFET fica para uma passagem propria
    if len(p.pins) != 2:
        return None
    v = (p.value + " " + p.note).lower()
    if pre in ("R", "JP"):
        return "r"
    if pre == "RT":
        return "rt"
    if pre == "C":
        return "c_pol" if ("elet" in v or "tant" in v) else "c"
    if pre == "L":
        return "l"
    if pre == "FB":
        return "fb"
    if pre == "D":
        if "esd" in v or "tvs" in v or "tpd" in v:
            return "tvs"
        if "led" in v or v.startswith("apt"):
            return "led"
        return "d"
    if pre == "SW":
        return "sw"
    if pre == "LS":
        return "buz"
    if pre == "PV":
        return "pv"
    if pre == "E":
        return "ant"
    return None


def _arte_bruta(cl: str) -> list[str]:
    """The class's art, lead axis on x, body from -1.27 to 1.27."""
    a = CORPO_2T
    if cl == "r":
        return [_traco([(-a, 0), (-1.016, 0.762), (-0.508, -0.762), (0, 0.762),
                        (0.508, -0.762), (1.016, 0.762), (a, 0)])]
    if cl == "rt":
        # resistor com a seta na diagonal: termistor
        return [_traco([(-a, 0), (-1.016, 0.762), (-0.508, -0.762), (0, 0.762),
                        (0.508, -0.762), (1.016, 0.762), (a, 0)]),
                _traco([(-1.27, -1.27), (1.27, 1.27)], largura=0.152),
                _traco([(1.27, 1.27), (0.6, 1.1), (0.95, 0.5), (1.27, 1.27)],
                       largura=0.152, preenche="outline")]
    if cl == "c":
        return [_traco([(-a, 0), (-0.254, 0)]), _traco([(0.254, 0), (a, 0)]),
                _traco([(-0.254, -1.016), (-0.254, 1.016)], largura=0.305),
                _traco([(0.254, -1.016), (0.254, 1.016)], largura=0.305)]
    if cl == "c_pol":
        return [_traco([(-a, 0), (-0.254, 0)]), _traco([(0.254, 0), (a, 0)]),
                _traco([(-0.254, -1.016), (-0.254, 1.016)], largura=0.305),
                _arco(0.254, 1.016, 0.762, 0.0, 0.254, -1.016, largura=0.305),
                _traco([(-1.016, 0.889), (-0.508, 0.889)], largura=0.152),
                _traco([(-0.762, 0.635), (-0.762, 1.143)], largura=0.152)]
    if cl == "l":
        # quatro arcos, que e como se desenha um indutor
        arcos = []
        for i in range(4):
            x0 = -a + i * (2 * a / 4)
            x1 = x0 + 2 * a / 4
            arcos.append(_arco(x0, 0, (x0 + x1) / 2, 0.635, x1, 0))
        return arcos
    if cl == "fb":
        arcos = []
        for i in range(4):
            x0 = -a + i * (2 * a / 4)
            x1 = x0 + 2 * a / 4
            arcos.append(_arco(x0, 0, (x0 + x1) / 2, 0.508, x1, 0))
        # o retangulo por cima e o que distingue a ferrite do indutor
        arcos.append(_traco([(-a, 0.889), (a, 0.889), (a, -0.254),
                             (-a, -0.254), (-a, 0.889)], largura=0.152))
        return arcos
    if cl in ("d", "led", "tvs", "pv"):
        art = [_traco([(-a, 0), (-0.508, 0)]), _traco([(0.508, 0), (a, 0)])]
        if cl == "tvs":
            # bidirecional: dois triangulos costas com costas
            art += [_traco([(-0.508, -1.016), (-0.508, 1.016), (0.254, 0),
                            (-0.508, -1.016)], preenche="outline"),
                    _traco([(0.508, -1.016), (0.508, 1.016), (-0.254, 0),
                            (0.508, -1.016)], preenche="outline")]
            return art
        art.append(_traco([(-0.508, -1.016), (-0.508, 1.016), (0.508, 0),
                           (-0.508, -1.016)], preenche="outline"))
        art.append(_traco([(0.508, -1.016), (0.508, 1.016)]))
        if cl == "led":
            for dy in (0.0, 0.508):
                art.append(_traco([(-0.2, 1.1 + dy), (0.4, 1.7 + dy)],
                                  largura=0.152))
                art.append(_traco([(0.4, 1.7 + dy), (0.1, 1.6 + dy),
                                   (0.3, 1.4 + dy), (0.4, 1.7 + dy)],
                                  largura=0.152, preenche="outline"))
        if cl == "pv":
            # as setas apontam PARA o diodo: e uma celula, nao um LED
            for dy in (0.0, 0.508):
                art.append(_traco([(0.4, 1.7 + dy), (-0.2, 1.1 + dy)],
                                  largura=0.152))
                art.append(_traco([(-0.2, 1.1 + dy), (0.1, 1.2 + dy),
                                   (-0.1, 1.4 + dy), (-0.2, 1.1 + dy)],
                                  largura=0.152, preenche="outline"))
            art.append(_traco([(-a, -1.4), (a, -1.4)], largura=0.152))
        return art
    if cl == "sw":
        # tecla de pressionar: dois contatos, a barra e o embolo
        return [_traco([(-a, 0), (-0.762, 0)]), _traco([(0.762, 0), (a, 0)]),
                _circulo(-0.762, 0, 0.2, preenche="outline"),
                _circulo(0.762, 0, 0.2, preenche="outline"),
                _traco([(-1.016, 0.635), (1.016, 0.635)]),
                _traco([(0, 0.635), (0, 1.27)], largura=0.152),
                _traco([(-0.508, 1.27), (0.508, 1.27)], largura=0.305)]
    if cl == "buz":
        return [_traco([(-a, 0), (-0.635, 0)]), _traco([(0.635, 0), (a, 0)]),
                _circulo(0, 0, 0.889),
                _traco([(-0.4, -0.5), (0.4, 0.5)], largura=0.152)]
    if cl == "ant":
        return [_traco([(-a, 0), (0, 0)]),
                _traco([(0, 0), (0, 1.016)]),
                _traco([(-0.889, 1.016), (0.889, 1.016)]),
                _traco([(-0.889, 1.016), (-1.397, 1.778)], largura=0.152),
                _traco([(0.889, 1.016), (1.397, 1.778)], largura=0.152)]
    if cl == "tp":
        return [_traco([(-a, 0), (0.508, 0)]),
                _circulo(0.889, 0, 0.381)]
    return []


def arte_do_simbolo(p) -> list[str]:
    """The class's art, turned to the axis this part's pins are on."""
    cl = classe_do_simbolo(p)
    if cl is None:
        return []
    bruta = _arte_bruta(cl)
    if {q.side for q in p.pins} != {"T", "B"}:
        return bruta
    # a quarter turn: (x, y) -> (-y, x)
    import re as _re

    def gira(m):
        x, y = float(m.group(1)), float(m.group(2))
        return f"{-y:.4f} {x:.4f}"

    return [_re.sub(r"(-?\d+\.\d+) (-?\d+\.\d+)", gira, t) for t in bruta]


@dataclass(frozen=True)
class Pin:
    number: str
    name: str
    etype: str = "passive"
    side: str = "L"  # L, R, T, B


@dataclass
class Part:
    """One component: its reference, its value and its pins."""
    ref: str
    value: str
    pins: tuple[Pin, ...]
    footprint: str = ""
    datasheet: str = ""
    note: str = ""
    # The LCSC catalogue number, because the board is made and assembled at
    # JLCPCB and their BOM is keyed on it, not on the manufacturer's part
    # number. Empty means one of three different things, and the BOM says
    # which: the part is a passive whose value is enough, the part is not
    # fitted (a land pattern, a test point), or nobody has checked yet.
    lcsc: str = ""
    # filled by layout()
    x: float = 0.0
    y: float = 0.0
    w: float = 0.0
    h: float = 0.0

    @property
    def sym_name(self) -> str:
        return f"gnssbike:{self.ref}"

    # ---------------------------------------------------- simbolo do KiCad
    @property
    def kicad(self) -> str:
        """"Biblioteca:Nome" quando a peca usa um simbolo do KiCad."""
        import simbolos
        return simbolos.KICAD.get(self.ref, "")

    def _k_bloco(self) -> str:
        import ksym
        return ksym.bloco(self.kicad)

    def _k_caixa(self) -> tuple[float, float, float, float]:
        """A caixa do simbolo no espaco DELE: desenho mais pontas de pino.

        E aqui que mora um erro que custou um curto-circuito. Os simbolos
        deste projeto sao centrados na origem por construcao, e o roteador de
        fios foi escrito contando com isso: `box()` devolvia um retangulo
        simetrico em volta do ponto onde a peca esta.

        Os simbolos da biblioteca do KiCad NAO sao. Um `Conn_01x10_Socket`
        tem o desenho em x de -1,27 a 0 e os dez pinos todos em x = -5,08; o
        `USB_C_Receptacle_USB2.0_16P` tem pinos de -7,62 a +15,24. Tratados
        como simetricos, alguns pinos caem DENTRO do obstaculo, o roteador
        nao consegue chegar neles pelo lado de fora e acaba passando por
        cima - e dois nos que se encostam viram um so para o KiCad, sem nada
        no desenho dizendo isso. Foi assim que o MPPT engoliu o terra.
        """
        import ksym
        # SO o desenho, sem as pontas dos pinos - exatamente como os simbolos
        # deste projeto, onde  e o corpo e o pino fica PIN_LEN para
        # fora. Conferido: nenhum dos pinos dos simbolos que usamos cai
        # dentro da caixa do desenho, entao todos continuam alcancaveis pelo
        # lado de fora, que e o que o roteador precisa.
        #
        # Incluir as pontas parece mais seguro e nao e: infla cada simbolo em
        # 5 mm por lado, as folhas crescem para caber, e o A* do roteador
        # passa de 26 segundos para mais de oito minutos sem terminar.
        return ksym.caixa(self._k_bloco())

    def avanco_dos_pinos(self) -> tuple[float, float]:
        """Quanto os pinos passam da caixa, a esquerda e a direita.

        Nos simbolos deste projeto e PIN_LEN dos dois lados. Nos da
        biblioteca do KiCad depende: num conector com todos os pinos numa
        borda o avanco e grande de um lado e zero do outro, e e isso que a
        colocacao precisa saber para nao encostar o vizinho nos pinos.
        """
        if not self.kicad:
            return (PIN_LEN, PIN_LEN)
        import ksym
        b = self._k_bloco()
        x0, _y0, x1, _y1 = ksym.caixa(b)
        px = [p[0] for p in ksym.pinos(b).values()] or [0.0]
        return (max(0.0, x0 - min(px)), max(0.0, max(px) - x1))

    def desloca_centro(self) -> tuple[float, float]:
        """Do ponto onde a peca esta ate o centro da caixa dela, na folha."""
        if not self.kicad:
            return (0.0, 0.0)
        x0, y0, x1, y1 = self._k_caixa()
        return (snap((x0 + x1) / 2.0), snap(-(y0 + y1) / 2.0))

    def _sides(self) -> dict[str, list[Pin]]:
        out: dict[str, list[Pin]] = {"L": [], "R": [], "T": [], "B": []}
        for p in self.pins:
            out[p.side].append(p)
        return out

    def eixo(self) -> str:
        """For a part with a symbol: which way its two leads run."""
        lados = {q.side for q in self.pins}
        return "v" if lados == {"T", "B"} else "h"

    def size(self) -> tuple[float, float]:
        """Body size, from the pin counts and the longest name.

        A part with a symbol of its own has the symbol's size, which is small
        and fixed: 2,54 mm of body with the pins 2,54 beyond it. A block is
        what an IC gets, and only there does the size come from counting
        pins and measuring names.
        """
        if self.kicad:
            # O simbolo vem pronto da biblioteca do KiCad: o tamanho e o da
            # caixa dele, e `desloca_centro()` diz onde essa caixa esta em
            # relacao ao ponto da peca - porque ela nao e centrada nele.
            x0, y0, x1, y1 = self._k_caixa()
            arr = lambda v: math.ceil(v / GRID) * GRID
            return (arr(x1 - x0), arr(y1 - y0))
        if classe_do_simbolo(self):
            return (2 * CORPO_2T, 2 * CORPO_2T)
        s = self._sides()
        rows = max(len(s["L"]), len(s["R"]), 1)
        cols = max(len(s["T"]), len(s["B"]), 1)
        longest_lr = max((len(p.name) for p in s["L"] + s["R"]), default=1)
        longest_tb = max((len(p.name) for p in s["T"] + s["B"]), default=1)
        w = max(cols * 2 * GRID + 2 * GRID,
                (longest_lr * 2 + 4) * GRID,
                len(self.value) * GRID + 2 * GRID,
                6 * GRID)
        h = max(rows * 2 * GRID + 2 * GRID, longest_tb * GRID + 4 * GRID, 4 * GRID)
        # Round up to an even number of grid steps, so that half the body is a
        # whole number of steps and every pin tip lands exactly on the grid.
        # Without this the tips fall between grid points and no wire reaches
        # them: KiCad reads the sheet as fully unconnected.
        step = 2 * GRID
        return (math.ceil(w / step - 1e-9) * step, math.ceil(h / step - 1e-9) * step)

    def pin_local(self) -> dict[str, tuple[float, float, int]]:
        """Pin number to its tip in symbol space, and the pin angle."""
        if self.kicad:
            import ksym
            return ksym.pinos(self._k_bloco())
        if classe_do_simbolo(self):
            a = CORPO_2T + PIN_LEN
            p = list(self.pins)
            if len(p) == 1:
                return {p[0].number: (a, 0.0, 180)}
            if self.eixo() == "v":
                return {p[0].number: (0.0, a, 270),
                        p[1].number: (0.0, -a, 90)}
            return {p[0].number: (-a, 0.0, 0), p[1].number: (a, 0.0, 180)}
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        s = self._sides()
        out: dict[str, tuple[float, float, int]] = {}
        for i, p in enumerate(s["L"]):
            y = hh - GRID * 2 - i * 2 * GRID
            out[p.number] = (-hw - PIN_LEN, y, 0)
        for i, p in enumerate(s["R"]):
            y = hh - GRID * 2 - i * 2 * GRID
            out[p.number] = (hw + PIN_LEN, y, 180)
        for i, p in enumerate(s["T"]):
            x = -hw + GRID * 2 + i * 2 * GRID
            out[p.number] = (x, hh + PIN_LEN, 270)
        for i, p in enumerate(s["B"]):
            x = -hw + GRID * 2 + i * 2 * GRID
            out[p.number] = (x, -hh - PIN_LEN, 90)
        return out

    def pin_sheet(self) -> dict[str, tuple[float, float]]:
        """Pin number to its tip on the sheet (y downward).

        The result must be exactly on the grid, or a wire drawn to it does not
        connect. size() guarantees it as long as the part itself is placed on
        the grid, which layout() does.
        """
        out = {}
        for n, (px, py, _a) in self.pin_local().items():
            x, y = self.x + px, self.y - py
            if abs(x / GRID - round(x / GRID)) > 1e-6 or abs(y / GRID - round(y / GRID)) > 1e-6:
                raise AssertionError(f"{self.ref} pino {n} fora da grade: ({x}; {y})")
            out[n] = (round(x, 4), round(y, 4))
        return out

    def box(self) -> tuple[float, float, float, float]:
        """Body rectangle on the sheet: x0, y0, x1, y1."""
        w, h = self.size()
        dx, dy = self.desloca_centro()
        cx, cy = self.x + dx, self.y + dy
        return (cx - w / 2.0, cy - h / 2.0, cx + w / 2.0, cy + h / 2.0)

    def _lib_symbol_kicad(self) -> str:
        """O simbolo da biblioteca do KiCad, com as nossas propriedades.

        Sai o que a biblioteca desenhou - e entra o que e deste projeto: a
        referencia, o valor, o footprint da nossa escolha e a nota. Deixar as
        propriedades da biblioteca faz o KiCad mostrar duas de cada, uma por
        cima da outra, e a da biblioteca aponta para o footprint dela, que
        nem sempre e o nosso.
        """
        import ksym
        nome = self.kicad.split(":", 1)[1]
        b = ksym.sem_propriedades(ksym.bloco(self.kicad))
        b = ksym.renomear(b, nome, self.ref)
        b = b.replace(f'(symbol "{self.ref}"', f'(symbol "{self.sym_name}"', 1)
        b = "\n".join("\t" + ln if ln.strip() else ln for ln in b.split("\n"))
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        props = "\n".join([
            f'\t\t\t(property "Reference" "{esc(self.ref)}"\n\t\t\t\t(at {-hw:.3f} {hh + 1.27:.3f} 0)'
            f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left bottom))\n\t\t\t)',
            f'\t\t\t(property "Value" "{esc(self.value)}"\n\t\t\t\t(at {-hw:.3f} {-hh - 1.27:.3f} 0)'
            f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left top))\n\t\t\t)',
            f'\t\t\t(property "Footprint" "{esc(self.footprint)}"\n\t\t\t\t(at 0 0 0)'
            f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
            f'\t\t\t(property "Datasheet" "{esc(self.datasheet)}"\n\t\t\t\t(at 0 0 0)'
            f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
            f'\t\t\t(property "Description" "{esc(self.note)}"\n\t\t\t\t(at 0 0 0)'
            f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)'])
        # as propriedades entram logo depois do cabecalho, antes da primeira
        # unidade de desenho
        i = b.index(f'\t\t\t(symbol "{self.ref}_')
        return b[:i] + props + "\n" + b[i:]

    def lib_symbol(self) -> str:
        if self.kicad:
            return self._lib_symbol_kicad()
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        loc = self.pin_local()
        # A two terminal symbol shows neither pin numbers nor pin names:
        # "1" and "2" on a resistor are noise, and the standard symbol already
        # says which end is which where it matters (the cathode bar, the
        # curved plate).
        esconde = "\n\t\t\t(pin_numbers hide)" if classe_do_simbolo(self) else ""
        nomes = ("(pin_names (offset 0.508) hide)" if classe_do_simbolo(self)
                 else "(pin_names (offset 0.508))")
        out = [f'\t\t(symbol "{self.sym_name}"{esconde}',
               f'\t\t\t{nomes}',
               '\t\t\t(exclude_from_sim no)\n\t\t\t(in_bom yes)\n\t\t\t(on_board yes)',
               f'\t\t\t(property "Reference" "{esc(self.ref)}"\n\t\t\t\t(at {-hw:.3f} {hh + 1.27:.3f} 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left bottom))\n\t\t\t)',
               f'\t\t\t(property "Value" "{esc(self.value)}"\n\t\t\t\t(at {-hw:.3f} {-hh - 1.27:.3f} 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left top))\n\t\t\t)',
               f'\t\t\t(property "Footprint" "{esc(self.footprint)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(property "Datasheet" "{esc(self.datasheet)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(property "Description" "{esc(self.note)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(symbol "{self.ref}_0_1"']
        arte = arte_do_simbolo(self)
        if arte:
            out += arte
        else:
            out.append(
                f'\t\t\t\t(rectangle\n\t\t\t\t\t(start {-hw:.3f} {-hh:.3f})'
                f'\n\t\t\t\t\t(end {hw:.3f} {hh:.3f})'
                '\n\t\t\t\t\t(stroke (width 0.254) (type default))'
                '\n\t\t\t\t\t(fill (type background))\n\t\t\t\t)')
        out += ['\t\t\t)',
               f'\t\t\t(symbol "{self.ref}_1_1"']
        for p in self.pins:
            px, py, ang = loc[p.number]
            out.append(
                f'\t\t\t\t(pin {p.etype} line\n\t\t\t\t\t(at {px:.3f} {py:.3f} {ang})'
                f'\n\t\t\t\t\t(length {PIN_LEN})'
                f'\n\t\t\t\t\t(name "{esc(p.name)}" (effects (font (size 1.016 1.016))))'
                f'\n\t\t\t\t\t(number "{esc(p.number)}" (effects (font (size 1.016 1.016))))'
                '\n\t\t\t\t)')
        out += ['\t\t\t)', '\t\t)']
        return "\n".join(out)

    def instance(self, project: str, path: str) -> str:
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        u = uid("inst", self.ref)
        pins = "\n".join(f'\t\t(pin "{esc(p.number)}" (uuid "{uid("pin", self.ref, p.number)}"))'
                         for p in self.pins)
        return (f'\t(symbol\n\t\t(lib_id "{self.sym_name}")\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} 0)\n\t\t(unit 1)\n'
                '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
                f'\t\t(uuid "{u}")\n'
                f'\t\t(property "Reference" "{esc(self.ref)}"\n'
                f'\t\t\t(at {self.x - hw:.3f} {self.y - hh - 1.27:.3f} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left bottom))\n\t\t)\n'
                f'\t\t(property "Value" "{esc(self.value)}"\n'
                f'\t\t\t(at {self.x - hw:.3f} {self.y + hh + 1.27:.3f} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left top))\n\t\t)\n'
                f'\t\t(property "Footprint" "{esc(self.footprint)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'\t\t(property "Datasheet" "{esc(self.datasheet)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'\t\t(property "Description" "{esc(self.note)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'{pins}\n'
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "{path}"\n\t\t\t\t\t(reference "{self.ref}")\n'
                '\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


@dataclass
class Router:
    """A* on the schematic grid, with the parts as obstacles."""
    parts: list[Part]
    margin: float = GRID
    turn_cost: float = 0.6
    cross_cost: float = 6.0

    blocked: set[tuple[int, int]] = field(default_factory=set)
    used: dict[tuple[int, int], set[str]] = field(default_factory=dict)
    # cell -> net -> the orientations that net occupies there, "H" and/or "V".
    # A cell where a net turns holds both, and no other net may cross there.
    dirs: dict[tuple[int, int], dict[str, set[str]]] = field(default_factory=dict)
    pin_cells: set[tuple[int, int]] = field(default_factory=set)
    segs: list[tuple[tuple[float, float], tuple[float, float], str]] = field(
        default_factory=list)

    def key(self, x: float, y: float) -> tuple[int, int]:
        return (int(round(x / GRID)), int(round(y / GRID)))

    def pos(self, k: tuple[int, int]) -> tuple[float, float]:
        return (k[0] * GRID, k[1] * GRID)

    def build_obstacles(self) -> None:
        self.blocked.clear()
        for p in self.parts:
            x0, y0, x1, y1 = p.box()
            kx0, ky0 = self.key(x0 - self.margin, y0 - self.margin)
            kx1, ky1 = self.key(x1 + self.margin, y1 + self.margin)
            for kx in range(kx0, kx1 + 1):
                for ky in range(ky0, ky1 + 1):
                    self.blocked.add((kx, ky))
        # a pin tip must stay reachable even though it sits next to its body,
        # and no other net may pass over it: a wire touching a pin tip
        # connects to that pin, whatever the drawing looks like
        self.pin_cells.clear()
        for p in self.parts:
            folha = p.pin_sheet()
            local = p.pin_local()
            bx0, by0, bx1, by1 = p.box()
            for n, xy in folha.items():
                self.blocked.discard(self.key(*xy))
                self.pin_cells.add(self.key(*xy))
                # E o TALO do pino, da ponta ate o corpo. Um fio que cruza o
                # talo encosta no pino tanto quanto um que cruza a ponta - o
                # KiCad junta os dois nos e nada no desenho diz isso.
                #
                # Nos simbolos deste projeto o talo tem uma celula e quase
                # nunca da problema. Nos da biblioteca do KiCad a folga entre
                # o corpo e a ponta chega a tres celulas: num
                # `Conn_01x10_Socket` o desenho vai de x -1,27 a 0 e os dez
                # pinos ficam em x = -5,08, entao um unico fio descendo em
                # x = -2,54 cruza os DEZ talos. Foi assim que o MPPT e o
                # terra viraram um no so.
                ang = local[n][2]
                dx, dy = {0: (1, 0), 90: (0, -1),
                          180: (-1, 0), 270: (0, 1)}.get(ang, (0, 0))
                cx, cy = xy
                for _ in range(8):
                    cx += dx * GRID
                    cy += dy * GRID
                    # a celula entra na protecao ANTES do teste de parada: a
                    # ultima do talo, encostada no corpo, tambem e talo, e
                    # deixa-la de fora abre exatamente a fresta por onde um
                    # fio passa raspando em todos os pinos de uma borda
                    self.pin_cells.add(self.key(cx, cy))
                    if bx0 <= cx <= bx1 and by0 <= cy <= by1:
                        break

    def route(self, net: str, a: tuple[float, float],
              targets: set[tuple[int, int]], folga: int = 90) -> list[tuple[int, int]] | None:
        """Shortest path from a to any cell of targets, as a list of cells."""
        start = self.key(*a)
        if start in targets:
            return [start]
        openq: list[tuple[float, float, tuple[int, int], tuple[int, int] | None]] = []
        tx = sum(t[0] for t in targets) / len(targets)
        ty = sum(t[1] for t in targets) / len(targets)
        # Search inside a box around the start and the targets, widened by a
        # detour allowance. Without it A* wanders the whole sheet on the nets
        # that cannot be reached directly, and the run takes tens of minutes.
        bx0 = min([start[0]] + [t[0] for t in targets]) - folga
        bx1 = max([start[0]] + [t[0] for t in targets]) + folga
        by0 = min([start[1]] + [t[1] for t in targets]) - folga
        by1 = max([start[1]] + [t[1] for t in targets]) + folga
        heapq.heappush(openq, (abs(start[0] - tx) + abs(start[1] - ty), 0.0, start, None))
        came: dict[tuple[int, int], tuple[int, int] | None] = {}
        seen: dict[tuple[int, int], float] = {start: 0.0}
        while openq:
            _f, g, cur, prev = heapq.heappop(openq)
            if cur in came:
                continue
            came[cur] = prev
            if cur in targets:
                path = [cur]
                while came[path[-1]] is not None:
                    path.append(came[path[-1]])
                path.reverse()
                return path
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nxt = (cur[0] + dx, cur[1] + dy)
                if nxt in came:
                    continue
                if not (bx0 <= nxt[0] <= bx1 and by0 <= nxt[1] <= by1):
                    continue
                if nxt in self.blocked and nxt not in targets:
                    continue
                if (nxt in self.pin_cells and nxt not in targets
                        and net not in self.used.get(nxt, set())):
                    continue
                step = 1.0
                owners = self.used.get(nxt, set())
                if owners and net not in owners:
                    # only a clean right-angle crossing of another net is
                    # allowed: never a shared run, never their corner, and
                    # never an end of ours on their wire
                    mine = "H" if dy == 0 else "V"
                    alheio = set()
                    for other, ds in self.dirs.get(nxt, {}).items():
                        if other != net:
                            alheio |= ds
                    if mine in alheio or len(alheio) > 1 or nxt in targets:
                        continue
                    step += self.cross_cost
                if prev is not None and (cur[0] - prev[0], cur[1] - prev[1]) != (dx, dy):
                    step += self.turn_cost
                ng = g + step
                if ng < seen.get(nxt, float("inf")):
                    seen[nxt] = ng
                    h = abs(nxt[0] - tx) + abs(nxt[1] - ty)
                    heapq.heappush(openq, (ng + h, ng, nxt, cur))
        return None

    def add_path(self, net: str, path: list[tuple[int, int]]) -> None:
        for i, k in enumerate(path):
            self.used.setdefault(k, set()).add(net)
            d = self.dirs.setdefault(k, {}).setdefault(net, set())
            for j in (i - 1, i + 1):
                if 0 <= j < len(path):
                    d.add("H" if path[j][1] == k[1] else "V")
            if len(path) == 1:
                d |= {"H", "V"}
        # collapse the cell path into straight segments
        if len(path) < 2:
            return
        run = [path[0]]
        d = (path[1][0] - path[0][0], path[1][1] - path[0][1])
        for i in range(1, len(path)):
            nd = (path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1])
            if nd != d:
                self.segs.append((self.pos(run[0]), self.pos(path[i - 1]), net))
                run = [path[i - 1]]
                d = nd
        self.segs.append((self.pos(run[0]), self.pos(path[-1]), net))


@dataclass
class PowerPort:
    """A supply or ground symbol: the professional way to carry a rail.

    KiCad joins every power symbol of the same value into one net across the
    whole design, which is what keeps a ground with eighty pins from being
    drawn as eighty lines to one rail.
    """
    net: str
    x: float
    y: float
    ground: bool = False   # graphic points down and the pin faces up
    ref: str = "#PWR"

    @property
    def sym_name(self) -> str:
        return f"power:{self.net}"

    def pin_sheet(self) -> tuple[float, float]:
        return (self.x, self.y)

    def lib_symbol(self) -> str:
        if self.ground:
            # the classic three bars, drawn below the connection point
            art = ('\t\t\t\t(polyline (pts (xy 0 0) (xy 0 -1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.905 -1.27) (xy 1.905 -1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.143 -1.905) (xy 1.143 -1.905))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -0.508 -2.54) (xy 0.508 -2.54))'
                   ' (stroke (width 0) (type default)) (fill (type none)))')
            pin_ang, val_y, val_just = 90, -3.81, "top"
        else:
            art = ('\t\t\t\t(polyline (pts (xy 0 0) (xy 0 1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.27 1.27) (xy 0 2.54) (xy 1.27 1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))')
            pin_ang, val_y, val_just = 270, 3.81, "bottom"
        return (f'\t\t(symbol "{self.sym_name}"\n\t\t\t(power)\n'
                '\t\t\t(pin_names (offset 0))\n'
                '\t\t\t(exclude_from_sim no)\n\t\t\t(in_bom no)\n\t\t\t(on_board yes)\n'
                f'\t\t\t(property "Reference" "{self.ref}"\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                f'\t\t\t(property "Value" "{esc(self.net)}"\n\t\t\t\t(at 0 {val_y} 0)\n'
                f'\t\t\t\t(effects (font (size 1.27 1.27)) (justify {val_just}))\n\t\t\t)\n'
                '\t\t\t(property "Footprint" ""\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                '\t\t\t(property "Datasheet" ""\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                f'\t\t\t(symbol "{self.net}_0_1"\n{art}\n\t\t\t)\n'
                f'\t\t\t(symbol "{self.net}_1_1"\n'
                f'\t\t\t\t(pin power_in line\n\t\t\t\t\t(at 0 0 {pin_ang})\n'
                '\t\t\t\t\t(length 0)\n'
                f'\t\t\t\t\t(name "{esc(self.net)}" (effects (font (size 1.27 1.27))))\n'
                '\t\t\t\t\t(number "1" (effects (font (size 1.27 1.27))))\n'
                '\t\t\t\t)\n\t\t\t)\n\t\t)')

    def instance(self, project: str, path: str) -> str:
        u = uid("pwr", self.net, self.x, self.y)
        val_y = self.y + (3.81 if self.ground else -3.81)
        return (f'\t(symbol\n\t\t(lib_id "{self.sym_name}")\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} 0)\n\t\t(unit 1)\n'
                '\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
                f'\t\t(uuid "{u}")\n'
                f'\t\t(property "Reference" "{self.ref}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(property "Value" "{esc(self.net)}"\n'
                f'\t\t\t(at {self.x:.3f} {val_y:.3f} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
                f'\t\t(property "Footprint" ""\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(property "Datasheet" ""\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(pin "1" (uuid "{uid("pwrpin", self.net, self.x, self.y)}"))\n'
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "{path}"\n\t\t\t\t\t(reference "{self.ref}")\n'
                '\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


@dataclass
class HierLabel:
    """A signal leaving the sheet, and the sheet pin it answers to on the root."""
    name: str
    x: float
    y: float
    angle: int = 0     # 0 text to the right, 180 text to the left
    shape: str = "bidirectional"

    def render(self) -> str:
        just = "left" if self.angle == 0 else "right"
        return (f'\t(hierarchical_label "{esc(self.name)}"\n\t\t(shape {self.shape})\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} {self.angle})\n'
                f'\t\t(effects (font (size 1.27 1.27)) (justify {just}))\n'
                f'\t\t(uuid "{uid("hl", self.name, self.x, self.y)}")\n\t)')


@dataclass
class SheetSymbol:
    """One block on the root sheet."""
    name: str
    filename: str
    x: float
    y: float
    w: float
    h: float
    page: str
    pins: list[tuple[str, float, int, str]] = field(default_factory=list)  # name, y, ang, shape

    @property
    def uuid(self) -> str:
        return uid("sheetsym", self.name)

    def pin_pos(self, name: str) -> tuple[float, float]:
        for n, py, ang, _s in self.pins:
            if n == name:
                return (self.x if ang == 180 else self.x + self.w, py)
        raise KeyError(name)

    def render(self, project: str, root_uuid: str) -> str:
        pins = "\n".join(
            f'\t\t(pin "{esc(n)}" {shape}\n'
            f'\t\t\t(at {self.x if ang == 180 else self.x + self.w:.3f} {py:.3f} {ang})\n'
            f'\t\t\t(effects (font (size 1.27 1.27)) '
            f'(justify {"left" if ang == 180 else "right"}))\n'
            f'\t\t\t(uuid "{uid("sp", self.name, n)}")\n\t\t)'
            for n, py, ang, shape in self.pins)
        return (f'\t(sheet\n\t\t(at {self.x:.3f} {self.y:.3f})\n'
                f'\t\t(size {self.w:.3f} {self.h:.3f})\n'
                '\t\t(stroke (width 0.2) (type solid))\n'
                '\t\t(fill (color 0 0 0 0.0000))\n'
                f'\t\t(uuid "{self.uuid}")\n'
                f'\t\t(property "Sheetname" "{esc(self.name)}"\n'
                f'\t\t\t(at {self.x:.3f} {self.y - 1.0:.3f} 0)\n'
                '\t\t\t(effects (font (size 2.0 2.0) (bold yes)) (justify left bottom))\n\t\t)\n'
                f'\t\t(property "Sheetfile" "{esc(self.filename)}"\n'
                f'\t\t\t(at {self.x:.3f} {self.y + self.h + 1.0:.3f} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (justify left top))\n\t\t)\n'
                + (pins + "\n" if pins else "") +
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "/{root_uuid}"\n\t\t\t\t\t(page "{self.page}")\n'
                '\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


class Schematic:
    def __init__(self, project: str, paper: tuple[float, float], *,
                 name: str = "", root_uuid: str | None = None,
                 title: str = "", rev: str = "", date: str = "") -> None:
        self.project = project
        self.paper = paper
        self.uuid = uid("sheet", project, name)
        self.root_uuid = root_uuid          # None means this file IS the root
        self.sheet_symbol_uuid: str | None = None
        self.title, self.rev, self.date = title, rev, date
        self.parts: list[Part] = []
        self.powers: list[PowerPort] = []
        self.labels: list[HierLabel] = []
        self.sheets: list[SheetSymbol] = []
        self.wires: list[tuple[tuple[float, float], tuple[float, float]]] = []
        self.junctions: list[tuple[float, float]] = []
        self.texts: list[tuple[float, float, str, float]] = []

    @property
    def inst_path(self) -> str:
        if self.root_uuid is None:
            return f"/{self.uuid}"
        return f"/{self.root_uuid}/{self.sheet_symbol_uuid}"

    def text(self, x: float, y: float, s: str, size: float = 2.0) -> None:
        self.texts.append((x, y, s, size))

    def render(self) -> str:
        libs_parts = [p.lib_symbol() for p in self.parts]
        vistos: set[str] = set()
        for pw in self.powers:
            if pw.sym_name not in vistos:
                vistos.add(pw.sym_name)
                libs_parts.append(pw.lib_symbol())
        libs = "\n".join(libs_parts)
        insts = "\n".join(
            [p.instance(self.project, self.inst_path) for p in self.parts]
            + [pw.instance(self.project, self.inst_path) for pw in self.powers])
        wires = "\n".join(
            f'\t(wire\n\t\t(pts\n\t\t\t(xy {a[0]:.3f} {a[1]:.3f}) (xy {b[0]:.3f} {b[1]:.3f})\n\t\t)'
            f'\n\t\t(stroke (width 0) (type default))\n\t\t(uuid "{uid("w", a, b)}")\n\t)'
            for a, b in self.wires)
        juncs = "\n".join(
            f'\t(junction\n\t\t(at {x:.3f} {y:.3f})\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)'
            f'\n\t\t(uuid "{uid("j", x, y)}")\n\t)' for x, y in self.junctions)
        texts = "\n".join(
            f'\t(text "{esc(s)}"\n\t\t(exclude_from_sim no)\n\t\t(at {x:.3f} {y:.3f} 0)'
            f'\n\t\t(effects (font (size {sz} {sz})) (justify left bottom))'
            f'\n\t\t(uuid "{uid("tx", x, y, s)}")\n\t)' for x, y, s, sz in self.texts)
        rotulos = "\n".join(lb.render() for lb in self.labels)
        folhas = "\n".join(sh.render(self.project, self.uuid) for sh in self.sheets)
        bloco = ""
        if self.title:
            bloco = (f'\t(title_block\n\t\t(title "{esc(self.title)}")\n'
                     + (f'\t\t(date "{esc(self.date)}")\n' if self.date else "")
                     + (f'\t\t(rev "{esc(self.rev)}")\n' if self.rev else "")
                     + '\t\t(company "GNSS Bike Computer")\n\t)\n')
        parts = [f'(kicad_sch\n\t(version 20231120)\n\t(generator "eeschema")\n'
                 f'\t(generator_version "8.0")\n\t(uuid "{self.uuid}")\n'
                 + (f'\t(paper "{self.paper}")\n' if isinstance(self.paper, str)
                    else f'\t(paper "User" {self.paper[0]} {self.paper[1]})\n')
                 + bloco +
                 f'\t(lib_symbols\n{libs}\n\t)']
        for chunk in (folhas, wires, juncs, rotulos, texts, insts):
            if chunk:
                parts.append(chunk)
        if self.root_uuid is None:
            paginas = '\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n'
            for sh in self.sheets:
                paginas += (f'\t\t(path "/{sh.uuid}"\n\t\t\t(page "{sh.page}")\n\t\t)\n')
            parts.append(f'\t(sheet_instances\n{paginas}\t)\n)')
        else:
            parts.append('\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n)')
        return "\n".join(parts) + "\n"
