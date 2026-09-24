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
# The stack-up is ASYMMETRIC, and that is the point. Section 4.4 of the
# MAX-F10S integration manual says to "select the stack-up, copper, and
# dielectric properties of the PCB accordingly to fulfil this condition" -
# the condition being 50 ohm on the RF line. With the dielectric split
# evenly, 0.22 mm each, a 50 ohm microstrip is 0.431 mm wide and does not fit
# between the module's pads: it cannot leave its own pin. A thin prepreg to
# the ground plane and a thick core in the middle brings it to 0.196 mm,
# which routes, and it is the usual four layer stack-up for a board with RF.
DIEL_RF = 0.10                 # F.Cu to the ground plane on In1.Cu
DIEL_NUCLEO = 0.80 - 4 * CU - 2 * DIEL_RF    # In1.Cu to In2.Cu
DIEL = (DIEL_RF, DIEL_NUCLEO, DIEL_RF)
CU_LAYERS = ("F.Cu", "In1.Cu", "In2.Cu", "B.Cu")
KEEPOUTS = {"KEEPOUT_ANTENA_GNSS", "KEEPOUT_ANTENA_MODULO"}
BORDA = 0.8                    # keep parts this far inside the outline
FOLGA = 0.05                   # between two courtyards: the courtyard
                               # already carries the maker's clearance
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
    # The mouth of a USB-C faces +Y in this footprint: the contacts leave at
    # the back, so the body sits on the far side of the pads. At 180 it was
    # pointing INTO the board, which no rule catches and no cable forgives.
    "J101": (18.0, 91.66, 0),     # USB-C, bottom edge, mouth out
    # 10.2 mm apart: the courtyard is 10.0 wide and two of them at 10.0
    # touch, which the placer refuses and is right to refuse
    "SW601": (7.0, 80.5, 0),      # the three keys, below the display and
    "SW602": (17.5, 80.5, 0),     # left of the antenna's 5 mm clear band
    "SW603": (28.0, 80.5, 0),
    # bottom right CORNER, which is the datasheet's "Best" (7.5, figure 1):
    # antenna over the notch, off the board edge, and as far from the GNSS
    # antenna as the board allows. x so that the courtyard ends exactly on
    # the edge: 55 - 17/2. y leaves 5.75 mm below the module, because the
    # power pins on its lower castellated edge need a capacitor within
    # 0.5 mm and a 0603 courtyard does not fit against the border margin.
    "U201": (46.5, 84.5, 270),
    "J401": (5.0, 35.5, 270),     # display flat cable, out to the left
    "J402": (4.3, 24.0, 270),     # the light's cable, same side
    # on the back face the footprint is mirrored, so the angle that sends
    # the cable to the left is 270, not 90
    "J102": (4.0, 60.0, 270),     # battery connector, back face, cable left
    # The light sensor moved out of the bottom left corner. Its datasheet
    # asks for every nearby component to be at least twice its own height
    # away, because of secondary optical reflections, and the 5 mm tactile
    # key was 4.55 mm from it against the 10 mm that rule gives. Up here the
    # nearest part of known height is the GNSS module, 2.4 mm tall and
    # 16.8 mm away. The case window follows the sensor.
    "U505": (2.5, 12.0, 0),       # ambient light, under its window
    "D601": (51.0, 10.2, 0),      # RGB LED, under its light pipe
    "U301": (27.5, 13.8, 0),      # GNSS receiver, just below the antenna zone
    # Where the antenna in the case wall lands: just under its keep-out, a
    # few millimetres from the pi network, which is what "linha de 50 ohm,
    # poucos mm" in 04-pcb-e-caixa.md asks for.
    "J302": (14.0, 10.0, 0),
    # The three groups of solar modules, down the left edge in the bands the
    # display and battery connectors leave free. They carry the harvester's
    # SRC node, which is high impedance and low voltage, so they sit as close
    # to the harvester's band as the edge allows.
    # In the free bands of the two edges, computed rather than guessed: the
    # left edge is taken by the light sensor, the two flat cables, the M2
    # hole, the battery connector and a key, which leaves y 66 to 77. The
    # other two go on the right edge, clear of the 5 mm band around the
    # radio antenna, which starts at y 70. All three stay within about
    # 15 mm of the harvester: SRC is a high impedance node coming off a
    # solar cell and a long run of it picks up everything.
    "J103": (2.5, 71.4, 90),
    "J104": (52.5, 58.0, 90),
    "J105": (52.5, 66.0, 90),
}



