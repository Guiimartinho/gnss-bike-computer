#!/usr/bin/env python3
"""A footprint for every part, and the ones KiCad does not have, generated.

Each entry says where the land pattern comes from, because that is what
decides whether the board can be assembled:

  EXATO      the KiCad library footprint is for this very part number.
  ENCAPSULAMENTO  the library footprint is for the same package - same pad
             count, same pitch, same body - but drawn for another part. It
             fits, and it still has to be checked against the datasheet
             before fabrication.
  GERADO     no library footprint exists, so it is generated here from the
             package dimensions. Where the datasheet's recommended land
             pattern was not in hand, the pads follow the package outline
             with the usual allowances, and the entry says so.

Nothing here has been fabricated or measured.
"""

from __future__ import annotations

import math

# ref -> (footprint, origin, note)
# origin is EXATO, ENCAPSULAMENTO or GERADO
FP: dict[str, tuple[str, str, str]] = {}

# Parts that are not on the board: they live in the case and reach the board
# through spring contacts or a flat cable that no document dimensions yet.
FORA_DA_PLACA = {"DS401", "PV101", "PV102", "PV103", "PV104", "PV105",
                 "PV106"}


def _fp(refs, name, origem, nota=""):
    for r in (refs if isinstance(refs, (list, tuple)) else [refs]):
        FP[r] = (name, origem, nota)


# ---------------------------------------------------------------- passivos
R0402 = "Resistor_SMD:R_0402_1005Metric"
C0402 = "Capacitor_SMD:C_0402_1005Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"

_fp(["R102", "R103", "R104", "R105", "R106", "R107", "R108", "R109", "R110",
     "R111", "R112", "R401", "R402", "R403", "R404", "R405", "R501", "R502",
     "R503", "R504", "R505", "R506", "R507", "R508", "R601", "R602", "R603",
     "R604", "R605", "R606", "R607", "R608", "R609", "R610", "R611",
     "R113", "R114", "R115", "C305", "C306",
     "R116", "R117", "R118", "R119", "R120", "R121", "R122", "R123",
     "C119", "C120", "C121",
     "JP102", "JP103", "JP104", "JP105", "JP106", "JP401"], R0402,
    "ENCAPSULAMENTO", "0402; a lista de compras usa a serie Panasonic ERJ-2")
_fp("JP101", "Resistor_SMD:R_1206_3216Metric", "ENCAPSULAMENTO",
    "0 ohm 1206, >= 2 A: o 0402 da lista nao serve")
_fp("RT101", R0402, "ENCAPSULAMENTO", "NTC 10 k B3380 em 0402")

_fp(["C110", "C111", "C113", "C114", "C115", "C116", "C118", "C201", "C304",
     "C404", "C503"], C0402, "ENCAPSULAMENTO", "")
_fp(["C101", "C102", "C103", "C104", "C105", "C106", "C107", "C108", "C109",
     "C112", "C117", "C210", "C303", "C501", "C502",
     "C601", "C602", "C603"], C0603, "ENCAPSULAMENTO", "")

_fp(["L101", "L102"], "Inductor_SMD:L_0805_2012Metric", "ENCAPSULAMENTO",
    "Murata DFE201610E e 2,0 x 1,6 mm; o 0805 da KiCad e 2,0 x 1,2 mm")
_fp("L103", "Inductor_SMD:L_1008_2520Metric", "ENCAPSULAMENTO",
    "TDK VLS252012HBX, 2,5 x 2,0 mm")
_fp("L301", "Inductor_SMD:L_0402_1005Metric", "ENCAPSULAMENTO", "")
_fp(["C301", "C302"], C0402, "ENCAPSULAMENTO", "")
_fp("FB301", "Inductor_SMD:L_0402_1005Metric", "ENCAPSULAMENTO",
    "ferrite Murata BLM15PX601SN1D")

# ---------------------------------------------------------------- CIs
_fp("U101", "Package_DFN_QFN:QFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm",
    "ENCAPSULAMENTO", "nPM1300 QFN32 5x5 P0,5; pad termico e o pino 33")
_fp("U103", "Package_DFN_QFN:QFN-28-1EP_4x4mm_P0.4mm_EP2.3x2.3mm",
    "ENCAPSULAMENTO", "AEM10900 QFN28 4x4 P0,4; pad termico e o pino 29")
_fp("U104", "Package_SON:Texas_X2SON-4_1x1mm_P0.65mm", "EXATO",
    "TPS7A02 em DQN; o pad termico e o pino 5")
_fp("U501", "Package_SO:SOIC-8_5.275x5.275mm_P1.27mm", "ENCAPSULAMENTO",
    "MX25R6435F em SOP-8 de 200 mil")
_fp("U503", "Package_LGA:Bosch_LGA-14_3x2.5mm_P0.5mm", "EXATO",
    "BMI270, LGA-14 da Bosch")
_fp(["Q401", "Q601", "Q602", "Q603"], "Package_TO_SOT_SMD:SOT-523",
    "ENCAPSULAMENTO", "DMG1012T-7; a Diodes nao numera os terminais")

# ---------------------------------------------------------------- conectores
_fp("J101", "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12", "EXATO",
    "USB-C de 16 contatos do HRO TYPE-C-31-M-12, que e a peca da lista: o "
    "footprint do KiCad tem o nome da peca. Os nomes de pad batem com o "
    "padrao USB-IF e o S1 da blindagem agora tem pino no esquematico")
_fp("J102", "Connector_JST:JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal",
    "EXATO", "JST GH de 6 vias, entrada lateral")
_fp("J201", "Connector:Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical", "EXATO",
    "TC2030-NL, so furos e pads")
_fp("J401", "Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal",
    "ENCAPSULAMENTO",
    "10 vias, passo 0,5 mm, CONTATO INFERIOR: a cauda do display entra com "
    "as trilhas viradas para a placa. O footprint e da peca: FH12-10S-"
    "0.5SH(55), LCSC C506791, que tem modelo STEP na biblioteca do KiCad")
_fp("J402", "Connector_FFC-FPC:TE_0-1734839-5_1x05-1MP_P0.5mm_Horizontal",
    "ENCAPSULAMENTO",
    "5 vias, passo 0,5 mm. A peca e o Molex 503480-0500, que a JDI nomeia no "
    "desenho de contorno; a KiCad nao tem essa serie")
import parts as _P  # noqa: E402

_fp([t[0] for t in _P.TESTE] + ["TP201", "TP202", "TP203"],
    "TestPoint:TestPoint_Pad_D1.0mm", "EXATO", "")
_fp("JP301", "gnssbike:JumperRF_3x0402", "GERADO",
    "escolhe entre a antena de chip soldada e o u.FL")
_fp("J302", "Connector_Coaxial:U.FL_Hirose_U.FL-R-SMT-1_Vertical", "EXATO",
    "conector da antena GNSS: 50 ohm coaxial, no lugar do contato de mola")
_fp("J103", "Connector_JST:JST_ZH_S4B-ZR-SM4A-TF_1x04-1MP_P1.50mm_Horizontal",
    "EXATO",
    "entrada dos painéis: ZH de 1,5 mm, 4 vias - tres grupos de dois "
    "modulos e um terra comum. "
    "Passo diferente do GH de 1,25 da bateria de proposito, para os dois "
    "chicotes nao trocarem de lugar")
_fp("U103", "Package_DFN_QFN:QFN-24-1EP_4x4mm_P0.5mm_EP2.15x2.15mm",
    "ENCAPSULAMENTO",
    "ADP5091 em LFCSP-24 de 4 x 4 mm, passo 0,5. A ADI NAO publica land "
    "pattern na ficha - so o desenho do encapsulamento -, entao este e o "
    "padrao IPC do KiCad para o mesmo encapsulamento. O pad termico de "
    "2,15 e conservador contra o pad exposto de 2,30 nominal da peca "
    "(2,16 a 2,44), o que deixa 0,075 mm de recuo de cada lado e afasta "
    "o risco de ponte com os pads de sinal. A ficha exige K >= 0,20 mm "
    "entre o terminal e o pad exposto, e este footprint da mais que isso")
_fp("E301", "gnssbike:Antena_Unictron_H2UJ4U1H2Q0100_5x3mm", "GERADO",
    "antena de chip L1+L5 soldada na borda; land pattern do guia de "
    "layout da ficha rev. E")
_fp("D105", "Diode_SMD:D_SOD-523", "ENCAPSULAMENTO",
    "grampo do SRC; a peca ainda nao foi escolhida, o SOD-523 e so o "
    "encapsulamento mais provavel de um TVS pequeno de fuga baixa")

# ---------------------------------------------------------------- interface
_fp(["SW601", "SW602", "SW603"], "gnssbike:SW_TS-1088R_3.9x3mm", "GERADO",
    "XunPu TS-1088R-02026, 2 terminais. Substitui a Omron B3S-1002P de "
    "6,6 x 6 mm: 157.033 pecas contra 10.161, um quarto do preco, e libera "
    "metade da area das tres teclas na borda. A forca sobe de 2,3 para "
    "2,6 N, que e melhor com luva, mas o embolo sobe so 0,50 mm acima da "
    "tampa contra os 0,7 da Omron - a mecanica do embolo da caixa muda")
_fp(["D103", "D104"], "LED_SMD:LED_0603_1608Metric", "ENCAPSULAMENTO",
    "Kingbright APT1608SURCK, 1,6 x 0,8 mm")
_fp("LS601", "gnssbike:Buzzer_PKLCS1212E4001_12x12mm", "GERADO",
    "Murata PKLCS1212E4001-R1. O CPT-1117-83-SMT que estava aqui tem UMA "
    "peca em estoque; este tem 12.328 e mantem os 84 dB, que e o que "
    "justifica ter buzzer numa bicicleta - os substitutos de 9 x 9 caem "
    "para 65 a 70 dB. Custa 144 mm2 na face de tras e 3,0 mm de altura")


# ------------------------------------------------------- footprints gerados
def _pad(num, x, y, w, h, tipo="smd", forma="roundrect", drill=0.0,
         camadas='"F.Cu" "F.Paste" "F.Mask"'):
    extra = f'\n\t\t(drill {drill})' if drill else ""
    rr = '\n\t\t(roundrect_rratio 0.25)' if forma == "roundrect" else ""
    return (f'\t(pad "{num}" {tipo} {forma}\n\t\t(at {x:.4f} {y:.4f})\n'
            f'\t\t(size {w:.4f} {h:.4f}){extra}\n\t\t(layers {camadas}){rr}\n'
            f'\t\t(uuid "{_uid(num, x, y)}")\n\t)')


def _uid(*p):
    import hashlib
    h = hashlib.sha256("|".join(str(x) for x in p).encode()).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-8{h[17:20]}-{h[20:32]}"


# Body height of each generated footprint, in mm, from its datasheet. It is
# what turns the board's 3D view from a bare set of pads into something you
# can look at, and it is also the number the case has to clear.
# Every body drawn by this file, generated footprint or library one, so the
# project's own renderer can draw the same box the KiCad viewer shows. KiCad
# exports GLB from STEP only, so a model written as VRML never reaches the
# GLB and has to be drawn again on this side.
CORPO_TODOS: dict[str, tuple[float, float, float]] = {}

