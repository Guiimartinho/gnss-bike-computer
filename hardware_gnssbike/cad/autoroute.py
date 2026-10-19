#!/usr/bin/env python3
"""Fechar o roteamento da placa com o Freerouting, pelo caminho nativo do KiCad.

    "D:/KiCAD/bin/python.exe" hardware_gnssbike/cad/autoroute.py

Roda com o Python DO KICAD, nao com o do sistema: quem exporta o DSN e
importa o SES e o proprio `pcbnew`, que so existe la (o mesmo motivo do
`fill_zones.py`). Entra na cadeia depois do `route.py` e antes do
`fill_zones.py`.

Por que ele existe. O `route.py` e um labirinto A* sem rip-up: ele faz bem o
que pede regra propria - as linhas de 50 ohm do GNSS com as vias de terra dos
dois lados, o par USB pelo diodo de protecao, a costura de terra da borda - e
deixa sempre algumas dezenas de ligacoes sem cobre (59 em 2026-09-26, 113 de
177 numa ordem de tentativa em 2026-10-18). O Freerouting fecha esse tamanho
de placa em minutos. O caminho e o do projeto irmao do medidor de potencia
(`33.bike_powermeter`, hardware_powermeter/cad/autoroute.py), com as licoes
que ele mediu la e estao comentadas abaixo.

O que o `route.py` desenhou nas redes de RF e do USB, e todas as vias de
terra, vao TRAVADAS para o DSN (`protect`): o Freerouting roteia em volta
delas e nao as refaz. O resto ele pode desfazer e refazer.

O que NAO muda: a colocacao, as zonas, as regras do `gnssbike.kicad_pro`, o
DRC do KiCad, o `check_pcb.py` e os dry runs continuam sendo os juizes.
Troca-se apenas quem desenha a trilha.

Freerouting, GPL-3.0, rodado como FERRAMENTA EXTERNA sobre os nossos
arquivos: nada dele entra no projeto, e a licenca CC BY-NC nao muda. O jar e
o Java ficam fora do repositorio, em C:/Users/<voce>/tools/freerouting.
"""
from __future__ import annotations

import os
import pathlib
import re
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
PLACA = HERE / "gnssbike.kicad_pcb"

FERRAMENTAS = pathlib.Path(
    os.environ.get("GNSSBIKE_FERRAMENTAS",
                   pathlib.Path.home() / "tools" / "freerouting"))
JAR = pathlib.Path(os.environ.get("FREEROUTING_JAR",
                                  FERRAMENTAS / "freerouting-2.4.1.jar"))
# o jar da v2.4.1 e compilado para Java 25 (class file 69): o Java 17 da
# maquina recusa, entao vai o runtime proprio, ao lado dele (medido no 33)
JAVA = pathlib.Path(os.environ.get(
    "FREEROUTING_JAVA",
    FERRAMENTAS / "jdk-25.0.4.1+1-jre" / "bin" / "java.exe"))

DSN = HERE / "_gnssbike.dsn"
SES = HERE / "_gnssbike.ses"

# passagens de otimizacao: o Freerouting para sozinho quando nao melhora mais
PASSAGENS = int(os.environ.get("FREEROUTING_PASSAGENS", "100"))

# As redes cujo cobre o route.py desenha por regra propria e que o
# Freerouting nao pode refazer: as quatro linhas de 50 ohm do GNSS e o par
# USB (diferencial, pelo TPD4E05U06). As vias de GND tambem ficam: sao as de
# costura e as das laterais da linha de RF.
TRAVADAS = re.compile(r"^(/\S*/)?(RF_\w+|USB_D[PM])$")

# A camada interna de terra, pelo nome de usuario que o KiCad exporta no DSN.
# E plano: nada se roteia nela, e as vias a atravessam (no 33, sem isso saiu
# trilha de sinal dentro do plano).
PLANOS = ("gnd",)

# A folga de cobre do projeto (gnssbike.kicad_pro, min_clearance) em um. Toda
# regra do DSN abaixo dela sobe para ela: o KiCad exporta a smd_smd de 22 um,
# a tolerancia de ilha contra ilha do encapsulamento, e o Freerouting a usa
# para espremer trilha entre ilhas vizinhas (no 33: 56 curtos e 59 pontes de
# mascara numa placa que ele dizia limpa). A via de 0,45 com furo de 0,25 tem
# anel de 0,10, e o min_hole_clearance de 0,20 fica coberto pelos 127.
FOLGA_COBRE_UM = 127.0

