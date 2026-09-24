#!/usr/bin/env python3
"""Write gnssbike.kicad_pcb: the board, with every part placed and every net.

The outline, the stackup, the single mounting hole and the two antenna
keep-outs come from hardware_gnssbike/04-pcb-e-caixa.md. What is new here is
the placement: each part that the document gives a zone to is put in its zone,
and everything else - the decoupling, the pull-ups, the series resistors -
lands on the nearest free slot to the part it belongs to, outside the
keep-outs and off the board edge. Nothing is routed: the tracks are the
owner's job, and this is the board he starts from.

Run:   python hardware_gnssbike/cad/make_pcb.py
Check: python hardware_gnssbike/cad/check_pcb.py
"""

from __future__ import annotations

import hashlib
import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import footprints as FPS  # noqa: E402
import fp_load  # noqa: E402
import make_dxf as M  # noqa: E402
import nets as N  # noqa: E402
import parts as P  # noqa: E402

ORIGEM = (25.0, 25.0)          # where the board's top left sits on the sheet
CU = 0.035
DIEL = (0.80 - 4 * CU) / 3.0
CU_LAYERS = ("F.Cu", "In1.Cu", "In2.Cu", "B.Cu")
KEEPOUTS = {"KEEPOUT_ANTENA_GNSS", "KEEPOUT_ANTENA_MODULO"}
BORDA = 0.8                    # keep parts this far inside the outline
FOLGA = 0.5                    # between two courtyards
PASSO = 0.5                    # placement grid

# The part that anchors each zone of 04-pcb-e-caixa.md.
ANCORAS: dict[str, str] = {
    "U301": "ZONA_GNSS_MAX-F10S",
    "D601": "ZONA_LED_RGB",
    "U503": "ZONA_IMU_MAGNETOMETRO",
    "J401": "ZONA_FPC_DISPLAY_J401",
    "LS601": "ZONA_BUZZER",
    "U101": "ZONA_ENERGIA",
    "U501": "ZONA_FLASH_MX25R6435F",
    "U502": "ZONA_BAROMETRO_BMP585",
    "U201": "ZONA_MODULO_ME54BS13",
    "SW602": "ZONA_BOTOES",
    "J101": "ZONA_USB_C",
    "U505": "ZONA_LUZ_AMBIENTE_OPT3001",
}
# Parts that follow an anchor instead of a zone of their own.
JUNTO: dict[str, str] = {
    "U102": "U101", "U103": "U101", "U104": "U101",
    "L101": "U101", "L102": "U101", "L103": "U103",
    "J102": "U101", "JP101": "U101", "RT101": "U101",
    "D101": "J101", "D102": "J101",
    "U302": "U301", "FB301": "U301", "L301": "U301",
    "U504": "U503",
    "J402": "J401", "Q401": "J401",
    "SW601": "SW602", "SW603": "SW602",
    "Q601": "D601", "Q602": "D601", "Q603": "D601",
    "J201": "U201", "TP201": "U201", "TP202": "U201", "TP203": "U201",
}
# The back face: what 04-pcb-e-caixa.md puts on the battery side.
ATRAS = {"U502", "J102", "RT101"}

# Parts whose position AND rotation the case decides, not the placer: x, y,
# angle. These are not the numbers of 04-pcb-e-caixa.md, and the difference is
# the point. That document's zone table was written before any footprint
# existed, and three of its rectangles do not hold the real part:
#
#   USB-C      the zone is 9 x 3 mm. A USB-C receptacle is about 9 x 10 mm:
#              the body goes INTO the board, it does not sit on its edge. The
#              three keys and the connector cannot share the bottom band, so
#              the keys moved up to y 79 and the connector kept the edge.
#   modulo     the zone is 10 x 16.2 mm, for the Fanstel part. The MinewSemi
#              ME54BS13 is 12 x 16.5, and its datasheet wants 4 mm clear
#              around the antenna side and that side facing off the board. It
#              lies down with the antenna to the right edge, above the keys.
#   GNSS       the zone starts at y 2, inside the antenna keep-out that runs
#              to y 8. 04 already counted that overlap as 90 mm2 and left it
#              open; here the receiver sits below the keep-out, at y 8.5.
BORDA_FIXA: dict[str, tuple[float, float, int]] = {
    "J101": (27.5, 90.85, 180),    # USB-C, bottom edge, opening out
    "SW601": (11.0, 80.5, 0),     # the three keys, in a row above it
    "SW602": (27.5, 80.5, 0),
    "SW603": (44.0, 80.5, 0),
    "U201": (45.7, 65.0, 270),    # radio module, antenna to the right edge
    "J401": (5.75, 34.5, 270),     # display flat cable, out to the left
    "J402": (5.05, 23.0, 270),     # the light's cable, same side
    "J102": (4.05, 62.0, 90),      # battery connector, back face, out to the left
    "U505": (2.5, 89.0, 0),       # ambient light, under its window
    "D601": (51.5, 10.5, 0),      # RGB LED, under its light pipe
    "U301": (27.5, 13.8, 0),      # GNSS receiver, just below the antenna zone
}

