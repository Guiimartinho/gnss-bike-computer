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

_PLACA_PATH = pathlib.Path(__file__).resolve().parent / "gnssbike.kicad_pcb"


def redes_do_arquivo() -> dict[str, int]:
    """Net name -> number, from the board file itself (fp_load explains why)."""
    return fp_load.redes_da_placa(
        fp_load.parse(_PLACA_PATH.read_text(encoding="utf-8")))

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
            "5 mm para o de reserva; tudo de borda a borda das ilhas, que e o "
            "cobre que o laco atravessa",
     "MinewSemi ME54BS13 V1.0.0, 7.2 (\"trace length ... <= 0.5 mm\"); os 2 e "
     "5 mm sao do projeto (as fichas do nPM1300, AEM10900, TPS7A02 e MAX-F10S "
     "dizem \"o mais perto possivel\")"),
    ("AL4", "no maximo 0,2 ohm em serie na linha VCC do GNSS",
     "u-blox MAX-F10S Integration Manual UBXDOC-963802114-12892, 4.1.1"),
    ("AL5", "footprint de filtro pi reservado junto ao pino de alimentacao "
            "do modulo de radio, por ele vir de fonte chaveada",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("AL2", "largura de trilha suficiente para a corrente, 10 C de subida",
     "IPC-2221B, 6.2, curva de condutor externo"),
    ("AL3", "laco de chaveamento curto: no no SW, no maximo 3 mm de cobre "
            "e nenhuma via, e o indutor a no maximo 2 mm do pino",
     "nPM1300 PS v1.1, 9.3.4, e ADP5091, layout (\"as close as possible\"); "
     "os numeros sao do projeto"),
    ("GN1", "uma via de terra junto de cada pad de terra do modulo de radio, "
            "e todo pad de terra de superficie ligado ao terra",
     "MinewSemi ME54BS13 V1.0.0, 7.2"),
    ("GN2", "costura de vias de terra na borda a cada 5 mm no maximo",
     "lambda/10 a 2,44 GHz em FR-4 e 6,1 mm"),
    ("AL6", "nenhuma via sob o pad termico de quem a ficha proibe: ela "
            "suga a solda da junta no forno, e sob o DQN do TPS7A02 a ficha "
            "proibe por escrito; o nPM1300 leva as 9 vias do layout de "
            "referencia no pad central",
     "TI TPS7A02 SBVS277C, 8.4.1; TI OPT3001 SBOS681B, layout (via no pad "
     "termico so com diametro abaixo de 0,2 mm); Nordic nPM1300 PS v1.1, "
     "figuras 61 a 64"),
    ("OP1", "nenhum componente a menos de duas vezes a propria altura do "
            "sensor de luz: reflexao optica secundaria",
     "TI OPT3001 SBOS681B, layout guidelines"),
    ("ME6", "o corpo 3D de cada peca tem o eixo do footprint (comprimento "
            "onde o F.Fab tem comprimento) e, onde as fileiras de pinos sao "
            "desiguais, as pernas saem pelos mesmos lados que as ilhas: uma "
            "peca desenhada 90 ou 180 graus fora das ilhas e o que o dono "
            "viu nos MOSFET",
     "o dono, no 3D, em 2026-09-26; o F.Fab e as ilhas do footprint"),
    ("ME5", "os rabichos de solda do modelo 3D de cada conector ficam sobre "
            "as ilhas de sinal do footprint, e nao do lado oposto: um corpo "
            "simetrico passa na ME4 girado 180 graus, e o dono viu o J103 "
            "com as pernas para a borda e as ilhas para dentro",
     "o dono, no 3D, em 2026-09-26; as ilhas do footprint do KiCad"),
    ("ME3", "o corpo que a ficha de cada peca cota cabe no footprint que "
            "foi desenhado para ela, e bate com o contorno dele",
     "desenho mecanico de cada ficha, em footprints.PACOTE"),
    ("ME1", "a placa cabe na caixa com folga",
     "hardware_gnssbike/04-pcb-e-caixa.md"),
    ("ME2", "altura dos componentes dentro da sombra da bateria e do display",
     "hardware_gnssbike/04-pcb-e-caixa.md#as-duas-sombras"),
    ("RT1", "toda ligacao da lista de nos existe em cobre: zero itens "
            "desconectados no DRC completo do KiCad",
     "kicad-cli pcb drc --severity-all. E a unica contagem que vale: o "
     "roteador daqui relata o que tentou, e um DRC so de erros esconde os "
     "desconectados, que sao aviso"),
    ("ME4", "as ilhas de solda de uma peca ficam sob o corpo do modelo 3D "
            "do fabricante",
     "um rabicho de solda fica em cima da sua ilha: se o corpo nao as cobre, "
     "o modelo esta fora de posicao ou fora de orientacao"),
    ("US2", "as duas linhas do par USB com o mesmo comprimento do diodo de "
            "protecao ao modulo: no maximo 50 mil (1,27 mm) de diferenca",
     "TI SPRAAR7J, tabela A-1 (\"skew within any USB2.0 differential pair: "
     "50 mils\")"),
    ("FA1", "via dentro de ilha de SMD so com enchimento e tampa (POFV) no "
            "pedido: sem eles a solda escorre pelo furo",
     "JLCPCB, via-in-pad e via covering (\"Epoxy Filled & Capped\"; cobrado "
     "em 4 camadas, sem custo de 6 em diante)"),
]

# As pecas cujo corpo vem de um modelo do fabricante: sao as unicas em que
# o modelo pode estar girado em relacao ao footprint, porque as outras tem o
# corpo desenhado aqui a partir da cota da ficha.
# J402 left on 2026-10-18: it is the Molex 503480-0540 now, a footprint drawn
# by footprints.py with a box for a body, not a maker's model
CORPOS_CONFERIDOS = ("J101", "J103")

