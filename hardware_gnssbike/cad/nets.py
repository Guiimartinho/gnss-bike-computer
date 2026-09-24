#!/usr/bin/env python3
"""Every net of the board, as (reference, pin name).

This is hardware_gnssbike/03-netlist.md turned into data. The pin NAME is used,
not the number, because that is what the documents state; parts.py turns the
name into the number the symbol carries.

Two rules were kept while writing this:

  - nothing that the documents do not state is wired here. Where a document
    leaves a connection open, the net is listed in ABERTO instead, and the
    schematic writes it as a note on the sheet rather than as a wire that
    would look decided.
  - where a document is internally inconsistent, the inconsistency is carried
    across with its name, not silently resolved. The central key is the
    clearest case: the devicetree wires it to P1.27 and four documents say it
    must go only to SHPHLD.
"""

from __future__ import annotations

# name -> [(ref, pin name), ...]
NETS: dict[str, list[tuple[str, str]]] = {}

# things a document explicitly leaves open: name -> why
ABERTO: dict[str, str] = {}


def net(name: str, *pins: tuple[str, str]) -> None:
    NETS[name] = list(pins)


# ------------------------------------------------------------ alimentacao
net("VBUS", ("J101", "VBUS_A4"), ("J101", "VBUS_A9"), ("J101", "VBUS_B4"),
    ("J101", "VBUS_B9"), ("U101", "VBUS"), ("D101", "IO"), ("C101", "1"))
net("VBUSOUT", ("U101", "VBUSOUT"), ("U201", "VBUS"), ("R104", "1"), ("C110", "1"))
net("CC1", ("J101", "CC1"), ("U101", "CC1"), ("D102", "D1P"))
net("CC2", ("J101", "CC2"), ("U101", "CC2"), ("D102", "D1N"))
net("USB_DP", ("J101", "DP_A6"), ("J101", "DP_B6"), ("U201", "USB_DP"), ("D102", "D2P"))
net("USB_DM", ("J101", "DM_A7"), ("J101", "DM_B7"), ("U201", "USB_DM"), ("D102", "D2N"))

# The cell reaches the gauge through the measuring jumper; the gauge's 7 mOhm
# sense sits INSIDE the chip, between BATT and SYS, so those are two nodes.
net("VBAT_CELULA", ("J102", "1"), ("JP101", "1"))
net("VBAT", ("JP101", "2"), ("U102", "BATT"), ("U102", "TH"))
net("VBAT_SYS", ("U102", "SYS"), ("U101", "VBAT"), ("U104", "IN"),
    ("U104", "EN"), ("C117", "1"), ("C102", "1"), ("C113", "1"))
ABERTO["VBAT / VBAT_SYS"] = ("de que lado do sensor interno do MAX17262 ficam o BATT e o "
                             "SYS nao esta fechado; trocar os dois inverte o sinal da corrente")

net("VSYS", ("U101", "VSYS"), ("U101", "LSIN2"), ("D103", "A"), ("D104", "A"),
    ("D601", "ANODO"), ("C103", "1"))
net("BUCK1_SW", ("U101", "SW1"), ("L101", "1"))
net("1V8", ("L101", "2"), ("U101", "VOUT1"), ("U302", "VCCB"), ("JP103", "1"),
    ("C104", "1"), ("C109", "1"))
net("1V8_BLOCO", ("JP103", "2"), ("FB301", "1"))
net("1V8_GNSS", ("FB301", "2"), ("U301", "VCC"), ("U301", "V_IO"), ("C303", "1"),
    ("C304", "1"))
net("BUCK2_SW", ("U101", "SW2"), ("L102", "1"))
net("3V0", ("L102", "2"), ("U101", "VOUT2"), ("U101", "LSIN1"),
    ("U302", "VCCA"), ("U302", "OE"), ("U103", "I2C_VDD"), ("U103", "SDA"),
    ("C105", "1"), ("C106", "1"), ("C107", "1"), ("C108", "1"),
    ("JP102", "1"), ("JP104", "1"), ("JP105", "1"),
    ("R107", "1"), ("R108", "1"), ("R109", "1"), ("R110", "1"), ("R111", "1"),
    ("R112", "1"), ("J201", "VTref"))
