#!/usr/bin/env python3
"""Measure the board against the rules the datasheets and the standards give.

check_pcb.py answers "does KiCad accept this board": outline, footprints,
nets, DRC. It says nothing about whether the board WORKS, because no DRC
knows that a chip antenna 6 mm from a switching inductor will not radiate.
Those rules live in the manufacturers' integration notes and in IPC-2221,
and this file writes each one down with where it comes from and then
measures the board against it.

Everything printed here is measured from gnssbike.kicad_pcb, not asserted.
A rule this cannot measure is listed at the end as pending, so that the
difference between "checked and passed" and "nobody looked" stays visible.

Run: python hardware_gnssbike/cad/dry_run_pcb.py
"""

from __future__ import annotations

import math
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import footprints as FPS  # noqa: E402
import fp_load  # noqa: E402
import make_dxf as M  # noqa: E402
import make_pcb as MP  # noqa: E402
import nets as N  # noqa: E402
import parts as P  # noqa: E402

PCB = HERE / "gnssbike.kicad_pcb"

# --- the rules, with their source -------------------------------------------
# Each entry: id, what it demands, where it is written.
# Every line below was read in the manufacturer's own PDF, not in a summary
# of it, and not in an earlier version of this comment either.
#
# What this file said on 2026-09-24, and got wrong: that "20 mm between the
# antenna and a switching converter" was a rule nobody could find in the
# datasheet, and that 7.3 only carried a qualitative "do not place modules
# adjacent to strong interference sources". The 20 mm is REAL. It is in 7.2,
# under "Interference Isolation Rule", in a table of four rows - the section
# number was wrong, not the rule - and deleting it took a measured constraint
# off the placer. It is back, as ISOLACAO below, with all four rows and with
# the row nobody had ever carried: 25 mm from a display or an FPC cable.
#
# What that older comment did get right, and stays corrected: the module's
# decoupling distance is 0.5 mm (7.2, "Power Supply Design"), not the 2 mm
# this once used.
#
# The PDFs live in ../datasheets/, which the .gitignore keeps out of this
# public repository.
REGRAS = [
    ("RF1", "sem cobre, sem componente e sem caixa metalica fechada sobre a "
            "area da antena do modulo",
     "MinewSemi ME54BS13 V1.0.0, 7.3"),
    ("RF2", "o lado de RF do modulo virado para a borda, nunca para dentro "
            "da placa",
     "MinewSemi ME54BS13 V1.0.0, 7.3"),
    ("RF3", "5 mm em volta da area da antena sem trilha de sinal, sem metal e "
            "sem fonte de interferencia; modulo na borda ou no canto",
     "MinewSemi ME54BS13 V1.0.0, 7.4"),
    ("RF4", "a placa vazada sob a area da antena do modulo, deixando-a suspensa",
     "MinewSemi ME54BS13 V1.0.0, 7.4"),
    ("RF5", "5 mm entre o receptor GNSS e qualquer componente de RF",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 4.4"),
    ("RF6", "terra sob o modulo GNSS na primeira e na segunda camada, sem "
            "trilha de sinal cruzando por baixo nessas duas",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 4.4"),
    ("RF7", "a rede pi do GNSS junto ao RF_IN, com a trilha mais curta possivel",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 4.4"),
    ("RF8", "as duas antenas o mais longe possivel uma da outra",
     "u-blox MAX-F10S IM 4.4; referencia: um quarto de onda de 2,44 GHz e "
     "30,7 mm"),
    ("RF9", "a distancia de isolacao que a ficha do modulo pede de cada tipo "
            "de fonte de interferencia: 20 mm de fonte chaveada, indutor de "
            "potencia ou transformador, 20 mm de USB 3.0/HDMI/DDR/SDIO "
            "rapido, 15 mm de clock de alta frequencia de MCU ou PHY "
            "Ethernet, 25 mm de display, camera ou cabo FPC com fiacao",
     "MinewSemi ME54BS13 V1.0.0, 7.2, Interference Isolation Rule"),
    ("RF10", "50 mm entre dois modulos de radio na mesma placa",
     "MinewSemi ME54BS13 V1.0.0, 7.2, Multiple Modules on the Same PCB"),
    ("US1", "o par USB_DP/USB_DM roteado, com a largura e o afastamento que "
            "dao 90 ohm diferenciais nesta pilha",
     "USB 2.0, 7.1.6: 90 ohm +-15%; a geometria sai do empilhamento, "
     "calculada em route.py"),
    ("AL1", "desacoplamento do modulo de radio a 0,5 mm do pino de "
            "alimentacao; dos demais CIs, 2 mm para o de alta frequencia e "
            "5 mm para o de reserva",
     "MinewSemi ME54BS13 V1.0.0, 7.2; fichas do nPM1300, AEM10900, TPS7A02"),
    ("AL4", "no maximo 0,2 ohm em serie na linha VCC do GNSS",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 4.1.1"),
    ("AL5", "footprint de filtro pi reservado junto ao pino de alimentacao "
            "do modulo de radio, por ele vir de fonte chaveada",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("AL2", "largura de trilha suficiente para a corrente, 10 C de subida",
     "IPC-2221B, 6.2, curva de condutor externo"),
    ("AL3", "laco de chaveamento curto: SW ao indutor e ao capacitor de saida",
     "ficha do nPM1300, layout recomendado"),
    ("GN1", "uma via de terra junto de cada pad de terra do modulo de radio, "
            "e todo pad de terra de superficie ligado ao terra",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("GN2", "costura de vias de terra na borda a cada 5 mm no maximo",
     "lambda/10 a 2,44 GHz em FR-4 e 6,1 mm"),
    ("AL6", "nenhuma via sob o pad termico: ela suga a solda da junta no "
            "forno, e sob o DQN do TPS7A02 a ficha proibe por escrito",
     "TI TPS7A02 SBVS277C, 8.4.1; TI OPT3001 SBOS681B, layout (via no pad "
     "termico so com diametro abaixo de 0,2 mm)"),
    ("OP1", "nenhum componente a menos de duas vezes a propria altura do "
            "sensor de luz: reflexao optica secundaria",
     "TI OPT3001 SBOS681B, layout guidelines"),
    ("ME3", "o corpo que a ficha de cada peca cota cabe no footprint que "
            "foi desenhado para ela, e bate com o contorno dele",
     "desenho mecanico de cada ficha, em footprints.PACOTE"),
    ("ME1", "a placa cabe na caixa com folga",
     "hardware_gnssbike/04-pcb-e-caixa.md"),
    ("ME2", "altura dos componentes dentro da sombra da bateria e do display",
     "hardware_gnssbike/04-pcb-e-caixa.md#as-duas-sombras"),
]

# What each rail carries, and where the number comes from. Without this a
# trace width is a guess.
CORRENTE = {
    "VBUS": (0.5, "USB-C 2.0 sem PD: 500 mA (04-pcb-e-caixa.md)"),
    "VBUSOUT": (0.5, "saida do limitador do nPM1300"),
    "VBAT": (0.8, "carga de 800 mA da celula (15-avaliacao-componentes.md)"),
    "VBAT_CELULA": (0.8, "idem, do conector da celula"),
    "VBAT_SYS": (0.8, "idem, para o sistema"),
    "VSYS": (0.5, "consumo de pico do aparelho"),
    "3V0": (0.4, "trilho de 3,0 V: display, sensores, flash"),
    "1V8": (0.2, "trilho de 1,8 V"),
    "SD3V0": (0.2, "trilho comutado do display"),
    "BUCK1_SW": (0.5, "no de chaveamento do buck 1"),
    "BUCK2_SW": (0.5, "no de chaveamento do buck 2"),
    "SW_DCDC": (0.3, "no de chaveamento do AEM10900"),
    "SW_DCDC_L": (0.3, "idem, lado do indutor"),
}

# The height of the packages that come out of KiCad's library, which the
# board file does not carry. These are the usual maximum for the case size,
# not a reading of the part's own drawing, and they say so: a number from a
# drawing lives in footprints.PACOTE and wins over anything here.
ALTURA_PADRAO = (
    ("_0402_", 0.55), ("_0603_", 0.95), ("_0805_", 1.00), ("_1008_", 1.20),
    ("_1206_", 0.75), ("SOIC-8", 2.00), ("LGA-14", 0.80),
    ("B3S-1000", 3.40), ("USB_C_Receptacle", 3.26), ("JST_GH", 4.25),
    ("FH12-10S", 0.90), ("1734839", 1.20), ("Buzzer", 1.70),
    ("TestPoint", 0.00), ("MountingHole", 0.00), ("Tag-Connect", 0.00),
    ("ContatoMola", 0.00),
)


def altura_do_footprint(nome: str):
    """(height, where the number comes from) or None when nobody knows."""
    import footprints as _F

    m = _F.altura_de(nome)
    if m:
        return m
    if nome in _F.CORPO:
        return (_F.CORPO[nome][2], "medida do encapsulamento em footprints.CORPO")
    for chave, h in ALTURA_PADRAO:
        if chave in nome:
            return (h, f"altura corrente de um {chave.strip('_')}, NAO lida da "
                       "ficha da peca")
    return None


CHAVEADOS = ["L101", "L102", "L103", "U101", "U103"]

# ME54BS13 V1.0.0, 7.2, "Interference Isolation Rule": the table of minimum
# recommended isolation from the module, by type of source. The datasheet
# names its own way out in the same paragraph - "Isolation using different
# PCB layers and shielding covers is recommended" - so a row this board
# cannot meet is reported with what it measures, not hidden.
#
# Rows with nothing on this board to match are left out of the table on
# purpose rather than carried empty: there is no USB 3.0, HDMI, DDR or
# high-speed SDIO here (the USB is 2.0 full speed), and the only
# high-frequency MCU clock is inside the module itself.
ISOLACAO = (
    ("fonte chaveada, indutor de potencia ou transformador", 20.0, CHAVEADOS),
    ("display, camera ou cabo FPC com fiacao", 25.0, ["J401", "J402"]),
)
# and the display itself, which is not a part on the board but a rectangle
# over it - the 25 mm row applies to it more than to its connector
ZONA_DISPLAY = "SOMBRA_DISPLAY_JDI_MAX_2-6MM"
PI_GNSS = ["L301", "C301", "C302"]
MODULO = "U201"          # the radio module
GNSS = "U301"            # the GNSS receiver
# The antenna band of the ME54BS13, from its mechanical drawing: 4.46 mm of
# the module's 16.5 mm length, across the whole 12 mm width. It is NOT the
# keep-out rectangle, which is drawn larger and reaches the board edge.
ANT_MOD = 4.46
FOLGA_ANTENA = 5.0       # 7.4: "3-5 mm around the antenna area"
FOLGA_RF_GNSS = 5.0      # MAX-F10S IM 4.4: "at least a 5 mm distance"
LIMITE_MODULO = 0.5      # 7.2: "trace length ... should be <= 0.5 mm"

# How far a decoupling capacitor may sit from the pin it serves. One number
# for both kinds is wrong in both directions: twelve 10 uF bulk capacitors
# cannot all fit inside 2 mm of a 4 x 4 mm QFN, and a 100 nF at 4 mm is no
# longer decoupling anything above a few tens of megahertz - the loop
# inductance of the track swamps it. So the limit follows the value.
LIMITE_HF, LIMITE_RESERVA = 2.0, 5.0
CORTE_HF = 1.1e-6            # at or below this the capacitor is the fast one


def farads(valor: str) -> float:
    """The value on the bill of materials, as a number."""
    v = valor.lower().replace(",", ".").strip()
    for letra, m in (("p", 1e-12), ("n", 1e-9), ("u", 1e-6),
                     ("µ", 1e-6), ("m", 1e-3)):
        if letra + "f" in v:
            try:
                return float(v.split()[0]) * m
            except (ValueError, IndexError):
                return 1.0
    return 1.0


def limite(ref: str) -> float:
    if ref not in P.PARTS:
        return LIMITE_HF
    return LIMITE_HF if farads(P.PARTS[ref].value) <= CORTE_HF \
        else LIMITE_RESERVA


def ler(caminho: pathlib.Path):
    arv = fp_load.parse(caminho.read_text(encoding="utf-8"))
    pecas = {}
    pads = []
    for f in fp_load.kids(arv, "footprint"):
        at = fp_load.kid(f, "at")
        fx, fy = float(at[1]) - MP.ORIGEM[0], float(at[2]) - MP.ORIGEM[1]
        ang = float(at[3]) if len(at) > 3 else 0.0
        ref = ""
        for pr in fp_load.kids(f, "property"):
            if pr[1] == "Reference":
                ref = pr[2]
        atras = (fp_load.kid(f, "layer") or ["", "F.Cu"])[1].startswith("B.")
        xs, ys = [], []
        for chave in ("fp_line", "fp_rect", "fp_poly", "fp_circle"):
            for g in fp_load.kids(f, chave):
                lay = fp_load.kid(g, "layer")
                if not lay or "CrtYd" not in lay[1]:
                    continue
                for tag in ("start", "end", "center"):
                    q = fp_load.kid(g, tag)
                    if q:
                        xs.append(float(q[1]))
                        ys.append(float(q[2]))
                pts = fp_load.kid(g, "pts")
                if pts:
                    for q in fp_load.kids(pts, "xy"):
                        xs.append(float(q[1]))
                        ys.append(float(q[2]))
        meus = []
        for p in fp_load.kids(f, "pad"):
            a = fp_load.kid(p, "at")
            px, py = float(a[1]), float(a[2])
            r = math.radians(ang)
            gx = fx + px * math.cos(r) + py * math.sin(r)
            gy = fy - px * math.sin(r) + py * math.cos(r)
            rede = fp_load.kid(p, "net")
            camadas = list(fp_load.kid(p, "layers")[1:])
            item = {"ref": ref, "pad": p[1], "x": gx, "y": gy,
                    "rede": rede[2] if rede else "",
                    "smd": p[2] == "smd",
                    "camada": "B.Cu" if any("B.Cu" in c for c in camadas)
                              else "F.Cu"}
            pads.append(item)
            meus.append(item)
        if xs:
            r = math.radians(ang)
            cx = [x * math.cos(r) + y * math.sin(r) for x, y in zip(xs, ys)]
            cy = [-x * math.sin(r) + y * math.cos(r) for x, y in zip(xs, ys)]
            caixa = (fx + min(cx), fy + min(cy), fx + max(cx), fy + max(cy))
        else:
            caixa = (fx, fy, fx, fy)
        pecas[ref] = {"x": fx, "y": fy, "ang": ang, "atras": atras,
                      "caixa": caixa, "pads": meus}
    seg = []
    for s in fp_load.kids(arv, "segment"):
        a, b = fp_load.kid(s, "start"), fp_load.kid(s, "end")
        seg.append({"a": (float(a[1]) - MP.ORIGEM[0], float(a[2]) - MP.ORIGEM[1]),
                    "b": (float(b[1]) - MP.ORIGEM[0], float(b[2]) - MP.ORIGEM[1]),
                    "w": float(fp_load.kid(s, "width")[1]),
                    "n": int(fp_load.kid(s, "net")[1]),
                    "c": fp_load.kid(s, "layer")[1]})
    # the cut-outs of the board outline, as boxes. A notch for the antenna
    # is what 7.4 asks for, and it lives on Edge.Cuts like the outline.
    ex, ey = [], []
    seg_edge = []
    for chave in ("gr_line", "gr_arc", "gr_rect"):
        for g in fp_load.kids(arv, chave):
            lay = fp_load.kid(g, "layer")
            if not lay or lay[1] != "Edge.Cuts":
                continue
            pts = []
            for tag in ("start", "end", "mid", "center"):
                q = fp_load.kid(g, tag)
                if q:
                    pts.append((float(q[1]) - MP.ORIGEM[0],
                                float(q[2]) - MP.ORIGEM[1]))
            if pts:
                seg_edge.append(pts)
                ex += [q[0] for q in pts]
                ey += [q[1] for q in pts]
    # A notch is a corner of Edge.Cuts that sits INSIDE the board instead of
    # on its rectangle. Collecting those points is enough to say where the
    # board has a bite taken out of it, and it works whether the cut was
    # drawn as lines or as a rectangle.
    cortes = []
    borda = 0.6
    dentro = [(x, y) for x, y in zip(ex, ey)
              if borda < x < M.W - borda and borda < y < M.H - borda]
    if dentro:
        cortes.append((min(q[0] for q in dentro), min(q[1] for q in dentro),
                       max(q[0] for q in dentro), max(q[1] for q in dentro)))
    del seg_edge

    vias = []
    for v in fp_load.kids(arv, "via"):
        a = fp_load.kid(v, "at")
        vias.append({"x": float(a[1]) - MP.ORIGEM[0],
                     "y": float(a[2]) - MP.ORIGEM[1],
                     "n": int(fp_load.kid(v, "net")[1])})
    return pecas, pads, seg, vias, cortes


def dist_caixas(a, b) -> float:
    """Gap between two boxes; negative when they overlap."""
    dx = max(a[0] - b[2], b[0] - a[2])
    dy = max(a[1] - b[3], b[1] - a[3])
    if dx < 0 and dy < 0:
        return max(dx, dy)
    return math.hypot(max(dx, 0), max(dy, 0))


def zona(nome: str):
    for n, r, _c, _s in M.ZONES:
        if n == nome:
            return r
    raise KeyError(nome)


# --- IPC-2221 ---------------------------------------------------------------
def largura_ipc(corrente: float, subida: float = 10.0,
                espessura_um: float = 35.0) -> float:
    """Minimum external-layer width, in mm, by IPC-2221B 6.2.

    A = (I / (k * dT^b))^(1/c) in mils squared, with k 0.048, b 0.44, c 0.725
    for an external conductor; the width is A divided by the thickness.
    """
    k, b, c = 0.048, 0.44, 0.725
    area_mils2 = (corrente / (k * subida ** b)) ** (1 / c)
    esp_mils = espessura_um / 25.4
    return area_mils2 / esp_mils * 0.0254


def main() -> int:
    if not PCB.exists():
        print("gnssbike.kicad_pcb nao existe: rode make_pcb.py")
        return 1
    pecas, pads, seg, vias, cortes = ler(PCB)
    achados: list[tuple[str, str]] = []
    ok: list[str] = []

    def falhou(rid: str, msg: str) -> None:
        achados.append((rid, msg))

    print(f"dry-run de {PCB.name}: {len(pecas)} pecas, {len(seg)} segmentos, "
          f"{len(vias)} vias\n")

    # -- a area real da antena do modulo, do desenho mecanico ----------------
    def rect_antena():
        """The 4.46 x 12 mm band at the RF end of U201, in board coordinates.

        The keep-out rectangle of make_dxf is a planning shape drawn out to
        the board edge; the datasheet's rules talk about the ANTENNA AREA,
        which is 4.46 mm of the module's 16.5 mm length. Measuring the rules
        against the planning rectangle answers a different question.
        """
        if MODULO not in pecas:
            return None
        m = pecas[MODULO]
        x0, y0, x1, y1 = m["caixa"]
        a = int(round(m["ang"])) % 360
        if a == 0:      # antenna at -Y in the footprint
            return (x0, y0, x1, y0 + ANT_MOD)
        if a == 180:
            return (x0, y1 - ANT_MOD, x1, y1)
        if a == 270:    # rotated so the antenna points +X
            return (x1 - ANT_MOD, y0, x1, y1)
        return (x0, y0, x0 + ANT_MOD, y1)

    ant = rect_antena()

    # -- RF1: nada sobre a area da antena ------------------------------------
    if ant:
        invadem = [(r, dist_caixas(p["caixa"], ant)) for r, p in pecas.items()
                   if r != MODULO and dist_caixas(p["caixa"], ant) < 0]
        if invadem:
            falhou("RF1", f"{len(invadem)} pecas sobre a area da antena: " +
                   ", ".join(f"{r}" for r, _d in sorted(invadem)[:6]))
        else:
            ok.append(f"RF1: a area da antena ({ant[2]-ant[0]:.2f} x "
                      f"{ant[3]-ant[1]:.2f} mm) esta livre de componente")

    # -- RF3: 5 mm em volta da area da antena --------------------------------
    if ant:
        # The module's own decoupling is not a foreign interference source,
        # and it CANNOT obey both rules of this datasheet at once: 7.2 wants
        # it 0.5 mm from the power pin and that pin is 2.1 mm from the
        # antenna band. The exemption is named, printed and limited to those
        # two capacitors.
        perto = sorted((dist_caixas(p["caixa"], ant), r)
                       for r, p in pecas.items()
                       if r != MODULO and r not in MP.DO_MODULO)
        dentro = [(d, r) for d, r in perto if d < FOLGA_ANTENA]
        proprias = sorted((dist_caixas(pecas[r]["caixa"], ant), r)
                          for r in MP.DO_MODULO if r in pecas)
        if dentro:
            falhou("RF3", f"{len(dentro)} pecas alheias a menos de "
                   f"{FOLGA_ANTENA:.0f} mm da area da antena: " +
                   ", ".join(f"{r} {d:.1f}" for d, r in dentro[:8]))
        else:
            ok.append(f"RF3: a peca alheia mais proxima da area da antena e "
                      f"{perto[0][1]} a {perto[0][0]:.1f} mm; o desacoplamento "
                      "do proprio modulo fica mais perto de proposito (" +
                      ", ".join(f"{r} {d:.1f}" for d, r in proprias) + ")")

    # -- RF4: a placa vazada sob a antena ------------------------------------
    if ant:
        vazado = any(cx1 > ant[0] and ant[2] > cx0 and
                     cy1 > ant[1] and ant[3] > cy0
                     for (cx0, cy0, cx1, cy1) in cortes)
        if not vazado:
            falhou("RF4", "a placa nao e vazada sob a area da antena; a ficha "
                   "pede a regiao suspensa")
        else:
            ok.append("RF4: a placa e vazada sob a area da antena")

    # -- RF5: 5 mm do receptor GNSS a componente de RF -----------------------
    if GNSS in pecas:
        rf = [r for r in ("E301", "L301", "C301", "C302", MODULO) if r in pecas]
        dd = sorted((dist_caixas(pecas[GNSS]["caixa"], pecas[r]["caixa"]), r)
                    for r in rf)
        # the pi network is the receiver's OWN RF front end: it has to be
        # close, and 4.4 is about foreign RF parts
        alheios = [(d, r) for d, r in dd if r not in PI_GNSS]
        if alheios and alheios[0][0] < FOLGA_RF_GNSS:
            falhou("RF5", f"{alheios[0][1]} esta a {alheios[0][0]:.1f} mm do "
                   f"receptor GNSS, contra {FOLGA_RF_GNSS:.0f} mm")
        elif alheios:
            ok.append(f"RF5: o componente de RF alheio mais proximo do receptor "
                      f"e {alheios[0][1]} a {alheios[0][0]:.1f} mm")

    # -- RF7: a rede pi do GNSS ----------------------------------------------
    if GNSS in pecas and all(r in pecas for r in PI_GNSS):
        rf_in = [q for q in pecas[GNSS]["pads"] if q["rede"] == "RF_IN"]
        if rf_in:
            # What the datasheet asks for is a SHORT RF PATH, and this used
            # to ask something else: that all three elements of the pi be
            # within 5 mm of the RF_IN pin. C301 is the shunt at the ANTENNA
            # end - it is the far end of the network by construction - so a
            # correctly built pi failed the rule for being correctly built.
            #
            # The path is the one the netlist gives: RF_IN pin -> L301 (the
            # series element) -> the antenna contact's FEED pad, with each
            # shunt measured against the node it hangs on. The limit is
            # lambda/10 at L1, which is where a line stops being electrically
            # short: with eps_eff 3.27 for a 0.196 mm microstrip over the
            # 0.10 mm prepreg of this stack-up, lambda at 1.575 GHz is
            # 105.3 mm, so lambda/10 is 10.5 mm.
            LIMITE_RF = 10.5
            p0 = (rf_in[0]["x"], rf_in[0]["y"])

            def perto(ref, ponto):
                return min(math.hypot(a["x"] - ponto[0], a["y"] - ponto[1])
                           for a in pecas[ref]["pads"])

            alim = [q for q in pecas.get("J302", {}).get("pads", [])
                    if q["rede"] == "RF_ANT"]
            trechos = [("RF_IN ate L301", perto("L301", p0))]
            if alim:
                pa = (alim[0]["x"], alim[0]["y"])
                trechos.append(("L301 ate a antena", perto("L301", pa)))
            caminho = sum(d for _n, d in trechos)
            shunts = [(r, perto(r, p0 if r == "C302" else
                                (pa if alim else p0)))
                      for r in ("C302", "C301") if r in pecas]
            mal = [t for t in trechos if t[1] > LIMITE_RF]
            # The shunt's own stub, against lambda/20 = 5.3 mm. A number and
            # not a feeling: a stub much shorter than an eighth of a
            # wavelength behaves as the lumped capacitor the network was
            # designed with, and lambda/20 is half of that with margin. The
            # 2.0 mm this carried for one run was invented - the datasheet
            # says "as short as possible" and gives no figure.
            LIMITE_STUB = LIMITE_RF / 2.0
            if caminho > LIMITE_RF or mal or                     any(d > LIMITE_STUB for _r, d in shunts):
                falhou("RF7", f"caminho de RF de {caminho:.1f} mm contra "
                       f"{LIMITE_RF:.1f} (lambda/10 em L1): " +
                       ", ".join(f"{n} {d:.1f}" for n, d in trechos) +
                       "; shunts " +
                       ", ".join(f"{r} {d:.1f} de {LIMITE_STUB:.1f}"
                                 for r, d in shunts))
            else:
                ok.append(f"RF7: caminho de RF do pino a antena {caminho:.1f} "
                          f"mm, dentro de lambda/10 em L1 ({LIMITE_RF:.1f}); " +
                          ", ".join(f"{r} a {d:.1f} mm do seu no"
                                    for r, d in shunts))

    # -- RF8: separacao das duas antenas -------------------------------------
    ag = zona("KEEPOUT_ANTENA_GNSS")
    gx, gy = (ag[0] + ag[2]) / 2, (ag[1] + ag[3]) / 2
    if ant:
        ax, ay = (ant[0] + ant[2]) / 2, (ant[1] + ant[3]) / 2
        sep = math.hypot(ax - gx, ay - gy)
        ok.append(f"RF8: as duas antenas estao a {sep:.1f} mm de centro a centro "
                  f"({sep / 30.7:.1f} quartos de onda de 2,44 GHz)")

    # -- RF9: a tabela de isolacao da 7.2 ------------------------------------
    if MODULO in pecas:
        cm = pecas[MODULO]["caixa"]
        perto_demais, medidos = [], []
        for tipo, limite_mm, refs in ISOLACAO:
            pior = None
            for r in refs:
                if r not in pecas:
                    continue
                d = dist_caixas(cm, pecas[r]["caixa"])
                if pior is None or d < pior[0]:
                    pior = (d, r)
            if tipo.startswith("display"):
                try:
                    z = zona(ZONA_DISPLAY)
                    d = dist_caixas(cm, z)
                    if pior is None or d < pior[0]:
                        pior = (d, "a propria sombra do display")
                except KeyError:
                    pass
            if pior is None:
                continue
            medidos.append((tipo, limite_mm, pior))
            if pior[0] < limite_mm:
                perto_demais.append((tipo, limite_mm, pior))
        if perto_demais:
            falhou("RF9", "; ".join(
                f"{t}: {p[1]} a {p[0]:.1f} mm, a ficha pede {lim:.0f}"
                for t, lim, p in perto_demais))
        elif medidos:
            ok.append("RF9: " + "; ".join(
                f"{t}: {p[1]} a {p[0]:.1f} de {lim:.0f} mm"
                for t, lim, p in medidos))

    # -- RF10: 50 mm entre os dois modulos -----------------------------------
    if MODULO in pecas and GNSS in pecas:
        d = dist_caixas(pecas[MODULO]["caixa"], pecas[GNSS]["caixa"])
        if d < 50.0:
            falhou("RF10", f"os dois modulos de radio estao a {d:.1f} mm, "
                   "contra os 50 mm que a ficha pede")
        else:
            ok.append(f"RF10: os dois modulos de radio estao a {d:.1f} mm, "
                      "acima dos 50 mm da ficha")

    # -- US1: o par diferencial do USB ---------------------------------------
    # Not "is it routed" but "is it the pair the spec asks for". The width
    # that gives 90 ohm differential on this stack-up is computed in route.py
    # and printed here beside what is actually drawn, because a pair routed
    # at the wrong width is not a pair - it is two tracks - and nothing else
    # in the chain would ever say so.
    import route as _R
    _numeros, _ = MP.redes()
    _por_num = {n: r for r, n in _numeros.items()}
    par = {}
    for r in ("USB_DP", "USB_DM"):
        sg = [q for q in seg if _por_num.get(q["n"], "") == r]
        comp = sum(math.hypot(q["b"][0] - q["a"][0], q["b"][1] - q["a"][1])
                   for q in sg)
        larg = sorted({round(q["w"], 3) for q in sg})
        par[r] = (len(sg), comp, larg)
    alvo = _R.LARGURA_USB_CALC
    faltando = [r for r, v in par.items() if v[0] == 0]
    if faltando:
        falhou("US1", "o par nao esta roteado: " + ", ".join(faltando))
    else:
        larguras = sorted({w for v in par.values() for w in v[2]})
        maior = max(larguras)
        if maior < alvo - 1e-6:
            falhou("US1", "o par esta roteado a " +
                   ", ".join(f"{w:.3f}" for w in larguras) +
                   f" mm, e 90 ohm diferenciais nesta pilha pedem "
                   f"{alvo:.3f} mm com {_R.PASSO_PAR:.1f} de afastamento. "
                   f"Comprimentos: " +
                   ", ".join(f"{r} {v[1]:.1f} mm" for r, v in par.items()) +
                   ". Tem de ser terminado a mao")
        else:
            ok.append("US1: o par esta roteado a " +
                      ", ".join(f"{w:.3f}" for w in larguras) +
                      f" mm, contra os {alvo:.3f} que dao 90 ohm")

    # -- AL5: o filtro pi no pino de alimentacao do modulo -------------------
    # 7.2, Power Supply Design: "For switching power supply applications, a
    # pi-type filter circuit footprint must be reserved near the module power
    # input pins." The module here is fed from BUCK2 of the nPM1300, which is
    # switching, so the rule applies. What is measured is the SHAPE of a pi:
    # a series element between the rail and the pin, with a capacitor to
    # ground on each side of it. Whether the series part is a 0 ohm jumper or
    # a ferrite is a stuffing decision - the footprint is what the datasheet
    # asks to be reserved - but a rail with no series element at all has no
    # pi footprint to stuff.
    alim_mod = [q["rede"] for q in pecas.get(MODULO, {}).get("pads", [])
                if q.get("pad") == "VDD" or q["rede"] in ("3V0_MOD",)]
    rede_mod = alim_mod[0] if alim_mod else None
    if rede_mod and rede_mod in N.NETS:
        # the series element: a two terminal part on this net whose other end
        # is on a different net
        serie = []
        for ref, _p in N.NETS[rede_mod]:
            if ref == MODULO or ref not in P.PARTS:
                continue
            if len(P.PARTS[ref].pins) != 2:
                continue
            outras = {n for n, v in N.NETS.items()
                      if n != rede_mod and any(t[0] == ref for t in v)}
            if outras - {"GND"}:
                serie.append((ref, sorted(outras - {"GND"})[0]))
        caps_mod = [r for r, _p in N.NETS[rede_mod]
                    if r.startswith("C") and r in P.PARTS]
        montante = serie[0][1] if serie else None
        caps_up = [r for r, _p in N.NETS.get(montante, [])
                   if r.startswith("C") and r in P.PARTS] if montante else []
        if not serie:
            falhou("AL5", f"o trilho {rede_mod} chega ao pino do modulo sem "
                   "elemento em serie: nao ha footprint de pi para povoar")
        elif not caps_mod or not caps_up:
            falhou("AL5", f"{serie[0][0]} esta em serie entre {montante} e "
                   f"{rede_mod}, mas faltam capacitores de um dos lados "
                   f"(montante {len(caps_up)}, lado do modulo {len(caps_mod)})")
        else:
            pino = [q for q in pecas[MODULO]["pads"] if q["rede"] == rede_mod]
            d = "?"
            if pino and serie[0][0] in pecas:
                p0 = (pino[0]["x"], pino[0]["y"])
                d = "%.1f" % min(math.hypot(a["x"] - p0[0], a["y"] - p0[1])
                                 for a in pecas[serie[0][0]]["pads"])
            ok.append(f"AL5: filtro pi do modulo montado como "
                      f"{'+'.join(caps_up)} | {serie[0][0]} | "
                      f"{'+'.join(caps_mod)}, com o elemento em serie a "
                      f"{d} mm do pino de alimentacao")

    # -- AL1: desacoplamento --------------------------------------------------
    piores = []
    for cap, chip in MP.DECOPLA.items():
        # MP.DECOPLA also carries pull-ups and series resistors, which go
        # beside their chip for a different reason and are not held to the
        # decoupling distance: a pull-up 5 mm from the master is fine, a
        # decoupling capacitor 5 mm from the pin is not.
        if not cap.startswith("C"):
            continue
        if cap not in pecas or chip not in pecas:
            continue
        # and only a capacitor that decouples a SUPPLY PIN. C601 to C603 sit
        # across a key for debounce: they are in the table so the placer knows
        # where to put them, and holding them to a supply pin's distance is
        # measuring the wrong thing.
        if chip not in P.PARTS or len(P.PARTS[chip].pins) < 4:
            continue
        # and only a capacitor that decouples an IC. The rule reads "do
        # modulo de radio a 0,5 mm do pino; dos DEMAIS CIs, 2 mm" - a
        # connector is not an IC, and the capacitor that sits on the rail
        # leaving through one is bulk for the cable, not decoupling for a
        # supply pin. C404 on J401, the display's flat cable, was being held
        # to a chip's 2 mm at 3,1.
        if chip.startswith("J"):
            continue
        redes = {q["rede"] for q in pecas[cap]["pads"]} - {"GND", ""}
        alvo = [q for q in pecas[chip]["pads"] if q["rede"] in redes]
        if not alvo:
            continue
        d = min(min(math.hypot(a["x"] - b["x"], a["y"] - b["y"])
                    for b in alvo) for a in pecas[cap]["pads"])
        piores.append((d, cap, chip))
    piores.sort(reverse=True)
    acima = [t for t in piores if t[0] > limite(t[1])]
    media = sum(t[0] for t in piores) / len(piores) if piores else 0.0
    hf = [t for t in piores if limite(t[1]) == 2.0]
    if acima:
        falhou("AL1", f"{len(acima)} de {len(piores)} capacitores alem do limite "
               f"(media geral {media:.2f} mm): " +
               ", ".join(f"{c}->{u} {d:.1f} de {limite(c):.0f}"
                         for d, c, u in acima[:6]))
    else:
        ok.append(f"AL1: {len(piores)} capacitores dentro do limite; o pior de "
                  f"alta frequencia esta a {max(t[0] for t in hf):.2f} mm e o "
                  f"pior de reserva a {piores[0][0]:.2f} mm")

    # -- AL2: largura por IPC-2221 -------------------------------------------
    numeros, _ = MP.redes()
    por_num = {n: r for r, n in numeros.items()}
    estreitas = []
    necks = 0
    for s in seg:
        rede = por_num.get(s["n"], "")
        if rede not in CORRENTE:
            continue
        i, _fonte = CORRENTE[rede]
        pedida = largura_ipc(i)
        if s["w"] >= pedida - 1e-6:
            continue
        # A short stretch next to a pad is a neck-down, not an undersized
        # conductor: the copper on either side carries the heat away, and
        # IPC-2221's curve is for a conductor long enough to reach a steady
        # temperature. The rail that feeds the TPS7A02 has to neck to 0.2 mm
        # because the part's pad is 0.2 mm wide, and no width of track fixes
        # a pad.
        # A neck-down is not an undersized conductor: it is the track
        # matching a pad that is narrower than the rule wants, and no width
        # of copper fixes a pad. Recognised by what it actually is - one end
        # sitting on a pad of this net whose narrow side is no wider than the
        # track - instead of by a length that someone picked.
        colado = any(
            q["rede"] == rede and
            math.hypot(q["x"] - e[0], q["y"] - e[1]) < 0.6
            for e in (s["a"], s["b"]) for q in pads)
        comp = math.hypot(s["b"][0] - s["a"][0], s["b"][1] - s["a"][1])
        if colado or comp < 2.0:
            necks += 1
            continue
        estreitas.append((rede, s["w"], pedida))
    if estreitas:
        piores_l = {}
        for rede, w, pedida in estreitas:
            piores_l[rede] = (w, pedida)
        falhou("AL2", f"{len(estreitas)} segmentos abaixo da largura IPC-2221: " +
               ", ".join(f"{r} {w:.2f} < {p:.2f} mm"
                         for r, (w, p) in sorted(piores_l.items())[:6]))
    elif seg:
        ok.append("AL2: nenhum trecho de alimentacao abaixo da largura IPC-2221 "
                  f"(10 C de subida, cobre de 35 um); {necks} estreitamentos "
                  "curtos junto a pad, que a norma nao cobra")
    else:
        falhou("AL2", "sem trilhas: nada a medir")

    # -- GN1: pad de terra com via -------------------------------------------
    gnd_num = numeros.get("GND")
    gnd_smd = [q for q in pads if q["rede"] == "GND" and q["smd"]]
    vias_gnd = [v for v in vias if v["n"] == gnd_num]
    sem = 0
    for q in gnd_smd:
        if not any(math.hypot(v["x"] - q["x"], v["y"] - q["y"]) < 2.0
                   for v in vias_gnd):
            sem += 1
    # F.Cu and B.Cu both carry a ground pour, so a pad without a via of its
    # own is still connected - KiCad ties it to the pour when it fills. What
    # the via buys is the short path down to In1.Cu, and that is what gets
    # counted: a pad without one has a longer return, not an open circuit.
    com = len(gnd_smd) - sem
    frac = com / len(gnd_smd) if gnd_smd else 0.0
    # The bar is "connected", not a fraction someone made up. Every outer
    # layer carries a ground pour, so a pad without a via of its own is still
    # tied to ground when KiCad fills. The via buys the SHORT path down to
    # the inner plane, and how many have one is a quality number worth
    # printing, not a rule to pass or fail: inventing a threshold and then
    # tuning it until the board clears it measures nothing.
    faces = {"F.Cu", "B.Cu"}      # the two that get a pour, from make_pcb.py
    soltos = [q for q in gnd_smd if q["camada"] not in faces
              and not any(math.hypot(v["x"] - q["x"], v["y"] - q["y"]) < 2.0
                          for v in vias_gnd)]
    if soltos:
        falhou("GN1", f"{len(soltos)} pads de terra sem plano na propria face "
               "e sem via: ficam soltos")
    else:
        ok.append(f"GN1: os {len(gnd_smd)} pads de terra de superficie estao "
                  f"ligados; {com} deles ({frac:.0%}) por via propria ao plano "
                  f"interno, os outros {sem} pelo plano da propria face")

    # -- GN2: costura na borda ------------------------------------------------
    if vias_gnd:
        borda = [v for v in vias_gnd
                 if min(v["x"], v["y"], M.W - v["x"], M.H - v["y"]) < 3.0]
        if len(borda) < 2:
            falhou("GN2", f"so {len(borda)} vias de terra a menos de 3 mm da "
                   "borda: nao ha costura")
        else:
            maior = 0.0
            for v in borda:
                d = min(math.hypot(v["x"] - w["x"], v["y"] - w["y"])
                        for w in borda if w is not v)
                maior = max(maior, d)
            if maior > 5.0:
                falhou("GN2", f"a maior falha na costura da borda e {maior:.1f} mm "
                       "(o limite e 5)")
            else:
                ok.append(f"GN2: {len(borda)} vias na borda, maior vao {maior:.1f} mm")
    else:
        falhou("GN2", "sem vias de terra: nao ha costura")

    # -- ME1: a placa na caixa ------------------------------------------------
    # 04-pcb-e-caixa.md: a caixa e 62 x 104 mm POR FORA, com paredes de cerca
    # de 2 mm, o que deixa cerca de 58 x 100 por dentro. O numero de fora e
    # o que aparece na tabela do documento e e o errado a usar aqui: a placa
    # entra na cavidade, nao no contorno externo.
    folga = (58.0 - M.W) / 2, (100.0 - M.H) / 2
    if min(folga) < 0.5:
        falhou("ME1", f"folga de {folga[0]:.1f} x {folga[1]:.1f} mm na caixa")
    else:
        ok.append(f"ME1: a placa de {M.W} x {M.H} deixa {folga[0]:.1f} mm de cada "
                  f"lado e {folga[1]:.1f} mm em cima e embaixo")

    # -- AL6: via sob pad termico ---------------------------------------
    # Which pad of which part is a thermal pad. It is the one the datasheet
    # numbers last and connects to ground, and the two that matter here say
    # so by name.
    TERMICOS = {("U104", "5"), ("U103", "29"), ("U101", "33"),
                ("U505", "7"), ("U302", "15")}
    sobre_termico = []
    for q in pads:
        if (q["ref"], q["pad"]) not in TERMICOS:
            continue
        for v in vias:
            if abs(v["x"] - q["x"]) < 1.2 and abs(v["y"] - q["y"]) < 1.2:
                sobre_termico.append((q["ref"], q["pad"],
                                      round(math.hypot(v["x"] - q["x"],
                                                       v["y"] - q["y"]), 2)))
                break
    if sobre_termico:
        falhou("AL6", f"{len(sobre_termico)} vias sobre um pad termico: " +
               ", ".join(f"{r}.{p} a {d} mm" for r, p, d in sobre_termico[:6]))
    else:
        ok.append(f"AL6: nenhuma via sobre os {len(TERMICOS)} pads termicos")

    # -- OP1: reflexao optica no sensor de luz ---------------------------
    if "U505" in pecas:
        alt_de = {}
        import footprints as _F
        for r, (nome_fp, _o, _n) in _F.FP.items():
            if nome_fp in _F.CORPO_TODOS:
                alt_de[r] = _F.CORPO_TODOS[nome_fp][2]
            elif nome_fp in _F.ALTURA:
                alt_de[r] = _F.ALTURA[nome_fp][0]
        perto_luz = []
        for r, p in pecas.items():
            if r == "U505" or r not in alt_de:
                continue
            d = dist_caixas(p["caixa"], pecas["U505"]["caixa"])
            if d < 2 * alt_de[r]:
                perto_luz.append((r, round(d, 2), alt_de[r]))
        if perto_luz:
            falhou("OP1", f"{len(perto_luz)} pecas a menos de duas alturas do "
                   "sensor de luz: " +
                   ", ".join(f"{r} a {d} mm, alta {h}" for r, d, h in
                             sorted(perto_luz, key=lambda t: t[1])[:5]))
        else:
            ok.append("OP1: nenhuma peca de altura conhecida a menos de duas "
                      "alturas do sensor de luz")

    # -- ME3: o 2D e o 3D contam a mesma historia ------------------------
    import footprints as _FP
    for _n in _FP.PACOTE:
        try:
            fp_load.carregar(_n)
        except FileNotFoundError:
            pass
    div = _FP.conferir_2d_3d()
    if div:
        falhou("ME3", f"{len(div)} encapsulamentos em que a ficha e o "
               "footprint discordam: " + "; ".join(div[:3]))
    else:
        ok.append(f"ME3: os {len(_FP.PACOTE)} encapsulamentos com cota de "
                  "ficha cabem no footprint desenhado para eles e batem com "
                  "o contorno")

    # -- ME2: cabe sob o display e sob a bateria? ------------------------
    # 04-pcb-e-caixa.md gives two ceilings: 2.6 mm on the front, under the
    # display, and 1.2 mm on the back, under the battery. A part taller than
    # the shadow it stands in does not fit, and no DRC will ever say so -
    # which is the whole reason the heights had to come from the datasheets.
    TETOS = (("SOMBRA_DISPLAY_JDI_MAX_2-6MM", 2.6, False),
             ("SOMBRA_BATERIA_MAX_1-2MM", 1.2, True))
    altos = []
    sem_altura = []
    for ref, pe in sorted(pecas.items()):
        nome_fp = FPS.FP[ref][0] if ref in FPS.FP else None
        if nome_fp is None:
            continue
        medida = altura_do_footprint(nome_fp)
        if medida is None:
            sem_altura.append(ref)
            continue
        h, _fonte = medida
        for znome, teto, atras in TETOS:
            try:
                z = zona(znome)
            except KeyError:
                continue
            if pe["atras"] != atras:
                continue
            if dist_caixas(pe["caixa"], z) >= 0:
                continue                      # not under this shadow
            if h > teto + 1e-9:
                altos.append((ref, h, teto, znome))
    if altos:
        falhou("ME2", f"{len(altos)} pecas mais altas que o teto da sombra em "
               "que estao: " +
               ", ".join(f"{r} {h:.2f} contra {t:.1f} mm"
                         for r, h, t, _z in altos[:6]))
    else:
        ok.append(f"ME2: nenhuma peca passa do teto da sombra em que esta "
                  f"(2,6 mm sob o display, 1,2 mm sob a bateria); "
                  f"{len(sem_altura)} pecas sem altura conhecida")

    # -- resultado -------------------------------------------------------------
    for linha in ok:
        print(f"  ok      {linha}")
    print()
    for rid, msg in achados:
        fonte = next((f for i, _d, f in REGRAS if i == rid), "")
        print(f"  FALHA   {rid}: {msg}")
        print(f"          fonte: {fonte}")
    print()
    medidas = {r for r, _ in achados} | {linha.split(":")[0] for linha in ok}
    naomedidas = [(i, d, f) for i, d, f in REGRAS if i not in medidas]
    if naomedidas:
        print("  nao medido aqui (precisa de bancada, do empilhamento do "
              "fabricante ou de modelo 3D):")
        for i, d, _f in naomedidas:
            print(f"    {i}: {d}")
    print()
    print(f"{len(ok)} regras medidas e cumpridas, {len(achados)} violadas, "
          f"{len(naomedidas)} nao medidas")
    return 1 if achados else 0


if __name__ == "__main__":
    sys.exit(main())
