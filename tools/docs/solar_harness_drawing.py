"""Draw the solar harness of hardware_gnssbike/06: the six modules, the wires and J103.

The question this answers is the owner's: "I have to solder a positive and a
negative on each panel - and connect them where?". The documents said the
topology (six in parallel) and the connector (a 4-way JST ZH), but a table
and a box diagram do not show a person what to build. This does: the six
modules at true size, seen from the side you solder, every wire, every
splice, and the contact each one lands on.

Three panels:

  A - where the six modules live on the case, so the names PV101..PV106 mean
      something physical;
  B - the harness itself, unfolded and to scale, which is the drawing you
      keep on the bench;
  C - where it lands on the board, with the three 0 ohm links that let one
      face be measured alone, and the connector magnified.

Everything is in millimetres and every frame carries its own px/mm, so a
part drawn in two panels is the same part at two scales, never a sketch.
Numbers come from hardware_gnssbike/06-conectores-e-pontos-de-teste.md,
from cad/parts.py and from the board file.

Usage: python tools/docs/solar_harness_drawing.py [output.svg]
       (default: docs/img/chicote-solar.svg)
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = (pathlib.Path(sys.argv[1]) if len(sys.argv) > 1
       else ROOT / "docs" / "img" / "chicote-solar.svg")

CANVAS_W, CANVAS_H = 2380, 1320
FONT = "Inter, 'Segoe UI', Roboto, Arial, sans-serif"

MOD_W, MOD_H = 23.0, 8.0     # ANYSOLAR KXOB25-05X3F, three cells in series
ZH_PASSO = 1.5               # JST ZH: 1.5 mm between contacts
ZH_L, ZH_P = 7.2, 5.8        # ZHR-4 housing, length x depth

COR = {
    "pv_a": "#cc2b2b",
    "pv_b": "#e08020",
    "pv_c": "#9b3fa8",
    "gnd": "#1a1a1a",
    "cobre": "#c8a12a",
    "placa": "#2f6b3a",
    "papel": "#ffffff",
    "campo": "#f5f6f8",
    "texto": "#3a3f47",
    "fraco": "#6b7178",
}

out = []


def add(s):
    out.append(s)


def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


class View:
    """A drawing frame: millimetres in, pixels out."""

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

    def text(self, x, y, s, px, fill=COR["texto"], anchor="start", weight=400):
        add(f'<text x="{f(self.X(x))}" y="{f(self.Y(y))}" font-family="{FONT}" '
            f'font-size="{f(px)}" fill="{fill}" text-anchor="{anchor}" '
            f'font-weight="{weight}">{s}</text>')


def rotulo(x, y, s, px=17, fill=COR["texto"], anchor="start", weight=400):
    add(f'<text x="{f(x)}" y="{f(y)}" font-family="{FONT}" font-size="{f(px)}" '
        f'fill="{fill}" text-anchor="{anchor}" font-weight="{weight}">{s}</text>')


def moldura(x, y, w, h, titulo, sub=""):
    add(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="{f(h)}" rx="14" '
        f'fill="{COR["campo"]}" stroke="#d7dae0" stroke-width="1.5"/>')
    rotulo(x + 22, y + 36, titulo, 24, "#111", weight=700)
    if sub:
        rotulo(x + 22, y + 60, sub, 16, COR["fraco"])


def modulo(v, x, y, ref, cor):
    """A KXOB25-05X3F seen from the BACK - the side the wire is soldered to.

    The datasheet does not number the terminals: it marks + and - on the back
    silkscreen and nothing else. The pads are drawn at the two ends, which is
    where they are, and the drawing says out loud that the mark printed on
    the part is what rules.
    """
    v.rect(x, y, MOD_W, MOD_H, 0.6, fill="#e4e6ea", stroke="#9aa0a8",
           stroke_width=0.3)
    for i in range(3):
        v.rect(x + 4.6 + i * 4.6, y + 0.7, 4.2, MOD_H - 1.4, 0.2, fill="none",
               stroke="#c6cad0", stroke_width=0.2)
    pn = (x + 2.3, y + MOD_H / 2)
    pp = (x + MOD_W - 2.3, y + MOD_H / 2)
    v.circle(pn[0], pn[1], 1.15, fill="#ffffff", stroke=COR["gnd"], stroke_width=0.4)
    v.circle(pp[0], pp[1], 1.15, fill="#ffffff", stroke=cor, stroke_width=0.4)
    v.text(pn[0], pn[1] + 1.05, "−", 19, COR["gnd"], "middle", 700)
    v.text(pp[0], pp[1] + 1.0, "+", 17, cor, "middle", 700)
    v.text(x + MOD_W / 2, y - 1.6, ref, 15, "#33383f", "middle", 700)
    return pn, pp


add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS_W}" '
    f'height="{CANVAS_H}" viewBox="0 0 {CANVAS_W} {CANVAS_H}">')
add(f'<rect width="{CANVAS_W}" height="{CANVAS_H}" fill="{COR["papel"]}"/>')

rotulo(40, 54, "Como ligar os seis módulos solares na placa", 32, "#111", weight=700)
rotulo(40, 84, "GNSS Bike Computer · placa gnssbike · conector J103 · desenho em "
               "escala, medidas em mm · nada montado nem medido", 17, COR["fraco"])

# ============================================================== A · a caixa
AX, AY, AW, AH = 40, 112, 500, 1156
moldura(AX, AY, AW, AH, "A · Onde fica cada módulo",
        "a caixa vista de frente, 62 × 104 mm")

SA = 5.4
cv = View(AX + (AW - 62 * SA) / 2, AY + 150, SA)
cv.rect(0, 0, 62, 104, 7, fill="#eceef1", stroke="#aab0b8", stroke_width=0.5)
cv.rect(0.6, 17.5, 6.2, 55.0, 2.8, fill="#e0e3e8", stroke="#b6bcc4", stroke_width=0.35)
cv.rect(55.2, 17.5, 6.2, 55.0, 2.8, fill="#e0e3e8", stroke="#b6bcc4", stroke_width=0.35)
cv.rect(9.0, 12.0, 44.0, 60.0, 1.2, fill="#d3d7dc", stroke="#aab0b8", stroke_width=0.35)
cv.text(31.0, 43.0, "display", 16, "#868c94", "middle")
cv.rect(7.5, 74.6, 47.0, 14.2, 1.6, fill="#e0e3e8", stroke="#b6bcc4", stroke_width=0.35)

MODULOS_NA_CAIXA = (
    (0.95, 21.5, 5.5, 23.0, "PV103", COR["pv_b"], "esq"),
    (0.95, 45.5, 5.5, 23.0, "PV104", COR["pv_b"], "esq"),
    (55.55, 21.5, 5.5, 23.0, "PV105", COR["pv_c"], "dir"),
    (55.55, 45.5, 5.5, 23.0, "PV106", COR["pv_c"], "dir"),
    (7.9, 77.6, 23.0, 8.0, "PV101", COR["pv_a"], "baixo"),
    (31.1, 77.6, 23.0, 8.0, "PV102", COR["pv_a"], "baixo"),
)
for x0, y0, w, h, ref, cor, lado in MODULOS_NA_CAIXA:
    cv.rect(x0, y0, w, h, 0.5, fill="#12161d", stroke=cor, stroke_width=0.8)
    if lado == "esq":
        cv.poly([(x0 - 0.6, y0 + h / 2), (-5.0, y0 + h / 2)], fill="none",
                stroke=cor, stroke_width=0.35)
        cv.text(-6.0, y0 + h / 2 + 1.4, ref, 16, cor, "end", 700)
    elif lado == "dir":
        cv.poly([(x0 + w + 0.6, y0 + h / 2), (67.0, y0 + h / 2)], fill="none",
                stroke=cor, stroke_width=0.35)
        cv.text(68.0, y0 + h / 2 + 1.4, ref, 16, cor, "start", 700)
    else:
        cv.text(x0 + w / 2, y0 + h + 5.0, ref, 16, cor, "middle", 700)

y = AY + 880
for cor, txt in ((COR["pv_a"], "frente inclinada → <tspan font-weight='700'>PV_A</tspan>"),
                 (COR["pv_b"], "chanfro esquerdo → <tspan font-weight='700'>PV_B</tspan>"),
                 (COR["pv_c"], "chanfro direito → <tspan font-weight='700'>PV_C</tspan>")):
    add(f'<rect x="{AX + 30}" y="{y - 13}" width="34" height="7" rx="3.5" fill="{cor}"/>')
    rotulo(AX + 76, y, txt, 17, COR["texto"])
    y += 34
rotulo(AX + 30, y + 16, "Três faces diferentes, e é isso que faz a colheita", 16,
       COR["fraco"])
rotulo(AX + 30, y + 40, "render com o guidão apontando para qualquer lado.", 16,
       COR["fraco"])
rotulo(AX + 30, y + 78, "Os seis ficam EM PARALELO, e não é escolha:", 16,
       COR["texto"], weight=700)
rotulo(AX + 30, y + 102, "cada módulo abre em 2,07 V e o MPPT do colhedor", 16,
       COR["texto"])
rotulo(AX + 30, y + 126, "rastreia até 2,73 V. Dois em série dariam 4,14 V.", 16,
       COR["texto"])

# ============================================================= B · o chicote
BX, BY, BW, BH = 570, 112, 1130, 1156
moldura(BX, BY, BW, BH, "B · O chicote, desenrolado",
        "os módulos vistos POR TRÁS, que é o lado onde se solda · "
        "12 pontos de solda, 4 emendas, 4 terminais")

SB = 6.6
hv = View(BX + 52, BY + 132, SB)

X1, X2 = 20.0, 66.0
X_SPLICE = 100.0        # a emenda dos positivos
X_BUS = 11.0            # a barra por onde descem os negativos
X_FUNIL = 116.0         # onde os quatro fios convergem
Y_BUS = 114.0           # a emenda dos seis negativos
CONEC_X, CONEC_Y = 124.0, 56.0   # canto do ZHR-4

LINHAS = (
    (0.0, ("PV101", "PV102"), COR["pv_a"], "PV_A", 0),
    (42.0, ("PV103", "PV104"), COR["pv_b"], "PV_B", 1),
    (84.0, ("PV105", "PV106"), COR["pv_c"], "PV_C", 2),
)

# o conector, em tamanho real, com as quatro vias a 1,5 mm
hv.rect(CONEC_X, CONEC_Y, ZH_P, ZH_L, 0.6, fill="#e9e4d7", stroke="#9a9488",
        stroke_width=0.35)
contatos = [CONEC_Y + (ZH_L - 3 * ZH_PASSO) / 2 + i * ZH_PASSO for i in range(4)]

for y0, refs, cor, nome, idx in LINHAS:
    yl = y0 + 12.0
    pads = [modulo(hv, xm, y0 + 8.0, ref, cor) for xm, ref in ((X1, refs[0]),
                                                               (X2, refs[1]))]
    for _pn, pp in pads:
        hv.poly([(pp[0], pp[1]), (X_SPLICE, pp[1])], fill="none", stroke=cor,
                stroke_width=0.8, stroke_linecap="round")
    hv.circle(X_SPLICE, yl, 1.4, fill=cor, stroke="#ffffff", stroke_width=0.35)
    hv.text(X_SPLICE + 2.6, yl - 2.6, "emenda", 15, cor, "start", 700)
    hv.text(X_SPLICE + 2.6, yl + 4.4, nome, 16, cor, "start", 700)
    # do funil ao contato: os quatro fios convergem para 1,5 mm de passo
    hv.poly([(X_SPLICE, yl), (X_FUNIL, yl), (CONEC_X, contatos[idx])],
            fill="none", stroke=cor, stroke_width=1.1, stroke_linecap="round",
            stroke_linejoin="round")
    # os negativos, para a esquerda; o do segundo modulo passa por baixo
    for k, (pn, _pp) in enumerate(pads):
        if k == 0:
            hv.poly([(pn[0], pn[1]), (X_BUS, pn[1])], fill="none",
                    stroke=COR["gnd"], stroke_width=0.8, stroke_linecap="round")
        else:
            hv.poly([(pn[0], pn[1]), (pn[0] - 3.4, pn[1]),
                     (pn[0] - 3.4, y0 + 23.0), (X_BUS, y0 + 23.0)],
                    fill="none", stroke=COR["gnd"], stroke_width=0.8,
                    stroke_linecap="round", stroke_linejoin="round")

hv.poly([(X_BUS, 12.0), (X_BUS, Y_BUS)], fill="none", stroke=COR["gnd"],
        stroke_width=1.1, stroke_linecap="round")
hv.circle(X_BUS, Y_BUS, 1.7, fill=COR["gnd"], stroke="#ffffff", stroke_width=0.4)
hv.text(X_BUS + 3.2, Y_BUS + 1.4, "emenda dos SEIS negativos", 16, COR["gnd"],
        "start", 700)
hv.poly([(X_BUS, Y_BUS), (X_FUNIL, Y_BUS), (CONEC_X, contatos[3])],
        fill="none", stroke=COR["gnd"], stroke_width=1.1, stroke_linecap="round",
        stroke_linejoin="round")

for i, cor in enumerate((COR["pv_a"], COR["pv_b"], COR["pv_c"], COR["gnd"])):
    hv.circle(CONEC_X + 1.5, contatos[i], 0.5, fill=cor)
hv.text(CONEC_X + ZH_P + 1.6, CONEC_Y + 2.6, "ZHR-4", 17, "#4a4f57", "start", 700)
hv.text(CONEC_X + ZH_P + 1.6, CONEC_Y + 6.4, "tamanho real", 14, COR["fraco"])
hv.text(CONEC_X + ZH_P + 1.6, CONEC_Y + 10.0, "ampliado em C", 14, COR["fraco"])

rotulo(BX + 40, BY + BH - 168,
       "As duas ilhas de cada módulo estão marcadas + e − na serigrafia do VERSO. "
       "A ficha do KXOB25-05X3F não numera os", 16, COR["fraco"])
rotulo(BX + 40, BY + BH - 144,
       "terminais: a marca impressa na peça é que manda, não este desenho.",
       16, COR["fraco"])
rotulo(BX + 40, BY + BH - 106,
       "Fio AWG 28 · 110 mA no arranjo inteiro, 37 mA por face · o terminal "
       "SZH-002T-P0.5 aceita de AWG 32 a 28.", 17, COR["texto"])
rotulo(BX + 40, BY + BH - 70,
       "Ordem de montagem: solde os 12 fios nos módulos → una os 2 positivos de "
       "cada face (3 emendas) → una os 6 negativos", 17, COR["texto"])
rotulo(BX + 40, BY + BH - 46,
       "(1 emenda) → crimpe os 4 terminais → encaixe na carcaça.", 17, COR["texto"])
rotulo(BX + 40, BY + BH - 16,
       "Confira a polaridade no multímetro, com os módulos no sol, ANTES de plugar.",
       17, COR["pv_a"], weight=700)

# ============================================================== C · a placa
CX, CY, CW, CH = 1730, 112, 610, 1156
moldura(CX, CY, CW, CH, "C · Onde entra na placa",
        "placa 34 × 90 mm · J103 na borda direita")

SC = 5.6
bv = View(CX + 40, CY + 150, SC)
bv.rect(0, 0, 34, 90, 2.5, fill=COR["placa"], stroke="#1f4a28", stroke_width=0.4)
bv.text(17, -4.0, "vista de cima", 16, COR["fraco"], "middle")

JX, JY = 29.0, 32.0                       # posicao real do J103 na placa
bv.rect(JX - 3.5, JY - ZH_L / 2, 7.0, ZH_L, 0.6, fill="#e9e4d7", stroke="#9a9488",
        stroke_width=0.35)
for i, cor in enumerate((COR["pv_a"], COR["pv_b"], COR["pv_c"], COR["gnd"])):
    bv.circle(JX + 1.0, JY - 2.25 + i * ZH_PASSO, 0.45, fill=cor)
bv.text(JX, JY - 6.0, "J103", 17, "#f0f4f0", "middle", 700)
# por onde o cabo entra
bv.poly([(37.4, JY), (34.0, JY)], fill="none", stroke="#2b2b2b", stroke_width=0.3)
bv.poly([(34.0, JY), (35.4, JY - 0.9), (35.4, JY + 0.9), (34.0, JY)],
        fill="#2b2b2b", stroke="none")
bv.text(38.2, JY - 0.4, "o cabo do", 15, "#2b2b2b", "start", 700)
bv.text(38.2, JY + 3.4, "painel entra", 15, "#2b2b2b", "start", 700)
bv.text(38.2, JY + 7.2, "por aqui", 15, "#2b2b2b", "start", 700)

# os tres 0 ohm e o caminho ate o colhedor
for i, cor in enumerate((COR["pv_a"], COR["pv_b"], COR["pv_c"])):
    yy = JY - 2.25 + i * ZH_PASSO
    bv.poly([(JX - 3.7, yy), (23.0, yy)], fill="none", stroke=cor, stroke_width=0.35)
    bv.rect(21.2, yy - 0.5, 1.8, 1.0, 0.15, fill="#d2d6dc", stroke=cor,
            stroke_width=0.2)
bv.text(22.1, JY + 8.6, "R113 · R114 · R115", 15, "#e4efe4", "middle", 700)
bv.text(22.1, JY + 12.2, "0 Ω, um por face", 14, "#bdd3be", "middle")
bv.poly([(20.8, JY), (17.0, JY), (17.0, 20.0), (13.6, 20.0)], fill="none",
        stroke=COR["cobre"], stroke_width=0.55, stroke_linejoin="round")
bv.rect(9.6, 18.0, 4.0, 4.0, 0.3, fill="#1b1d21", stroke="#454b53", stroke_width=0.25)
bv.text(11.6, 15.6, "ADP5091", 15, "#e4efe4", "middle", 700)
bv.text(11.6, 25.6, "colhedor", 14, "#bdd3be", "middle")

# o conector ampliado, abaixo da placa
rotulo(CX + 40, CY + 690, "O conector, ampliado 4,8 ×", 20, "#111", weight=700)
rotulo(CX + 40, CY + 716, "JST S4B-ZR-SM4A-TF na placa · ZH de 1,5 mm · 4 vias",
       16, COR["fraco"])

SZ = 24.0
zv = View(CX + 70, CY + 760, SZ)
zv.rect(0, 0, ZH_P, ZH_L, 0.6, fill="#e9e4d7", stroke="#9a9488", stroke_width=0.1)
for i, (cor, nome) in enumerate(((COR["pv_a"], "1 · PV_A · frente inclinada"),
                                 (COR["pv_b"], "2 · PV_B · chanfro esquerdo"),
                                 (COR["pv_c"], "3 · PV_C · chanfro direito"),
                                 (COR["gnd"], "4 · GND · os seis negativos"))):
    yy = (ZH_L - 3 * ZH_PASSO) / 2 + i * ZH_PASSO
    zv.circle(ZH_P / 2, yy, 0.38, fill=cor, stroke="#ffffff", stroke_width=0.05)
    zv.poly([(ZH_P / 2, yy), (ZH_P + 1.2, yy)], fill="none", stroke=cor,
            stroke_width=0.09)
    zv.text(ZH_P + 1.6, yy + 0.24, nome, 18, COR["texto"], "start", 600)

rotulo(CX + 40, CY + CH - 196,
       "Os três 0 Ω não são enfeite: abrindo um deles, mede-se", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 172,
       "UMA face sozinha no sol. Com um fio de positivo só, os", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 148,
       "seis dariam um número e nunca se saberia qual face", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 124, "está rendendo.", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 86,
       "O conector da bateria é de outra família de propósito", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 62,
       "(JST GH de 1,25 mm): os dois não entram um no outro.", 16, COR["texto"])
rotulo(CX + 40, CY + CH - 26,
       "A ordem das quatro vias ainda não foi conferida contra", 16, COR["pv_a"],
       weight=700)
rotulo(CX + 40, CY + CH - 4,
       "uma peça real: confirme o contato 1 na carcaça.", 16, COR["pv_a"],
       weight=700)

add("</svg>")

OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
print(f"{OUT.relative_to(ROOT)}: {CANVAS_W} x {CANVAS_H}, "
      f"6 módulos, 12 pontos de solda, 4 emendas, 4 terminais")
