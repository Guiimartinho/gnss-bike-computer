"""The manufacturer part number of every fitted part, by its LCSC code: what
a distributor other than LCSC (Mouser, DigiKey) is asked for.

Most come from docs/19-lista-de-compras.md, validated twice. The ones it
gave only as a series or a value were completed on 2026-10-19 with the same
value, size and tolerance (resistors 0402 1 % thick film in Yageo's RC
series, the antenna's C0G tuning caps in Murata's GJM15 at +-0.1 pF), and
checked to exist at Mouser through its search API (mouser.py).

MOUSER holds the parts swapped for that one distributor, same spec, because
the maker of the part above had no stock there on 2026-10-19. FORA_DA_MOUSER
holds what Mouser does not sell at all, and where it comes from instead: the
board was drawn for parts LCSC sells, and some of those makers sell nowhere
else. Their footprints are those parts' own, so they are not swapped here.
"""
from __future__ import annotations

MPN: dict[str, tuple[str, str]] = {
    "C11702": ("Yageo", "RC0402FR-071KL"),
    "C124205": ("Diodes", "BAT54WS-7-F"),
    "C127355": ("Molex", "503480-1000"),
    "C138714": ("TI", "TPD4E05U06DQAR"),
    "C15195": ("Samsung", "CL05B103KB5NNNC"),
    "C1588": ("Samsung", "CL10B102KB8NNNC"),
    "C1591": ("Samsung", "CL10B104KB8NNNC"),
    "C160405": ("JST", "SM06B-SRSS-TB"),
    "C161320": ("Murata", "GJM1555C1H3R6BB01D"),
    "C165948": ("HRO", "TYPE-C-31-M-12"),
    "C17168": ("Panasonic", "ERJ-2GE0R00X"),
    "C172116": ("Yageo", "RC0402FR-076M65L"),
    "C18184976": ("Bosch", "BMP585"),
    "C190124": ("Vishay", "CRCW12060000Z0EA"),
    "C19666": ("Samsung", "CL10A475KO8NNNC"),
    "C20512": ("Diodes", "DMG1012T-7"),
    "C2072359": ("TI", "TPS610997YFFR"),
    "C209959": ("TDK", "NTCG103JF103FT1"),
    "C227115": ("Yageo", "RC0402FR-074M3L"),
    "C23733": ("Samsung", "CL05A475MP5NRNC"),
    "C25076": ("Yageo", "RC0402FR-07100RL"),
    "C25082": ("Yageo", "RC0402FR-07150RL"),
    "C25105": ("Yageo", "RC0402FR-0733RL"),
    "C25741": ("Yageo", "RC0402FR-07100KL"),
    "C25744": ("Yageo", "RC0402FR-0710KL"),
    "C25745": ("Yageo", "RC0402FR-07110KL"),
    "C25755": ("Yageo", "RC0402FR-07150KL"),
    "C25760": ("Yageo", "RC0402FR-07180KL"),
    "C25785": ("Yageo", "RC0402FR-07402KL"),
    "C25792": ("Yageo", "RC0402FR-0747KL"),
    "C25900": ("Panasonic", "ERJ-2RKF4701X"),
    "C25902": ("Yageo", "RC0402FR-074K87L"),
    "C2802844": ("Macronix", "MX25R6435FM2IL0-TR"),      # SOP-8 209 mil, the footprint's
    "C2827321": ("TUOZHAN", "S4-3528RGBTA-A"),
    "C2836813": ("Bosch", "BMI270"),
    "C2862166": ("TI", "TPS7A0218PDQNR"),
    "C2863637": ("TI", "TLV7031DCKR"),
    "C2998090": ("Yageo", "RC0402FR-073M32L"),
    "C30005563": ("u-blox", "MAX-F10S-00B"),
    "C3170007": ("Molex", "503480-0440"),
    "C337891": ("Murata", "DFE201610E-2R2M=P2"),
    "C3812179": ("Same Sky", "CPT-12123-81-SMT-TR"),
    "C386081": ("Taiyo Yuden", "TMK107BBJ106MA-T"),
    "C404328": ("Memsic", "MMC5603NJ"),
    "C423169": ("Yageo", "RC0402FR-074M32L"),
    "C423222": ("UNI-ROYAL", "0402WGF1805TCE"),
    "C45323": ("Yageo", "RC0402FR-0739R2L"),
    "C455280": ("XunPu", "TS-1088R-02026"),
    "C477658": ("Yageo", "RC0402FR-072M67L"),
    "C477994": ("Nexperia", "PESD24VS1UL,315"),
    "C485354": ("JST", "S4B-ZR-SM4A-TF"),
    "C52923": ("Samsung", "CL05A105KA5NQNC"),
    "C5328777": ("Analog Devices", "MAX17262REWL+T"),
    "C5527934": ("Molex", "503480-0540"),
    "C579259": ("Analog Devices", "ADP5091ACPZ-2-R7"),
    "C5875723": ("Kingbright", "APT1608SURCK"),
    "C6569550": ("Unictron", "H2UJ4U1H2Q0100"),
    "C6600230": ("Taiyo Yuden", "MSASL168BB5226MTNA01"),
    "C71687": ("Murata", "GRM155R71A104KA01D"),
    "C7423886": ("Samsung", "CL05A226MQ5N6J8"),
    "C7466043": ("Nordic", "nPM1300-QEAA-R"),
    "C76900": ("Murata", "GJM1555C1H1R5BB01D"),
    "C76904": ("Murata", "GJM1555C1H2R7BB01D"),
    "C77003": ("Murata", "GRM155R61A474KE15D"),
    "C88374": ("Hirose", "U.FL-R-SMT-1"),
    "C88896": ("Murata", "GJM1555C1H6R8BB01D"),
    "C88980": ("Murata", "BLM18KG221SN1D"),
    "C90462": ("TI", "OPT3001DNPR"),
    "C920299": ("Nexperia", "PESD3V3S1UB,115"),
    "C98385": ("Sunlord", "SPH252012H220MT"),
}

