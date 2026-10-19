"""The files a fabricator quotes and builds from, in JLCPCB's format.

Gerbers and drill for the bare board, one zip; and for the assembly, one
BOM and one placement file (CPL) per display variant, because the board
takes either display and the two are not built the same:

    jdi    the JDI LPM027M128C: no 5 V boost, no Azumo light, and JP401's
           0 ohm between COMUM and 3V0
    sharp  the Sharp LS027B7DH01A with the Azumo 11103-06 light: no JDI
           light connector, and JP401's 0 ohm between COMUM and 5V0 (5 V
           on the JDI burns it)

JP301 and JP401 are three pads that take one 0402 across two of them; the
CPL puts that part on the pair the variant asks for, not on the
footprint's centre. A LEIA-ME with the order's options goes with them.

Run on the routed board, after fill_zones.py:

    python make_fab.py        -> ../fabricacao/ (generated, not in git)
"""
from __future__ import annotations

import csv
import datetime
import math
import pathlib
import re
import shutil
import subprocess
import sys
import zipfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import dry_run_pcb as D  # noqa: E402
import fp_load  # noqa: E402
import parts as P  # noqa: E402

KICAD_CLI = "D:/KiCAD/bin/kicad-cli.exe"
PLACA = HERE / "gnssbike.kicad_pcb"
SAIDA = HERE.parent / "fabricacao"
CAMADAS = "F.Cu,In1.Cu,In2.Cu,B.Cu,F.Paste,B.Paste,F.SilkS,B.SilkS,F.Mask,B.Mask,Edge.Cuts"

# Not fitted in either variant: C302 is the shunt Unictron says to leave
# open, JP404 and JP405 the reversed polarity of the Azumo light, and J202
# the SWD header, whose part is not chosen yet (J201's pogo pads program the
# board). JP301 takes the chip antenna in both: pads 2 (COMUM) and 3 (CHIP).
SEM_PECA = {"C302", "JP404", "JP405", "J202"}
VARIANTES = {
    "jdi": {"sem_peca": {"U402", "L401", "C405", "C406", "C407", "J403", "R410", "JP402", "JP403"},
            "jumpers": {"JP401": ("1", "2"), "JP301": ("2", "3")}},
    "sharp": {"sem_peca": {"J402"},
              "jumpers": {"JP401": ("2", "3"), "JP301": ("2", "3")}},
}
# what LCSC does not stock: it goes in the BOM without a code, so the quote
# shows it as a part to consign
CONSIGNADAS = {"U201": "MinewSemi ME54BS13 (nRF54LM20A), consignado"}


def kicad(*args: str) -> None:
    r = subprocess.run([KICAD_CLI, *args], capture_output=True, text=True)
    if r.returncode != 0:
        raise SystemExit(f"kicad-cli {' '.join(args[:3])}: {r.stderr.strip()[:400]}")


def codigo(ref: str) -> str:
    if ref in P.LCSC_PASSIVOS:
        return P.LCSC_PASSIVOS[ref]
    return getattr(P.PARTS[ref], "lcsc", "") or ""