CORPO: dict[str, tuple[float, float, float]] = {
    # nome do footprint -> largura, altura em planta, altura do corpo (mm)
    "gnssbike:MinewSemi_ME54BS13_16.5x12mm": (12.00, 16.50, 2.40),
    # altura: a ficha da 2,5 tipico e 2,7 MAXIMO (cota C da tabela 21).
    # Para folga de tampa vale o maximo, nao o tipico.
    "gnssbike:u-blox_MAX_LCC-18_9.7x10.1mm": (9.70, 10.10, 2.70),
    # 1,448 x 1,468 x 0,64, do desenho 21-100168 Rev A. O nome do
    # footprint ainda diz 1.4x1.4: e o nome, nao a cota.
    "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm": (1.448, 1.468, 0.64),
    "gnssbike:BMP585_LGA-8_3.25x3.25mm": (3.25, 3.25, 1.96),
    # 0,82 de corpo, 0,40 de dado mais 0,14 de bola = 0,54 de altura
    # total. O "0,8 x 0,8 x 0,4" dos catalogos e arredondamento.
    "gnssbike:MMC5603_WLP-4_0.82x0.82mm": (0.82, 0.82, 0.54),
    # XunPu TS-1088R-02026: 3,90 x 3,00 x 2,00 do desenho rev A
    "gnssbike:SW_TS-1088R_3.9x3mm": (3.90, 3.00, 2.00),
    # Unictron H2UJ4U1H2Q0100: 5,0 x 3,0 x 0,5, +-0,15 nos tres
    "gnssbike:Antena_Unictron_H2UJ4U1H2Q0100_5x3mm": (5.00, 3.00, 0.50),
    # Murata PKLCS1212E4001-R1: 12,0 x 12,0 x 3,0 max (JGB40-1584B)
    "gnssbike:Buzzer_PKLCS1212E4001_12x12mm": (12.00, 12.00, 3.00),
    # TUOZHAN S4-3528RGBTA-A: 3,5 x 2,8 x 1,9
    "gnssbike:LED_RGB_3528_3.5x2.8mm": (3.50, 2.80, 1.90),
    "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm": (2.00, 2.00, 0.65),
    "gnssbike:ESD761_X1SON-2_1x0.6mm": (1.00, 0.60, 0.45),
    "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm": (1.00, 2.50, 0.55),
    "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm": (3.00, 2.50, 0.80),
    # the spring contacts: the two 2.0 x 2.0 pads at 3.0 mm of pitch that
    # contato_mola() draws, so 5.0 mm across the pair. The 1.5 mm of leaf is
    # ALTURA's, and ALTURA says there where it does NOT come from.
    "gnssbike:ContatoMola_2x2mm_P3mm": (5.00, 2.00, 1.50),
    # Same Sky CPT-1117-83-SMT: 11,0 x 9,0 x 1,7, do desenho da pagina 2
}


# KiCad reads a VRML model in units of a tenth of an inch, not in
# millimetres, and multiplies by this to place it. Checked, not assumed:
# R_0402_1005Metric.wrl, a part 1.0 x 0.5 mm, has its corners at +-0.197 and
# +-0.098, which is 1.0/2.54 and 0.5/2.54. Written in millimetres with a
# scale of 1, every one of these boxes came out 2.54 times too big in the 3D
# viewer - a 16.5 mm module drawn 41.9 mm long, over most of the board.
VRML_POR_MM = 1.0 / 25.4 * 10.0


def wrl_caixa(caminho, w: float, h: float, alt: float, cor=(0.13, 0.13, 0.14)):
    """A plain box in VRML, so KiCad's 3D viewer has a body to show.

    KiCad's GLB and STEP exports read only STEP models, so this box shows in
    the 3D viewer and not in an exported GLB; the project's own renderer draws
    the same box from the footprint, which is why both exist.
    """
    e = VRML_POR_MM
    hw, hh, alt = w / 2.0 * e, h / 2.0 * e, alt * e
    pts = [(-hw, -hh, 0), (hw, -hh, 0), (hw, hh, 0), (-hw, hh, 0),
           (-hw, -hh, alt), (hw, -hh, alt), (hw, hh, alt), (-hw, hh, alt)]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    caminho.write_text(
        "#VRML V2.0 utf8\n# caixa do encapsulamento, nao o modelo do fabricante\n"
        "Shape {\n  appearance Appearance { material Material { diffuseColor "
        f"{cor[0]} {cor[1]} {cor[2]} }} }}\n"
        "  geometry IndexedFaceSet {\n    coord Coordinate { point [\n"
        + ",\n".join(f"      {x:.4f} {y:.4f} {z:.4f}" for x, y, z in pts)
        + " ] }\n    coordIndex [\n"
        + ",\n".join("      " + " ".join(str(i) for i in f) + " -1" for f in faces)
        + " ]\n  }\n}\n", encoding="utf-8", newline="\n")


def _corpo(nome, w, h, pads, descr):
    """Wrap pads in a footprint with a courtyard and a fab outline."""
    hw, hh = w / 2.0, h / 2.0
    linhas = []
    for lay, larg, folga in (("F.CrtYd", 0.05, 0.25), ("F.Fab", 0.1, 0.0)):
        a, b = hw + folga, hh + folga
        linhas.append(
            f'\t(fp_rect\n\t\t(start {-a:.4f} {-b:.4f})\n\t\t(end {a:.4f} {b:.4f})\n'
            f'\t\t(stroke (width {larg}) (type solid))\n\t\t(fill none)\n'
            f'\t\t(layer "{lay}")\n\t\t(uuid "{_uid(nome, lay)}")\n\t)')
    return ('(footprint "' + nome + '"\n\t(version 20240108)\n\t(generator "gnssbike")\n'
            '\t(generator_version "8.0")\n\t(layer "F.Cu")\n'
            f'\t(descr "{descr}")\n\t(attr smd)\n'
            f'\t(property "Reference" "REF**"\n\t\t(at 0 {-hh - 1.2:.3f} 0)\n'
            '\t\t(layer "F.SilkS")\n\t\t(uuid "' + _uid(nome, "ref") + '")\n'
            '\t\t(effects (font (size 0.8 0.8) (thickness 0.12)))\n\t)\n'
            f'\t(property "Value" "{nome}"\n\t\t(at 0 {hh + 1.2:.3f} 0)\n'
            '\t\t(layer "F.Fab")\n\t\t(uuid "' + _uid(nome, "val") + '")\n'
            '\t\t(effects (font (size 0.8 0.8) (thickness 0.12)))\n\t)\n'
            + "\n".join(linhas) + "\n" + "\n".join(pads) + "\n)\n")


def son(nome, n, pitch, pad_w, pad_h, span, corpo_w, corpo_h, descr,
        nomes=None, ep=None):
    """Two rows of pads, numbered counter-clockwise from the top left."""
    pads = []
    por_lado = n // 2
    y0 = -(por_lado - 1) * pitch / 2.0
    for i in range(por_lado):
        num = nomes[i] if nomes else str(i + 1)
        pads.append(_pad(num, -span / 2.0, y0 + i * pitch, pad_w, pad_h))
    for i in range(por_lado):
        num = nomes[por_lado + i] if nomes else str(n - i)
        pads.append(_pad(num, span / 2.0, y0 + i * pitch, pad_w, pad_h))
    if ep:
        pads.append(_pad(ep[0], 0, 0, ep[1], ep[2], forma="rect"))
    return _corpo(nome, corpo_w, corpo_h, pads, descr)


def wlp(nome, linhas, colunas, pitch, bola, corpo_w, corpo_h, descr):
    """A ball grid, named A1..: letter is the row, number the column."""
    pads = []
    for r in range(linhas):
        for c in range(colunas):
            x = (c - (colunas - 1) / 2.0) * pitch
            y = (r - (linhas - 1) / 2.0) * pitch
            pads.append(_pad(f"{chr(ord('A') + r)}{c + 1}", x, y, bola, bola,
                             forma="circle"))
    return _corpo(nome, corpo_w, corpo_h, pads, descr)


def duas_bordas(nome, n_por_lado, pitch, pad_w, pad_h, span, corpo_w, corpo_h,
                descr, primeiro_lado="R", pad_h_canto=None):
    """A module: pads down one edge and up the other, 1..n.

    `pad_h_canto` narrows the first and the last pad of each row. u-blox asks
    for that on the MAX form factor - K = 0,8 for a normal pad and L = 0,7 for
    a corner one - and the reason is the corner castellation, which is wider
    than the others and would bridge to its neighbour on a full-width pad.
    """
    pads = []
    y0 = -(n_por_lado - 1) * pitch / 2.0
    for i in range(n_por_lado):
        h = pad_h_canto if (pad_h_canto and i in (0, n_por_lado - 1)) else pad_h
        pads.append(_pad(str(i + 1), span / 2.0, -y0 - i * pitch, pad_w, h))
    for i in range(n_por_lado):
        h = pad_h_canto if (pad_h_canto and i in (0, n_por_lado - 1)) else pad_h
        pads.append(_pad(str(n_por_lado + i + 1), -span / 2.0, y0 + i * pitch,
                         pad_w, h))
    return _corpo(nome, corpo_w, corpo_h, pads, descr)


def tecla_ts1088(nome):
    """XunPu TS-1088R-02026: DOIS terminais, nao quatro.

    Desenho "TACT SWITCH / TS-1088R-XXX" rev A, vetorial, cotas lidas da
    geometria. O part number se decodifica no proprio desenho: R = sem pino
    de posicionamento, 020 = 2,0 mm de altura, 26 = 260 gf.

    A Omron B3S-1002P que esta peca substitui tem quatro pinos - dois pares
    ligados internamente - e este tem dois. O esquematico deste projeto ja
    tratava a tecla como duas pontas, entao a troca nao mexe na netlist; o
    land pattern, sim, e outro.

      corpo            3,90 x 3,00 x 2,00
      embolo           ø1,80, sobe 0,50 acima da tampa de aco
      curso            0,2 +-0,1
      pad              1,05 x 2,00, centro em +-2,225
      vao externo      5,50   vao interno 3,40
    """
    pads = [_pad("1", -2.225, 0.0, 1.05, 2.00),
            _pad("2", 2.225, 0.0, 1.05, 2.00)]
    return _corpo(nome, 3.90, 3.00, pads,
                  "XunPu TS-1088R-02026, 260 gf, 2 terminais; land pattern do "
                  "desenho rev A")


def buzzer_pklcs1212(nome):
    """Murata PKLCS1212E4001-R1, especificacao JGB40-1584B de 2021-05-21.

    12,0 x 12,0 x 3,0 max, tolerancia geral +-0,2. Transdutor PASSIVO: nao
    tem oscilador, e a nota 12-4 da especificacao pede resistor de serie de
    1 k a 2 k ou diodo em paralelo, e proibe DC.

    O furo de som NAO fica em cima: e uma fenda de 1,9 x 0,9 numa parede
    lateral, com a aresta de baixo a 0,30 mm do plano de assento, e a face
    inferior tem um canal de 6,4 x 2,1 x 0,7 saindo pela parede oposta. A
    caixa precisa de duto lateral, nao de furo na tampa.

      pad         1,2 x 4,0, centro em +-5,6
      vao externo 12,4   vao interno 10,0
      eletrodo    4,0 de comprimento, sobe 1,2 pela parede
    """
    pads = [_pad("1", -5.6, 0.0, 1.2, 4.0),
            _pad("2", 5.6, 0.0, 1.2, 4.0)]
    return _corpo(nome, 12.0, 12.0, pads,
                  "Murata PKLCS1212E4001-R1, 84 dB a 4 kHz, passivo; furo de "
                  "som lateral de 1,9 x 0,9 rente a placa")


def led_rgb_3528(nome):
    """TUOZHAN S4-3528RGBTA-A, ficha S35210052 de 2021-02-21.

    3,5 x 2,8 x 1,9, anodo comum. A pinagem sai do esquema interno impresso
    no mesmo desenho, e o anodo fica na DIAGONAL do vermelho:

      1  B   catodo azul      superior esquerdo
      2  +   anodo comum      inferior esquerdo
      3  G   catodo verde     superior direito
      4  R   catodo vermelho  inferior direito, no canto chanfrado

    Muito mais brilhante que o APTF1616 que substitui - 460 a 1000 mcd no
    vermelho contra 15, 1300 a 2900 no verde contra 50 - entao os resistores
    de serie precisam ser RECALCULADOS para nao ofuscar a noite.

      pad 1,8 x 0,8, centros em (+-1,70; +-0,75); o pad avanca 0,85 para
      fora do corpo de cada lado, como e proprio de LED
    """
    # Os nomes sao os do esquematico, nao os numeros da ficha: o pino 1 da
    # ficha e o catodo azul, o 2 o anodo comum, o 3 o verde e o 4 o vermelho.
    pads = [_pad("KB", -1.70, 0.75, 1.8, 0.8),
            _pad("A", -1.70, -0.75, 1.8, 0.8),
            _pad("KG", 1.70, 0.75, 1.8, 0.8),
            _pad("KR", 1.70, -0.75, 1.8, 0.8)]
    return _corpo(nome, 3.5, 2.8, pads,
                  "TUOZHAN S4-3528RGBTA-A, anodo comum no pino 2; canto "
                  "chanfrado junto ao pino 4 (vermelho). MSL4")


