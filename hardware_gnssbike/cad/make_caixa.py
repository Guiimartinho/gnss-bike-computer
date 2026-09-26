#!/usr/bin/env python3
"""The case with the board inside: 2D (a PDF) and 3D (two PNG views).

The case is the concept of docs/13 (tools/docs/case_drawing.py): 62 x 104 x
19 mm, corner radius 7, drawn on 2026-09-20 around the OLD 55 mm board. The
board is the real one, 34 x 90 mm, read from gnssbike.kicad_pcb with every
footprint's courtyard, height and face, plus the GLB the 3D renderer uses.
Nothing here decides the case: the drawing shows how the board of today sits
in the case of the concept, so that the owner can see what fits and what
does not (04-pcb-e-caixa.md: "a placa nao e dimensionada pela caixa, e a
caixa nao e dimensionada pela placa").

Where the board sits is an ASSUMPTION, stated on page 2 of the PDF and in
the numbers this script prints:
  - centred in the width of the case;
  - placed so that the display's outline (40,08 x 61,8, over the board's
    y 2,7-64,5 - make_dxf.ZONES) lands on the display outline of the concept
    (case y 8,6-70,4, viewing area at y 9,6, case_drawing.py `vy`);
  - the cell (36 x 60 x 7) lies on the floor of the back shell, the board on
    the cell, the display 2,6 mm over the board's front face (the shadow
    ceiling), the lid 1,5 mm thick on top of 19 mm.

Run:  python hardware_gnssbike/cad/make_caixa.py
Out:  gnssbike-caixa.pdf (front with the lid, the inside, a section, the
      assumptions and the conflicts), gnssbike-3d-caixa-aberta.png (the
      back shell with the board, the lid floating above it) and
      gnssbike-3d-caixa-frente.png (the lid on, seen from the front).
"""

from __future__ import annotations

import math
import pathlib
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
W_C, H_C, T_C, R_C = 62.0, 104.0, 19.0, 7.0     # case_drawing.py W, H, T, R
PAREDE, FUNDO, TAMPA = 2.0, 1.5, 1.5              # docs/14: walls about 2 mm
# ------------------------------------------------------- the parts held
DISPLAY_W, DISPLAY_H, DISPLAY_ESP, DISPLAY_VAO = 40.08, 61.8, 1.0, 2.6
JANELA_W, JANELA_H = 36.28, 59.8                  # viewing area (case_drawing.py)
CELULA_W, CELULA_H, CELULA_ESP, CELULA_VAO = 36.0, 60.0, 7.0, 1.2
PLACA_W, PLACA_H, PLACA_ESP = MD.W, MD.H, 0.8
SOMBRA_DISPLAY = (64.5 - 61.8, 64.5)              # board y, make_dxf.ZONES
SOMBRA_CELULA = (9.9, 69.9)
JANELA_Y_CAIXA = 9.6                              # case_drawing.py `vy`
# ---------------------------------------------- where the board sits
PLACA_X0 = (W_C - PLACA_W) / 2.0
PLACA_Y0 = JANELA_Y_CAIXA - (DISPLAY_H - JANELA_H) / 2.0 - SOMBRA_DISPLAY[0]
CELULA_Z0 = FUNDO
PLACA_Z0 = CELULA_Z0 + CELULA_ESP
DISPLAY_Z0 = PLACA_Z0 + PLACA_ESP + DISPLAY_VAO
TAMPA_Z0 = T_C - TAMPA
FURO_TECLA = 0.5                                  # air around a key's courtyard, in its hole
# the concept's own features, to show where they fall against the board
CONCEITO_TECLAS_Y = 95.0
CONCEITO_FACETA = (74.6, 88.8)                    # sloped solar facet, case y
CONCEITO_USB = (26.2, H_C - 1.4, 9.6, 2.8)         # x, y, w, h of the port slot

COR_CAIXA = (0.22, 0.23, 0.26)
COR_TAMPA = (0.27, 0.28, 0.31)
COR_CELULA = (0.30, 0.31, 0.34)
COR_DISPLAY = (0.13, 0.14, 0.17)
COR_JANELA = (0.55, 0.58, 0.55)


