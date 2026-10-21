"""Draw the assembly manual of the device: every part to scale, and the order.

The owner asked for a manual - case, panels, wires with connectors, the board
in the case, the display with its connectors, everything in order. This draws
it the way the other drawings here are drawn: an SVG in millimetres, generated
by program, so that when a connector moves on the board the sheet moves with
it.

Every part is at TRUE SIZE at its panel's own scale, and every dimension is
read, not typed, from the files that define it:

  * the case, its cavity, bevels, facet, module pockets, screws and the
    stack of display, board and cell      hardware_gnssbike/caixa/make_caixa.py
                                          (its Caixa, computed from the board)
  * the board, every connector's position, size and face, the keys, the
    holes                                 cad/gnssbike.kicad_pcb
  * the parts each build leaves off       cad/make_fab.py
  * the JDI LPM027M128C, outline 40.08 x 61.8 and active area 35.28 x 58.8
                                          hardware_gnssbike/04 (the spec)

Until 2026-10-20 the numbers were copied by hand, and the sheet kept the case
and the connectors of 2026-09-26 for a month after both had moved.

What no file defines is said so on the sheet, in red, not guessed.

Usage: python tools/docs/assembly_manual.py [output.svg]
       (default: docs/img/manual-de-montagem.svg)
"""
import math
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1
       else ROOT / "docs" / "img" / "manual-de-montagem.svg")
sys.path.insert(0, str(ROOT / "hardware_gnssbike" / "caixa"))
sys.path.insert(0, str(ROOT / "hardware_gnssbike" / "cad"))

import fp_load  # noqa: E402
import make_caixa as CX  # noqa: E402
import make_fab as MF  # noqa: E402
import parts as P  # noqa: E402

CANVAS_W, CANVAS_H = 2480, 4620
FONT = "Inter, 'Segoe UI', Roboto, Arial, sans-serif"

PECAS = CX.ler_placa()               # board frame: x right, y down, mm
CAIXA = CX.Caixa(PECAS)              # case frame: the same axes, case corner
FORA = MF.SEM_PECA | MF.VARIANTES["jdi"]["sem_peca"]


def _componentes() -> int:
    """What the JDI build mounts, as its fabrication BOM counts it: the
    board's parts, less the ones the build leaves off and the pads that are
    no part at all (test points, the Tag-Connect: exclude_from_bom)."""
    arv = fp_load.parse((ROOT / "hardware_gnssbike" / "cad" / "gnssbike.kicad_pcb")
                        .read_text(encoding="utf-8"))
    n = 0
    for fp in fp_load.kids(arv, "footprint"):
        ref = next((pr[2] for pr in fp_load.kids(fp, "property") if pr[1] == "Reference"), "")
        if ref in P.PARTS and ref not in FORA and "exclude_from_bom" not in (fp_load.kid(fp, "attr") or []):
            n += 1
    return n


N_PECAS = _componentes()

CAIXA_W, CAIXA_H, CAIXA_T, CAIXA_R = CX.W_C, CX.H_C, CX.T_C, CX.R_C
CAV_W, CAV_H = CX.W_C - 2 * CX.PAREDE, CX.H_C - 2 * CX.PAREDE
PCB_W, PCB_H, PCB_T = CX.PLACA_W, CX.PLACA_H, CX.PLACA_ESP
PX0, PY0 = CX.PLACA_X0, CX.PLACA_Y0  # the board's corner in the case
TELA_W, TELA_H = 40.08, 61.8
ATIVA_W, ATIVA_H = 35.28, 58.8
TELA_Y0 = CX.MD.DISPLAY_Y1 - TELA_H  # the JDI's top edge on the board
BAT_W, BAT_H, BAT_T = CX.CELULA_W, CX.CELULA_H, CX.CELULA_ESP
MOD_W, MOD_H = CX.MODULO_W, CX.MODULO_H
FUROS = CX.MD.FUROS_DOC
# heights measured down from the lid's outer face, as the sections show them
Z_DISP0, Z_DISP1 = CAIXA_T - CX.DISPLAY_Z1, CAIXA_T - CX.DISPLAY_Z0
Z_PLACA0, Z_PLACA1 = CAIXA_T - CX.PLACA_Z1, CAIXA_T - CX.PLACA_Z0
Z_CEL0, Z_CEL1 = CAIXA_T - CX.CELULA_Z1, CAIXA_T - CX.CELULA_Z0

# the connectors the manual names, and what each takes
CONEC = {
    "E301": "antena GNSS",
    "J402": "luz da JDI · 5 vias",
    "J401": "sinal das duas telas · 10 vias",
    "J403": "luz do Azumo · 4 vias",
    "J201": "gravação · Tag-Connect",
    "J103": "painel solar · 4 vias",
    "J102": "bateria · 6 vias",
    "J101": "USB-C",
}
CORPOS = ("U201", "U301")             # drawn for orientation, unnamed

C = {
    "papel": "#ffffff", "campo": "#f5f6f8", "borda": "#d7dae0",
    "texto": "#3a3f47", "fraco": "#6b7178", "titulo": "#111111",
    "placa": "#2f6b3a", "placa_borda": "#1f4a28", "cobre": "#c8a12a",
    "caixa": "#eceef1", "caixa_borda": "#aab0b8", "caixa_fundo": "#e0e3e8",
    "tela": "#cdd2d8", "tela_ativa": "#e8ebee",
    "bateria": "#8e99a6", "modulo": "#12161d",
    "fpc": "#b5893f", "fio_p": "#cc2b2b", "fio_n": "#1a1a1a",
    "alerta": "#cc2b2b", "ok": "#2e7d32",
}

out = []


def add(s):
    out.append(s)


def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


