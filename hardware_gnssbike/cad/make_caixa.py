#!/usr/bin/env python3
"""The case, drawn around today's board: 2D (a PDF), 3D (PNG views) and STL.

Until 2026-09-26 the only case was the concept of docs/13 (tools/docs/
case_drawing.py), drawn around the OLD 55 mm board. This file is a case
PROPOSAL around the real one, gnssbike.kicad_pcb, 34 x 90 mm, read with
every footprint's courtyard, height and face, plus the GLB the 3D renderer
uses. What the owner asked for on 2026-09-26, in his words: the inside of
the case, room to glue the cell, keys that match the push buttons, the cuts
to fit or glue the solar modules, and something transparent over the
modules for the rain.

What is decided HERE, and is a proposal until the owner says otherwise
(page 3 of the PDF repeats it):
  - outer size 62 x 104 (the concept's) by 17 mm thick (the concept's 19
    had air over the display; 16 was tried and the USB-C and the SWD header
    hit the lid); corner radius 7; walls 2, floor and lid 1,5; a 45-degree
    bevel of 6,2 mm along the long edges, on the lid, carrying two solar
    modules a side, like the concept;
  - the board centred in the width and 0,5 mm off the bottom wall, so that
    the USB-C reaches the wall's opening; the cell (36 x 60 x 7) glued on
    the floor inside four ribs, 0,5 mm under the parts of the board's back;
    the board on one M2 screw boss (the other hole of the board is over the
    cell and cannot take a boss) and three posts; the display glued under
    the lid round the window with 0,2 mm tape, its glass 1,5 mm below the
    lid's face (a raised bezel is the alternative, not drawn);
  - three square key caps through the lid, spread across the display's
    width at the concept's 14 mm pitch (the owner's request), each with a
    bar under the lid reaching its switch on the board - the switches
    themselves stop at x 21,5 of the board because of the radio module's
    antenna keep-out -, under a 0,3 mm TPU membrane glued in a 0,4 mm
    recess (rain);
  - a raised facet below the keys with two module pockets and one clear
    cover, two pockets and one cover on each bevel; wire holes to the
    inside; a light-pipe hole for the LED and a window for the light sensor
    at PROPOSED board positions (both parts are under the display today);
  - four M2 screws at the corners hold the lid; a notch in the bottom wall
    and lid for the USB-C; sound holes under the buzzer and a vent with a
    membrane recess under the barometer, in the floor.

Run:  python hardware_gnssbike/cad/make_caixa.py
Out:  gnssbike-caixa.pdf (3 pages), gnssbike-3d-caixa-aberta.png,
      gnssbike-3d-caixa-frente.png, gnssbike-3d-caixa-explodida.png and the
      STL files caixa-concha.stl, caixa-tampa.stl, caixa-tecla-1/2/3.stl,
      caixa-membrana-teclas.stl, caixa-cobertura-faceta.stl,
      caixa-cobertura-chanfro.stl (triangle soups of overlapping boxes, for
      a slicer, not a CAD solid).
"""

from __future__ import annotations

import math
import pathlib
import struct
import sys

import fitz
import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import dry_run_pcb as DR      # noqa: E402
import footprints as FPS      # noqa: E402
import make_3d as M3          # noqa: E402
import make_dxf as MD         # noqa: E402
import make_pcb as MP         # noqa: E402

# ---------------------------------------------------------------- the case
W_C, H_C, R_C = 62.0, 104.0, 7.0                  # the concept's plan
# 17, not the concept's 19: the board's top face sits 2,6 mm (the display
# shadow's ceiling) under the display, which is glued under the lid, so
# what is left under the lid for the parts outside the display is the
# same 2,6 plus the tape plus the glass: 3,6 mm. The USB-C is 3,26, the
# SWD header without its shroud 2,5. At 16 both hit the lid.
T_C = 17.0
PAREDE, FUNDO, TAMPA = 2.0, 1.5, 1.5
CHANFRO = 6.2                                     # 45-degree bevel, long edges
CHANFRO_Y = (16.0, 76.0)                          # where the bevel runs
FOLGA_PLACA = 0.5
# ------------------------------------------------------- the parts held
DISPLAY_W, DISPLAY_H, DISPLAY_ESP, DISPLAY_VAO = 40.08, 61.8, 1.0, 2.6
JANELA_W, JANELA_H = 36.28, 59.8                  # viewing area
ARO_JANELA, FITA_DISPLAY = 0.3, 0.2               # lid rim over the glass, tape
CELULA_W, CELULA_H, CELULA_ESP, CELULA_VAO = 36.0, 60.0, 7.0, 1.2
PLACA_W, PLACA_H, PLACA_ESP = MD.W, MD.H, 0.8
SOMBRA_DISPLAY = (64.5 - 61.8, 64.5)              # board y, make_dxf.ZONES
SOMBRA_CELULA = (9.9, 69.9)
MODULO_W, MODULO_H, MODULO_ESP = 23.0, 8.0, 1.8   # KXOB25-05X3F class
COBERTURA_ESP = 0.6                               # clear cover thickness
TECLA_ALT, TECLA_CURSO = 2.0, 0.2                 # TS-1088R over the board
# ------------------------------------------------ where everything sits
PLACA_X0 = (W_C - PLACA_W) / 2.0
PLACA_Y0 = H_C - PAREDE - FOLGA_PLACA - PLACA_H
TAMPA_Z0 = T_C - TAMPA
DISPLAY_Z1 = TAMPA_Z0 - FITA_DISPLAY              # glass glued under the lid, round the window
DISPLAY_Z0 = DISPLAY_Z1 - DISPLAY_ESP
PLACA_Z1 = DISPLAY_Z0 - DISPLAY_VAO               # board top face
PLACA_Z0 = PLACA_Z1 - PLACA_ESP
CELULA_Z1 = PLACA_Z0 - CELULA_VAO - 0.5
CELULA_Z0 = CELULA_Z1 - CELULA_ESP
# keys: square cap (stem) in a square hole, a bar under the lid to the switch,
# the caps at the concept's pitch across the display, a membrane recess
TECLA_FURO = 5.6
TECLA_CAPA = 5.0
TECLA_PASSO = 14.0
BARRA_LARG = 5.0
BARRA_ESP = 0.5
MEMBRANA_ESP = 0.3
MEMBRANA_REBAIXO = 0.4
# facet below the keys: a plateau on the lid with the module pockets
FACETA_ALT = 1.5
COBERTURA_FOLGA = 1.0                             # cover pocket around the modules
MODULO_FOLGA = 0.2
# fasteners
PARAFUSO_TAMPA = ((5.5, 5.5), (W_C - 5.5, 5.5), (5.5, H_C - 5.5), (W_C - 5.5, H_C - 5.5))
BOSSA_D, BOSSA_FURO, BOSSA_PLACA_D, PILAR_D = 5.0, 1.6, 4.5, 3.0
# the LED and the light sensor: PROPOSED board positions (page 3 says why)
LED_PROPOSTA = (3.5, 70.5)
SENSOR_LUZ_PROPOSTA = (17.5, 70.0)
LED_FURO, SENSOR_FURO = 2.5, 3.5

COR_CAIXA = (0.22, 0.23, 0.26)
COR_TAMPA = (0.27, 0.28, 0.31)
COR_CELULA = (0.30, 0.31, 0.34)
COR_DISPLAY = (0.13, 0.14, 0.17)
COR_JANELA = (0.55, 0.58, 0.55)
COR_MODULO = (0.06, 0.08, 0.14)
COR_COBERTURA = (0.70, 0.78, 0.82)
COR_TECLA = (0.45, 0.46, 0.48)
COR_MEMBRANA = (0.60, 0.62, 0.64)


# ------------------------------------------------------------ geometry
def contorno_arredondado(x0, y0, x1, y1, r, n=6):
    """A rounded rectangle as a closed polyline (y down), n points per corner."""
    pts = []
    cantos = ((x1 - r, y0 + r, -90.0), (x1 - r, y1 - r, 0.0),
              (x0 + r, y1 - r, 90.0), (x0 + r, y0 + r, 180.0))
    for cx, cy, a0 in cantos:
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def poligono_regular(cx, cy, r, n=12):
    return [(cx + r * math.cos(2 * math.pi * k / n), cy + r * math.sin(2 * math.pi * k / n))
            for k in range(n)]


def triangular(pts):
    """Ear clipping of a simple polygon (any winding), as index triples."""
    n = len(pts)
    if n < 3:
        return []
    area = sum(pts[i][0] * pts[(i + 1) % n][1] - pts[(i + 1) % n][0] * pts[i][1]
               for i in range(n))
    idx = list(range(n)) if area > 0 else list(range(n))[::-1]

    def convexo(a, b, c):
        return ((pts[b][0] - pts[a][0]) * (pts[c][1] - pts[a][1])
                - (pts[b][1] - pts[a][1]) * (pts[c][0] - pts[a][0])) > 1e-12

    def dentro(p, a, b, c):
        def s(u, v, w):
            return (v[0] - u[0]) * (w[1] - u[1]) - (v[1] - u[1]) * (w[0] - u[0])
        d1, d2, d3 = s(pts[a], pts[b], p), s(pts[b], pts[c], p), s(pts[c], pts[a], p)
        return not ((d1 < 0 or d2 < 0 or d3 < 0) and (d1 > 0 or d2 > 0 or d3 > 0))

    out = []
    guarda = 0
    while len(idx) > 3 and guarda < 10 * n:
        guarda += 1
        for k in range(len(idx)):
            a, b, c = idx[k - 1], idx[k], idx[(k + 1) % len(idx)]
            if not convexo(a, b, c):
                continue
            if any(dentro(pts[j], a, b, c) for j in idx if j not in (a, b, c)):
                continue
            out.append((a, b, c))
            del idx[k]
            break
    if len(idx) == 3:
        out.append(tuple(idx))
    return out


