#!/usr/bin/env python3
"""The case's dry run: does everything the case has to have exist, fit and
land where the board needs it? Measured on make_caixa.Caixa - the same
numbers that draw the PDF, the PNG views and the STL - against the board
file, its footprints' courtyards, heights and faces.

The owner asked for it on 2026-09-26: posts, holes, room; the cell, the
solar modules, the board, the USB-C and its door, the light sensor, the
cradle for an external GNSS antenna, the fixings of the side panels when
the lid closes. Every rule prints ok, FALHA or "nao medido", with the
number it measured, and a rule that finds nothing to measure FAILS saying
so - the lesson of the board's own dry run.

Run: python hardware_gnssbike/caixa/dry_run_caixa.py (it reads the placed
board, cad/gnssbike.kicad_pcb, through make_caixa.py in this folder)
Exit code 1 when a rule fails.
"""

from __future__ import annotations

import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import make_caixa as C   # noqa: E402

FOLGA_TAMPA = 0.3        # air between a part's top and the lid's underside
FOLGA_CELULA = 0.3       # air between a back part and the cell
FOLGA_GERAL = 0.5
DIST_PARAFUSO_ANTENA = 8.0
COBERTURA_SOBREPOSICAO = 0.8


def _cruza(a, b, folga=0.0) -> bool:
    return (a[2] + folga > b[0] and b[2] + folga > a[0]
            and a[3] + folga > b[1] and b[3] + folga > a[1])


def _dentro(a, b, margem=0.0) -> bool:
    """Is rectangle a inside rectangle b by at least `margem`?"""
    return (a[0] >= b[0] + margem - 1e-9 and a[1] >= b[1] + margem - 1e-9
            and a[2] <= b[2] - margem + 1e-9 and a[3] <= b[3] - margem + 1e-9)


def _dist_seg(p, a, b) -> float:
    """Distance from point p to segment a-b, in the (x, z) plane."""
    vx, vy = b[0] - a[0], b[1] - a[1]
    ll = vx * vx + vy * vy
    if ll < 1e-18:
        return math.hypot(p[0] - a[0], p[1] - a[1])
    t = max(0.0, min(1.0, ((p[0] - a[0]) * vx + (p[1] - a[1]) * vy) / ll))
    return math.hypot(p[0] - (a[0] + t * vx), p[1] - (a[1] + t * vy))


def _x_max_em_z(pol, z) -> float:
    """The largest x where the horizontal line at height z crosses the
    polygon's edges (the polygon is in (x, z)); -inf when it does not."""
    best = -float("inf")
    n = len(pol)
    for k in range(n):
        (x0, z0), (x1, z1) = pol[k], pol[(k + 1) % n]
        if abs(z1 - z0) < 1e-12:
            if abs(z - z0) < 1e-9:
                best = max(best, x0, x1)
            continue
        t = (z - z0) / (z1 - z0)
        if -1e-9 <= t <= 1 + 1e-9:
            best = max(best, x0 + t * (x1 - x0))
    return best


class Relatorio:
    def __init__(self):
        self.linhas: list[tuple[str, str, str]] = []

    def ok(self, regra, texto):
        self.linhas.append(("ok", regra, texto))

    def falha(self, regra, texto):
        self.linhas.append(("FALHA", regra, texto))

    def nao_medido(self, regra, texto):
        self.linhas.append(("nao medido", regra, texto))

    def imprimir(self) -> int:
        for estado, regra, texto in self.linhas:
            print(f"  {estado:<10} {regra}: {texto}")
        n_ok = sum(1 for e, _r, _t in self.linhas if e == "ok")
        n_f = sum(1 for e, _r, _t in self.linhas if e == "FALHA")
        n_n = sum(1 for e, _r, _t in self.linhas if e == "nao medido")
        print(f"\n{n_ok} regras medidas e cumpridas, {n_f} violadas, {n_n} nao medidas")
        return 1 if n_f else 0