# What each rail carries, and where the number comes from. Without this a
# trace width is a guess.
CORRENTE = {
    # 600 mA of charge plus 71 mA of system: the charger is linear, so what
    # comes in is the sum (02-calculos.md, the USB budget). The 1,5 A of a
    # USB-C source is what it offers, not what runs in the track; until
    # 2026-10-19 this was 0,5 A, the nPM1300's starting limit
    "VBUS": (0.671, "carga mais sistema, 02-calculos.md"),
    # VBUSOUT goes through a 7,5 ohm switch (RVBUSOUT, nPM1300 PS v1.1): at
    # 100 mA it already drops 0,75 V, and what it feeds is the module's VBUS
    # detection and a 100k + 180k divider. Until 2026-10-19 this was the
    # limiter's 0,5 A, a current the pin cannot pass
    "VBUSOUT": (0.1, "chave de 7,5 ohm do nPM1300 (RVBUSOUT)"),
    "VBAT": (0.8, "carga de 800 mA da celula (15-avaliacao-componentes.md)"),
    "VBAT_CELULA": (0.8, "idem, do conector da celula"),
    "VBAT_SYS": (0.8, "idem, para o sistema"),
    # The charge does not leave the nPM1300 through the VSYS pin: the
    # charger sits between the internal VSYS node and VBAT (PS v1.1,
    # CHARGER). What leaves is what the regulators on VSYS deliver, at most
    # their rated output - BUCK1 and BUCK2 200 mA each, LDSW2 100 mA and the
    # LEDs, about 0,47 A from a 3,0 V cell. Until 2026-10-19 02-calculos.md
    # gave VSYS 671 mA, the charge added in
    "VSYS": (0.5, "reguladores do VSYS na corrente nominal; a carga nao sai "
                  "pelo pino (nPM1300 PS v1.1)"),
    # 3V0 is BUCK2's output, and the nPM1300 has "two ... 200 mA BUCK
    # regulators" (PS v1.1, p. 2): the module, the display, the sensors, the
    # flash and the receiver share those 200 mA. Until 2026-10-19 this was
    # 0,4 A, more than the regulator gives. 1V8 (BUCK1) and SD3V0 (LDSW1)
    # carry nothing on this board - 1V8 reaches only its inductor and two
    # capacitors, SD3V0 only its test point (nets.py) - and are left out
    "3V0": (0.2, "trilho de 3,0 V do BUCK2, 200 mA (nPM1300 PS v1.1): modulo, "
                 "display, sensores, flash e receptor GNSS"),
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
# high-speed SDIO here (the USB is 2.0, 480 Mbit/s at most), and the only
# high-frequency MCU clock is inside the module itself.
ISOLACAO = (
    ("fonte chaveada, indutor de potencia ou transformador", 20.0, CHAVEADOS),
    ("display, camera ou cabo FPC com fiacao", 25.0, ["J401", "J402", "J403"]),
)
# Deviations the owner accepted on 2026-10-19, each with why and the bench
# check that closes it: a measured rule that hits one says so as a
# deviation, neither a pass nor a failure. The 7.2 table is of RECOMMENDED
# distances from STRONG sources, and names layers and shields as the other
# way; the display here is a memory LCD on a 2 MHz SPI, with a still image,
# above the board and not on it.
DESVIOS = {
    ("RF9", "a propria sombra do display"):
        "LCD de memoria em SPI de 2 MHz, imagem parada, fora da placa; "
        "conferir na bancada o RSSI e a taxa do BLE com a tela atualizando",
    ("RF9", "J403"):
        "a fita da luz do Azumo leva so o LED, em PWM de kHz; mesma "
        "conferencia de bancada",
    ("AL1", "C210"):
        "4,7 uF de reserva do modulo: o de alta frequencia, C201, cumpre os "
        "0,5 mm; a ficha pede a combinacao inteira",
}
# AL4: what sits in series on the GNSS supply besides copper, at its
# datasheet maximum: the ferrite (Murata BLM18KG221SN1D, 50 mOhm) and the
# current jumper of the block (0 ohm in 0402, 50 mOhm, Panasonic ERJ-2GE)
SERIE_VCC_GNSS = {"FB301": 0.050, "JP103": 0.050}
LIMITE_VCC_GNSS = 0.200           # MAX-F10S integration manual, 4.1.1
FONTE_3V0 = ("L102", "3V0")       # the BUCK2 inductor's output pad
RHO_COBRE = 1.72e-8 * 1e3         # ohm mm (copper at 20 C)
RESISTENCIA_VIA = 0.0006          # ohm, one plated barrel of 0.25 mm
# AL3: the converters' switching nodes, chip and inductor
CONVERSORES = (("BUCK1_SW", "U101", "L101"), ("BUCK2_SW", "U101", "L102"),
               ("SW_DCDC", "U103", "L103"), ("SW_5V0", "U402", "L401"))
LIMITE_SW_COBRE, LIMITE_SW_INDUTOR = 3.0, 2.0
# RF6: how much of the receiver's body ground must cover on F.Cu and In1.Cu
COBERTURA_TERRA_GNSS = 0.80
# US2: TI SPRAAR7J, table A-1
LIMITE_SKEW_USB = 1.27
# What goes in the fabrication order (2026-10-19): the stack-up the widths
# were computed on (impedancia.py), with impedance control, and the vias in
# SMD pads filled and capped. FA1 holds the order to this.
FABRICACAO = {"pilha": "JLCPCB JLC04081H-3313, 0,8 mm, 4 camadas",
              "impedancia": "controlada, +-10 %",
              "via_em_pad": "POFV (Epoxy Filled & Capped)"}
# and the display itself, which is not a part on the board but a rectangle
# over it - the 25 mm row applies to it more than to its connector
ZONA_DISPLAY = "SOMBRA_DISPLAY_JDI_MAX_3-0MM"
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
        # two outlines, both in the footprint's own frame: the courtyard
        # (what the placer keeps clear) and the fabrication body (where the
        # part itself is; the ME5 rule measures the model's solder tails
        # against it, because a tail is what sticks OUT of the body)
        xs, ys = [], []
        xs_f, ys_f = [], []
        for chave in ("fp_line", "fp_rect", "fp_poly", "fp_circle"):
            for g in fp_load.kids(f, chave):
                lay = fp_load.kid(g, "layer")
                if not lay:
                    continue
                if "CrtYd" in lay[1]:
                    lx, ly = xs, ys
                elif "Fab" in lay[1]:
                    lx, ly = xs_f, ys_f
                else:
                    continue
                for tag in ("start", "end", "center"):
                    q = fp_load.kid(g, tag)
                    if q:
                        lx.append(float(q[1]))
                        ly.append(float(q[2]))
                pts = fp_load.kid(g, "pts")
                if pts:
                    for q in fp_load.kids(pts, "xy"):
                        lx.append(float(q[1]))
                        ly.append(float(q[2]))
        meus = []
        for p in fp_load.kids(f, "pad"):
            a = fp_load.kid(p, "at")
            px, py = float(a[1]), float(a[2])
            r = math.radians(ang)
            gx = fx + px * math.cos(r) + py * math.sin(r)
            gy = fy - px * math.sin(r) + py * math.cos(r)
            rede = fp_load.kid(p, "net")
            camadas = list(fp_load.kid(p, "layers")[1:])
            sz = fp_load.kid(p, "size")
            item = {"ref": ref, "pad": p[1], "x": gx, "y": gy,
                    "ang": float(a[3]) if len(a) > 3 else 0.0,
                    "rede": rede[2] if rede else "",
                    "hw": float(sz[1]) / 2 if sz else 0.0,
                    "hh": float(sz[2]) / 2 if sz else 0.0,
                    "smd": p[2] == "smd",
                    "camada": "B.Cu" if any("B.Cu" in c for c in camadas)
                              else "F.Cu"}
            pads.append(item)
            meus.append(item)
        def _caixa(lx, ly):
            if not lx:
                return (fx, fy, fx, fy)
            r = math.radians(ang)
            cx = [x * math.cos(r) + y * math.sin(r) for x, y in zip(lx, ly)]
            cy = [-x * math.sin(r) + y * math.cos(r) for x, y in zip(lx, ly)]
            return (fx + min(cx), fy + min(cy), fx + max(cx), fy + max(cy))

        caixa = _caixa(xs, ys)
        pecas[ref] = {"x": fx, "y": fy, "ang": ang, "atras": atras,
                      "caixa": caixa, "fab": _caixa(xs_f, ys_f) if xs_f else None,
                      "pads": meus}
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


def caixa_pad(q) -> tuple:
    """A pad as a box on the board. The angle in the file is the pad's own
    on the board, footprint and pad together: 90 and 270 swap the sides."""
    hw, hh = (q["hh"], q["hw"]) if int(round(q.get("ang", 0.0))) % 180 == 90 \
        else (q["hw"], q["hh"])
    return (q["x"] - hw, q["y"] - hh, q["x"] + hw, q["y"] + hh)


def vao_pads(a, b) -> float:
    """Copper gap between two pads, edge to edge."""
    return max(dist_caixas(caixa_pad(a), caixa_pad(b)), 0.0)


def _grafo_rede(n: int, seg, vias, pads_rede) -> tuple[list, list]:
    """Nodes and edges (i, j, length mm, resistance ohm) of one net's copper:
    track ends per layer, vias joining the four layers, and the pads,
    each tied to every node inside it."""
    nos: list[tuple] = []

    def no(x, y, cam):
        for i, (a, b, c) in enumerate(nos):
            if c == cam and abs(a - x) < 0.02 and abs(b - y) < 0.02:
                return i
        nos.append((x, y, cam))
        return len(nos) - 1
    esp = {"F.Cu": MP.CU, "B.Cu": MP.CU, "In1.Cu": MP.CU_INTERNO, "In2.Cu": MP.CU_INTERNO}
    arestas = []
    meus = [q for q in seg if q["n"] == n]
    for q in meus:
        comp = math.hypot(q["b"][0] - q["a"][0], q["b"][1] - q["a"][1])
        r = RHO_COBRE * comp / (q["w"] * esp[q["c"]]) if comp > 0 else 1e-9
        arestas.append((no(*q["a"], q["c"]), no(*q["b"], q["c"]), comp, r))
    for q in meus:            # a track that ends on the middle of another
        for ponta in (q["a"], q["b"]):
            for t in meus:
                if t is not q and t["c"] == q["c"] and \
                        _dist_ponto_seg(ponta, t["a"], t["b"]) < min(q["w"], t["w"]) / 2:
                    arestas.append((no(*ponta, q["c"]), no(*t["a"], t["c"]),
                                    math.hypot(ponta[0] - t["a"][0], ponta[1] - t["a"][1]), 1e-6))
    for v in vias:
        if v["n"] != n:
            continue
        ids = [no(v["x"], v["y"], c) for c in esp]
        for k in range(3):
            arestas.append((ids[k], ids[k + 1], 0.0, RESISTENCIA_VIA))
        for q in meus:
            for ponta in (q["a"], q["b"]):
                if math.hypot(ponta[0] - v["x"], ponta[1] - v["y"]) < 0.25:
                    arestas.append((no(*ponta, q["c"]), ids[list(esp).index(q["c"])], 0.0, 1e-6))
    for q in pads_rede:
        k = no(q["x"], q["y"], q["camada"])
        cx = caixa_pad(q)
        for i, (x, y, c) in enumerate(list(nos)):
            if i != k and c == q["camada"] and cx[0] - 0.01 <= x <= cx[2] + 0.01 \
                    and cx[1] - 0.01 <= y <= cx[3] + 0.01:
                arestas.append((i, k, 0.0, 1e-6))
    return nos, arestas


def _dist_ponto_seg(p, a, b) -> float:
    vx, vy = b[0] - a[0], b[1] - a[1]
    nn = vx * vx + vy * vy
    u = 0.0 if nn == 0 else max(0.0, min(1.0, ((p[0] - a[0]) * vx + (p[1] - a[1]) * vy) / nn))
    return math.hypot(p[0] - a[0] - u * vx, p[1] - a[1] - u * vy)


def _no_do_pad(nos, q) -> int:
    return next(i for i, (x, y, c) in enumerate(nos)
                if c == q["camada"] and abs(x - q["x"]) < 0.02 and abs(y - q["y"]) < 0.02)


def comprimento(n: int, seg, vias, pa, pb, ilhas=()) -> float:
    """The routed length (mm) between two pads of one net: the shortest way
    through its copper (Dijkstra), via barrels counted as nothing."""
    import heapq
    nos, arestas = _grafo_rede(n, seg, vias, list(ilhas) + [pa, pb])
    viz: dict = {}
    for i, j, comp, _r in arestas:
        viz.setdefault(i, []).append((j, comp))
        viz.setdefault(j, []).append((i, comp))
    a, b = _no_do_pad(nos, pa), _no_do_pad(nos, pb)
    dist = {a: 0.0}
    fila = [(0.0, a)]
    while fila:
        d, i = heapq.heappop(fila)
        if i == b:
            return d
        if d > dist.get(i, float("inf")):
            continue
        for j, c in viz.get(i, ()):
            if d + c < dist.get(j, float("inf")):
                dist[j] = d + c
                heapq.heappush(fila, (d + c, j))
    return float("inf")


def resistencia(n: int, seg, vias, pa, pb, ilhas=()) -> float:
    """DC resistance (ohm) of one net's copper between two pads: the whole
    network of its tracks and vias, solved, not one path."""
    import numpy as np
    nos, arestas = _grafo_rede(n, seg, vias, list(ilhas) + [pa, pb])
    m = len(nos)
    L = np.zeros((m, m))
    for i, j, _c, r in arestas:
        if i != j:
            g = 1.0 / r
            L[i, i] += g
            L[j, j] += g
            L[i, j] -= g
            L[j, i] -= g
    Lp = np.linalg.pinv(L)
    a, b = _no_do_pad(nos, pa), _no_do_pad(nos, pb)
    return float(Lp[a, a] + Lp[b, b] - 2 * Lp[a, b])


def terra_preenchida(arv) -> dict:
    """The filled polygons of the ground pours, by layer, in board mm."""
    import numpy as np
    out: dict = {}
    for z in fp_load.kids(arv, "zone"):
        nn = fp_load.kid(z, "net_name")
        if not nn or nn[1] != "GND":
            continue
        for fp in fp_load.kids(z, "filled_polygon"):
            lay = fp_load.kid(fp, "layer")[1]
            xy = [(float(q[1]) - MP.ORIGEM[0], float(q[2]) - MP.ORIGEM[1])
                  for q in fp_load.kid(fp, "pts")[1:] if q[0] == "xy"]
            out.setdefault(lay, []).append(np.array(xy))
    return out


def dentro_poligono(x, y, xy) -> bool:
    import numpy as np
    px, py = xy[:, 0], xy[:, 1]
    qx, qy = np.roll(px, 1), np.roll(py, 1)
    corta = ((py > y) != (qy > y)) & (x < (qx - px) * (y - py) / (qy - py + 1e-30) + px)
    return int(np.count_nonzero(corta)) % 2 == 1


def zona(nome: str):
    for n, r, _c, _s in M.ZONES:
        if n == nome:
            return r
    raise KeyError(nome)


# --- IPC-2221 ---------------------------------------------------------------
def largura_ipc(corrente: float, subida: float = 10.0,
                espessura_um: float | None = None, interna: bool = False) -> float:
    """IPC-2221B, 6.2: the width a current needs for a given temperature rise.

    I = k * dT^0.44 * A^0.725, with A in square mils. k is 0.048 for an
    OUTER conductor and 0.024 for an INNER one: an inner track has no air to
    give its heat to, and the standard asks it to be about 2.6 times wider
    for the same rise. Until 2026-09-26 this applied the outer curve to every
    segment, and VBUS runs 37 mm on In2.Cu.

    The copper is the stack-up's (make_pcb): 35 um outside and JLCPCB's
    0.5 oz, 15.2 um, inside. Until 2026-10-19 every layer was taken as 35 um,
    which made an inner track look twice as able as it is.
    """
    if espessura_um is None:
        espessura_um = (MP.CU_INTERNO if interna else MP.CU) * 1000.0
    k = 0.024 if interna else 0.048
    area_mil2 = (corrente / (k * subida ** 0.44)) ** (1 / 0.725)
    esp_mil = espessura_um / 25.4
    return area_mil2 / esp_mil * 0.0254


def _ilhas_sob_o_corpo(ref: str, pecas: dict):
    """As ilhas de solda desta peca ficam sob o corpo do modelo 3D?

    Devolve `(n_fora, total, caixa_do_corpo)`, ou None se nao houver corpo.

    Esta pergunta e feita porque a outra - "de que lado fica a boca?" - nao
    tem resposta confiavel por geometria: tentei responde-la com um raio
    pela cavidade e o raio mentiu, porque a traseira de um receptaculo USB-C
    tambem e oca. Esta aqui nao depende de palpite nenhum. Um rabicho de
    solda fica em cima da sua ilha; se o corpo do modelo nao cobre as ilhas
    da peca, o modelo esta fora de posicao ou fora de orientacao, e ponto.

    Foi exatamente a assinatura do defeito do J101: com o STEP da LCSC como
    ele vem, as 16 ilhas de contato caiam 1,45 mm ALEM do corpo, do lado
    oposto, e a boca do conector apontava para o miolo da placa.
    """
    import numpy as np

    sys.path.insert(0, str(HERE))
    import make_3d as M3

    if ref not in pecas:
        return None
    pe = pecas[ref]
    ilhas = [(q["pad"], q["x"], q["y"]) for q in pe.get("pads", []) if q["smd"]]
    glb = M3._glb_atual()
    if glb is None:
        return None
    j, bina = M3.ler_glb(glb)
    tris, _cor = M3.triangulos(j, bina)
    X = tris[..., 0] * 1000.0 - MP.ORIGEM[0]
    Y = -tris[..., 1] * 1000.0 - MP.ORIGEM[1]
    H = tris[..., 2] * 1000.0
    x0, y0, x1, y1 = pe["caixa"]
    # so o que esta INTEIRO dentro da envoltoria da peca mais 1 mm: assim a
    # caixa medida e a do corpo dela, e nao a do vizinho - e na FACE dela:
    # ate 2026-09-26 a regra so olhava z > 0,95, e para o J103, que foi para
    # o verso nesse dia, mediu o sensor de luz e o diodo da frente por cima
    # dele e acusou uma ilha fora de um corpo que nao era o seu
    if pe.get("atras", False):
        na_face = H.max(axis=1) < -0.05
    else:
        na_face = H.min(axis=1) > 0.95
    c = ((X.min(axis=1) > x0 - 1.0) & (X.max(axis=1) < x1 + 1.0)
         & (Y.min(axis=1) > y0 - 1.0) & (Y.max(axis=1) < y1 + 1.0)
         & na_face)
    if not c.any():
        return None
    caixa = (float(X[c].min()), float(Y[c].min()),
             float(X[c].max()), float(Y[c].max()))
    fora = [nome for nome, px, py in ilhas
            if not (caixa[0] - 0.05 <= px <= caixa[2] + 0.05
                    and caixa[1] - 0.05 <= py <= caixa[3] + 0.05)]
    return (fora, len(ilhas), caixa)


# Os conectores cujos rabichos de solda a ME5 confere: os que tem modelo do
# fabricante ou da biblioteca do KiCad no GLB e ilhas de sinal numa fileira.
# J401 and J402 left on 2026-10-18, when the Molex 503480 was a box, and
# came back with J403 on 2026-10-21, with Molex's own STEPs
RABICHOS_CONFERIDOS = ("J101", "J102", "J103", "J401", "J402", "J403")

# Models whose solder contacts have a colour of their own in the maker's
# STEP, so that the contacts can be told from the nails' plate. Measured in
# the exported GLB on 2026-09-26: the HCTL FPC's five contacts are pure
# yellow (1, 1, 0), the housing white, the lid black, the nail plate grey.
# The general method of ME5 and ME6 - material touching the board outside
# the F.Fab, weighed by area - took the nail plate, which lay on the pads'
# side in the WRONG orientation, for the tails, and it outweighs five 0,3 mm
# contacts: both rules passed the model turned 180 degrees, and the owner
# saw it in the 3D. With the colour the question is direct: the coloured
# faces that touch the board have to lie on the pads' side of the body.
CONTATOS_POR_COR = {"HC-FPC-05-10-5RLTAG": (1.0, 1.0, 0.0)}


def _modelo_base(ref: str):
    """The base name of the maker's model this part carries, or None."""
    nome_fp = FPS.FP[ref][0] if ref in FPS.FP else None
    if not nome_fp or ":" not in nome_fp:
        return None
    real = FPS.modelo_de_verdade(nome_fp.split(":", 1)[1])
    if not real:
        return None
    return real.rsplit("/", 1)[-1].rsplit(".", 1)[0]


def _contatos_por_cor(ref: str, pe: dict, corpos, ux: float, uy: float, fc):
    """The signed position, along the pad axis from the F.Fab centre, of the
    faces with the contacts' colour that touch the board (positive is the
    pads' side), with their count; None when the model is not in
    CONTATOS_POR_COR or has no such face near the board."""
    import numpy as np
    base = _modelo_base(ref)
    if base not in CONTATOS_POR_COR:
        return None
    alvo = CONTATOS_POR_COR[base]
    x0, y0, x1, y1 = pe["caixa"]
    atras = pe.get("atras", False)
    soma = peso = 0.0
    n = 0
    for tris, cor in corpos:
        c = tuple(round(float(v), 2) for v in np.array(cor).reshape(-1)[:3])
        if c != alvo:
            continue
        X = tris[..., 0] * 1000.0 - MP.ORIGEM[0]
        Y = -tris[..., 1] * 1000.0 - MP.ORIGEM[1]
        H = tris[..., 2] * 1000.0
        if X.max() < x0 or X.min() > x1 or Y.max() < y0 or Y.min() > y1:
            continue
        cx = X.mean(axis=1)
        cy = Y.mean(axis=1)
        dentro = (cx > x0) & (cx < x1) & (cy > y0) & (cy < y1)
        if atras:
            toca = (H.min(axis=1) < -0.08) & (H.max(axis=1) < 0.001) & (H.max(axis=1) > -1.2)
        else:
            toca = (H.max(axis=1) > 0.9) & (H.min(axis=1) > 0.80) & (H.min(axis=1) < 2.0)
        sel = dentro & toca
        if not sel.any():
            continue
        a = tris[sel]
        area = 0.5 * np.linalg.norm(np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]), axis=1)
        proj = (cx[sel] - fc[0]) * ux + (cy[sel] - fc[1]) * uy
        soma += float((proj * area).sum())
        peso += float(area.sum())
        n += int(sel.sum())
    if peso <= 0.0:
        return None
    return (soma / peso, n)