def br(v, casas=1):
    """A number as the sheet writes it: decimal comma, no trailing zero."""
    return f"{v:.{casas}f}".rstrip("0").rstrip(".").replace(".", ",")


class View:
    def __init__(self, x0, y0, s):
        self.x0, self.y0, self.s = x0, y0, s

    def X(self, x):
        return self.x0 + x * self.s

    def Y(self, y):
        return self.y0 + y * self.s

    def rect(self, x, y, w, h, r=0.0, **kw):
        a = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<rect x="{f(self.X(x))}" y="{f(self.Y(y))}" width="{f(w * self.s)}" '
            f'height="{f(h * self.s)}" rx="{f(r * self.s)}" ry="{f(r * self.s)}" {a}/>')

    def circle(self, cx, cy, r, **kw):
        a = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<circle cx="{f(self.X(cx))}" cy="{f(self.Y(cy))}" r="{f(r * self.s)}" {a}/>')

    def poly(self, pts, **kw):
        a = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        d = " ".join(f"{f(self.X(x))},{f(self.Y(y))}" for x, y in pts)
        add(f'<polyline points="{d}" {a}/>')

    def text(self, x, y, s, px, fill=C["texto"], anchor="start", weight=400):
        add(f'<text x="{f(self.X(x))}" y="{f(self.Y(y))}" font-family="{FONT}" '
            f'font-size="{f(px)}" fill="{fill}" text-anchor="{anchor}" '
            f'font-weight="{weight}">{s}</text>')


def rotulo(x, y, s, px=17, fill=C["texto"], anchor="start", weight=400):
    add(f'<text x="{f(x)}" y="{f(y)}" font-family="{FONT}" font-size="{f(px)}" '
        f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>')


def moldura(x, y, w, h, n, titulo, sub=""):
    add(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="{f(h)}" rx="14" '
        f'fill="{C["campo"]}" stroke="{C["borda"]}" stroke-width="1.5"/>')
    if n:
        add(f'<circle cx="{f(x + 44)}" cy="{f(y + 42)}" r="21" fill="{C["titulo"]}"/>')
        rotulo(x + 44, y + 50, str(n), 23, "#ffffff", "middle", 700)
        rotulo(x + 78, y + 50, titulo, 23, C["titulo"], weight=700)
    else:
        rotulo(x + 24, y + 48, titulo, 23, C["titulo"], weight=700)
    if sub:
        rotulo(x + (78 if n else 24), y + 76, sub, 16, C["fraco"])


def onde(ref):
    """'(x; y)' of a part on the board, as the steps quote it."""
    p = PECAS[ref]
    return f"({br(p['x'], 2)}; {br(p['y'], 2)})"


# ------------------------------------------------------------------ peças
def desenha_placa(v, x, y, verso=False, destaque=()):
    """The board, seen from the front or mirrored for the back: every
    connector at its true size and place, from the board file."""
    def mx(px):
        return (PCB_W - px) if verso else px

    v.rect(x, y, PCB_W, PCB_H, 2.0, fill=C["placa"], stroke=C["placa_borda"],
           stroke_width=0.4)
    for fx, fy in FUROS:
        v.circle(x + mx(fx), y + fy, 1.1, fill=C["campo"],
                 stroke=C["placa_borda"], stroke_width=0.3)
    for ref in CORPOS + ("LS601",):
        p = PECAS.get(ref)
        if not p or p["atras"] != verso:
            continue
        x0, y0, x1, y1 = p["caixa"]
        a, b = sorted((mx(x0), mx(x1)))
        v.rect(x + a, y + y0, b - a, y1 - y0, 0.6, fill="#c9ced4", fill_opacity=0.55,
               stroke="#9aa0a8", stroke_width=0.3)
    for ref in CONEC:
        p = PECAS[ref]
        if p["atras"] != verso:
            continue
        x0, y0, x1, y1 = p["caixa"]
        a, b = sorted((mx(x0), mx(x1)))
        forte = ref in destaque
        sem_peca = ref in FORA
        cor = "#ffd24d" if forte else ("none" if sem_peca else "#e9e4d7")
        v.rect(x + a, y + y0, b - a, y1 - y0, 0.4, fill=cor,
               stroke="#8d887c", stroke_width=0.3,
               stroke_dasharray="0.8 0.6" if sem_peca and not forte else "none")
        if forte:
            # the label goes BESIDE the part, never over it: J402 and J401
            # are 14,7 mm apart, and a label above one falls on the other
            meio = (a + b) / 2
            if meio < PCB_W / 2:
                v.text(x + b + 1.2, y + (y0 + y1) / 2 + 1.2, ref, 15, "#f6f9f6", "start", 700)
            else:
                v.text(x + a - 1.2, y + (y0 + y1) / 2 + 1.2, ref, 15, "#f6f9f6", "end", 700)
    if not verso:
        for ref in ("SW601", "SW602", "SW603"):
            p = PECAS[ref]
            v.circle(x + mx(p["x"]), y + p["y"], 1.8, fill="#d8dce1",
                     stroke="#9aa0a8", stroke_width=0.3)


def desenha_tela(v, x, y, com_fpc=True):
    """The JDI, with its two tails where the board's J402 and J401 take
    them: both leave the same long edge."""
    v.rect(x, y, TELA_W, TELA_H, 1.0, fill=C["tela"], stroke="#9aa0a8",
           stroke_width=0.4)
    v.rect(x + (TELA_W - ATIVA_W) / 2, y + 1.5, ATIVA_W, ATIVA_H, 0.4,
           fill=C["tela_ativa"], stroke="#b6bcc4", stroke_width=0.3)
    if com_fpc:
        for ref, larg, txt in (("J402", 3.5, "5 vias · luz"), ("J401", 6.0, "10 vias · sinal")):
            yy = PECAS[ref]["y"] - TELA_Y0
            v.rect(x - 14.0, y + yy - larg / 2, 14.0, larg, 0.4, fill=C["fpc"],
                   stroke="#8a6a2e", stroke_width=0.3)
            v.text(x - 15.0, y + yy + 1.2, txt, 15, C["texto"], "end", 700)


def modulos_no_plano():
    """The six modules in plan, in the case frame: two in the facet's
    pockets, two along each bevel, lying across its 45-degree face."""
    rects = []
    for b in CAIXA.bolsos_faceta:
        cxm, cym = (b[0] + b[2]) / 2, (b[1] + b[3]) / 2
        rects.append((cxm - MOD_W / 2, cym - MOD_H / 2, MOD_W, MOD_H))
    meia = MOD_H / 2 / math.sqrt(2)          # the module's width, seen from above
    for lado, ya, yb in CAIXA.modulos_chanfro:
        xm = CX.CHANFRO / 2 if lado == "L" else CAIXA_W - CX.CHANFRO / 2
        ym = (ya + yb) / 2
        rects.append((xm - meia, ym - MOD_W / 2, 2 * meia, MOD_W))
    return rects


def desenha_caixa(v, x, y, com_modulos=True):
    """The case from the front: outline, the bevels' bands, the facet, the
    window, the key holes and the lid's screws, from make_caixa's Caixa."""
    v.rect(x, y, CAIXA_W, CAIXA_H, CAIXA_R, fill=C["caixa"],
           stroke=C["caixa_borda"], stroke_width=0.5)
    y0c, y1c = CX.CHANFRO_Y
    for bx in (0.0, CAIXA_W - CX.CHANFRO):
        v.rect(x + bx, y + y0c, CX.CHANFRO, y1c - y0c, 0.0, fill=C["caixa_fundo"],
               stroke="#b6bcc4", stroke_width=0.35)
    fx0, fy0, fx1, fy1 = CAIXA.faceta
    v.rect(x + fx0, y + fy0, fx1 - fx0, fy1 - fy0, 1.6, fill=C["caixa_fundo"],
           stroke="#b6bcc4", stroke_width=0.35)
    jx0, jy0, jx1, jy1 = CAIXA.janela
    v.rect(x + jx0, y + jy0, jx1 - jx0, jy1 - jy0, 0.6, fill="none",
           stroke="#b6bcc4", stroke_width=0.35)
    for qx0, qy0, qx1, qy1 in CAIXA.furos_teclas:
        v.rect(x + qx0, y + qy0, qx1 - qx0, qy1 - qy0, 0.5, fill="#ffffff",
               stroke="#9aa0a8", stroke_width=0.3)
    for sx, sy in CX.PARAFUSO_TAMPA:
        v.circle(x + sx, y + sy, 1.1, fill="#ffffff", stroke="#9aa0a8", stroke_width=0.3)
    if com_modulos:
        for mx0, my0, mw, mh in modulos_no_plano():
            v.rect(x + mx0, y + my0, mw, mh, 0.4, fill=C["modulo"],
                   stroke=C["fio_p"], stroke_width=0.6)


def cota_h(v, y, x1, x2, txt, px=15):
    v.poly([(x1, y - 1.2), (x1, y + 1.2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x2, y - 1.2), (x2, y + 1.2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x1, y), (x2, y)], fill="none", stroke=C["fraco"], stroke_width=0.25)
    v.text((x1 + x2) / 2, y - 1.8, txt, px, C["fraco"], "middle")


def cota_v(v, x, y1, y2, txt, px=15, lado=1):
    """A vertical dimension, its text right of the line (lado 1) or left (-1)."""
    v.poly([(x - 1.2, y1), (x + 1.2, y1)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x - 1.2, y2), (x + 1.2, y2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x, y1), (x, y2)], fill="none", stroke=C["fraco"], stroke_width=0.25)
    v.text(x + 1.6 * lado, (y1 + y2) / 2, txt, px, C["fraco"], "start" if lado > 0 else "end")