# Which chip each capacitor decouples, from 05-materiais.md. The netlist
# cannot say it: a decoupling capacitor sits between a rail and ground, and
# the rail touches everything. The bill of materials is where it is written -
# "C101 a C109: entradas e saidas do nPM1300", "C113, C114: entrada e saida do
# TPS7A02", "C115, C116: CSRC e CINT do AEM10900" - and without it the placer
# sends the capacitor to the centre of gravity of a rail, which is nowhere.
DECOPLA: dict[str, str] = {
    "C101": "U101", "C102": "U101", "C103": "U101", "C104": "U101",
    "C105": "U101", "C106": "U101", "C107": "U101", "C108": "U101",
    "C109": "U101", "C110": "U101", "C111": "U101", "C112": "U101",
    "C113": "U104", "C114": "U104",
    "C115": "U103", "C116": "U103", "C117": "U103",
    "C118": "U102",
    "C201": "U201", "C210": "U201",
    "C301": "E301", "C302": "U301", "C303": "U301", "C304": "U301",
    "C404": "J401",
    "C501": "U501", "C502": "U504", "C503": "U503",
    "C601": "SW601", "C602": "SW602", "C603": "SW603",
    # the pull-ups of a bus go beside the master, not beside a slave
    "R107": "U201", "R108": "U201", "R504": "U201", "R505": "U201",
    "R109": "U201",
    # the series resistors of the flash: two at the MCU, one at the flash
    "R506": "U201", "R507": "U201", "R508": "U501",
    # the gate pull-downs go at the transistor
    "R402": "Q401", "R609": "Q601", "R610": "Q602", "R611": "Q603",
}

# A part is allowed inside the keep-out that exists because of it: the
# radio module sits over its own antenna zone, which forbids copper, not it.
DONO_DO_KEEPOUT: dict[str, str] = {"U201": "KEEPOUT_ANTENA_MODULO"}

# What the ME54BS13 datasheet actually asks around its antenna, read in
# V1.0.0 and not in a summary of it:
#
#   7.3  no copper pour, no component and no fully enclosed metal housing
#        over the antenna area, and the RF side never faces inward;
#   7.4  "no signal traces, metal objects, or other interference sources
#        should exist within 3-5 mm around the antenna area", the module at
#        the edge or corner, and the PCB beneath the antenna hollowed out.
#
# There is NO "20 mm from a switching converter" rule; this file carried one
# until 2026-09-24, and while it kept the power supply 20 mm away it let ten
# other parts sit inside 5 mm of the antenna - one of them at 0.8 mm.
# It applies to EVERY part, not to a chosen list.
DIST_ANTENA = 5.0

# ME54BS13 V1.0.0, 7.2, "Interference Isolation Rule": 20 mm between the
# module and a DC-DC switching supply, a power inductor or a transformer.
#
# This constraint was in this file, was taken out on 2026-09-24 on the wrong
# conclusion that the rule did not exist, and is back. It does exist - the
# section is 7.2, not the 7.3 that was looked in - and taking it out let a
# power inductor sit 6.8 mm from the module.
#
# It is measured to the MODULE's courtyard, which is what the datasheet says,
# not to the antenna band: the sentence is "the module must maintain a
# minimum safe distance from strong interference sources".
LONGE_DO_MODULO = 20.0
CHAVEIA = {"L101", "L102", "L103", "U101", "U103"}
MODULO_DE_RADIO = "U201"
ANTENA_DO_RADIO = "KEEPOUT_ANTENA_MODULO"
# The module's OWN decoupling is exempt, and it has to be. Section 7.2 puts
# the capacitor 0.5 mm from the power pin, and the module's power pads sit
# 2.1 mm from its own antenna band - so no point exists that satisfies both
# 0.5 mm from the pin and 5 mm from the antenna. The 3-5 mm of 7.4 is about
# foreign interference sources, not about the module's own support parts,
# which every figure in 7.5 draws right against it.
DO_MODULO = {r for r, dono in ()} | {"C201", "C210"}

# Rotations that are not about the case but about the circuit.

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


