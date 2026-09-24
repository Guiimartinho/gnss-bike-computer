#!/usr/bin/env python3
"""Every part of the board, with its pins.

The reference designators and the values come from
hardware_gnssbike/05-materiais.md; the pin names come from
hardware_gnssbike/03-netlist.md and from the manufacturers' datasheets.

Pin NUMBERS are the dangerous part. A wrong number is a wrong netlist and a
wrong board, so a part whose numbering has not been read off the datasheet
carries `confirmed=False`, and its symbol says so on the drawing. Nothing here
is guessed: where the number is unknown the pin keeps its name as its number
and the part is marked.
"""

from __future__ import annotations

from sch_lib import Part, Pin

# Sides: L and R for signals, T for supply, B for ground - the usual reading
# order of a schematic, and what keeps the wires off the bodies.
L, R, T, B = "L", "R", "T", "B"

PARTS: dict[str, Part] = {}
UNCONFIRMED: set[str] = set()


def add(ref: str, value: str, pins: list[tuple], *, confirmed: bool,
        note: str = "", footprint: str = "") -> Part:
    """pins: (number, name, etype, side). number None means 'not read yet'."""
    ps = []
    for number, name, etype, side in pins:
        ps.append(Pin(str(number) if number is not None else name, name, etype, side))
    if not confirmed:
        UNCONFIRMED.add(ref)
        note = (note + " | PINAGEM NAO CONFIRMADA NA FICHA").strip(" |")
    p = Part(ref, value, tuple(ps), footprint=footprint, note=note)
    PARTS[ref] = p
    return p


def passive(ref: str, value: str, note: str = "", vertical: bool = False) -> Part:
    """A two-terminal part. Pins 1 and 2 are the two ends, which is universal."""
    sides = (T, B) if vertical else (L, R)
    return add(ref, value, [(1, "1", "passive", sides[0]), (2, "2", "passive", sides[1])],
               confirmed=True, note=note)


# ---------------------------------------------------------------- folha 1
# Molex 2036150003, sales drawing 2036150003 PSD 000 rev A: a USB 2.0
# receptacle with 16 of the 24 contacts, numbered as the USB-IF standard does.
# A2, A3, A10, A11, B2, B3, B10 and B11 (the SuperSpeed pairs) do not exist.
add("J101", "Molex 2036150003", [
    ("A4", "VBUS_A4", "power_out", T), ("A9", "VBUS_A9", "power_out", T),
    ("B4", "VBUS_B4", "power_out", T), ("B9", "VBUS_B9", "power_out", T),
    ("A5", "CC1", "passive", R), ("B5", "CC2", "passive", R),
    ("A6", "DP_A6", "passive", R), ("B6", "DP_B6", "passive", R),
    ("A7", "DM_A7", "passive", R), ("B7", "DM_B7", "passive", R),
    ("A8", "SBU1", "passive", R), ("B8", "SBU2", "passive", R),
    ("A1", "GND_A1", "power_out", B), ("A12", "GND_A12", "power_out", B),
    ("B1", "GND_B1", "power_out", B), ("B12", "GND_B12", "power_out", B),
], confirmed=True,
    note="USB-C IPX8, 16 contatos, USB 2.0. As pernas da blindagem sao dois "
         "furos marcados GND no desenho, sem numero de contato")

# ESD761: SLVSH10C. It is a BIDIRECTIONAL clamp between IO and GND - it has no
# anode and no cathode, which is what a draft of this schematic assumed.
add("D101", "ESD761DPYR", [
    (1, "IO", "passive", L), (2, "GND", "passive", B),
], confirmed=True, note="TVS do VBUS, bidirecional, 24 V de trabalho")

# TPD4E05U06 in DQA is a USON of TEN pins, 1.0 x 2.5 mm: four ESD channels,
# two grounds and four pins TI means to be used for straight-through routing,
# which is why they are passive here and not no_connect.
add("D102", "TPD4E05U06DQAR", [
    (1, "D1P", "passive", L), (2, "D1N", "passive", L), (3, "GND1", "passive", B),
    (4, "D2P", "passive", R), (5, "D2N", "passive", R), (6, "NC1", "passive", R),
    (7, "NC2", "passive", R), (8, "GND2", "passive", B),
    (9, "NC3", "passive", R), (10, "NC4", "passive", R),
], confirmed=True, note="ESD de CC1, CC2, D+ e D-")