# ------------------------------------------------------------ geometry
def contorno_arredondado(x0: float, y0: float, x1: float, y1: float, r: float,
                         n: int = 6) -> list[tuple[float, float]]:
    """A rounded rectangle as a closed polyline (y down), n points per corner."""
    pts: list[tuple[float, float]] = []
    cantos = ((x1 - r, y0 + r, -90.0), (x1 - r, y1 - r, 0.0),
              (x0 + r, y1 - r, 90.0), (x0 + r, y0 + r, 180.0))
    for cx, cy, a0 in cantos:
        for k in range(n + 1):
            a = math.radians(a0 + 90.0 * k / n)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def _quad(a, b, c, d, cor, tris, cols):
    tris.append(np.array([a, b, c], dtype=np.float64))
    tris.append(np.array([a, c, d], dtype=np.float64))
    cols.append(np.array(cor))
    cols.append(np.array(cor))


def caixa_3d(x0, y0, z0, x1, y1, z1, cor, tris, cols) -> None:
    b = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0)]
    t = [(x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
    _quad(b[0], b[3], b[2], b[1], cor, tris, cols)
    _quad(t[0], t[1], t[2], t[3], cor, tris, cols)
    for k in range(4):
        a, c = k, (k + 1) % 4
        _quad(b[a], b[c], t[c], t[a], cor, tris, cols)


def extrusao(pts, z0, z1, cor, tris, cols) -> None:
    """A convex polygon extruded between two heights (fans for the caps)."""
    n = len(pts)
    cx = sum(p[0] for p in pts) / n
    cy = sum(p[1] for p in pts) / n
    for k in range(n):
        a, b = pts[k], pts[(k + 1) % n]
        tris.append(np.array([(cx, cy, z0), (b[0], b[1], z0), (a[0], a[1], z0)]))
        cols.append(np.array(cor))
        tris.append(np.array([(cx, cy, z1), (a[0], a[1], z1), (b[0], b[1], z1)]))
        cols.append(np.array(cor))
        _quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1),
              (a[0], a[1], z1), cor, tris, cols)


def anel(fora, dentro, z0, z1, cor, tris, cols) -> None:
    """The wall between two closed polylines with the same point count."""
    n = len(fora)
    for k in range(n):
        a, b = fora[k], fora[(k + 1) % n]
        c, d = dentro[k], dentro[(k + 1) % n]
        _quad((a[0], a[1], z0), (b[0], b[1], z0), (d[0], d[1], z0),
              (c[0], c[1], z0), cor, tris, cols)
        _quad((a[0], a[1], z1), (c[0], c[1], z1), (d[0], d[1], z1),
              (b[0], b[1], z1), cor, tris, cols)
        _quad((a[0], a[1], z0), (b[0], b[1], z0), (b[0], b[1], z1),
              (a[0], a[1], z1), cor, tris, cols)
        _quad((c[0], c[1], z0), (c[0], c[1], z1), (d[0], d[1], z1),
              (d[0], d[1], z0), cor, tris, cols)


def placa_com_furos(x0, y0, x1, y1, z0, z1, furos, cor, tris, cols) -> None:
    """A plate minus rectangular holes, as the grid of cells between the
    holes' edges (every cell wholly inside a hole is left out)."""
    xs = sorted({x0, x1} | {f[0] for f in furos} | {f[2] for f in furos})
    ys = sorted({y0, y1} | {f[1] for f in furos} | {f[3] for f in furos})
    for i in range(len(xs) - 1):
        for j in range(len(ys) - 1):
            cx = (xs[i] + xs[i + 1]) / 2.0
            cy = (ys[j] + ys[j + 1]) / 2.0
            if any(f[0] <= cx <= f[2] and f[1] <= cy <= f[3] for f in furos):
                continue
            caixa_3d(xs[i], ys[j], z0, xs[i + 1], ys[j + 1], z1, cor, tris, cols)


# ------------------------------------------------------------ the board
def ler_placa():
    pecas, _pads, _seg, _vias, _cortes = DR.ler(HERE / "gnssbike.kicad_pcb")
    for ref, p in pecas.items():
        nome_fp = FPS.FP.get(ref, ("", 0, 0))[0]
        # the same resolution the ME2 rule uses: the data sheet's number,
        # else the package drawn in footprints.CORPO, else a common height
        alt = DR.altura_do_footprint(nome_fp) if nome_fp else None
        p["altura"] = alt[0] if alt else 1.0
    return pecas