_GLB_CACHE: dict = {}


def _glb_lido():
    """The GLB parsed once per run: five connectors, one file."""
    sys.path.insert(0, str(HERE))
    import make_3d as M3
    glb = M3._glb_atual()
    if glb is None:
        return None
    if _GLB_CACHE.get("caminho") != glb:
        j, bina = M3.ler_glb(glb)
        _GLB_CACHE.update(caminho=glb, corpos=M3.corpos(j, bina))
    return _GLB_CACHE["corpos"]


def _rabichos_sobre_as_ilhas(ref: str, pecas: dict):
    """Os rabichos de solda do modelo 3D ficam sobre as ilhas de sinal?

    A ME4 pergunta se as ilhas ficam sob o corpo. Um corpo simetrico cobre
    as ilhas nos dois sentidos, e foi assim que o JST ZH do J103 passou na
    ME4 girado 180 graus: o dono viu no 3D as pernas apontando para a borda
    enquanto as ilhas ficavam do lado de dentro. Um rabicho de solda e um
    corpo pequeno e fino que encosta na placa, e o centro dos rabichos tem
    de cair sobre o centro das ilhas de sinal - nao do lado oposto.

    Devolve (distancia, n_rabichos, centro_rabichos, centro_ilhas), ou
    None quando nao ha corpo ou rabicho para medir.
    """
    if ref not in pecas:
        return None
    pe = pecas[ref]

    def mecanica(nome: str) -> bool:
        # "MP1"/"MP2" (the HCTL FPC's nails) were counted as signal pads
        # until 2026-09-26, and pulled the pads' centre towards the nails
        return nome in ("", "MP") or nome.startswith("S") or nome.startswith("MP")

    ilhas = [(q["x"], q["y"]) for q in pe.get("pads", [])
             if q["smd"] and not mecanica(q["pad"])]
    fixacao = [(q["x"], q["y"]) for q in pe.get("pads", []) if mecanica(q["pad"])]
    if not ilhas:
        return None
    corpos = _glb_lido()
    if corpos is None:
        return None
    x0, y0, x1, y1 = pe["caixa"]
    fab = pe.get("fab")
    if fab is None:
        return None
    atras = pe.get("atras", False)
    # The GLB that KiCad exports is one primitive per FACE, not per solid,
    # so "a small thin body" is not something it can tell apart: a housing
    # is thousands of small faces too. What a tail IS, geometrically, is
    # material that touches the board OUTSIDE the fabrication outline of
    # the footprint - the outline is where the housing stands, and the
    # tails stick out of it over the pads. So: every face near the board
    # whose centre is outside the F.Fab box and inside the courtyard,
    # weighted by its area.
    import numpy as np
    ci = (sum(p[0] for p in ilhas) / len(ilhas), sum(p[1] for p in ilhas) / len(ilhas))
    # The axis that matters: from the centre of the body to the centre of
    # the signal pads. The tails stick out of the body ALONG it, on the pad
    # side; a housing whose ends poke 0,2 mm past the F.Fab box pokes out
    # ACROSS it, and must not count - it did, and pulled the centre of the
    # "tails" into the middle of the body.
    fc = ((fab[0] + fab[2]) / 2.0, (fab[1] + fab[3]) / 2.0)
    ux, uy = ci[0] - fc[0], ci[1] - fc[1]
    norma = math.hypot(ux, uy)
    if norma < 0.3:
        return None
    ux, uy = ux / norma, uy / norma
    meio = abs(ux) * (fab[2] - fab[0]) / 2.0 + abs(uy) * (fab[3] - fab[1]) / 2.0
    # a model whose contacts have their own colour is measured by them,
    # not by whatever metal touches the board (CONTATOS_POR_COR says why)
    por_cor = _contatos_por_cor(ref, pe, corpos, ux, uy, fc)
    if por_cor is not None:
        return (por_cor[0], por_cor[1], ci, meio)
    soma = 0.0
    peso = 0.0
    n_faces = 0
    for tris, _cor in corpos:
        X = tris[..., 0] * 1000.0 - MP.ORIGEM[0]
        Y = -tris[..., 1] * 1000.0 - MP.ORIGEM[1]
        H = tris[..., 2] * 1000.0
        # a quick reject on the primitive, then face by face: inside the
        # COURTYARD (no margin - the neighbours' courtyards do not overlap
        # ours, so their faces stay out), near the board, and past the body
        # along the pad axis
        if X.max() < x0 or X.min() > x1 or Y.max() < y0 or Y.min() > y1:
            continue
        cx = X.mean(axis=1)
        cy = Y.mean(axis=1)
        dentro = (cx > x0) & (cx < x1) & (cy > y0) & (cy < y1)
        # ON the board, but NOT the board: the GLB carries the substrate
        # (z 0 to 0,82) and the copper (0,82 to 0,85 on top, -0,04 to 0
        # underneath), and a pad is a flat gold face lying exactly where a
        # tail lies. Measured in the file on 2026-09-26: the tails of the
        # JST ZH start at z 0,91, the copper ends at 0,85. So a tail is a
        # face that starts above the top copper and within a millimetre of
        # it; on the back face, one that ends below the bottom copper.
        if atras:
            toca = (H.min(axis=1) < -0.08) & (H.max(axis=1) < 0.001) & (H.max(axis=1) > -1.2)
        else:
            toca = (H.max(axis=1) > 0.9) & (H.min(axis=1) > 0.80) & (H.min(axis=1) < 2.0)
        proj = (cx - fc[0]) * ux + (cy - fc[1]) * uy
        fora = np.abs(proj) > meio + 0.15
        sel = dentro & toca & fora
        if fixacao:
            perto = np.zeros(len(cx), dtype=bool)
            for fx_, fy_ in fixacao:
                perto |= np.hypot(cx - fx_, cy - fy_) < 1.2
            sel &= ~perto
        if not sel.any():
            continue
        a = tris[sel]
        area = 0.5 * np.linalg.norm(np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]), axis=1)
        soma += float((proj[sel] * area).sum())
        peso += float(area.sum())
        n_faces += int(sel.sum())
    if peso <= 0.0:
        return None
    # the signed position of the tails along the pad axis, from the body
    # centre: positive is the pad side, negative the opposite one
    media = soma / peso
    return (media, n_faces, ci, meio)