# nPM1300 Product Specification v1.1, table 35 and figure 50, QFN32.
# Note VSET1 and VSET2, not RVSET; and VDDIO on pin 12, which is the supply of
# the TWI and of the GPIOs - the domain 08-layout.md still carries as an open
# question. It is a real pin and it needs a rail.
add("U101", "nPM1300-QEAA", [
    (1, "VOUT1", "power_out", R), (2, "PVSS1", "power_in", B),
    (3, "SW1", "passive", R), (4, "PVDD", "power_in", L),
    (5, "SW2", "passive", R), (6, "PVSS2", "power_in", B),
    (7, "GPIO0", "bidirectional", R), (8, "GPIO1", "bidirectional", R),
    (9, "GPIO2", "bidirectional", R), (10, "GPIO3", "bidirectional", R),
    (11, "GPIO4", "bidirectional", R), (12, "VDDIO", "power_in", T),
    (13, "SDA", "bidirectional", L), (14, "SCL", "input", L),
    (15, "SHPHLD", "input", L), (16, "VSET2", "input", L),
    (17, "VSET1", "input", L), (18, "NTC", "input", L),
    (19, "VBAT", "power_in", L), (20, "VSYS", "power_out", R),
    (21, "VBUS", "power_in", L), (22, "VBUSOUT", "power_out", R),
    (23, "CC1", "input", L), (24, "CC2", "input", L),
    (25, "LED0", "open_collector", R), (26, "LED1", "open_collector", R),
    (27, "LED2", "open_collector", R), (28, "LSIN1", "power_in", L),
    (29, "LSOUT1", "power_out", R), (30, "LSIN2", "power_in", L),
    (31, "LSOUT2", "power_out", R), (32, "VOUT2", "power_out", R),
    (33, "AVSS", "power_in", B),
], confirmed=True, note="PMIC: carregador, 2 bucks, 2 chaves de carga")

passive("L101", "2,2 uH", "indutor do BUCK1")
passive("L102", "2,2 uH", "indutor do BUCK2")

# MAX17262 datasheet 19-100308 rev 0, WLP of nine balls. The 7 mOhm sense is
# internal, between BATT and SYS: the electrical table gives RSNS = 7 mOhm and
# the ordering information lists only "Internal Sensing" parts.
add("U102", "MAX17262REWL", [
    ("A1", "TH", "input", L), ("A2", "BATT", "power_in", L),
    ("A3", "NC", "no_connect", R), ("B1", "SCL", "input", L),
    ("B2", "ALRT", "open_collector", R), ("B3", "SYS", "power_out", R),
    ("C1", "SDA", "bidirectional", L), ("C2", "REG", "power_out", R),
    ("C3", "GND", "power_in", B),
], confirmed=True, note="medidor de carga; sensor de 7 mOhm INTERNO entre BATT e SYS")

# AEM10900, DS-AEM10900-v1.6.0, figure 3 and table 2, QFN28 plus thermal pad.
# There are no CSRC, CINT, CSTO or RDIV pins: a draft of this schematic
# invented them. The capacitors hang off SRC, VINT and STO, and the 22 k is
# the divider resistor between TH_REF and TH_MON, not a pin.
add("U103", "AEM10900", [
    (1, "GND1", "power_in", B), (2, "NC1", "no_connect", R),
    (3, "BUFSRC", "passive", L), (4, "GND2", "power_in", B),
    (5, "SW_DCDC", "passive", R), (6, "VINT", "power_out", R),
    (7, "NC2", "no_connect", R), (8, "STO", "passive", R),
    (9, "DIS_STO_CH", "input", L), (10, "I2C_VDD", "power_in", T),
    (11, "IRQ", "output", R), (12, "SCL", "input", L),
    (13, "SDA", "bidirectional", L), (14, "KEEP_ALIVE", "input", L),
    (15, "R_MPP1", "input", L), (16, "T_MPP0", "input", L),
    (17, "STO_CFG1", "input", L), (18, "VINT2", "power_out", R),
    (19, "STO_CFG0", "input", L), (20, "R_MPP0", "input", L),
    (21, "T_MPP1", "input", L), (22, "STO_CFG2", "input", L),
    (23, "GND3", "power_in", B), (24, "R_MPP2", "input", L),
    (25, "TH_MON", "input", L), (26, "TH_REF", "output", R),
    (27, "ZMPP", "passive", R), (28, "SRC", "power_in", L),
    (29, "GND_PAD", "power_in", B),
], confirmed=True,
    note="colhedor solar com MPPT; VINT sai em dois pinos, 6 e 18; o pad "
         "termico e o pino 29 e e a ligacao principal de GND")