# Rotations that are not about the case but about the circuit.
# A part is allowed inside the keep-out that exists because of it: the
# radio module sits over its own antenna zone, which forbids copper, not it.
DONO_DO_KEEPOUT: dict[str, str] = {"U201": "KEEPOUT_ANTENA_MODULO"}

ROTACAO: dict[str, int] = {
    "J201": 0,
    "U101": 0,
    "U103": 0,
}


def uid(*parts: object) -> str:
    h = hashlib.sha256("|".join(str(p) for p in parts).encode()).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-8{h[17:20]}-{h[20:32]}"


def P_(x: float, yd: float) -> tuple[float, float]:
    return (x + ORIGEM[0], yd + ORIGEM[1])


def redes() -> tuple[dict[str, int], dict[tuple[str, str], str]]:
    """Merge the nets that share a pin, number them, and index by pad."""
    bruto: dict[str, set] = {}
    for nome, pinos in N.NETS.items():
        membros = set()
        for ref, pin_name in pinos:
            part = P.PARTS[ref]
            numero = next(q.number for q in part.pins
                          if q.name == pin_name or q.number == pin_name)
            membros.add((ref, numero))
        bruto[nome] = membros
    pai = {n: n for n in bruto}

    def raiz(n):
        while pai[n] != n:
            pai[n] = pai[pai[n]]
            n = pai[n]
        return n

    de_pino: dict[tuple, str] = {}
    for nome, membros in bruto.items():
        for m in membros:
            if m in de_pino:
                a, b = raiz(de_pino[m]), raiz(nome)
                if a != b:
                    pai[b] = a
            else:
                de_pino[m] = nome
    juntos: dict[str, set] = {}
    nomes_do_grupo: dict[str, list[str]] = {}
    for nome, membros in bruto.items():
        juntos.setdefault(raiz(nome), set()).update(membros)
        nomes_do_grupo.setdefault(raiz(nome), []).append(nome)

    # A merged group keeps the best of its names, not whichever happened to
    # claim a pin first. Without this the ground comes out called FPC_VSS,
    # because the panel's ground is one of the names that merged into it.
    import sheets as S
    melhor: dict[str, str] = {}
    for r, membros_nomes in nomes_do_grupo.items():
        melhor[r] = sorted(
            membros_nomes,
            key=lambda n: (n not in S.TRILHOS, -len(bruto[n]), n))[0]

    numeros = {"": 0}
    por_pad: dict[tuple[str, str], str] = {}
    for i, (r, membros) in enumerate(sorted(juntos.items(),
                                            key=lambda kv: melhor[kv[0]]), start=1):
        numeros[melhor[r]] = i
        for m in membros:
            por_pad[m] = melhor[r]
    return numeros, por_pad


