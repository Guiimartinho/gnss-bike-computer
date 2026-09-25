#!/usr/bin/env python3
"""Generate the DXF files that carry this project's board geometry into a CAD tool.

Everything written here comes from a document in this repository; nothing is
invented. Each rectangle carries the document and the section it came from in
LAYERS below, and a value that no document gives is written as a layer name
ending in _CONFERIR, so that it cannot be mistaken for a decided number.

Output (hardware_gnssbike/cad/):
  contorno-r3.dxf   board outline only, 3 mm corner radius (case_drawing.py)
  contorno-r4.dxf   board outline only, 4 mm corner radius (docs/14)
  zonas.dxf         mounting holes, keep-outs, shadows and placement zones

Coordinates: the documents put the origin at the top left of the board seen
from the front, with y growing downward. CAD puts y upward, so this script
writes y_dxf = 97 - y_doc. The board looks the same on screen (the GNSS
antenna edge stays at the top); only the number changes.

Run:  python hardware_gnssbike/cad/make_dxf.py
Check: python hardware_gnssbike/cad/check_dxf.py
"""

from __future__ import annotations

import math
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent

# The board outline. NOT the case's, and not derived from it: the case's
# inside measurement has no say over how big this board is, any more than this
# board has a say over how big the case is. The two are independent, and the
# owner has had to say so twice.
#
# What DOES set it is the circuit, and inside the circuit one rule dominates:
# 7.2 of the ME54BS13 datasheet asks for at least 50 mm between two radio
# modules on the same board, and this board has two - the ME54BS13 itself and
# the MAX-F10S. With the module lying in one corner (17.0 x 13.5 courtyard)
# and the receiver in the opposite one (10.4 x 10.6, below the 8 mm antenna
# keep-out), the 50 mm between their courtyards is what fixes the long side:
#
# Narrower is smaller in area, because the 50 mm is spent along the long axis
# either way, and the search over every whole millimetre from 32 x 60 to
# 55 x 99 that still clears the rule AND still fits the row of three keys
# across the width (3 x 10,2 = 30,6 mm plus 0,8 mm of margin each side) gives:
#
#     32 x 89 = 2.848 mm2   modulos a 50,8 mm
#     33 x 89 = 2.937 mm2   modulos a 50,9 mm
#     34 x 88 = 2.992 mm2   modulos a 50,9 mm
#     44 x 86 = 3.784 mm2   com a fila de teclas ainda na borda de baixo
#
# 34 x 90 is the size taken: 3.060 mm2 against the 5.335 of the 55 x 97 that
# was typed by hand - 43% less board. Two millimetres over the 88 the rule
# alone allows, and they are not slack: at 88 the receiver had to sit hard
# against the left edge to reach the 50 mm, and then its own 1V8 pins, which
# come out on that side, had 1,2 mm of board to put a decoupling capacitor
# in. The two millimetres buy the receiver 2,5 mm of room on its left.
#
# GNSSBIKE_W and GNSSBIKE_H override both, so the size can be searched
# without editing the file.
W = float(os.environ.get("GNSSBIKE_W", 34.0))
H = float(os.environ.get("GNSSBIKE_H", 90.0))
THICKNESS = 0.8

# The two sources disagree on the corner radius; both are written out.
RADIUS_DRAWING = 3.0  # tools/docs/case_drawing.py:339, rect(3.5, 3.5, W-7, H-7, 3)
RADIUS_DOC14 = 4.0  # docs/14-hardware-placa-nova.md#placa-de-circuito-impresso

# Mounting hole, board coordinates. The owner decided on 2026-09-23 for a
# SINGLE hole instead of the four of the case drawing, to leave area for parts
# and tracks. The point is the freest on the board, computed by hole_spot() in
# make_pcb.py: outside every zone, every keep-out and both shadows, and,
# among the tied points, the closest to the centre of the board, because with
# one screw the distance to the centre is the lever arm.
FUROS_DOC = [(3.2, H * 0.5)]