passive("L103", "4,7 uH", "indutor do SW_DCDC")

# TPS7A02 in DQN is an X2SON of four pins, 1.0 x 1.0 mm, not a SOT-563.
add("U104", "TPS7A0218PDQN", [
    (1, "OUT", "power_out", R), (2, "GND", "power_in", B),
    (3, "EN", "input", L), (4, "IN", "power_in", L),
    (5, "PAD", "power_in", B),
], confirmed=True, note="LDO de 1,8 V do VBCKP do receptor; pad termico ao GND")

# KXOB25-05X3F: the datasheet does NOT number the terminals. The silkscreen on
# the back marks + on one pad and - on the other, and 1 and 2 below follow the
# usual convention; if the footprint numbers them the other way, this follows.
for i in range(1, 7):
    add(f"PV10{i}", "KXOB25-05X3F", [
        (1, "P", "passive", R), (2, "N", "passive", B),
    ], confirmed=False,
        note="modulo solar de 3 celulas, 23 x 8 mm; a ficha nao numera os "
             "terminais, so marca + e - na serigrafia do verso")

add("J102", "JST SM06B-GHS-TB", [
    (1, "1", "passive", R), (2, "2", "passive", R), (3, "3", "passive", R),
    (4, "4", "passive", R), (5, "5", "passive", R), (6, "6", "passive", R),
], confirmed=True,
    note="conector da celula, 6 vias, passo 1,25 mm; o catalogo da JST nao da "
         "funcao a contato nenhum, e quem decide e o fabricante do pack")

passive("RT101", "10 k B3380", "NTC do TH_MON, na face de tras sob a celula")
# APT1608SURCK spec DSAD0926 rev V.22A: the cathode bar is on terminal 1, so
# the ANODE IS TERMINAL 2. A draft of this schematic had it the other way.
add("D103", "APT1608SURCK", [(1, "K", "passive", L), (2, "A", "passive", R)],
    confirmed=True, note="LED de carga; anodo no terminal 2")
add("D104", "APT1608SURCK", [(1, "K", "passive", L), (2, "A", "passive", R)],
    confirmed=True, note="LED de erro; anodo no terminal 2")

passive("R102", "47 k", "VSET1: BUCK1 em 1,8 V")
passive("R103", "150 k", "VSET2: BUCK2 em 3,0 V")
passive("R104", "100 k", "divisor do DIS_STO_CH")
passive("R105", "1 M", "divisor do DIS_STO_CH")
passive("R106", "22 k", "RDIV do AEM10900")
passive("R107", "4,7 k", "pull-up do PWR_SDA")
passive("R108", "4,7 k", "pull-up do PWR_SCL")
passive("R109", "10 k", "pull-up do PMIC_INT")
passive("R110", "10 k", "pull-up do ALRT do MAX17262")
passive("R111", "10 k", "pull-up do IRQ do AEM10900")
passive("R112", "10 k", "pull-up do INT do OPT3001")

for n, v in (("C101", "10 uF"), ("C102", "10 uF"), ("C103", "10 uF"), ("C104", "10 uF"),
             ("C105", "10 uF"), ("C106", "10 uF"), ("C107", "10 uF"), ("C108", "10 uF"),
             ("C109", "10 uF"), ("C110", "1 uF"), ("C111", "1 uF"), ("C112", "2,2 uF"),
             ("C113", "1 uF"), ("C114", "1 uF"), ("C115", "22 uF"), ("C116", "22 uF"),
             ("C117", "22 uF"), ("C118", "0,47 uF")):
    passive(n, v)

passive("JP101", "0 R 1206", "jumper de medicao da corrente da celula")