def ordem(ref: str):
    m = re.match(r"([A-Z]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


def footprints() -> dict:
    """Ref -> (library footprint name, x, y, angle, pads {number: (x, y)}),
    board mm as KiCad writes them (y down)."""
    arv = fp_load.parse(PLACA.read_text(encoding="utf-8"))
    out = {}
    for f in fp_load.kids(arv, "footprint"):
        ref = next((pr[2] for pr in fp_load.kids(f, "property") if pr[1] == "Reference"), "")
        at = fp_load.kid(f, "at")
        fx, fy = float(at[1]), float(at[2])
        ang = float(at[3]) if len(at) > 3 else 0.0
        r = math.radians(ang)
        pads = {}
        for p in fp_load.kids(f, "pad"):
            a = fp_load.kid(p, "at")
            px, py = float(a[1]), float(a[2])
            pads[str(p[1])] = (fx + px * math.cos(r) + py * math.sin(r),
                               fy - px * math.sin(r) + py * math.cos(r))
        out[ref] = (str(f[1]).strip('"').split(":")[-1], fx, fy, ang, pads)
    return out


def gerbers() -> pathlib.Path:
    """JLCPCB's KiCad settings: Protel extensions, no X2, solder mask
    subtracted from the silkscreen, Excellon in mm with decimal zeros and
    the alternate drill mode for slots, plated and non-plated apart."""
    pasta = SAIDA / "gerber"
    shutil.rmtree(pasta, ignore_errors=True)
    pasta.mkdir(parents=True)
    kicad("pcb", "export", "gerbers", "--output", str(pasta) + "/", "--layers", CAMADAS,
          "--no-x2", "--subtract-soldermask", "--precision", "6", str(PLACA))
    kicad("pcb", "export", "drill", "--output", str(pasta) + "/", "--format", "excellon",
          "--drill-origin", "absolute", "--excellon-units", "mm",
          "--excellon-zeros-format", "decimal", "--excellon-oval-format", "alternate",
          "--excellon-separate-th", str(PLACA))
    zipado = SAIDA / "gnssbike-gerber-jlcpcb.zip"
    with zipfile.ZipFile(zipado, "w", zipfile.ZIP_DEFLATED) as z:
        for arq in sorted(pasta.iterdir()):
            z.write(arq, arq.name)
    return zipado


def posicoes() -> list[dict]:
    """KiCad's own placement file, both sides, in mm."""
    tmp = SAIDA / "_posicoes.csv"
    kicad("pcb", "export", "pos", "--output", str(tmp), "--format", "csv", "--units", "mm",
          "--side", "both", str(PLACA))
    with open(tmp, encoding="utf-8", newline="") as f:
        linhas = list(csv.DictReader(f))
    tmp.unlink()
    return linhas


def variante(nome: str, cfg: dict, pos: list[dict], fps: dict) -> tuple[int, int, list[str]]:
    fora = SEM_PECA | cfg["sem_peca"]
    montadas = [q for q in pos if q["Ref"] not in fora]
    # the placement file's y points up: the board's y with its sign turned,
    # which KiCad's own anchor shows
    with open(SAIDA / f"cpl-{nome}.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Designator", "Mid X", "Mid Y", "Layer", "Rotation"])
        for q in sorted(montadas, key=lambda q: ordem(q["Ref"])):
            x, y, rot = float(q["PosX"]), float(q["PosY"]), float(q["Rot"])
            if q["Ref"] in cfg["jumpers"]:
                a, b = cfg["jumpers"][q["Ref"]]
                pads = fps[q["Ref"]][4]
                fx, fy = fps[q["Ref"]][1], fps[q["Ref"]][2]
                mx = (pads[a][0] + pads[b][0]) / 2
                my = (pads[a][1] + pads[b][1]) / 2
                x += mx - fx
                y -= my - fy
            w.writerow([q["Ref"], f"{x:.4f}", f"{y:.4f}",
                        "Top" if q["Side"].lower() == "top" else "Bottom", f"{rot:g}"])
    grupos: dict = {}
    sem_codigo = []
    for q in montadas:
        ref = q["Ref"]
        if ref not in P.PARTS:
            continue
        lcsc = codigo(ref)
        valor = CONSIGNADAS.get(ref, P.PARTS[ref].value)
        if not lcsc:
            sem_codigo.append(ref)
        chave = (valor, fps[ref][0] if ref in fps else "", lcsc)
        grupos.setdefault(chave, []).append(ref)
    with open(SAIDA / f"bom-{nome}.csv", "w", encoding="utf-8", newline="") as f:
        w = csv.writer(f)
        w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
        for (valor, fp, lcsc), refs in sorted(grupos.items(), key=lambda kv: ordem(sorted(kv[1], key=ordem)[0])):
            w.writerow([valor, ",".join(sorted(refs, key=ordem)), fp, lcsc])
    return len(montadas), len(grupos), sorted(sem_codigo, key=ordem)