class Malha:
    """A pile of triangles with a colour each, in mm, y down, z up."""

    def __init__(self):
        self.tris: list = []
        self.cols: list = []

    def tri(self, a, b, c, cor):
        self.tris.append(np.array([a, b, c], dtype=np.float64))
        self.cols.append(np.array(cor))

    def quad(self, a, b, c, d, cor):
        self.tri(a, b, c, cor)
        self.tri(a, c, d, cor)

    def caixa(self, x0, y0, z0, x1, y1, z1, cor):
        b = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]
        t = [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        self.quad(b[0], b[3], b[2], b[1], cor)
        self.quad(t[0], t[1], t[2], t[3], cor)
        for k in range(4):
            a, c = k, (k + 1) % 4
            self.quad(b[a], b[c], t[c], t[a], cor)

    def extrusao(self, pts, z0, z1, cor):
        """Any simple polygon (plan, y down) extruded between two heights."""
        for a, b, c in triangular(pts):
            self.tri((*pts[a], z0), (*pts[c], z0), (*pts[b], z0), cor)
            self.tri((*pts[a], z1), (*pts[b], z1), (*pts[c], z1), cor)
        n = len(pts)
        for k in range(n):
            a, b = pts[k], pts[(k + 1) % n]
            self.quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1), cor)

    def anel(self, fora, dentro, z0, z1, cor, so_segmentos=None):
        """The wall between two closed polylines of equal length; a subset
        of segment indices when the wall is not the whole ring."""
        n = len(fora)
        segs = range(n) if so_segmentos is None else so_segmentos
        for k in segs:
            a, b = fora[k], fora[(k + 1) % n]
            c, d = dentro[k], dentro[(k + 1) % n]
            self.quad((a[0], a[1], z0), (b[0], b[1], z0), (d[0], d[1], z0), (c[0], c[1], z0), cor)
            self.quad((a[0], a[1], z1), (c[0], c[1], z1), (d[0], d[1], z1), (b[0], b[1], z1), cor)
            self.quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1), (a[0], a[1], z1), cor)
            self.quad((c[0], c[1], z0), (c[0], c[1], z1), (d[0], d[1], z1), (d[0], d[1], z0), cor)

    def placa_com_furos(self, x0, y0, x1, y1, z0, z1, furos, cor):
        """A plate minus rectangular holes: the grid of cells between the
        holes' edges, every cell wholly inside a hole left out."""
        xs = sorted({x0, x1} | {f[0] for f in furos if x0 < f[0] < x1} | {f[2] for f in furos if x0 < f[2] < x1})
        ys = sorted({y0, y1} | {f[1] for f in furos if y0 < f[1] < y1} | {f[3] for f in furos if y0 < f[3] < y1})
        for i in range(len(xs) - 1):
            for j in range(len(ys) - 1):
                cx, cy = (xs[i] + xs[i + 1]) / 2.0, (ys[j] + ys[j + 1]) / 2.0
                if any(f[0] <= cx <= f[2] and f[1] <= cy <= f[3] for f in furos):
                    continue
                self.caixa(xs[i], ys[j], z0, xs[i + 1], ys[j + 1], z1, cor)

    def prisma_yz(self, poligono_xz, y0, y1, cor):
        """A polygon in the (x, z) plane extruded along y."""
        pts = [(x, z) for x, z in poligono_xz]
        for a, b, c in triangular(pts):
            self.tri((pts[a][0], y0, pts[a][1]), (pts[b][0], y0, pts[b][1]), (pts[c][0], y0, pts[c][1]), cor)
            self.tri((pts[a][0], y1, pts[a][1]), (pts[c][0], y1, pts[c][1]), (pts[b][0], y1, pts[b][1]), cor)
        n = len(pts)
        for k in range(n):
            a, b = pts[k], pts[(k + 1) % n]
            self.quad((a[0], y0, a[1]), (b[0], y0, b[1]), (b[0], y1, b[1]), (a[0], y1, a[1]), cor)

    def cilindro(self, cx, cy, r, z0, z1, cor, r_furo=0.0, n=12):
        if r_furo > 0:
            self.anel(poligono_regular(cx, cy, r, n), poligono_regular(cx, cy, r_furo, n), z0, z1, cor)
        else:
            self.extrusao(poligono_regular(cx, cy, r, n), z0, z1, cor)

    def arrays(self):
        if not self.tris:
            return np.zeros((0, 3, 3)), np.zeros((0, 3))
        return np.array(self.tris), np.array(self.cols)

    def stl(self, caminho: pathlib.Path) -> int:
        """Binary STL, in mm, as the slicer wants it (y down stays y down:
        the slicer does not care which way the plan is read)."""
        with open(caminho, "wb") as f:
            f.write(b"gnssbike case, generated by make_caixa.py".ljust(80, b"\0"))
            f.write(struct.pack("<I", len(self.tris)))
            for t in self.tris:
                n = np.cross(t[1] - t[0], t[2] - t[0])
                ln = np.linalg.norm(n)
                n = n / ln if ln > 0 else n
                f.write(struct.pack("<3f", *n))
                for v in t:
                    f.write(struct.pack("<3f", *v))
                f.write(struct.pack("<H", 0))
        return len(self.tris)


def quadrado(cx, cy, lado):
    return (cx - lado / 2, cy - lado / 2, cx + lado / 2, cy + lado / 2)


# ------------------------------------------------------------ the board
def ler_placa():
    pecas, _pads, _seg, _vias, _cortes = DR.ler(HERE / "gnssbike.kicad_pcb")
    for ref, p in pecas.items():
        nome_fp = FPS.FP.get(ref, ("", 0, 0))[0]
        alt = DR.altura_do_footprint(nome_fp) if nome_fp else None
        p["altura"] = alt[0] if alt else 1.0
    return pecas


def caixa_na_caixa(p):
    x0, y0, x1, y1 = p["caixa"]
    return (PLACA_X0 + x0, PLACA_Y0 + y0, PLACA_X0 + x1, PLACA_Y0 + y1)


def teclas(pecas):
    return [p for ref, p in sorted(pecas.items()) if ref.startswith("SW")]