def _corpos_mm():
    """Every body on the board, in board millimetres: (triangles, source).

    Two sources. The GLB that kicad-cli exports carries the STEP models -
    the library's and the three from the LCSC - together with the board's
    own copper. The parts drawn from a .wrl (the ones footprints.py made
    from the datasheet cotes, and the library packages that have no STEP,
    like the SOT-523) are not in it: they are placed by make_3d.py the
    same way it draws them, so a rule sees exactly what the picture shows.
    """
    import numpy as np
    if "mm" in _GLB_CACHE:
        return _GLB_CACHE["mm"]
    saida = []
    corpos = _glb_lido()
    if corpos:
        for tris, _cor in corpos:
            T = np.stack([tris[..., 0] * 1000.0 - MP.ORIGEM[0],
                          -tris[..., 1] * 1000.0 - MP.ORIGEM[1],
                          tris[..., 2] * 1000.0], axis=-1)
            saida.append((T, "glb"))
    sys.path.insert(0, str(HERE))
    import make_3d as M3
    t, _c = M3.caixas_das_pecas()
    if len(t):
        T = np.stack([t[..., 0] - MP.ORIGEM[0], t[..., 1] - MP.ORIGEM[1],
                      t[..., 2]], axis=-1)
        saida.append((T, "wrl"))
    _GLB_CACHE["mm"] = saida
    return saida