def teclas(pecas) -> list[tuple[float, float]]:
    return [(p["x"], p["y"]) for ref, p in sorted(pecas.items()) if ref.startswith("SW")]


def furos_da_tampa(pecas) -> list[tuple[float, float, float, float]]:
    """The window over the viewing area and a hole over each key: the key's
    courtyard plus FURO_TECLA of air a side (a 7 mm square reached into the
    display's outline, 1,5 mm above the keys)."""
    jx0 = (W_C - JANELA_W) / 2.0
    furos = [(jx0, JANELA_Y_CAIXA, jx0 + JANELA_W, JANELA_Y_CAIXA + JANELA_H)]
    for ref, p in sorted(pecas.items()):
        if not ref.startswith("SW"):
            continue
        x0, y0, x1, y1 = p["caixa"]
        furos.append((PLACA_X0 + x0 - FURO_TECLA, PLACA_Y0 + y0 - FURO_TECLA,
                      PLACA_X0 + x1 + FURO_TECLA, PLACA_Y0 + y1 + FURO_TECLA))
    return furos


# ------------------------------------------------------------------ 3D
def cena_3d(pecas, tampa_z: float | None):
    """Everything in the case's frame: mm, y down, z up from the outer face
    of the floor. `tampa_z` is where the lid's underside goes (None: no lid)."""
    tris: list[np.ndarray] = []
    cols: list[np.ndarray] = []
    fora = contorno_arredondado(0.0, 0.0, W_C, H_C, R_C)
    dentro = contorno_arredondado(PAREDE, PAREDE, W_C - PAREDE, H_C - PAREDE,
                                  R_C - PAREDE)
    extrusao(fora, 0.0, FUNDO, COR_CAIXA, tris, cols)
    anel(fora, dentro, FUNDO, T_C - TAMPA, COR_CAIXA, tris, cols)
    # the cell on the floor, the display over the board
    cx0 = PLACA_X0 + PLACA_W / 2.0 - CELULA_W / 2.0
    caixa_3d(cx0, PLACA_Y0 + SOMBRA_CELULA[0], CELULA_Z0, cx0 + CELULA_W,
             PLACA_Y0 + SOMBRA_CELULA[1], CELULA_Z0 + CELULA_ESP, COR_CELULA, tris, cols)
    dx0 = PLACA_X0 + PLACA_W / 2.0 - DISPLAY_W / 2.0
    caixa_3d(dx0, PLACA_Y0 + SOMBRA_DISPLAY[0], DISPLAY_Z0, dx0 + DISPLAY_W,
             PLACA_Y0 + SOMBRA_DISPLAY[1], DISPLAY_Z0 + DISPLAY_ESP, COR_DISPLAY, tris, cols)
    jx0 = (W_C - JANELA_W) / 2.0
    caixa_3d(jx0, JANELA_Y_CAIXA, DISPLAY_Z0 + DISPLAY_ESP, jx0 + JANELA_W,
             JANELA_Y_CAIXA + JANELA_H, DISPLAY_Z0 + DISPLAY_ESP + 0.05, COR_JANELA,
             tris, cols)
    if tampa_z is not None:
        anel(fora, dentro, tampa_z, tampa_z + TAMPA, COR_TAMPA, tris, cols)
        placa_com_furos(PAREDE, PAREDE, W_C - PAREDE, H_C - PAREDE, tampa_z,
                        tampa_z + TAMPA, furos_da_tampa(pecas), COR_TAMPA, tris, cols)
    cena_t = np.array(tris)
    cena_c = np.array(cols)

    # the board, from the same sources make_3d.py draws it from: the GLB in
    # metres with y up, the bodies and the silkscreen in mm with y down
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
    placa_t = np.concatenate(partes_t)
    placa_c = np.concatenate(partes_c)
    # both are in the SHEET's coordinates (the board's top-left corner sits
    # at make_pcb.ORIGEM, measured on the GLB: x 25-59, y 25-115): the
    # origin comes off before the board goes where the case holds it
    placa_t = placa_t + np.array([PLACA_X0 - MP.ORIGEM[0], PLACA_Y0 - MP.ORIGEM[1], PLACA_Z0])
    todos_t = np.concatenate([cena_t, placa_t])
    todos_c = np.concatenate([cena_c, placa_c])
    # to the renderer's frame: metres, y up
    todos_t = np.stack([todos_t[..., 0], -todos_t[..., 1], todos_t[..., 2]], axis=-1) / 1000.0
    return todos_t, todos_c