# ----------------------------------------------------------- features
class Caixa:
    """Every feature of the proposal, in the case's frame, computed once from
    the board so that the 2D, the 3D and the STL cannot disagree."""

    def __init__(self, pecas):
        self.pecas = pecas
        self.fora = contorno_arredondado(0.0, 0.0, W_C, H_C, R_C)
        self.dentro = contorno_arredondado(PAREDE, PAREDE, W_C - PAREDE, H_C - PAREDE, R_C - PAREDE)
        # the display and its window, from the board's shadow
        self.display = (PLACA_X0 + PLACA_W / 2 - DISPLAY_W / 2, PLACA_Y0 + SOMBRA_DISPLAY[0],
                        PLACA_X0 + PLACA_W / 2 + DISPLAY_W / 2, PLACA_Y0 + SOMBRA_DISPLAY[1])
        dx0, dy0, dx1, dy1 = self.display
        self.janela = (dx0 + (DISPLAY_W - JANELA_W) / 2, dy0 + (DISPLAY_H - JANELA_H) / 2,
                       dx1 - (DISPLAY_W - JANELA_W) / 2, dy1 - (DISPLAY_H - JANELA_H) / 2)
        self.bolso_display = (dx0 - 0.3, dy0 - 0.3, dx1 + 0.3, dy1 + 0.3)
        self.celula = (PLACA_X0 + PLACA_W / 2 - CELULA_W / 2, PLACA_Y0 + SOMBRA_CELULA[0],
                       PLACA_X0 + PLACA_W / 2 + CELULA_W / 2, PLACA_Y0 + SOMBRA_CELULA[1])
        # Keys. The switches sit where the board could put them (x 4,5 to
        # 21,5 of the board: the module's antenna keep-out stops the third
        # one at 21,5); the CAPS are spread across the display's width, at
        # the concept's 14 mm pitch, centred on the window - the owner's
        # request of 2026-09-26. Each cap has a bar under the lid reaching
        # its switch; the bar is what presses the switch and what keeps the
        # cap in (it is wider than the hole), and the stem is square so the
        # bar cannot turn.
        self.teclas = [(PLACA_X0 + p["x"], PLACA_Y0 + p["y"]) for p in teclas(pecas)]
        cx_janela = (self.janela[0] + self.janela[2]) / 2
        y_teclas = self.teclas[0][1] if self.teclas else PLACA_Y0 + 66.0
        self.capas = [(cx_janela + (i - 1) * TECLA_PASSO, y_teclas) for i in range(len(self.teclas))]
        self.furos_teclas = [quadrado(x, y, TECLA_FURO) for x, y in self.capas]
        if self.capas:
            xs = [x for x, _y in self.capas]
            ys = [y for _x, y in self.capas]
            self.rebaixo_membrana = (min(xs) - 6.0, min(ys) - 5.0, max(xs) + 6.0, max(ys) + 5.0)
        else:
            self.rebaixo_membrana = None
        self.topo_tecla = PLACA_Z1 + TECLA_ALT            # the switch's plunger top
        # the facet: a plateau below the keys, two module pockets, one cover
        y_ini = (max(ys) + TECLA_FURO / 2 + 6.0) if self.teclas else PLACA_Y0 + 72.0
        self.faceta = (4.0, y_ini, W_C - 4.0, H_C - 4.0)
        fy = (self.faceta[1] + self.faceta[3]) / 2
        larg = 2 * (MODULO_W + 2 * MODULO_FOLGA) + 0.6
        fx0 = W_C / 2 - larg / 2
        self.bolsos_faceta = [
            (fx0, fy - MODULO_H / 2 - MODULO_FOLGA, fx0 + MODULO_W + 2 * MODULO_FOLGA,
             fy + MODULO_H / 2 + MODULO_FOLGA),
            (fx0 + MODULO_W + 2 * MODULO_FOLGA + 0.6, fy - MODULO_H / 2 - MODULO_FOLGA,
             fx0 + larg, fy + MODULO_H / 2 + MODULO_FOLGA)]
        self.cobertura_faceta = (fx0 - COBERTURA_FOLGA, fy - MODULO_H / 2 - MODULO_FOLGA - COBERTURA_FOLGA,
                                 fx0 + larg + COBERTURA_FOLGA, fy + MODULO_H / 2 + MODULO_FOLGA + COBERTURA_FOLGA)
        self.furos_fio = []
        for b in self.bolsos_faceta:
            for x in (b[0] + 1.5, b[2] - 1.5):
                self.furos_fio.append(quadrado(x, fy, 1.0))
        # the bevels: two modules a side, along y
        self.modulos_chanfro = []
        y0, y1 = CHANFRO_Y
        vao = (y1 - y0 - 2 * MODULO_W - 2 * MODULO_FOLGA * 2) / 3.0
        for lado in ("L", "R"):
            for k in range(2):
                ya = y0 + vao + k * (MODULO_W + 2 * MODULO_FOLGA + vao)
                self.modulos_chanfro.append((lado, ya, ya + MODULO_W + 2 * MODULO_FOLGA))
        # windows for the LED and the light sensor, at PROPOSED positions
        self.furo_led = quadrado(PLACA_X0 + LED_PROPOSTA[0], PLACA_Y0 + LED_PROPOSTA[1], LED_FURO)
        self.furo_sensor = quadrado(PLACA_X0 + SENSOR_LUZ_PROPOSTA[0], PLACA_Y0 + SENSOR_LUZ_PROPOSTA[1], SENSOR_FURO)
        # USB-C notch in the bottom wall (and the lid's rim), centred on J101
        j101 = pecas.get("J101")
        jx = PLACA_X0 + (j101["x"] if j101 else PLACA_W / 2)
        self.usb = (jx - 5.0, jx + 5.0, PLACA_Z1 + 0.2, PLACA_Z1 + 3.26 + 0.6)   # x0, x1, z0, z1
        # holes in the floor: sound under the buzzer, vent under the barometer
        self.furos_fundo = []
        ls = pecas.get("LS601")
        if ls and ls["atras"]:
            bx, by = PLACA_X0 + ls["x"], PLACA_Y0 + ls["y"]
            for dx in (-2.5, 0.0, 2.5):
                self.furos_fundo.append(quadrado(bx + dx, by, 1.2))
        baro = pecas.get("U502")
        self.respiro = None
        if baro and baro["atras"]:
            bx, by = PLACA_X0 + baro["x"], PLACA_Y0 + baro["y"]
            self.respiro = (bx, by)
            self.furos_fundo.append(quadrado(bx, by, 1.5))
        # what holds the board: the M2 holes of the board that are NOT over
        # the cell get a screw boss; the rest of the board rests on posts
        self.bossas_placa, self.furos_sobre_celula = [], []
        cx0, cy0, cx1, cy1 = self.celula
        for fx, fy in MD.FUROS_DOC:
            x, y = PLACA_X0 + fx, PLACA_Y0 + fy
            if cx0 - 1.0 < x < cx1 + 1.0 and cy0 - 1.0 < y < cy1 + 1.0:
                self.furos_sobre_celula.append((x, y))
            else:
                self.bossas_placa.append((x, y))
        self.pilares = [(PLACA_X0 + 1.5, PLACA_Y0 + 2.5), (PLACA_X0 + PLACA_W - 1.5, PLACA_Y0 + 2.5),
                        (PLACA_X0 + PLACA_W - 1.5, PLACA_Y0 + PLACA_H - 3.5)]
        # cell ribs: four, 2 mm thick, 5 mm tall, 0,25 mm off the cell
        f = 0.25
        self.nervuras = [
            (cx0 - f - 2.0, cy0 - f, cx0 - f, cy1 + f), (cx1 + f, cy0 - f, cx1 + f + 2.0, cy1 + f),
            (cx0 - f, cy0 - f - 2.0, cx1 + f, cy0 - f), (cx0 - f, cy1 + f, cx1 + f, cy1 + f + 2.0)]

    # ---- the segments of the outline that are the straight long sides
    def _segmentos_longos(self):
        n = len(self.fora)
        out = []
        for k in range(n):
            a, b = self.fora[k], self.fora[(k + 1) % n]
            if (abs(a[0]) < 1e-6 and abs(b[0]) < 1e-6) or (abs(a[0] - W_C) < 1e-6 and abs(b[0] - W_C) < 1e-6):
                out.append(k)
        return out

    # ---- the shell
    def concha(self) -> Malha:
        m = Malha()
        m.placa_com_furos(0.0, 0.0, W_C, H_C, 0.0, FUNDO, self.furos_fundo + self._cantos_fora(), COR_CAIXA)
        # the rounded corners of the floor, which the plate above left square
        for k, (x0, y0, x1, y1) in enumerate(self._cantos_fora()):
            pts = [p for p in self.fora if x0 - 1e-6 <= p[0] <= x1 + 1e-6 and y0 - 1e-6 <= p[1] <= y1 + 1e-6]
            centro = ((W_C - R_C) if x0 > W_C / 2 else R_C, (H_C - R_C) if y0 > H_C / 2 else R_C)
            m.extrusao([centro] + pts, 0.0, FUNDO, COR_CAIXA)
        longos = set(self._segmentos_longos())
        n = len(self.fora)
        z_baixo = T_C - CHANFRO
        for k in range(n):
            a, b = self.fora[k], self.fora[(k + 1) % n]
            ya, yb = sorted((a[1], b[1]))
            if k in longos and ya >= CHANFRO_Y[0] - 1e-6 and yb <= CHANFRO_Y[1] + 1e-6:
                m.anel(self.fora, self.dentro, FUNDO, z_baixo, COR_CAIXA, [k])
            elif k in longos and (yb > CHANFRO_Y[0] and ya < CHANFRO_Y[1]):
                # a long segment straddling the bevel's end: split at the end
                fim = CHANFRO_Y[0] if ya < CHANFRO_Y[0] else CHANFRO_Y[1]
                for (y0, y1, z1) in ((ya, fim, TAMPA_Z0 if ya < CHANFRO_Y[0] else z_baixo),
                                     (fim, yb, z_baixo if ya < CHANFRO_Y[0] else TAMPA_Z0)):
                    x = a[0]
                    xi = PAREDE if x < W_C / 2 else W_C - PAREDE
                    m.caixa(min(x, xi), y0, FUNDO, max(x, xi), y1, z1, COR_CAIXA)
            else:
                # the bottom wall carries the USB notch
                if abs(a[1] - H_C) < 1e-6 and abs(b[1] - H_C) < 1e-6:
                    ux0, ux1, uz0, _uz1 = self.usb
                    xa, xb = sorted((a[0], b[0]))
                    for (x0, x1, z0, z1) in ((xa, ux0, FUNDO, TAMPA_Z0), (ux1, xb, FUNDO, TAMPA_Z0),
                                             (ux0, ux1, FUNDO, uz0)):
                        if x1 > x0 + 1e-6:
                            m.caixa(x0, H_C - PAREDE, z0, x1, H_C, z1, COR_CAIXA)
                else:
                    m.anel(self.fora, self.dentro, FUNDO, TAMPA_Z0, COR_CAIXA, [k])
        for x, y in PARAFUSO_TAMPA:
            m.cilindro(x, y, BOSSA_D / 2, FUNDO, TAMPA_Z0, COR_CAIXA, BOSSA_FURO / 2)
        for x, y in self.bossas_placa:
            m.cilindro(x, y, BOSSA_PLACA_D / 2, FUNDO, PLACA_Z0, COR_CAIXA, BOSSA_FURO / 2)
        for x, y in self.pilares:
            m.cilindro(x, y, PILAR_D / 2, FUNDO, PLACA_Z0, COR_CAIXA)
        for x0, y0, x1, y1 in self.nervuras:
            m.caixa(x0, y0, FUNDO, x1, y1, FUNDO + 5.0, COR_CAIXA)
        return m

    def _cantos_fora(self):
        return [(0.0, 0.0, R_C, R_C), (W_C - R_C, 0.0, W_C, R_C), (0.0, H_C - R_C, R_C, H_C),
                (W_C - R_C, H_C - R_C, W_C, H_C)]

    # ---- the lid
    def tampa(self, z_base: float) -> Malha:
        """The lid with its underside at z_base (TAMPA_Z0 in place)."""
        m = Malha()
        dz = z_base - TAMPA_Z0
        z0, z1 = z_base, z_base + TAMPA
        furos = [self.janela] + self.furos_teclas + [quadrado(x, y, 2.2) for x, y in PARAFUSO_TAMPA]
        furos += [self.furo_led, self.furo_sensor] + self.furos_fio
        # the rim on the short sides and corners (the long sides are the bevels)
        longos = set(self._segmentos_longos())
        n = len(self.fora)
        for k in range(n):
            a, b = self.fora[k], self.fora[(k + 1) % n]
            ya, yb = sorted((a[1], b[1]))
            if k in longos and yb > CHANFRO_Y[0] and ya < CHANFRO_Y[1]:
                continue
            if abs(a[1] - H_C) < 1e-6 and abs(b[1] - H_C) < 1e-6:
                ux0, ux1, _uz0, uz1 = self.usb
                xa, xb = sorted((a[0], b[0]))
                for x0, x1, zz0 in ((xa, ux0, z0), (ux1, xb, z0), (ux0, ux1, uz1 + dz)):
                    if x1 > x0 + 1e-6 and z1 > zz0:
                        m.caixa(x0, H_C - PAREDE, zz0, x1, H_C, z1, COR_TAMPA)
                continue
            m.anel(self.fora, self.dentro, z0, z1, COR_TAMPA, [k])
        # the plate: full thickness everywhere but the membrane recess and the
        # facet (built below); the display is glued UNDER it, round the window
        px0, py0, px1, py1 = PAREDE, PAREDE, W_C - PAREDE, H_C - PAREDE
        rx = self.rebaixo_membrana
        fx0, fy0, fx1, fy1 = self.faceta
        especiais = [(fx0, fy0, fx1, fy1)] + ([rx] if rx else [])
        m.placa_com_furos(px0, py0, px1, py1, z0, z1, furos + especiais, COR_TAMPA)
        if rx:
            m.placa_com_furos(rx[0], rx[1], rx[2], rx[3], z0, z1 - MEMBRANA_REBAIXO, self.furos_teclas, COR_TAMPA)
        # the facet plateau: base with the wire holes, pockets, the cover step
        m.placa_com_furos(fx0, fy0, fx1, fy1, z0, z0 + 0.5, self.furos_fio, COR_TAMPA)
        topo = z1 + FACETA_ALT
        m.placa_com_furos(fx0, fy0, fx1, fy1, z0 + 0.5, topo - COBERTURA_ESP, self.bolsos_faceta, COR_TAMPA)
        m.placa_com_furos(fx0, fy0, fx1, fy1, topo - COBERTURA_ESP, topo, [self.cobertura_faceta], COR_TAMPA)
        # the bevels, with the module pockets and cover steps cut in
        for lado in ("L", "R"):
            self._chanfro(m, lado, dz)
        return m

    def _perfil_chanfro(self, lado: str, dz: float, entalhe: float | None):
        """Cross-section of the bevel solid in (x, z), for one y span:
        `entalhe` is the pocket depth cut into the sloped face (None: none)."""
        t = TAMPA
        z_pe = T_C - CHANFRO + dz
        z_top = T_C + dz
        A = (0.0, z_pe)
        B = (CHANFRO, z_top)
        off = t * math.sqrt(2.0)
        E = (off, z_pe)
        D = (CHANFRO + off, z_top - t)
        C = (CHANFRO + off, z_top)
        if entalhe is None:
            pol = [A, B, C, D, E]
        else:
            # the slope from A to B with a two-step notch: the cover step
            # (COBERTURA_ESP deep, wider) and the module pocket (deeper)
            s = (1 / math.sqrt(2), 1 / math.sqrt(2))       # along the slope
            nrm = (-1 / math.sqrt(2), 1 / math.sqrt(2))    # outward normal
            comp = CHANFRO * math.sqrt(2)                   # slope length
            meio = (A[0] + s[0] * comp / 2, A[1] + s[1] * comp / 2)
            h_cob = (MODULO_H + 2 * MODULO_FOLGA) / 2 + COBERTURA_FOLGA
            h_mod = (MODULO_H + 2 * MODULO_FOLGA) / 2

            def P(a, d):
                return (meio[0] + s[0] * a - nrm[0] * d, meio[1] + s[1] * a - nrm[1] * d)
            pol = [A, P(-h_cob, 0), P(-h_cob, COBERTURA_ESP), P(-h_mod, COBERTURA_ESP),
                   P(-h_mod, COBERTURA_ESP + entalhe), P(h_mod, COBERTURA_ESP + entalhe),
                   P(h_mod, COBERTURA_ESP), P(h_cob, COBERTURA_ESP), P(h_cob, 0), B, C, D, E]
        if lado == "R":
            pol = [(W_C - x, z) for x, z in pol]
        return pol

    def _chanfro(self, m: Malha, lado: str, dz: float):
        y0, y1 = CHANFRO_Y
        cortes = sorted({y0, y1} | {ya for l_, ya, yb in self.modulos_chanfro if l_ == lado}
                        | {yb for l_, ya, yb in self.modulos_chanfro if l_ == lado}
                        | {ya - COBERTURA_FOLGA for l_, ya, yb in self.modulos_chanfro if l_ == lado}
                        | {yb + COBERTURA_FOLGA for l_, ya, yb in self.modulos_chanfro if l_ == lado})
        for i in range(len(cortes) - 1):
            ya, yb = cortes[i], cortes[i + 1]
            ym = (ya + yb) / 2
            modulo = any(a <= ym <= b for l_, a, b in self.modulos_chanfro if l_ == lado)
            cobertura = any(a - COBERTURA_FOLGA <= ym <= b + COBERTURA_FOLGA
                            for l_, a, b in self.modulos_chanfro if l_ == lado)
            if modulo:
                pol = self._perfil_chanfro(lado, dz, MODULO_ESP + 0.1)
            elif cobertura:
                pol = self._perfil_chanfro(lado, dz, 0.0)
            else:
                pol = self._perfil_chanfro(lado, dz, None)
            m.prisma_yz(pol, ya, yb, COR_TAMPA)

    # ---- the small parts: keys, membrane, covers, modules
    def tecla_3d(self, i: int, dz: float = 0.0, origem=None) -> Malha:
        """One key: the bar over its switch reaching the cap, the square
        stem up through the lid. `origem` draws it alone at (0, 0, 0)."""
        m = Malha()
        (sx, sy), (cx, cy) = self.teclas[i], self.capas[i]
        z_pe = self.topo_tecla + dz
        z_topo = TAMPA_Z0 + TAMPA - MEMBRANA_REBAIXO + dz     # flush with the recess floor
        if origem is not None:
            ox, oy, oz = cx - origem[0], cy - origem[1], z_pe - origem[2]
            sx, sy, cx, cy, z_pe, z_topo = sx - ox, sy - oy, cx - ox, cy - oy, z_pe - oz, z_topo - oz
        x0, x1 = min(sx, cx) - BARRA_LARG / 2, max(sx, cx) + BARRA_LARG / 2
        m.caixa(x0, cy - BARRA_LARG / 2, z_pe, x1, cy + BARRA_LARG / 2, z_pe + BARRA_ESP, COR_TECLA)
        m.caixa(cx - TECLA_CAPA / 2, cy - TECLA_CAPA / 2, z_pe, cx + TECLA_CAPA / 2, cy + TECLA_CAPA / 2, z_topo, COR_TECLA)
        return m

    def teclas_3d(self, dz: float = 0.0) -> Malha:
        m = Malha()
        for i in range(len(self.teclas)):
            t = self.tecla_3d(i, dz)
            m.tris += t.tris
            m.cols += t.cols
        return m

    def membrana_3d(self, dz: float = 0.0) -> Malha:
        m = Malha()
        if self.rebaixo_membrana:
            x0, y0, x1, y1 = self.rebaixo_membrana
            z0 = TAMPA_Z0 + TAMPA - MEMBRANA_REBAIXO + dz
            m.caixa(x0, y0, z0, x1, y1, z0 + MEMBRANA_ESP, COR_MEMBRANA)
        return m

    def coberturas_e_modulos_3d(self, dz: float = 0.0) -> tuple[Malha, Malha]:
        cob, mod = Malha(), Malha()
        topo = TAMPA_Z0 + TAMPA + FACETA_ALT + dz
        x0, y0, x1, y1 = self.cobertura_faceta
        cob.caixa(x0, y0, topo - COBERTURA_ESP, x1, y1, topo, COR_COBERTURA)
        for bx0, by0, bx1, by1 in self.bolsos_faceta:
            mod.caixa(bx0 + MODULO_FOLGA, by0 + MODULO_FOLGA, topo - COBERTURA_ESP - MODULO_ESP - 0.1,
                      bx1 - MODULO_FOLGA, by1 - MODULO_FOLGA, topo - COBERTURA_ESP - 0.1, COR_MODULO)
        # on the bevels: boxes along the slope
        s = (1 / math.sqrt(2), 1 / math.sqrt(2))
        nrm = (-1 / math.sqrt(2), 1 / math.sqrt(2))
        comp = CHANFRO * math.sqrt(2)
        for lado, ya, yb in self.modulos_chanfro:
            A = (0.0, T_C - CHANFRO + dz)
            meio = (A[0] + s[0] * comp / 2, A[1] + s[1] * comp / 2)

            def P(a, d):
                x, z = meio[0] + s[0] * a - nrm[0] * d, meio[1] + s[1] * a - nrm[1] * d
                return (W_C - x, z) if lado == "R" else (x, z)
            h = MODULO_H / 2
            hc = (MODULO_H + 2 * MODULO_FOLGA) / 2 + COBERTURA_FOLGA
            quad_mod = [P(-h, COBERTURA_ESP + 0.1), P(h, COBERTURA_ESP + 0.1),
                        P(h, COBERTURA_ESP + 0.1 + MODULO_ESP), P(-h, COBERTURA_ESP + 0.1 + MODULO_ESP)]
            quad_cob = [P(-hc, 0.0), P(hc, 0.0), P(hc, COBERTURA_ESP), P(-hc, COBERTURA_ESP)]
            mod.prisma_yz(quad_mod, ya + MODULO_FOLGA, yb - MODULO_FOLGA, COR_MODULO)
            cob.prisma_yz(quad_cob, ya - COBERTURA_FOLGA, yb + COBERTURA_FOLGA, COR_COBERTURA)
        return cob, mod

    def celula_e_display_3d(self, dz_display: float = 0.0) -> Malha:
        m = Malha()
        cx0, cy0, cx1, cy1 = self.celula
        m.caixa(cx0, cy0, CELULA_Z0, cx1, cy1, CELULA_Z1, COR_CELULA)
        dx0, dy0, dx1, dy1 = self.display
        m.caixa(dx0, dy0, DISPLAY_Z0 + dz_display, dx1, dy1, DISPLAY_Z1 + dz_display, COR_DISPLAY)
        jx0, jy0, jx1, jy1 = self.janela
        m.caixa(jx0, jy0, DISPLAY_Z1 + dz_display, jx1, jy1, DISPLAY_Z1 + 0.05 + dz_display, COR_JANELA)
        return m


