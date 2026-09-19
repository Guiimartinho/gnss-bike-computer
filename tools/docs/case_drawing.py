"""Draw the proposed device of docs/13-placa-nova.md as an SVG (front, side, back and layout).

Concept drawing from the printed V3 case: memory LCD (the render shows the JDI 8-colour option; the
shopping list buys the Sharp mono with a front light for the same connector), solar modules on the
angled front facet and on the side chamfers, GNSS antennas on the top wall. Geometry is in millimetres, scaled
by S px/mm.

Usage: python tools/docs/case_drawing.py [output.svg]   (default: docs/img/placa-nova-caixa.svg)
"""
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "img" / "placa-nova-caixa.svg"

S = 6.0            # px per mm
W, H, T = 62.0, 104.0, 19.0   # case width, height, thickness (mm)
MOUNT = 3.0        # quarter-turn mount protrusion (mm)
R = 7.0            # case corner radius (mm)

CANVAS_W, CANVAS_H = 2100, 1000
FONT = "Inter, 'Segoe UI', Roboto, Arial, sans-serif"

# MIP 8-colour palette as it looks on a reflective panel (muted)
MIP = {
    "bg": "#e6e8e2", "black": "#16181a", "white": "#f4f5f0", "red": "#c8323c",
    "green": "#2e9d4a", "blue": "#2f5fb8", "yellow": "#d9a900", "cyan": "#2aa3b5",
    "magenta": "#a83a95",
}

out = []


def add(s):
    out.append(s)


def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