def _faces_da_peca(pe: dict, corpos=None):
    """The model faces of one part: inside its courtyard and off the copper.

    Returns (cx, cy, area, hmin, hmax) as arrays, or None. Same frame as
    the rest of this file: board millimetres, y downward, z up with the
    top copper ending at 0,855 and the bottom copper starting at -0,035.
    """
    import numpy as np
    x0, y0, x1, y1 = pe["caixa"]
    atras = pe.get("atras", False)
    cxs, cys, ars, h0s, h1s = [], [], [], [], []
    for T, fonte in _corpos_mm():
        X, Y, H = T[..., 0], T[..., 1], T[..., 2]
        if X.max() < x0 or X.min() > x1 or Y.max() < y0 or Y.min() > y1:
            continue
        cx = X.mean(axis=1)
        cy = Y.mean(axis=1)
        dentro = (cx > x0) & (cx < x1) & (cy > y0) & (cy < y1)
        # the GLB carries the copper: a face lying flat on it (0,82 to
        # 0,855 on top, -0,035 to 0 underneath) is the board, not the part.
        # A leg standing on the copper starts AT 0,855 and rises above 0,9.
        if fonte == "glb":
            if atras:
                fora_do_cobre = (H.min(axis=1) < -0.08) & (H.max(axis=1) < 0.001)
            else:
                fora_do_cobre = (H.max(axis=1) > 0.9) & (H.min(axis=1) > 0.80)
        else:
            # the bodies make_3d draws from a .wrl stand on FRENTE_Z (0,855)
            # or hang from VERSO_Z (-0,035). A back part under a front one
            # shares its courtyard in plan - the buzzer under the SWD header
            # - and until 2026-09-26 its faces were counted as the front
            # part's: ME6 called J202 "girado 90" with the buzzer's box.
            if atras:
                fora_do_cobre = H.max(axis=1) < 0.001
            else:
                fora_do_cobre = H.min(axis=1) > 0.80
        sel = dentro & fora_do_cobre
        if not sel.any():
            continue
        a = T[sel]
        area = 0.5 * np.linalg.norm(np.cross(a[:, 1] - a[:, 0], a[:, 2] - a[:, 0]), axis=1)
        cxs.append(cx[sel]); cys.append(cy[sel]); ars.append(area)
        h0s.append(H.min(axis=1)[sel]); h1s.append(H.max(axis=1)[sel])
    if not cxs:
        return None
    return (np.concatenate(cxs), np.concatenate(cys), np.concatenate(ars),
            np.concatenate(h0s), np.concatenate(h1s))


def _corpo_bate_com_o_footprint(ref: str, pecas: dict):
    """Does the 3D body have the footprint's shape and orientation?

    Two measurements, for every part that has a fabrication outline and a
    model in the GLB:

      1. the body's box against the F.Fab box. A part turned 90 degrees
         has its length where the footprint has its width, and a 0402 is
         twice as long as it is wide: the swap is unmistakable. This is
         the owner's question about the resistors, answered by measure.
      2. for a part whose pads sit in two facing rows of DIFFERENT counts
         (a SOT-23 has two on one side and one on the other), the legs of
         the model must stick out of the body on the same sides, in the
         same proportion. A part turned 180 degrees fails this and passes
         the first.

    Returns a dict with the verdicts and the numbers, or None.
    """
    import numpy as np
    if ref not in pecas:
        return None
    pe = pecas[ref]
    fab = pe.get("fab")
    if fab is None:
        return None
    corpos = _glb_lido()
    if corpos is None:
        return None
    faces = _faces_da_peca(pe, corpos)
    if faces is None:
        return None
    cx, cy, area, h0, h1 = faces
    # 1. the box of the body: the faces of the model, area-weighted 2nd and
    #    98th percentiles so that a stray sliver does not stretch it
    def _faixa(v):
        ordem = np.argsort(v)
        acum = np.cumsum(area[ordem]) / area.sum()
        return (float(v[ordem][np.searchsorted(acum, 0.02)]),
                float(v[ordem][min(np.searchsorted(acum, 0.98), len(v) - 1)]))
    bx0, bx1 = _faixa(cx)
    by0, by1 = _faixa(cy)
    w_m, h_m = bx1 - bx0, by1 - by0
    w_f, h_f = fab[2] - fab[0], fab[3] - fab[1]
    out = {"modelo": (w_m, h_m), "fab": (w_f, h_f), "eixo": "ok"}
    if abs(w_f - h_f) > 0.4 and abs(w_m - h_m) > 0.4:
        if (w_m > h_m) != (w_f > h_f):
            out["eixo"] = "girado 90"
    # 2. legs per side, only where the pad rows are asymmetric
    ilhas = [(q["x"], q["y"]) for q in pe.get("pads", []) if q["smd"]]
    out["lados"] = "sem medida"
    # a model with coloured contacts: the contacts decide the side (the
    # area count below took the HCTL FPC's nail plate for its tails)
    sinal = [(q["x"], q["y"]) for q in pe.get("pads", [])
             if q["smd"] and not (q["pad"] in ("", "MP") or q["pad"].startswith(("S", "MP")))]
    if sinal and _modelo_base(ref) in CONTATOS_POR_COR:
        fc = ((fab[0] + fab[2]) / 2.0, (fab[1] + fab[3]) / 2.0)
        ci = (sum(p[0] for p in sinal) / len(sinal), sum(p[1] for p in sinal) / len(sinal))
        ux, uy = ci[0] - fc[0], ci[1] - fc[1]
        norma = math.hypot(ux, uy)
        if norma >= 0.3:
            por_cor = _contatos_por_cor(ref, pe, corpos, ux / norma, uy / norma, fc)
            if por_cor is not None:
                out["lados"] = "ok" if por_cor[0] > 0.0 else "girado 180"
                out["lados_n"] = ("contatos pela cor", round(por_cor[0], 3), por_cor[1])
                return out
    if len(ilhas) >= 3:
        xs = sorted(set(round(p[0], 2) for p in ilhas))
        ys = sorted(set(round(p[1], 2) for p in ilhas))
        for eixo, vals, coord in (("x", xs, 0), ("y", ys, 1)):
            if len(vals) != 2:
                continue
            lo, hi = vals
            n_lo = sum(1 for p in ilhas if round(p[coord], 2) == lo)
            n_hi = sum(1 for p in ilhas if round(p[coord], 2) == hi)
            if n_lo == n_hi:
                out["lados"] = "simetrico"
                continue
            c = cx if coord == 0 else cy
            f0, f1 = (fab[0], fab[2]) if coord == 0 else (fab[1], fab[3])
            a_lo = float(area[c < f0 - 0.05].sum())
            a_hi = float(area[c > f1 + 0.05].sum())
            if a_lo + a_hi < 1e-6:
                out["lados"] = "sem perna fora do corpo"
                continue
            out["lados"] = ("ok" if (a_lo > a_hi) == (n_lo > n_hi)
                            else "girado 180")
            out["lados_n"] = (n_lo, n_hi, round(a_lo, 3), round(a_hi, 3), eixo)
    return out