# As redes de RF do GNSS vao numa classe propria no DSN: a largura de 50 ohm
# desta placa (0,196 mm, make_pcb.py: F.Cu a 0,10 mm do plano de terra) e so
# a camada da frente, porque uma via no meio da linha e um toco, que a 7.2 da
# ficha do modulo proibe pelo nome. O route.py deixa essas redes de fora.
RF = re.compile(r"\(net (RF_\w+)")
RF_LARGURA_UM = 196.0

# A largura minima de trilha do projeto, em mm: o Freerouting estreita trilha
# abaixo dela para passar em vao apertado (no 33, dez erros de track_width).
LARGURA_MINIMA = 0.127


def travar(placa) -> tuple[int, int]:
    """Trava as trilhas das redes TRAVADAS e todas as vias de terra."""
    import pcbnew
    n_t = n_v = 0
    for t in placa.GetTracks():
        rede = t.GetNetname()
        if t.Type() == pcbnew.PCB_VIA_T and rede == "GND":
            t.SetLocked(True)
            n_v += 1
        elif TRAVADAS.match(rede):
            t.SetLocked(True)
            n_t += 1
    return n_t, n_v


def preparar_dsn(dsn: pathlib.Path) -> str:
    """Plano interno como `power`, folgas no minimo do projeto, sem despejos
    nas faces. Devolve um resumo do que mudou."""
    texto = dsn.read_text(encoding="utf-8")
    n_plano = 0
    for nome in PLANOS:
        alvo = f"(layer {nome}\n      (type signal)"
        if alvo in texto:
            texto = texto.replace(alvo, f"(layer {nome}\n      (type power)", 1)
            n_plano += 1

    n_folga = 0

    def troca(m):
        nonlocal n_folga
        if float(m.group(1)) >= FOLGA_COBRE_UM:
            return m.group(0)
        n_folga += 1
        return f"(clearance {FOLGA_COBRE_UM:g}{m.group(2)})"

    texto = re.sub(r"\(clearance ([\d.]+)((?: \(type [a-z_]+\))?)\)", troca, texto)

    # a classe de RF: tira as redes de RF das classes do KiCad e as poe numa
    # classe so da frente, na largura de 50 ohm
    redes_rf = sorted(set(RF.findall(texto)))
    frente = re.search(r"\(layer (\S+)\s+\(type signal\)", texto).group(1)
    if redes_rf:
        def limpa(m):
            cab = m.group(0)
            for n in redes_rf:
                cab = re.sub(r"(?<=\s)" + re.escape(n) + r"(?=\s)", "", cab)
            return cab
        texto = re.sub(r"\(class [^(]*", limpa, texto)
        bloco = (f"    (class RF {' '.join(redes_rf)}\n"
                 f"      (circuit\n        (use_layer {frente})\n      )\n"
                 f"      (rule\n        (width {RF_LARGURA_UM:g})\n"
                 f"        (clearance {FOLGA_COBRE_UM:g})\n      )\n    )\n")
        i = texto.index("    (class ")
        texto = texto[:i] + bloco + texto[i:]

    # Os despejos de terra de F.Cu e B.Cu saem do DSN: o fill_zones.py os
    # derrama depois, em volta do cobre que ficou (primeiro a trilha, depois
    # o terra). O plano interno fica, porque nada e roteado nele.
    saida, i, n_desp = [], 0, 0
    while True:
        m = re.search(r"\(plane (\S+) \(polygon (\S+) ", texto[i:])
        if not m:
            saida.append(texto[i:])
            break
        ini = i + m.start()
        p, j = 0, ini
        while j < len(texto):
            if texto[j] == "(":
                p += 1
            elif texto[j] == ")":
                p -= 1
                if p == 0:
                    j += 1
                    break
            j += 1
        if m.group(2) in PLANOS:
            saida.append(texto[i:j])
        else:
            saida.append(texto[i:ini])
            n_desp += 1
        i = j
    texto = "".join(saida)
    dsn.write_text(texto, encoding="utf-8", newline="\n")
    return (f"{n_plano} camada(s) de plano, {n_folga} regra(s) de folga subidas a "
            f"{FOLGA_COBRE_UM:g} um, {n_desp} despejo(s) tirados, classe RF com "
            f"{len(redes_rf)} rede(s) so em {frente}")