add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS_W}" '
    f'height="{CANVAS_H}" viewBox="0 0 {CANVAS_W} {CANVAS_H}">')
add(f'<rect width="{CANVAS_W}" height="{CANVAS_H}" fill="{C["papel"]}"/>')

rotulo(40, 58, "Manual de montagem", 36, C["titulo"], weight=700)
rotulo(40, 92, "GNSS Bike Computer · placa gnssbike, montagem da JDI · todas as peças "
               "em escala, medidas em mm", 18, C["fraco"])
add(f'<rect x="1380" y="28" width="1060" height="78" rx="10" fill="#fdf3f3" '
    f'stroke="{C["alerta"]}" stroke-width="1.5"/>')
rotulo(1404, 58, "Nada disto foi montado, medido ou fabricado.", 18, C["alerta"],
       weight=700)
rotulo(1404, 84, "A caixa é proposta e a placa é arquivo de CAD. "
                 "O que ainda não está decidido está em vermelho.", 16, C["texto"])

# ========================================================== as peças
PX, PY, PW, PH = 40, 130, 2400, 680
moldura(PX, PY, PW, PH, 0, "As peças",
        "tudo no mesmo aumento, 4,2 px/mm · confira antes de começar")

SP = 4.2
pv = View(PX + 60, PY + 150, SP)
desenha_caixa(pv, 0, 0, com_modulos=False)
pv.text(CAIXA_W / 2, CAIXA_H + 7, f"caixa · {br(CAIXA_W)} × {br(CAIXA_H)} × {br(CAIXA_T)}",
        16, C["texto"], "middle", 700)
pv.text(CAIXA_W / 2, CAIXA_H + 12, "concha, tampa, 3 teclas, membrana,",
        15, C["fraco"], "middle")
pv.text(CAIXA_W / 2, CAIXA_H + 16.5, "5 coberturas e a porta do USB-C",
        15, C["fraco"], "middle")