# ---------------------------------------------------------------- folha 2
# MinewSemi ME54BS13 pad map, from "ME54BS13-nRF54LM20A Datasheet K EN"
# V1.0.0 of 2026-06-23, pages 6 to 9, cross-checked against the module's own
# schematic symbol on page 10 and against an independent KiCad footprint.
#
# The module is a hybrid: 20 CASTELLATED pads numbered 1 to 20 down the two
# long sides, and a 60 pad LGA matrix A0..F9 in the middle. 80 pads, 64 GPIO.
#
# Careful with one thing: three of the port pins this board uses come out on
# the castellated row, not on the matrix - P1.15 to P1.19 and P1.23 and P1.26
# are the ones the module brings to the edge.
PADS_ME54BS13 = {
    # LGA matrix, columns A..F left to right, rows 0..9 top to bottom
    "P0.04": "A0", "P0.02": "B0", "P0.05": "C0",
    "P0.01": "A1", "P0.03": "B1", "P0.06": "C1", "P0.07": "D1",
    "P0.08": "E1", "P0.09": "F1",
    "P0.00": "A2", "P3.03": "B2", "P3.07": "C2", "P3.11": "D2",
    "P3.12": "E2", "P1.14": "F2",
    "P3.04": "A3", "P3.02": "B3", "P3.08": "C3", "P3.10": "D3",
    "P1.10": "E3", "P1.13": "F3",
    "P1.01": "A4", "P3.01": "B4", "P3.06": "C4", "P1.24": "D4",
    "P1.22": "E4", "P1.12": "F4",
    "P1.02": "A5", "P3.00": "B5", "P3.05": "C5", "P3.09": "D5",
    "P1.25": "E5", "P1.11": "F5",
    "P1.06": "A6", "P1.07": "B6", "P1.08": "C6", "P1.09": "D6",
    "P1.28": "E6", "P1.27": "F6",
    "P1.00": "A7", "P1.31": "B7", "P1.05": "C7", "P2.10": "D7",
    "P2.09": "E7", "P2.06": "F7",
    "P1.04": "A8", "P1.03": "B8", "P2.04": "C8", "P2.05": "D8",
    "P2.07": "E8", "P2.08": "F8",
    "P1.30": "A9", "P1.29": "B9", "P2.03": "C9", "P2.02": "D9",
    "P2.01": "E9", "P2.00": "F9",
    # castellated row
    "P1.26": "12", "P1.23": "13", "P1.19": "14", "P1.18": "15",
    "P1.17": "16", "P1.16": "17", "P1.15": "18",
}
# only the port pins this board actually wires
_usados = ("P0.00", "P0.02", "P0.03", "P1.00", "P1.03", "P1.04", "P1.05", "P1.06",
           "P1.08", "P1.09", "P1.10", "P1.12", "P1.16", "P1.19", "P1.22", "P1.25",
           "P1.26", "P1.27", "P1.28", "P1.29", "P1.30", "P1.31", "P2.01", "P2.02",
           "P2.04", "P2.05", "P3.00", "P3.02", "P3.03", "P3.05", "P3.06", "P3.07",
           "P3.08")
_mod = [("7", "USB_DM", "bidirectional", R), ("8", "USB_DP", "bidirectional", R),
        ("9", "VBUS", "power_in", T), ("5", "SWDIO", "bidirectional", R),
        ("6", "SWDCLK", "input", R), ("4", "RESET", "input", L),
        ("2", "RF", "passive", R), ("19", "VDD", "power_in", T),
        ("1", "GND", "power_in", B), ("3", "GND3", "power_in", B),
        ("10", "GND10", "power_in", B), ("11", "GND11", "power_in", B),
        ("20", "GND20", "power_in", B), ("D0", "GND_D0", "power_in", B),
        ("E0", "GND_E0", "power_in", B), ("F0", "GND_F0", "power_in", B)]
for _i, _pp in enumerate(_usados):
    _mod.append((PADS_ME54BS13[_pp], _pp, "bidirectional", L if _i < 17 else R))
add("U201", "MinewSemi ME54BS13", _mod, confirmed=True,
    note="nRF54LM20A, 16,5 x 12,0 x 2,4 mm, 80 pads (20 castelados + 60 LGA), "
         "64 GPIO. A ficha V1.0.0 e a V0.5.0 discordam do espelhamento: "
         "CONFERIR num modulo real que os GND D0, E0 e F0 ficam do lado do "
         "VCC (pino 19) antes de fabricar")

