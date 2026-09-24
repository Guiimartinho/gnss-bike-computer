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
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent

# Board, from hardware_gnssbike/04-pcb-e-caixa.md#o-contorno
W = 55.0
H = 97.0
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
FUROS_DOC = [(3.2, 48.0)]

# The four screws the case drawing still has, at (6.5, 12.5), (55.5, 12.5),
# (6.5, 91.5) and (55.5, 91.5) in case coordinates, are 3.5 mm inside on every
# side: (3.0, 9.0), (52.0, 9.0), (3.0, 88.0) and (52.0, 88.0) on the board.
# They are kept here only as the geometry the case drawing still shows; the
# board carries one hole. The two are not reconciled in any document yet.
SCREWS_CASE_DRAWING = [(3.0, 9.0), (52.0, 9.0), (3.0, 88.0), (52.0, 88.0)]

# No document in this repository gives the drill diameter or the pad of the M2
# holes. 2.2 mm is the usual clearance hole for an M2 screw and is drawn here
# only so that something exists to snap to; the layer name says so.
M2_DRILL_UNVERIFIED = 2.2

# Placement zones and keep-outs, in document coordinates (x0, y0, x1, y1),
# y downward. Source of each one in the comment.
ZONES = [
    # name, rect, colour, source
    #
    # The floorplan of the 55 x 97 mm board. The blocks are the ones laid out
    # for the 50 x 86 attempt - they were right - spread over the area the
    # case actually has. The board is NOT sized by the electronics: the
    # display and its connector set the minimum at 49.58 x 84.83, and what is
    # left over is ground plane, which on a chip antenna is what radiates
    # (docs/13: the same chip gives 43,4 dB-Hz on an 80 x 40 mm plane and
    # 34,7 on a 24 x 15 one).
    ("KEEPOUT_ANTENA_GNSS", (0.0, 0.0, 55.0, 8.0), 1,
     "04#zonas-proibidas: sem cobre em nenhuma camada"),
    # The module sits in the bottom right CORNER with its antenna over a
    # notch, which is what figure 1 of section 7.5 calls "Best".
    ("KEEPOUT_ANTENA_MODULO", (50.3, 75.0, 55.0, 97.0), 1,
     "ficha ME54BS13 V1.0.0, 7.3 e 7.4: sobre a area da antena nao pode cobre, "
     "componente nem caixa metalica fechada, e 3 a 5 mm em volta dela nao "
     "pode trilha de sinal, metal nem fonte de interferencia"),
    ("RECORTE_ANTENA_MODULO", (50.7, 79.4, 55.0, 89.6), 2,
     "ficha ME54BS13 V1.0.0, 7.4: a placa sob a area da antena e VAZADA, para "
     "deixar a regiao suspensa. Comeca em x 50,7: a ultima coluna de pads LGA "
     "do modulo chega a 50,275, e o corte tem de deixar os 0,3 mm de cobre a "
     "borda"),
    ("SOMBRA_BATERIA_MAX_1-2MM", (11.0, 20.0, 47.0, 80.0), 30,
     "04#as-duas-sombras: teto de 1,2 mm na face de tras"),
    ("SOMBRA_DISPLAY_JDI_MAX_2-6MM", (9.00, 5.1, 49.08, 66.9), 30,
     "04#as-duas-sombras: contorno 40,08 x 61,8 do LPM027M128C, encostado no "
     "conector dele a esquerda"),
    ("DISPLAY_AREA_ATIVA", (11.40, 6.6, 46.68, 65.4), 8,
     "04#as-duas-sombras: 35,28 x 58,8, so referencia"),
    ("ZONA_GNSS_MAX-F10S", (19.5, 8.5, 35.5, 20.0), 3,
     "04#posicionamento: receptor logo abaixo da zona da antena"),
    ("ZONA_LED_RGB", (49.0, 8.5, 53.0, 12.0), 3,
     "04#posicionamento, abaixo da zona da antena GNSS"),
    ("ZONA_FLASH_MX25R6435F", (4.0, 22.0, 20.0, 34.0), 3,
     "flash NOR, na faixa sob o display: fala SPI com o modulo"),
    ("ZONA_IMU_MAGNETOMETRO", (26.0, 22.0, 42.0, 34.0), 3,
     "BMI270 e MMC5633NJL, na mesma faixa da flash: sob o display, longe das "
     "duas antenas e das correntes de chaveamento"),
    ("ZONA_FPC_DISPLAY_J401", (0.8, 30.0, 9.0, 41.0), 3,
     "04#posicionamento: 10 vias, sai pela esquerda"),
    ("ZONA_BUZZER", (32.0, 40.0, 44.0, 51.0), 3, "04#posicionamento"),
    ("ZONA_ENERGIA", (3.0, 52.0, 48.0, 74.0), 3,
     "nPM1300, MAX17262, AEM10900, TPS7A02, indutores e conectores, na faixa "
     "larga sob a metade de baixo do display"),
    ("ZONA_BAROMETRO_BMP585", (4.0, 76.0, 8.0, 80.0), 3,
     "04#posicionamento: face de tras, no respiro"),
    ("ZONA_BOTOES", (2.0, 76.0, 36.0, 85.0), 3,
     "3 teclas Omron B3S-1002P, abaixo do display e a esquerda da faixa de "
     "5 mm em volta da antena do modulo"),
    ("ZONA_MODULO_ME54BS13", (38.0, 77.75, 55.0, 91.25), 3,
     "MinewSemi ME54BS13, 16,5 x 12,0 mm, deitado no canto de baixo a direita "
     "com a antena sobre o recorte"),
    ("ZONA_USB_C", (12.0, 88.0, 24.0, 97.0), 3,
     "04#posicionamento: Molex 2036150003, na borda de baixo"),
    ("ZONA_LUZ_AMBIENTE_OPT3001", (1.2, 10.5, 3.8, 13.5), 3,
     "04#posicionamento"),
]



# Overlaps that the documents already flag as unresolved, drawn so that they
# are visible in the CAD tool instead of having to be recomputed by eye.
CONFLITOS = [
    ("CONFLITO_GNSS_NA_ZONA_DA_ANTENA", (20.0, 2.0, 35.0, 8.0), 2,
     "04#orcamento-de-area: 90 mm2"),
    ("CONFLITO_LED_NA_ZONA_DA_ANTENA", (50.0, 0.0, 53.0, 3.0), 2,
     "04#orcamento-de-area: 9 mm2"),
    ("CONFLITO_BOTAO_NA_ZONA_DO_MODULO", (45.5, 75.3, 49.0, 76.0), 2,
     "a tecla da direita encosta na area livre da antena do modulo"),
    ("CONFLITO_MODULO_NA_SOMBRA_DA_BATERIA", (37.2, 58.7, 45.5, 71.4), 2,
     "o modulo tem 2,4 mm de altura e a sombra da bateria so aceita 1,2 mm: "
     "ele vai na face da frente"),
]


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
