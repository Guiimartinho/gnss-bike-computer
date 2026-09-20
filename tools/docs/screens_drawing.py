"""Draw the screen mockups of docs/18-interface-telas.md as SVG contact sheets.

Every screen is 240 x 400 px in portrait, like the legacy LS027 layout, and uses only the eight
colours of the JDI LPM027M128C memory LCD (1 bit per channel). The colours are the muted tones a
reflective panel shows, the same as tools/docs/case_drawing.py. Layouts follow the legacy grid of
2 columns x 7 rows (legacy/source/vue/Vue.cpp) with a status bar on top.

Usage: python tools/docs/screens_drawing.py [output_dir]   (default: docs/img/telas)
"""
import math
import pathlib
import random
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT_DIR = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "docs" / "img" / "telas"

SW, SH = 240, 400          # screen size in portrait (px)
BAR = 20                   # status bar height (px)
ROWS = 7                   # legacy grid rows
SCALE = 1.5                # screen scale on the sheets
GAP = 44                   # space between screens on a sheet
CAPTION = 46               # caption band above each screen
FONT = "'DejaVu Sans Mono', Consolas, 'Courier New', monospace"
CAPTION_FONT = "Inter, 'Segoe UI', Roboto, Arial, sans-serif"

# The eight panel colours (muted as on a reflective MIP panel)
K = "#16181a"   # black
W = "#f4f5f0"   # white
R = "#c8323c"   # red
G = "#2e9d4a"   # green
B = "#2f5fb8"   # blue
Y = "#d9a900"   # yellow
C = "#2aa3b5"   # cyan
M = "#a83a95"   # magenta


def f(v):
    return f"{v:.2f}".rstrip("0").rstrip(".")


class Screen:
    """Collects the SVG elements of one 240 x 400 screen."""

    def __init__(self, name, caption):
        self.name, self.caption, self.el = name, caption, []

    def add(self, s):
        self.el.append(s)

    def rect(self, x, y, w, h, fill=W, stroke=None, sw=1):
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
        self.add(f'<rect x="{f(x)}" y="{f(y)}" width="{f(w)}" height="{f(h)}" fill="{fill}"{st}/>')

    def line(self, x1, y1, x2, y2, color=K, sw=1, dash=None):
        d = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<line x1="{f(x1)}" y1="{f(y1)}" x2="{f(x2)}" y2="{f(y2)}" stroke="{color}" '
                 f'stroke-width="{sw}"{d}/>')

    def text(self, x, y, s, size, color=K, anchor="start", weight=700):
        self.add(f'<text x="{f(x)}" y="{f(y)}" font-size="{f(size)}" fill="{color}" text-anchor="{anchor}" '
                 f'font-weight="{weight}" font-family="{FONT}">{s}</text>')

    def poly(self, pts, fill=K, stroke=None, sw=1):
        p = " ".join(f"{f(x)},{f(y)}" for x, y in pts)
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
        self.add(f'<polygon points="{p}" fill="{fill}"{st}/>')

    def polyline(self, pts, color=K, sw=2):
        p = " ".join(f"{f(x)},{f(y)}" for x, y in pts)
        self.add(f'<polyline points="{p}" fill="none" stroke="{color}" stroke-width="{sw}" '
                 f'stroke-linejoin="round" stroke-linecap="round"/>')

    def circle(self, cx, cy, r, fill="none", stroke=K, sw=1):
        st = f' stroke="{stroke}" stroke-width="{sw}"' if stroke else ""
        self.add(f'<circle cx="{f(cx)}" cy="{f(cy)}" r="{f(r)}" fill="{fill}"{st}/>')


# ---------------------------------------------------------------- building blocks