def livre(x: float, y: float, w: float, h: float,
          postos: list[tuple[float, float, float, float]],
          ref: str = "") -> bool:
    """Is the rectangle inside the board, out of the keep-outs and free?"""
    eps = 1e-6
    x0, y0, x1, y1 = x - w / 2, y - h / 2, x + w / 2, y + h / 2
    if x0 < BORDA - eps or y0 < BORDA - eps or             x1 > M.W - BORDA + eps or y1 > M.H - BORDA + eps:
        return False
    r = M.RADIUS_DRAWING
    for cx, cy in ((r, r), (M.W - r, r), (r, M.H - r), (M.W - r, M.H - r)):
        qx = min(max(x0, cx - r if cx < M.W / 2 else -1e9),
                 cx + r if cx > M.W / 2 else 1e9)
        del qx
    for nome, (kx0, ky0, kx1, ky1), _c, _s in M.ZONES:
        if nome not in KEEPOUTS or DONO_DO_KEEPOUT.get(ref) == nome:
            continue
        if x1 > kx0 and kx1 > x0 and y1 > ky0 and ky1 > y0:
            return False
    for px0, py0, px1, py1 in postos:
        if x1 + FOLGA > px0 and px1 + FOLGA > x0 and \
                y1 + FOLGA > py0 and py1 + FOLGA > y0:
            return False
    return True


def espiral(cx: float, cy: float, w: float, h: float,
            postos: list, raio_max: float = 60.0, ref: str = ""):
    """The nearest free slot to (cx, cy), searched outwards."""
    if livre(cx, cy, w, h, postos, ref):
        return (cx, cy)
    passo = PASSO
    r = passo
    while r <= raio_max:
        n = max(8, int(2 * math.pi * r / passo))
        for i in range(n):
            a = 2 * math.pi * i / n
            x = round((cx + r * math.cos(a)) / PASSO) * PASSO
            y = round((cy + r * math.sin(a)) / PASSO) * PASSO
            if livre(x, y, w, h, postos, ref):
                return (x, y)
        r += passo
    return None