# The four screws the case drawing still has, at (6.5, 12.5), (55.5, 12.5),
# (6.5, 91.5) and (55.5, 91.5) in case coordinates, are 3.5 mm inside on every
# side: (3.0, 9.0), (52.0, 9.0), (3.0, 88.0) and (52.0, 88.0) on the board.
# They are kept here only as the geometry the case drawing still shows; the
# board carries one hole. The two are not reconciled in any document yet.
SCREWS_CASE_DRAWING: list[tuple[float, float]] = []

# No document in this repository gives the drill diameter or the pad of the M2
# holes. 2.2 mm is the usual clearance hole for an M2 screw and is drawn here
# only so that something exists to snap to; the layer name says so.
M2_DRILL_UNVERIFIED = 2.2

# Placement zones and keep-outs, in document coordinates (x0, y0, x1, y1),
# y downward. Source of each one in the comment.
# The keep-out of the module's antenna, and the notch under it. Both follow
# the module, which lies in the bottom right corner with its antenna over the
# board edge - figure 1 of section 7.5 calls that placement "Best".
#
#   ANT_MOD   4,7 mm of board edge kept clear beside the antenna
#   RECORTE   the hollow under the antenna area itself, 7.4
_ANT_FAIXA = 4.7
_MOD_ALT = 13.5          # the module lying down: 17,0 wide by 13,5
_MOD_LARG = 17.0


def _f(x0: float, y0: float, x1: float, y1: float) -> tuple:
    """A rectangle, clipped to the board."""
    return (max(0.0, x0), max(0.0, y0), min(W, x1), min(H, y1))