def antena_unictron(nome):
    """Unictron H2UJ4U1H2Q0100 (CB501F), ficha rev. E de 2020-07-07.

    Antena de chip ceramica L1 + L5, 5,0 x 3,0 x 0,5 mm. Substitui a TE
    L000670-01, que esta com estoque zero na LCSC - e de quebra resolve o
    problema que a TE criava: a area sem cobre cai de 40,5 x 14,5 mm, que e
    mais larga que esta placa inteira, para 15,00 x 9,35.

    Land pattern do guia de layout, com o desenho medido em pixels contra as
    cotas impressas (13,92 px/mm em x e 13,95 em y, conferidas em 7 cotas
    cada). Origem aqui no centro do corpo:

      pino 1  terra / sintonia   (+2,50; 0)   0,80 x 2,98
      pino 2  terra / sintonia   (-2,50; 0)   0,80 x 2,98
      pino 3  SINAL              ( 0,00; 0)   1,00 x 2,00

    Os dois pads de terra ficam A CAVALEIRO das pontas: 0,40 mm sob o corpo
    e 0,40 para fora, porque as terminacoes das pontas sao wrap-around e o
    filete precisa subir pela lateral metalizada. Nao e folga de desenho.
    O terminal de sinal, de 0,75 mm, existe so na face de baixo, no centro
    geometrico da antena.

    Os pinos de terra NAO vao direto ao plano: vao por dois capacitores de
    sintonia, que e o que a ficha manda ([8] e [9] do circuito da pagina 11).
    """
    pads = [_pad("1", 2.50, 0.0, 0.80, 2.98),
            _pad("2", -2.50, 0.0, 0.80, 2.98),
            _pad("3", 0.0, 0.0, 1.00, 2.00)]
    # A marca de orientacao, e ela nao e enfeite. A peca e FISICAMENTE
    # SIMETRICA - as duas terminacoes de ponta sao wrap-around iguais e o
    # terminal de sinal fica no centro geometrico -, entao o pick-and-place
    # nao tem como distinguir a orientacao pela geometria. Uma peca montada
    # girada 180 graus fica com o lado errado voltado para o recorte largo e
    # PASSA em todo teste eletrico, porque eletricamente os dois terras sao
    # iguais. A unica assimetria da peca e a letra "U" impressa no topo,
    # descentrada 1,10 mm, e ela aponta para o lado do recorte de 7,54.
    #
    # Entao a serigrafia marca esse lado: uma seta curta fora do contorno,
    # do lado +x, que e para onde o "U" tem de apontar.
    seta = (
        '\t(fp_poly\n'
        '\t\t(pts (xy 3.10 -1.90) (xy 4.00 -1.40) (xy 3.10 -0.90))\n'
        '\t\t(stroke (width 0.12) (type solid))\n'
        '\t\t(fill solid)\n'
        '\t\t(layer "F.SilkS")\n'
        '\t\t(uuid "' + _uid(nome, "seta") + '")\n'
        '\t)')
    texto = (
        '\t(fp_text user "U"\n'
        '\t\t(at 2.20 -2.40 0)\n'
        '\t\t(layer "F.SilkS")\n'
        '\t\t(uuid "' + _uid(nome, "txtU") + '")\n'
        '\t\t(effects (font (size 0.6 0.6) (thickness 0.12)))\n'
        '\t)')
    return _corpo(nome, 5.0, 3.0, pads + [seta, texto],
                  "Unictron H2UJ4U1H2Q0100, antena de chip L1+L5; pino 3 e o "
                  "sinal, 1 e 2 sao terra por capacitor de sintonia")


GERADOS: dict[str, str] = {}


def _gerar():
    # MAX17262 WLP-9: 3 x 3 balls, 0.4 mm pitch, package 1.4 x 1.4 mm.
    GERADOS["gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm"] = wlp(
        "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm", 3, 3, 0.4, 0.20, 1.4, 1.4,
        "MAX17262 WLP de 9 bolas; land pattern aproximado, "
        "a ficha recomendada nao foi lida")
    # MMC5603NJ WLP-4, ficha Rev. B de 2018-07-12, paginas 17 e 18. O
    # MMC5633NJL que estava aqui nao existe na LCSC; este e da mesma
    # familia, responde no MESMO 0x30 e devolve o MESMO 0x10 no registrador
    # 0x39, entao o driver nao muda.
    #
    #   corpo   0,82 x 0,82 (NAO 0,85), dado de 0,40 mais 0,14 de bola
    #   passo   0,40 em x e em y      bola  o0,22
    #   pad     o0,23 - so 0,01 maior que a bola
    #
    # Mapa: A1 = VSA, A2 = SCL, B1 = VDD, B2 = SDA, com o ponto preto da
    # face de cima marcando o A1. E o mesmo mapa do MMC5633NJL.
    GERADOS["gnssbike:MMC5603_WLP-4_0.82x0.82mm"] = wlp(
        "gnssbike:MMC5603_WLP-4_0.82x0.82mm", 2, 2, 0.4, 0.23, 0.82, 0.82,
        "Memsic MMC5603NJ, WLP de 4 bolas; land pattern da pagina 18 da "
        "ficha Rev. B")
    GERADOS["gnssbike:Antena_Unictron_H2UJ4U1H2Q0100_5x3mm"] = antena_unictron(
        "gnssbike:Antena_Unictron_H2UJ4U1H2Q0100_5x3mm")
    GERADOS["gnssbike:SW_TS-1088R_3.9x3mm"] = tecla_ts1088(
        "gnssbike:SW_TS-1088R_3.9x3mm")
    GERADOS["gnssbike:Buzzer_PKLCS1212E4001_12x12mm"] = buzzer_pklcs1212(
        "gnssbike:Buzzer_PKLCS1212E4001_12x12mm")
    GERADOS["gnssbike:LED_RGB_3528_3.5x2.8mm"] = led_rgb_3528(
        "gnssbike:LED_RGB_3528_3.5x2.8mm")
    # OPT3001 USON-6, 2.0 x 2.0 mm, 0.65 mm pitch.
    GERADOS["gnssbike:OPT3001_USON-6_2x2mm_P0.65mm"] = son(
        "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm", 6, 0.65, 0.35, 0.28, 1.8,
        2.0, 2.0, "TI OPT3001 em DNP, USON-6; land pattern aproximado")
    # ESD761 X1SON-2, 1.0 x 0.6 mm.
    GERADOS["gnssbike:ESD761_X1SON-2_1x0.6mm"] = son(
        "gnssbike:ESD761_X1SON-2_1x0.6mm", 2, 0.5, 0.35, 0.45, 0.6,
        1.0, 0.6, "TI ESD761 em DPY, X1SON-2; land pattern aproximado")
    # TPD4E05U06 USON-10, 1.0 x 2.5 mm, 0.5 mm pitch.
    GERADOS["gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm"] = son(
        "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm", 10, 0.5, 0.3, 0.24, 0.8,
        1.0, 2.5, "TI TPD4E05U06 em DQA, USON-10; land pattern aproximado")
    # BMP585 LGA-8, 3.25 x 3.25 mm.
    GERADOS["gnssbike:BMP585_LGA-8_3.25x3.25mm"] = son(
        "gnssbike:BMP585_LGA-8_3.25x3.25mm", 8, 0.65, 0.5, 0.35, 2.5,
        3.25, 3.25, "Bosch BMP585, LGA-8; land pattern aproximado")
    # TXU0204 WQFN-14, 3.0 x 2.5 mm, 0.5 mm pitch, thermal pad.
    GERADOS["gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm"] = son(
        "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm", 14, 0.5, 0.45, 0.28, 2.6,
        3.0, 2.5, "TI TXU0204 em BQA, WQFN-14 com pad termico; "
                  "land pattern aproximado", ep=("PAD", 1.6, 1.6))
    # MAX form factor, 9,7 x 10,1 mm, 18 pads, 9 per edge, LCC with half
    # vias on the two long edges. It carries EITHER receiver: the MAX-F10S
    # and the MAX-M10S have the same 18 pins, the same names, the same
    # directions, the same package and the same recommended land pattern -
    # MAX-F10S data sheet UBXDOC-963802114-12732 R03 table 10 and figure 4
    # against MAX-M10S data sheet UBX-20035208 R08 table 10 and figure 4.
    #
    # The numbers below are that land pattern, read at last: M10S
    # integration manual UBX-20053088 R05 table 44, whose every value the
    # dimensioned figure 25 of the F10S manual repeats.
    #
    #   pad into the body      M = 1,0        body edge at 4,85
    #   pad out of the body    N = 0,8   ->   pad from 3,85 to 5,65
    #   pad length                             1,8, centred at 4,75
    #   normal pad width       K = 0,8
    #   corner pad width       L = 0,7
    #
    # What was here before had a 1,4 mm pad centred at 4,50: it reached the
    # right 1,05 mm under the module but left only 0,35 mm outside instead of
    # 0,8. That outside part is what lets the solder climb the half via and
    # is the only part of the joint anyone can inspect.
    GERADOS["gnssbike:u-blox_MAX_LCC-18_9.7x10.1mm"] = duas_bordas(
        "gnssbike:u-blox_MAX_LCC-18_9.7x10.1mm", 9, 1.1, 1.8, 0.8, 9.5,
        9.7, 10.1, "u-blox MAX-F10S ou MAX-M10S; 18 pads, 9 por borda, passo "
                   "1,1. Land pattern da tabela 44 do UBX-20053088 R05, "
                   "confirmado na figura 25 do UBXDOC-963802114-12892",
        pad_h_canto=0.7)
    # Kingbright APTF1616 RGB, 1.6 x 1.6 mm, four terminals.
    GERADOS["gnssbike:LED_RGB_APTF1616_1.6x1.6mm"] = son(
        "gnssbike:LED_RGB_APTF1616_1.6x1.6mm", 4, 0.8, 0.45, 0.55, 1.5,
        1.6, 1.6, "Kingbright APTF1616SEEZGKQBKC, anodo comum; "
                  "land pattern aproximado", nomes=["A", "KR", "KB", "KG"])


_gerar()


# The KiCad 3D library ships thousands of models but not one for every
# footprint, and eighteen of the ones this board uses are missing: the tactile
# key, the USB-C receptacle, SOT-523, X2SON-4, the 4 mm QFN-28, SOIC-8, the
# buzzer and the light's FPC connector, plus the test points and the mounting
# hole, which have no body at all. Those parts showed as bare pads, which is
# useless for the one thing a 3D view of a PCB is for: seeing whether anything
# collides and whether it all fits under the lid.
#
# So they get the same treatment as the ten footprints drawn here: a box. Its
# X and Y come from the footprint's own F.Fab outline, which is the package
# outline and is therefore exact. Its height comes from this table. Where the
# number is from the part's datasheet it says so; where it is the usual value
# for the package it says THAT, because a height nobody checked has no
# business looking like a measurement.
ALTURA: dict[str, tuple[float, str]] = {
    # the parts whose height decides whether the lid closes
    # lido no desenho cotado da ficha da Omron, pagina 2
    "gnssbike:SW_TS-1088R_3.9x3mm": (2.00, "XunPu TS-1088R-02026, desenho "
        "rev A: altura total 2,00, tampa de aco ate 1,50, embolo de o1,80 "
        "saindo 0,50 acima dela. Curso de 0,2 +-0,1"),
    "Connector_USB:USB_C_Receptacle_HRO_TYPE-C-31-M-12": (3.26, "altura "
        "corrente de um receptaculo USB-C de montagem em superficie - "
        "CONFERIR na ficha do HRO TYPE-C-31-M-12 (LCSC C165948)"),
    "Connector_JST:JST_ZH_S4B-ZR-SM4A-TF_1x04-1MP_P1.50mm_Horizontal": (4.50,
        "altura corrente de um header ZH de entrada lateral - CONFERIR na "
        "ficha da JST"),
    "Diode_SMD:D_SOD-523": (0.65, "altura corrente de um SOD-523 - a peca do "
        "D105 ainda nao foi escolhida"),

    "Connector_FFC-FPC:TE_0-1734839-5_1x05-1MP_P0.5mm_Horizontal": (1.20,
        "conector FPC horizontal de passo 0,5 - CONFERIR: a peca ainda nao "
        "esta escolhida (06#j402)"),
    # the flat ones, all well under the 2,6 mm the display leaves
    # Still without a dimensioned drawing in hand. PACOTE overrides every one
    # of these the moment its own datasheet is read, and altura_de() below is
    # what enforces that, so a number here can never quietly outlive the real
    # one: the QFN said 0.90 when its datasheet says 0.80.
    "Package_SO:SOIC-8_5.275x5.275mm_P1.27mm": (2.00, "altura normal do SOIC-8 "
        "de 200 mil - CONFERIR no desenho da Macronix"),
    # NAO lida de ficha nenhuma, porque nao ha peca escolhida: a mola so
    # precisa sobrar da placa para ser esmagada quando a caixa fecha, e
    # 1,5 mm e o curso corrente de um contato de mola SMD desse tamanho.
    # 04-pcb-e-caixa.md registra que a area dos contatos nao esta
    # dimensionada em lugar nenhum, e parts.py manda CONFERIR antes de
    # fabricar. O numero existe para o desenho, nao para a compra.
    "gnssbike:ContatoMola_2x2mm_P3mm": (1.50, "curso corrente de um contato "
        "de mola SMD - NAO lido de ficha: a mola ainda nao foi escolhida "
        "(04-pcb-e-caixa.md, parts.py J103/J302)"),
}