add("J201", "Tag-Connect TC2030-NL", [
    (1, "VTref", "power_in", L), (2, "SWDIO", "bidirectional", R),
    (3, "NC3", "no_connect", R), (4, "SWDCLK", "output", R),
    (5, "GND", "power_in", B), (6, "RESET", "output", R),
], confirmed=True, note="so furos e pads; pinagem do padrao TC2030 da Tag-Connect")

# The sixteen test points of 06-conectores-e-pontos-de-teste.md. Eleven of
# the twelve on the power sheet were missing from the board entirely - only
# the three of the console sheet existed - and a rail you cannot put a probe
# on is a rail you cannot debug. TP111 and TP401 are NOT here, and both
# absences are the document's own: ST_STO is a node the AEM10900 datasheet
# does not have, and 5V0 belongs to a REG710NA-5 that is not fitted with the
# JDI panel.
TESTE = (
    ("TP101", "VBUS", "o que a fonte USB entrega, depois do TVS"),
    ("TP102", "VBUSOUT", "saida do limitador do nPM1300"),
    ("TP103", "VBAT", "lado SYS do medidor, 3,0 a 4,2 V"),
    ("TP104", "VSYS", "saida do power path; chega a 5,5 V com cabo"),
    ("TP105", "3V0", "BUCK2, o trilho do MCU"),
    ("TP106", "1V8", "BUCK1; maximo absoluto de 1,98 V no V_IO do receptor"),
    ("TP107", "SD3V0", "LDSW1, a flash; desligado tem de estar em 0 V"),
    ("TP108", "3V3BL", "LDSW2, a luz"),
    ("TP109", "VBCKP", "TPS7A02; continua de pe com o aparelho desligado"),
    ("TP110", "VINT", "interno do AEM10900; ponto de LEITURA, nao de alimentacao"),
    ("TP112", "GND", "referencia do bloco de energia, com via propria ao plano"),
)

for n in [t[0] for t in TESTE] + ["TP201", "TP202", "TP203"]:
    add(n, "pad", [(1, "1", "passive", R)], confirmed=True, note="ponto de teste")
for n, v in (("C201", "100 nF"), ("C210", "4,7 uF")):
    passive(n, v)

# Onde os tres grupos de modulos solares encostam na placa. Eles estao na
# caixa - dois na face inclinada e dois em cada chanfro - e 04:388 os liga
# "em 3 grupos, mola ou FPC". Sem estes pads a rede SRC nao saia do
# colhedor: os seis modulos ficavam sem destino.
for _n in ("J103", "J104", "J105"):
    add(_n, "contato de mola, 2 vias", [
        (1, "P", "passive", R), (2, "N", "passive", B),
    ], confirmed=False,
        note="um grupo de dois modulos KXOB25-05X3F; dois pads de 2,0 x 2,0 mm "
             "a 3,0 mm de passo, sem pasta. CONFERIR a mola antes de fabricar")

# ---------------------------------------------------------------- folha 3
# MAX-F10S data sheet UBXDOC-963802114-12732 R03, table 10, page 9.
add("U301", "u-blox MAX-F10S", [
    (1, "GND", "power_in", B), (2, "TXD", "output", R), (3, "RXD", "input", L),
    (4, "TIMEPULSE", "output", R), (5, "EXTINT", "input", L),
    (6, "V_BCKP", "power_in", T), (7, "V_IO", "power_in", T),
    (8, "VCC", "power_in", T), (9, "RESET_N", "input", L),
    (10, "GND2", "power_in", B), (11, "RF_IN", "input", L),
    (12, "GND3", "power_in", B), (13, "LNA_EN", "output", R),
    (14, "VCC_RF", "power_out", R), (15, "VIO_SEL", "input", L),
    (16, "SDA", "bidirectional", L), (17, "SCL", "input", L),
    (18, "SAFEBOOT_N", "input", L),
], confirmed=True,
    note="receptor L1+L5; com VIO_SEL no GND o V_IO tem maximo absoluto de 1,98 V, "
         "com ele aberto sobe para 3,6 V e o tradutor deixa de ser preciso")