def colocar() -> tuple[dict[str, tuple[float, float, int, bool]], list[str]]:
    """Place every part: x, y, rotation and which face.

    The rules are the ones a person uses, in this order:

      1. a connector faces the edge it comes out of, and its rotation is not
         negotiable: the USB-C opening has to point out of the case, the
         display's flat cable has to leave to the left, the module's antenna
         has to look off the board;
      2. a part with a zone in 04-pcb-e-caixa.md goes to the middle of it;
      3. everything else goes to the centre of gravity of the parts it
         connects to, so a decoupling capacitor lands beside the pin it
         decouples and a series resistor lands between the two ends it joins;
      4. a two terminal part is turned to lie along the line between the two
         parts it connects, which is what makes the track short and straight
         instead of an L around the part.
    """
    zonas = {n: r for n, r, _c, _s in M.ZONES}
    tam_bruto: dict[str, tuple[float, float]] = {}
    for ref, (nome, _o, _n) in FPS.FP.items():
        tam_bruto[ref] = fp_load.carregar(nome)[1]

    def tam(ref: str, ang: int) -> tuple[float, float]:
        # a minimum, because a test point's courtyard is barely bigger than
        # its own pad and two of them then land on top of each other
        w, h = tam_bruto[ref]
        w, h = max(w, 1.8), max(h, 1.8)
        return (h, w) if ang % 180 else (w, h)

    lugar: dict[str, tuple[float, float, int, bool]] = {}
    fx, fy = M.FUROS_DOC[0]
    raio = M.M2_DRILL_UNVERIFIED / 2 + 0.6
    postos: list[tuple[float, float, float, float]] = [
        (fx - raio, fy - raio, fx + raio, fy + raio)]
    falhas: list[str] = []

    def por(ref: str, cx: float, cy: float, ang: int = 0,
            preso: bool = False) -> None:
        w, h = tam(ref, ang)
        if preso:
            # a fixed position has to be legal on its own: overlapping here
            # silently is how two connectors end up on top of each other
            p = (cx, cy) if livre(cx, cy, w, h, postos, ref) else None
            if p is None:
                falhas.append(f"{ref}: a posicao fixa ({cx:.1f}; {cy:.1f}) nao "
                              f"esta livre para {w:.1f} x {h:.1f} mm")
        else:
            p = espiral(cx, cy, w, h, postos, ref=ref)
        if p is None:
            falhas.append(f"{ref}: nao coube perto de ({cx:.1f}; {cy:.1f})")
            return
        lugar[ref] = (p[0], p[1], ang, ref in ATRAS)
        postos.append((p[0] - w / 2, p[1] - h / 2, p[0] + w / 2, p[1] + h / 2))

    # ---- 1. the parts whose orientation the case decides ----
    for ref, (cx, cy, ang) in BORDA_FIXA.items():
        if ref in tam_bruto:
            por(ref, cx, cy, ang, preso=True)

    # ---- 2. the zones ----
    for ref, zona in ANCORAS.items():
        if ref in lugar:
            continue
        if ref not in tam_bruto:
            falhas.append(f"{ref}: sem footprint, nao colocado")
            continue
        x0, y0, x1, y1 = zonas[zona]
        ang = ROTACAO.get(ref, 0)
        por(ref, round((x0 + x1) / 2 / PASSO) * PASSO,
            round((y0 + y1) / 2 / PASSO) * PASSO, ang)

    for ref, dono in JUNTO.items():
        if ref not in tam_bruto or dono not in lugar or ref in lugar:
            continue
        dx, dy = lugar[dono][0], lugar[dono][1]
        por(ref, dx, dy, ROTACAO.get(ref, 0))

    # ---- 3 and 4. the rest, by connectivity, turned along it ----
    ligados: dict[str, set[str]] = {}
    for _nome, pinos in N.NETS.items():
        refs = {r for r, _p in pinos}
        if len(refs) > 8:          # a rail joins everything: it says nothing
            continue
        for a in refs:
            ligados.setdefault(a, set()).update(refs - {a})

    # which parts each terminal of a two pin part talks to
    por_terminal: dict[str, dict[str, set[str]]] = {}
    for _nome, pinos in N.NETS.items():
        refs = {r for r, _p in pinos}
        if len(refs) > 8:
            continue
        for ref, pino in pinos:
            if len(P.PARTS[ref].pins) != 2:
                continue
            por_terminal.setdefault(ref, {}).setdefault(pino, set()).update(
                refs - {ref})

    def angulo(ref: str) -> int:
        """Lay a two terminal part along the line between its two ends."""
        if ref in ROTACAO:
            return ROTACAO[ref]
        lados = por_terminal.get(ref)
        if not lados or len(lados) < 2:
            return 0
        centros = []
        for _pino, outros in sorted(lados.items()):
            pts = [lugar[o] for o in outros if o in lugar]
            if not pts:
                return 0
            centros.append((sum(p[0] for p in pts) / len(pts),
                            sum(p[1] for p in pts) / len(pts)))
        dx = centros[1][0] - centros[0][0]
        dy = centros[1][1] - centros[0][1]
        return 0 if abs(dx) >= abs(dy) else 90

    resto = [r for r in FPS.FP if r not in lugar]
    while resto:
        pontuacao = {r: len(ligados.get(r, set()) & set(lugar)) for r in resto}
        melhor = max(pontuacao.values())
        if melhor == 0:
            for ref in sorted(resto):
                por(ref, M.W / 2, M.H / 2, ROTACAO.get(ref, 0))
            break
        onda = [r for r in resto if pontuacao[r] == melhor]
        for ref in sorted(onda):
            vizinhos = [lugar[v] for v in ligados.get(ref, ()) if v in lugar]
            cx = sum(v[0] for v in vizinhos) / len(vizinhos)
            cy = sum(v[1] for v in vizinhos) / len(vizinhos)
            por(ref, round(cx / PASSO) * PASSO, round(cy / PASSO) * PASSO,
                angulo(ref))
        resto = [r for r in resto if r not in lugar]

    return lugar, falhas