def livre(x: float, y: float, bx: tuple[float, float, float, float],
          postos: list[tuple[float, float, float, float]],
          ref: str = "", borda: float | None = None) -> bool:
    """Is the courtyard, placed at (x, y), inside the board and free?

    The box comes relative to the footprint origin, not centred on it: a
    connector's courtyard sits to one side of its pads, and treating it as
    centred is what pushed every decoupling capacitor two millimetres further
    out than it had to be.
    """
    eps = 1e-6
    b = BORDA if borda is None else borda
    x0, y0, x1, y1 = x + bx[0], y + bx[1], x + bx[2], y + bx[3]
    if x0 < b - eps or y0 < b - eps or             x1 > M.W - b + eps or y1 > M.H - b + eps:
        return False
    r = M.RADIUS_DRAWING
    for cx, cy in ((r, r), (M.W - r, r), (r, M.H - r), (M.W - r, M.H - r)):
        qx = min(max(x0, cx - r if cx < M.W / 2 else -1e9),
                 cx + r if cx > M.W / 2 else 1e9)
        del qx
    if ref in CHAVEIA:
        mx, my, mang = BORDA_FIXA[MODULO_DE_RADIO]
        m = caixa(MODULO_DE_RADIO, mang)
        dx = max(mx + m[0] - x1, x0 - (mx + m[2]), 0.0)
        dy = max(my + m[1] - y1, y0 - (my + m[3]), 0.0)
        if math.hypot(dx, dy) < LONGE_DO_MODULO:
            return False
    for nome, (kx0, ky0, kx1, ky1), _c, _s in M.ZONES:
        if nome == ANTENA_DO_RADIO and DONO_DO_KEEPOUT.get(ref) != nome                 and ref not in DO_MODULO:
            dx = max(kx0 - x1, x0 - kx1, 0.0)
            dy = max(ky0 - y1, y0 - ky1, 0.0)
            if math.hypot(dx, dy) < DIST_ANTENA:
                return False
        if nome not in KEEPOUTS or DONO_DO_KEEPOUT.get(ref) == nome:
            continue
        if x1 > kx0 and kx1 > x0 and y1 > ky0 and ky1 > y0:
            return False
    for px0, py0, px1, py1 in postos:
        if x1 + FOLGA > px0 and px1 + FOLGA > x0 and \
                y1 + FOLGA > py0 and py1 + FOLGA > y0:
            return False
    return True


def espiral(cx: float, cy: float, bx: tuple[float, float, float, float],
            postos: list, raio_max: float = 60.0, ref: str = ""):
    """The nearest free slot to (cx, cy), searched outwards."""
    if livre(cx, cy, bx, postos, ref):
        return (cx, cy)
    passo = PASSO
    r = passo
    while r <= raio_max:
        n = max(8, int(2 * math.pi * r / passo))
        for i in range(n):
            a = 2 * math.pi * i / n
            x = round((cx + r * math.cos(a)) / PASSO) * PASSO
            y = round((cy + r * math.sin(a)) / PASSO) * PASSO
            if livre(x, y, bx, postos, ref):
                return (x, y)
        r += passo
    return None


