#!/usr/bin/env python3
"""Gera o 2D da placa: uma pagina por camada de cobre, mais a de conjunto.

A vista de conjunto - as quatro camadas de cobre numa folha so - era legivel
enquanto as malhas de terra estavam vazias. Cheias, ela deixa de ser: o
despejo e uma area solida e cobre as trilhas que estao debaixo dele, de modo
que a folha passa a mostrar o contorno do cobre e quase mais nada.

Quem le uma placa le uma camada de cada vez, e e isso que este arquivo gera:
quatro paginas, cada uma com uma camada de cobre sobre o contorno e a
serigrafia, mais uma quinta com as quatro juntas para quem quiser a vista de
conjunto. O `kicad-cli pcb export pdf` junta as camadas que se pede numa
pagina so, entao sao cinco chamadas e uma juncao.

    python hardware_gnssbike/cad/make_2d.py
"""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
KICAD = pathlib.Path(r"D:\KiCAD\bin\kicad-cli.exe")
PCB = HERE / "gnssbike.kicad_pcb"

# Copper layer -> what the page is called, and which silkscreen goes with it.
# The silkscreen is per FACE: the front one names the parts on the front and
# the back one names the parts on the back, and printing F.SilkS over the
# B.Cu page labels the wrong side of the board. Four of this board's parts
# are on the back - the buzzer, the battery connector, the barometer and the
# thermistor - and on the back page they were unnamed.
COBRE = (
    ("F.Cu", "F.SilkS", "1 - F.Cu: frente"),
    ("In1.Cu", "F.SilkS", "2 - In1.Cu: plano de terra"),
    ("In2.Cu", "F.SilkS", "3 - In2.Cu: roteamento interno"),
    ("B.Cu", "B.SilkS", "4 - B.Cu: verso"),
)
CONTEXTO = "Edge.Cuts,F.Fab"


def exporta(saida: pathlib.Path, camadas: str) -> None:
    r = subprocess.run(
        [str(KICAD), "pcb", "export", "pdf", "--output", str(saida),
         "--layers", camadas, str(PCB)],
        capture_output=True, text=True)
    if r.returncode != 0 or not saida.exists():
        raise SystemExit(f"kicad-cli falhou em {camadas}: {r.stderr.strip()}")


def main() -> int:
    try:
        import fitz
    except ImportError:
        print("PyMuPDF nao esta instalado: sem ele nao da para juntar as "
              "paginas", file=sys.stderr)
        return 2
    if not KICAD.exists():
        print(f"kicad-cli nao esta em {KICAD}", file=sys.stderr)
        return 2

    tmp = HERE / "_2d"
    tmp.mkdir(exist_ok=True)
    junto = fitz.open()
    paginas = []
    for camada, silk, rotulo in COBRE:
        p = tmp / (camada.replace(".", "_") + ".pdf")
        exporta(p, camada + "," + silk + "," + CONTEXTO)
        paginas.append((p, rotulo))
    todas = tmp / "todas.pdf"
    exporta(todas, ",".join(c for c, _s, _r in COBRE) +
            ",F.SilkS,B.SilkS," + CONTEXTO)
    paginas.append((todas, "5 - as quatro camadas juntas"))

    for p, rotulo in paginas:
        d = fitz.open(p)
        junto.insert_pdf(d)
        pag = junto[-1]
        pag.insert_text((36, 28), rotulo, fontsize=11, fontname="helv")
        d.close()
    saida = HERE / "gnssbike-pcb.pdf"
    junto.save(saida, garbage=3, deflate=True)
    junto.close()
    for p, _r in paginas:
        p.unlink()
    tmp.rmdir()
    print(f"{saida.name}: {len(paginas)} paginas, uma por camada de cobre "
          "mais a de conjunto")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
