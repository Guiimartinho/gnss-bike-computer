"""Draw the assembly manual of the device: every part to scale, and the order.

The owner asked for a manual - case, panels, wires with connectors, the board
in the case, the display with its connectors, everything in order. This draws
it the way the other drawings here are drawn: an SVG in millimetres, generated
by program, so that when a connector moves on the board the sheet moves with
it.

Every part is at TRUE SIZE at its panel's own scale, and every dimension comes
from a file in this repository:

  * case 62 x 104 x 19 mm, corner radius 7          tools/docs/case_drawing.py
  * cavity 58 x 100 mm (rule ME1: the 34 x 90 board leaves 12 mm each side
    and 5 mm top and bottom)                        cad/dry_run_pcb.py
  * board 34 x 90 x 0.8 mm, one M2 hole at (3.2; 45.0)      gnssbike.kicad_pcb
  * every connector's position and face                     gnssbike.kicad_pcb
  * display LPM027M128C, outline 40.08 x 61.8, active 35.28 x 58.8
                                                    hardware_gnssbike/04
  * LiPo pouch 36 x 60 x 7                          hardware_gnssbike/04
  * solar module KXOB25-05X3F, 23 x 8               cad/parts.py

What is NOT in any file is said so on the sheet, not guessed: how the display
is held to the front, how the FPC is folded inside, and where the case
screws are.

Usage: python tools/docs/assembly_manual.py [output.svg]
       (default: docs/img/manual-de-montagem.svg)
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1
       else ROOT / "docs" / "img" / "manual-de-montagem.svg")

CANVAS_W, CANVAS_H = 2480, 4560
FONT = "Inter, 'Segoe UI', Roboto, Arial, sans-serif"

CAIXA_W, CAIXA_H, CAIXA_T = 62.0, 104.0, 19.0
CAV_W, CAV_H = 58.0, 100.0
PCB_W, PCB_H, PCB_T = 34.0, 90.0, 0.8
TELA_W, TELA_H = 40.08, 61.8
ATIVA_W, ATIVA_H = 35.28, 58.8
BAT_W, BAT_H, BAT_T = 36.0, 60.0, 7.0
MOD_W, MOD_H = 23.0, 8.0
FURO = (3.2, 45.0)

# ref -> (x, y, angulo, face, rotulo)
CONEC = {
    "J401": (5.10, 26.50, -90, "frente", "FPC do display · 10 vias"),
    "J402": (5.10, 37.00, -90, "frente", "FPC da luz · 5 vias"),
    "J103": (29.00, 32.00, 90, "frente", "painel solar · 4 vias"),
    "J102": (4.00, 62.00, -90, "verso", "bateria · 6 vias"),
    "J101": (6.20, 85.71, 0, "frente", "USB-C"),
    "J201": (11.50, 78.00, 0, "frente", "gravação SWD"),
    "E301": (13.25, 1.80, 0, "frente", "antena GNSS"),
}
TECLAS = ((6.20, 58.0), (16.40, 58.0), (26.60, 58.0))

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


# ------------------------------------------------------------------ peças
def desenha_placa(v, x, y, verso=False, destaque=()):
    """The board, 34 x 90, seen from the front or mirrored for the back."""
    def mx(px):
        return (PCB_W - px) if verso else px

    v.rect(x, y, PCB_W, PCB_H, 2.5, fill=C["placa"], stroke=C["placa_borda"],
           stroke_width=0.4)
    v.circle(x + mx(FURO[0]), y + FURO[1], 1.1, fill=C["campo"],
             stroke=C["placa_borda"], stroke_width=0.3)
    for ref, (cx, cy, _a, face, _r) in CONEC.items():
        se_ve = (face == "verso") if verso else (face == "frente")
        if not se_ve:
            continue
        forte = ref in destaque
        cor = "#ffd24d" if forte else "#e9e4d7"
        v.rect(x + mx(cx) - 3.0, y + cy - 4.2, 6.0, 8.4, 0.5, fill=cor,
               stroke="#8d887c", stroke_width=0.3)
        if forte:
            # o rotulo vai ao LADO, nunca por cima: J401 e J402 estao a 10,5 mm
            # um do outro e um rotulo acima do segundo cai sobre o primeiro
            if cx < PCB_W / 2:
                v.text(x + mx(cx) + 4.2, y + cy + 1.2, ref, 15, "#f6f9f6",
                       "start" if not verso else "end", 700)
            else:
                v.text(x + mx(cx) - 4.2, y + cy + 1.2, ref, 15, "#f6f9f6",
                       "end" if not verso else "start", 700)
    if not verso:
        for tx, ty in TECLAS:
            v.circle(x + mx(tx), y + ty, 1.8, fill="#d8dce1",
                     stroke="#9aa0a8", stroke_width=0.3)
    else:
        v.rect(x + mx(17.0) - 6.0, y + 67.5 - 6.0, 12.0, 12.0, 6.0,
               fill="#c9ced4", stroke="#9aa0a8", stroke_width=0.3)


def desenha_tela(v, x, y, com_fpc=True):
    v.rect(x, y, TELA_W, TELA_H, 1.0, fill=C["tela"], stroke="#9aa0a8",
           stroke_width=0.4)
    v.rect(x + (TELA_W - ATIVA_W) / 2, y + 1.5, ATIVA_W, ATIVA_H, 0.4,
           fill=C["tela_ativa"], stroke="#b6bcc4", stroke_width=0.3)
    if com_fpc:
        # o FPC de sinal, 10 vias, e o da luz, 5 vias, saem da mesma borda
        v.rect(x - 14.0, y + 12.0, 14.0, 6.0, 0.4, fill=C["fpc"],
               stroke="#8a6a2e", stroke_width=0.3)
        v.rect(x - 14.0, y + 24.0, 14.0, 3.5, 0.4, fill=C["fpc"],
               stroke="#8a6a2e", stroke_width=0.3)
        v.text(x - 15.0, y + 16.5, "10 vias · sinal", 15, C["texto"], "end", 700)
        v.text(x - 15.0, y + 27.5, "5 vias · luz", 15, C["texto"], "end", 700)


def desenha_caixa(v, x, y, com_modulos=True):
    v.rect(x, y, CAIXA_W, CAIXA_H, 7.0, fill=C["caixa"],
           stroke=C["caixa_borda"], stroke_width=0.5)
    v.rect(x + 0.6, y + 17.5, 6.2, 55.0, 2.8, fill=C["caixa_fundo"],
           stroke="#b6bcc4", stroke_width=0.35)
    v.rect(x + 55.2, y + 17.5, 6.2, 55.0, 2.8, fill=C["caixa_fundo"],
           stroke="#b6bcc4", stroke_width=0.35)
    v.rect(x + 7.5, y + 74.6, 47.0, 14.2, 1.6, fill=C["caixa_fundo"],
           stroke="#b6bcc4", stroke_width=0.35)
    if com_modulos:
        for mx, my, mw, mh in ((0.95, 21.5, 5.5, 23.0), (0.95, 45.5, 5.5, 23.0),
                               (55.55, 21.5, 5.5, 23.0), (55.55, 45.5, 5.5, 23.0),
                               (7.9, 77.6, 23.0, 8.0), (31.1, 77.6, 23.0, 8.0)):
            v.rect(x + mx, y + my, mw, mh, 0.5, fill=C["modulo"],
                   stroke=C["fio_p"], stroke_width=0.6)


def cota_h(v, y, x1, x2, txt, px=15):
    v.poly([(x1, y - 1.2), (x1, y + 1.2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x2, y - 1.2), (x2, y + 1.2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x1, y), (x2, y)], fill="none", stroke=C["fraco"], stroke_width=0.25)
    v.text((x1 + x2) / 2, y - 1.8, txt, px, C["fraco"], "middle")


def cota_v(v, x, y1, y2, txt, px=15):
    v.poly([(x - 1.2, y1), (x + 1.2, y1)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x - 1.2, y2), (x + 1.2, y2)], fill="none", stroke=C["fraco"],
           stroke_width=0.25)
    v.poly([(x, y1), (x, y2)], fill="none", stroke=C["fraco"], stroke_width=0.25)
    v.text(x + 1.6, (y1 + y2) / 2, txt, px, C["fraco"], "start")


add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS_W}" '
    f'height="{CANVAS_H}" viewBox="0 0 {CANVAS_W} {CANVAS_H}">')
add(f'<rect width="{CANVAS_W}" height="{CANVAS_H}" fill="{C["papel"]}"/>')

rotulo(40, 58, "Manual de montagem", 36, C["titulo"], weight=700)
rotulo(40, 92, "GNSS Bike Computer · placa gnssbike · todas as peças em escala, "
               "medidas em mm", 18, C["fraco"])
add(f'<rect x="1380" y="28" width="1060" height="78" rx="10" fill="#fdf3f3" '
    f'stroke="{C["alerta"]}" stroke-width="1.5"/>')
rotulo(1404, 58, "Nada disto foi montado, medido ou fabricado.", 18, C["alerta"],
       weight=700)
rotulo(1404, 84, "A caixa é o conceito da V3; a placa é arquivo de CAD. "
                 "O que não está em arquivo nenhum está marcado como tal.", 16,
       C["texto"])

# ========================================================== as peças
PX, PY, PW, PH = 40, 130, 2400, 620
moldura(PX, PY, PW, PH, 0, "As peças",
        "tudo no mesmo aumento, 4,2 px/mm · confira antes de começar")

SP = 4.2
pv = View(PX + 60, PY + 150, SP)
desenha_caixa(pv, 0, 0, com_modulos=False)
pv.text(CAIXA_W / 2, CAIXA_H + 7, "caixa · 62 × 104 × 19", 16, C["texto"], "middle", 700)
pv.text(CAIXA_W / 2, CAIXA_H + 12, "1 peça", 15, C["fraco"], "middle")

pv2 = View(PX + 400, PY + 150, SP)
desenha_placa(pv2, 0, 0)
pv2.text(PCB_W / 2, PCB_H + 7, "placa · 34 × 90 × 0,8", 16, C["texto"], "middle", 700)
pv2.text(PCB_W / 2, PCB_H + 12, "1 peça, 144 componentes", 15, C["fraco"], "middle")

pv3 = View(PX + 660, PY + 160, SP)
desenha_tela(pv3, 16.0, 0)
pv3.text(16 + TELA_W / 2, TELA_H + 7, "display JDI LPM027M128C", 16, C["texto"],
         "middle", 700)
pv3.text(16 + TELA_W / 2, TELA_H + 12, "40,08 × 61,8 · 1 peça", 15, C["fraco"], "middle")

pv4 = View(PX + 1130, PY + 190, SP)
pv4.rect(0, 0, BAT_W, BAT_H, 2.0, fill=C["bateria"], stroke="#6f7a86", stroke_width=0.4)
pv4.rect(BAT_W / 2 - 5, -3.0, 10, 3.0, 0.4, fill="#b9c2cc", stroke="#6f7a86",
         stroke_width=0.3)
pv4.text(BAT_W / 2, BAT_H + 7, "célula LiPo · 36 × 60 × 7", 16, C["texto"], "middle", 700)
pv4.text(BAT_W / 2, BAT_H + 12, "2000 mAh · 1 peça", 15, C["fraco"], "middle")

pv5 = View(PX + 1420, PY + 190, SP)
for i in range(6):
    col, lin = i % 2, i // 2
    pv5.rect(col * 27.0, lin * 12.0, MOD_W, MOD_H, 0.5, fill=C["modulo"],
             stroke=C["fio_p"], stroke_width=0.6)
pv5.text(24.0, 12 * 3 + 5, "módulos solares · 23 × 8", 16, C["texto"], "middle", 700)
pv5.text(24.0, 12 * 3 + 10, "KXOB25-05X3F · 6 peças", 15, C["fraco"], "middle")

pv6 = View(PX + 1830, PY + 190, SP)
pv6.rect(0, 0, 5.8, 7.2, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.3)
pv6.text(2.9, 12.0, "ZHR-4 + 4 × SZH-002T-P0.5", 16, C["texto"], "middle", 700)
pv6.text(2.9, 17.0, "carcaça do chicote solar", 15, C["fraco"], "middle")
pv6.rect(0, 26.0, 7.6, 8.4, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.3)
pv6.text(3.8, 40.0, "GHR-06V-S + 6 × SSHL-002T-P0.2", 16, C["texto"], "middle", 700)
pv6.text(3.8, 45.0, "carcaça do cabo da bateria", 15, C["fraco"], "middle")
pv6.text(3.8, 56.0, "fio AWG 28, cerca de 1 m", 16, C["texto"], "middle", 700)
pv6.text(3.8, 61.0, "parafuso M2 · 1 peça", 16, C["texto"], "middle", 700)

# =================================================== o aparelho montado
MX, MY, MW, MH = 40, 780, 2400, 820
moldura(MX, MY, MW, MH, 0, "O aparelho montado",
        "a caixa com a placa dentro e os seis módulos nas paredes · "
        "dois cortes, para ver os 19 mm de espessura")

# --- vista de frente, com a placa por transparencia
SM = 5.2
mv = View(MX + 250, MY + 140, SM)
desenha_caixa(mv, 0, 0)
mv.rect(10.96, 12.0, TELA_W, TELA_H, 1.0, fill="#ffffff", fill_opacity=0.55,
        stroke="#8f959d", stroke_width=0.4)
mv.text(31.0, 40.0, "display", 16, "#7d838b", "middle")
# a placa, por dentro: 34 x 90 centrada na cavidade de 58 x 100
PX0, PY0 = (CAIXA_W - PCB_W) / 2, (CAIXA_H - PCB_H) / 2
mv.rect(PX0, PY0, PCB_W, PCB_H, 2.5, fill=C["placa"], fill_opacity=0.5,
        stroke=C["placa_borda"], stroke_width=0.5, stroke_dasharray="1.6 1.2")
for ref, (cx, cy, _a, face, rot) in CONEC.items():
    mv.rect(PX0 + cx - 2.4, PY0 + cy - 3.2, 4.8, 6.4, 0.4, fill="#f3e9c9",
            stroke="#8d887c", stroke_width=0.3)
# os rotulos ficam FORA da caixa, com linha de chamada: dentro eles caem
# em cima dos modulos dos chanfros
for ref, lado, ly in (("J401 display", -1, 18.0), ("J402 luz", -1, 28.5),
                      ("J102 bateria (verso)", -1, 53.5),
                      ("J103 painel", +1, 23.5), ("J101 USB-C", +1, 77.2)):
    cx, cy = CONEC[ref.split(" ")[0]][:2]
    ax, ay = PX0 + cx, PY0 + cy
    bx = -3.0 if lado < 0 else CAIXA_W + 3.0
    mv.poly([(ax, ay), (bx, ly)], fill="none", stroke="#7d848c", stroke_width=0.22)
    mv.circle(ax, ay, 0.6, fill="#5b6068")
    mv.text(bx + (-1.4 if lado < 0 else 1.4), ly + 1.2, ref, 15, "#2b2b2b",
            "end" if lado < 0 else "start", 700)
mv.text(CAIXA_W / 2, CAIXA_H + 11.0, "a placa por transparência, 34 × 90", 16,
        C["fraco"], "middle")
mv.text(CAIXA_W / 2, CAIXA_H + 16.0, "centrada na cavidade de 58 × 100", 16,
        C["fraco"], "middle")
mv.poly([(-4.0, 52.0), (66.0, 52.0)], fill="none", stroke=C["alerta"],
        stroke_width=0.3, stroke_dasharray="2 1.5")
mv.text(-5.0, 53.4, "A", 17, C["alerta"], "end", 700)
mv.text(67.0, 53.4, "A", 17, C["alerta"], "start", 700)
mv.poly([(20.0, -4.0), (20.0, 106.0)], fill="none", stroke=C["alerta"],
        stroke_width=0.3, stroke_dasharray="2 1.5")
mv.text(20.0, -5.4, "B", 17, C["alerta"], "middle", 700)
mv.text(20.0, 109.5, "B", 17, C["alerta"], "middle", 700)

# --- corte A-A: pela largura, mostra os chanfros com os modulos e a placa
SS = 8.4
av = View(MX + 700, MY + 175, SS)
av.text(0, -8.0, "Corte A-A · pela largura", 19, C["titulo"], "start", 700)
av.text(0, -3.0, "os dois chanfros de 45°, com um módulo em cada", 15, C["fraco"])
# o contorno da secao: retangulo de 62 x 19 com os dois cantos da frente cortados
av.poly([(6.2, 0), (55.8, 0), (62, 6.2), (62, 19), (0, 19), (0, 6.2), (6.2, 0)],
        fill=C["caixa"], stroke=C["caixa_borda"], stroke_width=0.35)
for x1, y1, x2, y2 in ((0.0, 6.2, 6.2, 0.0), (55.8, 0.0, 62.0, 6.2)):
    av.poly([(x1, y1), (x2, y2)], fill="none", stroke=C["caixa_borda"],
            stroke_width=0.45)
# os modulos, deitados no chanfro (8 mm de largura na face de 45°)
av.poly([(0.55, 6.75), (5.65, 1.65)], fill="none", stroke=C["modulo"],
        stroke_width=1.6, stroke_linecap="round")
av.poly([(56.35, 1.65), (61.45, 6.75)], fill="none", stroke=C["modulo"],
        stroke_width=1.6, stroke_linecap="round")
av.text(3.0, 10.6, "PV104", 15, C["fio_p"], "middle", 700)
av.text(59.0, 10.6, "PV106", 15, C["fio_p"], "middle", 700)
# display, placa e bateria, pela espessura
av.rect(10.96, 1.2, TELA_W, 1.0, 0.2, fill=C["tela"], stroke="#8f959d",
        stroke_width=0.25)
av.rect(14.0, 7.0, PCB_W, PCB_T, 0.15, fill=C["placa"], stroke=C["placa_borda"],
        stroke_width=0.25)
av.rect(13.0, 10.0, BAT_W, BAT_T, 0.4, fill=C["bateria"], stroke="#6f7a86",
        stroke_width=0.25)
av.text(63.5, 2.4, "display", 16, C["texto"], "start", 700)
av.text(63.5, 8.2, "placa · 0,8", 16, C["texto"], "start", 700)
av.text(63.5, 14.4, "célula · 7", 16, C["texto"], "start", 700)
av.text(31.0, 5.6, "2,6 mm de teto", 14, C["fraco"], "middle")
av.text(31.0, 9.4, "1,2 mm de teto", 14, C["fraco"], "middle")
cota_v(av, -2.6, 0, 19, "19")
cota_h(av, 21.5, 0, 62, "62")

# --- corte B-B: pelo comprimento, mostra a frente inclinada e a pilha
bv2 = View(MX + 700, MY + 480, SS)
bv2.text(0, -8.0, "Corte B-B · pelo comprimento", 19, C["titulo"], "start", 700)
bv2.text(0, -3.0, "a frente inclinada com dois módulos, e a pilha inteira", 15,
         C["fraco"])
bv2.poly([(0, 0), (74.0, 0), (89.0, 7.2), (104.0, 7.2), (104.0, 19), (0, 19),
          (0, 0)], fill=C["caixa"], stroke=C["caixa_borda"], stroke_width=0.35)
bv2.poly([(74.0, 0), (89.0, 7.2)], fill="none", stroke=C["caixa_borda"],
         stroke_width=0.45)
bv2.poly([(75.4, 0.65), (86.8, 6.1)], fill="none", stroke=C["modulo"],
         stroke_width=1.6, stroke_linecap="round")
bv2.text(81.0, -2.2, "PV101", 15, C["fio_p"], "middle", 700)
bv2.rect(12.0, 1.2, TELA_H, 1.0, 0.2, fill=C["tela"], stroke="#8f959d",
         stroke_width=0.25)
bv2.rect(7.0, 7.0, PCB_H, PCB_T, 0.15, fill=C["placa"], stroke=C["placa_borda"],
         stroke_width=0.25)
bv2.rect(22.0, 10.0, BAT_H, BAT_T, 0.4, fill=C["bateria"], stroke="#6f7a86",
         stroke_width=0.25)
bv2.text(42.0, 0.2, "display · 61,8 de comprimento", 15, C["texto"], "middle", 700)
bv2.text(45.0, 6.2, "placa · 90", 15, C["texto"], "middle", 700)
bv2.text(52.0, 14.2, "célula · 60", 15, "#f2f5f7", "middle", 700)
bv2.poly([(92.7, 7.4), (92.7, 17.0)], fill="none", stroke=C["fio_p"],
         stroke_width=0.4)
bv2.text(92.7, 19.6, "USB-C", 15, C["fio_p"], "middle", 700)
cota_h(bv2, 21.5, 0, 104, "104")

rotulo(MX + 1660, MY + 150, "O que este corte mostra", 20, C["titulo"], weight=700)
for i, s in enumerate((
        "Os seis módulos ficam nas PAREDES, não na placa: dois",
        "na frente inclinada e um em cada ponta dos dois chanfros",
        "de 45°. É por isso que eles chegam por chicote, e por",
        "isso que há três faces em vez de uma.",
        "",
        "A placa fica no meio da espessura, com o display à frente",
        "e a célula atrás. Daí saem os dois tetos de altura que a",
        "conferência ME2 mede: 2,6 mm do lado do display e",
        "1,2 mm do lado da bateria.",
        "",
        "!O que NÃO está definido em arquivo nenhum: a espessura",
        "!das paredes, a espessura do display, a altura em que a",
        "!placa é presa dentro da caixa e onde ficam os parafusos.",
        "!Os dois cortes põem cada peça na sua largura e na sua",
        "!ordem, que é o que se sabe hoje.")):
    cor = C["alerta"] if s.startswith("!") else C["texto"]
    peso = 700 if s.startswith("!") else 400
    rotulo(MX + 1660, MY + 190 + i * 26, s.lstrip("!"), 16, cor, weight=peso)

# ========================================================== os oito passos
PASSO_W, PASSO_H = 1180, 680
COL = (40, 1260)
LIN = (1640, 2350, 3060, 3770)


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
x, y = passo(2, 1, 0, "Fixe os seis módulos na caixa",
             "2 na frente inclinada, 2 em cada chanfro")
v = View(x + 320, y + 130, 3.6)
desenha_caixa(v, 0, 0)
for mx, my in ((-5.0, 33.0), (-5.0, 57.0), (67.0, 33.0), (67.0, 57.0)):
    v.text(mx, my, "◀" if mx < 0 else "▶", 18, C["fio_p"],
           "end" if mx < 0 else "start", 700)
linhas(x + 60, y + 530, [
    "Cada face olha para um lado diferente, e é isso que faz a",
    "colheita render com o guidão em qualquer direção.",
    "Passe os fios para dentro antes de fixar.",
    "!Como o módulo é preso à parede não está definido em",
    "!arquivo nenhum deste projeto.",
])

# 3 ----------------------------------------------------------- display
x, y = passo(3, 0, 1, "Prepare o display",
             "dois cabos planos saem da mesma borda")
v = View(x + 420, y + 150, 4.4)
desenha_tela(v, 0, 0)
linhas(x + 60, y + 530, [
    "O LPM027M128C tem duas interfaces, as duas com passo de",
    "0,5 mm: 10 vias de sinal e 5 vias só para a luz frontal.",
    "!A ordem das 5 vias da luz ainda não foi levantada: os",
    "!dois PDF da JDI respondem 404.",
    "!Como o painel é preso à frente da caixa não está",
    "!definido em arquivo nenhum.",
])

# 4 ------------------------------------------------------- FPC na placa
x, y = passo(4, 1, 1, "Ligue os dois cabos do display",
             "J401 e J402, na borda esquerda da placa")
v = View(x + 380, y + 130, 3.6)
desenha_placa(v, 0, 0, destaque=("J401", "J402"))
v.poly([(-16.0, 26.5), (5.1, 26.5)], fill="none", stroke=C["fpc"], stroke_width=2.4)
v.poly([(-16.0, 37.0), (5.1, 37.0)], fill="none", stroke=C["fpc"], stroke_width=1.6)
v.text(-17.0, 25.0, "10 vias", 16, C["texto"], "end", 700)
v.text(-17.0, 35.5, "5 vias", 16, C["texto"], "end", 700)
linhas(x + 60, y + 530, [
    "Faça isto com a placa AINDA FORA da caixa: é onde se",
    "alcança a trava dos dois conectores.",
    "J401 fica em (5,1; 26,5) e J402 em (5,1; 37,0), medidos",
    "do canto superior esquerdo da placa.",
])

# 5 ------------------------------------------------------------ bateria
x, y = passo(5, 0, 2, "Ligue a bateria", "J102 fica no VERSO da placa")
v = View(x + 420, y + 130, 3.6)
desenha_placa(v, 0, 0, verso=True, destaque=("J102",))
v.rect(-46.0, 20.0, BAT_W, BAT_H, 2.0, fill=C["bateria"], stroke="#6f7a86",
       stroke_width=0.4)
v.poly([(-10.0, 62.0), (PCB_W - 4.0, 62.0)], fill="none", stroke="#6f7a86",
       stroke_width=1.2)
v.text(-28.0, 18.0, "célula LiPo", 16, C["texto"], "middle", 700)
linhas(x + 60, y + 530, [
    "A placa está virada aqui: J102 e o buzzer ficam no verso,",
    "e a célula fica atrás da placa.",
    "!A pinagem do conector da bateria é proposta deste",
    "!projeto e tem de ser combinada com o fabricante do pack.",
])

# 6 ------------------------------------------------------- chicote na placa
x, y = passo(6, 1, 2, "Ligue o chicote solar",
             "J103, na borda direita da placa")
v = View(x + 380, y + 130, 3.6)
desenha_placa(v, 0, 0, destaque=("J103",))
v.poly([(PCB_W + 16.0, 32.0), (PCB_W + 3.0, 32.0)], fill="none",
       stroke=C["fio_p"], stroke_width=1.4)
v.text(PCB_W + 17.0, 30.5, "4 vias", 16, C["texto"], "start", 700)
linhas(x + 60, y + 530, [
    "J103 fica em (29,0; 32,0). O conector da bateria é de",
    "outra família de propósito, JST GH de 1,25 mm contra o ZH",
    "de 1,5 mm: os dois não entram um no outro, e 4,2 V na",
    "entrada do colhedor, que aguenta 2,73 V, o queimaria.",
])

# 7 -------------------------------------------------------- placa na caixa
x, y = passo(7, 0, 3, "Assente a placa na cavidade",
             "um parafuso M2, em (3,2; 45,0)")
v = View(x + 320, y + 130, 3.6)
desenha_caixa(v, 0, 0, com_modulos=False)
v.rect(2.0, 2.0, CAV_W, CAV_H, 2.0, fill="none", stroke=C["fraco"],
       stroke_width=0.3, stroke_dasharray="1.2 1.2")
desenha_placa(v, 14.0, 7.0)
cota_h(v, 4.8, 2.0, 14.0, "12")
cota_h(v, 4.8, 48.0, 60.0, "12")
cota_v(v, 61.0, 2.0, 7.0, "5")
linhas(x + 60, y + 530, [
    "A cavidade é 58 × 100 e a placa é 34 × 90: sobram 12 mm",
    "de cada lado e 5 mm em cima e embaixo (regra ME1).",
    "A boca do USB-C fica a 0,64 mm da borda de baixo: ela",
    "precisa de furo na parede, alinhado.",
    "!Onde ficam os parafusos da caixa não está definido.",
])

# 8 ------------------------------------------------------------- fechar
x, y = passo(8, 1, 3, "Feche o aparelho",
             "a pilha, de frente para trás")
v = View(x + 240, y + 150, 4.4)
for i, (nome, alt, medida, cor) in enumerate(
        (("display", 1.2, "espessura não levantada", C["tela"]),
         ("placa", PCB_T, "0,8 mm", C["placa"]),
         ("célula LiPo", BAT_T, "7 mm", C["bateria"]))):
    yy = i * 18.0
    v.rect(0, yy, 62.0, alt * 1.8, 0.8, fill=cor, stroke="#7d848c", stroke_width=0.3)
    v.text(66.0, yy + alt * 1.2, f"{nome} · {medida}", 17, C["texto"], "start", 700)
    if i < 2:
        v.poly([(31.0, yy + alt * 1.8 + 1.5), (31.0, yy + 15.0)], fill="none",
               stroke=C["fraco"], stroke_width=0.35)
        v.poly([(29.5, yy + 13.0), (31.0, yy + 15.5), (32.5, yy + 13.0)],
               fill="none", stroke=C["fraco"], stroke_width=0.35)
linhas(x + 60, y + 400, [
    "Antes de fechar, confira: a boca do USB-C alinhada com o",
    "furo da parede, as três teclas alinhadas com os botões,",
    "e nenhum fio prensado entre a placa e a caixa.",
    "",
    "Os tetos de altura da placa saem daí: 2,6 mm sob o",
    "display e 1,2 mm sob a bateria. A conferência ME2 mede",
    "isso no CAD e passa.",
])

rotulo(40, CANVAS_H - 40,
       "Gerado por tools/docs/assembly_manual.py · as medidas saem do "
       "gnssbike.kicad_pcb, de hardware_gnssbike/04 e de cad/parts.py",
       16, C["fraco"])

add("</svg>")

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
print(f"{OUT.relative_to(ROOT)}: {CANVAS_W} x {CANVAS_H}, 8 passos, "
      f"{len(CONEC)} conectores em escala")