class View:
    def __init__(self, x0, y0):
        self.x0, self.y0 = x0, y0

    def X(self, x):
        return self.x0 + x * S

    def Y(self, y):
        return self.y0 + y * S

    def rect(self, x, y, w, h, r=0.0, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<rect x="{f(self.X(x))}" y="{f(self.Y(y))}" width="{f(w * S)}" height="{f(h * S)}" '
            f'rx="{f(r * S)}" ry="{f(r * S)}" {attrs}/>')

    def circle(self, cx, cy, r, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<circle cx="{f(self.X(cx))}" cy="{f(self.Y(cy))}" r="{f(r * S)}" {attrs}/>')

    def line(self, x1, y1, x2, y2, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<line x1="{f(self.X(x1))}" y1="{f(self.Y(y1))}" x2="{f(self.X(x2))}" y2="{f(self.Y(y2))}" {attrs}/>')

    def text(self, x, y, s, size_mm, fill="#111", anchor="start", weight=400, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<text x="{f(self.X(x))}" y="{f(self.Y(y))}" font-size="{f(size_mm * S)}" fill="{fill}" '
            f'text-anchor="{anchor}" font-weight="{weight}" font-family="{FONT}" {attrs}>{s}</text>')

    def poly(self, pts, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        p = " ".join(f"{f(self.X(x))},{f(self.Y(y))}" for x, y in pts)
        add(f'<polygon points="{p}" {attrs}/>')

    def path(self, d_mm, **kw):
        attrs = " ".join(f'{k.replace("_", "-")}="{v}"' for k, v in kw.items())
        add(f'<path d="{d_mm}" {attrs}/>')


def label(x, y, s, size=15, fill="#2b2b2b", anchor="start", weight=400):
    add(f'<text x="{f(x)}" y="{f(y)}" font-size="{size}" fill="{fill}" text-anchor="{anchor}" '
        f'font-weight="{weight}" font-family="{FONT}">{s}</text>')


def dim_v(v, x, y1, y2, txt):
    """Vertical dimension line at case-x x (mm), from y1 to y2 (mm)."""
    X = v.X(x)
    add(f'<g stroke="#6b6b6b" stroke-width="1">'
        f'<line x1="{f(X)}" y1="{f(v.Y(y1))}" x2="{f(X)}" y2="{f(v.Y(y2))}"/>'
        f'<line x1="{f(X - 5)}" y1="{f(v.Y(y1))}" x2="{f(X + 5)}" y2="{f(v.Y(y1))}"/>'
        f'<line x1="{f(X - 5)}" y1="{f(v.Y(y2))}" x2="{f(X + 5)}" y2="{f(v.Y(y2))}"/></g>')
    cy = (v.Y(y1) + v.Y(y2)) / 2
    add(f'<text x="{f(X - 8)}" y="{f(cy)}" font-size="14" fill="#555" text-anchor="middle" '
        f'font-family="{FONT}" transform="rotate(-90 {f(X - 8)} {f(cy)})">{txt}</text>')


def dim_h(v, y, x1, x2, txt):
    Y = v.Y(y)
    add(f'<g stroke="#6b6b6b" stroke-width="1">'
        f'<line x1="{f(v.X(x1))}" y1="{f(Y)}" x2="{f(v.X(x2))}" y2="{f(Y)}"/>'
        f'<line x1="{f(v.X(x1))}" y1="{f(Y - 5)}" x2="{f(v.X(x1))}" y2="{f(Y + 5)}"/>'
        f'<line x1="{f(v.X(x2))}" y1="{f(Y - 5)}" x2="{f(v.X(x2))}" y2="{f(Y + 5)}"/></g>')
    add(f'<text x="{f((v.X(x1) + v.X(x2)) / 2)}" y="{f(Y + 18)}" font-size="14" fill="#555" '
        f'text-anchor="middle" font-family="{FONT}">{txt}</text>')


# ---------------------------------------------------------------------------
add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{CANVAS_W}" height="{CANVAS_H}" '
    f'viewBox="0 0 {CANVAS_W} {CANVAS_H}">')
add("""<defs>
  <linearGradient id="caseGrad" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#34363a"/><stop offset="0.45" stop-color="#232427"/><stop offset="1" stop-color="#141517"/>
  </linearGradient>
  <linearGradient id="bezelGrad" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#2b2d30"/><stop offset="1" stop-color="#18191b"/>
  </linearGradient>
  <linearGradient id="btnGrad" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#3a3c40"/><stop offset="1" stop-color="#121314"/>
  </linearGradient>
  <linearGradient id="cellGrad" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#1b2a4d"/><stop offset="0.5" stop-color="#0f1a33"/><stop offset="1" stop-color="#162647"/>
  </linearGradient>
  <linearGradient id="glare" x1="0" y1="0" x2="1" y2="1">
    <stop offset="0" stop-color="#ffffff" stop-opacity="0.22"/><stop offset="0.35" stop-color="#ffffff" stop-opacity="0.04"/>
    <stop offset="1" stop-color="#ffffff" stop-opacity="0"/>
  </linearGradient>
  <linearGradient id="sideGrad" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#2e3033"/><stop offset="0.5" stop-color="#1f2023"/><stop offset="1" stop-color="#151618"/>
  </linearGradient>
  <linearGradient id="chamferTop" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#3b3d41"/><stop offset="1" stop-color="#232427"/>
  </linearGradient>
  <linearGradient id="chamferBot" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#1e1f22"/><stop offset="1" stop-color="#0f1011"/>
  </linearGradient>
  <linearGradient id="bevelL" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#4a4d53"/><stop offset="1" stop-color="#26282c"/>
  </linearGradient>
  <linearGradient id="bevelR" x1="0" y1="0" x2="1" y2="0">
    <stop offset="0" stop-color="#1f2124"/><stop offset="1" stop-color="#101113"/>
  </linearGradient>
  <linearGradient id="facetGrad" x1="0" y1="0" x2="0" y2="1">
    <stop offset="0" stop-color="#1c2233"/><stop offset="1" stop-color="#0a0c12"/>
  </linearGradient>
  <filter id="shadow" x="-20%" y="-20%" width="140%" height="140%">
    <feGaussianBlur in="SourceAlpha" stdDeviation="9"/><feOffset dx="6" dy="10" result="o"/>
    <feComponentTransfer><feFuncA type="linear" slope="0.35"/></feComponentTransfer>
    <feMerge><feMergeNode/><feMergeNode in="SourceGraphic"/></feMerge>
  </filter>
  <pattern id="hatch" width="8" height="8" patternUnits="userSpaceOnUse" patternTransform="rotate(45)">
    <line x1="0" y1="0" x2="0" y2="8" stroke="#c62828" stroke-width="2" stroke-opacity="0.55"/>
  </pattern>
</defs>""")
add(f'<rect width="{CANVAS_W}" height="{CANVAS_H}" fill="#f3f1ec"/>')
label(40, 52, "GNSS Bike Computer · proposta da placa nova", 30, "#1d1d1f", weight=700)
label(40, 82, "Conceito em escala (6 px por mm), a partir da caixa impressa da V3 (docs/img/front1.png): "
      "tela na opção JDI de 8 cores, painéis solares na frente inclinada e nos chanfros laterais. Medidas em mm.", 16, "#555")

# ======================= FRONT (render) =====================================
fv = View(70, 150)
add('<g filter="url(#shadow)">')
fv.rect(0, 0, W, H, R, fill="url(#caseGrad)")
add("</g>")
fv.rect(0.5, 0.5, W - 1, H - 1, R - 0.5, fill="none", stroke="#46484d", stroke_width=1.2)
# raised bezel around the display
fv.rect(8.5, 6.5, 45, 66, 4, fill="url(#bezelGrad)", stroke="#0c0d0e", stroke_width=1.5)
fv.rect(9.0, 7.0, 44, 65, 3.6, fill="none", stroke="#3c3e42", stroke_width=1)
# display viewing area (JDI viewing 36.28 x 59.8, portrait)
vx, vy, vw, vh = 12.86, 9.6, 36.28, 59.8
fv.rect(vx, vy, vw, vh, 0.6, fill="#0e0f10")
ax, ay, aw, ah = 13.36, 10.1, 35.28, 58.8     # active area
fv.rect(ax, ay, aw, ah, 0, fill=MIP["bg"])

# --- screen content (8-colour MIP) ---
# status bar
fv.rect(ax, ay, aw, 3.9, 0, fill=MIP["black"])
fv.text(ax + aw / 2, ay + 2.85, "10:42", 2.5, MIP["white"], "middle", 700)
# satellite icon (green) + BLE/ANT (blue/cyan)
fv.circle(ax + 1.9, ay + 1.95, 0.9, fill=MIP["green"])
fv.rect(ax + 0.9, ay + 1.75, 2.0, 0.4, 0, fill=MIP["black"])
fv.text(ax + 3.3, ay + 2.8, "BLE", 1.6, MIP["cyan"], "start", 700)
fv.text(ax + 7.6, ay + 2.8, "ANT+", 1.6, MIP["cyan"], "start", 700)
# battery + sun
fv.rect(ax + aw - 6.8, ay + 1.1, 4.6, 1.8, 0.3, fill="none", stroke=MIP["white"], stroke_width=1.2)
fv.rect(ax + aw - 6.5, ay + 1.4, 3.3, 1.2, 0, fill=MIP["green"])
fv.rect(ax + aw - 2.2, ay + 1.6, 0.4, 0.8, 0, fill=MIP["white"])
fv.circle(ax + aw - 0.95, ay + 1.95, 0.55, fill=MIP["yellow"])
# speed
fv.text(ax + 1.2, ay + 6.6, "VELOCIDADE", 1.55, MIP["black"], "start", 700)
fv.text(ax + aw / 2 - 1.5, ay + 18.2, "32,4", 11.5, MIP["black"], "middle", 800)
fv.text(ax + aw - 1.0, ay + 18.2, "km/h", 2.0, MIP["black"], "end", 700)
fv.line(ax + 0.8, ay + 20.2, ax + aw - 0.8, ay + 20.2, stroke=MIP["black"], stroke_width=1.2)
fv.line(ax + aw / 2, ay + 20.2, ax + aw / 2, ay + 30.0, stroke=MIP["black"], stroke_width=1.2)
# heart rate (red) and power (blue)
hx, hy = ax + 2.6, ay + 23.0
fv.path(f"M {f(fv.X(hx))} {f(fv.Y(hy + 0.9))} "
        f"C {f(fv.X(hx - 1.3))} {f(fv.Y(hy - 0.3))} {f(fv.X(hx - 0.2))} {f(fv.Y(hy - 1.3))} {f(fv.X(hx))} {f(fv.Y(hy - 0.4))} "
        f"C {f(fv.X(hx + 0.2))} {f(fv.Y(hy - 1.3))} {f(fv.X(hx + 1.3))} {f(fv.Y(hy - 0.3))} {f(fv.X(hx))} {f(fv.Y(hy + 0.9))} Z",
        fill=MIP["red"])
fv.text(ax + 4.2, ay + 23.6, "FC", 1.6, MIP["black"], "start", 700)
fv.text(ax + aw / 4, ay + 29.0, "142", 5.2, MIP["red"], "middle", 800)
fv.poly([(ax + aw / 2 + 1.6, ay + 21.6), (ax + aw / 2 + 0.9, ay + 23.4), (ax + aw / 2 + 1.7, ay + 23.4),
         (ax + aw / 2 + 1.2, ay + 25.0), (ax + aw / 2 + 2.6, ay + 22.9), (ax + aw / 2 + 1.8, ay + 22.9),
         (ax + aw / 2 + 2.4, ay + 21.6)], fill=MIP["yellow"])
fv.text(ax + aw / 2 + 3.2, ay + 23.6, "POT", 1.6, MIP["black"], "start", 700)
fv.text(ax + 3 * aw / 4, ay + 29.0, "215", 5.2, MIP["blue"], "middle", 800)
fv.text(ax + aw - 0.9, ay + 29.0, "W", 1.8, MIP["blue"], "end", 700)
fv.line(ax + 0.8, ay + 30.0, ax + aw - 0.8, ay + 30.0, stroke=MIP["black"], stroke_width=1.2)
# distance and climb
fv.text(ax + 1.2, ay + 32.4, "DIST", 1.5, MIP["black"], "start", 700)
fv.text(ax + 1.2, ay + 36.3, "48,7 km", 3.2, MIP["black"], "start", 800)
fv.text(ax + aw - 1.2, ay + 32.4, "SUBIDA", 1.5, MIP["black"], "end", 700)
fv.text(ax + aw - 1.2, ay + 36.3, "620 m", 3.2, MIP["green"], "end", 800)
# segment block
fv.rect(ax + 0.8, ay + 38.0, aw - 1.6, 9.4, 0.4, fill=MIP["white"], stroke=MIP["magenta"], stroke_width=1.4)
fv.text(ax + 1.8, ay + 40.6, "SEGMENTO · Serra do Mar", 1.6, MIP["magenta"], "start", 700)
fv.rect(ax + 1.8, ay + 42.0, aw - 3.6, 1.8, 0.3, fill="#c9ccc5")
fv.rect(ax + 1.8, ay + 42.0, (aw - 3.6) * 0.62, 1.8, 0.3, fill=MIP["green"])
fv.rect(ax + 1.8 + (aw - 3.6) * 0.57, ay + 41.4, 0.5, 3.0, 0, fill=MIP["red"])
fv.text(ax + 1.8, ay + 46.6, "62 %", 1.9, MIP["black"], "start", 700)
fv.text(ax + aw - 1.8, ay + 46.6, "-0:12 do PR", 1.9, MIP["green"], "end", 800)
# elevation profile
base = ay + ah - 1.0
pts = [(ax + 0.8, base), (ax + 0.8, base - 2.0), (ax + 5, base - 3.0), (ax + 9, base - 2.4), (ax + 13, base - 5.0),
       (ax + 17, base - 6.8), (ax + 21, base - 5.6), (ax + 25, base - 7.8), (ax + 29, base - 9.2),
       (ax + 32, base - 7.4), (ax + aw - 0.8, base - 6.0), (ax + aw - 0.8, base)]
fv.poly(pts, fill=MIP["cyan"], stroke=MIP["black"], stroke_width=1.2)
fv.circle(ax + 17, base - 6.8, 0.75, fill=MIP["red"], stroke=MIP["black"], stroke_width=0.8)
fv.text(ax + 1.2, ay + 50.4, "ALTIMETRIA", 1.5, MIP["black"], "start", 700)
# glass glare
fv.rect(vx, vy, vw, vh, 0.6, fill="url(#glare)")
# Solar: six 3-cell modules of 23 x 8 mm (ANYSOLAR KXOB25-05X3F class, IBC cells with a uniform
# dark front): two on the angled facet below the display, two on each 45-degree side chamfer.


def module(v, x, y, w, h, vertical):
    """A 3-cell module seen from the front; vertical modules sit on the 45-degree chamfers."""
    v.rect(x, y, w, h, 0.5, fill="#0a0d14", stroke="#3a3f4a", stroke_width=0.8)
    for i in range(3):
        if vertical:
            v.rect(x + 0.35, y + 0.35 + i * (h - 0.7) / 3, w - 0.7, (h - 0.7) / 3 - 0.3, 0.2, fill="url(#cellGrad)")
        else:
            v.rect(x + 0.35 + i * (w - 0.7) / 3, y + 0.35, (w - 0.7) / 3 - 0.3, h - 0.7, 0.2, fill="url(#cellGrad)")


# 45-degree chamfers along the long front edges (8 mm modules project to about 5.7 mm)
fv.rect(0.6, 17.5, 6.2, 55.0, 2.8, fill="url(#bevelL)")
fv.rect(55.2, 17.5, 6.2, 55.0, 2.8, fill="url(#bevelR)")
for x0 in (0.95, 55.55):
    for y0 in (21.5, 45.5):
        module(fv, x0, y0, 5.5, 23.0, True)
# angled facet below the display
fv.rect(7.5, 74.6, 47.0, 14.2, 1.6, fill="url(#facetGrad)", stroke="#34373d", stroke_width=1.2)
for x0 in (7.9, 31.1):
    module(fv, x0, 77.6, 23.0, 8.0, False)
fv.rect(7.5, 74.6, 47.0, 14.2, 1.6, fill="url(#glare)")
# buttons, V3 style: round keys joined by short grooves
for bx in (16.0, 31.0, 46.0):
    fv.circle(bx, 95.0, 5.1, fill="#0d0e0f")
    fv.circle(bx, 95.0, 4.4, fill="url(#btnGrad)", stroke="#4a4c51", stroke_width=1)
for gx in (21.1, 36.1):
    fv.rect(gx, 94.4, 4.8, 1.2, 0.5, fill="#0d0e0f")
# LED light pipe (top right, where the V3 has its hole); ambient light window at the bottom left
fv.circle(55.2, 4.6, 1.0, fill="#0d0e0f")
fv.circle(55.2, 4.6, 0.6, fill="#4ade80")
fv.rect(3.4, 91.6, 2.6, 1.6, 0.4, fill="#1d2f3a", stroke="#0b0c0d", stroke_width=0.8)
dim_v(fv, -3.2, 0, H, "104")
dim_h(fv, H + 3.0, 0, W, "62")
label(fv.X(W / 2), fv.Y(H) + 58, "Frente", 18, "#1d1d1f", "middle", 700)

# ======================= SIDE (right) =======================================
sv = View(560, 150)
sv.rect(0, 0, T, H, 3.2, fill="url(#sideGrad)", stroke="#0e0f10", stroke_width=1.2)
sv.line(7.0, 0.6, 7.0, H - 0.6, stroke="#0b0b0c", stroke_width=1.6)          # parting line
sv.rect(-1.2, 6.5, 1.4, 66, 0.5, fill="#26282b", stroke="#0e0f10", stroke_width=1)   # raised bezel
for by in (95.0,):
    sv.rect(-1.0, by - 4.4, 1.2, 8.8, 0.5, fill="#2f3135", stroke="#0e0f10", stroke_width=1)  # button stack (profile)
# 45-degree chamfer along the front edge with the two side modules
sv.rect(0.0, 17.5, 5.7, 55.0, 1.5, fill="#34373c", stroke="#0e0f10", stroke_width=1)
for y0 in (21.5, 45.5):
    module(sv, 0.1, y0, 5.5, 23.0, True)
# angled facet below the display: the front recedes about 2.5 mm toward the buttons
sv.poly([(0.0, 74.6), (2.5, 88.8), (0.0, 88.8)], fill="#0f1218", stroke="#3a3f4a", stroke_width=1)
# quarter-turn mount on the back
sv.rect(T, 30.0, MOUNT, 30.0, 0.8, fill="#1c1d20", stroke="#0e0f10", stroke_width=1)
dim_h(sv, H + 3.0, 0, T, "19")
dim_h(sv, 26.0, T, T + MOUNT, "")
label(sv.X(T + MOUNT + 1.5), sv.Y(45), "+3", 14, "#555")
label(sv.X(T / 2), sv.Y(H) + 58, "Lateral direita", 18, "#1d1d1f", "middle", 700)
label(sv.X(-1.5), sv.Y(-2.0), "frente", 13, "#777", "start")
label(sv.X(T + MOUNT), sv.Y(-2.0), "trás", 13, "#777", "end")

# ======================= BACK ===============================================
bv = View(760, 150)
add('<g filter="url(#shadow)">')
bv.rect(0, 0, W, H, R, fill="#1c1d20")
add("</g>")
add(f'<defs><clipPath id="backClip"><rect x="{f(bv.X(0))}" y="{f(bv.Y(0))}" width="{f(W * S)}" '
    f'height="{f(H * S)}" rx="{f(R * S)}" ry="{f(R * S)}"/></clipPath></defs>')
add('<g clip-path="url(#backClip)">')
bv.rect(0, 0, W, 9, 0, fill="url(#chamferTop)")
bv.rect(0, H - 9, W, 9, 0, fill="url(#chamferBot)")
bv.line(0, 9, W, 9, stroke="#101113", stroke_width=1.2)
bv.line(0, H - 9, W, H - 9, stroke="#3a3c41", stroke_width=1)
add("</g>")
bv.rect(0.5, 0.5, W - 1, H - 1, R - 0.5, fill="none", stroke="#3e4045", stroke_width=1.2)
# quarter-turn mount (as on the V3)
bv.circle(31, 45, 15.5, fill="#16171a", stroke="#3a3c41", stroke_width=1.5)
bv.circle(31, 45, 12.8, fill="#202124", stroke="#0b0c0d", stroke_width=1.2)
bv.poly([(31, 45), (22.0, 36.0), (40.0, 36.0)], fill="#2a2c30")
bv.poly([(31, 45), (22.0, 54.0), (40.0, 54.0)], fill="#2a2c30")
bv.rect(18.6, 44.2, 4.6, 1.6, 0.4, fill="#0b0c0d")
bv.rect(38.8, 44.2, 4.6, 1.6, 0.4, fill="#0b0c0d")
bv.circle(31, 45, 1.3, fill="#0b0c0d")
# barometer vent with membrane
bv.circle(W - 9.6, 88.0, 2.2, fill="#111214", stroke="#4b4e53", stroke_width=1)
for dx, dy in ((-0.7, -0.7), (0.7, -0.7), (-0.7, 0.7), (0.7, 0.7), (0, 0)):
    bv.circle(W - 9.6 + dx, 88.0 + dy, 0.28, fill="#6a6e75")
# screws
for sx, sy in ((6.5, 12.5), (55.5, 12.5), (6.5, 91.5), (55.5, 91.5)):
    bv.circle(sx, sy, 1.5, fill="#2c2e32", stroke="#0b0c0d", stroke_width=1)
    bv.line(sx - 0.8, sy, sx + 0.8, sy, stroke="#0b0c0d", stroke_width=1.2)
    bv.line(sx, sy - 0.8, sx, sy + 0.8, stroke="#0b0c0d", stroke_width=1.2)
# seen from the back the device's left side is on the right: microSD door there, low;
# IPX8 USB-C in the middle of the bottom edge, sealed by its ring, no flap
bv.rect(W - 1.6, 71.0, 3.2, 16.0, 1.2, fill="#232428", stroke="#46494e", stroke_width=1)
bv.rect(26.2, H - 1.4, 9.6, 2.8, 1.2, fill="none", stroke="#46494e", stroke_width=1)
bv.rect(27.4, H - 0.9, 7.2, 1.8, 0.8, fill="#050505")
bv.text(31, 70.0, "GNSS BIKE COMPUTER", 2.0, "#3a3c41", "middle", 700, letter_spacing="1")
label(bv.X(W / 2), bv.Y(H) + 58, "Traseira", 18, "#1d1d1f", "middle", 700)
# callouts for the back
label(bv.X(W) + 14, bv.Y(78.0), "tampa do", 13, "#555")
label(bv.X(W) + 14, bv.Y(78.0) + 16, "microSD", 13, "#555")
label(bv.X(W / 2), bv.Y(H) + 26, "USB-C IPX8 na base, sem tampa", 13, "#555", "middle")

# ======================= X-RAY (component layout) ==========================
xv = View(1230, 150)
xv.rect(0, 0, W, H, R, fill="#e9edf1", stroke="#1d1d1f", stroke_width=1.6)
xv.rect(3.5, 3.5, W - 7, H - 7, 3, fill="#cfe6d1", stroke="#2e7d32", stroke_width=1.4)    # PCB
xv.rect(13.0, 26.0, 36.0, 60.0, 2, fill="#ffe0b2", fill_opacity="0.55", stroke="#ef6c00",
        stroke_width=1.5, stroke_dasharray="8 5")                                            # battery (behind)
xv.rect(10.96, 8.6, 40.08, 61.8, 1, fill="none", stroke="#546e7a", stroke_width=1.3,
        stroke_dasharray="4 4")                                                              # display outline
# solar modules: angled facet below the display and the two side chamfers
for x0 in (7.9, 31.1):
    xv.rect(x0, 77.6, 23.0, 8.0, 0.5, fill="#c5cae9", fill_opacity="0.7", stroke="#1a237e", stroke_width=1.2)
for x0 in (0.95, 55.55):
    for y0 in (21.5, 45.5):
        xv.rect(x0, y0, 5.5, 23.0, 0.5, fill="#c5cae9", fill_opacity="0.7", stroke="#1a237e", stroke_width=1.2)
# GNSS: module under a shield at the top, L1 and L5 elements on the top wall
xv.rect(24.2, 5.8, 13.6, 13.4, 0.8, fill="#cfd8dc", stroke="#607d8b", stroke_width=1.2)
xv.rect(26.0, 7.6, 10.1, 9.7, 0.6, fill="#90a4ae", stroke="#37474f", stroke_width=1)
xv.line(6.0, 1.3, 28.5, 1.3, stroke="#ff9800", stroke_width=5, stroke_linecap="round")       # L1
xv.line(33.5, 1.3, 56.0, 1.3, stroke="#e65100", stroke_width=5, stroke_linecap="round")      # L5
xv.circle(27.0, 3.3, 0.7, fill="#ff9800")
xv.circle(35.0, 3.3, 0.7, fill="#e65100")
# BM20C module (BLE + ANT), 10.0 x 16.2 mm: the last 5.5 mm are the antenna area, in the
# bottom-right corner, opposite the GNSS and outside the panel area, with a copper-free zone
xv.rect(48.5, 75.1, 10.0, 16.2, 0.6, fill="#ffcdd2", stroke="#c62828", stroke_width=1.2)
xv.rect(48.5, 85.8, 10.0, 8.9, 0.5, fill="url(#hatch)", stroke="#c62828", stroke_width=1)
# display FPC to the left, away from the antennas
xv.rect(7.2, 33.0, 3.4, 10.0, 0.4, fill="#b0bec5", stroke="#455a64", stroke_width=1)
# microSD low on the left side (door on the left wall)
xv.rect(3.8, 71.0, 14.0, 15.0, 0.8, fill="#e1bee7", stroke="#6a1b9a", stroke_width=1.2)
# power: USB-C on the bottom edge, nPM1300 and MAX17262 next to it, AEM10900 by the panel connectors
xv.rect(26.5, 98.8, 9.0, 3.6, 1.2, fill="#bbdefb", stroke="#0d47a1", stroke_width=1.2)
xv.rect(36.3, 88.0, 4.6, 4.6, 0.4, fill="#90caf9", stroke="#0d47a1", stroke_width=1)
xv.rect(21.4, 88.6, 2.5, 2.5, 0.3, fill="#90caf9", stroke="#0d47a1", stroke_width=1)
xv.rect(20.0, 71.4, 4.0, 4.0, 0.3, fill="#c5cae9", stroke="#1a237e", stroke_width=1)
xv.rect(25.5, 71.8, 10.0, 2.6, 0.3, fill="#9fa8da", stroke="#1a237e", stroke_width=1)
# sensors: IMU and magnetometer, baro by the vent, light sensor at the top window, LED at the light pipe
xv.rect(12.0, 24.0, 3.0, 2.5, 0.2, fill="#fff59d", stroke="#827717", stroke_width=1)
xv.rect(16.0, 24.2, 2.2, 2.2, 0.2, fill="#fff59d", stroke="#827717", stroke_width=1)
xv.rect(8.0, 86.6, 3.25, 3.25, 0.2, fill="#fff59d", stroke="#827717", stroke_width=1)
xv.rect(3.9, 91.8, 2.0, 1.2, 0.2, fill="#fff59d", stroke="#827717", stroke_width=1)
xv.circle(55.2, 4.6, 0.9, fill="#a5d6a7", stroke="#1b5e20", stroke_width=1)
# buzzer and buttons
xv.rect(14.0, 50.0, 11.0, 9.0, 0.8, fill="#d7ccc8", stroke="#4e342e", stroke_width=1)
for bx in (16.0, 31.0, 46.0):
    xv.rect(bx - 3, 92.0, 6.0, 6.0, 0.5, fill="#eeeeee", stroke="#424242", stroke_width=1)
    xv.circle(bx, 95.0, 1.6, fill="#bdbdbd", stroke="#424242", stroke_width=0.8)

numbers = [
    (31.0, 22.2, "1"), (17.0, 4.2, "2"), (45.0, 4.2, "2"), (53.5, 72.6, "3"), (9.0, 46.0, "4"),
    (5.8, 68.6, "5"), (31.0, 44.0, "6"), (38.6, 95.2, "7"), (22.0, 69.0, "8"), (15.0, 29.6, "9"),
    (8.4, 95.6, "10"), (19.5, 62.0, "11"), (23.5, 99.6, "12"), (14.4, 87.6, "13"),
    (31.0, 75.6, "14"), (58.3, 18.6, "14"),
]
for nx, ny, s in numbers:
    xv.circle(nx, ny, 1.9, fill="#1d1d1f")
    xv.text(nx, ny + 0.75, s, 2.0, "#ffffff", "middle", 700)
label(xv.X(W / 2), xv.Y(H) + 58, "Por dentro (raio X)", 18, "#1d1d1f", "middle", 700)

# ======================= LEGEND =============================================
lx, ly = 1660, 150
label(lx, ly, "Por dentro", 19, "#1d1d1f", weight=700)
items = [
    ("1", "GNSS u-blox MAX-M10N-10B sob blindagem"),
    ("2", "antenas GNSS L1 e L5 na parede de cima,"),
    ("", "longe dos painéis, com contatos de mola"),
    ("3", "BM20C (nRF54LM20A, BLE + ANT+): antena"),
    ("", "de 2,4 GHz no canto oposto ao GNSS"),
    ("4", "FPC do display sai pela esquerda"),
    ("5", "microSD só no protótipo; depois SD NAND"),
    ("6", "LiPo 2000 mAh 36 × 60 × 7 mm, atrás da placa"),
    ("7", "USB-C na base, nPM1300 e MAX17262"),
    ("8", "AEM10900 e conectores dos painéis"),
    ("9", "BMI270 e MMC5633NJL"),
    ("10", "LED RGB em cima (como na V3), OPT3001"),
    ("", "embaixo à esquerda, longe das antenas"),
    ("11", "buzzer piezo"),
    ("12", "três botões, como na V3"),
    ("13", "BMP585 junto do respiro com membrana,"),
    ("", "fora da sombra da bateria"),
    ("14", "painéis: 2 na frente inclinada e 2 em"),
    ("", "cada chanfro lateral de 45°"),
]
y = ly + 34
for n, s in items:
    if n:
        add(f'<circle cx="{lx + 10}" cy="{y - 5}" r="10" fill="#1d1d1f"/>')
        label(lx + 10, y, n, 12, "#fff", "middle", 700)
    label(lx + 28, y, s, 14, "#333")
    y += 24
y += 16
label(lx, y, "Medidas e escolhas", 19, "#1d1d1f", weight=700)
y += 30
facts = [
    "62 × 104 × 19 mm, mais 3 mm do encaixe;",
    "a V3 tem cerca de 60 × 85 mm",
    "tela Sharp 2,7\" 400 × 240 com luz frontal;",
    "a imagem mostra a opção JDI de 8 cores",
    "6 módulos solares de 3 células, 23 × 8 mm",
    "(11 cm² de células), todos em paralelo;",
    "cada um dá cerca de 2 V; o AEM10900 carrega",
    "a LiPo até 3,9 V e o USB, até 4,2 V; os 3,0 V",
    "e 1,8 V saem do nPM1300",
    "USB-C IPX8 sem tampa, respiro com",
    "membrana",
]
for s in facts:
    label(lx, y, s, 14, "#333")
    y += 22
y += 14
label(lx, y, "Conceito para discutir: não há", 14, "#b71c1c", weight=700)
y += 20
label(lx, y, "projeto mecânico nem layout ainda.", 14, "#b71c1c", weight=700)

add("</svg>")
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text("\n".join(out) + "\n", encoding="utf-8", newline="\n")
print(OUT, sum(len(s) for s in out))