# The floorplan, derived from W and H instead of written out.
#
# The vertical budget is what the RULES leave, read top to bottom:
#
#   0 .. 8            keep-out of the GNSS antenna, which lives in the case
#                     wall and lands on J302; no copper on any layer
#   8,5 .. 20         the receiver and its pi network, as close under the
#                     antenna pads as the keep-out allows (MAX-F10S 4.4:
#                     "as short as possible")
#   21 .. 32          flash, IMU and magnetometer: no switching, no RF
#   33 .. H-35        POWER. It has to end 20 mm above the module, because
#                     7.2 asks 20 mm between the module and a switching
#                     supply or a power inductor, and on a board this size
#                     that single rule decides where the supply may live
#   H-34 .. H-24      the two flat cables and the buzzer
#   H-23 .. H-15      the three keys, in a row across the width
#   H-14 .. H         the USB-C on the left of the bottom edge and the radio
#                     module on the right of it, which is the only pair that
#                     fits there and the only corner the module may have
ZONES = [
    # name, rect, colour, source
    # O recorte da antena GNSS encolheu MUITO em 2026-09-24, quando a antena
    # passou da TE L000670-01 (estoque zero) para a Unictron
    # H2UJ4U1H2Q0100. A TE pedia 40,5 x 14,5 mm, mais larga que esta placa
    # inteira; a Unictron pede 15,00 x 9,35 na face de cima e 15,00 x 9,88 na
    # de baixo, medidos da BORDA DA PLACA.
    #
    # O recorte e ASSIMETRICO, e isso e de proposito da Unictron: a antena
    # ocupa 5,0 mm no meio, com 2,46 mm de recorte de um lado e 7,54 do
    # outro. Nao e artefato de canto de placa - na placa de avaliacao de
    # 80 x 40 o recorte fica no MEIO, a 30 mm de uma borda lateral e 35 da
    # outra, entao havia espaco de sobra para centra-lo e nao centraram.
    #
    # Qual lado leva os 7,54: o da letra "U" impressa no topo da peca. O "U"
    # e descentrado 1,10 mm (22 % do comprimento) e cai na porcao voltada
    # para o recorte largo, o que confere em duas figuras independentes - o
    # desenho de dimensoes e a figura da placa de avaliacao. RESSALVA: o "U"
    # e o logotipo da Unictron, e a ficha NAO o declara como marca de
    # orientacao; e leitura de figura, nao afirmacao do fabricante. A ficha
    # tambem nao numera os pads em vista nenhuma e nao traz nota de keep-out
    # minimo - remete a uma nota de aplicacao que nao esta no PDF.
    #
    # Aqui o lado largo aponta para +x, e o estreito para -x: assim as duas
    # ilhas de cobre que sobram na faixa tem 8,3 e 10,7 mm, em vez de uma de
    # 3,2 mm que nao serve de plano para nada.
    #
    # ATENCAO DE MONTAGEM: a peca e FISICAMENTE SIMETRICA - as duas pontas
    # sao wrap-around iguais e o sinal fica no centro geometrico -, entao o
    # pick-and-place nao distingue a orientacao pela geometria, e uma peca
    # girada 180 graus fica com o lado errado no recorte largo sem nenhum
    # sinal no teste eletrico. Dai a marca de orientacao na serigrafia.
    #
    # A profundidade e 9,88, a da face de BAIXO, que e a maior das duas: uma
    # zona so vale para todas as camadas.
    ("KEEPOUT_ANTENA_GNSS", _f(13.25 - 4.96, 0.0, 13.25 + 10.04, 9.88), 1,
     "04#zonas-proibidas: sem cobre em nenhuma camada. Unictron "
     "H2UJ4U1H2Q0100, guia de layout da ficha rev. E"),
    ("KEEPOUT_ANTENA_MODULO",
     _f(W - _ANT_FAIXA, H - _MOD_ALT - 8.5, W, H), 1,
     "ficha ME54BS13 V1.0.0, 7.3 e 7.4: sobre a area da antena nao pode cobre, "
     "componente nem caixa metalica fechada, e 3 a 5 mm em volta dela nao "
     "pode trilha de sinal, metal nem fonte de interferencia"),
    ("RECORTE_ANTENA_MODULO",
     _f(W - _ANT_FAIXA + 0.4, H - _MOD_ALT - 4.1, W, H - _MOD_ALT + 6.1), 2,
     "ficha ME54BS13 V1.0.0, 7.4: a placa sob a area da antena e VAZADA, para "
     "deixar a regiao suspensa. Comeca 0,4 mm dentro da faixa: a ultima coluna "
     "de pads LGA do modulo tem de manter 0,3 mm de cobre a borda do corte"),
    ("ZONA_GNSS_MAX-F10S", _f(W / 2 - 14.0, 10.5, W / 2 + 8.0, 22.0), 3,
     "MAX-F10S IM 4.4: o receptor logo abaixo da zona da antena, com a rede pi "
     "entre o pino RF_IN e o contato de mola"),
    ("ZONA_LUZ_AMBIENTE_OPT3001", _f(1.2, 10.5, 3.8, 13.5), 3,
     "OPT3001 SBOS681B: sob a janela, e longe de peca alta (reflexao "
     "optica secundaria)"),
    # Alargada em 2026-09-24: o LED RGB passou de 1,6 x 1,6 para
    # 3,5 x 2,8 mm, porque o APTF1616 saiu de linha e o que sobrou tem
    # 305 pecas. Com folga de contorno o corpo pede 5,2 x 3,3, e a zona
    # de 4,0 x 3,5 que estava aqui nao o continha.
    ("ZONA_LED_RGB", _f(W - 5.6, 8.1, W - 0.2, 11.8), 3,
     "sob o guia de luz, do lado oposto ao sensor de luz"),
    ("ZONA_FLASH_MX25R6435F", _f(2.5, 21.0, W / 2 - 1.5, 32.0), 3,
     "flash NOR: fala SPI com o modulo, fora da faixa de energia"),
    ("ZONA_IMU_MAGNETOMETRO", _f(W / 2 + 1.5, 21.0, W - 2.5, 32.0), 3,
     "BMI270 e MMC5633NJL: longe das duas antenas e das correntes de "
     "chaveamento"),
    ("ZONA_ENERGIA", _f(2.0, 33.0, W - 2.0, H - 35.0), 3,
     "nPM1300, MAX17262, AEM10900, TPS7A02 e os indutores. O limite de baixo "
     "nao e estetico: 7.2 pede 20 mm entre o modulo e uma fonte chaveada ou "
     "um indutor de potencia, e o modulo comeca em H-13,5"),
    ("ZONA_FPC_DISPLAY_J401", _f(0.8, H - 34.0, 9.0, H - 24.0), 3,
     "cabo plano do display, 10 vias, saindo pela esquerda"),
    ("ZONA_BUZZER", _f(1.0, H - 28.0, W - 1.0, H - 17.0), 3,
     "buzzer piezo, na FACE DE TRAS: 10,5 x 9,5 mm nao cabem na faixa de "
     "9,25 mm que sobra na frente entre a fila de teclas e o modulo"),
    ("ZONA_BAROMETRO_BMP585", _f(2.0, H - 23.0, 6.0, H - 19.0), 3,
     "BMP585 na face de tras, no respiro"),
    ("ZONA_BOTOES", _f(1.5, H - 23.0, W - 6.0, H - 15.0), 3,
     "3 teclas Omron B3S-1002P em fila: 3 x 10,2 mm de passo"),
    ("ZONA_USB_C", _f(1.5, H - 9.5, 12.5, H), 3,
     "Molex 2036150003, boca na borda de baixo, a esquerda do modulo"),
    ("ZONA_MODULO_ME54BS13", _f(W - _MOD_LARG, H - _MOD_ALT, W, H), 3,
     "MinewSemi ME54BS13, 16,5 x 12,0 mm, deitado no canto de baixo a direita "
     "com a antena sobre o recorte"),
]