def cabecalho(n_redes: int, nomes: dict[str, int]) -> str:
    lay = [(0, "F.Cu", "signal", "top_cu"), (1, "In1.Cu", "signal", "gnd"),
           (2, "In2.Cu", "signal", "pwr"), (31, "B.Cu", "signal", "bottom_cu"),
           (32, "B.Adhes", "user", "B.Adhesive"), (33, "F.Adhes", "user", "F.Adhesive"),
           (34, "B.Paste", "user", None), (35, "F.Paste", "user", None),
           (36, "B.SilkS", "user", "B.Silkscreen"), (37, "F.SilkS", "user", "F.Silkscreen"),
           (38, "B.Mask", "user", None), (39, "F.Mask", "user", None),
           (40, "Dwgs.User", "user", "User.Drawings"), (41, "Cmts.User", "user", "User.Comments"),
           (42, "Eco1.User", "user", "User.Eco1"), (43, "Eco2.User", "user", "User.Eco2"),
           (44, "Edge.Cuts", "user", None), (45, "Margin", "user", None),
           (46, "B.CrtYd", "user", "B.Courtyard"), (47, "F.CrtYd", "user", "F.Courtyard"),
           (48, "B.Fab", "user", None), (49, "F.Fab", "user", None)]
    rows = "\n".join(f'\t\t({n} "{nm}" {ty}' + (f' "{al}")' if al else ")")
                     for n, nm, ty, al in lay)
    pilha = [('\t\t\t(layer "F.SilkS"\n\t\t\t\t(type "Top Silk Screen")\n\t\t\t)'),
             ('\t\t\t(layer "F.Paste"\n\t\t\t\t(type "Top Solder Paste")\n\t\t\t)'),
             ('\t\t\t(layer "F.Mask"\n\t\t\t\t(type "Top Solder Mask")\n'
              '\t\t\t\t(thickness 0.01)\n\t\t\t)')]
    for i, nm in enumerate(CU_LAYERS):
        pilha.append(f'\t\t\t(layer "{nm}"\n\t\t\t\t(type "copper")\n'
                     f'\t\t\t\t(thickness {CU})\n\t\t\t)')
        if i < 3:
            tipo = "core" if i == 1 else "prepreg"
            pilha.append(f'\t\t\t(layer "dielectric {i + 1}"\n\t\t\t\t(type "{tipo}")\n'
                         f'\t\t\t\t(thickness {DIEL:.4f})\n\t\t\t\t(material "FR4")\n'
                         f'\t\t\t\t(epsilon_r 4.5)\n\t\t\t\t(loss_tangent 0.02)\n\t\t\t)')
    pilha += [('\t\t\t(layer "B.Mask"\n\t\t\t\t(type "Bottom Solder Mask")\n'
               '\t\t\t\t(thickness 0.01)\n\t\t\t)'),
              ('\t\t\t(layer "B.Paste"\n\t\t\t\t(type "Bottom Solder Paste")\n\t\t\t)'),
              ('\t\t\t(layer "B.SilkS"\n\t\t\t\t(type "Bottom Silk Screen")\n\t\t\t)'),
              '\t\t\t(copper_finish "None")\n\t\t\t(dielectric_constraints no)']
    ax = P_(0.0, 0.0)
    redes_txt = "\n".join(f'\t(net {i} "{nome}")'
                          for nome, i in sorted(nomes.items(), key=lambda kv: kv[1]))
    return ("(kicad_pcb\n\t(version 20240108)\n\t(generator \"pcbnew\")\n"
            "\t(generator_version \"8.0\")\n\t(general\n\t\t(thickness 0.8)\n"
            "\t\t(legacy_teardrops no)\n\t)\n\t(paper \"A4\")\n\t(layers\n" + rows + "\n\t)\n"
            "\t(setup\n\t\t(stackup\n" + "\n".join(pilha) + "\n\t\t)\n"
            "\t\t(pad_to_mask_clearance 0)\n"
            f"\t\t(aux_axis_origin {ax[0]:.4f} {ax[1]:.4f})\n"
            f"\t\t(grid_origin {ax[0]:.4f} {ax[1]:.4f})\n\t)\n" + redes_txt)


def contorno(saida: list[str], r: float) -> None:
    w = 0.1

    def linha(a, b):
        saida.append(f'\t(gr_line\n\t\t(start {a[0]:.4f} {a[1]:.4f})\n'
                     f'\t\t(end {b[0]:.4f} {b[1]:.4f})\n'
                     f'\t\t(stroke (width {w}) (type solid))\n\t\t(layer "Edge.Cuts")\n'
                     f'\t\t(uuid "{uid("l", a, b)}")\n\t)')

    def arco(c, a0, a1):
        def pt(d):
            return (c[0] + r * math.cos(math.radians(d)), c[1] + r * math.sin(math.radians(d)))
        s, m, e = pt(a0), pt((a0 + a1) / 2), pt(a1)
        saida.append(f'\t(gr_arc\n\t\t(start {s[0]:.4f} {s[1]:.4f})\n'
                     f'\t\t(mid {m[0]:.4f} {m[1]:.4f})\n\t\t(end {e[0]:.4f} {e[1]:.4f})\n'
                     f'\t\t(stroke (width {w}) (type solid))\n\t\t(layer "Edge.Cuts")\n'
                     f'\t\t(uuid "{uid("a", c, a0)}")\n\t)')

    linha(P_(r, 0.0), P_(M.W - r, 0.0))
    linha(P_(M.W, r), P_(M.W, M.H - r))
    linha(P_(M.W - r, M.H), P_(r, M.H))
    linha(P_(0.0, M.H - r), P_(0.0, r))
    arco(P_(M.W - r, r), 270, 360)
    arco(P_(M.W - r, M.H - r), 0, 90)
    arco(P_(r, M.H - r), 90, 180)
    arco(P_(r, r), 180, 270)