# TXU0204 SCES936A, figure 6-2 and table 6-1. The part ordered is the BQA,
# WQFN-14: the DYY and DQM packages this project once assumed do not exist for
# this device. The Y in A3Y, A4Y, B1Y and B2Y marks the output of its channel.
add("U302", "TI TXU0204BQAR", [
    (1, "VCCA", "power_in", T), (2, "A1", "input", L), (3, "A2", "input", L),
    (4, "A3Y", "output", L), (5, "A4Y", "output", L), (6, "NC1", "no_connect", L),
    (7, "GND", "power_in", B), (8, "OE", "input", L), (9, "NC2", "no_connect", R),
    (10, "B4", "input", R), (11, "B3", "input", R), (12, "B2Y", "output", R),
    (13, "B1Y", "output", R), (14, "VCCB", "power_in", T),
    ("PAD", "PAD", "passive", B),
], confirmed=True,
    note="direcao FIXA: A1 e A2 vao de A para B; B3 e B4 vao de B para A. "
         "Pad termico ao GND, recomendado pela TI")

add("E301", "TE L000670-01", [(1, "FEED", "passive", R), (2, "GND", "passive", B)],
    confirmed=False, note="antena linear L1/L5 na borda de cima")
add("J302", "contato de mola, 2 vias", [
    (1, "FEED", "passive", R), (2, "GND", "passive", B),
], confirmed=False,
    note="onde a antena da parede da caixa encosta na placa. 04:271 decidiu "
         "contatos de mola e 04:661 registrava que a area deles nao estava "
         "dimensionada; sao dois pads de 2,0 x 2,0 mm a 3,0 mm de passo, "
         "sem pasta. CONFERIR a mola escolhida antes de fabricar")
passive("FB301", "600 R @100 MHz", "ferrite do 1V8 junto do receptor")
passive("C301", "2,2 pF", "paralelo da rede em pi, valor de partida")
passive("C302", "2,2 pF", "paralelo da rede em pi, valor de partida")
passive("L301", "3,9 nH", "serie da rede em pi, valor de partida")
passive("C303", "10 uF")
passive("C304", "100 nF")

# ---------------------------------------------------------------- folha 4
# The 10-way display FPC order is the one 03-netlist.md uses, contact by
# contact, and it is the same for the JDI and for the Sharp.
add("J401", "Hirose FH28-10S-0.5SH", [
    (1, "SCLK", "input", R), (2, "SI", "input", R), (3, "SCS", "input", R),
    (4, "EXTCOMIN", "input", R), (5, "DISP", "input", R), (6, "VDDA", "power_in", R),
    (7, "VDD", "power_in", R), (8, "EXTMODE", "input", R), (9, "VSS", "power_in", R),
    (10, "VSSA", "power_in", R),
], confirmed=True, note="FPC de sinal do painel, 10 vias; ordem de 03-netlist.md")

# LPM027M128C specification ver.02: the signal FPC in 1.4.1 (page 4) and the
# backlight FPC in 1.4.2 (page 5). The light is four white LEDs in parallel
# inside the panel: pin 5 is the common anode and pins 1 to 4 are one cathode
# each. No via is spare, which is what this project assumed for a whole day.
add("DS401", "JDI LPM027M128C", [
    (1, "SCLK", "input", L), (2, "SI", "input", L), (3, "SCS", "input", L),
    (4, "EXTCOMIN", "input", L), (5, "DISP", "input", L), (6, "VDDA", "power_in", L),
    (7, "VDD", "power_in", L), (8, "EXTMODE", "input", L), (9, "VSS", "power_in", B),
    (10, "VSSA", "power_in", B),
    ("BL1", "LUZ_K1", "passive", R), ("BL2", "LUZ_K2", "passive", R),
    ("BL3", "LUZ_K3", "passive", R), ("BL4", "LUZ_K4", "passive", R),
    ("BL5", "LUZ_ANODO", "passive", R),
], confirmed=True,
    note="painel MIP 2,7 pol com luz de 4 LED em paralelo; V_F 2,68 V tipico a "
         "4 mA por LED (16 mA no total), maximo absoluto de 30 mA no conjunto")

# The same outline drawing names the connector: "Backlight FPC 503480-0500,
# 5pin / molex". The 4-way 5034800440 of the Sharp plan B is the same family.
add("J402", "Molex 503480-0500", [
    (1, "1", "passive", L), (2, "2", "passive", L), (3, "3", "passive", L),
    (4, "4", "passive", L), (5, "5", "passive", L),
], confirmed=True, note="conector da luz do painel, 5 vias, passo 0,5 mm, "
                        "recomendado pela propria JDI no desenho de contorno")