def altura_de(nome: str) -> tuple[float, str] | None:
    """The height of a package: the datasheet's if there is one.

    PACOTE holds what a mechanical drawing says and ALTURA what is merely
    usual for the family. When both have an entry the drawing wins, and it
    has to: those two disagreed by 0.10 mm on the harvester's QFN and by
    0.15 on the ESD array, and the number that was being used was the guess.
    """
    if nome in PACOTE:
        return (PACOTE[nome][2], PACOTE[nome][8])
    return ALTURA.get(nome)
# No body at all, and that is correct: a test point is a pad and a mounting
# hole is a hole.
SEM_CORPO = {"TestPoint:TestPoint_Pad_D1.0mm",
             "MountingHole:MountingHole_2.2mm_M2",
             "Connector:Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical"}


def fab_do_footprint(nome: str) -> tuple[float, float] | None:
    """The package outline of a footprint, from its own F.Fab drawing."""
    import fp_load as _fl

    arv = _fl.parse(_fl.carregar(nome)[0])
    xs: list[float] = []
    ys: list[float] = []
    for chave in ("fp_line", "fp_rect", "fp_poly", "fp_circle"):
        for g in _fl.kids(arv, chave):
            lay = _fl.kid(g, "layer")
            if not lay or lay[1] not in ("F.Fab", "B.Fab"):
                continue
            for tag in ("start", "end", "center", "mid"):
                q = _fl.kid(g, tag)
                if q:
                    xs.append(float(q[1]))
                    ys.append(float(q[2]))
            pts = _fl.kid(g, "pts")
            if pts:
                for q in _fl.kids(pts, "xy"):
                    xs.append(float(q[1]))
                    ys.append(float(q[2]))
    if not xs:
        return None
    return (max(xs) - min(xs), max(ys) - min(ys))


# Where KiCad keeps its own 3D models.
LIB3D = __import__("pathlib").Path(r"D:\KiCAD\share\kicad\3dmodels")
NL = chr(10)
TAB = chr(9)


# A part the maker does publish a model for goes in cad/3d/real/, and it
# wins over the box drawn here. Put the file there under the footprint's own
# name - SW_SPST_B3S-1000.step for the key, USB_C_Receptacle_Palconn_UTC16-G
# .step for the receptacle - and the next run picks it up with nothing else
# to change. STEP is preferred because it is the only format KiCad's GLB and
# STEP exports read; a .wrl works in the 3D viewer alone.
#
# The model files themselves are NOT committed: this repository is public and
# a manufacturer's 3D model carries the manufacturer's terms, the same reason
# the datasheets stay out. cad/3d/real/ is in the .gitignore, and 3d/LEIAME.md
# lists which parts are worth fetching and under what name.
def modelo_de_verdade(base: str) -> str | None:
    """A manufacturer model dropped into cad/3d/real/, if there is one."""
    import pathlib as _pl

    pasta = _pl.Path(__file__).resolve().parent / "3d" / "real"
    for ext in (".step", ".stp", ".STEP", ".wrl"):
        p = pasta / (base + ext)
        if p.exists():
            return "3d/real/" + p.name
    return None


def linha_de_modelo(rel: str) -> str:
    return (TAB + '(model "${KIPRJMOD}/' + rel + '"' + NL +
            TAB * 2 + "(offset (xyz 0 0 0))" + NL +
            TAB * 2 + "(scale (xyz 1 1 1))" + NL +
            TAB * 2 + "(rotate (xyz 0 0 0))" + NL +
            TAB + ")" + NL)


def trocar_modelo(nome: str, corpo: str) -> str:
    """Give a library footprint a body when KiCad has no model for it.

    Leaves the model alone when the file is really there - 85 of this board's
    footprints are in that case and use KiCad's own STEP. When it is not,
    draws the box and points the footprint at it. When the part has no body
    to speak of - a test point, a mounting hole, a Tag-Connect that is only
    pads - takes the model line out, so nothing pretends otherwise.
    """
    import pathlib as _pl
    import re as _re

    m = _re.search(r'\(model "([^"]+)"', corpo)
    if not m:
        return corpo
    caminho = m.group(1)
    if caminho.startswith("${KIPRJMOD}"):
        return corpo                      # already one of ours
    rel = caminho.replace("${KICAD8_3DMODEL_DIR}/", "")
    if (LIB3D / rel).exists() or (LIB3D / rel.replace(".wrl", ".step")).exists():
        return corpo                      # KiCad has it: use KiCad's

    def sem_bloco(texto: str) -> str:
        i = texto.index("(model ")
        d, j = 0, i
        while j < len(texto):
            c = texto[j]
            if c == '"':
                j += 1
                while j < len(texto) and texto[j] != '"':
                    j += 2 if texto[j] == "\\" else 1
            elif c == "(":
                d += 1
            elif c == ")":
                d -= 1
                if d == 0:
                    break
            j += 1
        inicio = texto.rfind(NL, 0, i) + 1
        return texto[:inicio] + texto[j + 1:].lstrip(NL)

    base_real = nome.split(":", 1)[1]
    real = modelo_de_verdade(base_real)
    if real:
        return sem_bloco(corpo).rstrip(NL) + NL + linha_de_modelo(real)
    if nome in SEM_CORPO:
        return sem_bloco(corpo)
    tam = fab_do_footprint(nome)
    medida = altura_de(nome)
    if tam is None or medida is None:
        return sem_bloco(corpo)
    alt, _fonte = medida
    # The BODY is the datasheet's, whenever the datasheet has been read. The
    # F.Fab outline is what someone drew for the land pattern, and where the
    # two disagree it is the drawing that is right - the Molex receptacle is
    # 9.99 x 8.58 and the footprint in use draws 8.94 x 7.32, so taking the
    # footprint's size would have drawn the part a millimetre small in both
    # directions and hidden, in the 3D view, exactly the mismatch that ME3
    # reports in the 2D one.
    if nome in PACOTE:
        tam = (PACOTE[nome][0], PACOTE[nome][1])
    base = nome.split(":", 1)[1]
    pasta = _pl.Path(__file__).resolve().parent / "3d"
    pasta.mkdir(exist_ok=True)
    est = estilo_de(nome)
    if nome in DESENHADOS:
        DESENHADOS[nome](pasta / (base + ".wrl"), tam[0], tam[1], alt)
    elif est:
        wrl_ci(pasta / (base + ".wrl"), tam[0], tam[1], alt, est, nome)
    else:
        wrl_caixa(pasta / (base + ".wrl"), tam[0], tam[1], alt, cor=cor_de(nome))
    CORPO_TODOS[nome] = (tam[0], tam[1], alt)
    return sem_bloco(corpo).rstrip(NL) + NL + linha_de_modelo("3d/" + base + ".wrl")


def _caixa_vrml(pts, faces, cor) -> str:
    return ("Shape {\n  appearance Appearance { material Material { "
            f"diffuseColor {cor[0]} {cor[1]} {cor[2]} }} }}\n"
            "  geometry IndexedFaceSet {\n    coord Coordinate { point [\n"
            + ",\n".join(f"      {x:.4f} {y:.4f} {z:.4f}" for x, y, z in pts)
            + " ] }\n    coordIndex [\n"
            + ",\n".join("      " + " ".join(str(i) for i in f) + " -1"
                         for f in faces)
            + " ]\n  }\n}\n")


def _bloco(x0, y0, z0, x1, y1, z1, cor) -> str:
    e = VRML_POR_MM
    pts = [(x0 * e, y0 * e, z0 * e), (x1 * e, y0 * e, z0 * e),
           (x1 * e, y1 * e, z0 * e), (x0 * e, y1 * e, z0 * e),
           (x0 * e, y0 * e, z1 * e), (x1 * e, y0 * e, z1 * e),
           (x1 * e, y1 * e, z1 * e), (x0 * e, y1 * e, z1 * e)]
    faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4),
             (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
    return _caixa_vrml(pts, faces, cor)


def wrl_me54bs13(caminho) -> None:
    """The radio module as the mechanical drawing of V1.0.0 draws it.

    Not one box but two, because the part is not one box: a 12.00 x 16.50 mm
    printed circuit 0.80 mm thick, and a metal shield can 1.60 mm high over
    everything except the last 4.46 mm, which is the antenna and has to stay
    open. Seeing that in the 3D view is the difference between believing the
    antenna faces the board edge and knowing it.
    """
    W, H, ANT, ESP, ALT = 12.00, 16.50, 4.46, 0.80, 2.40
    # footprint coordinates: the antenna is at -Y, which is up on the screen
    pcb = _bloco(-W / 2, -H / 2, 0.0, W / 2, H / 2, ESP, (0.05, 0.28, 0.12))
    lata = _bloco(-W / 2 + 0.1, -H / 2 + ANT, ESP, W / 2 - 0.1, H / 2 - 0.1,
                  ALT, (0.62, 0.63, 0.66))
    # the meander, drawn flat on the antenna end so it is visible
    trilha = _bloco(-W / 2 + 0.6, -H / 2 + 0.6, ESP, W / 2 - 0.6,
                    -H / 2 + 1.2, ESP + 0.05, (0.78, 0.66, 0.30))
    trilha += _bloco(-W / 2 + 0.6, -H / 2 + 2.4, ESP, W / 2 - 0.6,
                     -H / 2 + 3.0, ESP + 0.05, (0.78, 0.66, 0.30))
    caminho.write_text(
        "#VRML V2.0 utf8\n"
        "# MinewSemi ME54BS13, do desenho mecanico da ficha V1.0.0:\n"
        "# 12,00 x 16,50 x 2,40 mm, com a antena nos 4,46 mm de uma ponta,\n"
        "# fora da blindagem. Corpo desenhado aqui: a Minew nao publica STEP.\n"
        + pcb + lata + trilha, encoding="utf-8", newline="\n")


def wrl_max_f10s(caminho, w: float, h: float, alt: float) -> None:
    """The GNSS receiver: a printed circuit under a metal shield can.

    u-blox publishes no STEP for the MAX-F10S and their site does not serve
    a page this can read, so the body is drawn from the dimensions the
    footprint already carries. The can covers the whole part, which is why
    section 4.4 of the integration manual can ask for ground under it.
    """
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# u-blox MAX-F10S, 9,7 x 10,1 x 2,4 mm, blindagem metalica sobre" + NL +
        "# toda a peca. Corpo desenhado aqui: a u-blox nao publica STEP." + NL +
        _bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, 0.8, (0.05, 0.28, 0.12)) +
        _bloco(-w / 2 + 0.1, -h / 2 + 0.1, 0.8, w / 2 - 0.1, h / 2 - 0.1,
               alt, (0.62, 0.63, 0.66)),
        encoding="utf-8", newline=NL)


# The two modules are drawn properly instead of as a plain box: they are the
# parts whose shape says something - where the shield ends and the antenna
# begins - and they are the two the owner asked to see as they really are.


# What each package actually looks like. A board where every drawn part is
# the same grey box tells you nothing; these are the colours of the real
# materials, so a moulded plastic IC reads as black epoxy, a shield can as
# tin plate and a ceramic capacitor as the pale tan it is. The key is matched
# against the footprint name, first hit wins.
COR_PACOTE = (
    ("MinewSemi", (0.62, 0.63, 0.66)),     # shield can
    ("u-blox", (0.62, 0.63, 0.66)),        # shield can
    ("USB_C", (0.78, 0.79, 0.80)),         # stainless shell
    ("FFC-FPC", (0.90, 0.88, 0.82)),       # ivory housing with a dark latch
    ("SW_SPST", (0.10, 0.10, 0.11)),       # black body
    ("Buzzer", (0.09, 0.09, 0.10)),        # black can
    ("LED_RGB", (0.92, 0.92, 0.90)),       # clear lens
    ("TestPoint", (0.80, 0.70, 0.35)),
    ("QFN", (0.09, 0.09, 0.10)),
    ("SOIC", (0.09, 0.09, 0.10)),
    ("SON", (0.09, 0.09, 0.10)),
    ("SOT", (0.09, 0.09, 0.10)),
    ("WLP", (0.22, 0.20, 0.24)),           # bare silicon, purple-grey
    ("LGA", (0.12, 0.12, 0.13)),
)
COR_PADRAO = (0.11, 0.11, 0.12)