def keepout(nome: str, x0: float, y0: float, x1: float, y1: float) -> str:
    pts = [P_(x0, y0), P_(x1, y0), P_(x1, y1), P_(x0, y1)]
    poly = "\n".join(f"\t\t\t\t(xy {px:.4f} {py:.4f})" for px, py in pts)
    camadas = " ".join(f'"{ly}"' for ly in CU_LAYERS)
    return (f'\t(zone\n\t\t(net 0)\n\t\t(net_name "")\n\t\t(layers {camadas})\n'
            f'\t\t(uuid "{uid("z", nome)}")\n\t\t(name "{nome}")\n\t\t(hatch edge 0.5)\n'
            '\t\t(connect_pads\n\t\t\t(clearance 0)\n\t\t)\n\t\t(min_thickness 0.25)\n'
            '\t\t(filled_areas_thickness no)\n\t\t(keepout\n\t\t\t(tracks not_allowed)\n'
            # An antenna zone forbids COPPER, not parts: the radio module's
            # own antenna sits inside its own keep-out, and its pads with it.
            '\t\t\t(vias not_allowed)\n\t\t\t(pads allowed)\n'
            '\t\t\t(copperpour not_allowed)\n\t\t\t(footprints allowed)\n\t\t)\n'
            '\t\t(fill\n\t\t\t(thermal_gap 0.5)\n\t\t\t(thermal_bridge_width 0.5)\n\t\t)\n'
            f'\t\t(polygon\n\t\t\t(pts\n{poly}\n\t\t\t)\n\t\t)\n\t)')


def virar(corpo: str) -> str:
    """Flip a footprint to the back face.

    KiCad does not flip a footprint when its layer says B.Cu: the file has to
    already hold the mirrored geometry and the back layer names. Leaving the
    pads on F.Cu under a footprint declared on B.Cu is what makes the DRC say
    two pads of the same connector are shorted with zero clearance - which is
    what it said before this existed.
    """
    for f, b in (('"F.Cu"', '"B.Cu"'), ('"F.Paste"', '"B.Paste"'),
                 ('"F.Mask"', '"B.Mask"'), ('"F.SilkS"', '"B.SilkS"'),
                 ('"F.CrtYd"', '"B.CrtYd"'), ('"F.Fab"', '"B.Fab"'),
                 ('"F.Adhes"', '"B.Adhes"')):
        corpo = corpo.replace(f, b)

    # mirror x on every coordinate the footprint carries
    saida: list[str] = []
    i = 0
    chaves = ("(at ", "(start ", "(end ", "(center ", "(mid ", "(xy ")
    while i < len(corpo):
        for k in chaves:
            if corpo.startswith(k, i):
                j = corpo.index(")", i) if ")" in corpo[i:] else len(corpo)
                dentro = corpo[i + len(k):j].split()
                if dentro and _numero(dentro[0]):
                    dentro[0] = f"{-float(dentro[0]):g}"
                    if k == "(at " and len(dentro) == 3 and _numero(dentro[2]):
                        dentro[2] = f"{(180.0 - float(dentro[2])) % 360:g}"
                saida.append(k + " ".join(dentro) + ")")
                i = j + 1
                break
        else:
            saida.append(corpo[i])
            i += 1
    return "".join(saida)


def _numero(s: str) -> bool:
    try:
        float(s)
        return True
    except ValueError:
        return False