net("3V0_MOD", ("JP102", "2"), ("U201", "VDD"), ("C201", "1"), ("C210", "1"))
net("3V0_SENS", ("JP105", "2"), ("U502", "VDD"), ("U502", "VDDIO"), ("U502", "CSB"),
    ("U502", "SDO"), ("U503", "VDD"), ("U503", "VDDIO"), ("U503", "ASDX"), ("U503", "ASCX"),
    ("U503", "CSB"), ("U504", "VDD"), ("U505", "VDD"),
    ("R504", "1"), ("R505", "1"), ("C502", "1"), ("C503", "1"))
net("SD3V0", ("U101", "LSOUT1"), ("JP106", "1"), ("R501", "1"), ("R502", "1"),
    ("R503", "1"), ("C501", "1"))
net("SD3V0_FLASH", ("JP106", "2"), ("U501", "VCC"))
net("3V3BL", ("U101", "LSOUT2"), ("R401", "1"), ("C111", "1"))
net("VBCKP", ("U104", "OUT"), ("U301", "V_BCKP"), ("C114", "1"))
net("VINT", ("U103", "VINT"), ("U103", "VINT2"), ("C116", "1"), ("U103", "R_MPP0"), ("U103", "R_MPP1"), ("U103", "R_MPP2"),
    ("U103", "T_MPP0"), ("U103", "T_MPP1"), ("U103", "STO_CFG0"), ("U103", "STO_CFG2"),
    ("U103", "KEEP_ALIVE"))
net("SRC", ("U103", "SRC"), ("C115", "1"),
    *[(f"PV10{i}", "P") for i in range(1, 7)])
net("SW_DCDC", ("U103", "SW_DCDC"), ("L103", "1"))
net("SW_DCDC_L", ("L103", "2"), ("U103", "STO"))
net("TH_MON", ("U103", "TH_MON"), ("RT101", "1"), ("R106", "2"))
net("TH_REF", ("U103", "TH_REF"), ("R106", "1"))
net("DIS_STO_CH", ("U103", "DIS_STO_CH"), ("R104", "2"), ("R105", "1"))
ABERTO["DIS_STO_CH"] = ("qual dos dois resistores fica em serie nao esta em fonte "
                        "nenhuma; 100 k em serie da 5,0 V no pino e 1 M da 0,5 V")
net("NTC_BAT", ("U101", "NTC"), ("J102", "4"))
ABERTO["RDIV do AEM10900"] = ("o documento diz \"com TH_REF e RDIV de 22 k\" sem dizer entre que pinos o R106 fica; aqui ele vai de TH_REF a TH_MON e o pino RDIV segue sem no")
ABERTO["C107, C108, C109, C112"] = ("a lista de materiais nao diz em que no cada um destes entra: C101 a C109 sao \"entradas e saidas do nPM1300\" em bloco")
ABERTO["NTC_BAT"] = "qual via do J102 leva o NTC do pack vem do fabricante do pack"

net("RVSET1", ("U101", "VSET1"), ("R102", "1"))
net("RVSET2", ("U101", "VSET2"), ("R103", "1"))
net("REG_GAUGE", ("U102", "REG"), ("C118", "1"))
net("LED_CHG", ("U101", "LED1"), ("D103", "K"))
net("LED_ERR", ("U101", "LED0"), ("D104", "K"))
net("ALRT", ("U102", "ALRT"), ("R110", "2"))
net("IRQ_AEM", ("U103", "IRQ"), ("R111", "2"))

# ------------------------------------------------------------ MCU e I2C
net("PWR_SDA", ("U201", "P0.02"), ("U101", "SDA"), ("U102", "SDA"), ("U103", "SDA"),
    ("R107", "2"))
