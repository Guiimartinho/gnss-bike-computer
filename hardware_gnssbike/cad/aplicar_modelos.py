"""Bring the routed board's 3D models up to date without touching its copper.

make_pcb.py writes each footprint's (model ...) block when it places the
board, and the board is routed since 2026-10-19: placing it again would undo
the routing, which route.py and acabar.py closed one connection at a time.
A maker's STEP dropped into 3d/real/, a turn measured into
footprints.MODELO_GIRADO or a height added to footprints.ALTURA changes only
that block. So this script works out, part by part, the block make_pcb.py
would write now - the same fp_load.corpo(), the same flip and the same
MODELO_GIRADO_VERSO for the back - and rewrites it in gnssbike.kicad_pcb in
KiCad's own layout. Everything else in the file stays as it is.

    python aplicar_modelos.py          # lists what changes, and writes it
    python aplicar_modelos.py --ver    # lists only

Then make_3d.py for the pictures and dry_run_pcb.py for ME4 to ME6, which
measure each body against its pads and the footprint's outline.
"""
from __future__ import annotations

import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import footprints as FPS  # noqa: E402
import fp_load  # noqa: E402
import make_pcb as MP  # noqa: E402

PLACA = HERE / "gnssbike.kicad_pcb"
TAB = "\t"


def fecha(texto: str, i: int) -> int:
    """Index of the parenthesis that closes the one at i, strings skipped."""
    d = 0
    j = i
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
                return j
        j += 1
    raise ValueError("parenteses sem fechar")


def modelo(texto: str) -> tuple[int, int, tuple] | None:
    """The (model ...) block in a footprint's text: where it starts, where it
    ends, and what it says (path, offset, scale, rotate)."""
    i = texto.find("(model ")
    if i < 0:
        return None
    j = fecha(texto, i)
    bloco = texto[i:j + 1]
    caminho = re.match(r'\(model "([^"]+)"', bloco).group(1)

    def xyz(nome: str) -> tuple[float, ...]:
        m = re.search(r"\(" + nome + r"\s*\(xyz ([^)]*)\)", bloco)
        return tuple(float(v) for v in m.group(1).split()) if m else ()
    return i, j, (caminho, xyz("offset"), xyz("scale"), xyz("rotate"))


def bloco_kicad(caminho: str, offset, scale, rotate, recuo: str) -> str:
    """A model block as KiCad saves it, indented for a footprint on the board."""
    def n(v: float) -> str:
        return f"{v:g}"
    linhas = [f'(model "{caminho}"']
    for nome, v in (("offset", offset), ("scale", scale), ("rotate", rotate)):
        linhas += [f"{TAB}({nome}", f"{TAB * 2}(xyz {' '.join(n(x) for x in v)})", f"{TAB})"]
    linhas.append(")")
    return ("\n" + recuo).join(linhas)


def desejado(nome: str, atras: bool) -> tuple | None:
    """What make_pcb.py would write for this footprint now."""
    corpo = fp_load.corpo(nome)
    if atras:
        corpo = MP.virar(corpo)
        corpo = FPS.modelo_no_verso(corpo)
    achado = modelo(corpo)
    return achado[2] if achado else None


def main() -> int:
    so_ver = "--ver" in sys.argv[1:]
    texto = PLACA.read_text(encoding="utf-8")
    trocas: list[tuple[int, int, str]] = []
    resumo: list[str] = []
    i = 0
    while True:
        i = texto.find("\n\t(footprint ", i)
        if i < 0:
            break
        ini = i + 2
        fim = fecha(texto, ini)
        fp = texto[ini:fim + 1]
        nome = re.match(r'\(footprint "([^"]+)"', fp).group(1)
        ref = re.search(r'\(property "Reference" "([^"]*)"', fp)
        ref = ref.group(1) if ref else "?"
        atras = re.search(r'\n\t\t\(layer "B\.Cu"\)', fp) is not None
        alvo = desejado(nome, atras)
        atual = modelo(fp)
        agora = atual[2] if atual else None
        if alvo != agora:
            if atual:
                a, b = ini + atual[0], ini + atual[1] + 1
                if alvo:
                    trocas.append((a, b, bloco_kicad(*alvo, TAB * 2)))
                else:
                    a = texto.rfind("\n", 0, a)
                    trocas.append((a, b, ""))
            else:
                trocas.append((fim, fim, TAB + bloco_kicad(*alvo, TAB * 2) + "\n" + TAB))
            de = agora[0].rsplit("/", 1)[-1] if agora else "sem corpo"
            para = alvo[0].rsplit("/", 1)[-1] if alvo else "sem corpo"
            resumo.append(f"  {ref}: {de} -> {para}"
                          + (f" (giro {alvo[3][2]:g}, desloca {alvo[1][0]:g}; {alvo[1][1]:g})" if alvo else ""))
        i = fim
    for a, b, novo in sorted(trocas, reverse=True):
        texto = texto[:a] + novo + texto[b:]
    print(f"{len(trocas)} modelos a mudar" + (":" if trocas else ""))
    print("\n".join(resumo))
    if trocas and not so_ver:
        PLACA.write_text(texto, encoding="utf-8", newline="\n")
        print(f"gravado em {PLACA.name}; rode make_3d.py e dry_run_pcb.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