def girar_pads(corpo: str, ang: int) -> str:
    """Turn every pad by the footprint's angle.

    In a .kicad_pcb a pad's own angle is ABSOLUTE, not relative to its
    footprint: KiCad rotates the pad's position with the footprint but not the
    pad's body. Leave it out and a rotated connector's pads keep their
    original orientation while their centres turn, so they overlap and the DRC
    reports zero clearance between neighbours - which is what it reported
    until this existed.
    """
    if ang % 360 == 0:
        return corpo
    saida: list[str] = []
    i = 0
    dentro_pad = False
    while i < len(corpo):
        if corpo.startswith("(pad ", i):
            dentro_pad = True
        elif dentro_pad and corpo.startswith("(at ", i):
            j = corpo.index(")", i)
            v = corpo[i + 4:j].split()
            a = float(v[2]) if len(v) > 2 else 0.0
            saida.append(f"(at {v[0]} {v[1]} {(a + ang) % 360:g})")
            i = j + 1
            dentro_pad = False
            continue
        saida.append(corpo[i])
        i += 1
    return "".join(saida)


def plano_de_terra(numero: int, camadas: tuple[str, ...], recuo: float) -> str:
    """A ground pour, which is what makes the inner layer a reference plane.

    04-pcb-e-caixa.md is explicit that In1.Cu is a solid ground and that the
    plane is what the GNSS antenna radiates against. The zone is drawn here
    and KiCad fills it when the board is opened; the keep-outs already refuse
    a pour, so the two antenna areas stay clear on their own.
    """
    r = M.RADIUS_DRAWING
    pts = []
    for cx, cy, a0, a1 in ((r, r, 180, 270), (M.W - r, r, 270, 360),
                           (M.W - r, M.H - r, 0, 90), (r, M.H - r, 90, 180)):
        for k in range(9):
            a = math.radians(a0 + (a1 - a0) * k / 8.0)
            pts.append(P_(cx + (r - recuo) * math.cos(a) + (recuo if cx < M.W / 2 else -recuo),
                          cy + (r - recuo) * math.sin(a) + (recuo if cy < M.H / 2 else -recuo)))
    poly = "\n".join(f"\t\t\t\t(xy {px:.4f} {py:.4f})" for px, py in pts)
    lay = " ".join(f'"{ly}"' for ly in camadas)
    return (f'\t(zone\n\t\t(net {numero})\n\t\t(net_name "GND")\n\t\t(layers {lay})\n'
            f'\t\t(uuid "{uid("gnd", camadas)}")\n\t\t(name "PLANO_GND")\n'
            '\t\t(hatch edge 0.5)\n\t\t(priority 0)\n'
            '\t\t(connect_pads\n\t\t\t(clearance 0.2)\n\t\t)\n'
            '\t\t(min_thickness 0.2)\n\t\t(filled_areas_thickness no)\n'
            '\t\t(fill yes\n\t\t\t(thermal_gap 0.3)\n\t\t\t(thermal_bridge_width 0.3)\n\t\t)\n'
            f'\t\t(polygon\n\t\t\t(pts\n{poly}\n\t\t\t)\n\t\t)\n\t)')


def com_redes(corpo: str, ref: str, por_pad: dict, numeros: dict) -> str:
    """Put the net of each pad into the footprint body."""
    saida = []
    for linha in corpo.split("\n"):
        saida.append(linha)
        s = linha.strip()
        if s.startswith('(pad "'):
            num = s.split('"')[1]
            nome = por_pad.get((ref, num))
            if nome:
                ident = linha[:len(linha) - len(linha.lstrip())]
                saida.append(f'{ident}\t(net {numeros[nome]} "{nome}")')
    return "\n".join(saida)