pv2 = View(PX + 420, PY + 150, SP)
desenha_placa(pv2, 0, 0)
pv2.text(PCB_W / 2, PCB_H + 7, f"placa · {br(PCB_W)} × {br(PCB_H)} × {br(PCB_T)}",
         16, C["texto"], "middle", 700)
pv2.text(PCB_W / 2, PCB_H + 12, f"1 peça, {N_PECAS} componentes", 15, C["fraco"], "middle")

pv3 = View(PX + 700, PY + 160, SP)
desenha_tela(pv3, 16.0, 0)
pv3.text(16 + TELA_W / 2, TELA_H + 7, "display JDI LPM027M128C", 16, C["texto"],
         "middle", 700)
pv3.text(16 + TELA_W / 2, TELA_H + 12, f"{br(TELA_W, 2)} × {br(TELA_H)} × "
         f"{br(CX.DISPLAY_ESP)} · 1 peça", 15, C["fraco"], "middle")

pv4 = View(PX + 1150, PY + 190, SP)
pv4.rect(0, 0, BAT_W, BAT_H, 2.0, fill=C["bateria"], stroke="#6f7a86", stroke_width=0.4)
pv4.rect(BAT_W / 2 - 5, -3.0, 10, 3.0, 0.4, fill="#b9c2cc", stroke="#6f7a86",
         stroke_width=0.3)
pv4.text(BAT_W / 2, BAT_H + 7, f"célula LiPo · {br(BAT_W)} × {br(BAT_H)} × {br(BAT_T)}",
         16, C["texto"], "middle", 700)
pv4.text(BAT_W / 2, BAT_H + 12, "2000 mAh, PCM e NTC · 1 peça", 15, C["fraco"], "middle")

pv5 = View(PX + 1440, PY + 190, SP)
for i in range(6):
    col, lin = i % 2, i // 2
    pv5.rect(col * 27.0, lin * 12.0, MOD_W, MOD_H, 0.5, fill=C["modulo"],
             stroke=C["fio_p"], stroke_width=0.6)
pv5.text(24.0, 12 * 3 + 5, f"módulos solares · {br(MOD_W)} × {br(MOD_H)}", 16,
         C["texto"], "middle", 700)
pv5.text(24.0, 12 * 3 + 10, "KXOB25-05X3F · 6 peças", 15, C["fraco"], "middle")

pv6 = View(PX + 1850, PY + 190, SP)
pv6.rect(0, 0, 5.8, 7.2, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.3)
pv6.text(2.9, 12.0, "ZHR-4 + 4 × SZH-002T-P0.5", 16, C["texto"], "middle", 700)
pv6.text(2.9, 17.0, "carcaça do chicote solar", 15, C["fraco"], "middle")
pv6.rect(0, 26.0, 6.0, 4.2, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.3)
pv6.text(3.0, 37.0, "SHR-06V-S-B + 6 × SSH-003T-P0.2", 16, C["texto"], "middle", 700)
pv6.text(3.0, 42.0, "cabo da bateria, se o pack vier sem", 15, C["fraco"], "middle")
pv6.text(3.0, 53.0, "fio AWG 28, cerca de 1 m", 16, C["texto"], "middle", 700)
pv6.text(3.0, 61.0, "parafusos M2 · 2 da placa e 4 da tampa", 16, C["texto"],
         "middle", 700)

# =================================================== o aparelho montado
MX, MY, MW, MH = 40, 840, 2400, 820
moldura(MX, MY, MW, MH, 0, "O aparelho montado",
        "a caixa com a placa dentro e os seis módulos na tampa · "
        f"dois cortes, para ver os {br(CAIXA_T)} mm de espessura")

# --- vista de frente, com a placa e o display por transparencia
SM = 5.2
mv = View(MX + 250, MY + 140, SM)
desenha_caixa(mv, 0, 0)
dx0, dy0, dx1, dy1 = CAIXA.display
mv.rect(dx0, dy0, dx1 - dx0, dy1 - dy0, 1.0, fill="#ffffff", fill_opacity=0.5,
        stroke="#8f959d", stroke_width=0.4)
mv.text((dx0 + dx1) / 2, dy0 + 14.0, "display", 16, "#7d838b", "middle")
mv.rect(PX0, PY0, PCB_W, PCB_H, 2.0, fill=C["placa"], fill_opacity=0.45,
        stroke=C["placa_borda"], stroke_width=0.5, stroke_dasharray="1.6 1.2")
for ref in CONEC:
    x0, y0, x1, y1 = PECAS[ref]["caixa"]
    mv.rect(PX0 + x0, PY0 + y0, x1 - x0, y1 - y0, 0.3, fill="#f3e9c9",
            stroke="#8d887c", stroke_width=0.3,
            stroke_dasharray="0.8 0.6" if ref in FORA else "none")
# os rotulos ficam FORA da caixa, com linha de chamada: dentro eles caem em
# cima dos modulos dos chanfros
CHAMADAS = (("E301", -1, 12.0, "E301 antena"), ("J402", -1, 26.0, "J402 luz da JDI"),
            ("J401", -1, 40.0, "J401 sinal da tela"), ("J103", -1, 80.0, "J103 painel (verso)"),
            ("J102", -1, 91.0, "J102 bateria"), ("J101", -1, 102.0, "J101 USB-C"),
            ("J201", +1, 52.0, "J201 gravação"), ("J403", +1, 66.0, "J403 luz Azumo"))
