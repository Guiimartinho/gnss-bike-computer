#!/usr/bin/env python3
"""Qual peca deste projeto usa qual simbolo da biblioteca do KiCad.

O dono pediu, em 2026-09-25: "componentes melhores e nao blocos". Ate entao
as 99 pecas de dois terminais tinham simbolo de verdade e as 36 restantes -
circuitos integrados, conectores e o display - eram retangulos com pinos.

Um retangulo nao diz nada. Quem le um esquematico reconhece um receptaculo
USB-C pelo desenho da lingueta, um conector coaxial pelo circulo com o ponto,
uma chave pelo contato que abre. A biblioteca do KiCad tem 21.110 simbolos
revisados contra a KLC, e entre eles estao alguns EXATAMENTE das nossas
pecas: o ADP5091 e o MAX-M10S sao os nossos, pino por pino.

Regra para entrar nesta tabela: **o numero de cada pino tem de bater**. Nao
se adapta pinagem aqui - `ksym.checar()` roda em `check_sch.py` e falha se
sobrar ou faltar um pino, porque um simbolo bonito com o fio no pino errado
e pior que um retangulo certo.
"""
from __future__ import annotations

# peca -> "Biblioteca:Simbolo" da biblioteca oficial do KiCad
KICAD: dict[str, str] = {}

# Pinos em que o nome do nosso e o do KiCad diferem DE PROPOSITO, aceitos um
# a um. Nao e para acomodar divergencia de ficha - e para o caso em que os
# dois nomes dizem a mesma coisa e a normalizacao nao alcanca.
ALIAS: dict[str, set[str]] = {
    # Os quatro contatos de dados do USB-C: a ficha os chama D+ e D-, e este
    # projeto precisou de nome unico por contato porque quatro pinos nao
    # podem ter o mesmo nome na nossa estrutura. Mesmo sinal.
    "J101": {"A6", "B6", "A7", "B7", "S1"},
    # V_IO e VCC_IO sao o mesmo pino 7 do receptor: a ficha do MAX-F10S
    # escreve V_IO e a do MAX-M10S escreve VCC_IO. Foi conferido nas duas.
    "U301": {"7"},
}


def _p(refs, simbolo: str) -> None:
    for r in ([refs] if isinstance(refs, str) else refs):
        KICAD[r] = simbolo


# ---------------------------------------------------------------- exatos
# O simbolo do KiCad foi desenhado para este numero de encomenda: os 18
# pinos, os 18 nomes e o encapsulamento LCC-18 sao os do nosso receptor.
# O ADP5091 SAIU daqui em 2026-09-25, e o motivo e serio: o simbolo do
# KiCad e a leitura que este projeto fez da ficha DISCORDAM em seis pinos,
# em duas permutacoes de tres -
#
#     pino 2  nos SETHYST   KiCad SETSD
#     pino 4  nos SETSD     KiCad TERM
#     pino 6  nos TERM      KiCad SETHYST
#     pino 7  nos MPPT      KiCad AGND
#     pino 9  nos VIN       KiCad MPPT
#     pino 10 nos AGND      KiCad VIN
#
# Os NUMEROS batem, entao a conferencia antiga - que so comparava numeros -
# passava, e o esquematico sairia com o fio do MPPT no pino do terra. Foi
# esse curto que apareceu no netlist e levou a este achado.
#
# Uma das duas fontes esta errada, e isso vai para a placa: enquanto a ficha
# nao responder, nem o simbolo entra nem a nossa pinagem e dada por boa.
_p("U301", "RF_GPS:MAX-M10S")
_p("J101", "Connector:USB_C_Receptacle_USB2.0_16P")

# ------------------------------------------------------------ conectores
# Coaxial: o circulo com o ponto no meio, que e como se desenha desde sempre.
_p("J302", "Connector:Conn_Coaxial")

# Os conectores de fio. Sao genericos de proposito: o que importa no
# esquematico e quantas vias e em que ordem, e o corpo com os quadradinhos
# de contato diz isso melhor que um retangulo vazio.
_p("J102", "Connector:Conn_01x06_Socket")
_p("J103", "Connector:Conn_01x04_Socket")
_p("J401", "Connector:Conn_01x10_Socket")
_p("J402", "Connector:Conn_01x05_Socket")

# ------------------------------------------------------------- discretos
_p(["SW601", "SW602", "SW603"], "Switch:SW_Push")
_p(["Q401", "Q601", "Q602", "Q603"], "Device:Q_NMOS_GSD")


# ------------------------------------------------------------- na espera
# Estes passam na conferencia de pinos e QUEBRAM o desenho. Cada um fica
# aqui com a razao MEDIDA, e nao com "nao funcionou":
#
#   U103  Battery_Management:ADP5091 - e a nossa peca, 25 pinos, o simbolo
#         que a propria ADI usaria, e ele aponta o footprint LFCSP certo. O
#         MPPT (pino 7) fica na borda de BAIXO junto dos tres pinos de terra
#         (10, 12, 25), e o colocador de simbolos de alimentacao poe o GND a
#         dois passos abaixo deles. O fio do MPPT sai por baixo tambem, os
#         dois se encostam, e o KiCad junta MPPT e GND num no so. Consertar
#         e trabalho no colocador de simbolos de alimentacao.
#
#   J101  Connector:USB_C_Receptacle_USB2.0_16P - com a lingueta desenhada.
#         Ele EMPILHA pinos: A4, A9, B4 e B9 num ponto so, A1, A12, B1 e B12
#         noutro - onze pontos para dezessete contatos, com 5,08 mm de talo
#         cada. VBUS e CC1 saem pela mesma borda a quatro celulas um do
#         outro e acabam no mesmo no.
#
#   J401  Connector:Conn_01x10_Socket e
#   J402  Connector:Conn_01x05_Socket - os dez pinos do FPC do display ficam
#         TODOS na mesma borda, em x = -5,08, com o desenho em x = -1,27.
#         Sao dez fios que precisam chegar pela esquerda num corredor
#         estreito, e dois nao roteiam. Papel maior NAO resolve: com A0 a
#         folha continuou falhando, porque o aperto e local. Consertar e
#         reservar espaco do lado dos pinos na colocacao.
#
# Fora estes, faltam os que a biblioteca do KiCad nao tem e que precisam de
# simbolo desenhado: o nPM1300 (ha um de terceiros em CERN-OHL-P-2.0, em
# hlord2000/nordic-lib-kicad, com os 33 pinos batendo), o modulo ME54BS13, o
# MAX17262, o TPS7A02, o TXU0204, o BMP585, o OPT3001 e o display.