def cor_de(nome: str) -> tuple[float, float, float]:
    for chave, c in COR_PACOTE:
        if chave.lower() in nome.lower():
            return c
    return COR_PADRAO


def _cilindro(x, y, z0, z1, raio, cor, lados: int = 20) -> str:
    """A can: the buzzer, and the plunger of a tactile switch."""
    e = VRML_POR_MM
    pts = []
    for z in (z0, z1):
        for i in range(lados):
            a = 2 * math.pi * i / lados
            pts.append(((x + raio * math.cos(a)) * e,
                        (y + raio * math.sin(a)) * e, z * e))
    faces = []
    for i in range(lados):
        j = (i + 1) % lados
        faces.append((i, j, lados + j, lados + i))
    faces.append(tuple(range(lados - 1, -1, -1)))
    faces.append(tuple(range(lados, 2 * lados)))
    return _caixa_vrml(pts, faces, cor)


def wrl_tecla(caminho, w: float, h: float, alt: float) -> None:
    """Omron B3S: a black body with a round plunger on top.

    It is the tallest part on the board, and the one whose shape decides
    whether the lid can be pressed, so drawing it as a plain 5 mm block hides
    the only thing about it that matters.
    """
    corpo_h = alt - 0.7          # o embolo sai 0,7 mm do corpo
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# Omron B3S-1002P: corpo de 3,5 mm e botao ate 5,0 mm" + NL +
        _bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, corpo_h, (0.10, 0.10, 0.11)) +
        _cilindro(0.0, 0.0, corpo_h, alt, 1.65, (0.20, 0.20, 0.22)),
        encoding="utf-8", newline=NL)


def wrl_buzzer(caminho, w: float, h: float, alt: float) -> None:
    """Same Sky CPT-1117-83-SMT: a black rectangle with two tabs, not a can.

    Drawn round, it was the wrong shape twice over - the part on the
    shopping list is 11,0 x 9,0 x 1,7 mm of black LCP, and what holds it to
    the board is two flat tinned brass tabs, 2,0 mm wide by 0,2 thick, one
    at each end and on OPPOSITE sides of the axis, each with a 0,8 mm hole,
    taking it to 15,0 mm end to end. The sound comes out of seven holes on
    the top face. Every number is from the mechanical drawing on page 2 of
    the datasheet.
    """
    PRETO = (0.07, 0.07, 0.08)           # LCP preto
    LATAO = (0.80, 0.78, 0.72)           # latao estanhado
    partes = [_bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, alt, PRETO)]
    # as duas abas, em lados opostos do eixo, levando o total a 15,0 mm
    for lado in (-1, 1):
        x0 = lado * w / 2
        x1 = lado * (w / 2 + 2.0)
        y = lado * (h / 2 - 1.0)
        partes.append(_bloco(min(x0, x1), y - 1.0, 0.0,
                             max(x0, x1), y + 1.0, 0.2, LATAO))
        partes.append(_cilindro((x0 + x1) / 2, y, 0.0, 0.21, 0.4,
                                (0.55, 0.54, 0.50), 10))
    # os sete furos de som
    for bx, by in ((0.0, 0.0), (-2.2, 0.0), (2.2, 0.0), (-1.1, 1.9),
                   (1.1, 1.9), (-1.1, -1.9), (1.1, -1.9)):
        partes.append(_cilindro(bx, by, alt - 0.05, alt + 0.01, 0.35,
                                (0.30, 0.30, 0.31), 12))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# Same Sky CPT-1117-83-SMT: 11,0 x 9,0 x 1,7 preto, duas abas de "
        "latao e sete furos de som" + NL + "".join(partes),
        encoding="utf-8", newline=NL)


def wrl_buzzer_pklcs(caminho, w: float, h: float, alt: float) -> None:
    """Murata PKLCS1212E4001-R1: 12 x 12 x 3,0, cinza-grafite, furo LATERAL.

    Duas coisas que a especificacao JGB40-1584B corrige de uma vez:

      - o furo de som NAO fica em cima. E uma fenda de (1,9) x (0,9) numa
        parede lateral sem eletrodo, com a aresta de baixo a 0,30 mm do
        plano de assento, e a face inferior tem um canal de (6,4) x (2,1) x
        (0,7) saindo pela parede oposta. Quem desenha furos no topo desenha
        outro buzzer, e a caixa acaba com um furo no lugar errado.
      - a cor. A Murata nao declara cor nenhuma no documento; a foto do
        produto mostra corpo cinza-grafite escuro FOSCO E UNIFORME, topo e
        parede praticamente iguais, sem a face superior clara que os
        buzzers de lata costumam ter. Os terminais sao DOURADOS.

    Os eletrodos tem 4,0 mm de comprimento, sobem 1,2 mm pela parede e
    avancam 1,0 mm sobre a face inferior.
    """
    GRAFITE = (0.42, 0.42, 0.46)          # medido na foto: RGB 107,108,118
    PAREDE = (0.38, 0.38, 0.42)           # 98,97,107
    DOURADO = (0.83, 0.71, 0.36)
    VAO = (0.10, 0.10, 0.11)
    partes = [_bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, alt, GRAFITE)]
    # os dois eletrodos, 4,0 de comprimento centrados, subindo 1,2
    for lado in (-1, 1):
        x = lado * w / 2
        partes.append(_bloco(min(x, x - lado * 0.08), -2.0, 0.0,
                             max(x, x - lado * 0.08), 2.0, 1.2, DOURADO))
        # o avanco de 1,0 sobre a face inferior
        partes.append(_bloco(min(x, x - lado * 1.0), -2.0, 0.0,
                             max(x, x - lado * 1.0), 2.0, 0.05, DOURADO))
    # a fenda de som: parede SEM eletrodo, 1,9 x 0,9, a 0,30 da placa
    partes.append(_bloco(-0.95, h / 2 - 0.09, 0.30, 0.95, h / 2 + 0.01,
                         0.30 + 0.9, VAO))
    # o canal da face inferior, na parede oposta
    partes.append(_bloco(-3.2, -h / 2 - 0.01, 0.0, 3.2, -h / 2 + 2.1, 0.7,
                         PAREDE))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# Murata PKLCS1212E4001-R1: 12 x 12 x 3,0 cinza-grafite, fenda de "
        "som lateral de 1,9 x 0,9 a 0,30 da placa, eletrodos dourados" + NL +
        "".join(partes), encoding="utf-8", newline=NL)


def wrl_tecla_ts1088(caminho, w: float, h: float, alt: float) -> None:
    """XunPu TS-1088R-02026: corpo preto, tampa de aco nu, embolo preto.

    Cotas do desenho rev A, cores da tabela de materiais do proprio desenho:
    corpo e embolo em LCP preto, tampa em aco inoxidavel "limpo" (aco nu,
    prateado brilhante) com quatro abas dobradas nos cantos, terminais em
    bronze fosforoso prateado.

      corpo 3,90 x 3,00, tampa ate 1,50, embolo o1,80 ate 2,00
      ou seja: o botao sobe 0,50 mm acima da tampa, e nao 0,7 como a Omron
    """
    PRETO = (0.07, 0.07, 0.08)            # LCP preto
    ACO = (0.78, 0.79, 0.81)              # aco inoxidavel nu
    BRONZE = (0.72, 0.71, 0.66)           # bronze fosforoso estanhado
    partes = [
        _bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, 1.20, PRETO),
        _bloco(-w / 2, -h / 2, 1.20, w / 2, h / 2, 1.50, ACO),
        _cilindro(0.0, 0.0, 1.50, 2.00, 0.90, PRETO, 16),
    ]
    # os dois terminais, saindo 0,545 de cada lado, 1,30 de largura
    for lado in (-1, 1):
        x0 = lado * 1.952
        x1 = lado * 2.497
        partes.append(_bloco(min(x0, x1), -0.65, 0.0, max(x0, x1), 0.65,
                             0.13, BRONZE))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# XunPu TS-1088R-02026: 3,90 x 3,00 x 2,00, corpo e embolo pretos, "
        "tampa de aco nu, embolo o1,80 subindo 0,50 acima da tampa" + NL +
        "".join(partes), encoding="utf-8", newline=NL)


def wrl_led_3528(caminho, w: float, h: float, alt: float) -> None:
    """TUOZHAN S4-3528RGBTA-A: corpo branco, lente transparente incolor.

    A ficha declara o encapsulante como 透明 "water clear" - transparente
    INCOLOR, nao leitoso. A cor do corpo a ficha nao declara; a foto do
    produto mostra resina BRANCA (parede RGB 217,212,227, fundo da cavidade
    245,239,245), que e o PPA/PCT de praxe. Registrado como inferido da
    foto, e nao da ficha.

    A secao de baixo tem 1,05 mm de altura reta; os 0,85 de cima afunilam de
    3,5 para 3,2 mm. A cavidade refletora mede 2,64 x 2,14, e o canto
    chanfrado fica junto ao pino 4, o vermelho.
    """
    BRANCO = (0.86, 0.85, 0.88)
    CAVIDADE = (0.96, 0.94, 0.96)
    LENTE = (0.92, 0.94, 0.96)
    ESTANHO = (0.74, 0.75, 0.77)
    partes = [
        _bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, 1.05, BRANCO),
        _bloco(-1.6, -h / 2 + 0.15, 1.05, 1.6, h / 2 - 0.15, alt, BRANCO),
        # a cavidade refletora, 2,64 x 2,14, cheia de resina transparente
        _bloco(-1.32, -1.07, 0.9, 1.32, 1.07, alt, CAVIDADE),
        _bloco(-1.30, -1.05, 1.0, 1.30, 1.05, alt - 0.02, LENTE),
    ]
    # os quatro terminais na face de baixo, 0,80 x 0,785
    for sx in (-1, 1):
        for sy in (-1, 1):
            partes.append(_bloco(sx * 1.35 - 0.40, sy * 0.7075 - 0.3925, 0.0,
                                 sx * 1.35 + 0.40, sy * 0.7075 + 0.3925,
                                 0.06, ESTANHO))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# TUOZHAN S4-3528RGBTA-A: 3,5 x 2,8 x 1,9, corpo branco, lente "
        "transparente incolor, anodo comum no pino 2" + NL +
        "".join(partes), encoding="utf-8", newline=NL)


def wrl_fpc(caminho, w: float, h: float, alt: float) -> None:
    """An FPC connector: a pale housing with the dark latch across the back.

    What makes an FPC connector recognisable at a glance is that it is two
    pieces in two colours - the moulded housing, which is ivory or beige, and
    the actuator, which is almost always dark brown or black and runs the
    whole width. Drawn as one black cuboid it reads as an IC, and the one
    thing a person checks in a 3D view of a flat cable connector is which way
    the mouth faces.

    NO DIMENSION HERE IS FROM A DATASHEET, because there is no datasheet to
    take one from: the part is not chosen yet (06-conectores-e-pontos-de-teste
    .md, J402) and its ALTURA entry says CONFERIR. The outline is the
    footprint's own courtyard, which is what the board already commits to,
    and the split between housing and actuator is a proportion, not a cote.
    """
    ALOJAMENTO = (0.88, 0.85, 0.76)      # marfim
    TRAVA = (0.24, 0.17, 0.13)           # marrom escuro
    CONTATO = (0.80, 0.70, 0.40)
    trava_h = min(1.8, h * 0.33)
    partes = [
        # o alojamento, com a boca no lado -Y
        _bloco(-w / 2, -h / 2 + trava_h, 0.0, w / 2, h / 2, alt, ALOJAMENTO),
        # a trava, atravessada na largura toda
        _bloco(-w / 2, -h / 2, alt * 0.15, w / 2, -h / 2 + trava_h, alt,
               TRAVA),
    ]
    # a fenda da boca, onde o cabo entra
    partes.append(_bloco(-w / 2 + 0.6, -h / 2 + trava_h - 0.05, alt * 0.25,
                         w / 2 - 0.6, -h / 2 + trava_h + 0.35, alt * 0.75,
                         (0.10, 0.09, 0.09)))
    # os contatos vistos pela boca
    n = max(2, int((w - 1.6) / 0.5))
    for i in range(n):
        bx = -w / 2 + 0.8 + (w - 1.6) * (i + 0.5) / n
        partes.append(_bloco(bx - 0.12, -h / 2 + trava_h, 0.0,
                             bx + 0.12, h / 2 - 0.3, 0.10, CONTATO))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# conector FPC: alojamento marfim e trava escura, contorno do "
        "footprint" + NL + "".join(partes), encoding="utf-8", newline=NL)