# ------------------------------------------------------------------ 2D
S = 4.2                        # points per mm on the page


class Vista:
    def __init__(self, page: fitz.Page, x0: float, y0: float):
        self.page, self.x0, self.y0 = page, x0, y0

    def P(self, x: float, y: float) -> fitz.Point:
        return fitz.Point(self.x0 + x * S, self.y0 + y * S)

    def rect(self, x, y, w, h, cor=(0, 0, 0), fill=None, largura=0.6, tracejado=None):
        sh = self.page.new_shape()
        sh.draw_rect(fitz.Rect(self.P(x, y), self.P(x + w, y + h)))
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
        sh.draw_circle(self.P(x, y), r * S)
        sh.finish(color=cor, fill=fill, width=largura)
        sh.commit()

    def texto(self, x, y, s, tam=6.0, cor=(0.1, 0.1, 0.1), rot=0):
        self.page.insert_text(self.P(x, y), s, fontsize=tam, fontname="helv",
                              color=cor, rotate=rot)

    def cota_h(self, y, x1, x2, s):
        self.linha(x1, y, x2, y, (0.4, 0.4, 0.4), 0.4)
        for x in (x1, x2):
            self.linha(x, y - 1.0, x, y + 1.0, (0.4, 0.4, 0.4), 0.4)
        self.texto((x1 + x2) / 2 - len(s) * 0.7, y - 1.0, s, 5.5, (0.35, 0.35, 0.35))

    def cota_v(self, x, y1, y2, s):
        self.linha(x, y1, x, y2, (0.4, 0.4, 0.4), 0.4)
        for y in (y1, y2):
            self.linha(x - 1.0, y, x + 1.0, y, (0.4, 0.4, 0.4), 0.4)
        self.texto(x + 1.2, (y1 + y2) / 2 + 1.0, s, 5.5, (0.35, 0.35, 0.35))


def _titulo(page: fitz.Page, x: float, y: float, s: str, tam: float = 10.0):
    page.insert_text(fitz.Point(x, y), s, fontsize=tam, fontname="hebo", color=(0.1, 0.1, 0.12))


