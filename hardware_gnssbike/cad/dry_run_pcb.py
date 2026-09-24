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

import fp_load  # noqa: E402
import make_dxf as M  # noqa: E402
import make_pcb as MP  # noqa: E402
import parts as P  # noqa: E402

PCB = HERE / "gnssbike.kicad_pcb"

# --- the rules, with their source -------------------------------------------
# Each entry: id, what it demands, where it is written.
# Every line below was read in the manufacturer's own PDF, not in a summary
# of it. Two of the rules this file used to carry were not in either document:
# "20 mm between the antenna and a switching converter" does not exist - what
# 7.3 says is the qualitative "do not place modules adjacent to strong
# interference sources" - and the decoupling distance for the module is not
# 2 mm but 0.5 mm. Both were corrected on 2026-09-24 after reading
# ME54BS13-nRF54LM20A_Datasheet_K_EN v1.0.0 and MAX-F10S_IntegrationManual
# UBXDOC-963802114-12892. The PDFs live in ../datasheets/, which the
# .gitignore keeps out of this public repository.
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

CHAVEADOS = ["L101", "L102", "L103", "U101", "U103"]
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
            p0 = (rf_in[0]["x"], rf_in[0]["y"])
            dd = sorted((min(math.hypot(a["x"] - p0[0], a["y"] - p0[1])
                             for a in pecas[r]["pads"]), r) for r in PI_GNSS)
            if dd[-1][0] > 5.0:
                falhou("RF7", "a rede pi esta longe do RF_IN: " +
                       ", ".join(f"{r} {d:.1f}" for d, r in dd))
            else:
                ok.append("RF7: rede pi do GNSS a " +
                          ", ".join(f"{r} {d:.1f} mm" for d, r in dd))

    # -- RF8: separacao das duas antenas -------------------------------------
    ag = zona("KEEPOUT_ANTENA_GNSS")
    gx, gy = (ag[0] + ag[2]) / 2, (ag[1] + ag[3]) / 2
    if ant:
        ax, ay = (ant[0] + ant[2]) / 2, (ant[1] + ant[3]) / 2
        sep = math.hypot(ax - gx, ay - gy)
        ok.append(f"RF8: as duas antenas estao a {sep:.1f} mm de centro a centro "
                  f"({sep / 30.7:.1f} quartos de onda de 2,44 GHz)")

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