# ------------------------------------------------------------------ 3D
def placa_3d() -> tuple[np.ndarray, np.ndarray]:
    """The board, from the sources make_3d.py draws it from, moved to where
    the case holds it (the GLB and the bodies are in the sheet's frame, the
    board's top-left at make_pcb.ORIGEM)."""
    glb = M3._glb_atual()
    if glb is None:
        raise SystemExit("sem GLB da placa")
    j, bina = M3.ler_glb(glb)
    pt, pc = M3.triangulos(j, bina)
    pt = np.stack([pt[..., 0] * 1000.0, -pt[..., 1] * 1000.0, pt[..., 2] * 1000.0], axis=-1)
    extra_t, extra_c = M3.caixas_das_pecas()
    silk_t, silk_c = M3.serigrafia()
    partes_t = [pt] + ([extra_t] if len(extra_t) else []) + ([silk_t] if len(silk_t) else [])
    partes_c = [pc] + ([extra_c] if len(extra_c) else []) + ([silk_c] if len(silk_c) else [])
    t = np.concatenate(partes_t) + np.array([PLACA_X0 - MP.ORIGEM[0], PLACA_Y0 - MP.ORIGEM[1], PLACA_Z0])
    return t, np.concatenate(partes_c)


def juntar(*malhas) -> tuple[np.ndarray, np.ndarray]:
    ts, cs = [], []
    for m in malhas:
        if isinstance(m, tuple):
            t, c = m
        else:
            t, c = m.arrays()
        if len(t):
            ts.append(t)
            cs.append(c)
    return np.concatenate(ts), np.concatenate(cs)


def renderizar(t, c, nome, w, h, az, el):
    t = np.stack([t[..., 0], -t[..., 1], t[..., 2]], axis=-1) / 1000.0
    img = M3.render(t, c, w, h, az, el)
    img.save(HERE / nome)
    print(f"  {nome}: {len(t)} triangulos")