add("Q401", "DMG1012T-7", [
    (1, "G", "input", L), (2, "S", "passive", B), (3, "D", "passive", T),
], confirmed=False,
    note="chave de canal N da luz. A Diodes NAO publica numero de pino: o "
         "diagrama so mostra a posicao, e a numeracao vem do padrao SOT-523")

passive("R401", "39 R", "R_BL: 16 mA a 2,67 V no trilho de 3,3 V")
passive("R402", "100 k", "pull-down da porta do Q401")
passive("R403", "100 k", "pull-down do DISP_PWR_EN")
passive("R404", "100 k", "pull-down do DISP_ON")
passive("R405", "100 k", "pull-down do DISP_CS")
passive("JP401", "0 R", "VDD do painel: 0 R fixo na posicao do 3V0")
passive("C404", "100 nF")

# ---------------------------------------------------------------- folha 5
add("U501", "MX25R6435F", [
    (1, "CS_N", "input", L), (2, "SO_IO1", "bidirectional", R),
    (3, "WP_N_IO2", "bidirectional", R), (4, "GND", "power_in", B),
    (5, "SI_IO0", "bidirectional", L), (6, "SCLK", "input", L),
    (7, "HOLD_N_IO3", "bidirectional", R), (8, "VCC", "power_in", T),
], confirmed=True, note="flash NOR SPI; numeros 1, 2, 5 e 6 vem de 03-netlist.md")

# BMP585: BST-BMP585-DS003-02 rev 1.2, section 6.1, table 26, page 45.
# LGA of 8 pins, 3.25 x 3.25 mm - not the 2.0 x 2.0 this project assumed.
add("U502", "Bosch BMP585", [
    (1, "SCX", "input", L), (2, "SDX", "bidirectional", L),
    (3, "SDO", "bidirectional", L), (4, "VDDIO", "power_in", T),
    (5, "INT", "output", R), (6, "VSSIO", "power_in", B),
    (7, "CSB", "input", L), (8, "VDD", "power_in", T),
], confirmed=True,
    note="barometro; 0x46 com SDO em 0 e 0x47 com SDO em 1, e a Bosch proibe "
         "SDO flutuando. CSB ao VDDIO para I2C. O pino 9 e so marcacao a laser")

# BMI270: BST-BMI270-DS000-08 rev 1.6, table 22, page 135.
add("U503", "Bosch BMI270", [
    (1, "SDO", "bidirectional", L), (2, "ASDX", "bidirectional", L),
    (3, "ASCX", "bidirectional", L), (4, "INT1", "bidirectional", R),
    (5, "VDDIO", "power_in", T), (6, "GNDIO", "power_in", B),
    (7, "GND", "power_in", B), (8, "VDD", "power_in", T),
    (9, "INT2", "bidirectional", R), (10, "OCSB", "input", R),
    (11, "OSDO", "output", R), (12, "CSB", "input", L),
    (13, "SCX", "input", L), (14, "SDX", "bidirectional", L),
], confirmed=True,
    note="IMU, 0x68 com o SDO no GND; CSB ao VDDIO para I2C; ASDX e ASCX ao "
         "VDDIO ou abertos, NUNCA ao GND")

# MMC5633NJL Rev A: WLP of four balls, 0.85 x 0.85 mm. There is no LGA version
# and no INT or address pin on this package.
add("U504", "MMC5633NJL", [
    ("A1", "VSA", "power_in", B), ("A2", "SCL", "input", L),
    ("B1", "VDD", "power_in", T), ("B2", "SDA", "bidirectional", L),
], confirmed=True,
    note="magnetometro, 0x30; VSA e o terra; NAO varrer o barramento "
         "(0x7E poe a peca em I3C ate faltar energia)")

# OPT3001: SBOS681C, section 5, page 3. One package only, DNP, USON-6.
add("U505", "TI OPT3001DNPR", [
    (1, "VDD", "power_in", T), (2, "ADDR", "input", L), (3, "GND", "power_in", B),
    (4, "SCL", "input", L), (5, "INT", "open_collector", R),
    (6, "SDA", "bidirectional", L),
], confirmed=True, note="luz ambiente, 0x44 com ADDR no GND")