def wrl_led_rgb(caminho, w: float, h: float, alt: float) -> None:
    """Kingbright APTF1616: white housing, water-clear lens, gold corners.

    Every number here is from the PACKAGE DIMENSIONS block on page 1 of the
    datasheet: body 1.6 x 1.6, total height 0.7 (with the sheet's general
    tolerance of +-0.2 over it), the base plate 0.25 thick across the full
    width, and the moulded encapsulant above it a truncated pyramid 1.2 at
    the bottom and 1.1 at the top. Four corner terminals of 0.35 x 0.65, each
    with a castellation on its outer edge. The lens is "Water Clear" by the
    SELECTION GUIDE, not diffused.

    The 0.45 of lens height is 0.7 - 0.25, arithmetic, not a dimension: the
    sheet does not cote the lens on its own. The white of the housing is read
    off the product photograph on the same page, which the sheet does not
    state in words either - both are said so here rather than passed off as
    cotes.
    """
    BRANCO = (0.93, 0.93, 0.91)          # corpo, da foto do produto
    LENTE = (0.88, 0.90, 0.93)           # "Water Clear"
    OURO = (0.83, 0.68, 0.22)
    base_h = 0.25
    partes = [_bloco(-w / 2, -h / 2, 0.0, w / 2, h / 2, base_h, BRANCO)]
    # os quatro terminais de canto, na face de baixo
    for sx in (-1, 1):
        for sy in (-1, 1):
            partes.append(_bloco(sx * (w / 2 - 0.35), sy * (h / 2 - 0.65),
                                 -0.01, sx * w / 2, sy * (h / 2), 0.08, OURO))
    # o tronco de piramide: 1,2 na base e 1,1 no topo
    e = VRML_POR_MM
    b, t = 1.2 / 2, 1.1 / 2
    z0, z1 = base_h, alt
    pts = [(-b, -b, z0), (b, -b, z0), (b, b, z0), (-b, b, z0),
           (-t, -t, z1), (t, -t, z1), (t, t, z1), (-t, t, z1)]
    pts = [(x * e, y * e, z * e) for x, y, z in pts]
    faces = [(0, 3, 2), (0, 2, 1), (4, 5, 6), (4, 6, 7)]
    for k in range(4):
        i, j = k, (k + 1) % 4
        faces.append((i, j, 4 + j))
        faces.append((i, 4 + j, 4 + i))
    partes.append(_caixa_vrml(pts, faces, LENTE))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# Kingbright APTF1616: corpo branco 1,6 x 1,6 x 0,25 e lente "
        "transparente ate 0,7" + NL + "".join(partes),
        encoding="utf-8", newline=NL)


def wrl_mola(caminho, w: float, h: float, alt: float) -> None:
    """Two gold spring fingers, one arched leaf over each pad.

    This is where the antenna in the case wall and the three groups of solar
    modules meet the board, and it was the one part on it with no body at
    all - four of them, drawn as nothing. A flat pad is not what is there:
    what is there is a leaf that stands proud of the board and is squashed
    when the case closes, and that is the only reason its height matters.

    The two pads, 2.0 x 2.0 mm at 3.0 mm of pitch, are the project's own and
    are in contato_mola(). The ARCH is not: no spring has been chosen yet -
    `04-pcb-e-caixa.md` says the contact area is not dimensioned anywhere and
    parts.py says CONFERIR before manufacturing - so the leaf here is a
    generic SMD spring finger at the height ALTURA declares, and that number
    is marked in ALTURA as not read from any datasheet. When the part is
    chosen, its own model goes in 3d/real/ and this is never used again.
    """
    OURO = (0.83, 0.68, 0.22)
    partes = []
    for cx in (-1.5, 1.5):
        # the base that is soldered, and the leaf arching back over it
        partes.append(_bloco(cx - 1.0, -1.0, 0.0, cx + 1.0, -0.4, 0.12, OURO))
        passos = 8
        for i in range(passos):
            t0, t1 = i / passos, (i + 1) / passos
            y0, y1 = -0.4 + 1.6 * t0, -0.4 + 1.6 * t1
            z0 = 0.12 + (alt - 0.12) * math.sin(math.pi * t0)
            z1 = 0.12 + (alt - 0.12) * math.sin(math.pi * t1)
            partes.append(_bloco(cx - 0.9, y0, min(z0, z1) - 0.06,
                                 cx + 0.9, y1, max(z0, z1) + 0.06, OURO))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# contato de mola de 2 vias: lamina dourada sobre cada pad de 2x2" + NL
        + "".join(partes), encoding="utf-8", newline=NL)


def wrl_usb_c(caminho, w: float, h: float, alt: float) -> None:
    """Molex 2036150003: steel shell, black sealing flange, Type-C mouth.

    Every number from the Product Customer Drawing (PSD 000 rev A,
    2022-04-14), sheet 1: 9,99 +-0,12 across the sealing flange by 8,58
    deep, 4,21 +-0,12 above the mounting surface, the main shell 3,56 tall
    and standing 0,20 off the board, the mouth 8,34 +0,06/-0,02 by
    2,56 +-0,04. The corrugated front flange is a silica rubber sealing
    ring 1,15 mm thick, black, and it is what makes the part IPX8 - drawing
    the receptacle as one steel box throws away the only feature that
    explains why this connector was chosen.

    Colours from the bill of materials on sheet 2: shells in stainless
    steel, housing in black glass filled nylon, the seal in black silica
    rubber.
    """
    ACO = (0.76, 0.77, 0.79)             # aco inoxidavel
    VEDACAO = (0.10, 0.10, 0.11)         # borracha de silica preta
    NYLON = (0.08, 0.08, 0.09)           # nylon com fibra de vidro, preto
    a1 = 0.20                            # a carcaca fica 0,20 mm da placa
    corpo_alt = 3.56
    selo = 1.15
    boca_w, boca_h = 8.34, 2.56
    partes = [
        # a carcaca principal, recuada do selo
        _bloco(-w / 2 + 0.3, -h / 2, a1, w / 2 - 0.3, h / 2 - selo,
               a1 + corpo_alt, ACO),
        # o anel de vedacao, na frente, mais largo e mais alto: e ele que da
        # os 9,99 de largura e os 4,21 de altura
        _bloco(-w / 2, h / 2 - selo, 0.0, w / 2, h / 2, alt, VEDACAO),
    ]
    # a boca, aberta atraves do selo
    partes.append(_bloco(-boca_w / 2, h / 2 - selo - 0.2,
                         a1 + (corpo_alt - boca_h) / 2,
                         boca_w / 2, h / 2 + 0.01,
                         a1 + (corpo_alt + boca_h) / 2, NYLON))
    # a lingueta, dentro da boca
    partes.append(_bloco(-boca_w / 2 + 0.6, h / 2 - selo - 2.6,
                         a1 + corpo_alt / 2 - 0.35,
                         boca_w / 2 - 0.6, h / 2 - selo + 0.01,
                         a1 + corpo_alt / 2 + 0.35, NYLON))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        "# Molex 2036150003: carcaca de aco, anel de vedacao preto e boca "
        "Type-C de 8,34 x 2,56" + NL + "".join(partes),
        encoding="utf-8", newline=NL)


DESENHADOS = {
    "gnssbike:MinewSemi_ME54BS13_16.5x12mm":
        lambda c, w, h, a: wrl_me54bs13(c),
    "gnssbike:u-blox_MAX_LCC-18_9.7x10.1mm": wrl_max_f10s,
    "gnssbike:SW_TS-1088R_3.9x3mm": wrl_tecla_ts1088,
    "gnssbike:Buzzer_PKLCS1212E4001_12x12mm": wrl_buzzer_pklcs,
    # wrl_usb_c desenha o Molex 2036150003, que saiu da lista em
    # 2026-09-24 (309 unidades no mundo). O HRO TYPE-C-31-M-12 que
    # entrou no lugar ainda nao teve o desenho cotado lido, e desenhar
    # o corpo do Molex com o nome do HRO seria pior do que nao desenhar:
    # sem entrada aqui, o 3D usa o contorno F.Fab do proprio footprint
    # do KiCad, que e o contorno do encapsulamento certo.
    "gnssbike:ContatoMola_2x2mm_P3mm": wrl_mola,
    "gnssbike:LED_RGB_3528_3.5x2.8mm": wrl_led_3528,
    # Only the TE one. The Hirose FH12 has a model in KiCad's own library and
    # trocar_modelo() keeps it, which is right: a maker's model beats a sketch.
    "Connector_FFC-FPC:TE_0-1734839-5_1x05-1MP_P0.5mm_Horizontal": wrl_fpc,
}