net("PWR_SCL", ("U201", "P0.03"), ("U101", "SCL"), ("U102", "SCL"), ("U103", "SCL"),
    ("R108", "2"))
net("PMIC_INT", ("U201", "P0.00"), ("U101", "GPIO3"), ("R109", "2"))
net("SENS_SDA", ("U201", "P1.29"), ("U502", "SDX"), ("U503", "SDX"),
    ("U504", "SDA"), ("U505", "SDA"), ("R504", "2"))
net("SENS_SCL", ("U201", "P1.03"), ("U502", "SCX"), ("U503", "SCX"),
    ("U504", "SCL"), ("U505", "SCL"), ("R505", "2"))
net("IMU_INT", ("U201", "P1.10"), ("U503", "INT1"))
net("BARO_INT", ("U201", "P1.12"), ("U502", "INT"))
net("INT_OPT", ("U505", "INT"), ("R112", "2"))

net("SWDIO", ("U201", "SWDIO"), ("J201", "SWDIO"))
net("SWDCLK", ("U201", "SWDCLK"), ("J201", "SWDCLK"))
net("MOD_RESET", ("U201", "RESET"), ("J201", "RESET"))
net("CON_TX", ("U201", "P1.00"), ("TP201", "1"))
net("CON_RX", ("U201", "P1.31"), ("TP202", "1"))

# ------------------------------------------------------------ armazenamento
net("NOR_SCK", ("U201", "P2.01"), ("R506", "1"))
net("NOR_SCK_F", ("R506", "2"), ("U501", "SCLK"))
net("NOR_MOSI", ("U201", "P2.02"), ("R507", "1"))
net("NOR_MOSI_F", ("R507", "2"), ("U501", "SI_IO0"))
net("NOR_MISO", ("U201", "P2.04"), ("R508", "1"))
net("NOR_MISO_F", ("R508", "2"), ("U501", "SO_IO1"))
net("NOR_CS", ("U201", "P2.05"), ("U501", "CS_N"), ("R501", "2"))
net("NOR_WP", ("U501", "WP_N_IO2"), ("R502", "2"))
net("NOR_HOLD", ("U501", "HOLD_N_IO3"), ("R503", "2"))

# ------------------------------------------------------------ display
net("DISP_SCK", ("U201", "P3.03"), ("J401", "SCLK"))
net("DISP_MOSI", ("U201", "P3.00"), ("J401", "SI"))
net("DISP_CS", ("U201", "P3.02"), ("J401", "SCS"), ("R405", "1"))
net("DISP_EXTCOMIN", ("U201", "P3.06"), ("J401", "EXTCOMIN"))
net("DISP_ON", ("U201", "P3.05"), ("J401", "DISP"), ("R404", "1"))
net("DISP_PWR_EN", ("U201", "P3.07"), ("R403", "1"))
net("DISP_VDD", ("JP401", "2"), ("J401", "VDD"), ("J401", "VDDA"), ("J401", "EXTMODE"),
    ("C404", "1"))
net("DISP_VDD_SEL", ("JP401", "1"), ("JP104", "2"))
net("BL_PWM", ("U201", "P3.08"), ("Q401", "G"), ("R402", "1"))
net("BL_ANODO", ("R401", "2"), ("J402", "5"), ("DS401", "LUZ_ANODO"))
net("BL_CATODO", ("J402", "1"), ("J402", "2"), ("J402", "3"), ("J402", "4"),
    ("Q401", "D"), ("DS401", "LUZ_K1"), ("DS401", "LUZ_K2"),
    ("DS401", "LUZ_K3"), ("DS401", "LUZ_K4"))
ABERTO["J402"] = ("a ordem das cinco vias da luz nao esta em fonte nenhuma: as vias 1 e 2 "
                  "aqui sao posicao, nao pinagem, e a peca tambem esta em aberto")

# the panel itself, wired to the signal connector contact by contact
for _n in ("SCLK", "SI", "SCS", "EXTCOMIN", "DISP", "VDDA", "VDD", "EXTMODE",
           "VSS", "VSSA"):
    NETS.setdefault(f"FPC_{_n}", []).extend([("J401", _n), ("DS401", _n)])

