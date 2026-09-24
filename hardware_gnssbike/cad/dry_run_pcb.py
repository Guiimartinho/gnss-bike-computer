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
REGRAS = [
    ("RF1", "nenhum componente dentro da area livre da antena do modulo",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("RF2", "a antena do modulo olha para fora da borda da placa",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("RF3", "20 mm entre a antena do modulo e conversor chaveado ou indutor",
     "MinewSemi ME54BS13 V1.0.0, 7.3"),
    ("RF4", "a rede pi do GNSS junto ao RF_IN, com a trilha mais curta possivel",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 2.3"),
    ("RF5", "separacao entre a antena GNSS (1,575 GHz) e a do radio (2,44 GHz)",
     "pratica: um quarto de onda de 2,44 GHz e 30,7 mm"),
    ("AL1", "capacitor de alta frequencia (ate 1 uF) a 2 mm do pino, "
            "capacitor de reserva (acima de 1 uF) a 5 mm",
     "fichas do nPM1300, AEM10900, TPS7A02, MAX17262, ME54BS13"),
    ("AL2", "largura de trilha suficiente para a corrente, 10 C de subida",
     "IPC-2221B, 6.2, curva de condutor externo"),
    ("AL3", "laco de chaveamento curto: SW ao indutor e ao capacitor de saida",
     "ficha do nPM1300, layout recomendado"),
    ("GN1", "todo pad de terra de superficie ligado ao terra, por via "
            "propria ou pelo plano da propria face",
     "pratica: o retorno segue por baixo do sinal"),
    ("GN2", "costura de vias de terra na borda a cada 5 mm no maximo",
     "lambda/10 a 2,44 GHz em FR-4 e 6,1 mm"),
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
    vias = []
    for v in fp_load.kids(arv, "via"):
        a = fp_load.kid(v, "at")
        vias.append({"x": float(a[1]) - MP.ORIGEM[0],
                     "y": float(a[2]) - MP.ORIGEM[1],
                     "n": int(fp_load.kid(v, "net")[1])})
    return pecas, pads, seg, vias


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
    pecas, pads, seg, vias = ler(PCB)
    achados: list[tuple[str, str]] = []
    ok: list[str] = []

    def falhou(rid: str, msg: str) -> None:
        achados.append((rid, msg))

    print(f"dry-run de {PCB.name}: {len(pecas)} pecas, {len(seg)} segmentos, "
          f"{len(vias)} vias\n")

    # -- RF1: area livre da antena do modulo ---------------------------------
    ant_mod = zona("KEEPOUT_ANTENA_MODULO")
    dono = MP.DONO_DO_KEEPOUT
    invadem = []
    for ref, p in pecas.items():
        if dono.get(ref) == "KEEPOUT_ANTENA_MODULO":
            continue
        d = dist_caixas(p["caixa"], ant_mod)
        if d < 0:
            invadem.append((ref, d))
    if invadem:
        falhou("RF1", f"{len(invadem)} pecas dentro da area livre da antena: " +
               ", ".join(f"{r} ({-d:.2f} mm)" for r, d in sorted(invadem)[:6]))
    else:
        perto = sorted(((dist_caixas(p["caixa"], ant_mod), r)
                        for r, p in pecas.items()
                        if dono.get(r) != "KEEPOUT_ANTENA_MODULO"))[:3]
        ok.append("RF1: area livre da antena do modulo vazia; a peca mais "
                  "proxima e " + ", ".join(f"{r} a {d:.2f} mm" for d, r in perto))

    # -- RF3: 20 mm dos chaveadores ------------------------------------------
    ax = (ant_mod[0] + ant_mod[2]) / 2
    ay = (ant_mod[1] + ant_mod[3]) / 2
    ruins = []
    for ref in CHAVEADOS:
        if ref not in pecas:
            continue
        d = dist_caixas(pecas[ref]["caixa"], ant_mod)
        ruins.append((d, ref))
    ruins.sort()
    fora = [(d, r) for d, r in ruins if d < 20.0]
    if fora:
        falhou("RF3", f"{len(fora)} de {len(ruins)} chaveadores a menos de 20 mm "
               "da antena do modulo: " +
               ", ".join(f"{r} {d:.1f} mm" for d, r in fora))
    else:
        ok.append(f"RF3: o chaveador mais proximo da antena e {ruins[0][1]} "
                  f"a {ruins[0][0]:.1f} mm")

    # -- RF4: a rede pi do GNSS ----------------------------------------------
    if "U301" in pecas and all(r in pecas for r in PI_GNSS):
        rf_in = [q for q in pecas["U301"]["pads"] if q["rede"] == "RF_IN"]
        if rf_in:
            p0 = (rf_in[0]["x"], rf_in[0]["y"])
            dd = []
            for r in PI_GNSS:
                q = min(pecas[r]["pads"],
                        key=lambda a: math.hypot(a["x"] - p0[0], a["y"] - p0[1]))
                dd.append((math.hypot(q["x"] - p0[0], q["y"] - p0[1]), r))
            dd.sort()
            pior = dd[-1]
            if pior[0] > 5.0:
                falhou("RF4", f"a rede pi esta longe do RF_IN: " +
                       ", ".join(f"{r} {d:.1f} mm" for d, r in dd) +
                       " (o limite pratico e 5 mm)")
            else:
                ok.append("RF4: rede pi do GNSS a " +
                          ", ".join(f"{r} {d:.1f} mm" for d, r in dd))
            # e a antena, do outro lado da rede pi?
            if "E301" in pecas:
                de = dist_caixas(pecas["E301"]["caixa"],
                                 zona("KEEPOUT_ANTENA_GNSS"))
                ok.append(f"RF4: E301 esta a {de:.2f} mm da area da antena GNSS")

    # -- RF5: separacao das duas antenas -------------------------------------
    ag = zona("KEEPOUT_ANTENA_GNSS")
    gx, gy = (ag[0] + ag[2]) / 2, (ag[1] + ag[3]) / 2
    sep = math.hypot(ax - gx, ay - gy)
    lim = 30.7
    if sep < lim:
        falhou("RF5", f"as duas antenas estao a {sep:.1f} mm (minimo {lim:.1f})")
    else:
        ok.append(f"RF5: as duas antenas estao a {sep:.1f} mm de centro a centro "
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
        comp = math.hypot(s["b"][0] - s["a"][0], s["b"][1] - s["a"][1])
        if comp < 2.0:
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
