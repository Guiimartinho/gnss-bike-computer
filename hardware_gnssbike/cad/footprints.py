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
FORA_DA_PLACA = {"DS401", "E301", "PV101", "PV102", "PV103", "PV104", "PV105",
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
_fp("U501", "Package_SO:SOIC-8_5.23x5.23mm_P1.27mm", "ENCAPSULAMENTO",
    "MX25R6435F em SOP-8 de 200 mil")
_fp("U503", "Package_LGA:Bosch_LGA-14_3x2.5mm_P0.5mm", "EXATO",
    "BMI270, LGA-14 da Bosch")
_fp(["Q401", "Q601", "Q602", "Q603"], "Package_TO_SOT_SMD:SOT-523",
    "ENCAPSULAMENTO", "DMG1012T-7; a Diodes nao numera os terminais")

# ---------------------------------------------------------------- conectores
_fp("J101", "Connector_USB:USB_C_Receptacle_Palconn_UTC16-G", "ENCAPSULAMENTO",
    "USB-C de 16 contatos; os nomes de pad batem com o padrao USB-IF. "
    "O Molex 2036150003 e IPX8 e tem furos de blindagem proprios")
_fp("J102", "Connector_JST:JST_GH_SM06B-GHS-TB_1x06-1MP_P1.25mm_Horizontal",
    "EXATO", "JST GH de 6 vias, entrada lateral")
_fp("J201", "Connector:Tag-Connect_TC2030-IDC-NL_2x03_P1.27mm_Vertical", "EXATO",
    "TC2030-NL, so furos e pads")
_fp("J401", "Connector_FFC-FPC:Hirose_FH12-10S-0.5SH_1x10-1MP_P0.50mm_Horizontal",
    "ENCAPSULAMENTO",
    "10 vias, passo 0,5 mm, contato por baixo. A peca escolhida e o FH28-10S-"
    "0.5SH(05), que a propria JDI recomenda; a KiCad so tem o FH12")
_fp("J402", "Connector_FFC-FPC:TE_0-1734839-5_1x05-1MP_P0.5mm_Horizontal",
    "ENCAPSULAMENTO",
    "5 vias, passo 0,5 mm. A peca e o Molex 503480-0500, que a JDI nomeia no "
    "desenho de contorno; a KiCad nao tem essa serie")
_fp(["TP201", "TP202", "TP203"], "TestPoint:TestPoint_Pad_D1.0mm", "EXATO", "")

# ---------------------------------------------------------------- interface
_fp(["SW601", "SW602", "SW603"], "Button_Switch_SMD:SW_SPST_B3S-1000", "EXATO",
    "Omron B3S; o footprint junta os terminais 1-2 num pad e 3-4 no outro")
_fp(["D103", "D104"], "LED_SMD:LED_0603_1608Metric", "ENCAPSULAMENTO",
    "Kingbright APT1608SURCK, 1,6 x 0,8 mm")
_fp("LS601", "Buzzer_Beeper:Buzzer_CUI_CPT-9019S-SMT", "ENCAPSULAMENTO",
    "buzzer piezo SMD; o CPT-1117-83-SMT da lista nao esta na KiCad")


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
    "gnssbike:u-blox_MAX-F10S_9.7x10.1mm": (9.70, 10.10, 2.40),
    "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm": (1.40, 1.40, 0.50),
    "gnssbike:BMP585_LGA-8_3.25x3.25mm": (3.25, 3.25, 1.96),
    "gnssbike:MMC5633_WLP-4_0.85x0.85mm": (0.85, 0.85, 0.40),
    "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm": (2.00, 2.00, 0.65),
    "gnssbike:ESD761_X1SON-2_1x0.6mm": (1.00, 0.60, 0.45),
    "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm": (1.00, 2.50, 0.55),
    "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm": (3.00, 2.50, 0.80),
    "gnssbike:LED_RGB_APTF1616_1.6x1.6mm": (1.60, 1.60, 0.70),
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
                descr, primeiro_lado="R"):
    """A module: pads down one edge and up the other, 1..n."""
    pads = []
    y0 = -(n_por_lado - 1) * pitch / 2.0
    for i in range(n_por_lado):
        pads.append(_pad(str(i + 1), span / 2.0, -y0 - i * pitch, pad_w, pad_h))
    for i in range(n_por_lado):
        pads.append(_pad(str(n_por_lado + i + 1), -span / 2.0, y0 + i * pitch,
                         pad_w, pad_h))
    return _corpo(nome, corpo_w, corpo_h, pads, descr)


GERADOS: dict[str, str] = {}