# ------------------------------------------------------------ GNSS
net("GNSS_TX", ("U201", "P1.04"), ("U302", "A1"))
net("GNSS_TX_1V8", ("U302", "B1Y"), ("U301", "RXD"))
net("GNSS_EXTINT", ("U201", "P1.08"), ("U302", "A2"))
net("GNSS_EXTINT_1V8", ("U302", "B2Y"), ("U301", "EXTINT"))
net("GNSS_RX", ("U201", "P1.05"), ("U302", "A3Y"))
net("GNSS_RX_1V8", ("U302", "B3"), ("U301", "TXD"))
net("GNSS_TIMEPULSE", ("U201", "P1.09"), ("U302", "A4Y"))
net("GNSS_TIMEPULSE_1V8", ("U302", "B4"), ("U301", "TIMEPULSE"))
net("GNSS_RESET_N", ("U201", "P1.06"), ("U301", "RESET_N"))
net("RF_IN", ("U301", "RF_IN"), ("L301", "2"), ("C302", "1"))
net("RF_ANT", ("E301", "FEED"), ("L301", "1"), ("C301", "1"))

# ------------------------------------------------------------ interface
net("KEY_L", ("U201", "P1.26"), ("R604", "1"))
net("KEY_L_SW", ("R604", "2"), ("SW601", "1"), ("C601", "1"))
net("KEY_C", ("U201", "P1.27"), ("R605", "1"))
net("KEY_C_SW", ("R605", "2"), ("SW602", "1"), ("C602", "1"),
    ("U101", "SHPHLD"))
ABERTO["KEY_C"] = ("a tecla central esta ligada em dois lugares: o devicetree a poe em "
                   "P1.27 e quatro documentos dizem que ela vai SO ao SHPHLD do nPM1300")
net("KEY_R", ("U201", "P1.30"), ("R606", "1"))
net("KEY_R_SW", ("R606", "2"), ("SW603", "1"), ("C603", "1"))

net("RGB_R", ("U201", "P1.16"), ("Q601", "G"), ("R609", "1"))
net("RGB_G", ("U201", "P1.19"), ("Q602", "G"), ("R610", "1"))
net("RGB_B", ("U201", "P1.22"), ("Q603", "G"), ("R611", "1"))
net("RGB_R_D", ("D601", "K_R"), ("R601", "1"))
net("RGB_G_D", ("D601", "K_G"), ("R602", "1"))
net("RGB_B_D", ("D601", "K_B"), ("R603", "1"))
net("RGB_R_Q", ("R601", "2"), ("Q601", "D"))
net("RGB_G_Q", ("R602", "2"), ("Q602", "D"))
net("RGB_B_Q", ("R603", "2"), ("Q603", "D"))

net("BUZ_A", ("U201", "P1.25"), ("R607", "1"))
net("BUZ_A_LS", ("R607", "2"), ("LS601", "A"))
net("BUZ_B", ("U201", "P1.28"), ("R608", "1"))
net("BUZ_B_LS", ("R608", "2"), ("LS601", "B"))