def status_bar(s, time="07:42", gnss=G, rec=True, solar=True, usb=False, batt=91):
    """Black bar with time, GNSS, radio links, recording, solar or USB, and battery."""
    s.rect(0, 0, SW, BAR, K)
    s.text(4, 15, time, 13, W)
    s.text(52, 15, "GPS", 11, gnss)
    s.text(80, 15, "ANT+", 11, C)
    s.text(114, 15, "BLE", 11, C)
    if rec:
        s.circle(146, 10, 4, R, None)
    if solar:
        s.circle(166, 10, 3.2, Y, None)
        for a in range(0, 360, 45):
            ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
            s.line(166 + 4.8 * ca, 10 + 4.8 * sa, 166 + 7 * ca, 10 + 7 * sa, Y, 1.2)
    if usb:
        s.rect(158, 7, 12, 6, Y)
        s.rect(170, 8.5, 3, 3, Y)
    fill = G if batt > 50 else (Y if batt > 20 else R)
    s.rect(182, 5, 30, 10, K, W, 1.2)
    s.rect(212, 8, 2.5, 4, W)
    s.rect(184, 7, 26 * batt / 100, 6, fill)
    s.text(236, 15, f"{batt}", 11, W, "end")


def row_y(i):
    """Top of legacy grid row i (0..6) under the status bar."""
    h = (SH - BAR) / ROWS
    return BAR + i * h, h


def field(s, col, row, label, value, unit, vcolor=K, span=1, lcolor=K):
    """Legacy 'cadran': small label on the left, unit on the right, big value centred."""
    y, h = row_y(row)
    x = col * SW / 2
    w = SW / 2 * span
    s.rect(x, y, w, h, W, K, 1)
    s.text(x + 4, y + 12, label, 10, lcolor, weight=400)
    s.text(x + w - 4, y + 12, unit, 10, K, "end", weight=400)
    s.text(x + w / 2, y + h - 9, value, 28 if span == 1 else 32, vcolor, "middle")


def band(s, row, nrows=1, fill=W):
    y, h = row_y(row)
    s.rect(0, y, SW, h * nrows, fill, K, 1)
    return y, h * nrows


def segment_map(s, row, pct, delta, ahead):
    """Mini-map of an active segment over two rows: track, rider arrow, % done and gap."""
    y, h = band(s, row, 2)
    pts = [(18, y + 20), (46, y + 32), (70, y + 30), (96, y + 50), (124, y + 58),
           (150, y + 70), (180, y + 72), (204, y + 88), (222, y + 94)]
    s.polyline(pts, M, 3)
    s.circle(222, y + 94, 5, "none", K, 1.5)
    rx, ry = 124, y + 58
    s.poly([(rx - 8, ry + 6), (rx + 9, ry - 1), (rx - 4, ry - 8)], K)
    s.text(8, y + 18, f"{pct}%", 14, K)
    s.text(150, y + h - 10, delta, 18, G if ahead else R, "middle")


def partner(s, row, ratio, delta, ahead):
    """Legacy 'partner': marker between -25 % and +25 % of the record pace."""
    y, h = band(s, row)
    cy = y + h / 2 - 4
    s.line(20, cy, 220, cy, K, 3)
    for x in (20, 70, 120, 170, 220):
        s.line(x, cy - 5, x, cy + 5, K, 2)
    mx = 120 + ratio * 400
    col = G if ahead else R
    s.poly([(mx, cy - 3), (mx - 9, cy - 16), (mx + 9, cy - 16)], col)
    s.text(mx, cy + 20, delta, 13, col, "middle")


def title_bar(s, title):
    s.rect(0, BAR, SW, 28, B)
    s.text(SW / 2, BAR + 20, title, 15, W, "middle")


def menu_list(s, items, selected, top=BAR + 36, step=34):
    """Legacy menu: selected item as a bar (white text on black in the legacy, blue here)."""
    for i, (txt, color) in enumerate(items):
        y = top + i * step
        if i == selected:
            s.rect(6, y - 4, SW - 12, step - 6, K)
            s.text(16, y + step / 2 + 2, txt, 15, W)
        else:
            s.text(16, y + step / 2 + 2, txt, 15, color)


# ---------------------------------------------------------------- screens