# Swapped at Mouser only, same value, size and tolerance (2026-10-19)
MOUSER: dict[str, tuple[str, str, str]] = {
    "C25902": ("Panasonic", "ERJ-2RKF4871X", "4,87 k 1 % 0402: a Yageo sem estoque na Mouser"),
    "C477658": ("Vishay", "CRCW04022M67FKED", "2,67 M 1 % 0402: a Yageo sem estoque na Mouser"),
    "C209959": ("Murata", "NCP15XH103F03RC",
                "NTC 10 k 1 %, B25/50 3380 K 1 %, 0402: o TDK sem estoque na Mouser"),
    "C23733": ("TDK", "C1005X5R1A475M050BC", "4,7 uF 10 V X5R 20 % 0402: o Samsung sem estoque na Mouser"),
    "C1591": ("TDK", "C1608X7R1H104K080AA", "100 nF 50 V X7R 10 % 0603: o Samsung sem estoque na Mouser"),
    # the same parts, under the codes Mouser sells them by
    "C160405": ("JST", "SM06B-SRSS-TB(LF)(SN)", "codigo da Mouser"),
    "C485354": ("JST", "S4B-ZR-SM4A-TF(LF)(SN)", "codigo da Mouser"),
    "C88374": ("Hirose", "U.FL-R-SMT-1(10)", "codigo da Mouser"),
}

# Not sold by Mouser: where each comes from instead
FORA_DA_MOUSER: dict[str, str] = {
    "C423222": "18 M 1 % 0402 nao existe na Mouser em marca nenhuma; LCSC (UNI-ROYAL)",
    "C2827321": "LED RGB TUOZHAN, o footprint e dele; LCSC",
    "C455280": "chave XunPu, o footprint e dela; LCSC",
    "C165948": "USB-C HRO, o footprint e dele; LCSC",
    "C98385": "indutor Sunlord 22 uH 1008; LCSC",
    "C6569550": "antena de chip Unictron; LCSC",
}

# Parts with no LCSC code, and the ones that live off the board
SEM_LCSC: dict[str, tuple[str, str, str]] = {
    "U201": ("MinewSemi", "ME54BS13-1Y20TI", "DigiKey 6024-ME54BS13-1Y20TITR-ND ou a loja da MinewSemi"),
}
FORA_DA_PLACA: list[tuple[str, str, str, int, str]] = [
    ("PV101 a PV106", "ANYSOLAR", "KXOB25-05X3F-TR", 6, "DigiKey KXOB25-05X3FCT-ND"),
    ("DS401 (tela JDI)", "JDI", "LPM027M128C", 1, "fora de venda: AliExpress, sem garantia"),
    ("DS402 (tela Sharp)", "Sharp", "LS027B7DH01A", 1, "DigiKey 425-2908-ND"),
    ("DS402 com luz", "Azumo", "11103-06", 1, "DigiKey 2004-11103-06_A1-ND"),
]