# The mechanical drawing of each package, read in the part's own datasheet.
# Not the footprint outline, which is a land pattern and is deliberately
# bigger than the body; not "the usual value for the package" either. Each
# entry is: body w x h x A, standoff A1, exposed pad (or None), terminal
# width b, terminal length L, pitch e, and where the numbers come from.
#
# w and h follow the FOOTPRINT's orientation, which is not always the
# datasheet's: the TPD4E05U06 drawing gives 2.50 x 1.00 and the footprint
# stands it on end. conferir_2d_3d() compares the two ignoring orientation,
# so a transposition shows up as a match and a wrong package does not.
PACOTE: dict[str, tuple] = {
    "Package_DFN_QFN:QFN-28-1EP_4x4mm_P0.4mm_EP2.3x2.3mm":
        (4.00, 4.00, 0.80, 0.00, (2.60, 2.60), 0.20, 0.40, 0.40,
         "e-peas DS-AEM1090x-v2.4.0, figura 29: QFN28 4x4"),
    "Package_SON:Texas_X2SON-4_1x1mm_P0.65mm":
        (1.00, 1.00, 0.48, 0.03, None, 0.28, 0.30, 0.65,
         "TI SBVS277C, desenho do DQN0004A: 1,05/0,95 x 1,05/0,95, "
         "altura 0,48 +0,12/-0,10"),
    "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm":
        (3.00, 2.50, 0.75, 0.03, (1.50, 1.00), 0.25, 0.40, 0.50,
         "TI SCES936A, desenho do BQA0014A: 3,1/2,9 x 2,6/2,4, altura 0,8/0,7, "
         "pad exposto 1,6/1,4 x 1,1/0,9"),
    "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm":
        (2.00, 2.00, 0.60, 0.03, (0.65, 1.35), 0.30, 0.30, 0.65,
         "TI SBOS681B, desenho do DNP0006A: 2,1/1,9 quadrado, altura 0,65/0,55, "
         "pad exposto 0,65 x 1,35"),
    "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm":
        (1.00, 2.50, 0.40, 0.03, None, 0.20, 0.36, 0.50,
         "TI, desenho do DQA0010A: 2,6/2,4 x 1,1/0,9, altura 0,45/0,35"),
    "gnssbike:BMP585_LGA-8_3.25x3.25mm":
        (3.25, 3.25, 1.86, 0.00, None, 0.30, 0.35, 0.80,
         "Bosch BST-BMP585-DS003-02, tabela 3: contorno 3,25 x 3,25 tipico "
         "(3,2 a 3,3), ALTURA 1,86 tipica, 1,76 a 1,96"),
    "gnssbike:MMC5603_WLP-4_0.82x0.82mm":
        (0.82, 0.82, 0.40, 0.14, None, 0.22, 0.22, 0.40,
         "MEMSIC MMC5603NJ Rev. B (2018-07-12), pagina 17: corpo 0,82 +-0,03 "
         "quadrado, dado de 0,40 +-0,03, esferas saindo 0,14 +-0,03 por "
         "baixo - altura TOTAL 0,54, e nao os 0,40 que os catalogos citam. "
         "Esfera o0,22 +-0,03, passo 0,40 em x e em y. Face de cima "
         "cinza-escura quase preta com o ponto de A1; paredes serradas "
         "cinza-claras. A cor nao esta na ficha: e medicao de foto"),
    "gnssbike:LED_RGB_3528_3.5x2.8mm":
        (3.50, 2.80, 1.90, 0.00, None, 0.80, 0.785, 1.50,
         "TUOZHAN S4-3528RGBTA-A, ficha S35210052 (2021-02-21): 3,5 +-0,2 "
         "por 2,8 +-0,2 por 1,9 de altura, resina do topo 3,2 +-0,2 "
         "afunilando para 3,5 na base, seccao reta de baixo com 1,05, "
         "cavidade refletora de 2,64 x 2,14, terminais de 0,80 x 0,785. "
         "Lente 'water clear' TRANSPARENTE INCOLOR, declarada; corpo branco "
         "inferido da foto, porque a ficha nao declara. Canto chanfrado "
         "junto ao pino 4, o vermelho"),
    "Package_DFN_QFN:QFN-32-1EP_5x5mm_P0.5mm_EP3.45x3.45mm":
        (5.00, 5.00, 0.90, 0.035, (3.50, 3.50), 0.25, 0.40, 0.50,
         "Nordic nPM1300 Product Specification v1.1 (4490_483, 2024-06-16), "
         "pagina 154, figura 52 e tabela 37: D e E 5,0 nominais, A 0,8/0,85/"
         "0,9, A1 0/0,035/0,05, D2 e E2 3,4/3,5/3,6, b 0,2/0,25/0,3, "
         "L 0,3/0,4/0,45, e 0,5. A2 so nominal 0,815; A3 nao consta"),
    "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm":
        (1.448, 1.468, 0.64, 0.19, None, 0.27, 0.27, 0.40,
         "Maxim, desenho de encapsulamento 21-100168 Rev A (W91G1+2), "
         "COMMON DIMENSIONS: D 1,448 +-0,025 e E 1,468 +-0,025 medidos pelas "
         "linhas de centro entre os cortes, A 0,64 +-0,05, A1 (altura da "
         "esfera) 0,19 +-0,03, esfera 0,27 +-0,03 de diametro, e 0,40 BASIC, "
         "matriz 3x3 cheia (DEPOPULATED BUMPS: NONE), D1 e E1 0,80 BASIC"),
    "gnssbike:ESD761_X1SON-2_1x0.6mm":
        (1.00, 0.60, 0.45, 0.025, None, 0.25, 0.50, 0.65,
         "TI SLVSH10C (Rev C, 2025-11-09), pagina 21, PACKAGE OUTLINE "
         "DPY0002A X1SON, desenho 4224561/C 07/2024: A 0,9 a 1,1, B 0,5 a "
         "0,7, C maximo 0,45 (nominal nao consta), standoff 0 a 0,05, "
         "terminais 0,2 a 0,3 por 0,45 a 0,55, passo 0,65 basico"),
    "gnssbike:Buzzer_PKLCS1212E4001_12x12mm":
        (12.00, 12.00, 3.00, 0.00, None, 1.20, 4.00, 11.20,
         "Murata PKLCS1212E4001-R1, especificacao JGB40-1584B (2021-05-21), "
         "item 9: corpo 12,0 x 12,0 +-0,2 por 3,0 de altura MAXIMA. "
         "Eletrodos de 4,0 de comprimento centrados na aresta, subindo 1,2 "
         "pela parede e avancando 1,0 sobre a face inferior; pads de 1,2 x "
         "4,0 com vao externo 12,4 e interno 10,0. FURO DE SOM LATERAL: "
         "fenda de (1,9) x (0,9) numa parede sem eletrodo, aresta de baixo a "
         "0,30 do plano de assento, e canal de (6,4) x (2,1) x (0,7) na face "
         "inferior saindo pela parede oposta - a caixa precisa de duto "
         "lateral, nao de furo na tampa. Cinza-grafite escuro fosco e "
         "uniforme, terminais DOURADOS; a Murata nao declara cor, e isto e "
         "medicao de foto. Sem polaridade. Pede resistor de serie de 1 k a "
         "2 k ou diodo em paralelo, e PROIBE DC (item 12-4)"),
    "Connector_JST:JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal":
        (10.75, 4.05, 4.25, 0.10, None, 0.50, 0.80, 1.25,
         "JST, catalogo da serie GH (eGH.pdf), pagina 3, bloco Header, linha "
         "SM06B-GHS-TB: B 10,75 de largura total, A 6,25 entre os pinos "
         "extremos (5 x 1,25), corpo de 4,05 de profundidade, altura 4,25, "
         "folga corpo-placa 0,1, rabichos SMT saindo 0,8 pela face de tras. "
         "Corpo de PA natural (marfim), contatos e reforcos de liga de cobre "
         "estanhada"),
    # O Molex 2036150003 saiu da lista em 2026-09-24: 309 unidades no
    # mundo a US$ 5,69, contra 91.943 a US$ 0,186 do HRO TYPE-C-31-M-12
    # (LCSC C165948), que e o USB-C mais usado do catalogo da JLCPCB.
    # As cotas do HRO ainda nao foram lidas em desenho cotado, entao
    # nao ha entrada aqui: uma cota inventada passaria pelo ME3 sem
    # dizer nada. Corpo de 7,35 mm segundo a parametria da LCSC contra
    # 8,58 do Molex, e a abertura da caixa muda com isso.
    "Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal":
        (8.10, 5.60, 2.00, 0.00, None, 0.30, 1.30, 0.50,
         "8,10 e o CORPO ISOLANTE, que e o que o F.Fab do KiCad desenha; a "
         "largura total com as duas abas metalicas e 9,1 +-0,3 e o land "
         "pattern se estende por 10,1. As tres medidas sao da ficha, e "
         "descrevem coisas diferentes. "
         "Hirose EDC3-150229-11 (code CL586-0522-3-55), a ficha da peca que "
         "esta na lista: largura total 9,1 +-0,3, corpo isolante 8,1 +-0,2, "
         "profundidade 5,6 +-0,3 (6,4 com as caudas), ALTURA 2,00 +-0,2 "
         "fechado. Ilha de contato 0,30 +-0,03 por 1,30 +-0,1 a passo 0,50, "
         "ilha de fixacao 1,80 x 2,20 com centros em +-4,15, vao entre as "
         "fileiras 1,50. FPC de 0,30 +-0,05, CONTATO INFERIOR. Alojamento de "
         "poliamida BEGE, atuador de PPS MARROM ESCURO, contatos dourados "
         "(o sufixo 55 e a versao de ouro). Substituiu o FH28-10S-0.5SH, que "
         "tem 2,55 de altura contra o teto de 2,6 mm da sombra do display"),
    "LED_SMD:LED_0603_1608Metric":
        (1.60, 0.80, 0.75, 0.00, None, 0.30, 0.30, 1.00,
         "Kingbright APT1608SURCK, spec DSAD0926 rev V.22A (2023-04-08), "
         "pagina 1, PACKAGE DIMENSIONS: 1,6 x 0,8 x 0,75, base de 0,25 e a "
         "resina em tronco trapezoidal de 1,2 na base para 1,1 no topo, topo "
         "chato, terminais de 0,3 em cada ponta subindo pelas laterais. "
         "Lente Water Clear (transparente, nao difusa); a COR DO CORPO nao "
         "consta na ficha"),
    "Package_SO:SOIC-8_5.275x5.275mm_P1.27mm":
        (5.23, 5.28, 2.16, 0.15, None, 0.41, 0.65, 1.27,
         "Macronix MX25R6435F v1.6, pagina 79, 19 PACKAGE INFORMATION, "
         "Package Outline for SOP 8L 200MIL: D 5,23 (5,13-5,33), E1 5,28 "
         "(5,18-5,38), A maximo 2,16 e nominal 1,95, A1 0,15 (0,05-0,20), "
         "b 0,41, L 0,65, e 1,27; com os terminais E 7,90. Sem pad exposto: "
         "essa nota e do WSON, pagina 80"),
    "Package_LGA:Bosch_LGA-14_3x2.5mm_P0.5mm":
        (3.00, 2.50, 0.87, 0.13, None, 0.25, 0.475, 0.50,
         "Bosch BST-BMI270-DS000, pagina 143, 8.1 Package outline dimensions: "
         "D 3,00 (2,95-3,05), E 2,50 (2,45-2,55), A maximo 0,87 e nominal "
         "0,83, A1 0,13, e 0,50 BSC nos dois eixos; os oito pads laterais sao "
         "0,475 x 0,250 e os seis de topo 0,250 x 0,475, recuados L1 0,100 da "
         "aresta (metallized pad detail, mesma pagina)"),
    "Package_TO_SOT_SMD:SOT-523":
        (1.60, 0.80, 0.75, 0.05, None, 0.22, 0.33, 0.50,
         "Diodes DS31783 Rev.8, SOT523: D 1,60, E1 0,80, A2 0,75, A1 0,05, "
         "b 0,22, e 0,50 BSC"),
}


def conferir_2d_3d() -> list[str]:
    """Does the body the datasheet gives fit the footprint that was drawn?

    A land pattern is bigger than the body, on purpose, so the two are never
    equal - but they cannot disagree by much either, and a footprint chosen
    for the wrong package shows up here as a body that does not fit inside
    its own outline or that rattles around in it. Orientation is ignored,
    because a footprint may stand the package on end.
    """
    import fp_load as _fl

    achados = []
    for nome, dados in PACOTE.items():
        w, h, alt = dados[0], dados[1], dados[2]
        fab = fab_do_footprint(nome)
        if fab is None:
            achados.append(f"{nome}: o footprint nao tem contorno em F.Fab")
            continue
        corpo = tuple(sorted((w, h)))
        desenho = tuple(sorted(fab))
        for i, (c, d) in enumerate(zip(corpo, desenho)):
            if abs(c - d) > 0.15:
                achados.append(
                    f"{nome}: a ficha da {'menor' if i == 0 else 'maior'} "
                    f"medida do corpo como {c:.2f} mm e o footprint desenha "
                    f"{d:.2f} mm ({abs(c - d):.2f} de diferenca)")
        cy = _fl.CAIXA.get(nome)
        if cy and (w > cy[2] - cy[0] + 0.01 or h > cy[3] - cy[1] + 0.01):
            achados.append(f"{nome}: o corpo de {w:.2f} x {h:.2f} nao cabe no "
                           f"contorno de {cy[2]-cy[0]:.2f} x {cy[3]-cy[1]:.2f}")
    return achados


def wrl_ci(caminho, w: float, h: float, alt: float, estilo: str,
           nome: str = "") -> None:
    """A moulded package with the shape its family actually has.

    A board where every part is the same black cuboid tells you nothing. What
    distinguishes these packages at a glance is where the metal is: a QFN
    shows its exposed pad and a ring of lead flags underneath, a SOIC has
    gull-wing leads standing out on two sides, a SOT has three of them, and a
    wafer-level package is bare silicon with a grid of solder balls. All of
    it is drawn from the same outline the footprint already carries, so
    nothing here is invented dimension - only which part of it is metal.
    """
    METAL = (0.72, 0.73, 0.75)
    EPOXI = (0.09, 0.09, 0.10)
    SILICIO = (0.24, 0.21, 0.28)
    partes = []
    # the datasheet's own numbers when there are any: body, standoff,
    # exposed pad, terminal width and length, pitch
    dados = PACOTE.get(nome)
    a1, ep, bw, bl, passo_t = 0.03, None, 0.25, 0.35, 0.5
    if dados:
        w, h, alt, a1, ep, bw, bl, passo_t = dados[:8]

    if estilo == "wlp":
        # bare die, with the ball grid under it
        partes.append(_bloco(-w / 2, -h / 2, 0.12, w / 2, h / 2, alt, SILICIO))
        passo = 0.4
        nx = max(1, int(w / passo))
        ny = max(1, int(h / passo))
        for i in range(nx):
            for j in range(ny):
                bx = -w / 2 + passo / 2 + i * passo
                by = -h / 2 + passo / 2 + j * passo
                partes.append(_cilindro(bx, by, 0.0, 0.14, 0.11, METAL, 8))
    elif estilo == "soic":
        # body raised on its leads, with the gull wings on two sides
        corpo_alt = alt - 0.15
        partes.append(_bloco(-w / 2 + 0.9, -h / 2, 0.15, w / 2 - 0.9, h / 2,
                             corpo_alt, EPOXI))
        n = max(2, int(h / 1.27))
        for i in range(n):
            by = -h / 2 + h * (i + 0.5) / n
            for lado in (-1, 1):
                x0 = lado * (w / 2 - 0.9)
                x1 = lado * (w / 2)
                partes.append(_bloco(min(x0, x1), by - 0.2, 0.0,
                                     max(x0, x1), by + 0.2, 0.15, METAL))
    elif estilo == "sot":
        corpo_alt = alt - 0.1
        partes.append(_bloco(-w / 2, -h / 2 + 0.25, 0.10, w / 2, h / 2 - 0.25,
                             corpo_alt, EPOXI))
        for bx, by in ((-w / 4, -h / 2 + 0.12), (w / 4, -h / 2 + 0.12),
                       (0.0, h / 2 - 0.12)):
            partes.append(_bloco(bx - 0.18, by - 0.15, 0.0,
                                 bx + 0.18, by + 0.15, 0.10, METAL))
    else:
        # qfn, dfn, son, lga: a moulded body with metal underneath
        partes.append(_bloco(-w / 2, -h / 2, a1, w / 2, h / 2, a1 + alt, EPOXI))
        if ep:
            partes.append(_bloco(-ep[0] / 2, -ep[1] / 2, 0.0,
                                 ep[0] / 2, ep[1] / 2, a1 + 0.02, METAL))
        # the terminals, at the pitch and the size the drawing gives
        for eixo, comp in ((0, w), (1, h)):
            n = max(1, int(round((comp - bw) / passo_t)))
            for i in range(n + 1):
                d = -comp / 2 + bw / 2 + i * passo_t
                if d > comp / 2 - bw / 2 + 1e-6:
                    break
                for lado in (-1, 1):
                    if eixo == 0:
                        bx0, bx1 = d - bw / 2, d + bw / 2
                        by = lado * (h / 2 - bl / 2)
                        by0, by1 = by - bl / 2, by + bl / 2
                    else:
                        by0, by1 = d - bw / 2, d + bw / 2
                        bx = lado * (w / 2 - bl / 2)
                        bx0, bx1 = bx - bl / 2, bx + bl / 2
                    partes.append(_bloco(bx0, by0, 0.0, bx1, by1,
                                         a1 + 0.02, METAL))

    # pin 1, as the dot the real package carries
    partes.append(_cilindro(-w / 2 + 0.35, -h / 2 + 0.35, alt, alt + 0.02,
                            min(0.22, w / 8), (0.45, 0.45, 0.47), 10))
    caminho.write_text(
        "#VRML V2.0 utf8" + NL +
        f"# encapsulamento {estilo}, {w:.2f} x {h:.2f} x {alt:.2f} mm" + NL +
        "".join(partes), encoding="utf-8", newline=NL)