def crs1():
    s = Screen("crs1", "CRS, página 1 (sem segmento)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    field(s, 0, 0, "Dist", "18.2", "km")
    field(s, 1, 0, "Pot", "222", "W")
    field(s, 0, 1, "Vel", "20.0", "km/h")
    field(s, 1, 1, "Subida", "575", "m")
    field(s, 0, 2, "Cad", "88", "rpm")
    field(s, 1, 2, "FC", "147", "bpm")
    field(s, 0, 3, "Incl", "4", "%")
    field(s, 1, 3, "VA", "0.21", "m/s")
    field(s, 0, 4, "Próx. segmento", "875", "m", span=2, lcolor=M)
    field(s, 0, 5, "Média", "23.4", "km/h")
    field(s, 1, 5, "Score", "0.2", "")
    field(s, 0, 6, "Solar", "18", "mW", G)
    field(s, 1, 6, "Bat", "91", "%")
    return s


def crs1_seg():
    s = Screen("crs1_seg", "CRS, página 1 (1 segmento)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    field(s, 0, 0, "Dist", "18.2", "km")
    field(s, 1, 0, "Pot", "222", "W")
    field(s, 0, 1, "Vel", "20.0", "km/h")
    field(s, 1, 1, "Subida", "575", "m")
    field(s, 0, 2, "Cad", "88", "rpm")
    field(s, 1, 2, "FC", "147", "bpm")
    field(s, 0, 3, "Incl", "7", "%")
    field(s, 1, 3, "VA", "0.34", "m/s")
    segment_map(s, 4, 45, "+12.4 s", True)
    partner(s, 6, 0.06, "+6 %", True)
    return s


def crs1_2seg():
    s = Screen("crs1_2seg", "CRS, página 1 (2 segmentos)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    field(s, 0, 0, "VA", "0.0", "m/s")
    field(s, 1, 0, "FC", "152", "bpm")
    segment_map(s, 1, 45, "-72.2 s", False)
    partner(s, 3, -0.2, "-20 %", False)
    segment_map(s, 4, 35, "+0.9 s", True)
    partner(s, 6, 0.01, "+1 %", True)
    return s


def crs2():
    s = Screen("crs2", "CRS, página 2 (navegação e RR)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    field(s, 0, 0, "Dist", "18.2", "km")
    field(s, 1, 0, "Vel", "20.0", "km/h")
    field(s, 0, 1, "Próxima curva", "350", "m", span=2, lcolor=B)
    y, h = band(s, 2, 3)
    cx, cy = SW / 2, y + h / 2 + 4
    s.poly([(cx - 20, cy + 50), (cx - 20, cy - 8), (cx + 18, cy - 8), (cx + 18, cy - 28),
            (cx + 52, cy + 4), (cx + 18, cy + 36), (cx + 18, cy + 16), (cx + 4, cy + 16),
            (cx + 4, cy + 50)], B)
    s.text(cx, y + 18, "Rua das Flores", 13, K, "middle")
    y, h = band(s, 5, 2)
    s.text(6, y + 14, "RR por zona (ms)", 10, K, weight=400)
    vals = [42, 38, 31, 24, 18]
    cols = [B, G, Y, M, R]
    for i, (v, col) in enumerate(zip(vals, cols)):
        bx = 20 + i * 42
        s.rect(bx, y + h - 14 - v * 1.6, 28, v * 1.6, col)
        s.text(bx + 14, y + h - 3, f"Z{i + 1}", 10, K, "middle", 400)
    s.text(20 + 2 * 42 + 14, y + h - 14 - 31 * 1.6 - 4, ">", 14, K, "middle")
    return s


def crs3():
    s = Screen("crs3", "CRS, página 3 (inclinação, bússola, rugosidade)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    y, h = band(s, 0, 2)
    s.text(6, y + 14, "Inclinação", 10, K, weight=400)
    s.text(SW / 2, y + 60, "7.2 %", 34, K, "middle")
    s.rect(20, y + h - 22, 200, 10, W, K, 1)
    s.rect(120, y + h - 22, 36, 10, R)
    y, h = band(s, 2, 2)
    s.text(6, y + 14, "Histograma da inclinação", 10, K, weight=400)
    base = y + h / 2 + 8
    s.line(10, base, 230, base, K, 1, "4 3")
    rnd = random.Random(7)
    for i in range(36):
        v = rnd.uniform(-1, 1) * 30 + 12 * math.sin(i / 5)
        x = 12 + i * 6
        col = R if v > 0 else G
        top = base - max(v, 0)
        s.rect(x, top, 4, abs(v), col)
    y, h = band(s, 4, 2)
    cx, cy, r = 70, y + h / 2 + 4, 40
    s.circle(cx, cy, r, "none", K, 2)
    s.text(cx, cy - r + 14, "N", 12, R, "middle")
    ang = math.radians(35)
    s.poly([(cx + (r - 8) * math.sin(ang), cy - (r - 8) * math.cos(ang)),
            (cx + 8 * math.cos(ang), cy + 8 * math.sin(ang)),
            (cx - 8 * math.cos(ang), cy - 8 * math.sin(ang))], K)
    s.text(175, cy - 12, "Rumo", 10, K, "middle", 400)
    s.text(175, cy + 16, "035°", 22, K, "middle")
    y, h = band(s, 6)
    s.text(6, y + 14, "Rugosidade", 10, K, weight=400)
    for i, (v, col) in enumerate([(0.6, G), (0.3, Y), (0.1, R)]):
        s.rect(110 + i * 40, y + h - 8 - v * 36, 28, v * 36, col)
    return s


def prc():
    s = Screen("prc", "PRC, percurso com mapa")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    field(s, 0, 0, "Dist", "42.7", "km")
    field(s, 1, 0, "Pot", "198", "W")
    field(s, 0, 1, "Vel", "24.8", "km/h")
    field(s, 1, 1, "Subida", "812", "m")
    field(s, 0, 2, "FC", "139", "bpm")
    field(s, 1, 2, "Restam", "31.5", "km")
    y, h = band(s, 3, 3)
    route = [(10, y + 150), (40, y + 128), (62, y + 132), (84, y + 104), (110, y + 96),
             (126, y + 70), (150, y + 64), (176, y + 40), (206, y + 34), (230, y + 12)]
    s.polyline(route, B, 4)
    s.polyline(route[:5], G, 4)
    rx, ry = 110, y + 96
    s.poly([(rx - 8, ry + 8), (rx + 10, ry - 2), (rx - 2, ry - 10)], K)
    s.line(176, y + h - 10, 224, y + h - 10, K, 2)
    s.text(200, y + h - 14, "250 m", 10, K, "middle", 400)
    field(s, 0, 6, "Média", "22.1", "km/h")
    field(s, 1, 6, "Bat", "88", "%")
    return s


def fec():
    s = Screen("fec", "FEC, rolo")
    s.rect(0, 0, SW, SH, W)
    status_bar(s, gnss=K, solar=False)
    field(s, 0, 0, "Tempo", "00:42:17", "", span=2)
    field(s, 0, 1, "Cad", "92", "rpm")
    field(s, 1, 1, "FC", "151", "bpm")
    field(s, 0, 2, "Score", "12.3", "")
    field(s, 1, 2, "Zona", "4", "", Y)
    field(s, 0, 3, "Pot", "245", "W")
    field(s, 1, 3, "RR", "38", "ms")
    y, h = band(s, 4, 3)
    s.text(6, y + 14, "Zonas de potência", 10, K, weight=400)
    zones = [(8, B), (22, C), (30, G), (26, Y), (12, M), (6, R), (2, K)]
    for i, (v, col) in enumerate(zones):
        bx = 10 + i * 17
        s.rect(bx, y + h - 20 - v * 3.4, 12, v * 3.4, col)
        s.text(bx + 6, y + h - 6, str(i + 1), 9, K, "middle", 400)
    cx, cy, r = 180, y + h / 2 + 6, 46
    s.circle(cx, cy, r, "none", K, 1)
    s.line(cx - r, cy, cx + r, cy, K, 1, "3 3")
    s.line(cx, cy - r, cx, cy + r, K, 1, "3 3")
    pts = []
    for i in range(24):
        a = 2 * math.pi * i / 24
        rr = r * (0.35 + 0.55 * abs(math.sin(a + 0.3)))
        pts.append((cx + rr * math.sin(a), cy - rr * math.cos(a)))
    s.poly(pts, "none", M, 2)
    s.text(cx, y + 14, "Vetor", 10, K, "middle", 400)
    return s


def gps():
    s = Screen("gps", "GNSS procurando (posição com mais de 6 s)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s, gnss=Y, rec=False)
    s.text(SW / 2, BAR + 34, "Procurando satélites", 16, K, "middle")
    cx, cy, r = SW / 2, 170, 88
    s.circle(cx, cy, r, "none", K, 1.5)
    s.circle(cx, cy, r * 0.5, "none", K, 1)
    s.line(cx - r, cy, cx + r, cy, K, 1, "3 3")
    s.line(cx, cy - r, cx, cy + r, K, 1, "3 3")
    s.text(cx, cy - r - 4, "N", 11, R, "middle")
    sats = [(40, 0.3, G), (110, 0.6, G), (200, 0.4, Y), (260, 0.8, R), (320, 0.5, G),
            (15, 0.85, R), (150, 0.2, G), (285, 0.35, Y)]
    for az, el, col in sats:
        a = math.radians(az)
        s.circle(cx + r * el * math.sin(a), cy - r * el * math.cos(a), 6, col, K, 1)
    s.text(cx, 290, "5 de 8", 30, K, "middle")
    s.text(cx, 312, "satélites em uso", 12, K, "middle", 400)
    s.text(cx, 346, "Modo: potência plena", 13, B, "middle")
    s.text(cx, 370, "Última posição há 8 s", 13, K, "middle", 400)
    return s


def dbg():
    s = Screen("dbg", "DBG, diagnóstico")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    lines = [
        ("GNSS", "LEAP, fix 3D", G),
        ("Satélites", "18 (GPS 7, GAL 6, BDS 5)", K),
        ("Idade da posição", "0,4 s", K),
        ("Precisão", "2,1 m", K),
        ("Bateria", "3,92 V  -42 mA  91 %", K),
        ("Carga", "solar, 18 mW", G),
        ("Temperatura", "31 °C", K),
        ("Segmentos", "12 carregados", M),
        ("Versão", "3.0.0", K),
    ]
    y = BAR + 22
    for k, v, col in lines:
        s.text(8, y, k, 11, K, weight=400)
        s.text(232, y, v, 11, col, "end")
        y += 24
    y += 4
    s.text(8, y, "C/N0 (dB-Hz)", 10, K, weight=400)
    vals = [44, 41, 38, 36, 35, 33, 31, 29, 27, 24, 22, 19]
    for i, v in enumerate(vals):
        col = G if v >= 35 else (Y if v >= 28 else R)
        s.rect(10 + i * 18, SH - 8 - (v - 15) * 2.1, 12, (v - 15) * 2.1, col)
    return s


def menu():
    s = Screen("menu", "Menu principal")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "Menu")
    items = [("Voltar", K), ("Modo FEC", K), ("Modo CRS", K), ("Modo PRC", K), ("Modo Zwift", K),
             ("Modo DBG", K), ("Ajustes", K), ("Desligar", R)]
    menu_list(s, items, 2, top=BAR + 38, step=40)
    return s


def ajustes():
    s = Screen("ajustes", "Ajustes")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "Ajustes")
    items = [("Voltar", K), ("Sensores", K), ("FTP  250 W", K), ("Peso  72 kg", K),
             ("Calibrar bússola", K), ("Tela e luz", K), ("GNSS: LEAP", K), ("Energia", K),
             ("Formatar", R)]
    menu_list(s, items, 1, top=BAR + 36, step=36)
    return s


def parear():
    s = Screen("parear", "Pareamento (ANT+ e BLE)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "Parear FC")
    s.text(SW / 2, BAR + 50, "Procurando...", 13, B, "middle")
    rows = [("Voltar", "", K), ("ANT+ 17334", "-52 dBm", K), ("ANT+ 20111", "-71 dBm", K),
            ("BLE Polar H10", "-60 dBm", K), ("BLE HRM-Pro", "-78 dBm", K)]
    y0 = BAR + 70
    for i, (name, rssi, col) in enumerate(rows):
        y = y0 + i * 40
        if i == 1:
            s.rect(6, y - 4, SW - 12, 34, K)
            s.text(14, y + 18, name, 14, W)
            s.text(232, y + 18, rssi, 12, W, "end", 400)
        else:
            s.text(14, y + 18, name, 14, col)
            s.text(232, y + 18, rssi, 12, K, "end", 400)
    s.text(SW / 2, SH - 14, "centro: parear", 11, K, "middle", 400)
    return s


def valor():
    s = Screen("valor", "Editar valor (FTP)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "FTP")
    s.text(SW / 2, 210, "250", 64, K, "middle")
    s.text(SW / 2, 240, "W", 18, K, "middle")
    s.rect(14, 320, 60, 44, W, K, 2)
    s.text(44, 350, "-1", 18, K, "middle")
    s.rect(166, 320, 60, 44, W, K, 2)
    s.text(196, 350, "+1", 18, K, "middle")
    s.text(SW / 2, 350, "gravar", 12, B, "middle")
    return s


def boot():
    s = Screen("boot", "Partida")
    s.rect(0, 0, SW, SH, W)
    # bicycle outline, after the legacy splash (legacy/drivers/lcd/ls027_splash.h)
    s.circle(70, 190, 42, "none", K, 5)
    s.circle(170, 190, 42, "none", K, 5)
    s.polyline([(70, 190), (108, 128), (150, 128), (170, 190)], K, 5)
    s.polyline([(108, 128), (122, 190), (70, 190)], K, 5)
    s.polyline([(122, 190), (150, 128)], K, 5)
    s.polyline([(100, 112), (118, 112)], K, 5)
    s.polyline([(150, 128), (146, 108), (160, 104)], K, 5)
    s.text(SW / 2, 290, "GNSS Bike", 24, K, "middle")
    s.text(SW / 2, 318, "Computer", 24, K, "middle")
    s.text(SW / 2, 360, "3.0.0", 12, B, "middle", 400)
    return s


def notif():
    s = crs1()
    s.name, s.caption = "notif", "Notificação sobre a página"
    h = (SH - BAR) / ROWS
    s.rect(0, BAR, SW, h, K)
    s.text(8, BAR + 17, "Segmento", 11, Y)
    s.text(8, BAR + 40, "Serra do Mar", 15, W)
    s.text(232, BAR + 40, "+12.4 s", 15, G, "end")
    return s


def energia():
    s = Screen("energia", "Energia (nova)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "Energia")
    s.text(SW / 2, BAR + 92, "91 %", 44, K, "middle")
    s.rect(30, BAR + 108, 170, 26, W, K, 2)
    s.rect(200, BAR + 115, 6, 12, K)
    s.rect(33, BAR + 111, 164 * 0.91, 20, G)
    rows = [("Tensão", "3,92 V", K), ("Corrente", "-42 mA", K), ("Fonte", "solar 18 mW", G),
            ("Carga solar até", "3,90 V", K), ("Temperatura", "31 °C", K),
            ("Autonomia", "cerca de 290 h", B)]
    y = BAR + 168
    for k, v, col in rows:
        s.text(10, y, k, 12, K, weight=400)
        s.text(230, y, v, 12, col, "end")
        y += 30
    return s


def sensores():
    s = Screen("sensores", "Sensores (nova)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s)
    title_bar(s, "Sensores")
    rows = [("FC", "ANT+ 17334", "147 bpm", G), ("Vel./cad.", "ANT+ 15568", "perdido", R),
            ("Potência", "BLE Assioma", "222 W", G), ("Rolo", "FE-C", "sem par", K),
            ("Radar", "ANT+ 3301", "livre", G), ("Luz", "ANT+ 1204", "ligada", G)]
    y = BAR + 44
    for name, dev, st, col in rows:
        s.circle(14, y + 10, 6, col, K, 1)
        s.text(28, y + 8, name, 13, K)
        s.text(28, y + 26, dev, 11, K, weight=400)
        s.text(232, y + 18, st, 13, col, "end")
        s.line(6, y + 34, 234, y + 34, K, 1, "2 3")
        y += 52
    return s


def usb():
    s = Screen("usb", "Modo USB (MSC)")
    s.rect(0, 0, SW, SH, W)
    status_bar(s, rec=False, solar=False, usb=True)
    s.rect(80, 110, 80, 60, W, K, 4)
    s.rect(96, 170, 48, 30, K)
    s.text(SW / 2, 250, "Modo USB", 24, K, "middle")
    s.text(SW / 2, 280, "arquivos no computador", 12, K, "middle", 400)
    s.text(SW / 2, 330, "Não desconecte", 16, R, "middle")
    return s


def desligando():
    s = Screen("desligando", "Desligando")
    s.rect(0, 0, SW, SH, W)
    s.text(SW / 2, 180, "Salvando", 22, K, "middle")
    s.text(SW / 2, 208, "atividade", 22, K, "middle")
    s.rect(40, 240, 160, 14, W, K, 2)
    s.rect(43, 243, 110, 8, B)
    s.text(SW / 2, 300, "Desligando", 16, K, "middle", 400)
    return s


SHEETS = {
    "telas-crs.svg": [crs1, crs1_seg, crs1_2seg, crs2, crs3],
    "telas-modos.svg": [prc, fec, gps, dbg],
    "telas-menus.svg": [menu, ajustes, parear, valor],
    "telas-sistema.svg": [boot, notif, energia, sensores, usb, desligando],
}


def sheet(screens):
    cell_w = SW * SCALE
    cell_h = SH * SCALE
    width = int(len(screens) * (cell_w + GAP) + GAP)
    height = int(cell_h + CAPTION + GAP)
    out = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" '
           f'viewBox="0 0 {width} {height}">',
           f'<rect width="{width}" height="{height}" fill="#ffffff"/>']
    for i, make in enumerate(screens):
        s = make()
        x0 = GAP + i * (cell_w + GAP)
        out.append(f'<text x="{f(x0 + cell_w / 2)}" y="{f(GAP / 2 + 16)}" font-size="16" fill="#1d1d1f" '
                   f'text-anchor="middle" font-weight="600" font-family="{CAPTION_FONT}">{s.caption}</text>')
        y0 = GAP / 2 + CAPTION
        out.append(f'<g transform="translate({f(x0)},{f(y0)}) scale({SCALE})">')
        out.append(f'<clipPath id="clip-{s.name}"><rect width="{SW}" height="{SH}"/></clipPath>')
        out.append(f'<g clip-path="url(#clip-{s.name})">')
        out.extend(s.el)
        out.append("</g>")
        out.append(f'<rect width="{SW}" height="{SH}" fill="none" stroke="#1d1d1f" stroke-width="2"/>')
        out.append("</g>")
    out.append("</svg>")
    return "\n".join(out) + "\n"


def main():
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    for name, screens in SHEETS.items():
        path = OUT_DIR / name
        path.write_text(sheet(screens), encoding="utf-8", newline="\n")
        print(path)


if __name__ == "__main__":
    main()