def main() -> int:
    if not PCB.exists():
        print("gnssbike.kicad_pcb nao existe: rode make_pcb.py")
        return 1
    pecas, pads, seg, vias, cortes = ler(PCB)
    achados: list[tuple[str, str]] = []
    ok: list[str] = []
    desvios: list[tuple[str, str, str]] = []

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

    # -- RF2: a ponta de RF do modulo virada para a borda --------------------
    if ant and MODULO in pecas:
        lados = {"esquerda": ant[0], "de cima": ant[1], "direita": M.W - ant[2],
                 "de baixo": M.H - ant[3]}
        lado, d = min(lados.items(), key=lambda kv: kv[1])
        cm = pecas[MODULO]["caixa"]
        centro = {"esquerda": (cm[0] + cm[2]) / 2, "de cima": (cm[1] + cm[3]) / 2,
                  "direita": M.W - (cm[0] + cm[2]) / 2, "de baixo": M.H - (cm[1] + cm[3]) / 2}
        if d > 0.5 or centro[lado] <= d:
            falhou("RF2", f"a ponta de RF do modulo fica a {d:.1f} mm da borda {lado}")
        else:
            ok.append(f"RF2: a ponta de RF do modulo encosta na borda {lado} "
                      f"({d:.1f} mm), com o corpo para dentro")

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

    # -- RF6: terra sob o receptor, sem sinal cruzando ----------------------
    if GNSS in pecas:
        arv_rf6 = fp_load.parse(PCB.read_text(encoding="utf-8"))
        nomes6 = {nn: r for r, nn in fp_load.redes_da_placa(arv_rf6).items()}
        corpo = pecas[GNSS]["fab"] or pecas[GNSS]["caixa"]
        # the pins run round the edge of the body: crossing below it means
        # through the middle, 1.2 mm in from every side
        meio = (corpo[0] + 1.2, corpo[1] + 1.2, corpo[2] - 1.2, corpo[3] - 1.2)
        cruzam = set()
        for q in seg:
            rede = nomes6.get(q["n"], "")
            if q["c"] not in ("F.Cu", "In1.Cu") or rede == "GND":
                continue
            for k in range(21):
                x = q["a"][0] + (q["b"][0] - q["a"][0]) * k / 20
                y = q["a"][1] + (q["b"][1] - q["a"][1]) * k / 20
                if meio[0] <= x <= meio[2] and meio[1] <= y <= meio[3]:
                    cruzam.add(f"{rede} ({q['c']})")
                    break
        terra = terra_preenchida(arv_rf6)
        cobre = {}
        for cam in ("F.Cu", "In1.Cu"):
            tot = cob = 0
            y = corpo[1] + 0.1
            while y < corpo[3]:
                x = corpo[0] + 0.1
                while x < corpo[2]:
                    if not any(caixa_pad(q)[0] <= x <= caixa_pad(q)[2] and
                               caixa_pad(q)[1] <= y <= caixa_pad(q)[3] for q in pecas[GNSS]["pads"]):
                        tot += 1
                        cob += any(dentro_poligono(x, y, xy) for xy in terra.get(cam, []))
                    x += 0.25
                y += 0.25
            cobre[cam] = cob / max(tot, 1)
        if cruzam or min(cobre.values()) < COBERTURA_TERRA_GNSS:
            falhou("RF6", ("sinal cruzando sob o receptor: " + ", ".join(sorted(cruzam)) + "; " if cruzam else "")
                   + "terra cobre " + ", ".join(f"{c} {v:.0%}" for c, v in cobre.items())
                   + f" do corpo fora das ilhas (pede {COBERTURA_TERRA_GNSS:.0%})")
        else:
            ok.append("RF6: nenhuma trilha de sinal sob o receptor na F.Cu e na In1.Cu; o terra cobre "
                      + " e ".join(f"{v:.0%} na {c}" for c, v in cobre.items())
                      + " do corpo fora das ilhas")

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
            # short: with eps_eff 3.04 for the 0.15 mm line over the 0.0994 mm
            # prepreg of JLC04081H-3313, under the mask and beside the pour
            # (impedancia.py), lambda at 1.575 GHz is 109.2 mm, so lambda/10
            # is 10.9 mm. Until 2026-10-19: 3.27, 105.3 and 10.5, from the
            # closed form for the old 0.196 mm line.
            LIMITE_RF = 10.9
            p0 = (rf_in[0]["x"], rf_in[0]["y"])

            def perto(ref, ponto):
                return min(math.hypot(a["x"] - ponto[0], a["y"] - ponto[1])
                           for a in pecas[ref]["pads"])

            # the antenna end of the line is now behind the choice jumper:
            # the path is RF_IN -> L301 -> JP301, and from there to whichever
            # antenna is fitted. JP301 is what the path has to be measured to.
            alim = [q for q in pecas.get("JP301", {}).get("pads", [])
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
            # The shunt's own stub, against lambda/20 = 5.5 mm. A number and
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
            itens = [(dist_caixas(cm, pecas[r]["caixa"]), r) for r in refs if r in pecas]
            if tipo.startswith("display"):
                try:
                    itens.append((dist_caixas(cm, zona(ZONA_DISPLAY)), "a propria sombra do display"))
                except KeyError:
                    pass
            if not itens:
                continue
            if all(d >= limite_mm for d, _n in itens):
                medidos.append((tipo, limite_mm, min(itens)))
            for d, nome in sorted(itens):
                if d >= limite_mm:
                    continue
                if ("RF9", nome) in DESVIOS:
                    desvios.append(("RF9", f"{tipo}: {nome} a {d:.1f} mm, a ficha recomenda "
                                           f"{limite_mm:.0f}", DESVIOS[("RF9", nome)]))
                else:
                    perto_demais.append((tipo, limite_mm, (d, nome)))
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
    # from the board's own net table, never from nets.py at run time
    _numeros = redes_do_arquivo()
    _por_num = {n: r for r, n in _numeros.items()}
    par = {}
    for r in ("USB_DP", "USB_DM"):
        sg = [q for q in seg if _por_num.get(q["n"], "") == r]
        comp = sum(math.hypot(q["b"][0] - q["a"][0], q["b"][1] - q["a"][1])
                   for q in sg)
        larg = sorted({round(q["w"], 3) for q in sg})
        par[r] = (len(sg), comp, larg)
    alvo = _R.LARGURA_USB_CALC
    entre_eixos = _R.LARGURA_USB_CALC + _R.PASSO_PAR
    faltando = [r for r, v in par.items() if v[0] == 0]
    if faltando:
        falhou("US1", "o par nao esta roteado: " + ", ".join(faltando))
    else:
        larguras = sorted({w for v in par.values() for w in v[2]})
        # where the two run side by side, at the gap the impedance was
        # computed for (impedancia.py: 0.15 mm 0.2 apart, 93.6 ohm)
        dp = [q for q in seg if _por_num.get(q["n"]) == "USB_DP" and q["c"] == "F.Cu"]
        dm = [q for q in seg if _por_num.get(q["n"]) == "USB_DM" and q["c"] == "F.Cu"]
        juntos = 0.0
        for a in dp:
            ax, ay = a["b"][0] - a["a"][0], a["b"][1] - a["a"][1]
            la = math.hypot(ax, ay)
            if la < 0.3:
                continue
            for b in dm:
                bx, by = b["b"][0] - b["a"][0], b["b"][1] - b["a"][1]
                lb = math.hypot(bx, by)
                if lb < 0.3 or abs(ax * by - ay * bx) / (la * lb) > 0.02:
                    continue
                d = abs((b["a"][0] - a["a"][0]) * -ay / la + (b["a"][1] - a["a"][1]) * ax / la)
                t = sorted([((b["a"][0] - a["a"][0]) * ax + (b["a"][1] - a["a"][1]) * ay) / la,
                            ((b["b"][0] - a["a"][0]) * ax + (b["b"][1] - a["a"][1]) * ay) / la])
                if abs(d - entre_eixos) < 0.01:
                    juntos += max(0.0, min(la, t[1]) - max(0.0, t[0]))
        if max(larguras) > alvo + 1e-6 or min(larguras) < alvo - 1e-6:
            falhou("US1", "o par esta roteado a " + ", ".join(f"{w:.3f}" for w in larguras) +
                   f" mm, e 90 ohm diferenciais nesta pilha pedem {alvo:.3f} mm com "
                   f"{_R.PASSO_PAR:.1f} de vao (impedancia.py)")
        else:
            ok.append(f"US1: o par esta roteado a {alvo:.3f} mm, e corre junto a "
                      f"{_R.PASSO_PAR:.1f} mm de vao por {juntos:.1f} mm: 93,6 ohm "
                      f"diferenciais na JLC3313 (impedancia.py)")

    # -- US2: o par com o mesmo comprimento ------------------------------------
    def pad_de(ref, num):
        return next((q for q in pecas.get(ref, {}).get("pads", []) if str(q["pad"]) == num), None)
    n_dp, n_dm = _numeros.get("USB_DP"), _numeros.get("USB_DM")
    pontas = {"USB_DP": (pad_de("D102", "6"), pad_de("U201", "8")),
              "USB_DM": (pad_de("D102", "7"), pad_de("U201", "7"))}
    if None not in pontas["USB_DP"] + pontas["USB_DM"]:
        l_dp = comprimento(n_dp, seg, vias, *pontas["USB_DP"])
        l_dm = comprimento(n_dm, seg, vias, *pontas["USB_DM"])
        # the receptacle has each line twice (A6 and B6, A7 and B7): which
        # one the plug touches decides which line goes through its tie
        laco_dp = comprimento(n_dp, seg, vias, pad_de("J101", "A6"), pad_de("J101", "B6"))
        laco_dm = comprimento(n_dm, seg, vias, pad_de("J101", "B7"), pad_de("J101", "A7"))
        nota = (f"; as amarracoes dos contatos duplicados do USB-C somam {laco_dp:.1f} mm no "
                f"D+ e {laco_dm:.1f} no D-, e uma delas entra no caminho conforme o lado do plugue")
        if abs(l_dp - l_dm) > LIMITE_SKEW_USB:
            falhou("US2", f"do diodo ao modulo o D+ tem {l_dp:.2f} mm e o D- {l_dm:.2f}: "
                          f"{abs(l_dp - l_dm):.2f} mm de diferenca" + nota)
        else:
            ok.append(f"US2: do diodo ao modulo o D+ tem {l_dp:.2f} mm e o D- {l_dm:.2f} "
                      f"({abs(l_dp - l_dm):.2f} mm de diferenca)" + nota)

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
        # edge to edge: the copper the loop has to cross. Pad centre to pad
        # centre, as this rule measured until 2026-10-19, adds half of each
        # pad - 1.27 mm for C304 against the receiver's 1.8 mm pin
        d = min(vao_pads(a, b) for b in alvo for a in pecas[cap]["pads"] if a["rede"] in redes)
        lim = LIMITE_MODULO if chip == MODULO else limite(cap)
        piores.append((d, cap, chip, lim))
    piores.sort(reverse=True)
    acima = []
    for d, c, u, lim in piores:
        if d <= lim:
            continue
        if ("AL1", c) in DESVIOS:
            desvios.append(("AL1", f"{c} a {d:.2f} mm do pino do {u}, a ficha pede {lim:g}",
                            DESVIOS[("AL1", c)]))
        else:
            acima.append((d, c, u, lim))
    hf = [t for t in piores if t[3] == LIMITE_HF]
    res = [t for t in piores if t[3] == LIMITE_RESERVA]
    mod = [t for t in piores if t[3] == LIMITE_MODULO]
    if acima:
        falhou("AL1", f"{len(acima)} de {len(piores)} capacitores alem do limite: " +
               ", ".join(f"{c}->{u} {d:.2f} de {lim:g}" for d, c, u, lim in acima[:6]))
    else:
        mhf = min(mod, key=lambda t: t[0])
        ok.append(f"AL1: {len(piores)} capacitores, de borda a borda: o pior de alta frequencia "
                  f"a {hf[0][0]:.2f} mm ({hf[0][1]}), o pior de reserva a {res[0][0]:.2f} "
                  f"({res[0][1]}), e o de alta frequencia do modulo a {mhf[0]:.2f} ({mhf[1]})")

    # -- AL3: o laco de chaveamento -----------------------------------------
    _nums = redes_do_arquivo()
    laco = []
    for rede, chip, ind in CONVERSORES:
        n = _nums.get(rede)
        if n is None or chip not in pecas or ind not in pecas:
            continue
        cobre = sum(math.hypot(q["b"][0] - q["a"][0], q["b"][1] - q["a"][1])
                    for q in seg if q["n"] == n)
        nv = sum(1 for v in vias if v["n"] == n)
        gap = min(vao_pads(a, b) for a in pecas[chip]["pads"] if a["rede"] == rede
                  for b in pecas[ind]["pads"] if b["rede"] == rede)
        laco.append((rede, cobre, nv, gap))
    ruins = [t for t in laco if t[1] > LIMITE_SW_COBRE or t[2] or t[3] > LIMITE_SW_INDUTOR]
    texto = ", ".join(f"{r} {c:.1f} mm, {v} vias, indutor a {g:.1f}" for r, c, v, g in laco)
    if ruins:
        falhou("AL3", "no de chaveamento longo demais: " + texto)
    elif laco:
        ok.append("AL3: " + texto)

    # -- AL4: resistencia em serie no VCC do receptor -------------------------
    p_l = next((q for q in pecas.get(FONTE_3V0[0], {}).get("pads", []) if q["rede"] == FONTE_3V0[1]), None)
    p_jp = {q["rede"]: q for q in pecas.get("JP103", {}).get("pads", [])}
    p_fb = {q["rede"]: q for q in pecas.get("FB301", {}).get("pads", [])}
    p_vcc = next((q for q in pecas.get(GNSS, {}).get("pads", [])
                  if q["rede"] == "3V0_GNSS" and q["pad"] == next(
                      pn.number for pn in P.PARTS[GNSS].pins if pn.name == "VCC")), None)
    if p_l and p_vcc and "3V0" in p_jp and "3V0_BLOCO" in p_fb:
        trechos = [("3V0", p_l, p_jp["3V0"]), ("3V0_BLOCO", p_jp["3V0_BLOCO"], p_fb["3V0_BLOCO"]),
                   ("3V0_GNSS", p_fb["3V0_GNSS"], p_vcc)]
        cobre = {r: resistencia(_nums[r], seg, vias, a, b, [q for q in pads if q["rede"] == r])
                 for r, a, b in trechos}
        total = sum(cobre.values()) + sum(SERIE_VCC_GNSS.values())
        texto = (f"{total * 1000:.0f} mOhm no pior caso, do indutor do BUCK2 ao pino VCC: "
                 f"cobre {sum(cobre.values()) * 1000:.0f} ("
                 + ", ".join(f"{r} {v * 1000:.1f}" for r, v in cobre.items()) + ") e "
                 + ", ".join(f"{r} {v * 1000:.0f}" for r, v in SERIE_VCC_GNSS.items()))
        if total > LIMITE_VCC_GNSS:
            falhou("AL4", texto + f", acima dos {LIMITE_VCC_GNSS * 1000:.0f}")
        else:
            ok.append("AL4: " + texto)

    # -- AL2: largura por IPC-2221 -------------------------------------------
    numeros = redes_do_arquivo()
    por_num = {n: r for r, n in numeros.items()}
    estreitas = []
    necks = 0
    for s in seg:
        rede = por_num.get(s["n"], "")
        if rede not in CORRENTE:
            continue
        i, _fonte = CORRENTE[rede]
        pedida = largura_ipc(i, interna=s["c"] in ("In1.Cu", "In2.Cu"))
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
                  f"(10 C de subida, cobre de {MP.CU * 1000:.0f} um por fora e {MP.CU_INTERNO * 1000:.1f} por dentro); {necks} estreitamentos "
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
    TERMICOS = {("U104", "5"), ("U103", "25"), ("U505", "7")}
    # The nPM1300's exposed pad is the other way round: Nordic's reference
    # layout puts a 3 x 3 matrix of vias in it (PS v1.1, figures 61 to 64),
    # the path for the heat of a charger that dissipates up to 1,5 W. Until
    # 2026-10-19 it was in the list above, which no datasheet asked for.
    COM_MATRIZ = {("U101", "33"): 9}
    sobre_termico = []
    for q in pads:
        if (q["ref"], q["pad"]) not in TERMICOS:
            continue
        for v in vias:
            # "sob o pad" e o pad mais 0,5 mm de margem, nao 1,2 mm fixos:
            # com 1,2 a regra acusou uma via de terra a 1,3 mm do centro de
            # um pad de 0,58, que esta a 1 mm do cobre dele. A ponte de
            # mascara entre via e pad e assunto do DRC, que a mede
            if (abs(v["x"] - q["x"]) < q["hw"] + 0.5
                    and abs(v["y"] - q["y"]) < q["hh"] + 0.5):
                sobre_termico.append((q["ref"], q["pad"],
                                      round(math.hypot(v["x"] - q["x"],
                                                       v["y"] - q["y"]), 2)))
                break
    poucas = []
    for q in pads:
        quer = COM_MATRIZ.get((q["ref"], q["pad"]))
        if quer is None:
            continue
        tem = sum(1 for v in vias
                  if abs(v["x"] - q["x"]) < q["hw"] and abs(v["y"] - q["y"]) < q["hh"])
        if tem < quer:
            poucas.append(f"{q['ref']}.{q['pad']} com {tem} de {quer}")
    if sobre_termico or poucas:
        falhou("AL6", f"{len(sobre_termico)} vias sobre um pad termico: " +
               ", ".join(f"{r}.{p} a {d} mm" for r, p, d in sobre_termico[:6]) +
               ("; vias termicas faltando: " + ", ".join(poucas) if poucas else ""))
    else:
        ok.append(f"AL6: nenhuma via sobre os {len(TERMICOS)} pads termicos sem "
                  f"via, e as {sum(COM_MATRIZ.values())} vias termicas do nPM1300 no lugar")

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
        sensor = pecas["U505"]
        alt_sensor = alt_de.get("U505", 0.0)
        for r, p in pecas.items():
            if r == "U505" or r not in alt_de:
                continue
            # a part on the other face cannot shade the sensor's window,
            # and one no taller than the sensor casts no shadow on its top
            # (the guideline is about TALLER neighbours): until 2026-09-26
            # the rule counted the barometer on the back and every 0402
            if p.get("atras", False) != sensor.get("atras", False):
                continue
            if alt_de[r] <= alt_sensor + 1e-9:
                continue
            d = dist_caixas(p["caixa"], sensor["caixa"])
            if d < 2 * alt_de[r]:
                perto_luz.append((r, round(d, 2), alt_de[r]))
        if perto_luz:
            falhou("OP1", f"{len(perto_luz)} pecas mais altas que o sensor de luz "
                   f"({alt_sensor:g}) a menos de duas vezes a propria altura: " +
                   ", ".join(f"{r} a {d} mm, alta {h}" for r, d, h in
                             sorted(perto_luz, key=lambda t: t[1])[:5]))
        else:
            ok.append(f"OP1: nenhuma peca da mesma face mais alta que o sensor "
                      f"de luz ({alt_sensor:g} mm) a menos de duas vezes a "
                      "propria altura dele")

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
    # 04-pcb-e-caixa.md gives two ceilings: 3.0 mm on the front, under the
    # display (2.6 until 2026-09-26: the receiver's 2.7 maximum did not fit,
    # and the case lowered the board 0.4 mm instead - make_caixa.DISPLAY_VAO),
    # and 1.2 mm on the back, under the battery. A part taller than the
    # shadow it stands in does not fit, and no DRC will ever say so - which
    # is the whole reason the heights had to come from the datasheets.
    TETOS = ((ZONA_DISPLAY, 3.0, False),
             ("SOMBRA_BATERIA_MAX_1-2MM", 1.2, True))
    altos = []
    sem_altura = []
    sem_zona: set = set()
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
                # A zona nao existe: ate 2026-09-26 isto era um `continue`
                # mudo e a regra saia "ok" sem medir peca nenhuma - as
                # sombras foram tiradas do make_dxf de proposito, porque a
                # caixa nao esta definida, e a regra continuou passando.
                sem_zona.add(znome)
                continue
            if pe["atras"] != atras:
                continue
            if dist_caixas(pe["caixa"], z) >= 0:
                continue                      # not under this shadow
            if h > teto + 1e-9:
                altos.append((ref, h, teto, znome))
    if sem_zona:
        falhou("ME2", "nao mede nada: as zonas " + ", ".join(sorted(sem_zona))
                      + " nao existem no make_dxf - a posicao do display e da "
                      "celula sobre a placa nao esta definida em arquivo nenhum")
    elif altos:
        falhou("ME2", f"{len(altos)} pecas mais altas que o teto da sombra em "
               "que estao: " +
               ", ".join(f"{r} {h:.2f} contra {t:.1f} mm"
                         for r, h, t, _z in altos[:6]))
    else:
        ok.append(f"ME2: nenhuma peca passa do teto da sombra em que esta "
                  f"({TETOS[0][1]:.1f} mm sob o display, {TETOS[1][1]:.1f} mm sob a bateria); "
                  f"{len(sem_altura)} pecas sem altura conhecida")

    # -- ME4: a boca do conector de borda aponta para fora? ------------------
    # O 2D nao responde isto e o DRC muito menos: as ilhas de um receptaculo
    # USB-C sao quase simetricas em y, entao a peca "cabe" nos dois sentidos,
    # e a diferenca esta so no corpo. Em 2026-09-25 o J101 passou o dia a 180
    # graus, com a boca 10 mm para dentro da placa, e quem viu foi o dono - no
    # desenho 3D, duas vezes, porque da primeira eu "corrigi" para o lado
    # errado. Aqui a pergunta e feita ao STEP do fabricante e nao a um numero
    # decorado: um raio atravessa o corpo ao longo do eixo do conector, na
    # meia altura da cavidade. Do lado da boca ele entra na parede externa e
    # so reencontra material milimetros adiante, que e o vao onde o plugue
    # entra; do lado de tras bate em material logo na entrada. O lado com o
    # vao tem de ser o que olha para a borda.
    for ref in sorted(CORPOS_CONFERIDOS):
        achado = _ilhas_sob_o_corpo(ref, pecas)
        if achado is None:
            falhou("ME4", f"{ref}: nao achei o corpo 3D para conferir")
            continue
        fora, total, caixa = achado
        if fora:
            falhou("ME4", f"{ref}: {len(fora)} das {total} ilhas de solda "
                          f"ficam FORA do corpo do modelo 3D "
                          f"({' '.join(sorted(set(fora))[:6])}); o modelo esta "
                          f"deslocado ou girado em relacao ao footprint")
        else:
            ok.append(f"ME4: as {total} ilhas de {ref} ficam sob o corpo do "
                      f"modelo do fabricante")

    # -- ME5: os rabichos do modelo ficam sobre as ilhas de sinal? ----------
    # A pergunta que a ME4 nao responde: de que lado esta a peca. Medido nos
    # corpos pequenos e finos do GLB que encostam na placa, contra o centro
    # das ilhas de sinal do footprint.
    for ref in sorted(RABICHOS_CONFERIDOS):
        achado = _rabichos_sobre_as_ilhas(ref, pecas)
        if achado is None:
            falhou("ME5", f"{ref}: nao achei rabichos de solda no modelo 3D "
                          "para conferir")
            continue
        media, n_rab, ci, meio = achado
        if media < 0.0:
            falhou("ME5", f"{ref}: o que sai do corpo do modelo junto da placa "
                          f"(os rabichos, {n_rab} faces) esta a {-media:.1f} mm "
                          "do centro do corpo pelo lado OPOSTO ao das ilhas de "
                          f"sinal (({ci[0]:.1f}; {ci[1]:.1f})): o modelo esta "
                          "girado 180 graus em relacao ao footprint")
        else:
            ok.append(f"ME5: os rabichos de {ref} saem do corpo pelo lado das "
                      f"ilhas de sinal ({media:.1f} mm do centro, corpo de "
                      f"{meio:.1f} de meia largura; {n_rab} faces)")

    # -- ME6: cada corpo 3D com o eixo e os lados do seu footprint ----------
    # Toda peca que tem modelo, do 0402 ao modulo: a caixa do corpo contra
    # o F.Fab, e as pernas contra as fileiras de ilhas. E a pergunta do dono
    # sobre os passivos e os CIs "girados", respondida peca a peca.
    girados = []
    n_medidos = 0
    sem_corpo = []
    for ref in sorted(pecas):
        if ref.startswith(("TP", "REF", "JP")):
            continue
        achado = _corpo_bate_com_o_footprint(ref, pecas)
        if achado is None:
            sem_corpo.append(ref)
            continue
        n_medidos += 1
        if achado["eixo"] != "ok" or achado["lados"] == "girado 180":
            girados.append((ref, achado))
    if girados:
        falhou("ME6", f"{len(girados)} de {n_medidos} corpos fora do eixo ou "
                      "do lado das ilhas: " +
                      "; ".join(f"{r} {a['eixo'] if a['eixo'] != 'ok' else a['lados']} "
                                f"(corpo {a['modelo'][0]:.2f} x {a['modelo'][1]:.2f}, "
                                f"F.Fab {a['fab'][0]:.2f} x {a['fab'][1]:.2f})"
                                for r, a in girados[:6]))
    else:
        ok.append(f"ME6: os {n_medidos} corpos 3D medidos tem o eixo do F.Fab "
                  f"e as pernas do lado das ilhas; {len(sem_corpo)} pecas sem "
                  "corpo ou sem F.Fab para medir")

    # -- FA1: via em ilha de SMD so com POFV ---------------------------------
    em_ilha = []
    for v in vias:
        for q in pads:
            if not q["smd"] or q["rede"] == "" or _nums.get(q["rede"]) != v["n"]:
                continue
            cx = caixa_pad(q)
            if cx[0] <= v["x"] <= cx[2] and cx[1] <= v["y"] <= cx[3]:
                em_ilha.append(f"{q['ref']}.{q['pad']}")
                break
    if em_ilha and "POFV" not in FABRICACAO["via_em_pad"]:
        falhou("FA1", f"{len(em_ilha)} vias dentro de ilha de SMD, e o pedido nao pede POFV")
    else:
        ok.append(f"FA1: {len(em_ilha)} vias dentro de ilha de SMD; o pedido pede "
                  f"{FABRICACAO['via_em_pad']}, na pilha {FABRICACAO['pilha']}")

    # -- RT1: a placa esta inteira? ------------------------------------------
    # O DRC do KiCad com TODAS as severidades. Ate 2026-09-25 as rodadas a
    # mao usavam --severity-error, e "0 itens desconectados" saiu impresso
    # em documento e em pull request enquanto havia 55: item desconectado e
    # aviso, e o filtro de erros o esconde.
    import subprocess
    sys.path.insert(0, str(HERE))
    import make_3d as _M3
    rel = HERE / "_drc.json"
    r = subprocess.run([str(_M3.KICAD_CLI), "pcb", "drc", "--severity-all",
                        "--format", "json", "--output", str(rel), str(PCB)],
                       capture_output=True, text=True)
    if not rel.exists():
        falhou("RT1", "o kicad-cli nao rodou o DRC: " + r.stderr.strip()[:120])
    else:
        import json as _json
        d = _json.loads(rel.read_text(encoding="utf-8"))
        soltos = d.get("unconnected_items", [])
        por_rede: dict[str, int] = {}
        for u in soltos:
            for it in u.get("items", []):
                s = it.get("description", "")
                if "[" in s and "]" in s:
                    n = s[s.index("[") + 1:s.index("]")]
                    por_rede[n] = por_rede.get(n, 0) + 1
        viol = d.get("violations", [])
        tipos: dict[str, int] = {}
        for v in viol:
            tipos[v.get("type", "?")] = tipos.get(v.get("type", "?"), 0) + 1
        if soltos:
            piores = ", ".join(f"{k} {v}" for k, v in
                               sorted(por_rede.items(), key=lambda kv: -kv[1])[:8])
            falhou("RT1", f"{len(soltos)} itens desconectados em "
                          f"{len(por_rede)} redes ({piores}); e o DRC completo "
                          f"tem {len(viol)} violacoes: "
                          + ", ".join(f"{k} {v}" for k, v in sorted(tipos.items())))
        else:
            ok.append(f"RT1: 0 itens desconectados no DRC completo; "
                      f"{len(viol)} violacoes de qualquer severidade"
                      + (" (" + ", ".join(f"{k} {v}" for k, v in sorted(tipos.items())) + ")"
                         if viol else ""))

    # -- resultado -------------------------------------------------------------
    for linha in ok:
        print(f"  ok      {linha}")
    print()
    for rid, msg, porque in desvios:
        print(f"  desvio  {rid}: {msg}")
        print(f"          aceito pelo dono em 2026-10-19: {porque}")
    if desvios:
        print()
    for rid, msg in achados:
        fonte = next((f for i, _d, f in REGRAS if i == rid), "")
        print(f"  FALHA   {rid}: {msg}")
        print(f"          fonte: {fonte}")
    print()
    medidas = ({r for r, _ in achados} | {linha.split(":")[0] for linha in ok}
               | {r for r, _m, _p in desvios})
    naomedidas = [(i, d, f) for i, d, f in REGRAS if i not in medidas]
    if naomedidas:
        print("  nao medido aqui (precisa de bancada, do empilhamento do "
              "fabricante ou de modelo 3D):")
        for i, d, _f in naomedidas:
            print(f"    {i}: {d}")
    print()
    com_desvio = len({r for r, _m, _p in desvios})
    print(f"{len(ok)} regras medidas e cumpridas, {com_desvio} com desvio aceito, "
          f"{len(achados)} violadas, {len(naomedidas)} nao medidas")
    return 1 if achados else 0


if __name__ == "__main__":
    sys.exit(main())