# Overlaps that the documents already flag as unresolved, drawn so that they
# are visible in the CAD tool instead of having to be recomputed by eye.
# Overlaps the documents flag as unresolved. The list is empty since the
# floorplan stopped being a set of rectangles typed by hand: every one of the
# four that used to be here came from the 55 x 97 outline, and three of them
# were the display's and the battery's shadows fighting the parts - shadows
# that do not belong in a board floorplan at all, because what sits over the
# board is a question for the mechanical layout, not for the board's size.
CONFLITOS: list[tuple] = []


def y(v: float) -> float:
    """Document y (downward, origin at the top) to CAD y (upward)."""
    return H - v


class Dxf:
    """The smallest DXF an importer accepts: R12, LINE, ARC and CIRCLE only."""

    def __init__(self) -> None:
        self.layers: dict[str, int] = {}
        self.ents: list[str] = []

    def layer(self, name: str, colour: int = 7) -> str:
        self.layers.setdefault(name, colour)
        return name

    def line(self, lay: str, x1: float, y1: float, x2: float, y2: float) -> None:
        self.ents += ["0", "LINE", "8", lay,
                      "10", f"{x1:.4f}", "20", f"{y1:.4f}", "30", "0.0",
                      "11", f"{x2:.4f}", "21", f"{y2:.4f}", "31", "0.0"]

    def arc(self, lay: str, cx: float, cy: float, r: float, a0: float, a1: float) -> None:
        self.ents += ["0", "ARC", "8", lay,
                      "10", f"{cx:.4f}", "20", f"{cy:.4f}", "30", "0.0",
                      "40", f"{r:.4f}", "50", f"{a0:.4f}", "51", f"{a1:.4f}"]

    def circle(self, lay: str, cx: float, cy: float, r: float) -> None:
        self.ents += ["0", "CIRCLE", "8", lay,
                      "10", f"{cx:.4f}", "20", f"{cy:.4f}", "30", "0.0",
                      "40", f"{r:.4f}"]

    def rect(self, lay: str, x0: float, y0: float, x1: float, y1: float) -> None:
        self.line(lay, x0, y0, x1, y0)
        self.line(lay, x1, y0, x1, y1)
        self.line(lay, x1, y1, x0, y1)
        self.line(lay, x0, y1, x0, y0)

    def cross(self, lay: str, cx: float, cy: float, arm: float) -> None:
        self.line(lay, cx - arm, cy, cx + arm, cy)
        self.line(lay, cx, cy - arm, cx, cy + arm)

    def text(self) -> str:
        head = ["0", "SECTION", "2", "HEADER",
                "9", "$ACADVER", "1", "AC1009",
                "9", "$INSUNITS", "70", "4",            # 4 = millimetres
                "9", "$MEASUREMENT", "70", "1",         # 1 = metric
                "9", "$EXTMIN", "10", "0.0", "20", "0.0", "30", "0.0",
                "9", "$EXTMAX", "10", f"{W:.4f}", "20", f"{H:.4f}", "30", "0.0",
                "0", "ENDSEC"]
        tab = ["0", "SECTION", "2", "TABLES",
               "0", "TABLE", "2", "LAYER", "70", str(len(self.layers))]
        for name, colour in self.layers.items():
            tab += ["0", "LAYER", "2", name, "70", "0", "62", str(colour),
                    "6", "CONTINUOUS"]
        tab += ["0", "ENDTAB", "0", "ENDSEC"]
        body = ["0", "SECTION", "2", "ENTITIES"] + self.ents + ["0", "ENDSEC", "0", "EOF"]
        return "\r\n".join(head + tab + body) + "\r\n"