# ------------------------------------------------------------------ 2D
S = 4.2


class Vista:
    def __init__(self, page, x0, y0, s=S):
        self.page, self.x0, self.y0, self.s = page, x0, y0, s

    def P(self, x, y):
        return fitz.Point(self.x0 + x * self.s, self.y0 + y * self.s)

    def rect(self, x0, y0, x1, y1, cor=(0, 0, 0), fill=None, largura=0.5, tracejado=None):
        sh = self.page.new_shape()
        sh.draw_rect(fitz.Rect(self.P(x0, y0), self.P(x1, y1)))
        sh.finish(color=cor, fill=fill, width=largura, dashes=tracejado)
        sh.commit()

    def poli(self, pts, cor=(0, 0, 0), fill=None, largura=0.8, tracejado=None):
        sh = self.page.new_shape()
        sh.draw_polyline([self.P(x, y) for x, y in pts] + [self.P(*pts[0])])
        sh.finish(color=cor, fill=fill, width=largura, dashes=tracejado, closePath=True)
        sh.commit()

    def linha(self, x1, y1, x2, y2, cor=(0.3, 0.3, 0.3), largura=0.5, tracejado=None):
        sh = self.page.new_shape()
        sh.draw_line(self.P(x1, y1), self.P(x2, y2))
        sh.finish(color=cor, width=largura, dashes=tracejado)
        sh.commit()

    def circulo(self, x, y, r, cor=(0, 0, 0), fill=None, largura=0.5):
        sh = self.page.new_shape()
        sh.draw_circle(self.P(x, y), r * self.s)
        sh.finish(color=cor, fill=fill, width=largura)
        sh.commit()

    def texto(self, x, y, s, tam=6.0, cor=(0.1, 0.1, 0.1), rot=0):
        self.page.insert_text(self.P(x, y), s, fontsize=tam, fontname="helv", color=cor, rotate=rot)

    def cota_h(self, y, x1, x2, s, acima=True):
        self.linha(x1, y, x2, y, (0.4, 0.4, 0.4), 0.4)
        for x in (x1, x2):
            self.linha(x, y - 1.0, x, y + 1.0, (0.4, 0.4, 0.4), 0.4)
        self.texto((x1 + x2) / 2 - len(s) * 0.65, y - 1.0 if acima else y + 2.6, s, 5.5, (0.35, 0.35, 0.35))

    def cota_v(self, x, y1, y2, s):
        self.linha(x, y1, x, y2, (0.4, 0.4, 0.4), 0.4)
        for y in (y1, y2):
            self.linha(x - 1.0, y, x + 1.0, y, (0.4, 0.4, 0.4), 0.4)
        self.texto(x + 1.2, (y1 + y2) / 2 + 1.0, s, 5.5, (0.35, 0.35, 0.35))


def _t(page, x, y, s, tam=10.0, negrito=True):
    page.insert_text(fitz.Point(x, y), s, fontsize=tam, fontname="hebo" if negrito else "helv",
                     color=(0.1, 0.1, 0.12))


def _paragrafo(page, x, y, s, tam, largura=780.0):
    linha = ""
    for w in s.split():
        prova = (linha + " " + w).strip()
        if fitz.get_text_length(prova, fontname="helv", fontsize=tam) > largura:
            page.insert_text(fitz.Point(x, y), linha, fontsize=tam, fontname="helv", color=(0.12, 0.12, 0.12))
            y += tam + 3
            linha = w
        else:
            linha = prova
    if linha:
        page.insert_text(fitz.Point(x, y), linha, fontsize=tam, fontname="helv", color=(0.12, 0.12, 0.12))
        y += tam + 3
    return y


def f2(v):
    return f"{v:.1f}".replace(".", ",")