for ref, lado, ly, txt in CHAMADAS:
    x0, y0, x1, y1 = PECAS[ref]["caixa"]
    ax, ay = PX0 + (x0 + x1) / 2, PY0 + (y0 + y1) / 2
    bx = -3.0 if lado < 0 else CAIXA_W + 3.0
    mv.poly([(ax, ay), (bx, ly)], fill="none", stroke="#7d848c", stroke_width=0.22)
    mv.circle(ax, ay, 0.6, fill="#5b6068")
    mv.text(bx + (-1.4 if lado < 0 else 1.4), ly + 1.2, txt, 15, "#2b2b2b",
            "end" if lado < 0 else "start", 700)
mv.text(CAIXA_W / 2, CAIXA_H + 11.0, f"a placa por transparência, {br(PCB_W)} × {br(PCB_H)}",
        16, C["fraco"], "middle")
mv.text(CAIXA_W / 2, CAIXA_H + 16.0, f"na cavidade de {br(CAV_W)} × {br(CAV_H)}; "
        "tracejado, o que a JDI não leva", 16, C["fraco"], "middle")
# where the two sections cut: A-A across the lower bevel modules, B-B along
# the left facet pocket, the board, the cell and the USB-C
_lado, YA0, YA1 = CAIXA.modulos_chanfro[1]
Y_A = (YA0 + YA1) / 2
X_B = (CAIXA.bolsos_faceta[0][0] + CAIXA.bolsos_faceta[0][2]) / 2
mv.poly([(-4.0, Y_A), (CAIXA_W + 4.0, Y_A)], fill="none", stroke=C["alerta"],
        stroke_width=0.3, stroke_dasharray="2 1.5")
mv.text(-5.0, Y_A + 1.4, "A", 17, C["alerta"], "end", 700)
mv.text(CAIXA_W + 5.0, Y_A + 1.4, "A", 17, C["alerta"], "start", 700)
mv.poly([(X_B, -4.0), (X_B, CAIXA_H + 4.0)], fill="none", stroke=C["alerta"],
        stroke_width=0.3, stroke_dasharray="2 1.5")
mv.text(X_B, -5.4, "B", 17, C["alerta"], "middle", 700)
mv.text(X_B, CAIXA_H + 7.5, "B", 17, C["alerta"], "middle", 700)

# --- corte A-A: pela largura, mostra os chanfros com os modulos e a pilha
SS = 8.4
av = View(MX + 780, MY + 175, SS)
av.text(0, -8.0, "Corte A-A · pela largura", 19, C["titulo"], "start", 700)
av.text(0, -3.0, "os dois chanfros de 45°, com um módulo em cada", 15, C["fraco"])
CH, TT, W = CX.CHANFRO, CAIXA_T, CAIXA_W
av.poly([(CH, 0), (W - CH, 0), (W, CH), (W, TT), (0, TT), (0, CH), (CH, 0)],
        fill=C["caixa"], stroke=C["caixa_borda"], stroke_width=0.35)
for x1, y1, x2, y2 in ((0.0, CH, CH, 0.0), (W - CH, 0.0, W, CH)):
    av.poly([(x1, y1), (x2, y2)], fill="none", stroke=C["caixa_borda"],
            stroke_width=0.45)
# os modulos, deitados no chanfro e centrados na face de 45 graus
d = CH / 2 - MOD_H / 2 / math.sqrt(2)
e = CH / 2 + MOD_H / 2 / math.sqrt(2)
av.poly([(d, CH - d), (e, CH - e)], fill="none", stroke=C["modulo"],
        stroke_width=1.6, stroke_linecap="round")
av.poly([(W - e, CH - e), (W - d, CH - d)], fill="none", stroke=C["modulo"],
        stroke_width=1.6, stroke_linecap="round")
av.text(3.0, CH + 3.2, "PV104", 15, C["fio_p"], "middle", 700)
av.text(W - 3.0, CH + 3.2, "PV106", 15, C["fio_p"], "middle", 700)
cx0, _cy0, cx1, _cy1 = CAIXA.celula
av.rect(dx0, Z_DISP0, dx1 - dx0, Z_DISP1 - Z_DISP0, 0.2, fill=C["tela"], stroke="#8f959d",
        stroke_width=0.25)
av.rect(PX0, Z_PLACA0, PCB_W, PCB_T, 0.15, fill=C["placa"], stroke=C["placa_borda"],
        stroke_width=0.25)
av.rect(cx0, Z_CEL0, cx1 - cx0, BAT_T, 0.4, fill=C["bateria"], stroke="#6f7a86",
        stroke_width=0.25)
# the levels are a millimetre or two apart: the names go in a column of
# their own, each with a short line to its level
for i, (txt, z, forte) in enumerate((
        (f"display · {br(CX.DISPLAY_ESP)}", (Z_DISP0 + Z_DISP1) / 2, True),
        (f"{br(Z_PLACA0 - Z_DISP1)} mm de teto", (Z_DISP1 + Z_PLACA0) / 2, False),
        (f"placa · {br(PCB_T)}", (Z_PLACA0 + Z_PLACA1) / 2, True),
        (f"{br(Z_CEL0 - Z_PLACA1)} mm até a célula", (Z_PLACA1 + Z_CEL0) / 2, False),
        (f"célula · {br(BAT_T)}", (Z_CEL0 + Z_CEL1) / 2, True))):
    zl = 1.5 + i * 3.6
    av.poly([(W + 0.4, z), (W + 2.2, zl)], fill="none", stroke="#9aa0a8", stroke_width=0.2)
    av.text(W + 2.8, zl + 0.6, txt, 16 if forte else 14, C["texto"] if forte else C["fraco"],
            "start", 700 if forte else 400)
cota_v(av, -2.6, 0, TT, br(TT), lado=-1)
cota_h(av, TT + 2.5, 0, W, br(W))