def _gerar():
    # MAX17262 WLP-9: 3 x 3 balls, 0.4 mm pitch, package 1.4 x 1.4 mm.
    GERADOS["gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm"] = wlp(
        "gnssbike:MAX17262_WLP-9_1.4x1.4mm_P0.4mm", 3, 3, 0.4, 0.20, 1.4, 1.4,
        "MAX17262 WLP de 9 bolas; land pattern aproximado, "
        "a ficha recomendada nao foi lida")
    # MMC5633NJL WLP-4: 0.85 x 0.85 mm, 2 x 2 balls.
    GERADOS["gnssbike:MMC5633_WLP-4_0.85x0.85mm"] = wlp(
        "gnssbike:MMC5633_WLP-4_0.85x0.85mm", 2, 2, 0.4, 0.20, 0.85, 0.85,
        "MMC5633NJL WLP de 4 bolas; land pattern aproximado")
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
    # MAX-F10S: MAX form factor, 9.7 x 10.1 mm, 18 pads, 9 per edge.
    GERADOS["gnssbike:u-blox_MAX-F10S_9.7x10.1mm"] = duas_bordas(
        "gnssbike:u-blox_MAX-F10S_9.7x10.1mm", 9, 1.1, 1.4, 0.8, 9.0,
        9.7, 10.1, "u-blox MAX-F10S; 18 pads, 9 por borda. Passo e tamanho de "
                   "pad aproximados: o desenho de montagem nao foi lido")
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
    "Button_Switch_SMD:SW_SPST_B3S-1000": (5.00, "Omron B3S-1002P: corpo de "
        "3,5 mm mais o botao ate 5,0 - CONFERIR na ficha da Omron"),
    "Connector_USB:USB_C_Receptacle_Palconn_UTC16-G": (3.26, "altura corrente "
        "de um receptaculo USB-C de montagem em superficie - CONFERIR na "
        "ficha do Molex 2036150003, que e a peca da lista de compras"),
    "Buzzer_Beeper:Buzzer_CUI_CPT-9019S-SMT": (3.00, "CONFERIR: a lista de "
        "compras traz o CPT-1117-83-SMT, nao o CPT-9019S deste footprint"),
    "Connector_FFC-FPC:TE_0-1734839-5_1x05-1MP_P0.5mm_Horizontal": (1.20,
        "conector FPC horizontal de passo 0,5 - CONFERIR: a peca ainda nao "
        "esta escolhida (06#j402)"),
    # the flat ones, all well under the 2,6 mm the display leaves
    "Package_TO_SOT_SMD:SOT-523": (0.60, "altura normal do SOT-523"),
    "Package_SON:Texas_X2SON-4_1x1mm_P0.65mm": (0.40, "altura normal do X2SON"),
    "Package_DFN_QFN:QFN-28-1EP_4x4mm_P0.4mm_EP2.3x2.3mm": (0.90,
        "altura normal de um QFN"),
    "Package_SO:SOIC-8_5.23x5.23mm_P1.27mm": (2.00, "altura normal do SOIC-8"),
}
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
    if tam is None or nome not in ALTURA:
        return sem_bloco(corpo)
    alt, _fonte = ALTURA[nome]
    base = nome.split(":", 1)[1]
    pasta = _pl.Path(__file__).resolve().parent / "3d"
    pasta.mkdir(exist_ok=True)
    wrl_caixa(pasta / (base + ".wrl"), tam[0], tam[1], alt, cor=(0.18, 0.18, 0.20))
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
DESENHADOS = {
    "gnssbike:MinewSemi_ME54BS13_16.5x12mm":
        lambda c, w, h, a: wrl_me54bs13(c),
    "gnssbike:u-blox_MAX-F10S_9.7x10.1mm": wrl_max_f10s,
}


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
        CORPO_TODOS[nome] = (w, h, alt)
        base = nome.split(":", 1)[1]
        if nome in DESENHADOS:
            DESENHADOS[nome](pasta / (base + ".wrl"), w, h, alt)
        else:
            wrl_caixa(pasta / (base + ".wrl"), w, h, alt)
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
_fp("U504", "gnssbike:MMC5633_WLP-4_0.85x0.85mm", "GERADO", "")
_fp("U505", "gnssbike:OPT3001_USON-6_2x2mm_P0.65mm", "GERADO", "")
_fp("D101", "gnssbike:ESD761_X1SON-2_1x0.6mm", "GERADO", "")
_fp("D102", "gnssbike:TPD4E05U06_USON-10_1x2.5mm_P0.5mm", "GERADO", "")
_fp("U502", "gnssbike:BMP585_LGA-8_3.25x3.25mm", "GERADO", "")
_fp("U302", "gnssbike:TXU0204_WQFN-14_3x2.5mm_P0.5mm", "GERADO", "")
_fp("U301", "gnssbike:u-blox_MAX-F10S_9.7x10.1mm", "GERADO", "")
_fp("D601", "gnssbike:LED_RGB_APTF1616_1.6x1.6mm", "GERADO", "")


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