def outline(d: Dxf, lay: str, r: float) -> None:
    """Rounded rectangle 0,0 to W,H, four lines and four arcs."""
    d.line(lay, r, 0.0, W - r, 0.0)
    d.line(lay, W, r, W, H - r)
    d.line(lay, W - r, H, r, H)
    d.line(lay, 0.0, H - r, 0.0, r)
    d.arc(lay, W - r, r, r, 270.0, 360.0)
    d.arc(lay, W - r, H - r, r, 0.0, 90.0)
    d.arc(lay, r, H - r, r, 90.0, 180.0)
    d.arc(lay, r, r, r, 180.0, 270.0)


def write_outline(path: pathlib.Path, r: float) -> None:
    d = Dxf()
    lay = d.layer("BOARD_OUTLINE", 7)
    outline(d, lay, r)
    path.write_text(d.text(), encoding="ascii", newline="")


def write_zones(path: pathlib.Path) -> None:
    d = Dxf()
    # The outline comes along as a reference so the zones land in the right
    # place when this file is imported on its own; it uses the drawing radius.
    outline(d, d.layer("BOARD_OUTLINE_REFERENCIA", 8), RADIUS_DRAWING)

    lay_c = d.layer("FURO_M2_CENTRO", 5)
    lay_h = d.layer(f"FURO_M2_D{M2_DRILL_UNVERIFIED:.1f}_CONFERIR".replace(".", "-"), 5)
    for sx, sy in FUROS_DOC:
        d.cross(lay_c, sx, y(sy), 1.5)
        d.circle(lay_h, sx, y(sy), M2_DRILL_UNVERIFIED / 2.0)

    for name, (x0, y0, x1, y1), colour, _src in ZONES + CONFLITOS:
        d.rect(d.layer(name, colour), x0, y(y1), x1, y(y0))

    path.write_text(d.text(), encoding="ascii", newline="")


def main() -> int:
    write_outline(HERE / "contorno-r3.dxf", RADIUS_DRAWING)
    write_outline(HERE / "contorno-r4.dxf", RADIUS_DOC14)
    write_zones(HERE / "zonas.dxf")
    print(f"placa {W:g} x {H:g} mm, {THICKNESS:g} mm")
    print(f"contorno-r3.dxf  raio {RADIUS_DRAWING:g} mm (case_drawing.py)")
    print(f"contorno-r4.dxf  raio {RADIUS_DOC14:g} mm (docs/14)")
    print(f"zonas.dxf        {len(ZONES)} zonas, {len(CONFLITOS)} conflitos, "
          f"{len(FUROS_DOC)} furo M2")
    return 0


if __name__ == "__main__":
    sys.exit(main())