# --- corte B-B: pelo comprimento, mostra a faceta, a pilha e o USB-C
bv2 = View(MX + 780, MY + 480, SS)
bv2.text(0, -8.0, "Corte B-B · pelo comprimento", 19, C["titulo"], "start", 700)
bv2.text(0, -4.0, "a faceta com um módulo, a pilha inteira e o USB-C", 15, C["fraco"])
L = CAIXA_H
bv2.rect(0, 0, L, TT, 0.0, fill=C["caixa"], stroke=C["caixa_borda"], stroke_width=0.35)
fx0, fy0, fx1, fy1 = CAIXA.faceta
bv2.rect(fy0, -CX.FACETA_ALT, fy1 - fy0, CX.FACETA_ALT, 0.0, fill=C["caixa"],
         stroke=C["caixa_borda"], stroke_width=0.35)
b0 = CAIXA.bolsos_faceta[0]
bv2.poly([((b0[1] + b0[3]) / 2 - MOD_H / 2, -0.4), ((b0[1] + b0[3]) / 2 + MOD_H / 2, -0.4)],
         fill="none", stroke=C["modulo"], stroke_width=1.6, stroke_linecap="round")
bv2.text((b0[1] + b0[3]) / 2, -CX.FACETA_ALT - 1.0, "PV101", 15, C["fio_p"], "middle", 700)
bv2.rect(dy0, Z_DISP0, dy1 - dy0, Z_DISP1 - Z_DISP0, 0.2, fill=C["tela"], stroke="#8f959d",
         stroke_width=0.25)
bv2.rect(PY0, Z_PLACA0, PCB_H, PCB_T, 0.15, fill=C["placa"], stroke=C["placa_borda"],
         stroke_width=0.25)
_cx0, cy0, _cx1, cy1 = CAIXA.celula
bv2.rect(cy0, Z_CEL0, cy1 - cy0, BAT_T, 0.4, fill=C["bateria"], stroke="#6f7a86",
         stroke_width=0.25)
_ux0, _ux1, uz0, uz1 = CAIXA.usb
j101 = PECAS["J101"]
alt_usb = j101["altura"]
bv2.rect(PY0 + j101["caixa"][1], Z_PLACA0 - alt_usb, j101["caixa"][3] - j101["caixa"][1], alt_usb,
         0.3, fill="#c3c8ce", stroke="#8f959d", stroke_width=0.25)
bv2.rect(L - CX.PAREDE, TT - uz1, CX.PAREDE, uz1 - uz0, 0.0, fill="#ffffff",
         stroke=C["fio_p"], stroke_width=0.3)
bv2.text((dy0 + dy1) / 2, Z_DISP0 + 1.7, "display", 15, C["texto"], "middle", 700)
bv2.text((dy0 + dy1) / 2 + 12.0, Z_PLACA0 - 0.6, f"placa · {br(PCB_H)}", 15, C["texto"], "middle", 700)
bv2.text((cy0 + cy1) / 2, Z_CEL0 + 4.4, f"célula · {br(BAT_H)}", 15, "#f2f5f7", "middle", 700)
bv2.text(L - 1.0, TT + 2.6, "USB-C", 15, C["fio_p"], "end", 700)
cota_h(bv2, TT + 5.5, 0, L, br(L))

rotulo(MX + 1720, MY + 150, "O que estes cortes mostram", 20, C["titulo"], weight=700)
for i, s in enumerate((
        "Os seis módulos ficam na TAMPA, não na placa: dois na",
        "faceta abaixo das teclas e dois em cada chanfro de 45°.",
        "Cada face olha para um lado, e por isso eles chegam à",
        "placa por chicote.",
        "",
        "A placa fica no meio da espessura, com o display colado",
        "sob a tampa e a célula atrás. Daí os dois tetos que o dry",
        f"run mede: {br(Z_PLACA0 - Z_DISP1)} mm sob o display e "
        f"{br(CX.CELULA_VAO)} mm sob a placa, para",
        "as peças do verso sobre a célula.",
        "",
        "!O que ainda não está decidido: a vedação do USB-C, a",
        "!dos furos do fundo e a da linha de partição entre a",
        "!concha e a tampa (hardware_gnssbike/12).")):
    cor = C["alerta"] if s.startswith("!") else C["texto"]
    peso = 700 if s.startswith("!") else 400
    rotulo(MX + 1720, MY + 190 + i * 26, s.lstrip("!"), 16, cor, weight=peso)

# ========================================================== os oito passos
PASSO_W, PASSO_H = 1180, 680
COL = (40, 1260)
LIN = (1700, 2410, 3120, 3830)


def passo(n, col, lin, titulo, sub):
    x, y = COL[col], LIN[lin]
    moldura(x, y, PASSO_W, PASSO_H, n, titulo, sub)
    return x, y


def linhas(x, y, txts, px=17):
    for i, s in enumerate(txts):
        cor = C["alerta"] if s.startswith("!") else C["texto"]
        peso = 700 if s.startswith("!") else 400
        rotulo(x, y + i * 26, s.lstrip("!"), px, cor, weight=peso)


# 1 ------------------------------------------------------------- chicote
x, y = passo(1, 0, 0, "Monte o chicote solar",
             "12 pontos de solda, 4 emendas, 4 terminais")
v = View(x + 70, y + 150, 5.0)
for i in range(6):
    lin_i, col_i = i // 2, i % 2
    v.rect(col_i * 28.0, lin_i * 14.0, MOD_W, MOD_H, 0.5, fill="#e4e6ea",
           stroke="#9aa0a8", stroke_width=0.3)
    v.circle(col_i * 28.0 + 2.3, lin_i * 14.0 + 4.0, 1.0, fill="#fff",
             stroke=C["fio_n"], stroke_width=0.35)
    v.circle(col_i * 28.0 + MOD_W - 2.3, lin_i * 14.0 + 4.0, 1.0, fill="#fff",
             stroke=C["fio_p"], stroke_width=0.35)
    v.poly([(col_i * 28.0 + MOD_W - 2.3, lin_i * 14.0 + 4.0), (62.0, lin_i * 14.0 + 4.0)],
           fill="none", stroke=C["fio_p"], stroke_width=0.6)
    v.poly([(col_i * 28.0 + 2.3, lin_i * 14.0 + 4.0), (col_i * 28.0 - 2.0, lin_i * 14.0 + 4.0),
            (col_i * 28.0 - 2.0, lin_i * 14.0 + 10.5), (-6.0, lin_i * 14.0 + 10.5)],
           fill="none", stroke=C["fio_n"], stroke_width=0.6)