def regras(cx: C.Caixa, r: Relatorio) -> None:
    pecas = cx.pecas
    frente = {k: v for k, v in pecas.items() if not v["atras"]}
    verso = {k: v for k, v in pecas.items() if v["atras"]}
    if not pecas:
        r.falha("CX0", "nenhuma peca lida da placa: nada para medir")
        return

    # ---- CX1: room under the lid, outside the display
    teto = C.TAMPA_Z0
    altos = []
    for ref, p in frente.items():
        cima = C.PLACA_Z1 + p["altura"]
        if cima > teto - FOLGA_TAMPA + 1e-9:
            altos.append(f"{ref} chega a z {cima:.2f} ({p['altura']:g} mm) contra a tampa em {teto:g}")
    if altos:
        r.falha("CX1", f"{len(altos)} pecas da frente batem na tampa ou ficam a menos de {FOLGA_TAMPA} mm dela: " + "; ".join(altos))
    else:
        mais = max(frente.items(), key=lambda kv: kv[1]["altura"])
        r.ok("CX1", f"toda peca da frente fica a pelo menos {FOLGA_TAMPA} mm da tampa; a mais alta e {mais[0]} com {mais[1]['altura']:g} mm, "
                    f"contra {teto - C.PLACA_Z1:.1f} mm da face da placa a tampa")

    # ---- CX2: the back, over the cell and over the floor
    cel = cx.celula
    ruins = []
    for ref, p in verso.items():
        cx0, cy0, cx1, cy1 = C.caixa_na_caixa(p)
        base = C.PLACA_Z0 - p["altura"]
        if _cruza((cx0, cy0, cx1, cy1), cel):
            if base < C.CELULA_Z1 + FOLGA_CELULA - 1e-9:
                ruins.append(f"{ref} desce a z {base:.2f} sobre a celula, que vai ate {C.CELULA_Z1:g} (folga {FOLGA_CELULA})")
        elif base < C.FUNDO + FOLGA_GERAL - 1e-9:
            ruins.append(f"{ref} desce a z {base:.2f} contra o fundo em {C.FUNDO:g}")
    if ruins:
        r.falha("CX2", f"{len(ruins)} pecas do verso sem folga: " + "; ".join(ruins))
    else:
        r.ok("CX2", f"as {len(verso)} pecas do verso ficam a pelo menos {FOLGA_CELULA} mm da celula ou {FOLGA_GERAL} mm do fundo")

    # ---- CX3: under the display (the board's ME2 seen from the case)
    disp = cx.display
    sob = []
    for ref, p in frente.items():
        if _cruza(C.caixa_na_caixa(p), disp) and C.PLACA_Z1 + p["altura"] > C.DISPLAY_Z0 - FOLGA_TAMPA + 1e-9:
            sob.append(f"{ref} {p['altura']:g} mm")
    if sob:
        r.falha("CX3", f"{len(sob)} pecas sob o display passam de {C.DISPLAY_VAO - FOLGA_TAMPA:.1f} mm: " + "; ".join(sob))
    else:
        r.ok("CX3", f"nenhuma peca sob o display passa de {C.DISPLAY_VAO - FOLGA_TAMPA:.1f} mm")

    # ---- CX4: keys
    if not cx.teclas:
        r.falha("CX4", "a placa nao tem tecla nenhuma para a tampa alcancar")
    else:
        erros = []
        for (sx, sy), (kx, ky) in zip(cx.teclas, cx.capas):
            if math.hypot(sx - kx, sy - ky) > 0.3:
                erros.append(f"capa em ({kx:.1f}; {ky:.1f}) a {math.hypot(sx - kx, sy - ky):.2f} mm da chave")
        for furo in cx.furos_teclas:
            if _cruza(furo, cx.janela, 1.0):
                erros.append(f"furo de tecla {tuple(round(v, 1) for v in furo)} a menos de 1 mm da janela")
            if cx.rebaixo_membrana and not _dentro(furo, cx.rebaixo_membrana, 1.0):
                erros.append(f"furo de tecla {tuple(round(v, 1) for v in furo)} fora do rebaixo da membrana")
        curso = C.TAMPA_Z0 - cx.topo_tecla
        if curso < C.BARRA_ESP + 0.2:
            erros.append(f"entre o embolo da chave ({cx.topo_tecla:.1f}) e a tampa ({C.TAMPA_Z0:g}) ha {curso:.2f} mm, menos que a aba ({C.BARRA_ESP}) mais 0,2")
        if erros:
            r.falha("CX4", "; ".join(erros))
        else:
            r.ok("CX4", f"{len(cx.teclas)} capas sobre as chaves (desvio maximo "
                        f"{max(math.hypot(a[0] - b[0], a[1] - b[1]) for a, b in zip(cx.teclas, cx.capas)):.2f} mm), "
                        f"furos a mais de 1 mm da janela e dentro do rebaixo, {curso:.1f} mm do embolo a tampa")

    # ---- CX5: the window and the glass
    d = cx.display
    j = cx.janela
    margens = (j[0] - d[0], j[1] - d[1], d[2] - j[2], d[3] - j[3])
    # 1,0 is what the display gives on its short sides: 61,8 of glass for
    # 59,8 of viewing area; the window is the viewing area, so the tape's
    # land there is that 1,0 (1,9 on the long sides)
    if min(margens) < 1.0 - 1e-9:
        r.falha("CX5", f"a janela deixa so {min(margens):.2f} mm de vidro em volta para a fita (minimo 1,0, o que o display da nos lados curtos)")
    else:
        r.ok("CX5", f"a janela ({j[2] - j[0]:.2f} x {j[3] - j[1]:.2f}) e a area visivel e deixa {min(margens):.2f} mm de vidro nos lados curtos "
                    f"e {max(margens):.2f} nos longos para a fita; vidro {C.T_C - C.DISPLAY_Z1:.1f} mm abaixo da face da tampa")

    # ---- CX6: USB-C, its notch and its door
    j101 = pecas.get("J101")
    if not j101:
        r.falha("CX6", "a placa nao tem J101 para a parede ter entalhe")
    else:
        ux0, ux1, uz0, uz1 = cx.usb
        jx = C.PLACA_X0 + j101["x"]
        fim = C.PLACA_Y0 + j101["caixa"][3]
        parede = C.H_C - C.PAREDE
        erros = []
        if abs((ux0 + ux1) / 2 - jx) > 0.5:
            erros.append(f"entalhe descentrado {abs((ux0 + ux1) / 2 - jx):.2f} mm do J101")
        if uz0 > C.PLACA_Z1 - 0.2 + 1e-9 or uz1 < C.PLACA_Z1 + j101["altura"] + 0.2 - 1e-9:
            erros.append(f"entalhe z {uz0:.1f}-{uz1:.1f} nao cobre o receptaculo z {C.PLACA_Z1:.1f}-{C.PLACA_Z1 + j101['altura']:.2f} com 0,2")
        if parede - fim > 1.0:
            erros.append(f"a boca do J101 fica {parede - fim:.2f} mm antes da parede (maximo 1,0)")
        px0, px1, pz0, pz1 = cx.porta
        if not (px0 <= ux0 - 1.0 and px1 >= ux1 + 1.0 and pz0 <= uz0 - 1.0 and pz1 >= uz1 + 1.0):
            erros.append("a porta nao cobre o entalhe com 1 mm de aba em volta")
        for lx0, lx1 in cx.lugs_porta:
            for x, y in C.PARAFUSO_TAMPA:
                if abs(y - C.H_C) < 8.0 and lx0 - 1.0 < x < lx1 + 1.0:
                    erros.append(f"ressalto da porta em x {lx0:.1f}-{lx1:.1f} bate na bossa do parafuso em ({x:g}; {y:g})")
        if erros:
            r.falha("CX6", "; ".join(erros))
        else:
            r.ok("CX6", f"entalhe de {ux1 - ux0:g} x {uz1 - uz0:.1f} centrado no J101, boca a {parede - fim:.2f} mm da parede, "
                        f"porta de {px1 - px0:.1f} x {pz1 - pz0:.1f} com {C.PORTA_ABA:g} mm de aba, ressaltos longe das bossas")

    # ---- CX7: what holds the board
    erros = []
    for x, y in cx.furos_sobre_celula:
        erros.append(f"furo M2 da placa em ({x - C.PLACA_X0:g}; {y - C.PLACA_Y0:g}) sobre a celula, sem bossa")
    apoios = [(x, y, C.BOSSA_PLACA_D / 2) for x, y in cx.bossas_placa] + [(x, y, C.PILAR_D / 2) for x, y in cx.pilares]
    for x, y, raio in apoios:
        quad = (x - raio, y - raio, x + raio, y + raio)
        if _cruza(quad, cx.celula, 0.3):
            erros.append(f"apoio em ({x:.1f}; {y:.1f}) entra na celula")
        for ref, p in verso.items():
            if _cruza(quad, C.caixa_na_caixa(p), 0.3):
                erros.append(f"apoio em ({x:.1f}; {y:.1f}) bate em {ref} no verso")
        placa = (C.PLACA_X0, C.PLACA_Y0, C.PLACA_X0 + C.PLACA_W, C.PLACA_Y0 + C.PLACA_H)
        if not _cruza(quad, placa):
            erros.append(f"apoio em ({x:.1f}; {y:.1f}) fora da placa")
    if len(cx.bossas_placa) < 1:
        erros.append("nenhuma bossa com parafuso segura a placa")
    if erros:
        r.falha("CX7", "; ".join(erros))
    else:
        r.ok("CX7", f"{len(cx.bossas_placa)} bossa(s) M2 e {len(cx.pilares)} pilares sob a placa, fora da celula e das pecas do verso")

    # ---- CX8: the lid's screws
    erros = []
    for x, y in C.PARAFUSO_TAMPA:
        quad = (x - C.BOSSA_D / 2, y - C.BOSSA_D / 2, x + C.BOSSA_D / 2, y + C.BOSSA_D / 2)
        placa = (C.PLACA_X0, C.PLACA_Y0, C.PLACA_X0 + C.PLACA_W, C.PLACA_Y0 + C.PLACA_H)
        for nome, cx_r in (("a placa", placa), ("o display", cx.display), ("a celula", cx.celula)):
            if _cruza(quad, cx_r, FOLGA_GERAL):
                erros.append(f"bossa da tampa em ({x:g}; {y:g}) a menos de {FOLGA_GERAL} de {nome}")
        for ref in ("E301", "U201"):
            p = pecas.get(ref)
            if p:
                ax0, ay0, ax1, ay1 = C.caixa_na_caixa(p)
                dx = max(ax0 - x, 0.0, x - ax1)
                dy = max(ay0 - y, 0.0, y - ay1)
                if math.hypot(dx, dy) < DIST_PARAFUSO_ANTENA:
                    erros.append(f"parafuso em ({x:g}; {y:g}) a {math.hypot(dx, dy):.1f} mm de {ref} (minimo {DIST_PARAFUSO_ANTENA})")
    if erros:
        r.falha("CX8", "; ".join(erros))
    else:
        r.ok("CX8", f"as {len(C.PARAFUSO_TAMPA)} bossas da tampa ficam longe da placa, do display, da celula e a mais de "
                    f"{DIST_PARAFUSO_ANTENA} mm das duas antenas")

    # ---- CX9: windows and holes over the parts that need them
    erros = []
    for ref, furo, nome in (("D601", cx.furo_led, "janela do LED"), ("U505", cx.furo_sensor, "janela do sensor de luz")):
        p = pecas.get(ref)
        if not p:
            erros.append(f"{ref} nao esta na placa")
        elif furo is None:
            erros.append(f"{ref} sem {nome}: esta {'no verso' if p['atras'] else 'sob o display'}")
        else:
            cxp, cyp = C.PLACA_X0 + p["x"], C.PLACA_Y0 + p["y"]
            if math.hypot((furo[0] + furo[2]) / 2 - cxp, (furo[1] + furo[3]) / 2 - cyp) > 0.5:
                erros.append(f"{nome} descentrada")
            if _cruza(furo, cx.janela):
                erros.append(f"{nome} entra na janela do display")
    ls = pecas.get("LS601")
    if ls:
        cb = C.caixa_na_caixa(ls)
        furos_som = [f for f in cx.furos_fundo if _dentro(f, cb)]
        if ls["atras"] and len(furos_som) < 3:
            erros.append(f"so {len(furos_som)} furo(s) de som sob o buzzer")
        elif not ls["atras"]:
            erros.append("o buzzer esta na frente: os furos de som no fundo nao servem")
    baro = pecas.get("U502")
    if baro:
        cb = C.caixa_na_caixa(baro)
        if baro["atras"] and not any(_dentro(f, cb) for f in cx.furos_fundo):
            erros.append("sem respiro sob o barometro")
        elif not baro["atras"]:
            erros.append("o barometro esta na frente: o respiro no fundo nao serve")
    if erros:
        r.falha("CX9", "; ".join(erros))
    else:
        r.ok("CX9", "janela do LED e do sensor de luz sobre as pecas, fora da janela do display; 3 furos de som sob o buzzer; respiro sob o barometro")

    # ---- CX10: the solar modules, their pockets, covers and wire passages,
    # measured on the facet's rectangles and on the bevel's cross-sections
    # (the very polygons the STL is extruded from)
    erros = []
    precisa = (C.MODULO_W + 2 * C.MODULO_FOLGA, C.MODULO_H + 2 * C.MODULO_FOLGA)
    if not cx.bolsos_faceta or not cx.modulos_chanfro:
        erros.append("sem bolso de modulo para medir")
    for b in cx.bolsos_faceta:
        if b[2] - b[0] < precisa[0] - 1e-9 or b[3] - b[1] < precisa[1] - 1e-9:
            erros.append(f"bolso da faceta {b[2] - b[0]:.1f} x {b[3] - b[1]:.1f} menor que o modulo com folga {precisa[0]:.1f} x {precisa[1]:.1f}")
        if not _dentro(b, cx.cobertura_faceta, COBERTURA_SOBREPOSICAO):
            erros.append(f"a cobertura da faceta nao sobra {COBERTURA_SOBREPOSICAO} mm em volta de um bolso")
        if not any(_dentro(f, b) for f in cx.furos_fio):
            erros.append("bolso da faceta sem furo de fio")
    fundo_bolso_faceta = C.TAMPA + C.FACETA_ALT - C.COBERTURA_ESP - C.FACETA_FUNDO
    if fundo_bolso_faceta < C.MODULO_ESP + 0.1 - 1e-9:
        erros.append(f"o bolso da faceta tem {fundo_bolso_faceta:.2f} de fundo para um modulo de {C.MODULO_ESP:g} mais 0,1")
    if C.FACETA_FUNDO < 0.5 - 1e-9:
        erros.append(f"o fundo do bolso da faceta tem {C.FACETA_FUNDO:g} mm (minimo 0,5)")
    # the bevel: one side's pocket profile, in slope coordinates
    g = cx._geo_chanfro()
    A, s, nrm, comp = g["A"], g["s"], g["nrm"], g["comp"]

    def ao_longo(p):
        return (p[0] - A[0]) * s[0] + (p[1] - A[1]) * s[1]

    def fundo(p):
        return -((p[0] - A[0]) * nrm[0] + (p[1] - A[1]) * nrm[1])
    pol = cx._perfil_chanfro("L", 0.0, C.MODULO_ESP + 0.1)
    prof = C.COBERTURA_ESP + C.MODULO_ESP + 0.1
    chao = [p for p in pol if abs(fundo(p) - prof) < 1e-6]
    if len(chao) != 2:
        erros.append(f"nao achei o fundo do bolso do chanfro no perfil ({len(chao)} pontos a {prof:.2f} de profundidade)")
    else:
        a0, a1 = sorted(ao_longo(p) for p in chao)
        if a1 - a0 < precisa[1] - 1e-6:
            erros.append(f"o bolso do chanfro tem {a1 - a0:.2f} ao longo da rampa para um modulo de {precisa[1]:.1f}")
        if a0 < 1.0 - 1e-6 or comp - a1 < 1.0 - 1e-6:
            erros.append(f"o bolso do chanfro deixa {a0:.2f} mm de parede no pe e {comp - a1:.2f} no alto da rampa de {comp:.2f} (minimo 1,0 para a cobertura assentar)")
        # the wall behind the pocket floor: the floor's corners against the
        # inner boundary (E-D parallel to the face, D-C vertical)
        E, D, Cc = g["E"], g["D"], g["C"]
        sobra = min(_dist_seg(p, E, D) for p in chao) if D != E else float("inf")
        if D != Cc:
            sobra = min(sobra, min(_dist_seg(p, D, Cc) for p in chao))
        if sobra < 0.5 - 1e-6:
            erros.append(f"o bolso do chanfro deixa {sobra:.2f} mm de parede atras dele (minimo 0,5)")
    # the recess for the cover strip: it starts on the wall's top (A' at
    # x >= 0) and ends on the lid's top before the inner vertical face
    A_, B_, Cc = g["A_"], g["B_"], g["C"]
    if A_[0] < -1e-9 or B_[0] > Cc[0] - 0.5:
        erros.append(f"a face rebaixada do chanfro vai de x {A_[0]:.2f} a {B_[0]:.2f} no alto, contra a face interna em {Cc[0]:.2f}")
    # the wire slots: two per pocket, inside it, cutting the floor through
    # to the inner boundary
    for lado, ya, yb in cx.modulos_chanfro:
        fendas = [(a, b) for l_, a, b in cx.furos_fio_chanfro if l_ == lado and a >= ya - 1e-9 and b <= yb + 1e-9]
        if len(fendas) < 2:
            erros.append(f"bolso do chanfro {lado} em y {ya:.1f}-{yb:.1f} com {len(fendas)} fenda(s) de fio (precisa de 2)")
        if ya < C.CHANFRO_Y[0] + C.COBERTURA_FOLGA - 1e-9 or yb > C.CHANFRO_Y[1] - C.COBERTURA_FOLGA + 1e-9:
            erros.append(f"modulo do chanfro {lado} em y {ya:.1f}-{yb:.1f} sai do chanfro ({C.CHANFRO_Y[0]:g}-{C.CHANFRO_Y[1]:g}) com a cobertura")
        if yb - ya < precisa[0] - 1e-9:
            erros.append(f"bolso do chanfro {lado} com {yb - ya:.1f} mm, menor que o modulo com folga {precisa[0]:.1f}")
    baixo, cima = cx._perfis_chanfro_fio("L", 0.0)
    E, D = g["E"], g["D"]
    for nome, p in (("de baixo", baixo[-2]), ("de cima", cima[-1])):
        if _dist_seg(p, E, D) > 1e-6:
            erros.append(f"a fenda de fio nao chega a face interna do chanfro (canto {nome} a {_dist_seg(p, E, D):.2f} mm dela)")
    if C.COBERTURA_FOLGA < COBERTURA_SOBREPOSICAO - 1e-9:
        erros.append(f"a tira do chanfro sobra {C.COBERTURA_FOLGA:g} alem do bolso em y (minimo {COBERTURA_SOBREPOSICAO})")
    if erros:
        r.falha("CX10", "; ".join(erros))
    else:
        r.ok("CX10", f"faceta: 2 bolsos de {precisa[0]:.1f} x {precisa[1]:.1f} x {fundo_bolso_faceta:.1f} sobre {C.FACETA_FUNDO:g} de fundo, "
                     f"cobertura sobrando {COBERTURA_SOBREPOSICAO} mm, furo de fio em cada; chanfro: rampa de {comp:.2f} com o bolso de "
                     f"{a1 - a0:.1f} entre paredes de {a0:.2f} e {comp - a1:.2f}, {sobra:.2f} mm de parede atras dele, a tira em toda a rampa "
                     f"com {C.COBERTURA_FOLGA:g} alem do bolso em y, 2 fendas de fio por bolso ate a face interna")

    # ---- CX11: the bevels when the lid closes on the shell: the foot on
    # the wall's top, and the solid's inner boundary against the glass, the
    # board, the cell and the supports, all read off the plain profile
    erros = []
    pol = cx._perfil_chanfro("L", 0.0, None)
    z_pe = cx.z_parede_chanfro
    pe_pts = [p for p in pol if abs(p[1] - z_pe) < 1e-9]
    if len(pe_pts) < 2:
        erros.append("o perfil do chanfro nao tem um pe horizontal para assentar na parede")
    else:
        x_pe0, x_pe1 = min(p[0] for p in pe_pts), max(p[0] for p in pe_pts)
        if x_pe0 > 1e-9 or x_pe1 < C.PAREDE - 1e-9:
            erros.append(f"o pe do chanfro cobre x {x_pe0:.2f}-{x_pe1:.2f} e a parede vai de 0 a {C.PAREDE:g}")
    if z_pe < C.FUNDO + 5.0:
        erros.append(f"o pe do chanfro ({z_pe:.1f}) desce abaixo das nervuras da celula ({C.FUNDO + 5.0:.1f})")
    if z_pe > C.TAMPA_Z0:
        erros.append("o pe do chanfro fica acima da tampa: a parede e o chanfro nao se encontram")
    # the lid's plate beside the bevel starts at x = CHANFRO: inside the solid?
    x_rampa_tampa = C.CHANFRO - C.TAMPA
    x_max_tampa = _x_max_em_z(pol, C.TAMPA_Z0)
    if not (x_rampa_tampa <= C.CHANFRO <= x_max_tampa):
        erros.append(f"a placa da tampa comeca em x {C.CHANFRO:g} e o chanfro vai de {x_rampa_tampa:.2f} a {x_max_tampa:.2f} em z {C.TAMPA_Z0:g}")
    # the inner boundary against what is inside, at each thing's heights
    alvos = [("o vidro do display", cx.display[0], (C.DISPLAY_Z0, C.DISPLAY_Z1)),
             ("a placa", C.PLACA_X0, (C.PLACA_Z0, C.PLACA_Z1)),
             ("a celula", cx.celula[0], (C.CELULA_Z0, C.CELULA_Z1)),
             ("as nervuras da celula", min(x0 for x0, _y0, _x1, _y1 in cx.nervuras), (C.FUNDO, C.FUNDO + 5.0))]
    folgas = []
    for nome, x_lim, (za, zb) in alvos:
        if zb < z_pe:
            continue                                  # entirely below the bevel
        x_in = max(_x_max_em_z(pol, z) for z in (max(za, z_pe), zb))
        folgas.append((nome, x_lim - x_in))
        if x_lim - x_in < FOLGA_GERAL - 1e-9:
            erros.append(f"a face interna do chanfro chega a x {x_in:.2f} e {nome} comeca em {x_lim:.2f} (folga {FOLGA_GERAL})")
    for x, y, raio in [(x, y, C.BOSSA_PLACA_D / 2) for x, y in cx.bossas_placa] + [(x, y, C.PILAR_D / 2) for x, y in cx.pilares]:
        if C.CHANFRO_Y[0] - 1.0 < y < C.CHANFRO_Y[1] + 1.0 and C.PLACA_Z0 > z_pe:
            x_in = _x_max_em_z(pol, C.PLACA_Z0)
            if min(x - raio, C.W_C - x - raio) < x_in + FOLGA_GERAL:
                erros.append(f"apoio da placa em ({x:.1f}; {y:.1f}) dentro do vao do chanfro")
    if erros:
        r.falha("CX11", "; ".join(erros))
    else:
        r.ok("CX11", f"os chanfros assentam nas paredes longas em z {z_pe:.1f} (pe de x {x_pe0:.2f} a {x_pe1:.2f} sobre a parede de {C.PAREDE:g}), "
                     f"de y {C.CHANFRO_Y[0]:g} a {C.CHANFRO_Y[1]:g}; a placa da tampa entra neles; folga da face interna: "
                     + ", ".join(f"{n} {f:.2f}" for n, f in folgas))

    # ---- CX12: the external antenna's cradle
    erros = []
    bx0, by0, bx1, by1, bz0, bz1 = cx.berco
    placa = (C.PLACA_X0, C.PLACA_Y0, C.PLACA_X0 + C.PLACA_W, C.PLACA_Y0 + C.PLACA_H)
    if _cruza((bx0, by0, bx1, by1 + 1.0), placa, FOLGA_GERAL):
        erros.append("o berco (com o labio) entra na placa")
    if bz1 > C.TAMPA_Z0 - FOLGA_GERAL:
        erros.append(f"o berco sobe a z {bz1:.1f}, contra a tampa em {C.TAMPA_Z0:g}")
    for x, y in C.PARAFUSO_TAMPA:
        if _cruza((bx0 - 1.0, by0, bx1 + 1.0, by1 + 1.0), (x - C.BOSSA_D / 2, y - C.BOSSA_D / 2, x + C.BOSSA_D / 2, y + C.BOSSA_D / 2), FOLGA_GERAL):
            erros.append(f"o berco bate na bossa do parafuso em ({x:g}; {y:g})")
    j302 = pecas.get("J302")
    if not j302:
        erros.append("a placa nao tem o U.FL J302 para a antena externa")
    else:
        dist = math.hypot((bx0 + bx1) / 2 - (C.PLACA_X0 + j302["x"]), by1 - (C.PLACA_Y0 + j302["y"]))
        if dist > 40.0:
            erros.append(f"o U.FL fica a {dist:.0f} mm do berco: um rabicho de 50 mm nao chega")
    if bx1 - bx0 < C.ANT_EXT_W + 2 * C.ANT_EXT_FOLGA - 1e-9 or by1 - by0 < C.ANT_EXT_ESP + 2 * C.ANT_EXT_FOLGA - 1e-9:
        erros.append("a fenda do berco e menor que a patch com folga")
    if erros:
        r.falha("CX12", "; ".join(erros))
    else:
        r.ok("CX12", f"berco de {bx1 - bx0:.1f} x {by1 - by0:.1f} x {bz1 - bz0:.1f} contra a parede de cima, fora da placa e das bossas, "
                     f"U.FL a {dist:.0f} mm")

    # ---- CX13: things that need the bench or a print
    r.nao_medido("CX13", "a passagem dos fios dos seis modulos por dentro da tampa ate J103, J104 e J105, a junta da porta e da "
                         "particao, o aperto das capas na membrana e o encaixe dos parafusos auto-atarraxantes: so uma prova impressa diz")


def main() -> int:
    pecas = C.ler_placa()
    cx = C.Caixa(pecas)
    print(f"caixa {C.W_C:g} x {C.H_C:g} x {C.T_C:g}; placa {C.PLACA_W:g} x {C.PLACA_H:g} em x {C.PLACA_X0:g}-{C.PLACA_X0 + C.PLACA_W:g}, "
          f"y {C.PLACA_Y0:.1f}-{C.PLACA_Y0 + C.PLACA_H:.1f}; {len(pecas)} pecas\n")
    r = Relatorio()
    regras(cx, r)
    return r.imprimir()


if __name__ == "__main__":
    sys.exit(main())