def pagina_1(doc: fitz.Document, pecas) -> None:
    page = doc.new_page(width=842, height=595)
    _titulo(page, 30, 32, "GNSS Bike Computer - a placa de 34 x 90 dentro da caixa de 62 x 104 x 19 (conceito de 2026-09-20)", 11)
    page.insert_text(fitz.Point(30, 46), "Escala 4,2 pt/mm. Nada montado nem medido: a posicao da placa e uma premissa (pagina 2).",
                     fontsize=7, fontname="helv", color=(0.35, 0.35, 0.35))
    furos = furos_da_tampa(pecas)
    contorno = contorno_arredondado(0, 0, W_C, H_C, R_C)

    # ---- front, lid on
    fv = Vista(page, 40, 90)
    _titulo(page, 40, 80, "Frente, com a tampa", 8.5)
    fv.poli(contorno, (0.1, 0.1, 0.1), (0.86, 0.87, 0.89), 1.0)
    jx0 = (W_C - JANELA_W) / 2.0
    fv.rect(jx0, JANELA_Y_CAIXA, JANELA_W, JANELA_H, (0.1, 0.1, 0.1), (0.72, 0.75, 0.72), 0.8)
    fv.texto(jx0 + 6, JANELA_Y_CAIXA + JANELA_H / 2, "area visivel 36,28 x 59,8", 5.5, (0.25, 0.3, 0.25))
    for f in furos[1:]:
        fv.rect(f[0], f[1], f[2] - f[0], f[3] - f[1], (0.1, 0.1, 0.1), (0.4, 0.4, 0.42), 0.6)
    tk = teclas(pecas)
    if tk:
        fv.texto(PLACA_X0 + max(kx for kx, _ky in tk) + 4.5, PLACA_Y0 + tk[0][1] + 1.5,
                 "teclas SW601-603 da placa", 5.0, (0.2, 0.2, 0.2))
    # the concept's facet and buttons, dashed, to show the clash
    fv.rect(7.5, CONCEITO_FACETA[0], 47.0, CONCEITO_FACETA[1] - CONCEITO_FACETA[0],
            (0.75, 0.3, 0.1), None, 0.6, "[2 2] 0")
    fv.texto(9.0, CONCEITO_FACETA[0] + 4.5, "faceta solar do conceito", 5.0, (0.75, 0.3, 0.1))
    for bx in (16.0, 31.0, 46.0):
        fv.circulo(bx, CONCEITO_TECLAS_Y, 4.4, (0.75, 0.3, 0.1), None, 0.6)
    fv.texto(4.0, CONCEITO_TECLAS_Y + 8.0, "botoes do conceito (y 95)", 5.0, (0.75, 0.3, 0.1))
    # the USB of the board against the slot of the concept
    j101 = pecas.get("J101")
    if j101:
        fv.rect(PLACA_X0 + j101["caixa"][0], PLACA_Y0 + j101["caixa"][1],
                j101["caixa"][2] - j101["caixa"][0], j101["caixa"][3] - j101["caixa"][1],
                (0.1, 0.3, 0.7), None, 0.7)
        fv.texto(PLACA_X0 + j101["caixa"][2] + 1.0, PLACA_Y0 + j101["caixa"][3], "USB-C da placa", 5.0, (0.1, 0.3, 0.7))
    fv.rect(*CONCEITO_USB, cor=(0.75, 0.3, 0.1), fill=None, largura=0.6, tracejado="[2 2] 0")
    fv.cota_v(-4.0, 0, H_C, "104")
    fv.cota_h(H_C + 4.0, 0, W_C, "62")

    # ---- inside
    xv = Vista(page, 340, 90)
    _titulo(page, 340, 80, "Por dentro (tampa e display tirados)", 8.5)
    xv.poli(contorno, (0.1, 0.1, 0.1), (0.93, 0.94, 0.95), 1.0)
    xv.poli(contorno_arredondado(PAREDE, PAREDE, W_C - PAREDE, H_C - PAREDE, R_C - PAREDE),
            (0.5, 0.5, 0.5), None, 0.4)
    # the cell (behind the board) and the display outline (over it)
    cx0 = PLACA_X0 + PLACA_W / 2.0 - CELULA_W / 2.0
    xv.rect(cx0, PLACA_Y0 + SOMBRA_CELULA[0], CELULA_W, CELULA_H, (0.85, 0.5, 0.1),
            (1.0, 0.93, 0.8), 0.7, "[3 2] 0")
    xv.texto(cx0 + 1.0, PLACA_Y0 + SOMBRA_CELULA[1] - 1.5, "celula 36 x 60 x 7, atras", 5.0, (0.7, 0.4, 0.05))
    # the board
    rb = MD.RADIUS_DRAWING if hasattr(MD, "RADIUS_DRAWING") else 1.5
    xv.poli(contorno_arredondado(PLACA_X0, PLACA_Y0, PLACA_X0 + PLACA_W, PLACA_Y0 + PLACA_H, rb, 4),
            (0.1, 0.45, 0.15), (0.80, 0.90, 0.80), 0.9)
    for fx, fy in MD.FUROS_DOC:
        xv.circulo(PLACA_X0 + fx, PLACA_Y0 + fy, 1.1, (0.1, 0.45, 0.15), (1, 1, 1), 0.5)
    # every part: front filled, back dashed; the big ones named
    for ref, p in sorted(pecas.items()):
        x0, y0, x1, y1 = p["caixa"]
        w, h = x1 - x0, y1 - y0
        if p["atras"]:
            xv.rect(PLACA_X0 + x0, PLACA_Y0 + y0, w, h, (0.6, 0.2, 0.2), None, 0.4, "[1.5 1] 0")
        else:
            xv.rect(PLACA_X0 + x0, PLACA_Y0 + y0, w, h, (0.25, 0.25, 0.3), (0.62, 0.64, 0.68), 0.3)
        if w * h >= 14.0 or ref.startswith("SW") or ref in ("J101", "E301", "J102", "J103"):
            xv.texto(PLACA_X0 + x0 + 0.4, PLACA_Y0 + y0 + min(h, 2.6), ref, 4.0,
                     (0.5, 0.1, 0.1) if p["atras"] else (0.05, 0.05, 0.1))
    dx0 = PLACA_X0 + PLACA_W / 2.0 - DISPLAY_W / 2.0
    xv.rect(dx0, PLACA_Y0 + SOMBRA_DISPLAY[0], DISPLAY_W, DISPLAY_H, (0.2, 0.3, 0.6), None, 0.8, "[3 2] 0")
    xv.texto(dx0 + 1.0, PLACA_Y0 + SOMBRA_DISPLAY[0] - 1.0, "contorno do display 40,08 x 61,8", 5.0, (0.2, 0.3, 0.6))
    xv.cota_h(H_C + 4.0, PLACA_X0, PLACA_X0 + PLACA_W, "34")
    xv.cota_v(W_C + 3.0, PLACA_Y0, PLACA_Y0 + PLACA_H, "90")
    xv.cota_v(W_C + 9.0, 0, PLACA_Y0, f"{PLACA_Y0:.1f}".replace(".", ","))
    xv.texto(-2.0, H_C + 12.0, "cinza: frente; tracejado vermelho: verso", 5.0, (0.3, 0.3, 0.3))

    # ---- section along the length (seen from the right)
    sv = Vista(page, 660, 120)
    _titulo(page, 640, 80, "Corte pelo comprimento (visto da direita)", 8.5)
    page.insert_text(fitz.Point(640, 92), "eixo vertical: y da caixa; horizontal: z, do fundo para a tampa",
                     fontsize=6, fontname="helv", color=(0.35, 0.35, 0.35))
    # here x on the page is z (0..19 mm), y on the page is the case's y
    def zr(z0, y0, z1, y1, cor, fill, largura=0.5, tracejado=None):
        sv.rect(z0, y0, z1 - z0, y1 - y0, cor, fill, largura, tracejado)
    zr(0, 0, FUNDO, H_C, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))               # floor
    zr(0, 0, T_C, PAREDE, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))              # top wall
    zr(0, H_C - PAREDE, T_C, H_C, (0.1, 0.1, 0.1), (0.75, 0.76, 0.78))      # bottom wall
    zr(TAMPA_Z0, 0, T_C, H_C, (0.1, 0.1, 0.1), (0.80, 0.81, 0.83))         # lid
    zr(CELULA_Z0, PLACA_Y0 + SOMBRA_CELULA[0], CELULA_Z0 + CELULA_ESP,
       PLACA_Y0 + SOMBRA_CELULA[1], (0.85, 0.5, 0.1), (1.0, 0.93, 0.8))    # cell
    zr(PLACA_Z0, PLACA_Y0, PLACA_Z0 + PLACA_ESP, PLACA_Y0 + PLACA_H,
       (0.1, 0.45, 0.15), (0.55, 0.75, 0.55))                               # board
    zr(DISPLAY_Z0, PLACA_Y0 + SOMBRA_DISPLAY[0], DISPLAY_Z0 + DISPLAY_ESP,
       PLACA_Y0 + SOMBRA_DISPLAY[1], (0.2, 0.3, 0.6), (0.75, 0.8, 0.95))  # display
    for ref, p in pecas.items():
        y0, y1 = PLACA_Y0 + p["caixa"][1], PLACA_Y0 + p["caixa"][3]
        if p["atras"]:
            zr(PLACA_Z0 - p["altura"], y0, PLACA_Z0, y1, (0.6, 0.2, 0.2), (0.95, 0.8, 0.8), 0.3)
        else:
            zr(PLACA_Z0 + PLACA_ESP, y0, PLACA_Z0 + PLACA_ESP + p["altura"], y1,
               (0.25, 0.25, 0.3), (0.62, 0.64, 0.68), 0.3)
    sv.cota_v(T_C + 6.0, 0, H_C, "104")
    sv.cota_h(-4.0, 0, T_C, "19")
    sv.texto(T_C + 1.5, PLACA_Y0 + SOMBRA_DISPLAY[0] + 3, "display", 5.0, (0.2, 0.3, 0.6))
    sv.texto(T_C + 1.5, PLACA_Y0 + SOMBRA_CELULA[1] - 1, "celula", 5.0, (0.7, 0.4, 0.05))
    sv.texto(T_C + 1.5, PLACA_Y0 + PLACA_H, "placa", 5.0, (0.1, 0.45, 0.15))
    sv.texto(-2.0, H_C + 8.0, f"pilha: fundo {FUNDO:g} + celula {CELULA_ESP:g} + placa {PLACA_ESP:g} + vao {DISPLAY_VAO:g} + display {DISPLAY_ESP:g} = {DISPLAY_Z0 + DISPLAY_ESP:.1f} mm; tampa a {TAMPA_Z0:g}".replace(".", ","),
             5.0, (0.3, 0.3, 0.3))