v.poly([(-6.0, 10.5), (-6.0, 38.5), (68.0, 38.5)], fill="none", stroke=C["fio_n"],
       stroke_width=0.8)
v.rect(68.0, 4.0, 5.8, 7.2, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.3)
v.text(75.5, 8.0, "ZHR-4", 16, C["texto"], "start", 700)
linhas(x + 60, y + 400, [
    "Solde um par de fios em cada módulo, no + e no − da",
    "serigrafia do VERSO. Una os 2 positivos de cada face:",
    "3 emendas. Una os 6 negativos: 1 emenda. Crimpe os 4",
    "terminais e encaixe na carcaça ZHR-4.",
    "",
    "O desenho detalhado está em docs/img/chicote-solar.svg.",
    "!Confira a polaridade no multímetro antes de plugar.",
])

# 2 -------------------------------------------------------- módulos na caixa
x, y = passo(2, 1, 0, "Fixe os seis módulos na tampa",
             "2 na faceta, abaixo das teclas, e 2 em cada chanfro")
v = View(x + 320, y + 130, 3.6)
desenha_caixa(v, 0, 0)
for lado_m, ya, yb in CAIXA.modulos_chanfro:
    ym = (ya + yb) / 2
    v.text(-5.0 if lado_m == "L" else CAIXA_W + 5.0, ym + 2.0, "◀" if lado_m == "L" else "▶",
           18, C["fio_p"], "end" if lado_m == "L" else "start", 700)
v.text(-3.0, (CAIXA.faceta[1] + CAIXA.faceta[3]) / 2 + 1.5, "faceta ▶", 16, C["fio_p"], "end", 700)
linhas(x + 60, y + 530, [
    "Cada módulo vai no seu bolso da tampa, sobre fita dupla-face,",
    "com os fios passando pelos furos do fundo do bolso. Uma",
    "cobertura transparente de PC ou PET, cortada no contorno do",
    "STL, fecha cada bolso, colada com silicone de cura neutra.",
    "Cada face olha para um lado: a colheita rende com o guidão",
    "em qualquer direção.",
])

# 3 ----------------------------------------------------------- display
x, y = passo(3, 0, 1, "Prepare o display",
             "dois cabos planos saem da mesma borda longa")
v = View(x + 420, y + 150, 4.4)
desenha_tela(v, 0, 0)
linhas(x + 60, y + 530, [
    "O LPM027M128C tem duas fitas, as duas com passo de 0,5 mm:",
    "10 vias de sinal e 5 vias só para a luz frontal (na fita, os",
    "quatro catodos nas vias 1 a 4 e o anodo comum na 5).",
    "O vidro é colado sob a tampa, em volta da janela, com fita",
    f"de {br(CX.FITA_DISPLAY)} mm.",
])

# 4 ------------------------------------------------------- FPC na placa
x, y = passo(4, 1, 1, "Ligue os dois cabos do display",
             "J402 e J401, na borda esquerda da face da frente")
v = View(x + 380, y + 130, 3.6)
desenha_placa(v, 0, 0, destaque=("J402", "J401"))
for ref, larg, txt in (("J402", 1.6, "5 vias"), ("J401", 2.4, "10 vias")):
    yy = PECAS[ref]["y"]
    v.poly([(-16.0, yy), (PECAS[ref]["caixa"][0], yy)], fill="none", stroke=C["fpc"],
           stroke_width=larg)
    v.text(-17.0, yy - 1.0, txt, 16, C["texto"], "end", 700)
linhas(x + 60, y + 530, [
    "Faça isto com a placa AINDA FORA da caixa: é onde se",
    "alcança a trava dos dois conectores. J402, o da luz, fica em",
    f"{onde('J402')} e J401, o do sinal, em {onde('J401')}, medidos do",
    "canto superior esquerdo da placa; o contato é dos dois lados.",
    "Na montagem da Sharp, a fita dela vai no mesmo J401, e a",
    f"luz do Azumo no J403, no verso, em {onde('J403')}.",
])

# 5 ------------------------------------------------------- chicote na placa
x, y = passo(5, 0, 2, "Ligue o chicote solar", "J103 fica no VERSO da placa")
v = View(x + 420, y + 130, 3.6)
desenha_placa(v, 0, 0, verso=True, destaque=("J103",))
# the board is mirrored here: J103's mouth, which points to the board's
# LEFT edge on the front, is on the right of this view
j103 = PECAS["J103"]
v.poly([(PCB_W - j103["caixa"][0], j103["y"]), (PCB_W - j103["x"] + 16.0, j103["y"])], fill="none",
       stroke=C["fio_p"], stroke_width=1.4)
v.text(PCB_W - j103["x"] + 17.0, j103["y"] - 1.5, "4 vias", 16, C["texto"], "start", 700)
linhas(x + 60, y + 530, [
    "A placa está virada aqui: J103 e o buzzer ficam no verso.",
    f"J103 fica em {onde('J103')}, em pé na borda esquerda, com a",
    "boca para a parede da caixa; o chicote sobe pela lateral",
    "da placa até os módulos na tampa. O conector da bateria é",
    "de outra família de propósito, JST SH de 1,0 mm contra o",
    "ZH de 1,5 mm: os dois não entram um no outro, e 4,2 V na",
    "entrada do colhedor, que aguenta 3,3 V, o queimaria.",
])