# ------------------------------------------------------------ terra
net("GND",
    ("J101", "GND_A1"), ("J101", "GND_A12"), ("J101", "GND_B1"), ("J101", "GND_B12"),
    ("D101", "GND"), ("D102", "GND1"), ("D102", "GND2"),
    ("U101", "AVSS"), ("U101", "PVSS1"), ("U101", "PVSS2"), ("U102", "GND"), ("U103", "GND1"), ("U103", "GND2"), ("U103", "GND3"), ("U103", "GND_PAD"), ("U103", "STO_CFG1"),
    ("U104", "GND"), ("U104", "PAD"), ("J102", "6"), ("RT101", "2"),
    ("U201", "GND"), ("U201", "GND3"), ("U201", "GND10"), ("U201", "GND11"),
    ("U201", "GND20"), ("U201", "GND_D0"), ("U201", "GND_E0"),
    ("U201", "GND_F0"), ("J201", "GND"), ("TP203", "1"),
    ("U301", "GND"), ("U301", "GND2"), ("U301", "GND3"), ("U301", "VIO_SEL"), ("U302", "GND"), ("U302", "PAD"), ("E301", "GND"),
    ("C301", "2"), ("C302", "2"), ("C303", "2"), ("C304", "2"),
    ("U501", "GND"), ("U502", "VSSIO"), ("U503", "GND"), ("U503", "GNDIO"),
    ("U503", "SDO"), ("U504", "VSA"), ("U505", "GND"), ("U505", "ADDR"),
    ("DS401", "VSS"), ("DS401", "VSSA"),
    ("Q401", "S"), ("Q601", "S"), ("Q602", "S"), ("Q603", "S"),
    ("R402", "2"), ("R403", "2"), ("R404", "2"), ("R405", "2"),
    ("R609", "2"), ("R610", "2"), ("R611", "2"),
    ("R105", "2"), ("C601", "2"), ("C602", "2"), ("C603", "2"),
    ("SW601", "2"), ("SW602", "2"), ("SW603", "2"),
    *[(f"PV10{i}", "N") for i in range(1, 7)],
    *[(c, "2") for c in ("C101", "C102", "C103", "C104", "C105", "C106", "C110",
                         "C111", "C114", "C115", "C116", "C117", "C118",
                         "C201", "C210", "C404", "C501", "C502", "C503")],
    ("R102", "2"), ("R103", "2"))

# ------------------------------------------------------------ nao ligados
# Pins that the documents leave with no destination, and why. Writing them
# here keeps them off the drawing as loose ends and out of the netlist as
# silent errors.
SEM_LIGACAO: dict[str, str] = {
    "U101 LDSW2_IN": "entrada da LDSW2, ja no VSYS",
    "D102 NC": "pino sem funcao na matriz de ESD",
    "U503 P10 / P11": "a Bosch manda deixar abertos",
    "J201 NC3": "o padrao TC2030 nao usa o pino 3",
}

# The nPM1300's VDDIO (pin 12) is the supply of its TWI and of its GPIOs. It
# is a real pin that this project had never wired, and 08-layout.md carries
# "domain of the nPM1300's digital I/O" as an open question. It goes to 3V0
# so that the bus matches the MCU's 3.0 V.
NETS["3V0"].append(("U101", "VDDIO"))
# PVDD (pin 4) is the power input of both bucks.
NETS["VSYS"].append(("U101", "PVDD"))

ABERTO["ST_STO do AEM10900"] = (
    "03-netlist.md leva um no ST_STO do AEM10900 ao ponto de teste TP111, "
    "mas a ficha DS-AEM10900-v1.6.0 nao tem esse pino: e de outro CI da "
    "familia. Nesta peca o estado sai pelo IRQ")
ABERTO["I2C_ADDR do AEM10900"] = (
    "03-netlist.md manda amarrar um pino I2C_ADDR ao I2C_VDD para o endereco "
    "0x41, mas a ficha DS-AEM10900-v1.6.0 lista os 28 pinos e NENHUM se chama "
    "assim: nao ha onde ligar, e de onde sai o endereco do CI precisa ser "
    "confirmado na ficha antes do layout")
ABERTO["VDDIO do nPM1300"] = (
    "ligado ao 3V0, que e a saida do BUCK2 do proprio CI: ate o BUCK2 partir, "
    "o TWI e os GPIO do PMIC ficam sem alimentacao. E o arranjo da referencia "
    "da Nordic, mas a consequencia na partida nao foi verificada")

def resumo() -> str:
    pins = sum(len(v) for v in NETS.values())
    return (f"{len(NETS)} nos, {pins} ligacoes de pino, "
            f"{len(ABERTO)} pontos em aberto")


if __name__ == "__main__":
    print(resumo())
    for k, v in ABERTO.items():
        print(f"  em aberto: {k}: {v}")