def main() -> int:
    numeros, por_pad = redes()
    lugar, falhas = colocar()

    saida: list[str] = []
    contorno(saida, M.RADIUS_DRAWING)
    for nome, (x0, y0, x1, y1), _c, src in M.ZONES:
        if nome in KEEPOUTS:
            saida.append(keepout(nome, x0, y0, x1, y1))
        else:
            a, b = P_(x0, y0), P_(x1, y1)
            saida.append(
                f'\t(gr_rect\n\t\t(start {a[0]:.4f} {a[1]:.4f})\n'
                f'\t\t(end {b[0]:.4f} {b[1]:.4f})\n'
                f'\t\t(stroke (width 0.1) (type dash))\n\t\t(fill none)\n'
                f'\t\t(layer "Dwgs.User")\n\t\t(uuid "{uid("r", nome)}")\n\t)')

    # the ground planes: In1.Cu solid, and the leftover copper on the back
    if "GND" in numeros:
        saida.append(plano_de_terra(numeros["GND"], ("In1.Cu",), 0.3))
        saida.append(plano_de_terra(numeros["GND"], ("B.Cu",), 0.3))

    # the single mounting hole
    fx, fy = P_(*M.FUROS_DOC[0])
    furo = pathlib.Path(
        r"D:\KiCAD\share\kicad\footprints\MountingHole.pretty"
        r"\MountingHole_2.2mm_M2.kicad_mod").read_text(encoding="utf-8")
    corpo_furo = furo[furo.index("\n"):].rstrip()[:-1].rstrip()
    saida.append('\t(footprint "MountingHole:MountingHole_2.2mm_M2"\n'
                 f'\t\t(at {fx:.4f} {fy:.4f})\n\t\t(uuid "{uid("furo")}")'
                 + corpo_furo.replace("\n", "\n\t") + "\n\t)")

    # the parts
    for ref, (x, y, ang, atras) in sorted(lugar.items()):
        nome_fp = FPS.FP[ref][0]
        corpo = fp_load.corpo(nome_fp)
        if atras:
            corpo = virar(corpo)
        corpo = girar_pads(corpo, ang)
        corpo = com_redes(corpo, ref, por_pad, numeros)
        px, py = P_(x, y)
        camada = "B.Cu" if atras else "F.Cu"
        cab = (f'\t(footprint "{nome_fp}"\n\t\t(layer "{camada}")\n'
               f'\t\t(at {px:.4f} {py:.4f} {ang})\n'
               f'\t\t(uuid "{uid("fp", ref)}")\n'
               f'\t\t(property "Reference" "{ref}"\n\t\t\t(at 0 -1.8 0)\n'
               f'\t\t\t(layer "{"B" if atras else "F"}.SilkS")\n'
               f'\t\t\t(uuid "{uid("fpref", ref)}")\n'
               '\t\t\t(effects (font (size 0.7 0.7) (thickness 0.1))'
               + (' (justify mirror)' if atras else '') + ')\n\t\t)\n'
               f'\t\t(property "Value" "{P.PARTS[ref].value}"\n\t\t\t(at 0 1.8 0)\n'
               f'\t\t\t(layer "{"B" if atras else "F"}.Fab")\n'
               f'\t\t\t(uuid "{uid("fpval", ref)}")\n'
               '\t\t\t(effects (font (size 0.6 0.6) (thickness 0.1))'
               + (' (justify mirror)' if atras else '') + ')\n\t\t)')
        saida.append(cab + corpo + "\n\t)")

    notas = [
        "GNSS BIKE COMPUTER - placa 55 x 97 mm, 0,8 mm, 4 camadas",
        f"{len(lugar)} pecas colocadas, {len(FPS.FORA_DA_PLACA)} fora da placa "
        "(painel, antena e modulos solares vivem na caixa)",
        "furo M2 unico; reparticao do dieletrico PROVISORIA (0,22 mm por vao)",
        "NADA ROTEADO, NADA FABRICADO, NADA MEDIDO",
    ]
    for i, s in enumerate(notas):
        p = P_(0.0, M.H + 3.0 + i * 2.0)
        saida.append(f'\t(gr_text "{s}"\n\t\t(at {p[0]:.4f} {p[1]:.4f} 0)\n'
                     f'\t\t(layer "Cmts.User")\n\t\t(uuid "{uid("t", i)}")\n'
                     '\t\t(effects (font (size 1.2 1.2) (thickness 0.2)) '
                     '(justify left top))\n\t)')

    out = HERE / "gnssbike.kicad_pcb"
    out.write_text(cabecalho(len(numeros), numeros) + "\n" + "\n".join(saida) + "\n)\n",
                   encoding="utf-8", newline="\n")
    print(f"{out.name}: {len(lugar)} pecas, {len(numeros) - 1} redes, "
          f"{len(KEEPOUTS)} areas de regra")
    if falhas:
        print(f"  NAO COLOCADAS: {len(falhas)}")
        for f in falhas:
            print(f"    {f}")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