ESTILO = (
    ("QFN", "qfn"), ("DFN", "qfn"), ("SON", "qfn"), ("LGA", "qfn"),
    ("SOIC", "soic"), ("SO-", "soic"),
    ("SOT", "sot"),
    ("WLP", "wlp"), ("WLCSP", "wlp"),
)


def estilo_de(nome: str) -> str | None:
    for chave, est in ESTILO:
        if chave.lower() in nome.lower():
            return est
    return None


def _com_modelo() -> None:
    """Give every generated footprint a body, and reference it.

    The box is the package outline of CORPO, raised to the height of the part.
    It goes into 3d/ as VRML, which is what KiCad's 3D viewer reads; the
    project's own renderer draws the same box from the same table, because
    KiCad's GLB and STEP exports read only STEP models.
    """
    import pathlib as _pl

    pasta = _pl.Path(__file__).resolve().parent / "3d"
    pasta.mkdir(exist_ok=True)
    for nome, (w, h, alt) in CORPO.items():
        if nome not in GERADOS:
            continue
        # PACOTE wins here too: CORPO is the outline this file drew the
        # footprint from, PACOTE is what the mechanical drawing cotes.
        if nome in PACOTE:
            w, h, alt = PACOTE[nome][0], PACOTE[nome][1], PACOTE[nome][2]
        CORPO_TODOS[nome] = (w, h, alt)
        base = nome.split(":", 1)[1]
        est = estilo_de(nome)
        if nome in DESENHADOS:
            DESENHADOS[nome](pasta / (base + ".wrl"), w, h, alt)
        elif est:
            wrl_ci(pasta / (base + ".wrl"), w, h, alt, est, nome)
        else:
            wrl_caixa(pasta / (base + ".wrl"), w, h, alt, cor=cor_de(nome))
        modelo = (
            '\t(model "${KIPRJMOD}/3d/' + base + '.wrl"\n'
            '\t\t(offset (xyz 0 0 0))\n'
            '\t\t(scale (xyz 1 1 1))\n'
            '\t\t(rotate (xyz 0 0 0))\n'
            '\t)\n')
        texto = GERADOS[nome]
        GERADOS[nome] = texto[:texto.rindex(")")] + modelo + ")\n"



# the generated ones, assigned
_fp("U102", "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm", "GERADO", "")
_fp("U504", "gnssbike:MMC5603_WLP-4_0.82x0.82mm", "GERADO",
    "Memsic MMC5603NJ: o MMC5633NJL nao existe na LCSC. Mesma familia, "
    "mesmo 0x30, mesmo 0x10 no registrador 0x39 - o driver nao muda")
_fp("U505", "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm", "GERADO", "")
_fp("D101", "gnssbike:ESD761_X1SON-2_1x0.6mm", "GERADO", "")
_fp("D102", "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm", "GERADO", "")
_fp("U502", "gnssbike:BMP585_LGA-8_3.25x3.25mm", "GERADO", "")
_fp("U302", "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm", "GERADO", "")
_fp("U301", "gnssbike:u-blox_MAX_LCC-18_9.7x10.1mm", "GERADO", "")
_fp("D601", "gnssbike:LED_RGB_3528_3.5x2.8mm", "GERADO",
    "TUOZHAN S4-3528RGBTA-A: o sufixo do APTF1616 do projeto nao existe "
    "mais e o que existe tem 305 pecas. Este tem 2 milhoes, e anodo comum "
    "como o projeto quer, e custa 1/20. RECALCULAR os resistores de serie")


def me54bs13() -> str:
    """MinewSemi ME54BS13, from the mechanical drawing of datasheet V1.0.0.

    Body 12.00 x 16.50 mm, antenna on one 12 mm end. Twenty castellated pads
    on the long sides at 1.1 mm pitch and a 60 pad LGA matrix of 0.6 mm pads
    at 1.5 x 1.2 mm pitch. The datasheet's origin is the bottom left corner
    with the antenna at +Y; the footprint's is the body centre with the
    antenna at -Y, which is up on the screen.

    Minew does not publish a land pattern - the datasheet says to ask for it -
    so the pads here are the module's own pads with the usual allowance:
    1.50 mm across the edge by 0.70 mm along it for the castellated ones,
    half inside the body and half outside, and 0.65 mm circles for the matrix.
    The 0.70 is what has to run along the edge: the pitch there is 1.10 mm,
    so a 1.50 mm pad along it would overlap its neighbour.
    """
    W, H = 12.00, 16.50
    ANT = 4.46                      # antenna band, measured from the +Y end
    ys_cast = [1.10 + i * 1.10 for i in range(10)]
    xs_lga = [2.25 + i * 1.50 for i in range(6)]
    ys_lga = [0.90 + i * 1.20 for i in range(10)]

    def fy(y_ds: float) -> float:
        return H / 2.0 - y_ds       # datasheet Y up, footprint Y down

    pads = []
    # 1..10 down the left side, 11..20 up the right side
    for i in range(10):
        pads.append(_pad(str(i + 1), -W / 2.0, fy(ys_cast[9 - i]), 1.50, 0.70,
                         forma="rect"))
    for i in range(10):
        pads.append(_pad(str(11 + i), W / 2.0, fy(ys_cast[i]), 1.50, 0.70,
                         forma="rect"))
    for c, letra in enumerate("ABCDEF"):
        for linha in range(10):
            pads.append(_pad(f"{letra}{linha}", xs_lga[c] - W / 2.0,
                             fy(ys_lga[9 - linha]), 0.65, 0.65, forma="circle"))

    corpo = _corpo("gnssbike:MinewSemi_ME54BS13_16.5x12mm", W, H, pads,
                   "MinewSemi ME54BS13, nRF54LM20A; 20 pads castelados e 60 LGA. "
                   "Land pattern NAO oficial: a Minew fornece o dela sob pedido")
    # the antenna band, and the 4 mm the datasheet asks for around the RF side
    faixa = (f'\t(fp_rect\n\t\t(start {-W / 2.0:.3f} {-H / 2.0:.3f})\n'
             f'\t\t(end {W / 2.0:.3f} {-H / 2.0 + ANT:.3f})\n'
             '\t\t(stroke (width 0.12) (type dash))\n\t\t(fill none)\n'
             '\t\t(layer "Dwgs.User")\n'
             f'\t\t(uuid "{_uid("me54", "ant")}")\n\t)\n'
             f'\t(fp_text user "antena: sem cobre, 4 mm livres, virada para a borda"\n'
             f'\t\t(at 0 {-H / 2.0 - 1.0:.3f} 0)\n\t\t(layer "Dwgs.User")\n'
             f'\t\t(uuid "{_uid("me54", "txt")}")\n'
             '\t\t(effects (font (size 0.6 0.6) (thickness 0.1)))\n\t)\n')
    return corpo[:-2] + faixa + ")\n"


GERADOS["gnssbike:MinewSemi_ME54BS13_16.5x12mm"] = me54bs13()


def contato_mola() -> str:
    """Two gold pads a spring presses on. No paste: nothing is soldered.

    This is how the antenna in the case wall and the three groups of solar
    modules reach the board. 2.0 x 2.0 mm gives a spring plenty of landing
    area even with the tolerance of a moulded case, and 3.0 mm of pitch
    keeps the two contacts apart by more than a spring can wander.
    """
    pads = [_pad("1", -1.5, 0.0, 2.0, 2.0, forma="rect",
                 camadas='"F.Cu" "F.Mask"'),
            _pad("2", 1.5, 0.0, 2.0, 2.0, forma="rect",
                 camadas='"F.Cu" "F.Mask"')]
    return _corpo("gnssbike:ContatoMola_2x2mm_P3mm", 5.6, 2.6, pads,
                  "contato de mola de 2 vias, sem pasta de solda")


def buzzer_cpt1117() -> str:
    """Same Sky CPT-1117-83-SMT, do desenho mecanico da propria ficha.

    The footprint in use was `Buzzer_CUI_CPT-9019S-SMT`, which is a ROUND
    9 mm part. The one on the shopping list is 11,0 x 9,0 x 1,7 mm and
    RECTANGULAR, and it is soldered by two flat metal tabs, one at each end
    and on opposite sides of the axis, 2,0 mm wide and 0,2 mm thick, each
    with a 0,8 mm hole. The land pattern the datasheet recommends is two
    2,5 x 2,5 mm pads 10,5 mm apart, centre to centre - the body itself
    rests on nothing. Drawn here because that is not the same shape as a
    9 mm circle and no amount of courtyard makes it one.
    """
    pads = [_pad("1", -5.25, 0.0, 2.5, 2.5, forma="rect"),
            _pad("2", 5.25, 0.0, 2.5, 2.5, forma="rect")]
    return _corpo("gnssbike:Buzzer_CPT-1117-83-SMT_11x9mm", 11.0, 9.0, pads,
                  "buzzer piezo SMD 11,0 x 9,0 x 1,7, duas abas a 10,5 mm")


def jumper_rf() -> str:
    """Three pads at 0402 pitch: a 0 ohm bridges the middle to one side.

    The geometry is the KiCad 0402 land pattern repeated - pads of
    0,59 x 0,66 mm with 0,975 mm between centres - so an ordinary 0402 0 ohm
    fits either pair. Nothing here is a number from a datasheet, because
    there is no part: it is a land pattern for a jumper, and it is drawn from
    the 0402 the rest of the board already uses.
    """
    pads = [_pad("1", -0.975, 0.0, 0.59, 0.66, forma="roundrect"),
            _pad("2", 0.0, 0.0, 0.59, 0.66, forma="roundrect"),
            _pad("3", 0.975, 0.0, 0.59, 0.66, forma="roundrect")]
    return _corpo("gnssbike:JumperRF_3x0402", 2.56, 0.66, pads,
                  "escolha de antena: 0 ohm entre o pad do meio e um dos lados")


GERADOS["gnssbike:JumperRF_3x0402"] = jumper_rf()
GERADOS["gnssbike:Buzzer_CPT-1117-83-SMT_11x9mm"] = buzzer_cpt1117()
GERADOS["gnssbike:ContatoMola_2x2mm_P3mm"] = contato_mola()
_fp("U201", "gnssbike:MinewSemi_ME54BS13_16.5x12mm", "GERADO",
    "modulo de radio; a Minew nao publica land pattern")

# every generated footprint gets its body last, once they all exist
_com_modelo()


def resumo() -> str:
    from collections import Counter
    c = Counter(v[1] for v in FP.values())
    return (f"{len(FP)} pecas com footprint: "
            + ", ".join(f"{k} {v}" for k, v in sorted(c.items()))
            + f"; {len(GERADOS)} footprints gerados aqui; "
            + f"{len(FORA_DA_PLACA)} pecas fora da placa")


if __name__ == "__main__":
    import parts as P
    print(resumo())
    faltam = [r for r in P.PARTS if r not in FP and r not in FORA_DA_PLACA]
    print(f"sem footprint: {len(faltam)}")
    if faltam:
        print("  " + ", ".join(sorted(faltam)))