def leia_me(zipado, resumo, verso) -> None:
    hoje = datetime.date.today().isoformat()
    txt = f"""GNSS Bike Computer: arquivos para cotar e fabricar na JLCPCB
gerados por hardware_gnssbike/cad/make_fab.py em {hoje}; nada foi fabricado

PLACA (PCB): {zipado.name}
  Gerber RS-274X das 4 camadas de cobre, mascara, pasta, serigrafia e
  contorno, e a furacao Excellon (com e sem metalizacao separadas).

  Opcoes do pedido:
    Layers ............... 4
    Dimensions ........... 34 x 95 mm
    PCB Thickness ........ 0.8 mm
    Impedance Control .... Yes, stackup {D.FABRICACAO["pilha"].split(",")[0].replace("JLCPCB ", "")}
                           50 ohm single-ended em L1 (0,15 mm), referencia L2
                           90 ohm diferencial em L1 (0,15 mm com 0,2 de vao), referencia L2
                           (as larguras sairam de cad/impedancia.py nessa pilha)
    Outer Copper ......... 1 oz;  Inner Copper: 0.5 oz
    Via Covering ......... Epoxy Filled & Capped (POFV): ha vias dentro de
                           ilhas de SMD, e sem isso a solda escorre pelo furo
    Min via hole/diameter  0,25 / 0,45 mm
    Surface Finish ....... ENIG (WLP de 0,4 mm, QFN e via em ilha)
    Min track/spacing .... 0,1 / 0,127 mm (a JLCPCB faz 0,09 / 0,09)
    Remove Order Number .. especificar local, ou aceitar onde a JLCPCB por

MONTAGEM (PCBA): uma BOM e um CPL por tela
  bom-jdi.csv   + cpl-jdi.csv     tela JDI LPM027M128C
  bom-sharp.csv + cpl-sharp.csv   Sharp LS027B7DH01A com a luz do Azumo 11103-06
{resumo}
  Montagem nas duas faces (pecas no verso: {", ".join(verso)}).
  Sem peca nas duas: {", ".join(sorted(SEM_PECA, key=ordem))} (J202: o conector SWD
  ainda nao foi escolhido; a placa se grava pelas ilhas do J201).
  O U201 (modulo MinewSemi ME54BS13) nao existe na LCSC: vai na BOM sem
  codigo, para consignar ou soldar a mao.
  JP301 (antena de chip) e JP401 (tensao da tela) sao 0 ohm entre dois de
  tres pads: o CPL ja os poe no par certo de cada variante. Conferir na
  previa da JLCPCB, porque 5 V na JDI a queima.
  Conferir na previa da JLCPCB a rotacao de cada peca: a biblioteca deles
  tem convencao propria, e as do verso costumam precisar de ajuste.
"""
    (SAIDA / "LEIA-ME.txt").write_text(txt, encoding="utf-8", newline="\n")


def main() -> int:
    SAIDA.mkdir(exist_ok=True)
    zipado = gerbers()
    fps = footprints()
    pos = posicoes()
    verso = sorted((q["Ref"] for q in pos if q["Side"].lower() != "top"), key=ordem)
    resumo = []
    for nome, cfg in VARIANTES.items():
        n, g, sem = variante(nome, cfg, pos, fps)
        resumo.append(f"  {nome}: {n} pecas em {g} linhas; sem codigo da LCSC: {', '.join(sem) or 'nenhuma'}")
        print(f"bom-{nome}.csv e cpl-{nome}.csv: {n} pecas, {g} linhas da BOM; sem codigo: {', '.join(sem) or 'nenhuma'}")
    leia_me(zipado, "\n".join(resumo), [r for r in verso if r not in SEM_PECA])
    print(f"{zipado.name}: {len(zipfile.ZipFile(zipado).namelist())} arquivos")
    print(f"tudo em {SAIDA}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