def conflitos(pecas) -> list[str]:
    out = []
    tk = teclas(pecas)
    if tk:
        ky = PLACA_Y0 + tk[0][1]
        out.append(f"As teclas da placa (y = {tk[0][1]:g}) caem em y = {ky:.1f} da caixa, dentro da faceta solar do "
                   f"conceito (y {CONCEITO_FACETA[0]:g} a {CONCEITO_FACETA[1]:g}); os botoes do conceito estao em y = {CONCEITO_TECLAS_Y:g}. "
                   "A faceta e os botoes do conceito precisam mudar de lugar: a placa e que manda.")
    j101 = pecas.get("J101")
    if j101:
        fim = PLACA_Y0 + j101["caixa"][3]
        out.append(f"O USB-C da placa termina em y = {fim:.1f}; a parede de baixo da caixa comeca em y = {H_C - PAREDE:g}: "
                   f"faltam {H_C - PAREDE - fim:.1f} mm ate a boca do conector chegar a parede. Ou a placa desce, ou a parede entra.")
    out.append(f"A celula (36 mm) e o display (40,08 mm) sao mais largos que a placa (34): quem os segura e a caixa, nao a placa; "
               f"a celula sobra {(CELULA_W - PLACA_W) / 2:g} mm de cada lado e o display {(DISPLAY_W - PLACA_W) / 2:.2f} mm.")
    altos = []
    for ref, p in sorted(pecas.items()):
        y0, y1 = p["caixa"][1], p["caixa"][3]
        if p["atras"] and y1 > SOMBRA_CELULA[0] and y0 < SOMBRA_CELULA[1] and p["altura"] > CELULA_VAO:
            altos.append(f"{ref} {p['altura']:g} mm sob a celula (teto {CELULA_VAO:g})")
        if not p["atras"] and y1 > SOMBRA_DISPLAY[0] and y0 < SOMBRA_DISPLAY[1] and p["altura"] > DISPLAY_VAO:
            altos.append(f"{ref} {p['altura']:g} mm sob o display (teto {DISPLAY_VAO:g})")
    if altos:
        out.append("Pecas mais altas que o teto da sombra em que estao (a regra ME2 do dry run): " + "; ".join(altos) + ".")
    sobra = TAMPA_Z0 - (DISPLAY_Z0 + DISPLAY_ESP)
    out.append(f"Da face do display ate a tampa sobram {sobra:.1f} mm: a caixa de 19 mm tem folga, ou o display fica fundo demais atras da janela.")
    out.append("O receptor GNSS (U301) e a antena de chip (E301) ficam sob a borda de cima do display, como 04 ja registra; "
               "o conceito punha as antenas na parede de cima da caixa, fora da placa.")
    out.append("O conceito tem microSD, AEM10900 e modulo BM20C; a placa tem memoria soldada, ADP5091 e ME54BS13. O raio X do conceito "
               "(docs/img/placa-nova-caixa.svg) esta desatualizado e nao foi redesenhado aqui.")
    return out