def pagina_1(doc, cx: Caixa):
    page = doc.new_page(width=842, height=595)
    _t(page, 30, 32, f"GNSS Bike Computer - proposta de caixa em volta da placa de 34 x 90: {W_C:g} x {H_C:g} x {T_C:g} mm", 11)
    page.insert_text(fitz.Point(30, 46), "Escala 4,2 pt/mm. Proposta de 2026-09-26, nada impresso nem medido; as premissas e o que nao bate estao na pagina 3.",
                     fontsize=7, fontname="helv", color=(0.35, 0.35, 0.35))
    # ---- front, lid on
    fv = Vista(page, 40, 90)
    _t(page, 40, 80, "Frente, com a tampa", 8.5)
    fv.poli(cx.fora, (0.1, 0.1, 0.1), (0.86, 0.87, 0.89), 1.0)
    for lado in ("L", "R"):
        x0 = 0.0 if lado == "L" else W_C - CHANFRO
        fv.rect(x0, CHANFRO_Y[0], x0 + CHANFRO, CHANFRO_Y[1], (0.3, 0.3, 0.32), (0.78, 0.79, 0.81), 0.5)
    for lado, ya, yb in cx.modulos_chanfro:
        x0 = 0.4 if lado == "L" else W_C - CHANFRO + 0.4
        fv.rect(x0, ya, x0 + CHANFRO - 0.8, yb, (0.1, 0.12, 0.3), (0.35, 0.40, 0.55), 0.5)
    fv.rect(*cx.janela, cor=(0.1, 0.1, 0.1), fill=(0.72, 0.75, 0.72), largura=0.8)
    jx0, jy0, jx1, jy1 = cx.janela
    fv.texto(jx0 + 5, (jy0 + jy1) / 2, f"janela {JANELA_W:g} x {JANELA_H:g}".replace(".", ","), 5.5, (0.25, 0.3, 0.25))
    if cx.rebaixo_membrana:
        fv.rect(*cx.rebaixo_membrana, cor=(0.3, 0.3, 0.3), fill=(0.80, 0.81, 0.83), largura=0.4)
    for x, y in cx.capas:
        fv.rect(*quadrado(x, y, TECLA_CAPA), cor=(0.1, 0.1, 0.1), fill=(0.45, 0.46, 0.48), largura=0.6)
    for (sx, sy), (kx, ky) in zip(cx.teclas, cx.capas):
        fv.rect(*quadrado(sx, sy, 3.5), cor=(0.4, 0.1, 0.1), fill=None, largura=0.4, tracejado="[1.5 1] 0")
        fv.linha(sx, sy, kx, ky, (0.4, 0.1, 0.1), 0.4, "[1.5 1] 0")
    if cx.capas:
        fv.texto(cx.capas[0][0] - 6.0, cx.capas[0][1] - 6.5, "capas no passo de 14; tracejado: a chave na placa e a barra", 4.5, (0.4, 0.1, 0.1))
    fv.rect(*cx.faceta, cor=(0.2, 0.2, 0.22), fill=(0.80, 0.81, 0.84), largura=0.6)
    fv.rect(*cx.cobertura_faceta, cor=(0.2, 0.4, 0.5), fill=(0.80, 0.88, 0.92), largura=0.5)
    for b in cx.bolsos_faceta:
        fv.rect(*b, cor=(0.1, 0.12, 0.3), fill=(0.35, 0.40, 0.55), largura=0.5)
    fv.rect(*cx.furo_led, cor=(0.1, 0.5, 0.1), fill=(0.6, 0.9, 0.6), largura=0.5)
    fv.rect(*cx.furo_sensor, cor=(0.1, 0.3, 0.6), fill=(0.7, 0.8, 0.95), largura=0.5)
    fv.texto(cx.furo_led[0] - 12.5, cx.furo_led[1] - 0.8, "LED (proposta)", 4.5, (0.1, 0.4, 0.1))
    fv.texto(cx.furo_sensor[2] + 0.8, cx.furo_sensor[3] + 0.5, "sensor de luz (proposta)", 4.5, (0.1, 0.3, 0.6))
    for x, y in PARAFUSO_TAMPA:
        fv.circulo(x, y, 1.1, (0.2, 0.2, 0.2), (0.9, 0.9, 0.9), 0.5)
    ux0, ux1, _z0, _z1 = cx.usb
    fv.rect(ux0, H_C - 1.2, ux1, H_C, (0.1, 0.3, 0.7), (0.6, 0.7, 0.9), 0.5)
    fv.texto(ux1 + 1.0, H_C - 0.3, "USB-C", 4.5, (0.1, 0.3, 0.7))
    fv.cota_v(-4.0, 0, H_C, f"{H_C:g}")
    fv.cota_h(H_C + 4.0, 0, W_C, f"{W_C:g}", acima=False)
    fv.cota_v(W_C + 3.0, jy0, jy1, f2(JANELA_H))
    fv.cota_v(W_C + 3.0, cx.faceta[1], cx.faceta[3], "faceta")
    # ---- inside: the shell with the board
    xv = Vista(page, 340, 90)
    _t(page, 340, 80, "Por dentro: concha, placa, celula (tracejada) e display (tracejado)", 8.5)
    xv.poli(cx.fora, (0.1, 0.1, 0.1), (0.93, 0.94, 0.95), 1.0)
    xv.poli(cx.dentro, (0.5, 0.5, 0.5), None, 0.4)
    for x0, y0, x1, y1 in cx.nervuras:
        xv.rect(x0, y0, x1, y1, (0.3, 0.3, 0.3), (0.75, 0.76, 0.78), 0.4)
    for x, y in PARAFUSO_TAMPA:
        xv.circulo(x, y, BOSSA_D / 2, (0.2, 0.2, 0.2), (0.8, 0.8, 0.8), 0.5)
        xv.circulo(x, y, BOSSA_FURO / 2, (0.2, 0.2, 0.2), (1, 1, 1), 0.4)
    for f in cx.furos_fundo:
        xv.rect(*f, cor=(0.3, 0.3, 0.3), fill=(1, 1, 1), largura=0.4)
    xv.rect(*cx.celula, cor=(0.85, 0.5, 0.1), fill=None, largura=0.7, tracejado="[3 2] 0")
    rb = MD.RADIUS_DRAWING if hasattr(MD, "RADIUS_DRAWING") else 1.5
    xv.poli(contorno_arredondado(PLACA_X0, PLACA_Y0, PLACA_X0 + PLACA_W, PLACA_Y0 + PLACA_H, rb, 4),
            (0.1, 0.45, 0.15), (0.80, 0.90, 0.80), 0.9)
    for ref, p in sorted(cx.pecas.items()):
        x0, y0, x1, y1 = caixa_na_caixa(p)
        w, h = x1 - x0, y1 - y0
        if p["atras"]:
            xv.rect(x0, y0, x1, y1, (0.6, 0.2, 0.2), None, 0.4, "[1.5 1] 0")
        else:
            xv.rect(x0, y0, x1, y1, (0.25, 0.25, 0.3), (0.62, 0.64, 0.68), 0.3)
        if w * h >= 14.0 or ref.startswith("SW") or ref in ("J101", "E301", "J102", "J103", "J202", "D601", "U505"):
            xv.texto(x0 + 0.4, y0 + min(h, 2.6), ref, 4.0, (0.5, 0.1, 0.1) if p["atras"] else (0.05, 0.05, 0.1))
    xv.rect(*cx.display, cor=(0.2, 0.3, 0.6), fill=None, largura=0.8, tracejado="[3 2] 0")
    # what holds the board, drawn over it so that it shows
    for x, y in cx.bossas_placa:
        xv.circulo(x, y, BOSSA_PLACA_D / 2, (0.1, 0.4, 0.1), (0.75, 0.9, 0.75), 0.6)
        xv.circulo(x, y, BOSSA_FURO / 2, (0.1, 0.4, 0.1), (1, 1, 1), 0.4)
    for x, y in cx.pilares:
        xv.circulo(x, y, PILAR_D / 2, (0.1, 0.4, 0.1), (0.75, 0.9, 0.75), 0.6)
    for x, y in cx.furos_sobre_celula:
        xv.circulo(x, y, 1.6, (0.8, 0.1, 0.1), None, 0.7)
        xv.texto(x + 2.2, y + 0.8, "furo da placa sobre a celula: sem parafuso", 4.2, (0.8, 0.1, 0.1))
    xv.cota_h(H_C + 4.0, PLACA_X0, PLACA_X0 + PLACA_W, f"{PLACA_W:g}", acima=False)
    xv.cota_v(W_C + 3.0, PLACA_Y0, PLACA_Y0 + PLACA_H, f"{PLACA_H:g}")
    xv.cota_v(W_C + 9.0, 0, PLACA_Y0, f2(PLACA_Y0))
    xv.texto(-2.0, H_C + 12.0, "cinza: frente; tracejado vermelho: verso; verde: bossa e pilares que seguram a placa", 5.0, (0.3, 0.3, 0.3))
    # ---- section A-A (along the length, seen from the right): page x = z, page y = case y
    sv = Vista(page, 660, 120)
    _t(page, 640, 80, "Corte A-A pelo comprimento", 8.5)
    page.insert_text(fitz.Point(640, 92), "vertical: y da caixa; horizontal: z, do fundo para a tampa",
                     fontsize=6, fontname="helv", color=(0.35, 0.35, 0.35))

    def zr(z0, y0, z1, y1, cor, fill, largura=0.5, tracejado=None):
        sv.rect(z0, y0, z1, y1, cor, fill, largura, tracejado)
    zr(0, 0, FUNDO, H_C, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))
    zr(0, 0, TAMPA_Z0, PAREDE, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))
    ux0, ux1, uz0, uz1 = cx.usb
    zr(0, H_C - PAREDE, uz0, H_C, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))
    zr(uz1, H_C - PAREDE, T_C, H_C, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    zr(TAMPA_Z0, 0, T_C, H_C, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    zr(TAMPA_Z0, cx.janela[1], T_C, cx.janela[3], (0.1, 0.1, 0.1), (1, 1, 1), 0.3)     # window
    zr(DISPLAY_Z1, cx.display[1], TAMPA_Z0, cx.display[3], (0.5, 0.5, 0.5), (0.9, 0.9, 0.7), 0.2)   # tape
    zr(T_C, cx.faceta[1], T_C + FACETA_ALT, cx.faceta[3], (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    for b in cx.bolsos_faceta:
        zr(T_C + FACETA_ALT - COBERTURA_ESP - MODULO_ESP - 0.1, b[1], T_C + FACETA_ALT - COBERTURA_ESP, b[3],
           (0.1, 0.12, 0.3), (0.35, 0.40, 0.55), 0.4)
    zr(T_C + FACETA_ALT - COBERTURA_ESP, cx.cobertura_faceta[1], T_C + FACETA_ALT, cx.cobertura_faceta[3],
       (0.2, 0.4, 0.5), (0.80, 0.88, 0.92), 0.4)
    cx0, cy0, cx1, cy1 = cx.celula
    zr(CELULA_Z0, cy0, CELULA_Z1, cy1, (0.85, 0.5, 0.1), (1.0, 0.93, 0.8))
    zr(PLACA_Z0, PLACA_Y0, PLACA_Z1, PLACA_Y0 + PLACA_H, (0.1, 0.45, 0.15), (0.55, 0.75, 0.55))
    dx0, dy0, dx1, dy1 = cx.display
    zr(DISPLAY_Z0, dy0, DISPLAY_Z1, dy1, (0.2, 0.3, 0.6), (0.75, 0.8, 0.95))
    for ref, p in cx.pecas.items():
        y0, y1 = PLACA_Y0 + p["caixa"][1], PLACA_Y0 + p["caixa"][3]
        if p["atras"]:
            zr(PLACA_Z0 - p["altura"], y0, PLACA_Z0, y1, (0.6, 0.2, 0.2), (0.95, 0.8, 0.8), 0.3)
        else:
            zr(PLACA_Z1, y0, PLACA_Z1 + p["altura"], y1, (0.25, 0.25, 0.3), (0.62, 0.64, 0.68), 0.3)
    for x, y in cx.capas:
        zr(cx.topo_tecla, y - BARRA_LARG / 2, cx.topo_tecla + BARRA_ESP, y + BARRA_LARG / 2, (0.1, 0.1, 0.1), (0.45, 0.46, 0.48), 0.3)
        zr(cx.topo_tecla, y - TECLA_CAPA / 2, T_C - MEMBRANA_REBAIXO, y + TECLA_CAPA / 2, (0.1, 0.1, 0.1), (0.45, 0.46, 0.48), 0.3)
    if cx.rebaixo_membrana:
        zr(T_C - MEMBRANA_REBAIXO, cx.rebaixo_membrana[1], T_C - MEMBRANA_REBAIXO + MEMBRANA_ESP, cx.rebaixo_membrana[3],
           (0.3, 0.3, 0.3), (0.60, 0.62, 0.64), 0.3)
    sv.cota_v(T_C + FACETA_ALT + 6.0, 0, H_C, f"{H_C:g}")
    sv.cota_h(-4.0, 0, T_C, f"{T_C:g}")
    sv.texto(T_C + 2.0, dy0 + 3, "display", 5.0, (0.2, 0.3, 0.6))
    sv.texto(T_C + 2.0, cy1 - 1, "celula", 5.0, (0.7, 0.4, 0.05))
    sv.texto(T_C + 2.0, PLACA_Y0 + PLACA_H, "placa", 5.0, (0.1, 0.45, 0.15))
    sv.texto(-2.0, H_C + 8.0, (f"z: fundo {FUNDO:g}; celula {f2(CELULA_Z0)}-{f2(CELULA_Z1)}; placa {f2(PLACA_Z0)}-{f2(PLACA_Z1)}; "
                                f"display {f2(DISPLAY_Z0)}-{f2(DISPLAY_Z1)} colado sob a tampa; tampa {f2(TAMPA_Z0)}-{T_C:g}; faceta ate {f2(T_C + FACETA_ALT)}"),
             5.0, (0.3, 0.3, 0.3))


def pagina_2(doc, cx: Caixa):
    page = doc.new_page(width=842, height=595)
    _t(page, 30, 32, "Corte B-B pela largura (pelos chanfros e pelo display), a tecla e o bolso de um modulo", 11)
    # ---- B-B: page x = case x, page y = z (up)
    bv = Vista(page, 60, 330, s=8.0)
    _t(page, 60, 80, "Corte B-B, na altura do display (escala 8 pt/mm)", 8.5)

    def xz(x0, z0, x1, z1, cor, fill, largura=0.5, tracejado=None):
        bv.rect(x0, -z1, x1, -z0, cor, fill, largura, tracejado)
    xz(0, 0, W_C, FUNDO, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))
    for x0 in (0.0, W_C - PAREDE):
        xz(x0, FUNDO, x0 + PAREDE, T_C - CHANFRO, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))
    # the lid: plate and the two bevel solids (with a pocket cut)
    xz(PAREDE, TAMPA_Z0, W_C - PAREDE, T_C, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    xz(cx.janela[0], TAMPA_Z0, cx.janela[2], T_C, (0.1, 0.1, 0.1), (1, 1, 1), 0.3)
    xz(cx.display[0], DISPLAY_Z1, cx.display[2], TAMPA_Z0, (0.5, 0.5, 0.5), (0.9, 0.9, 0.7), 0.2)   # tape
    for lado in ("L", "R"):
        pol = cx._perfil_chanfro(lado, 0.0, MODULO_ESP + 0.1)
        bv.poli([(x, -z) for x, z in pol], (0.1, 0.1, 0.1), (0.80, 0.81, 0.83), 0.6)
    cob, mod = cx.coberturas_e_modulos_3d()
    # the modules and covers on the bevels, as their (x, z) outlines
    s = (1 / math.sqrt(2), 1 / math.sqrt(2))
    nrm = (-1 / math.sqrt(2), 1 / math.sqrt(2))
    comp = CHANFRO * math.sqrt(2)
    for lado in ("L", "R"):
        A = (0.0, T_C - CHANFRO)
        meio = (A[0] + s[0] * comp / 2, A[1] + s[1] * comp / 2)

        def P(a, d):
            x, z = meio[0] + s[0] * a - nrm[0] * d, meio[1] + s[1] * a - nrm[1] * d
            return ((W_C - x) if lado == "R" else x, -z)
        h = MODULO_H / 2
        hc = (MODULO_H + 2 * MODULO_FOLGA) / 2 + COBERTURA_FOLGA
        bv.poli([P(-h, COBERTURA_ESP + 0.1), P(h, COBERTURA_ESP + 0.1), P(h, COBERTURA_ESP + 0.1 + MODULO_ESP),
                 P(-h, COBERTURA_ESP + 0.1 + MODULO_ESP)], (0.1, 0.12, 0.3), (0.35, 0.40, 0.55), 0.5)
        bv.poli([P(-hc, 0.0), P(hc, 0.0), P(hc, COBERTURA_ESP), P(-hc, COBERTURA_ESP)], (0.2, 0.4, 0.5), (0.80, 0.88, 0.92), 0.5)
    cx0, _cy0, cx1, _cy1 = cx.celula
    xz(cx0, CELULA_Z0, cx1, CELULA_Z1, (0.85, 0.5, 0.1), (1.0, 0.93, 0.8))
    for x0, _y0, x1, _y1 in cx.nervuras[:2]:
        xz(x0, FUNDO, x1, FUNDO + 5.0, (0.3, 0.3, 0.3), (0.75, 0.76, 0.78), 0.4)
    xz(PLACA_X0, PLACA_Z0, PLACA_X0 + PLACA_W, PLACA_Z1, (0.1, 0.45, 0.15), (0.55, 0.75, 0.55))
    dx0, _dy0, dx1, _dy1 = cx.display
    xz(dx0, DISPLAY_Z0, dx1, DISPLAY_Z1, (0.2, 0.3, 0.6), (0.75, 0.8, 0.95))
    bv.cota_h(-T_C - 3.0, 0, W_C, f"{W_C:g}")
    bv.cota_v(W_C + 3.0, -T_C, 0, f"{T_C:g}")
    bv.cota_v(W_C + 9.0, -T_C, -(T_C - CHANFRO), f2(CHANFRO))
    bv.texto(W_C / 2 - 8, -T_C - 5.5, "chanfro de 45 graus com dois modulos por lado, em bolso, sob cobertura transparente", 5.5, (0.3, 0.3, 0.3))
    bv.texto(cx0 + 1, -CELULA_Z0 - 1, "celula", 5.0, (0.7, 0.4, 0.05))
    bv.texto(dx0 + 1, -DISPLAY_Z1 - 0.5, "display colado sob a tampa, em volta da janela", 5.0, (0.2, 0.3, 0.6))
    # ---- key detail: page x = case x, page y = z
    kv = Vista(page, 560, 330, s=11.0)
    _t(page, 560, 80, "Tecla da direita: barra da chave ate a capa (escala 11 pt/mm)", 8.5)
    sx = cx.teclas[-1][0] if cx.teclas else 0.0
    kx = cx.capas[-1][0] if cx.capas else sx
    xk = min(sx, kx) - 5.0
    fim = max(sx, kx) + 5.0

    def kz(x0, z0, x1, z1, cor, fill, largura=0.5):
        kv.rect(x0 - xk, -z1, x1 - xk, -z0, cor, fill, largura)
    kz(xk, PLACA_Z0, fim, PLACA_Z1, (0.1, 0.45, 0.15), (0.55, 0.75, 0.55))
    kz(sx - 1.95, PLACA_Z1, sx + 1.95, PLACA_Z1 + TECLA_ALT - 0.5, (0.25, 0.25, 0.3), (0.62, 0.64, 0.68))
    kz(sx - 0.9, PLACA_Z1 + TECLA_ALT - 0.5, sx + 0.9, PLACA_Z1 + TECLA_ALT, (0.25, 0.25, 0.3), (0.62, 0.64, 0.68))
    kz(xk, TAMPA_Z0, kx - TECLA_FURO / 2, T_C - MEMBRANA_REBAIXO, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    kz(kx + TECLA_FURO / 2, TAMPA_Z0, fim, T_C - MEMBRANA_REBAIXO, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    kz(min(sx, kx) - BARRA_LARG / 2, cx.topo_tecla, max(sx, kx) + BARRA_LARG / 2, cx.topo_tecla + BARRA_ESP,
       (0.1, 0.1, 0.1), (0.45, 0.46, 0.48))
    kz(kx - TECLA_CAPA / 2, cx.topo_tecla, kx + TECLA_CAPA / 2, T_C - MEMBRANA_REBAIXO, (0.1, 0.1, 0.1), (0.45, 0.46, 0.48))
    kz(xk, T_C - MEMBRANA_REBAIXO, fim, T_C - MEMBRANA_REBAIXO + MEMBRANA_ESP, (0.3, 0.3, 0.3), (0.60, 0.62, 0.64))
    kv.texto(0.2, -PLACA_Z1 - 0.3, "placa", 4.5, (0.1, 0.45, 0.15))
    kv.texto(0.2, -TAMPA_Z0 - 0.3, "tampa", 4.5, (0.1, 0.1, 0.1))
    kv.texto(sx - xk - 4.0, -PLACA_Z1 - TECLA_ALT + 0.7, "TS-1088R na placa", 4.5, (0.25, 0.25, 0.3))
    kv.texto(0.2, -cx.topo_tecla - 0.9, f"barra {f2(BARRA_LARG)} x {f2(BARRA_ESP)} sob a tampa, da chave a capa", 4.5, (0.1, 0.1, 0.1))
    kv.texto(0.2, -(T_C - MEMBRANA_REBAIXO + MEMBRANA_ESP) - 0.4, "membrana TPU 0,3 no rebaixo de 0,4", 4.5, (0.3, 0.3, 0.3))
    kv.cota_h(-PLACA_Z0 + 1.2, sx - xk, kx - xk, f"{f2(abs(kx - sx))}", acima=False)
    kv.cota_h(-T_C - 0.8, kx - TECLA_FURO / 2 - xk, kx + TECLA_FURO / 2 - xk, f"furo {f2(TECLA_FURO)}")
    # ---- module pocket detail (facet), page x = case x, page y = z
    # page y = 640 - 10 z: the pocket (z 15,5 to 18,5) lands at y 455-485,
    # under its title
    mv = Vista(page, 560, 640, s=10.0)
    _t(page, 560, 430, "Bolso de um modulo na faceta (escala 10 pt/mm)", 8.5)
    b = cx.bolsos_faceta[0]
    xm = b[0] - 2.0
    topo = T_C + FACETA_ALT

    def mz(x0, z0, x1, z1, cor, fill, largura=0.5):
        mv.rect(x0 - xm, -z1, x1 - xm, -z0, cor, fill, largura)
    mz(b[0] - 2.0, TAMPA_Z0, b[2] + 2.0, TAMPA_Z0 + 0.5, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    mz(b[0] - 2.0, TAMPA_Z0 + 0.5, b[0], topo - COBERTURA_ESP, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    mz(b[2], TAMPA_Z0 + 0.5, b[2] + 2.0, topo - COBERTURA_ESP, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))
    mz(b[0] + MODULO_FOLGA, topo - COBERTURA_ESP - 0.1 - MODULO_ESP, b[2] - MODULO_FOLGA, topo - COBERTURA_ESP - 0.1,
       (0.1, 0.12, 0.3), (0.35, 0.40, 0.55))
    mz(b[0] - 1.0, topo - COBERTURA_ESP, b[2] + 1.0, topo, (0.2, 0.4, 0.5), (0.80, 0.88, 0.92))
    mv.texto(0.2, -TAMPA_Z0 - 0.2, "tampa (0,5 sob o bolso; furos de fio o1)", 4.5, (0.1, 0.1, 0.1))
    mv.texto(2.6, -(topo - COBERTURA_ESP - 0.1) - 0.6, f"modulo 23 x 8 x {f2(MODULO_ESP)}, colado no fundo do bolso", 4.5, (0.1, 0.12, 0.3))
    mv.texto(2.6, -topo - 0.4, f"cobertura transparente {f2(COBERTURA_ESP)} (resina clara ou PET), colada no degrau", 4.5, (0.2, 0.4, 0.5))
    mv.cota_h(-TAMPA_Z0 + 1.6, b[0] - xm, b[2] - xm, f"bolso {f2(b[2] - b[0])}", acima=False)


def conflitos(cx: Caixa) -> list[str]:
    out = []
    p = cx.pecas
    for ref, nome in (("D601", "LED RGB"), ("U505", "sensor de luz OPT3001")):
        q = p.get(ref)
        if q:
            x0, y0, x1, y1 = q["caixa"]
            sob = y0 < SOMBRA_DISPLAY[1] and not q["atras"]
            if sob:
                prop = LED_PROPOSTA if ref == "D601" else SENSOR_LUZ_PROPOSTA
                out.append(f"{ref} ({nome}) esta em ({q['x']:g}; {q['y']:g}) da placa, DEBAIXO do vidro do display (y < {SOMBRA_DISPLAY[1]:g}): "
                           f"nenhuma janela na tampa o alcanca. A janela foi desenhada na posicao PROPOSTA ({prop[0]:g}; {prop[1]:g}), "
                           "abaixo das teclas; a peca tem de mudar de lugar na placa.")
    for x, y in cx.furos_sobre_celula:
        out.append(f"O furo M2 da placa em ({x - PLACA_X0:g}; {y - PLACA_Y0:g}) fica em cima da celula: nao pode ter bossa. "
                   "A placa fica em UM parafuso (o outro furo) e tres pilares; o furo precisa mudar para fora da sombra da celula, por exemplo para y >= 72.")
    altos = []
    for ref, q in sorted(p.items()):
        y0, y1 = q["caixa"][1], q["caixa"][3]
        if q["atras"] and y1 > SOMBRA_CELULA[0] and y0 < SOMBRA_CELULA[1] and q["altura"] > CELULA_VAO:
            altos.append(f"{ref} {q['altura']:g} mm sob a celula (teto {CELULA_VAO:g})")
        if not q["atras"] and y1 > SOMBRA_DISPLAY[0] and y0 < SOMBRA_DISPLAY[1] and q["altura"] > DISPLAY_VAO:
            altos.append(f"{ref} {q['altura']:g} mm sob o display (teto {DISPLAY_VAO:g})")
    if altos:
        out.append("Pecas mais altas que o teto da sombra em que estao (regra ME2 do dry run), que a caixa nao resolve: " + "; ".join(altos) + ".")
    out.append(f"A celula ({CELULA_W:g}) e o display ({DISPLAY_W:g}) sao mais largos que a placa ({PLACA_W:g}): as nervuras da celula e o bolso do display "
               "estao na caixa, fora da placa, e e a caixa que os posiciona.")
    out.append("O conceito de 2026-09-20 (docs/13, docs/img/placa-nova-caixa.svg) tinha a faceta solar entre o display e os botoes e o "
               "engate de quarto de volta atras; aqui a faceta desceu para baixo das teclas, porque a placa poe as teclas logo abaixo "
               "do display, e o engate nao foi desenhado.")
    return out


def pagina_3(doc, cx: Caixa):
    page = doc.new_page(width=842, height=595)
    _t(page, 30, 32, "Premissas, o que e proposta, o que nao bate, e as pecas", 11)
    y = 54
    itens = [
        f"Caixa {W_C:g} x {H_C:g} x {T_C:g} mm (o conceito tinha 19 de espessura; com 16 o USB-C e o conector SWD batiam na tampa: sob a tampa sobram {f2(TAMPA_Z0 - PLACA_Z1)} mm da face da placa), raio {R_C:g}, paredes {PAREDE:g}, fundo {FUNDO:g}, tampa {TAMPA:g}; chanfro de 45 graus e {CHANFRO:g} mm nas arestas longas, na tampa, de y {CHANFRO_Y[0]:g} a {CHANFRO_Y[1]:g}, com dois modulos solares por lado; faceta plana de {FACETA_ALT:g} mm sobre a tampa abaixo das teclas, com dois modulos; cada grupo sob uma cobertura transparente de {f2(COBERTURA_ESP)} mm colada num degrau de {COBERTURA_FOLGA:g} mm em volta dos bolsos; furos de fio de o1 de cada modulo para dentro.",
        f"Placa {PLACA_W:g} x {PLACA_H:g} x {PLACA_ESP:g}, gnssbike.kicad_pcb de hoje: centrada na largura (x {PLACA_X0:g} a {PLACA_X0 + PLACA_W:g}) e a {FOLGA_PLACA:g} mm da parede de baixo (y {f2(PLACA_Y0)} a {f2(PLACA_Y0 + PLACA_H)}), para o USB-C chegar ao entalhe da parede ({cx.usb[1] - cx.usb[0]:g} x {f2(cx.usb[3] - cx.usb[2])}, na parede e na aba da tampa).",
        f"Pilha: celula {CELULA_W:g} x {CELULA_H:g} x {CELULA_ESP:g} colada no fundo (fita dupla face) dentro de quatro nervuras de 2 x 5 mm, a 0,25 mm dela; 0,5 mm de ar e {CELULA_VAO:g} mm de pecas do verso ate a placa; a placa em {f2(PLACA_Z0)}-{f2(PLACA_Z1)}, apoiada em {len(cx.bossas_placa)} bossa(s) M2 e {len(cx.pilares)} pilares de o{PILAR_D:g}; {DISPLAY_VAO:g} mm de pecas da frente; o display ({DISPLAY_W:g} x {DISPLAY_H:g} x {DISPLAY_ESP:g}) colado por baixo da tampa, em volta da janela, com fita de {FITA_DISPLAY:g}: o vidro fica {f2(T_C - DISPLAY_Z1)} mm abaixo da face da tampa (a alternativa, um aro em relevo, nao esta desenhada).",
        f"Teclas: as tres chaves ficam onde a placa as pos (x 4,5 a 21,5 da placa: a zona da antena do modulo nao deixa a terceira passar de 21,5), e as CAPAS ficam espalhadas na largura do display, no passo de {TECLA_PASSO:g} mm centrado na janela, como o dono pediu; cada capa e quadrada, de {TECLA_CAPA:g} mm num furo de {TECLA_FURO:g}, e tem por baixo da tampa uma barra de {BARRA_LARG:g} x {BARRA_ESP:g} que vai ate a sua chave (a da direita anda {f2(abs(cx.capas[-1][0] - cx.teclas[-1][0])) if cx.capas else '0'} mm): a barra e o que aperta o embolo do TS-1088R ({TECLA_ALT:g} mm) e o que segura a capa; a haste quadrada nao a deixa girar. Membrana de TPU de {MEMBRANA_ESP:g} colada no rebaixo de {MEMBRANA_REBAIXO:g} sobre as tres (chuva); curso {TECLA_CURSO:g}.",
        f"Tampa presa por 4 parafusos M2 nos cantos, em bossas de o{BOSSA_D:g} com furo de o{BOSSA_FURO:g} (auto-atarraxante); a vedacao da linha de particao (cordao de silicone ou junta cortada) nao esta desenhada. No fundo: {len(cx.furos_fundo)} furos (som do buzzer, respiro do barometro com rebaixo para a membrana).",
        "Nao desenhados: o engate de quarto de volta atras, a saida dos fios dos seis modulos ate J103, J104 e J105 (passam pelos furos de o1 e correm por dentro da tampa), o respiro do USB-C IPX8, textos e logotipo.",
        "Nada disto e decisao final: e uma proposta desenhada em volta da placa de hoje, para decidir em cima dela, e nao foi impressa.",
    ]
    for s in itens:
        y = _paragrafo(page, 30, y, s, 7.2)
        y += 3
    y += 6
    _t(page, 30, y, "O que nao bate", 9.5)
    y += 15
    for i, s in enumerate(conflitos(cx), 1):
        y = _paragrafo(page, 30, y, f"{i}. {s}", 7.2)
        y += 3
    y += 6
    _t(page, 30, y, "Pecas e arquivos", 9.5)
    y += 15
    for s in [
        "caixa-concha.stl (PETG ou ASA, 0,2 mm, sem suporte); caixa-tampa.stl (idem, impressa de cabeca para baixo, com suporte sob os chanfros); caixa-tecla.stl (3x, resina ou PETG); "
        "caixa-membrana-teclas.stl (TPU 0,3 mm, ou filme de PET cortado); caixa-cobertura-faceta.stl e caixa-cobertura-chanfro.stl (4x): resina transparente, ou PET de 0,5 cortado no tamanho.",
        "Ferragens: 4 parafusos M2 x 8 (tampa), 1 M2 x 6 (placa), fita dupla face de 0,5 para a celula e os modulos, fita de 0,2 para o display, silicone neutro para as coberturas e a particao.",
        "Os STL sao sopas de triangulos de caixas sobrepostas (o fatiador as une), nao solidos de CAD; servem para imprimir a primeira prova, nao para usinar.",
    ]:
        y = _paragrafo(page, 30, y, s, 7.2)
        y += 3


# ------------------------------------------------------------------ main
def main() -> int:
    pecas = ler_placa()
    cx = Caixa(pecas)
    print(f"caixa {W_C:g} x {H_C:g} x {T_C:g}; placa em x {PLACA_X0:g}-{PLACA_X0 + PLACA_W:g}, y {PLACA_Y0:.1f}-{PLACA_Y0 + PLACA_H:.1f}; "
          f"z: celula {CELULA_Z0:.1f}-{CELULA_Z1:.1f}, placa {PLACA_Z0:.1f}-{PLACA_Z1:.1f}, display {DISPLAY_Z0:.1f}-{DISPLAY_Z1:.1f}, "
          f"tampa {TAMPA_Z0:.1f}-{T_C:g}; {len(cx.teclas)} teclas, {len(cx.bossas_placa)} bossa(s) da placa, {len(cx.furos_sobre_celula)} furo(s) sobre a celula")
    doc = fitz.open()
    pagina_1(doc, cx)
    pagina_2(doc, cx)
    pagina_3(doc, cx)
    doc.save(HERE / "gnssbike-caixa.pdf", garbage=3, deflate=True)
    print("  gnssbike-caixa.pdf: 3 paginas")
    for s in conflitos(cx):
        print("  - " + s)

    concha = cx.concha()
    tampa = cx.tampa(TAMPA_Z0)
    teclas3 = cx.teclas_3d()
    membrana = cx.membrana_3d()
    cob, mod = cx.coberturas_e_modulos_3d()
    for nome, malha in (("caixa-concha.stl", concha), ("caixa-tampa.stl", tampa),
                        ("caixa-membrana-teclas.stl", membrana)):
        n = malha.stl(HERE / nome)
        print(f"  {nome}: {n} triangulos")
    # each key alone at the origin (the bars differ), one bevel cover, the facet cover
    for i in range(len(cx.teclas)):
        uma = cx.tecla_3d(i, origem=(cx.capas[i][0], cx.capas[i][1], cx.topo_tecla))
        nome = f"caixa-tecla-{i + 1}.stl"
        print(f"  {nome}: {uma.stl(HERE / nome)} triangulos (barra de {abs(cx.capas[i][0] - cx.teclas[i][0]):.1f} mm)")
    cf = Malha()
    x0, y0, x1, y1 = cx.cobertura_faceta
    cf.caixa(0, 0, 0, x1 - x0, y1 - y0, COBERTURA_ESP, COR_COBERTURA)
    print(f"  caixa-cobertura-faceta.stl: {cf.stl(HERE / 'caixa-cobertura-faceta.stl')} triangulos")
    cc = Malha()
    hc = (MODULO_H + 2 * MODULO_FOLGA) + 2 * COBERTURA_FOLGA
    cc.caixa(0, 0, 0, hc, MODULO_W + 2 * MODULO_FOLGA + 2 * COBERTURA_FOLGA, COBERTURA_ESP, COR_COBERTURA)
    print(f"  caixa-cobertura-chanfro.stl: {cc.stl(HERE / 'caixa-cobertura-chanfro.stl')} triangulos (imprimir 4)")

    placa = placa_3d()
    interior = cx.celula_e_display_3d()
    t, c = juntar(concha, placa, interior, teclas3)
    renderizar(t, c, "gnssbike-3d-caixa-aberta.png", 1600, 1500, 200.0, 40.0)
    t, c = juntar(concha, placa, interior, tampa, teclas3, membrana, cob, mod)
    renderizar(t, c, "gnssbike-3d-caixa-frente.png", 1100, 1800, 0.0, 90.0)
    dz = 24.0
    tampa_alta = cx.tampa(TAMPA_Z0 + dz)
    cob_a, mod_a = cx.coberturas_e_modulos_3d(dz)
    # the display goes up with the lid: it is glued into the lid's pocket
    t, c = juntar(concha, placa, cx.celula_e_display_3d(dz_display=dz), tampa_alta, cx.teclas_3d(dz),
                  cx.membrana_3d(dz), cob_a, mod_a)
    renderizar(t, c, "gnssbike-3d-caixa-explodida.png", 1600, 1600, 200.0, 32.0)
    return 0


if __name__ == "__main__":
    sys.exit(main())