def alargar_finas(placa: pathlib.Path) -> int:
    """Devolve a largura minima a toda trilha que saiu abaixo dela."""
    texto = placa.read_text(encoding="utf-8")
    n = 0

    def troca(m):
        nonlocal n
        if float(m.group(1)) >= LARGURA_MINIMA - 1e-9:
            return m.group(0)
        n += 1
        return f"(width {LARGURA_MINIMA})"

    novo = re.sub(r"\(width ([\d.]+)\)(?=\s*\(layer \"(?:[FB]|In[12])\.Cu\")",
                  troca, texto)
    if n:
        placa.write_text(novo, encoding="utf-8", newline="\n")
    return n


def _exige(caminho: pathlib.Path, o_que: str) -> None:
    if not caminho.exists():
        raise SystemExit(
            f"nao achei {o_que}: {caminho}\n"
            "  o jar vem de https://github.com/freerouting/freerouting/releases "
            "e o runtime de https://adoptium.net (Java 25)")


def main() -> int:
    import pcbnew                                          # noqa: E402

    _exige(PLACA, "a placa")
    _exige(JAR, "o jar do Freerouting")
    _exige(JAVA, "o Java 25")

    placa = pcbnew.LoadBoard(str(PLACA))
    n_t, n_v = travar(placa)
    print(f"placa carregada: {len(placa.GetFootprints())} pecas, "
          f"{len(placa.GetTracks())} itens de cobre; travados {n_t} trechos de "
          f"RF e USB e {n_v} vias de terra", flush=True)
    if not pcbnew.SaveBoard(str(PLACA), placa):
        raise SystemExit("nao consegui gravar a placa com as travas")

    # 1. DSN
    if not pcbnew.ExportSpecctraDSN(placa, str(DSN)):
        raise SystemExit("o KiCad recusou exportar o DSN")
    print(f"{DSN.name}: {DSN.stat().st_size} B; " + preparar_dsn(DSN), flush=True)

    # 2. o roteamento. Sem janela (`headless`): com ela o Freerouting estoura
    #    ao desenhar o despejo e grava um .ses de zero byte, saindo com 0
    #    (medido no 33). Uma thread: a otimizacao multi-thread gera violacao
    #    de isolamento, e o proprio programa avisa.
    cmd = [str(JAVA), "-Djava.awt.headless=true", "-jar", str(JAR),
           "-de", str(DSN), "-do", str(SES), "-mp", str(PASSAGENS), "-mt", "1"]
    print("rodando: " + " ".join(cmd[3:]), flush=True)
    t0 = time.time()
    if SES.exists():
        SES.unlink()
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8",
                       errors="replace")
    for linha in (r.stdout or "").splitlines():
        if any(p in linha for p in ("WARN", "ERROR", "unrouted", "Routing",
                                    "completed", "incomplete")):
            print("  " + linha[:200], flush=True)
    if r.returncode != 0:
        print((r.stderr or "")[:2000], file=sys.stderr)
        raise SystemExit(f"o Freerouting saiu com {r.returncode}")
    if not SES.exists() or SES.stat().st_size == 0:
        raise SystemExit("o Freerouting nao gravou o .ses (ou gravou vazio)")
    print(f"{SES.name}: {SES.stat().st_size} B em {time.time() - t0:.0f} s", flush=True)

    # 3. de volta para a placa
    placa = pcbnew.LoadBoard(str(PLACA))
    if not pcbnew.ImportSpecctraSES(placa, str(SES)):
        raise SystemExit("o KiCad recusou importar o SES")
    if not pcbnew.SaveBoard(str(PLACA), placa):
        raise SystemExit("nao consegui gravar a placa")
    n_larg = alargar_finas(PLACA)
    if n_larg:
        print(f"{n_larg} trilha(s) alargadas ate {LARGURA_MINIMA} mm", flush=True)

    conferida = pcbnew.LoadBoard(str(PLACA))
    trilhas = [t for t in conferida.GetTracks() if t.Type() == pcbnew.PCB_TRACE_T]
    vias = [t for t in conferida.GetTracks() if t.Type() == pcbnew.PCB_VIA_T]
    print(f"{PLACA.name}: {len(trilhas)} trilhas e {len(vias)} vias; rode "
          "fill_zones.py, check_pcb.py --como-esta e dry_run_pcb.py, que "
          "continuam sendo os juizes", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