def caixa(ref: str, ang: int) -> tuple[float, float, float, float]:
    """The real courtyard, turned: x0, y0, x1, y1 around the origin.

    Module level on purpose. While this lived inside colocar() the checker
    had a copy of its own - the symmetric box, twice the furthest edge, with
    a 1.8 mm floor - and the two disagreed: the checker reported 54
    overlapping courtyards on a board where KiCad reported one. A rule
    measured by two definitions is not a rule.
    """
    fp_load.carregar(FPS.FP[ref][0])
    x0, y0, x1, y1 = fp_load.CAIXA[FPS.FP[ref][0]]
    for _ in range((ang // 90) % 4):
        x0, y0, x1, y1 = y0, -x1, y1, -x0
    # a minimum, because a test point's courtyard is barely bigger than
    # its own pad and two of them then land on top of each other
    if x1 - x0 < 1.2:
        m = (x0 + x1) / 2
        x0, x1 = m - 0.6, m + 0.6
    if y1 - y0 < 1.2:
        m = (y0 + y1) / 2
        y0, y1 = m - 0.6, m + 0.6
    return (x0, y0, x1, y1)


def tam(ref: str, ang: int) -> tuple[float, float]:
    x0, y0, x1, y1 = caixa(ref, ang)
    return (x1 - x0, y1 - y0)


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

    lugar: dict[str, tuple[float, float, int, bool]] = {}
    fx, fy = M.FUROS_DOC[0]
    raio = M.M2_DRILL_UNVERIFIED / 2 + 0.6
    postos: list[tuple[float, float, float, float]] = [
        (fx - raio, fy - raio, fx + raio, fy + raio)]
    falhas: list[str] = []

    def por(ref: str, cx: float, cy: float, ang: int = 0,
            preso: bool = False) -> None:
        bx = caixa(ref, ang)
        if preso:
            # a fixed position has to be legal on its own: overlapping here
            # silently is how two connectors end up on top of each other.
            # A connector that comes out of the case may touch the edge - that
            # is the point of it - so the edge margin does not apply to it.
            p = (cx, cy) if livre(cx, cy, bx, postos, ref, borda=0.0) else None
            if p is None:
                falhas.append(f"{ref}: a posicao fixa ({cx:.1f}; {cy:.1f}) nao "
                              f"esta livre para o contorno {bx}")
        else:
            p = espiral(cx, cy, bx, postos, ref=ref)
        if p is None:
            falhas.append(f"{ref}: nao coube perto de ({cx:.1f}; {cy:.1f})")
            return
        lugar[ref] = (p[0], p[1], ang, ref in ATRAS)
        postos.append((p[0] + bx[0], p[1] + bx[1], p[0] + bx[2], p[1] + bx[3]))

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

    # ---- 3. decoupling and inductors, hugging the pin they serve ----
    # The datasheets do not say "near": the ME54BS13 asks for 0.5 mm between a
    # capacitor's pad and the power pin, and both the nPM1300 and the AEM10900
    # ask for the inductor and the reactive parts "as close as possible" to
    # their pins. A capacitor 7 mm away is 15 nH of loop, which at 200 mA and a
    # 2 ns edge is 1.5 V of ringing on a 3.7 V rail - the capacitor stops being
    # a capacitor and becomes part of the problem.
    def pad_global(ref: str, numero: str) -> tuple[float, float] | None:
        if ref not in lugar:
            return None
        nome_fp = FPS.FP[ref][0]
        texto = fp_load.carregar(nome_fp)[0]
        arv = fp_load.parse(texto)
        for p in fp_load.kids(arv, "pad"):
            if p[1] != numero:
                continue
            a = fp_load.kid(p, "at")
            px, py = float(a[1]), float(a[2])
            x, y, ang, atras = lugar[ref]
            if atras:
                px = -px
            r = math.radians(ang)
            return (x + px * math.cos(r) + py * math.sin(r),
                    y - px * math.sin(r) + py * math.cos(r))
        return None

    def pads_locais(ref: str) -> list[tuple[float, float]]:
        """Every pad of a footprint, as an offset from the footprint origin."""
        arv = fp_load.parse(fp_load.carregar(FPS.FP[ref][0])[0])
        saida = []
        for p in fp_load.kids(arv, "pad"):
            a = fp_load.kid(p, "at")
            if a:
                saida.append((float(a[1]), float(a[2])))
        return saida

    def encostar(ref: str) -> bool:
        """Put a two terminal part right beside the pin it serves."""
        if ref in lugar or ref not in tam_bruto or len(P.PARTS[ref].pins) != 2:
            return False
        melhor = None
        # the bill of materials decides first; the netlist only breaks ties
        dono_bom = DECOPLA.get(ref)
        for _nome, pinos in N.NETS.items():
            refs = {r for r, _p in pinos}
            if ref not in refs:
                continue
            if dono_bom is None and len(refs) > 8:
                continue
            if dono_bom is not None and dono_bom not in refs:
                continue
            for outro, pino in pinos:
                if outro == ref or outro not in lugar:
                    continue
                if dono_bom is not None and outro != dono_bom:
                    continue
                if dono_bom is None and len(P.PARTS[outro].pins) < 4:
                    continue
                numero = next(q.number for q in P.PARTS[outro].pins
                              if q.name == pino or q.number == pino)
                p = pad_global(outro, numero)
                if p and (melhor is None or len(P.PARTS[outro].pins) > melhor[2]):
                    melhor = (p, outro, len(P.PARTS[outro].pins))
        if melhor is None:
            return False
        (px, py), dono, _n = melhor
        dx, dy, _a, _b = lugar[dono]
        v = math.hypot(px - dx, py - dy) or 1.0
        ux, uy = (px - dx) / v, (py - dy) / v

        # What every one of these datasheets constrains is the loop from the
        # supply PIN to the capacitor's own PAD, and that is not the distance
        # between the two parts' centres: a 0402 turned the wrong way puts its
        # near pad half a millimetre further out for nothing, and stepping a
        # fixed 0.6 mm out of the chip ignores which pad ends up facing the
        # pin. So each free candidate is scored pad to pad, both orientations
        # are tried, and the best wins. C111 sat 2.02 mm from U101's 3V3BL pin
        # against a 2.00 limit while a slot 1.90 mm away was free one grid
        # step up - the old rule could not see it, because by centre distance
        # that slot was the worse of the two.
        locais = pads_locais(ref)
        fora = max((math.hypot(a, b) for a, b in locais), default=0.0)
        espelha = -1.0 if ref in ATRAS else 1.0
        melhor_pos, melhor_d = None, float("inf")
        caixas = {a: caixa(ref, a) for a in (0, 90)}
        raio = PASSO
        while raio <= 12.0:
            # a pad can only be `fora` closer than the part's own centre, so
            # once the ring itself is further than that, nothing on it or
            # beyond it can beat what is already in hand
            if raio - fora > melhor_d:
                break
            n = max(12, int(2 * math.pi * raio / PASSO))
            for i in range(n):
                a = 2 * math.pi * i / n
                cx = round((px + raio * math.cos(a)) / PASSO) * PASSO
                cy = round((py + raio * math.sin(a)) / PASSO) * PASSO
                for ang, bx in caixas.items():
                    if not livre(cx, cy, bx, postos, ref):
                        continue
                    r = math.radians(ang)
                    d = min(math.hypot(cx + espelha * ax * math.cos(r)
                                       + ay * math.sin(r) - px,
                                       cy - espelha * ax * math.sin(r)
                                       + ay * math.cos(r) - py)
                            for ax, ay in locais)
                    if d < melhor_d:
                        melhor_d, melhor_pos = d, (cx, cy, ang)
            raio += PASSO
        if melhor_pos is None:
            # nothing free within 12 mm: fall back to the old step-out so the
            # failure is reported by por() instead of vanishing
            ang = 0 if abs(ux) >= abs(uy) else 90
            w, h = tam(ref, ang)
            d = 0.6 + max(w, h) / 2
            por(ref, round((px + ux * d) / PASSO) * PASSO,
                round((py + uy * d) / PASSO) * PASSO, ang)
            return ref in lugar
        por(ref, melhor_pos[0], melhor_pos[1], melhor_pos[2])
        return ref in lugar

    def ordem_de_encostar(ref: str) -> tuple:
        """Who gets first pick of the ring around a chip.

        The smallest capacitor decouples the highest frequency, and it is the
        one whose loop to the pin has to be shortest: a 100 nF part 4 mm away
        is not decoupling anything above a few tens of megahertz, while a
        10 uF bulk capacitor 4 mm away is doing its job. So the ring is
        handed out by capacitance, smallest first, and only then to
        everything else. Alphabetical order - which is what this did before -
        put C101 ahead of C112 for no reason at all.
        """
        if ref not in DECOPLA or not ref.startswith("C"):
            return (2, 0.0, ref)
        # The radio module's own decoupling picks first, whatever its value:
        # its datasheet asks for 0.5 mm from the pin, which is the tightest
        # number on this board, and the module sits in a corner where the
        # room runs out. Sorting only by capacitance let a 4.7 uF that had to
        # be within 5 mm land 11.2 mm away, behind parts with no such rule.
        if DECOPLA[ref] == "U201":
            return (0, 0.0, ref)
        v = P.PARTS[ref].value.lower().replace(",", ".")
        mult = {"p": 1e-12, "n": 1e-9, "u": 1e-6, "µ": 1e-6}
        f = 1.0
        for letra, m in mult.items():
            if letra + "f" in v:
                f = m
                break
        try:
            num = float(v.split()[0])
        except (ValueError, IndexError):
            num = 1.0
        return (1, num * f, ref)

    # first pass: the parts that hug an anchor, before the other
    # chips take the ring around it
    for ref in sorted(FPS.FP, key=ordem_de_encostar):
        encostar(ref)

    for ref, dono in JUNTO.items():
        if ref not in tam_bruto or dono not in lugar or ref in lugar:
            continue
        dx, dy = lugar[dono][0], lugar[dono][1]
        por(ref, dx, dy, ROTACAO.get(ref, 0))

    # second pass: now the chips that follow an anchor are placed too
    for ref in sorted(FPS.FP, key=ordem_de_encostar):
        encostar(ref)

    # ---- 4. the rest, by connectivity, turned along it ----
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
                         f'\t\t\t\t(thickness {DIEL[i]:.4f})\n\t\t\t\t(material "FR4")\n'
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

    # The notch under the module's antenna. Section 7.4 of the ME54BS13
    # datasheet asks for the PCB beneath the antenna to be hollowed out so
    # the antenna region is suspended, and figure 1 of 7.5 - the one it calls
    # "Best" - shows exactly this: the module in a corner with its RF end
    # over the void. Taking it out to the board edge makes it a notch rather
    # than a slot, which avoids leaving a 1 mm rib of board on the outside.
    recorte = next((z for n, z, _c, _s in M.ZONES
                    if n == "RECORTE_ANTENA_MODULO"), None)

    linha(P_(r, 0.0), P_(M.W - r, 0.0))
    if recorte:
        rx0, ry0, _rx1, ry1 = recorte
        linha(P_(M.W, r), P_(M.W, ry0))
        linha(P_(M.W, ry0), P_(rx0, ry0))
        linha(P_(rx0, ry0), P_(rx0, ry1))
        linha(P_(rx0, ry1), P_(M.W, ry1))
        linha(P_(M.W, ry1), P_(M.W, M.H - r))
    else:
        linha(P_(M.W, r), P_(M.W, M.H - r))
    linha(P_(M.W - r, M.H), P_(r, M.H))
    linha(P_(0.0, M.H - r), P_(0.0, r))
    arco(P_(M.W - r, r), 270, 360)
    arco(P_(M.W - r, M.H - r), 0, 90)
    arco(P_(r, M.H - r), 90, 180)
    arco(P_(r, r), 180, 270)


def pad_no_lugar(ref: str, numero: str, lugar: dict):
    """Where a pad ends up on the board, given the placement."""
    if ref not in lugar or ref not in FPS.FP:
        return None
    arv = fp_load.parse(fp_load.carregar(FPS.FP[ref][0])[0])
    for q in fp_load.kids(arv, "pad"):
        if q[1] != numero:
            continue
        a = fp_load.kid(q, "at")
        px, py = float(a[1]), float(a[2])
        x, y, ang, atras = lugar[ref]
        if atras:
            px = -px
        r = math.radians(ang)
        return (x + px * math.cos(r) + py * math.sin(r),
                y - px * math.sin(r) + py * math.cos(r))
    return None


def sem_plano_no_chaveamento(lugar: dict, por_pad: dict) -> list[tuple]:
    """Where no copper pour may go: under a switching node.

    Section 13 of DS-AEM1090x-v2.4.0, word for word: "PCB track capacitance
    must be reduced as much as possible on the boost converter switching node
    SWDCDC. This is done as follows: keep the connection between the SWDCDC
    pin and the inductor short; REMOVE THE GROUND AND POWER PLANES UNDER THE
    SWDCDC NODE - the polygon on the opposite external layer may also be
    removed - increase the distance between SWDCDC and the ground polygon on
    the external PCB layer where the AEM1090x is mounted." And the same
    principle for TH_REF.

    A pour under a switching node is capacitance the converter has to charge
    and discharge at its switching frequency, and on a harvester that runs on
    microwatts that is not a detail. So the rectangle that holds the node's
    pads, with margin, is cut out of every plane.
    """
    zonas = []
    for rede, folga in (("SW_DCDC", 0.6), ("TH_REF", 0.4)):
        pontos = []
        for (ref, num), nome in por_pad.items():
            if nome != rede or ref not in lugar:
                continue
            q = pad_no_lugar(ref, num, lugar)
            if q:
                pontos.append(q)
        if len(pontos) < 2:
            continue
        x0 = min(q[0] for q in pontos) - folga
        y0 = min(q[1] for q in pontos) - folga
        x1 = max(q[0] for q in pontos) + folga
        y1 = max(q[1] for q in pontos) + folga
        zonas.append((f"SEM_PLANO_{rede}", (x0, y0, x1, y1)))
    return zonas


def keepout(nome: str, x0: float, y0: float, x1: float, y1: float,
            so_plano: bool = False) -> str:
    """A forbidden area. With so_plano, only the pour is forbidden.

    An antenna zone forbids every kind of copper. A switching node is
    different: section 13 of the AEM10900 datasheet asks to "remove the
    ground and power PLANES under the SWDCDC node", and the node's own track
    obviously has to be there - it is the whole point of the rule. Forbidding
    tracks there too put sixteen items_not_allowed in the DRC, every one of
    them the switching node itself.
    """
    pts = [P_(x0, y0), P_(x1, y0), P_(x1, y1), P_(x0, y1)]
    poly = "\n".join(f"\t\t\t\t(xy {px:.4f} {py:.4f})" for px, py in pts)
    camadas = " ".join(f'"{ly}"' for ly in CU_LAYERS)
    return (f'\t(zone\n\t\t(net 0)\n\t\t(net_name "")\n\t\t(layers {camadas})\n'
            f'\t\t(uuid "{uid("z", nome)}")\n\t\t(name "{nome}")\n\t\t(hatch edge 0.5)\n'
            '\t\t(connect_pads\n\t\t\t(clearance 0)\n\t\t)\n\t\t(min_thickness 0.25)\n'
            '\t\t(filled_areas_thickness no)\n\t\t(keepout\n'
            # An antenna zone forbids COPPER, not parts: the radio module's
            # own antenna sits inside its own keep-out, and its pads with it.
            # A switching node's zone forbids only the POUR, because the node
            # itself has to run there.
            f'\t\t\t(tracks {"allowed" if so_plano else "not_allowed"})\n'
            f'\t\t\t(vias {"allowed" if so_plano else "not_allowed"})\n'
            '\t\t\t(pads allowed)\n'
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
            # SOLID, not thermal reliefs. Left on the KiCad default, eleven
            # ground pads came out with fewer than two spokes - U101, U102,
            # U104, U503, U504, both shield rows of the USB-C receptacle and
            # three passives - because a track runs past the side the second
            # spoke needed. A one spoke connection is a 0.3 mm neck in series
            # with the return current of the USB shell and of the MCU, on a
            # board that carries a 2.4 GHz radio and two switching
            # converters, and it is the only thing the DRC calls an error
            # here. Thermal relief is a HAND and WAVE soldering aid; nothing
            # on this board can be soldered by hand anyway - 0402s, a WLP-9
            # and a 0.4 mm pitch QFN.
            #
            # What it costs, written down because it is a real cost: reworking
            # a ground pad by hand needs more heat, and a 0402 with one end on
            # the pour and the other not is a little likelier to tombstone at
            # reflow. If the assembly house objects, this is the one line to
            # change - `yes` back to nothing.
            '\t\t(connect_pads yes\n\t\t\t(clearance 0.2)\n\t\t)\n'
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

    # The switching nodes get their pour cut away, which is what section 13
    # of the AEM10900 datasheet asks for by name.
    for nome_z, (zx0, zy0, zx1, zy1) in sem_plano_no_chaveamento(lugar, por_pad):
        saida.append(keepout(nome_z, zx0, zy0, zx1, zy1, so_plano=True))

    # The ground planes. In1.Cu is the solid one and the reference the return
    # current follows; F.Cu and B.Cu get the leftover copper.
    #
    # The front pour is not decoration. Without it every ground pad on the
    # front face depended on a via of its own to reach In1.Cu, and in the
    # crowded corner around the power supply eighteen of the ninety-five had
    # nowhere to put one - so eighteen ground pads had no ground. With the
    # pour, KiCad ties them to it when it fills, and the stubs that do fit
    # stay as the low impedance path they were meant to be.
    if "GND" in numeros:
        saida.append(plano_de_terra(numeros["GND"], ("In1.Cu",), 0.3))
        saida.append(plano_de_terra(numeros["GND"], ("F.Cu",), 0.3))
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