# 6 ------------------------------------------------------------ bateria
x, y = passo(6, 1, 2, "Ligue a bateria", "J102 fica na FRENTE da placa")
v = View(x + 380, y + 130, 3.6)
desenha_placa(v, 0, 0, destaque=("J102",))
j102 = PECAS["J102"]
v.rect(-46.0, 20.0, BAT_W, BAT_H, 2.0, fill=C["bateria"], stroke="#6f7a86",
       stroke_width=0.4)
v.poly([(-10.0, j102["y"]), (j102["caixa"][0], j102["y"])], fill="none", stroke="#6f7a86",
       stroke_width=1.2)
v.text(-28.0, 18.0, "célula LiPo", 16, C["texto"], "middle", 700)
linhas(x + 60, y + 530, [
    f"J102 fica em {onde('J102')}, em pé, com a boca para a",
    "esquerda: a célula fica debaixo da placa e o cabo dela dá a",
    "volta pela lateral esquerda. Pode ligar com a placa já",
    "assentada; a trava do SH é por atrito, e o cabo curto vai",
    "preso à caixa com fita.",
    "!A pinagem do conector da bateria é proposta deste",
    "!projeto e tem de ser combinada com o fabricante do pack.",
])

# 7 -------------------------------------------------------- placa na caixa
x, y = passo(7, 0, 3, "Assente a placa na cavidade",
             f"dois parafusos M2, em ({br(FUROS[0][0])}; {br(FUROS[0][1])}) e "
             f"({br(FUROS[1][0])}; {br(FUROS[1][1])}) da placa")
v = View(x + 320, y + 130, 3.6)
desenha_caixa(v, 0, 0, com_modulos=False)
v.rect(CX.PAREDE, CX.PAREDE, CAV_W, CAV_H, CAIXA_R - CX.PAREDE, fill="none", stroke=C["fraco"],
       stroke_width=0.3, stroke_dasharray="1.2 1.2")
desenha_placa(v, PX0, PY0)
for px_, py_ in CAIXA.pilares:
    v.rect(px_ - 1.0, py_ - 1.0, 2.0, 2.0, 0.3, fill="#9aa0a8")
lado = PX0 - CX.PAREDE
cota_h(v, 4.8, CX.PAREDE, PX0, br(lado))
cota_h(v, 4.8, PX0 + PCB_W, CAIXA_W - CX.PAREDE, br(lado))
cota_v(v, CAIXA_W - 1.0, CX.PAREDE, PY0, br(PY0 - CX.PAREDE))
linhas(x + 60, y + 530, [
    f"A cavidade é {br(CAV_W)} × {br(CAV_H)} e a placa é {br(PCB_W)} × {br(PCB_H)}: sobram "
    f"{br(lado)} mm",
    f"de cada lado, {br(PY0 - CX.PAREDE)} em cima (o berço da antena externa) e "
    f"{br(CX.FOLGA_PLACA)}",
    "embaixo, para a boca do USB-C chegar ao entalhe da parede,",
    "que tem uma porta por fora. Os dois parafusos M2 entram nas",
    "bossas dos furos da placa; quatro pilares apoiam os cantos.",
    "A tampa fecha com quatro M2 nos cantos.",
])

# 8 ------------------------------------------------------------- fechar
x, y = passo(8, 1, 3, "Feche o aparelho",
             "a pilha, de frente para trás")
v = View(x + 240, y + 150, 4.4)
for i, (nome, alt, medida, cor) in enumerate(
        (("display", CX.DISPLAY_ESP, f"{br(CX.DISPLAY_ESP)} mm, colado sob a tampa", C["tela"]),
         ("placa", PCB_T, f"{br(PCB_T)} mm", C["placa"]),
         ("célula LiPo", BAT_T, f"{br(BAT_T)} mm", C["bateria"]))):
    yy = i * 18.0
    hh = alt * 1.8
    v.rect(0, yy, 62.0, hh, 0.8, fill=cor, stroke="#7d848c", stroke_width=0.3)
    v.text(66.0, yy + hh / 2 + 1.0, f"{nome} · {medida}", 17, C["texto"], "start", 700)
    if i < 2:
        v.poly([(31.0, yy + hh + 1.5), (31.0, yy + 15.0)], fill="none",
               stroke=C["fraco"], stroke_width=0.35)
        v.poly([(29.5, yy + 13.0), (31.0, yy + 15.5), (32.5, yy + 13.0)],
               fill="none", stroke=C["fraco"], stroke_width=0.35)
linhas(x + 60, y + 400, [
    "Antes de fechar, confira: a boca do USB-C alinhada com o",
    "entalhe da parede, as três teclas alinhadas com os botões,",
    "e nenhum fio prensado entre a placa e a caixa.",
    "",
    f"Os tetos de altura da placa saem daí: {br(Z_PLACA0 - Z_DISP1)} mm sob o",
    f"display e {br(CX.CELULA_VAO)} mm sob a placa, sobre a célula. O dry run",
    "da placa e o da caixa medem isso no CAD.",
])

rotulo(40, CANVAS_H - 40,
       "Gerado por tools/docs/assembly_manual.py · as medidas saem do "
       "gnssbike.kicad_pcb, de hardware_gnssbike/caixa/make_caixa.py e de cad/make_fab.py",
       16, C["fraco"])

add("</svg>")

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
print(f"{OUT.relative_to(ROOT)}: {CANVAS_W} x {CANVAS_H}, 8 passos, "
      f"{len(CONEC)} conectores em escala, {N_PECAS} componentes na montagem da JDI")