def pagina_2(doc: fitz.Document, pecas) -> None:
    page = doc.new_page(width=842, height=595)
    _titulo(page, 30, 32, "Premissas e conflitos", 11)
    y = 56
    premissas = [
        f"Caixa: {W_C:g} x {H_C:g} x {T_C:g} mm, raio {R_C:g}, paredes {PAREDE:g}, fundo {FUNDO:g}, tampa {TAMPA:g} (conceito de tools/docs/case_drawing.py e docs/14).",
        f"Placa: {PLACA_W:g} x {PLACA_H:g} x {PLACA_ESP:g} mm, gnssbike.kicad_pcb de hoje, com o courtyard, a altura e a face de cada peca (footprints.ALTURA).",
        f"Onde a placa fica: centrada na largura (x de {PLACA_X0:g} a {PLACA_X0 + PLACA_W:g}) e com o contorno do display sobre o do conceito, "
        f"o que poe a placa em y de {PLACA_Y0:.1f} a {PLACA_Y0 + PLACA_H:.1f} (o display cobre y {SOMBRA_DISPLAY[0]:g} a {SOMBRA_DISPLAY[1]:g} da placa: make_dxf.ZONES).",
        f"Pilha, do fundo para a tampa: celula {CELULA_ESP:g} mm sobre o fundo, placa sobre a celula, display {DISPLAY_VAO:g} mm sobre a frente da placa "
        f"(o teto da sombra), tampa de {TAMPA:g} mm; o display fica a z {DISPLAY_Z0:.1f}-{DISPLAY_Z0 + DISPLAY_ESP:.1f}, a tampa a {TAMPA_Z0:g}-{T_C:g}.",
        "Os seis modulos solares, o engate de quarto de volta e o respiro nao estao desenhados: ficam na caixa, e nenhum documento os poe nas coordenadas da placa.",
        "Nada disto e decisao: e o desenho do que existe hoje, para decidir em cima dele.",
    ]
    for s in premissas:
        y = _paragrafo(page, 30, y, s, 7.5)
        y += 4
    y += 8
    _titulo(page, 30, y, "O que nao bate", 9.5)
    y += 16
    for i, s in enumerate(conflitos(pecas), 1):
        y = _paragrafo(page, 30, y, f"{i}. {s}", 7.5)
        y += 4