passive("R501", "100 k", "pull-up do NOR_CS ao SD3V0")
passive("R502", "47 k", "pull-up do WP ao SD3V0")
passive("R503", "47 k", "pull-up do HOLD ao SD3V0")
passive("R504", "4,7 k", "pull-up do SENS_SDA")
passive("R505", "4,7 k", "pull-up do SENS_SCL")
passive("R506", "33 R", "serie do NOR_SCK, junto do MCU")
passive("R507", "33 R", "serie do NOR_MOSI, junto do MCU")
passive("R508", "33 R", "serie do NOR_MISO, junto da flash")
passive("C501", "22 uF")
passive("C502", "4,7 uF", "VDD do MMC5633NJL, minimo de 2,2 uF")
passive("C503", "100 nF")

# ---------------------------------------------------------------- folha 6
for n in ("SW601", "SW602", "SW603"):
    # four terminals on the part, two nodes on the board: 1 and 2 are joined
    # inside, 3 and 4 too, and the KiCad footprint gives each pair one pad.
    add(n, "Omron B3S-1002P", [
        (1, "1", "passive", L), (2, "2", "passive", R),
    ], confirmed=True,
        note="tecla tatil SPST-NO; 1 e 2 ligados entre si por dentro, 3 e 4 "
             "tambem, e fechar une os dois pares. O sufixo P e fita, nao "
             "terminal de terra: a versao com terra e a B3S-1102")

add("LS601", "CPT-1117-83-SMT", [(1, "A", "passive", L), (2, "B", "passive", R)],
    confirmed=False, note="buzzer piezo, acionado em contrafase")

add("D601", "APTF1616SEEZGKQBKC", [
    ("A", "ANODO", "passive", T), ("KR", "K_R", "passive", B),
    ("KG", "K_G", "passive", B), ("KB", "K_B", "passive", B),
], confirmed=False, note="LED RGB de ANODO COMUM: o resistor fica do lado do catodo")

for n in ("Q601", "Q602", "Q603"):
    add(n, "DMG1012T-7", [
        (1, "G", "input", L), (2, "S", "passive", B), (3, "D", "passive", T),
    ], confirmed=False,
        note="chave de canal N de uma cor do LED; a Diodes nao publica numero "
             "de pino, a numeracao vem do padrao SOT-523")

passive("R601", "1 k", "R_LEDR, do lado do catodo")
passive("R602", "1 k", "R_LEDG, do lado do catodo")
passive("R603", "1 k", "R_LEDB, do lado do catodo")
passive("R604", "100 R", "serie da tecla esquerda")
passive("R605", "100 R", "serie da tecla central")
passive("R606", "100 R", "serie da tecla direita")
passive("C601", "1 nF", "da tecla esquerda ao GND")
passive("C602", "1 nF", "da tecla central ao GND")
passive("C603", "1 nF", "da tecla direita ao GND")
passive("R607", "330 R", "serie do BUZ_A")
passive("R608", "330 R", "serie do BUZ_B")
passive("R609", "100 k", "pull-down da porta do Q601")
passive("R610", "100 k", "pull-down da porta do Q602")
passive("R611", "100 k", "pull-down da porta do Q603")

for n in ("JP102", "JP103", "JP104", "JP105", "JP106"):
    passive(n, "0 R", "jumper de corrente do bloco")


def by_sheet() -> dict[str, list[str]]:
    """Which parts belong to which sheet of 01-esquematico.md."""
    out: dict[str, list[str]] = {"1 Energia": [], "2 MCU": [], "3 GNSS": [],
                                 "4 Display": [], "5 Memoria e sensores": [],
                                 "6 Interface": []}
    for ref in PARTS:
        digits = "".join(c for c in ref if c.isdigit())
        n = int(digits[0]) if digits else 1
        key = {1: "1 Energia", 2: "2 MCU", 3: "3 GNSS", 4: "4 Display",
               5: "5 Memoria e sensores", 6: "6 Interface"}.get(n, "1 Energia")
        out[key].append(ref)
    for k in out:
        out[k].sort()
    return out


if __name__ == "__main__":
    print(f"{len(PARTS)} posicoes")
    for k, v in by_sheet().items():
        print(f"  folha {k}: {len(v)}")
    print(f"pinagem nao confirmada em {len(UNCONFIRMED)}: {', '.join(sorted(UNCONFIRMED))}")