def _paragrafo(page: fitz.Page, x: float, y: float, s: str, tam: float, largura: float = 780.0) -> float:
    palavras = s.split()
    linha = ""
    for w in palavras:
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


def main() -> int:
    pecas = ler_placa()
    print(f"placa {PLACA_W:g} x {PLACA_H:g} em x {PLACA_X0:g}-{PLACA_X0 + PLACA_W:g}, "
          f"y {PLACA_Y0:.1f}-{PLACA_Y0 + PLACA_H:.1f} da caixa; z: celula {CELULA_Z0:g}-{CELULA_Z0 + CELULA_ESP:g}, "
          f"placa {PLACA_Z0:g}-{PLACA_Z0 + PLACA_ESP:g}, display {DISPLAY_Z0:.1f}-{DISPLAY_Z0 + DISPLAY_ESP:.1f}, "
          f"tampa {TAMPA_Z0:g}-{T_C:g}; {len(pecas)} pecas")
    doc = fitz.open()
    pagina_1(doc, pecas)
    pagina_2(doc, pecas)
    doc.save(HERE / "gnssbike-caixa.pdf", garbage=3, deflate=True)
    print("  gnssbike-caixa.pdf: 2 paginas")
    for s in conflitos(pecas):
        print("  - " + s)

    # the open shell, seen from the bottom edge (the USB-C end) so that the
    # keys and the port are nearest; with the lid floating over it, the lid
    # hid the board
    t, c = cena_3d(pecas, tampa_z=None)
    img = M3.render(t, c, 1600, 1500, 200.0, 40.0)
    img.save(HERE / "gnssbike-3d-caixa-aberta.png")
    print(f"  gnssbike-3d-caixa-aberta.png: {len(t)} triangulos")
    t, c = cena_3d(pecas, tampa_z=TAMPA_Z0)
    img = M3.render(t, c, 1100, 1800, 0.0, 90.0)
    img.save(HERE / "gnssbike-3d-caixa-frente.png")
    print("  gnssbike-3d-caixa-frente.png")
    return 0


if __name__ == "__main__":
    sys.exit(main())
