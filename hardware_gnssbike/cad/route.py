#!/usr/bin/env python3
"""Route the board: tracks on the two outer layers, vias, and ground by plane.

How a four layer board of this kind is routed, and what this does:

  GND      is not routed as tracks. In1.Cu is a solid ground pour and F.Cu
           and B.Cu carry one too, so a ground pad is connected by the pour
           of its own face; on top of that each one gets a short stub to a
           via down into the inner plane, which is the short return path.
           The edge is stitched with a ground via about every 3 mm, because
           two planes are one plane only if they are tied together often
           enough - a tenth of a wavelength at 2.44 GHz is about 6 mm.
  signals  are maze routed on F.Cu, In2.Cu and B.Cu, with a via to change
           layer. The cost of a via is high enough that a run only changes
           layer when it has to.
  power    by the current each rail carries (02-calculos.md): 0.25 mm for
           the rails of hundreds of mA, which stay on the outer layers
           (FORA_DA_INTERNA), 0.25 for 3V0 and its branches on any layer,
           signal width for the rails of microamps to a few tens of mA.
  RF       goes FIRST, on the front layer only, at the calculated 50 ohm
           width for this stack-up (0.196 mm), with ground vias down both
           sides. First because its path is the one that is not negotiable
           and it has to leave the receiver's pin before anything else takes
           the room; front only because a via in the middle of an RF run is
           a stub, which 7.2 of the module datasheet forbids by name. The
           width still assumes the dielectric constant of ordinary FR-4 -
           the fabricator's own stack-up is an open item in
           04-pcb-e-caixa.md, and the dry-run prints the assumption.

Order matters: the switching nodes of the two bucks and of the harvester go
first and stay short, because their loop area is what radiates; then the USB
pair, together; then everything else, shortest first.

TWO GRIDS, and why. The first version kept one grid, inflated by half a
track plus the clearance, and used it for tracks and for vias alike. The
board it produced failed 974 design rules. So: one grid for what a track may
occupy, one for where the CENTRE of a via may land. Along the way, each of
these was a defect that the DRC found and the grid had not:

  a via is 0.45 mm wide where a track is 0.15, with a 0.25 mm hole that the
  grid did not model, so every via sat about 0.15 mm too close;
  pads were inflated as circles of half their longest side, which fits a
  round pad and leaves the corners of a square one bare;
  the ground stub left the pad on whatever bearing was free AT THE VIA,
  crossing whatever lay in between without asking;
  the clearance of a pad was remembered with "first come, first served", so
  a cell inside TWO pads' clearance kept only one of them and a track of the
  first ran 0.025 mm from the second;
  the USB class asks for 0.2 mm and was treated as the default 0.127;
  a 0.4 mm power track ran onto a 0.2 mm pad and stuck out on both sides;
  a via was checked at the grid cell and then drilled at the raw coordinate,
  up to half a step - 0.075 mm - away from what had been checked;
  and writing the tracks did not remove the previous run's, so routing twice
  left two sets of copper on top of each other.

What guards against the next one is conferir(): it measures the result
against itself, shape by shape, before the board is written.

Run:   python hardware_gnssbike/cad/route.py
Check: python hardware_gnssbike/cad/check_pcb.py
"""

from __future__ import annotations

import heapq
import math
import pathlib
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import footprints as FPS  # noqa: E402
import fp_load  # noqa: E402
import make_dxf as M  # noqa: E402
import make_pcb as MP  # noqa: E402

PASSO = 0.15                # routing grid, mm. 0.3 does not fit between
                            # the balls of the module's LGA: the free band
                            # between two columns is 0.45 mm wide
LARGURA = 0.15              # default track width
LARGURA_ALIM = 0.4          # ground stub width, where the pad allows it
LARGURA_MEDIA = 0.25        # the power rails and 3V0
LARGURA_USB = 0.2
# The rails by what they carry (02-calculos.md, CORRENTE of dry_run_pcb.py):
# VBUS 671 mA (the charge plus the system, through a linear charger), the
# battery 600 to 800 mA, VSYS at most 0,5 A (what its regulators deliver at
# their rated output: the charge never leaves the nPM1300 through the VSYS
# pin), 3V0 47 mA and 123 at the receiver's start; 1V8 has no load, 3V3BL
# is 16 mA, VBUSOUT a few mA through a 7,5 ohm switch, the flash 3,1 mA and
# the receiver's backup 28 uA. By IPC-2221 at 10 C and 35 um (AL2), 0,25 mm
# carries 0,87 A on an outer layer and 0,44 A on an inner one: the rails of
# more than 0,44 A take 0,25 mm and never In2.Cu, 3V0 takes 0,25 anywhere.
# Until 2026-10-19 the first ones were 0,4 mm, the width an inner layer
# would want, AND kept off the inner layer - the two margins on top of each
# other - and VBUSOUT was 0,4 mm for a limiter current it cannot pass.
MEDIOS = {"3V0", "3V0_BLOCO", "3V0_GNSS", "3V0_MOD", "3V0_SENS"}
FORA_DA_INTERNA = {"VBUS", "VSYS", "VBAT", "VBAT_SYS", "VBAT_CELULA"}
# The strip that joins each ground and tuning pin of the chip antenna to
# its tuning capacitor runs along the board edge, 0,35 mm wide (Unictron
# H2UJ4U1H2Q0100, figure 5-1, y 9,35 to 9,70).
LARGURA_FAIXA_ANTENA = 0.35
VIA_D, VIA_FURO = 0.45, 0.25

# The grid is marked for the NARROWEST track and the smallest clearance, and
# a wider or better spaced net asks for the extra radius when it looks. The
# first version inflated everything by the widest track and the widest
# clearance, and then nothing could escape a 0.5 mm pitch LGA: every net
# failed, every failure retried over the whole board, and a pass took
# forever. Two nets need centre to centre wA/2 + folga + wB/2, and marking
# wA/2 + FOLGA + LARGURA/2 while B asks for (wB - LARGURA)/2 gives exactly
# that.
# The classes of gnssbike.kicad_pro, read and not guessed: Default 0.127,
# Alimentacao 0.127 (it differs by via size, not by clearance) and USB 0.2.
# Treating USB as 0.127 is what put a ground track 0.075 mm from USB_DP.
FOLGA = 0.14                # 0.127 of the class, plus the grid
FOLGA_FURO = 0.21           # 0.2 do min_hole_clearance, mais a grade
FOLGA_USB = 0.21            # 0.2 of the USB class, plus the grid
# What a via keeps from a pad of another net. A web of solder mask between
# the two matters only where both have an opening, and the vias of this
# board have none: KiCad 8 makes them tented (PCB_VIA.IsTented(), on no mask
# layer) and the plot does not open them (GetPlotViaOnMaskLayer() is False),
# so the DRC finds no bridge at any distance and the electrical clearance is
# the rule. Until 2026-10-19 this was 0.35, for a web of 0.25 mm between two
# openings that do not exist, and it kept every via a further 0.21 mm off
# every pad: PVDD of the nPM1300, boxed between its switching pins, had no
# place for one.
FOLGA_MASCARA = FOLGA
BORDA_COBRE = 0.3           # copper to board edge
# O que um ponto da grade precisa guardar ate a borda: o isolamento de
# cobre MAIS metade da via, porque o roteador poe uma via em qualquer
# ponto onde troca de camada.
MARGEM_BORDA = 0.3 + 0.45 / 2
# Three routing layers, not two. In1.Cu is the ground plane and stays one:
# it is what the return current follows and what the GNSS antenna radiates
# against, and cutting it up to gain routing space would cost more than it
# gains. In2.Cu was EMPTY - a whole copper layer paid for and unused - while
# 77 connections had nowhere to go on the other two. A signal on In2.Cu has
# the ground plane above it and the back pour below, which is a better
# reference than either outer layer gets.
CAMADAS = ("F.Cu", "In2.Cu", "B.Cu")
NC = len(CAMADAS)
I_BCU = NC - 1
CUSTO_VIA = 16.0            # in grid steps, about 2,4 mm
# Passes after the first, each with the nets that failed in the one before
# routed first. A pass costs about five minutes on this board. Measured on
# 2026-10-19, twice: 70 failures in the first pass, 86 and 78 in the two
# after it, so the extra passes are off; the knob stays for a board where
# the order is what blocks.
PASSAGENS_EXTRA = 0
# How far a bent ground stub may look for room for its via, mm
RAIO_TERRA = 3.0
# Rip-up and reroute (uma_passagem): a net that failed is searched again over
# the copper of the nets that may be torn up, at CUSTO_MACIO a step on top of
# the usual; when the way is blocked by at most RRR_MAX_BLOQ of them, they are
# torn up, the failed net is routed first and they after it, and the change
# stays only if the failures of all of them together went down. RRR_RODADAS
# sweeps over the failed nets, RRR_SEGUNDOS at most.
#
# A change that leaves the count EQUAL may stay too, RRR_LATERAIS times in a
# pass and never twice for the same nets: the net that
# failed goes through and one of those that made room fails instead, and that
# one gets its own try on the next sweep. Taking only the changes that lower
# the count stopped the search on the first plateau - on 2026-10-19, 45 tries,
# 8 taken, and the whole sweep after them took nothing.
CUSTO_MACIO = 6.0
RRR_MAX_BLOQ = 10
RRR_RODADAS = 40
RRR_SEGUNDOS = 5400
RRR_LATERAIS = 120
# The preferred direction of each layer, as a person routes two signal layers
# of a board this shape: a step against it costs CUSTO_CONTRA more. Without
# one, the module's long nets ran down In2.Cu side by side and walled off
# every crossing between them (2026-10-19). Layer index -> "x" or "y"; empty
# leaves every layer free. In2.Cu along the board took the loose items of
# the same board from 25 to 21, and 0.5 did as well as 1.0.
DIRECAO: dict[int, str] = {1: "y"}
CUSTO_CONTRA = 0.5
# The history of the passes, after PathFinder (McMurchie and Ebeling, 1995):
# after a pass, every cell that a net which failed would have needed and
# found held by another becomes HISTORICO_PESO more expensive for everyone
# else, and the order is routed again; the nets that crossed there for no
# reason of their own go round, and the one that needed it finds it free.
# HISTORICO_PASSAGENS passes on top of the plain ones, the best one kept.
HISTORICO_PASSAGENS = 0
HISTORICO_PESO = 2.0
# The signal corridor beside the power band (decision of the owner,
# 2026-10-19). Every signal of the module, at the bottom, to the receiver,
# the display, the flash and the sensors, at the top, has to cross the power
# band, which takes the board's whole width; crossing it through the middle
# took the room the converters' own connections needed. So a signal net
# that has pads above and below the band is routed right after the
# switching nodes, and every step it takes inside the band costs
# CUSTO_FAIXA_ENERGIA more, except in the corridor (board mm, x0 y0 x1 y1):
# the left edge, which make_pcb.CORREDOR_SINAIS keeps free of parts on both
# faces from y 38 down, and up past the display connector to the band's
# top edge.
CORREDOR_SINAIS = (0.8, 33.0, 6.8, 64.0)
CUSTO_FAIXA_ENERGIA = 4.0
# Those signals keep off the front, where every chip's pins have to leave
# and the keys, the SWD header and the connectors have their pads: a step on
# F.Cu outside a pad field costs them this much more, so they run on In2.Cu
# and B.Cu. And they run as one bus: a step beside one already routed costs
# them only the fraction of the pair (perto_de in a_estrela()), so they lie
# together instead of each taking its own way across the bottom of the
# board, which is how they shut the module's console, keys and SWD lines in
# on 2026-10-19.
CUSTO_FRENTE_TRAVESSIA = 1.5
CUSTO_CURVA = 0.7             # virar 90 graus
CUSTO_CURVA_45 = 0.2          # virar 45: meia virada, meio custo
# A maze search that cannot get through explores everything it is allowed to
# before saying so, and on a 367 x 647 grid over two layers that is nearly a
# million cells for one net that was never going to route. The budget turns a
# hopeless search into two seconds instead of half a minute; what it costs is
# that a genuinely tortuous path may be given up on, and that shows in the
# report as a net left unrouted rather than as a wrong board.
ORCAMENTO = 300000   # era 90000: o tabuleiro tem ~400 mil celulas
# The last try of every connection still loose after rip-up and reroute:
# the whole board as the box and this budget, for the ways that exist but
# are long, which the budget above gives up on.
ORCAMENTO_FINAL = 2000000

BLOQUEADO = "\x00"          # a net name no net can have: blocked for everyone
# The chip antenna's keep-out forbids copper, except the antenna's own
# network: Unictron's layout guide puts the 50 ohm feed and the two tuning
# capacitors INSIDE the cut-out (make_pcb.DONO_DO_KEEPOUT). Its empty cells
# on the front layer carry this mark, which those nets may cross. Until
# 2026-10-19 the keep-out was painted BLOQUEADO over everything, the
# antenna's own pads included, and neither the feed nor the 1,7 mm from a
# pin of E301 to its tuning capacitor could be drawn.
KEEPOUT_LIVRE = "\x01"
KEEPOUT_DA_ANTENA = "KEEPOUT_ANTENA_GNSS"
REDES_DO_KEEPOUT = {"ANT_T1", "ANT_T2", "RF_CHIP"}

# The RF nets. They are routed LAST and at the calculated 50 ohm width, on
# the front layer only, with the ground pour beside them and ground vias
# along both sides: a via in the middle of an RF run is a stub, and a stub is
# what section 7.2 of the module datasheet forbids by name.
# RF_UFL and RF_CHIP are the two branches after the choice jumper: both are
# 50 ohm line and both are routed at the RF width on the front layer, even
# though only one is ever fitted. The unfitted one ends at an open pad.
NAO_ROTEAR = {"RF_IN", "RF_ANT", "RF_UFL", "RF_CHIP"}
# O par do USB tambem fica na frente: os 0,207 mm de 90 ohm foram
# calculados para microstrip em F.Cu sobre o plano de terra a 0,10 mm. Em
# In2.Cu a mesma trilha tem o plano a 0,46 mm de um lado e o despejo de B.Cu
# a 0,10 do outro, e nao e 90 ohm de nada. As amarracoes dos contatos
# repetidos ficam em In2.Cu de proposito, mas sao emendas de 1,7 mm entre
# ilhas do mesmo no, nao a linha.
SO_FRENTE = NAO_ROTEAR | {"USB_DP", "USB_DM"}
# The differential pair. They are routed one after the other, and the second
# one is drawn towards the first, so they run together instead of taking two
# unrelated paths across the board.
PAR = ("USB_DP", "USB_DM")
# Nets that go first and have to stay short: the switching loops.
PRIMEIRO = ["BUCK1_SW", "BUCK2_SW", "SW_DCDC", "SW_DCDC_L", "USB_DP", "USB_DM"]


ALIMENTACAO = ("GND", "VSYS", "VBAT", "VBAT_SYS", "VBAT_CELULA", "VBUS",
               "VBUSOUT", "3V0", "1V8", "SD3V0", "3V3BL", "VBCKP", "VINT")


def e_alimentacao(rede: str) -> bool:
    return rede in ALIMENTACAO or rede.startswith(("3V0_", "1V8_", "SD3V0_"))




# --- impedancia ------------------------------------------------------------
# The RF line has to be 50 ohm and the USB pair 90 ohm differential, and both
# datasheets say so: ME54BS13 7.2 ("strict 50 ohm characteristic impedance,
# tolerance +-10 %") and MAX-F10S integration manual 4.4 ("the impedance of
# the RF signal line must be 50 ohm; select the stack-up, copper, and
# dielectric properties of the PCB accordingly").
#
# The stack-up is in the board file, so the width is a calculation and not an
# open question: four copper layers of 35 um in 0.80 mm, which leaves
# (0.80 - 4 x 0.035) / 3 = 0.22 mm of dielectric between F.Cu and the ground
# plane on In1.Cu. What is NOT known is the dielectric constant - it is the
# fabricator's, and 4.3 is the usual value for FR-4 at these frequencies.
# That assumption is the reason the result is printed with its inputs.
ER_FR4 = 4.3                 # ASSUMIDO: confirmar com o fabricante
H_DIEL = MP.DIEL_RF           # F.Cu ao plano de terra, do empilhamento
T_CU = 0.035


def z0_microstrip(w: float, h: float = H_DIEL, er: float = ER_FR4) -> float:
    """Hammerstad's microstrip impedance, in ohm."""
    u = w / h
    ef = (er + 1) / 2 + (er - 1) / 2 * (1 + 12 / u) ** -0.5
    if u <= 1:
        return 60 / math.sqrt(ef) * math.log(8 / u + u / 4)
    return 120 * math.pi / (math.sqrt(ef) * (u + 1.393 + 0.667 * math.log(u + 1.444)))


def largura_para(z_alvo: float) -> float:
    """The width that gives this impedance, by bisection on the formula."""
    lo, hi = 0.05, 3.0
    for _ in range(60):
        meio = (lo + hi) / 2
        # a narrower track has HIGHER impedance, so overshooting the target
        # means the track has to get wider, not narrower
        if z0_microstrip(meio) > z_alvo:
            lo = meio
        else:
            hi = meio
    return round((lo + hi) / 2, 3)


LARGURA_RF = largura_para(50.0)
# A coplanar waveguide with its ground far enough away behaves as a plain
# microstrip; the datasheets ask for CPWG, so the pour stays beside the line
# with a gap of at least three widths, and the ground vias go along both
# sides. Closer than that and this number would have to be recomputed with
# the coplanar formula.
FOLGA_CPWG = 3.0 * LARGURA_RF
# The USB pair is 90 ohm DIFFERENTIAL, and that is not two 45 ohm lines: a
# 0.2 mm gap over 0.22 mm of dielectric couples them, and coupling lowers the
# differential impedance. The usual closed form for an edge-coupled
# microstrip is used here,
#
#     Zdiff = 2 x Z0 x (1 - 0.48 x exp(-0.96 x S/H))
#
# which is an approximation: the real number comes from the fabricator's
# field solver together with the real dielectric constant. Both assumptions
# are printed by the dry-run so that neither hides.
PASSO_PAR = 0.2              # gap between the two tracks of the pair


def z_diferencial(w: float, s: float = PASSO_PAR) -> float:
    return 2 * z0_microstrip(w) * (1 - 0.48 * math.exp(-0.96 * s / H_DIEL))


def largura_par(z_alvo: float = 90.0) -> float:
    lo, hi = 0.05, 3.0
    for _ in range(60):
        meio = (lo + hi) / 2
        if z_diferencial(meio) > z_alvo:
            lo = meio
        else:
            hi = meio
    return round((lo + hi) / 2, 3)


LARGURA_USB_CALC = largura_par(90.0)


# The finish, one connection at a time (the owner, 2026-10-19: one track at
# a time, and change the rule until it is 100%). A net still loose after the
# pass is tried again with its own rule eased: the narrowest track IPC-2221
# allows it on an outer layer (CORRENTE of dry_run_pcb.py, 10 C, 35 um),
# rounded up to 0,05 mm and never under the signal width, and then only on
# the outer layers. ACABAMENTO holds that width for the pass, net by net.
ACABAMENTO: dict[str, tuple[float, bool]] = {}


def afrouxar_regra(rede: str) -> bool:
    """Ease the rule of one net (ACABAMENTO). True if it changed: the width
    went down, and the net keeps to the outer layers when the new width is
    under what an inner one would ask for its current."""
    import dry_run_pcb as D
    if rede in ACABAMENTO or rede not in D.CORRENTE or rede in NAO_ROTEAR:
        return False
    corrente = D.CORRENTE[rede][0]
    minima = max(LARGURA, math.ceil(round(D.largura_ipc(corrente) / 0.05, 6)) * 0.05)
    if minima >= largura(rede) - 1e-9:
        return False
    fora = rede in FORA_DA_INTERNA or minima < D.largura_ipc(corrente, interna=True)
    ACABAMENTO[rede] = (round(minima, 3), fora)
    return True


def so_por_fora(rede: str) -> bool:
    if rede in ACABAMENTO:
        return ACABAMENTO[rede][1]
    return rede in FORA_DA_INTERNA


def largura(rede: str) -> float:
    if rede in ACABAMENTO:
        return ACABAMENTO[rede][0]
    if rede in NAO_ROTEAR:
        return LARGURA_RF
    if rede.startswith("USB_D"):
        # A largura que da 90 ohm diferenciais NESTA pilha, calculada logo
        # acima: 0,207 mm com 0,2 de afastamento. Ela cabe.
        #
        # Este comentario dizia 0,352 mm, e isso ficou aqui desde
        # 2026-09-24 sem nunca ter sido conta: 0,352 e a largura de 90 ohm
        # na pilha UNIFORME de 0,22 mm por vao, que foi abandonada no mesmo
        # commit que criou este calculo. Com os 0,10 mm de prepreg que a
        # placa tem, a mesma formula da 0,207. O numero errado sustentava um
        # argumento inteiro - "0,352 nao sai do campo de ilhas de 0,5 mm de
        # passo, entao o par e roteado mais fino e a diferenca e reportada" -
        # que simplesmente nao existe: 0,207 sai.
        return LARGURA_USB_CALC
    if rede in FORA_DA_INTERNA or rede in MEDIOS:
        return LARGURA_MEDIA
    if rede in ("ANT_T1", "ANT_T2"):
        return LARGURA_FAIXA_ANTENA
    return LARGURA


def folga_de(rede: str) -> float:
    return FOLGA_USB if rede.startswith("USB_D") else FOLGA


def extra_de(rede: str) -> float:
    """How much further than the marked halo this net has to look.

    The grid carries the narrowest track at the smallest clearance. A net
    that is wider, or that belongs to a class with more clearance, makes up
    the difference here, at the moment it looks - which costs a small disc
    per cell instead of shutting every fine-pitch escape route.
    """
    return max(0.0, (folga_de(rede) - FOLGA) + (largura(rede) - LARGURA) / 2)


def _disco_off(raio: float) -> tuple[tuple[int, int], ...]:
    """The cells a net of this net's width has to look at, beyond its own.

    A full grid step of slack, not none. The marked cells are a discrete
    set: the nearest one to a given cell can be up to a step further away
    than the true nearest point of the obstacle, and asking only about cells
    whose CENTRE is within the radius then misses it. That gap is what let a
    0.4 mm power track sit 0.110 mm from a pad that wanted 0.127.
    """
    if raio <= 1e-9:
        # nothing to make up: a cell that is not marked is already far
        # enough, because that is exactly what the marking means
        return ((0, 0),)
    lim = raio + PASSO
    n = int(math.ceil(lim / PASSO))
    return tuple((dx, dy)
                 for dx in range(-n, n + 1) for dy in range(-n, n + 1)
                 if math.hypot(dx * PASSO, dy * PASSO) <= lim + 1e-9)


class Grade:
    """Where a track may run, and where the centre of a via may land.

    Two maps, because the two things have different sizes. `t` answers "may
    a track of this net occupy this cell"; `v` answers "may a via of this
    net be centred on this cell". Both are keyed by (layer, ix, iy); the via
    map is checked on both layers, since a via goes through.
    """

    def __init__(self) -> None:
        self.nx = int(M.W / PASSO) + 1
        self.ny = int(M.H / PASSO) + 1
        self.t: dict[tuple[int, int, int], str] = {}
        self.v: dict[tuple[int, int, int], str] = {}
        self.fixo: set[tuple[int, int, int]] = set()   # a pad's own copper
        self.postas: set[tuple[int, int]] = set()      # via centres already used
        # a net let into the antenna's keep-out for a moment: the ground of
        # a tuning capacitor finding its way out (terra())
        self.ko_extra: str | None = None

    def cel(self, x: float, y: float) -> tuple[int, int]:
        return (int(round(x / PASSO)), int(round(y / PASSO)))

    def pos(self, ix: int, iy: int) -> tuple[float, float]:
        return (ix * PASSO, iy * PASSO)

    def dentro(self, ix: int, iy: int) -> bool:
        """Can a track or a via CENTRE sit here?

        The margin is not BORDA_COBRE. BORDA_COBRE is what the DRC asks
        between COPPER and the board outline, and what sits on a grid point
        is a centre line: a via of 0,45 mm centred 0,3 mm from the edge
        leaves copper at 0,075, and the DRC says so. The router also drops a
        via wherever it changes layer, so every grid point has to hold the
        widest thing that can land on it, which is the via and not the
        track. That costs 0,225 mm of routable area all round, and it is the
        difference between a board that passes the DRC and one that does
        not - two errors of exactly this kind, a GND track at 0,100 mm and a
        stitching via at 0,075 mm, are what put this comment here.
        """
        x, y = self.pos(ix, iy)
        if x < MARGEM_BORDA or y < MARGEM_BORDA or \
                x > M.W - MARGEM_BORDA or y > M.H - MARGEM_BORDA:
            return False
        # the notch under the module's antenna is not board: copper there is
        # copper hanging in the air, and the DRC calls it what it is
        for nome, (rx0, ry0, rx1, ry1), _c, _s in M.ZONES:
            if nome != "RECORTE_ANTENA_MODULO":
                continue
            if rx0 - MARGEM_BORDA < x < rx1 + MARGEM_BORDA and \
                    ry0 - MARGEM_BORDA < y < ry1 + MARGEM_BORDA:
                return False
        r = M.RADIUS_DRAWING
        for cx, cy in ((r, r), (M.W - r, r), (r, M.H - r), (M.W - r, M.H - r)):
            fora_x = x < r if cx < M.W / 2 else x > M.W - r
            fora_y = y < r if cy < M.H / 2 else y > M.H - r
            if fora_x and fora_y and math.hypot(x - cx, y - cy) > r - MARGEM_BORDA:
                return False
        return True

    # -- marking ----------------------------------------------------------
    def _por(self, mapa: dict, k, rede: str,
             respeitar_fixo: bool = True) -> None:
        """Claim one cell for a net's clearance, and refuse to lie about it.

        A cell can fall inside the clearance of TWO different nets. Marking
        it with setdefault - which is what this did - makes it remember only
        the first, and then a track of that first net may legally run through
        it although it is 0.025 mm from the second net's pad. That was 42 of
        the board's clearance errors, and the same mistake in the via map was
        another 47.

        A cell wanted by two nets belongs to neither: nobody may put copper
        there, because whoever does is too close to the other one. Copper
        that is already a pad of some net is the exception, in `fixo`: it is
        real metal, its own net has to be able to reach it, and two pads too
        close together is a placement problem that the DRC reports on its own.
        """
        if respeitar_fixo and k in self.fixo:
            return
        d = mapa.get(k)
        if d is None:
            mapa[k] = rede
        elif d != rede:
            mapa[k] = BLOQUEADO

    def _ret(self, mapa: dict, camada: int, x: float, y: float,
             hw: float, hh: float, rede: str,
             respeitar_fixo: bool = True, raio: float = 0.0) -> None:
        """Reserve the cells of an axis-aligned rectangle, inflated already.

        A cell is a place a track CENTRE may sit, and the rectangle is
        already inflated by the clearance plus half a track. So the cells to
        reserve are exactly those whose centre falls inside it: a centre
        0.075 mm outside the boundary is 0.075 mm MORE than the clearance
        away from the copper, and it is legal.

        This used floor and ceil on the corners, reserving every cell the
        rectangle so much as touched - up to a full step, 0.15 mm, beyond
        the boundary on each side. A 0.365 mm ring became a 0.5 mm one, and
        at 0.5 mm of pad pitch the rings of two neighbouring pads met ON TOP
        of every pad, where two nets' rings make a cell blocked for all.
        Nothing could leave a fine-pitch row: not the USB pair from the
        receptacle, not CC1 from the ESD diode, not the module's pins from
        the top. The DRC, which measures real copper, never saw a problem,
        because there was none: the only thing too close was the model.
        """
        eps = 1e-6
        ix0 = int(math.floor((x - hw) / PASSO + eps)) + 1
        iy0 = int(math.floor((y - hh) / PASSO + eps)) + 1
        ix1 = int(math.ceil((x + hw) / PASSO - eps)) - 1
        iy1 = int(math.ceil((y + hh) / PASSO - eps)) - 1
        # With `raio`, the rectangle was inflated by that much and its corners
        # are round, as the clearance round a pad's corner is: the square
        # corners held cells up to 0.2 mm too far, and between two pins of a
        # QFN the via of PVDD, 0.144 mm from the corner of the next pin and
        # legal, sat in that corner of the model (2026-10-19)
        nx, ny = hw - raio, hh - raio
        for ix in range(ix0, ix1 + 1):
            for iy in range(iy0, iy1 + 1):
                if raio:
                    ddx = max(abs(ix * PASSO - x) - nx, 0.0)
                    ddy = max(abs(iy * PASSO - y) - ny, 0.0)
                    if math.hypot(ddx, ddy) > raio + eps:
                        continue
                self._por(mapa, (camada, ix, iy), rede, respeitar_fixo)

    def _disco(self, mapa: dict, camada: int, x: float, y: float,
               raio: float, rede: str, respeitar_fixo: bool = True) -> None:
        # measured from the true centre, not from the cell it falls in: a
        # via drawn by hand off the grid (acabar.MAO) had its halo 0.07 mm
        # off, and a track of CC2 went 0.103 mm from it (2026-10-19)
        n = int(math.ceil(raio / PASSO)) + 1
        cx, cy = self.cel(x, y)
        for dx in range(-n, n + 1):
            for dy in range(-n, n + 1):
                if math.hypot((cx + dx) * PASSO - x, (cy + dy) * PASSO - y) > raio + 1e-9:
                    continue
                self._por(mapa, (camada, cx + dx, cy + dy), rede,
                          respeitar_fixo)

    def cobre(self, camadas, x: float, y: float, hw: float, hh: float,
              rede: str) -> None:
        """A pad's own copper: real metal, and its net has to reach it."""
        for c in camadas:
            ix0, iy0 = self.cel(x - hw, y - hh)
            ix1, iy1 = self.cel(x + hw, y + hh)
            for ix in range(ix0, ix1 + 1):
                for iy in range(iy0, iy1 + 1):
                    k = (c, ix, iy)
                    self.t[k] = rede
                    self.fixo.add(k)

    def pad(self, camadas, x: float, y: float, hw: float, hh: float,
            rede: str, folga: float = FOLGA) -> None:
        """A pad, as the rectangle it is, into both maps.

        A track has to keep FOLGA from the copper, so its centre line has to
        keep FOLGA + half a track. A via has to keep FOLGA from the copper
        with its 0.45 mm pad, AND 0.2 mm from the copper with its 0.25 mm
        hole; the first is the larger, so it decides.
        """
        for c in camadas:
            self._ret(self.t, c, x, y, hw + folga + LARGURA / 2,
                      hh + folga + LARGURA / 2, rede, raio=folga + LARGURA / 2)
        # a via goes through: a pad on any layer blocks it on all of them
        # respeitar_fixo=False on purpose. In the track map that exemption
        # is what lets a net reach its own pad; in the VIA map there is no
        # such thing to protect, and keeping it there let a ground stub drop
        # a via 0.075 mm from a neighbouring pad - eleven of the board's
        # nineteen clearance errors.
        # and the class of the pad's net decides it, like for a track: a
        # VBUSOUT via sat 0.175 mm from a USB_DP pad of the module, where the
        # USB class asks 0.2 (2026-10-19)
        fv = max(FOLGA_MASCARA, folga)
        for c in range(NC):
            self._ret(self.v, c, x, y, hw + fv + VIA_D / 2,
                      hh + fv + VIA_D / 2, rede,
                      respeitar_fixo=False, raio=fv + VIA_D / 2)

    def trilha(self, camada: int, p0, p1, larg: float, rede: str,
               folga: float = FOLGA) -> None:
        """Reserve a run of track on both maps, along its whole length.

        `folga` exists because the clearance is not one number for the whole
        board: the USB class asks 0.2 mm where the default class asks 0.127.
        Reserving the default around a USB track is how the router put a
        sensor line 0.11 mm from a USB via and the DRC found it afterwards.
        """
        n = max(1, int(math.ceil(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / (PASSO / 2))))
        rt = larg / 2 + folga + LARGURA / 2
        rv = larg / 2 + folga + VIA_D / 2
        for i in range(n + 1):
            f = i / n
            x = p0[0] + (p1[0] - p0[0]) * f
            y = p0[1] + (p1[1] - p0[1]) * f
            self._disco(self.t, camada, x, y, rt, rede)
            for c in range(NC):
                self._disco(self.v, c, x, y, rv, rede, respeitar_fixo=False)

    def via(self, x: float, y: float, rede: str,
            folga: float = FOLGA) -> None:
        """Reserve a via: its pad on both layers, and room for the next one.

        Two vias have to keep their holes 0.2 mm apart and their pads 0.2 mm
        apart; the pads decide, so the next via centre stays VIA_D + FOLGA
        away. `folga` follows the net's class, for the same reason it does
        in trilha().
        """
        self.postas.add(self.cel(x, y))
        for c in range(NC):
            self._disco(self.t, c, x, y, VIA_D / 2 + folga + LARGURA / 2, rede)
            self._disco(self.v, c, x, y, VIA_D + folga, rede,
                        respeitar_fixo=False)

    def bloquear(self, camada: int, x: float, y: float, raio: float) -> None:
        self._disco(self.t, camada, x, y, raio, BLOQUEADO)
        self._disco(self.v, camada, x, y, raio + VIA_D / 2, BLOQUEADO)

    # -- asking -----------------------------------------------------------
    def livre_t(self, k, rede: str, off=((0, 0),)) -> bool:
        c, ix, iy = k
        t = self.t
        for dx, dy in off:
            d = t.get((c, ix + dx, iy + dy))
            if d is not None and d != rede and not (
                    d == KEEPOUT_LIVRE
                    and (rede in REDES_DO_KEEPOUT or rede == self.ko_extra)):
                return False
        return True

    def livre_macio(self, k, rede: str, off, macio: dict) -> bool:
        """livre_t, except that a cell held only by copper that may be torn
        up (`macio`: cell -> the nets whose tracks or vias claim it) counts
        as free. The rip-up search walks over it to find who is in the way."""
        c, ix, iy = k
        t = self.t
        for dx, dy in off:
            kk = (c, ix + dx, iy + dy)
            d = t.get(kk)
            if d is None or d == rede or kk in macio:
                continue
            if d == KEEPOUT_LIVRE and (rede in REDES_DO_KEEPOUT or rede == self.ko_extra):
                continue
            return False
        return True

    def cabe_via(self, ix: int, iy: int, rede: str) -> bool:
        # Two vias of the SAME net still may not share a hole. The ground
        # stubs put five pairs of vias exactly on top of each other, at
        # 0.000 mm, because the via map only ever asked about other nets.
        n = int(math.ceil((VIA_D + FOLGA) / PASSO))
        for dx in range(-n, n + 1):
            for dy in range(-n, n + 1):
                if math.hypot(dx * PASSO, dy * PASSO) > VIA_D + FOLGA:
                    continue
                if (ix + dx, iy + dy) in self.postas:
                    return False
        for c in range(NC):
            d = self.v.get((c, ix, iy))
            if d is not None and d != rede:
                return False
        return True

    def corredor_livre(self, camada: int, p0, p1, rede: str) -> bool:
        """Can this net's track run from p0 to p1 without touching anyone
        else? Sampled at half a grid step, which is what the marking uses, so
        what passes here is what gets reserved there."""
        off = _disco_off(extra_de(rede))
        d = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        n = max(1, int(math.ceil(d / (PASSO / 2))))
        for i in range(n + 1):
            f = i / n
            x = p0[0] + (p1[0] - p0[0]) * f
            y = p0[1] + (p1[1] - p0[1]) * f
            cx, cy = self.cel(x, y)
            if not self.dentro(cx, cy):
                return False
            if not self.livre_t((camada, cx, cy), rede, off):
                return False
        return True


# The thermal pads no via may touch, the same four the AL6 of dry_run_pcb.py
# checks with the same 0,5 mm margin: a via under one sucks the solder out of
# the joint in the oven, and under the TPS7A02 the datasheet forbids it. Until
# 2026-10-18 nothing here knew them, and the OPT3001's new exposed pad got a
# ground via 1,0 mm from its centre on the first run.
TERMICOS_SEM_VIA = {("U104", "5"), ("U103", "25"), ("U505", "7")}
MARGEM_TERMICO = 0.5
# filled by pads_da_placa(): (x, y, half w, half h) of each pad above
PADS_TERMICOS: list[tuple[float, float, float, float]] = []
# The thermal pads that take a matrix of ground vias instead, n x n at a
# pitch, as the maker's reference layout draws them. The nPM1300's QFN has
# 3 x 3 in its exposed pad (PS v1.1, figures 61 to 64): the heat of a
# charger that dissipates up to 1,5 W goes down them into the plane. Until
# 2026-10-19 the pad was in the list above, on the word of the TPS7A02 and
# OPT3001 datasheets, which do not speak for it, and it reached ground only
# where the front pour found a way out between its pins.
MATRIZ_TERMICA = {("U101", "33"): (3, 1.0)}
# Ground pads whose only way to ground is the ground pad next to them,
# joined by a track drawn as it is: ("REF.pad", "REF.pad", width), and the
# first of the two gets no via of its own. ALRT of the MAX17262 is the middle
# bump of a 3 x 3 WLP at 0.4 mm, tied to GND since 2026-10-19 (nets.py), and
# the GND bump is its diagonal neighbour: 0.1 mm of track keeps 0.133 mm from
# the two round bumps it passes between, and gnssbike.kicad_dru allows that
# width inside U102 and nowhere else.
PONTES = [("U102.B2", "U102.C3", 0.1)]
# filled by pads_da_placa(): (x, y) of each pad above -> (n, pitch)
VIAS_NO_TERMICO: dict[tuple[float, float], tuple[int, float]] = {}
# filled by pads_da_placa(): the pad field (x0, y0, x1, y1) of every
# footprint with at least this many pads, the chips whose pins have to
# escape; terra() puts the ground vias round them on the outside
PADS_DENSO = 8
CAIXAS_DENSAS: list[tuple[float, float, float, float]] = []
# The GNSS receiver, and the box of its body in board coordinates (x0, y0,
# x1, y1) as pads_da_placa() finds it, from footprints.CORPO: the area
# fechar_sob_gnss() closes to foreign signals on the front
RECEPTOR = "U301"
CORPO_DO_RECEPTOR: list[float] = []
# The footprints whose inner pads are escaped before anything is routed
# (leque()), and their pads as pads_da_placa() finds them
LEQUE = {"U201"}
PADS_DO_LEQUE: dict[str, list[tuple]] = {}
# every pad by "REF.number", as pads_da_placa() finds it, for GUIAS
PAD_POR_NOME: dict[str, tuple] = {}
# id() of every round pad, as pads_da_placa() finds them: a track that ends
# in the corner of a round pad's box is not on its copper
PADS_REDONDOS: set[int] = set()
# The connections finished by hand (option 2 of 2026-10-19: "terminar a
# mao"), written here instead of in KiCad's interactive router so the board
# can still be generated from these files. Each is (net, "REF.pad",
# "REF.pad", points): the points, in board mm, are where a person would make
# the track pass, and the search draws every stretch between two of them
# with the same clearances as any other track; a point may carry a layer
# name to pin it. They go right after the switching nodes, whole or not at
# all, and rip-up never tears them up.
GUIAS: list[tuple[str, str, str, list[tuple]]] = []


def pads_da_placa(arv) -> tuple[list[tuple], dict[str, list[tuple]], dict]:
    """Every pad: (net, layer index or -1 for through, x, y, half w, half h).

    The half sizes are the rotated ones: a 1.5 x 0.7 pad turned 90 degrees is
    0.7 x 1.5, and marking it the other way round is how a track ends up
    running through a pad it should have gone around.
    """
    todos = []
    por_rede: dict[str, list[tuple]] = {}
    # the bounding box of the pads of each footprint, looked up by the
    # position of any one of them: that box is the pad field the neck-down
    # has to cover, and no wider neighbourhood is
    caixa_fp: dict[tuple[float, float], tuple] = {}
    PADS_TERMICOS.clear()
    VIAS_NO_TERMICO.clear()
    CAIXAS_DENSAS.clear()
    CORPO_DO_RECEPTOR.clear()
    PADS_DO_LEQUE.clear()
    PAD_POR_NOME.clear()
    PADS_REDONDOS.clear()
    for f in fp_load.kids(arv, "footprint"):
        at = fp_load.kid(f, "at")
        fx, fy = float(at[1]), float(at[2])
        ang = math.radians(float(at[3])) if len(at) > 3 else 0.0
        ref_fp = next((pr[2] for pr in fp_load.kids(f, "property")
                       if len(pr) > 2 and pr[1] == "Reference"), "")
        if ref_fp == RECEPTOR and f[1] in FPS.CORPO:
            w, h = FPS.CORPO[f[1]][:2]
            if round(math.degrees(ang)) % 180:
                w, h = h, w
            CORPO_DO_RECEPTOR.extend((fx - w / 2 - MP.ORIGEM[0], fy - h / 2 - MP.ORIGEM[1],
                                      fx + w / 2 - MP.ORIGEM[0], fy + h / 2 - MP.ORIGEM[1]))
        meus: list[tuple[float, float, float, float]] = []
        for p in fp_load.kids(f, "pad"):
            a = fp_load.kid(p, "at")
            px, py = float(a[1]), float(a[2])
            gx = fx + px * math.cos(ang) + py * math.sin(ang)
            gy = fy - px * math.sin(ang) + py * math.cos(ang)
            s = fp_load.kid(p, "size")
            sw, sh = float(s[1]) / 2.0, float(s[2]) / 2.0
            # A custom pad's (size ...) is only its anchor: the copper lives
            # in (primitives ...) and can be much bigger. On the TPS7A02 in
            # X2SON the anchor is 0.148 mm square and the copper reaches
            # 0.46 x 0.31, so reading the size alone leaves 0.16 mm of real
            # copper unguarded and a track may cross it.
            if len(p) > 3 and p[3] == "custom":
                prim = fp_load.kid(p, "primitives")
                if prim:
                    px_, py_ = [], []
                    for g in prim:
                        if not isinstance(g, list):
                            continue
                        pts = fp_load.kid(g, "pts")
                        if pts:
                            for q in fp_load.kids(pts, "xy"):
                                px_.append(float(q[1]))
                                py_.append(float(q[2]))
                        for tag in ("start", "end", "center", "mid"):
                            q = fp_load.kid(g, tag)
                            if q:
                                px_.append(float(q[1]))
                                py_.append(float(q[2]))
                    if px_:
                        sw = max(sw, abs(min(px_)), abs(max(px_)))
                        sh = max(sh, abs(min(py_)), abs(max(py_)))
            # the pad angle in a board file is already the final one
            pa = math.radians(float(a[3])) if len(a) > 3 else 0.0
            hw = abs(sw * math.cos(pa)) + abs(sh * math.sin(pa))
            hh = abs(sw * math.sin(pa)) + abs(sh * math.cos(pa))
            camadas = list(fp_load.kid(p, "layers")[1:])
            # A pad on the paste layer only is a stencil window, not copper.
            # The nine of the nPM1300's exposed pad went in as copper of no
            # net, sitting on the pad itself: its ground stub could not leave
            # its centre, and no track or via of anyone could come near.
            if not any(c.endswith("Cu") for c in camadas):
                continue
            # "connect" is an SMD pad without paste - the Tag-Connect's six -
            # and it lives on ONE face like any SMD pad. Treating everything
            # that is not "smd" as through-hole let the search end a track
            # on In2.Cu or B.Cu at a pad that only exists on F.Cu: three
            # dangling tracks on SWDIO, SWDCLK and MOD_RESET, counted as
            # routed while the pin had no copper reaching it.
            passante = (any(c.startswith("*") for c in camadas)
                        or p[2] in ("thru_hole", "np_thru_hole"))
            if passante:
                idx = -1
            elif any("B.Cu" in c for c in camadas):
                idx = I_BCU
            else:
                idx = 0
            rede = fp_load.kid(p, "net")
            nome = rede[2] if rede else ""
            item = (nome, idx, gx, gy, hw, hh)
            if len(p) > 3 and p[3] == "circle":
                PADS_REDONDOS.add(id(item))
            todos.append(item)
            if ref_fp in LEQUE:
                PADS_DO_LEQUE.setdefault(ref_fp, []).append(item)
            PAD_POR_NOME[f"{ref_fp}.{p[1]}"] = item
            if (ref_fp, str(p[1])) in TERMICOS_SEM_VIA:
                PADS_TERMICOS.append((gx, gy, hw, hh))
            if (ref_fp, str(p[1])) in MATRIZ_TERMICA:
                VIAS_NO_TERMICO[(round(gx, 4), round(gy, 4))] = MATRIZ_TERMICA[(ref_fp, str(p[1]))]
            meus.append((gx, gy, hw, hh))
            if nome:
                por_rede.setdefault(nome, []).append(item)
        if meus:
            b = (min(q[0] - q[2] for q in meus), min(q[1] - q[3] for q in meus),
                 max(q[0] + q[2] for q in meus), max(q[1] + q[3] for q in meus))
            for gx, gy, _hw, _hh in meus:
                caixa_fp[(round(gx, 4), round(gy, 4))] = b
            if len(meus) >= PADS_DENSO:
                CAIXAS_DENSAS.append(b)
    return todos, por_rede, caixa_fp


def longe_dos_pads(g: Grade, ix: int, iy: int) -> bool:
    """No pad copper, of any net and on any layer, within a via's radius
    plus the clearance: the same gap terra() leaves between a pad and the
    via its straight stub ends on, so the via never eats into a pad."""
    r = VIA_D / 2 + FOLGA
    n = int(math.ceil(r / PASSO))
    for dx in range(-n, n + 1):
        for dy in range(-n, n + 1):
            if math.hypot(dx * PASSO, dy * PASSO) > r:
                continue
            for c in range(NC):
                if (c, ix + dx, iy + dy) in g.fixo:
                    return False
    return True


def a_estrela(g: Grade, rede: str, inicio: tuple[int, int, int],
              alvos: set[tuple[int, int, int]], folga: int = 200,
              orcamento: int = ORCAMENTO, so_camada: int | None = None,
              perto_de: frozenset | None = None,
              campos: list | None = None, larg_estreita: float = LARGURA,
              fim_via: bool = False, macio: dict | None = None,
              camadas: tuple | None = None, custo_extra: dict | None = None,
              isento: frozenset = frozenset(), inicios: set | None = None,
              faixa: tuple | None = None, custo_frente: float = 0.0):
    """Shortest path from one cell to any target, changing layer at a cost.

    `custo_frente` is what a step on F.Cu costs on top of the usual outside
    the pad fields in `campos` (CUSTO_FRENTE_TRAVESSIA).

    `faixa` is (band, corridor, extra), two boxes in CELL coordinates: a
    step inside the band and outside the corridor costs `extra` more
    (CORREDOR_SINAIS).

    `camadas`, when given, are the only layers the path may change to
    (FORA_DA_INTERNA). `custo_extra` is the history of the passes before
    (HISTORICO): what a step onto a cell costs on top of the usual, except
    on the cells in `isento`, which are the ones this net itself wanted.
    `inicios`, when given, are all the cells the path may start from - a
    piece of the net already joined, pads and copper - and `inicio` is
    ignored.

    `campos` are the pad fields of this net, in CELL coordinates. Inside one
    of them the track is allowed to be as narrow as the pad it is leaving,
    and the search has to know that or it never finds the way out: emitir()
    already cut the neck, but it only ever saw paths the search had already
    found. With one width for the whole search the USB pair at its calculated
    0.352 mm could not leave a 0.5 mm pitch receptacle at all, and came out
    with zero segments.

    With `fim_via` the search ends on the first cell where a via of this net
    fits clear of every pad, and `alvos` may be empty: that is a ground stub
    looking for its way down to the plane. With `macio` it may also walk over
    copper that can be torn up, at CUSTO_MACIO a step (rip-up and reroute).
    """
    partidas = set(inicios) if inicios else {inicio}
    comum = partidas & alvos
    if comum:
        return [next(iter(comum))]
    inicio = next(iter(partidas))
    off = _disco_off(extra_de(rede))
    # The neck is as wide as the PAD, not as the narrowest track on the
    # board, and the search has to look for exactly the width that emitir()
    # will draw. Assuming the minimum put USB_DM 0.335 mm from KEY_R where
    # the geometry needs 0.378.
    off_estreito = _disco_off(max(0.0, (folga_de(rede) - FOLGA)
                                  + (larg_estreita - LARGURA) / 2))

    def _off(ix: int, iy: int):
        if campos:
            for x0, y0, x1, y1 in campos:
                if x0 <= ix <= x1 and y0 <= iy <= y1:
                    return off_estreito
        return off
    def passa(k, off) -> bool:
        if g.livre_t(k, rede, off):
            return True
        return macio is not None and g.livre_macio(k, rede, off, macio)

    if alvos:
        tx = sum(t[1] for t in alvos) / len(alvos)
        ty = sum(t[2] for t in alvos) / len(alvos)
    else:
        tx, ty = inicio[1], inicio[2]
    bx0 = min([s[1] for s in partidas] + [t[1] for t in alvos]) - folga
    bx1 = max([s[1] for s in partidas] + [t[1] for t in alvos]) + folga
    by0 = min([s[2] for s in partidas] + [t[2] for t in alvos]) - folga
    by1 = max([s[2] for s in partidas] + [t[2] for t in alvos]) + folga

    fila = [(abs(s[1] - tx) + abs(s[2] - ty), 0.0, s, None) for s in partidas]
    heapq.heapify(fila)
    veio: dict[tuple, tuple | None] = {}
    melhor = {s: 0.0 for s in partidas}
    while fila:
        _f, custo, atual, ant = heapq.heappop(fila)
        if atual in veio:
            continue
        veio[atual] = ant
        if len(veio) > orcamento:
            return None
        if atual in alvos or (fim_via and atual not in partidas
                              and g.cabe_via(atual[1], atual[2], rede)
                              and longe_dos_pads(g, atual[1], atual[2])):
            caminho = [atual]
            while veio[caminho[-1]] is not None:
                caminho.append(veio[caminho[-1]])
            caminho.reverse()
            return caminho
        c, ix, iy = atual
        # Eight ways, not four. With four the router can only turn 90
        # degrees, so EVERY corner on the board was a right angle - which is
        # not how a board is drawn: the discontinuity is real on a fast edge
        # and on an impedance-controlled line, and it makes a longer track
        # besides. A diagonal step may not cut a corner: both of the
        # orthogonal cells it passes between have to be free too, or the
        # track would squeeze through a gap that does not exist.
        vizinhos = [(c, ix + 1, iy), (c, ix - 1, iy), (c, ix, iy + 1),
                    (c, ix, iy - 1)]
        for dx, dy in ((1, 1), (1, -1), (-1, 1), (-1, -1)):
            if passa((c, ix + dx, iy), _off(ix + dx, iy)) and \
                    passa((c, ix, iy + dy), _off(ix, iy + dy)):
                vizinhos.append((c, ix + dx, iy + dy))
        if so_camada is None:
            vizinhos += [(k, ix, iy) for k in (camadas or range(NC)) if k != c]
        for v in vizinhos:
            if v in veio:
                continue
            if not (bx0 <= v[1] <= bx1 and by0 <= v[2] <= by1):
                continue
            if not g.dentro(v[1], v[2]) or not passa(v, _off(v[1], v[2])):
                continue
            if v[0] != c:
                # a layer change is a via: every via here goes right through
                # the board, so it needs room for its pad on EVERY layer and
                # for its hole
                if not g.cabe_via(v[1], v[2], rede):
                    continue
                if not passa((v[0], ix, iy), _off(ix, iy)):
                    continue
            if v[0] != c:
                passo = CUSTO_VIA
            elif v[1] != ix and v[2] != iy:
                # A diagonal step costs more than its length. At the true
                # 1.414 the shortest path IS the diagonal, so the router
                # drew long diagonals straight across the board, cutting
                # through everyone else's channels - which is legal, passes
                # every rule, and is not how a board is drawn. At 1.9 a
                # diagonal only pays for itself where it replaces a corner,
                # which is exactly what a 45 degree chamfer is for.
                # 2.2, and the number is not taste. A diagonal run of n steps
                # costs 2.2n and covers n cells each way; the L that replaces
                # it costs 2n plus 0.7 for its one corner. At 1.9 the diagonal
                # still won for any n - which is why the board came out with
                # 45 degree lines crossing it end to end - and at 2.2 the L
                # wins from about four steps up, while a single diagonal still
                # beats a corner (2.2 against 2.7). That is a chamfer, which
                # is what 45 degrees is for.
                passo = 2.2
            else:
                passo = 1.0
                pref = DIRECAO.get(c)
                if pref and v[0] == c and ((pref == "y" and v[1] != ix)
                                           or (pref == "x" and v[2] != iy)):
                    passo += CUSTO_CONTRA
            if ant is not None and v[0] == c and ant[0] == c:
                d1 = (ix - ant[1], iy - ant[2])
                d2 = (v[1] - ix, v[2] - iy)
                if d1 != d2:
                    # Half a turn costs less than a whole one. Charging the
                    # same for both is what killed the chamfer: cutting a
                    # corner is orth -> diag -> orth, which is TWO turns, so
                    # at 0.7 each it cost 3.6 against the square corner's
                    # 2.7 and the router squared every corner on the board.
                    # A 45 degree turn is 0.2, and the chamfer comes to 2.6.
                    reto = (d1[0] == 0) != (d2[0] == 0) or                            (d1[1] == 0) != (d2[1] == 0)
                    passo += CUSTO_CURVA if (d1[0] and d1[1]) ==                         (d2[0] and d2[1]) and reto else CUSTO_CURVA_45
            if perto_de is not None and (v[1], v[2]) in perto_de:
                # the second half of a differential pair: running beside its
                # partner is cheaper than going its own way, so the two stay
                # together instead of crossing the board separately
                passo *= 0.35
            if macio is not None and not g.livre_t(v, rede, _off(v[1], v[2])):
                passo += CUSTO_MACIO
            if custo_extra and v not in isento:
                passo += custo_extra.get(v, 0.0)
            if faixa is not None:
                (fx0, fy0, fx1, fy1), (kx0, ky0, kx1, ky1), extra_f = faixa
                if fx0 <= v[1] <= fx1 and fy0 <= v[2] <= fy1 and \
                        not (kx0 <= v[1] <= kx1 and ky0 <= v[2] <= ky1):
                    passo += extra_f
            if custo_frente and v[0] == 0 and v[0] == c and \
                    _off(v[1], v[2]) is off:
                passo += custo_frente
            novo = custo + passo
            if novo < melhor.get(v, float("inf")):
                melhor[v] = novo
                h = abs(v[1] - tx) + abs(v[2] - ty)
                heapq.heappush(fila, (novo + h, novo, v, atual))
    return None


def fechar_sob_gnss(g: Grade) -> int:
    """No foreign signal crosses under the GNSS receiver on the front.

    Section 4.4 of the MAX-F10S integration manual: "It is recommended to
    ground the area below the module, on the top and second layer. Avoid
    signal lines crossing below the module at these two layers." In1.Cu is
    already solid ground, so what is left to enforce is the front.

    It runs AFTER the RF nets are routed, and that order is the whole point:
    the receiver's own RF line lives under the receiver, and closing the area
    first blocked the one net the rule exists to protect. "Signal lines
    crossing below" means somebody else's.

    "Below the module" is the receiver's BODY, from its footprint on the
    board (CORPO_DO_RECEPTOR). Until 2026-10-19 this closed the whole
    placement zone of 04-pcb-e-caixa.md, 22 x 11,5 mm, and once the receiver
    moved 2,67 mm right for the JDI light connector, that rectangle covered
    the connector's pads, its four resistors and the receiver's own pins on
    the left edge: none of them could leave on the front, and the five GNSS
    signals and three of the light's cathodes never routed.
    """
    if not CORPO_DO_RECEPTOR:
        return 0
    x0, y0, x1, y1 = CORPO_DO_RECEPTOR
    ix0, iy0 = g.cel(x0, y0)
    ix1, iy1 = g.cel(x1, y1)
    n = 0
    for ix in range(ix0, ix1 + 1):
        for iy in range(iy0, iy1 + 1):
            k = (0, ix, iy)
            if k in g.fixo or g.t.get(k) is not None:
                continue          # a pad, or copper already drawn there
            g.t[k] = BLOQUEADO
            n += 1
    return n


def base(arv, todos):
    """The grid with everything that never moves: pads, hole, keep-outs."""
    g = Grade()
    # The pad's own copper FIRST, and all of it, not only its centre cell:
    # that is what a track of its own net may stand on, and it is what makes
    # a 0.5 mm pitch connector escapable along the pad instead of between
    # two of them - which does not fit anyway, 0.15 of track and twice 0.127
    # of clearance needing 0.404 where a 0.5 pitch with 0.3 pads leaves 0.2.
    for nome, idx, x, y, hw, hh in todos:
        g.cobre(range(NC) if idx < 0 else (idx,), x - MP.ORIGEM[0],
                y - MP.ORIGEM[1], hw, hh, nome or BLOQUEADO)
    # and only then the clearance around it
    for nome, idx, x, y, hw, hh in todos:
        # A pad with no net is a hole with nothing around it - the pegs of
        # the receptacle, the three of the Tag-Connect - and the rule for
        # copper next to a HOLE is 0.2 mm, not the 0.127 of copper to
        # copper. Reserving the smaller one put SWDIO 0.175 mm from a
        # Tag-Connect hole, which the DRC reports as a hole clearance error.
        g.pad(range(NC) if idx < 0 else (idx,), x - MP.ORIGEM[0],
              y - MP.ORIGEM[1], hw, hh, nome or BLOQUEADO,
              folga=folga_de(nome) if nome else FOLGA_FURO)
    # no via of any net under a thermal pad (TERMICOS_SEM_VIA)
    for x, y, hw, hh in PADS_TERMICOS:
        for c in range(NC):
            g._ret(g.v, c, x - MP.ORIGEM[0], y - MP.ORIGEM[1],
                   hw + MARGEM_TERMICO, hh + MARGEM_TERMICO, BLOQUEADO,
                   respeitar_fixo=False)

    # EVERY mounting hole, not the first: when the second one came in on
    # 2026-09-25 this still read FUROS_DOC[0], and a track or a via was free
    # to run through the hole at (4.0; 75.75)
    for fx, fy in M.FUROS_DOC:
        for c in range(NC):
            g.bloquear(c, fx, fy, M.M2_DRILL_UNVERIFIED / 2 + FOLGA + 0.3)
    # No via inside a switching node's no-plane area: a ground via there
    # brings the plane straight back under the node, which is exactly what
    # section 13 of the AEM10900 datasheet asks to remove. The node's own
    # track is welcome; its ground is not.
    # These zones are not in the zone table: they are computed from where the
    # parts ended up, so they come off the board itself.
    folga_v = int(math.ceil((VIA_D / 2 + FOLGA) / PASSO))
    for z in fp_load.kids(arv, "zone"):
        nm = fp_load.kid(z, "name")
        if not nm or not nm[1].startswith("SEM_PLANO"):
            continue
        pts = fp_load.kid(fp_load.kid(z, "polygon"), "pts")
        xs = [float(q[1]) - MP.ORIGEM[0] for q in fp_load.kids(pts, "xy")]
        ys = [float(q[2]) - MP.ORIGEM[1] for q in fp_load.kids(pts, "xy")]
        ix0, iy0 = g.cel(min(xs), min(ys))
        ix1, iy1 = g.cel(max(xs), max(ys))
        for c in range(NC):
            for ix in range(ix0 - folga_v, ix1 + folga_v + 1):
                for iy in range(iy0 - folga_v, iy1 + folga_v + 1):
                    g.v[(c, ix, iy)] = BLOQUEADO

    for nome, (x0, y0, x1, y1), _c, _s in M.ZONES:
        if nome not in MP.KEEPOUTS:
            continue
        ix0, iy0 = g.cel(x0, y0)
        ix1, iy1 = g.cel(x1, y1)
        for c in range(NC):
            marca = KEEPOUT_LIVRE if (c == 0 and nome == KEEPOUT_DA_ANTENA) else BLOQUEADO
            for ix in range(ix0, ix1 + 1):
                for iy in range(iy0, iy1 + 1):
                    k = (c, ix, iy)
                    # a pad inside, and the clearance round it, stay its own
                    if marca == KEEPOUT_LIVRE and k in g.t:
                        continue
                    g.t[k] = marca
        # a via is 0.45 mm wide: its CENTRE has to stay that much further out
        # of a keep-out, or the pad of it reaches inside
        folga_v = int(math.ceil((VIA_D / 2 + FOLGA) / PASSO))
        for c in range(NC):
            for ix in range(ix0 - folga_v, ix1 + folga_v + 1):
                for iy in range(iy0 - folga_v, iy1 + folga_v + 1):
                    g.v[(c, ix, iy)] = BLOQUEADO
    return g


# The copper-to-copper rule of the board (0,127 mm) and a hair over it: what
# the DRC measures, not the grid's FOLGA, which carries half a step of slack
FOLGA_REAL = 0.127 + 0.005


def _dist_seg_ret(p0, p1, cx, cy, hw, hh) -> float:
    """Distance from the segment p0-p1 to a pad's rectangle (centre, half
    sizes), sampled every 0,02 mm: good to a hundredth, which is what the
    clearance rule is written in."""
    n = max(1, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / 0.02))
    melhor = 1e9
    for i in range(n + 1):
        f = i / n
        x = p0[0] + (p1[0] - p0[0]) * f
        y = p0[1] + (p1[1] - p0[1]) * f
        dx = max(abs(x - cx) - hw, 0.0)
        dy = max(abs(y - cy) - hh, 0.0)
        melhor = min(melhor, math.hypot(dx, dy))
    return melhor


def longe_das_ilhas(outras, camada: int, p0, p1, larg: float,
                    via_no_fim: bool) -> bool:
    """Does a ground stub from p0 to p1 (grid coordinates), and the via at its
    end when there is one, keep the board's clearance to every pad of another
    net? Measured on the pads themselves. The grid's 0,15 mm step cannot
    see a 0,4 mm pitch: on the TPS610997's DSBGA a straight stub passed
    0,11 mm from the 5 V ball and the DRC said so (2026-10-19)."""
    xs, ys = (p0[0], p1[0]), (p0[1], p1[1])
    for idx, qx, qy, hw, hh in outras:
        if (qx + hw < min(xs) - 1.0 or qx - hw > max(xs) + 1.0
                or qy + hh < min(ys) - 1.0 or qy - hh > max(ys) + 1.0):
            continue
        if (idx < 0 or idx == camada) and \
                _dist_seg_ret(p0, p1, qx, qy, hw, hh) < larg / 2 + FOLGA_REAL:
            return False
        if via_no_fim and _dist_seg_ret(p1, p1, qx, qy, hw, hh) < VIA_D / 2 + FOLGA_REAL:
            return False
    return True


def para_fora(x: float, y: float) -> tuple[float, float] | None:
    """Which way is OUT for a ground via at (x, y), board coordinates.

    A pad of a dense chip: away from the chip's own centre, past the edge
    its pin sits on. A pad within 4 mm of a dense chip's pad field (the
    chip's decoupling, as a rule): away from that chip. Anything else: no
    preference. Until 2026-10-19 every ground via took the first bearing
    that fitted, in a fixed order that starts with "down", and the vias of
    the nPM1300's own capacitors closed a fence round it: half of its 32
    pins never got out (the RT1 of that day).
    """
    melhor = None
    for x0, y0, x1, y1 in CAIXAS_DENSAS:
        dx = max(x0 - x, 0.0, x - x1)
        dy = max(y0 - y, 0.0, y - y1)
        d = math.hypot(dx, dy)
        if d <= 4.0 and (melhor is None or d < melhor[0]):
            melhor = (d, ((x0 + x1) / 2, (y0 + y1) / 2))
    if melhor is None:
        return None
    vx, vy = x - melhor[1][0], y - melhor[1][1]
    n = math.hypot(vx, vy)
    return (vx / n, vy / n) if n > 1e-6 else None


def vias_no_termico(g: Grade, vias, bx: float, by: float, n: int,
                    passo: float) -> int:
    """The via matrix of a thermal pad (MATRIZ_TERMICA), centred on it.

    Each via goes on the grid cell nearest its place, which moves it at most
    half a step: with a 1,0 mm pitch in a 3,45 mm pad the outer ones still
    keep 0,4 mm of the pad's copper round them.
    """
    feitas = 0
    for i in range(n):
        for j in range(n):
            c0, c1 = g.cel(bx + (i - (n - 1) / 2) * passo, by + (j - (n - 1) / 2) * passo)
            if not g.cabe_via(c0, c1, "GND"):
                continue
            vx, vy = g.pos(c0, c1)
            vias.append((vx, vy, "GND"))
            g.via(vx, vy, "GND")
            feitas += 1
    return feitas


def terra(g: Grade, por_rede, segmentos, vias, falhas,
          sem_lugar: list | None = None) -> int:
    """A stub from every ground pad to a via down into the plane.

    The stub is axis-aligned and the whole of it is checked before anything
    is drawn: a stub that leaves on a free bearing but crosses a neighbour's
    pad on the way is exactly the sort of short a plane is supposed to save
    you from. A pad with no room for a straight stub goes to `sem_lugar`, when
    given, for uma_passagem() to try a bent one.
    """
    n_gnd = 0
    # every pad of every other net, in grid coordinates, for the real check
    outras = [(q[1], q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1], q[4], q[5])
              for rede, qs in por_rede.items() if rede != "GND" for q in qs]
    em_ponte = {id(PAD_POR_NOME[a]) for a, _b, _w in PONTES if a in PAD_POR_NOME}
    for q in por_rede.get("GND", []):
        if id(q) in em_ponte:
            continue                     # it reaches ground by its bridge
        _nome, idx, x, y, hw, hh = q
        if idx < 0:
            continue                     # a through pad already meets the plane
        bx, by = x - MP.ORIGEM[0], y - MP.ORIGEM[1]
        matriz = VIAS_NO_TERMICO.get((round(x, 4), round(y, 4)))
        if matriz:
            n_gnd += vias_no_termico(g, vias, bx, by, *matriz)
            continue
        g.ko_extra = "GND" if no_keepout_da_antena(bx, by) else None
        posto = None
        # the four sides first, then the corners: a via straight out of the
        # pad gives the shortest loop, and the diagonal is what saves the
        # pads in the crowded corner by the power supply
        dirs = ((0, 1), (0, -1), (1, 0), (-1, 0),
                (0.7071, 0.7071), (0.7071, -0.7071),
                (-0.7071, 0.7071), (-0.7071, -0.7071))
        fora = para_fora(x, y)
        if fora:
            # outward first; the sort is stable, so the sides still come
            # before the corners among bearings that point out alike
            dirs = tuple(sorted(dirs, key=lambda d: -round(d[0] * fora[0] + d[1] * fora[1], 1)))
        for extra in (0.0, 0.2, 0.45, 0.8, 1.3, 2.0):
            for dx, dy in dirs:
                meia = math.hypot(hw * dx, hh * dy)
                d = meia + VIA_D / 2 + FOLGA + extra
                vx, vy = bx + dx * d, by + dy * d
                c0, c1 = g.cel(vx, vy)
                if not g.dentro(c0, c1) or not g.cabe_via(c0, c1, "GND"):
                    continue
                # the grid was asked about the CELL, so the via goes on the
                # cell. Checking at (c0, c1) and then drilling at (vx, vy)
                # puts the hole up to half a step off what was checked, and
                # half a step is 0.075 mm - thirteen of the board's fourteen
                # clearance errors were exactly that.
                vx, vy = g.pos(c0, c1)
                if not g.corredor_livre(idx, (bx, by), (vx, vy), "GND"):
                    continue
                larg_t = max(LARGURA, min(LARGURA_ALIM, 2 * hw, 2 * hh))
                if not longe_das_ilhas(outras, idx, (bx, by), (vx, vy), larg_t, True):
                    continue
                posto = (vx, vy)
                break
            if posto:
                break
        if posto is None:
            if sem_lugar is not None:
                sem_lugar.append((idx, bx, by, hw, hh))
            else:
                falhas.append(f"GND: sem lugar para a via ao lado de ({bx:.1f}; {by:.1f})")
            continue
        # the stub necks down to the pad, exactly like a signal track: a
        # 0.4 mm stub leaving a 0.3 mm ground pad sticks out on both sides and
        # lands inside the neighbouring pad's clearance
        larg_g = max(LARGURA, min(LARGURA_ALIM, 2 * hw, 2 * hh))
        vias.append((posto[0], posto[1], "GND"))
        segmentos.append(((bx, by), posto, idx, "GND", larg_g))
        g.trilha(idx, (bx, by), posto, larg_g, "GND")
        g.via(posto[0], posto[1], "GND")
        n_gnd += 1
    g.ko_extra = None
    return n_gnd


def no_keepout_da_antena(bx: float, by: float) -> bool:
    """Is this point (grid coordinates) inside the chip antenna's keep-out?
    Only the antenna's tuning capacitors have a ground pad there."""
    for nome, (x0, y0, x1, y1), _c, _s in M.ZONES:
        if nome == KEEPOUT_DA_ANTENA and x0 <= bx <= x1 and y0 <= by <= y1:
            return True
    return False


def costurar(g: Grade, vias: list) -> int:
    """Stitch the edge of the board with ground vias.

    The two ground planes are only one plane electrically if they are tied
    together often enough. The number is not taste: at 2.44 GHz a tenth of a
    wavelength in FR-4 is about 6.1 mm, and a gap larger than that turns the
    space between the planes into a slot that radiates. So: a via every
    3 mm around the edge, wherever one fits, and the measured worst gap goes
    into the dry-run.

    It runs LAST, on whatever room the signals left, so a stitch never costs
    a connection.
    """
    passo = 2.5                   # 3,0 deixou um vao de 6,1 mm depois que
                                 # a costura do RF tomou lugares de via
    # in from the edge: the via's own copper has to keep BORDA_COBRE too, and
    # dentro() only ever looked at the cell centre
    d = BORDA_COBRE + VIA_D / 2 + 0.45
    pontos: list[tuple[float, float]] = []
    n_x = max(2, int((M.W - 2 * d) / passo) + 1)
    n_y = max(2, int((M.H - 2 * d) / passo) + 1)
    for i in range(n_x):
        x = d + (M.W - 2 * d) * i / (n_x - 1)
        pontos += [(x, d), (x, M.H - d)]
    for i in range(n_y):
        y = d + (M.H - 2 * d) * i / (n_y - 1)
        pontos += [(d, y), (M.W - d, y)]
    postas = 0
    for x, y in pontos:
        achou = None
        for r in (0.0, 0.3, 0.6, 0.9, 1.2):
            for ang in range(0, 360, 45) if r else (0,):
                vx = x + r * math.cos(math.radians(ang))
                vy = y + r * math.sin(math.radians(ang))
                c0, c1 = g.cel(vx, vy)
                if not g.dentro(c0, c1) or not g.cabe_via(c0, c1, "GND"):
                    continue
                # the whole via pad inside the copper edge, not just its centre
                folga_borda = min(vx, vy, M.W - vx, M.H - vy)
                if folga_borda < BORDA_COBRE + VIA_D / 2:
                    continue
                canto = M.RADIUS_DRAWING
                longe = True
                for cx_, cy_ in ((canto, canto), (M.W - canto, canto),
                                 (canto, M.H - canto), (M.W - canto, M.H - canto)):
                    dentro_x = vx < canto if cx_ < M.W / 2 else vx > M.W - canto
                    dentro_y = vy < canto if cy_ < M.H / 2 else vy > M.H - canto
                    if dentro_x and dentro_y and math.hypot(vx - cx_, vy - cy_) >                             canto - BORDA_COBRE - VIA_D / 2:
                        longe = False
                if not longe:
                    continue
                achou = g.pos(c0, c1)
                break
            if achou:
                break
        if achou is None:
            continue
        vias.append((achou[0], achou[1], "GND"))
        g.via(achou[0], achou[1], "GND")
        postas += 1
    return postas


def costurar_area(g: Grade, vias: list) -> int:
    """Ground vias on a grid across the whole board, not only its edge.

    Edge stitching keeps the boundary of the two surface pours from becoming
    a radiating slot. It does nothing for the MIDDLE: a patch of top pour
    that reaches the internal plane only through a via 20 mm away is not
    ground at 2.4 GHz, it is an antenna with a long feed. What ties the three
    layers into one ground is a mesh, and a mesh is what this lays down.

    The pitch is the same number the edge uses and for the same reason: a
    tenth of a wavelength in FR-4 at 2.44 GHz is 6.1 mm, so anything under
    that keeps every point of pour within half a stitch of a via. 4.0 mm
    leaves margin without spending the room the signals need.

    It runs LAST, after the signals and after the edge, on whatever is free,
    so a stitch never costs a connection.
    """
    passo = 4.0
    d = BORDA_COBRE + VIA_D / 2 + 0.45
    postas = 0
    n_x = max(2, int((M.W - 2 * d) / passo) + 1)
    n_y = max(2, int((M.H - 2 * d) / passo) + 1)
    for i in range(n_x):
        for j in range(n_y):
            x = d + (M.W - 2 * d) * i / (n_x - 1)
            y = d + (M.H - 2 * d) * j / (n_y - 1)
            achou = None
            for r in (0.0, 0.4, 0.8, 1.3):
                for ang in range(0, 360, 45) if r else (0,):
                    vx = x + r * math.cos(math.radians(ang))
                    vy = y + r * math.sin(math.radians(ang))
                    c0, c1 = g.cel(vx, vy)
                    if not g.dentro(c0, c1) or not g.cabe_via(c0, c1, "GND"):
                        continue
                    if min(vx, vy, M.W - vx, M.H - vy) < BORDA_COBRE + VIA_D / 2:
                        continue
                    achou = g.pos(c0, c1)
                    break
                if achou:
                    break
            if achou is None:
                continue
            vias.append((achou[0], achou[1], "GND"))
            g.via(achou[0], achou[1], "GND")
            postas += 1
    return postas


def costurar_rf(g: Grade, vias: list, segmentos: list) -> int:
    """Ground vias along both sides of the RF line.

    A coplanar waveguide is only coplanar if the ground beside it is really
    ground: both datasheets ask for the pour around the RF line to be filled
    with ground vias, and 4.4 of the u-blox manual adds that a stub in the
    ground plane has to end in a via or it picks up interference. They go
    every 2 mm, which is well under a twentieth of a wavelength at 1.6 GHz.
    """
    postas = 0
    for (p0, p1, c, rede, _w) in list(segmentos):
        if rede not in NAO_ROTEAR:
            continue
        comp = math.hypot(p1[0] - p0[0], p1[1] - p0[1])
        n = max(1, int(comp / 2.0))
        ux, uy = (p1[0] - p0[0]) / (comp or 1), (p1[1] - p0[1]) / (comp or 1)
        nx, ny = -uy, ux                 # perpendicular
        for i in range(n + 1):
            f = i / n
            bx = p0[0] + (p1[0] - p0[0]) * f
            by = p0[1] + (p1[1] - p0[1]) * f
            for lado in (-1, 1):
                for d in (FOLGA_CPWG, FOLGA_CPWG + 0.4, FOLGA_CPWG + 0.8):
                    vx, vy = bx + nx * d * lado, by + ny * d * lado
                    c0, c1 = g.cel(vx, vy)
                    if not g.dentro(c0, c1) or not g.cabe_via(c0, c1, "GND"):
                        continue
                    pos = g.pos(c0, c1)
                    vias.append((pos[0], pos[1], "GND"))
                    g.via(pos[0], pos[1], "GND")
                    postas += 1
                    break
    return postas


def _mao(g: Grade, segmentos: list, camada: int, pts, larg: float,
         rede: str, folga: float, cells: set) -> None:
    """A hand-drawn run: emit it, reserve it, and remember its cells."""
    for p0, p1 in zip(pts, pts[1:]):
        segmentos.append((p0, p1, camada, rede, larg))
        g.trilha(camada, p0, p1, larg, rede, folga)
        n = max(2, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / (PASSO / 2)))
        for k in range(n + 1):
            f = k / n
            cells.add((camada, *g.cel(p0[0] + (p1[0] - p0[0]) * f,
                                      p0[1] + (p1[1] - p0[1]) * f)))


def _par_no_diodo(pads):
    """The two pads of a net that sit on the ESD diode: same x, 0.8 apart.

    Entry first (the lower one, larger y, facing the receptacle), exit
    second. None when the net does not cross the diode.
    """
    for q1 in pads:
        for q2 in pads:
            if q1 is q2:
                continue
            if abs(q1[2] - q2[2]) < 0.02 and abs(abs(q1[3] - q2[3]) - 0.8) < 0.05:
                return sorted((q1, q2), key=lambda q: -q[3])
    return None


def ligar_usb(g: Grade, segmentos: list, vias: list, por_rede) -> dict:
    """The USB data pair from the receptacle through the ESD diode, by hand.

    No maze search gets a USB-class track out of a 0.5 mm pitch pad row,
    and this board has two of them in series: the receptacle and the ESD
    diode. Between two pads 0.5 mm apart there are 0.20 mm of board; the
    USB class wants 0.20 mm of clearance on each side of a track, and the
    0.15 mm grid puts the track up to 0.075 mm off the pad's centre. Every
    routing run before this one ended with the pair at zero segments and
    the checker saying "not routed" - the search was never going to win.

    So the whole stretch is drawn here, like a person does:

      1. the two contacts of each signal on the receptacle are tied UNDER
         the connector, on In2.Cu. A Type-C carries D+ twice and D- twice,
         interleaved - B7 A6 A7 B6 - so joining the two D+ crosses a D-, and
         only the inner layer is free there (In1.Cu is the ground plane,
         F.Cu and B.Cu carry the pour);
      2. the INNER contact of each signal - A6 for D+, A7 for D- - rises
         to the diode's entry pad. The diode sits half a pitch to the
         right, so the two rises are parallel diagonals that never cross;
      3. the signal crosses the diode from its entry pad to the pad in
         front of it. The datasheet (SLVSBO7O, table 4-2) reserves pins 6,
         7, 9 and 10 for exactly this: "used for optional straight-through
         routing";
      4. a short stub leaves the exit pad upward, past the reach of the
         pad row's clearance, and the search takes the pair from there to
         the module on F.Cu at its 90 ohm width.

    The stubs are 0.15 mm, not the pair's width: inside a 0.5 mm field the
    narrow track is what buys the clearance. It is the neck a person draws
    at every fine-pitch connector, three millimetres long here.
    """
    ligados: dict[str, set] = {}
    xs_rec = []
    fila: dict[str, tuple] = {}
    for rede in PAR:
        linhas: dict[float, list] = {}
        for q in por_rede.get(rede, []):
            linhas.setdefault(round(q[3], 3), []).append(q)
        dois = [v for v in linhas.values() if len(v) == 2]
        if len(dois) != 1:
            return ligados
        a, b = sorted(dois[0], key=lambda q: q[2])
        fila[rede] = (a, b)
        xs_rec += [a[2], b[2]]
    centro = sum(xs_rec) / len(xs_rec) - MP.ORIGEM[0]
    # Which contact of each signal rises to the diode, and to which side
    # each exit stub opens, follow the diode's pads, not the net's name:
    # the two rises are parallel diagonals only when the contacts stand in
    # the same order as the diode pads they go to, and the stubs open
    # outward. The pair of adjacent contacts in that order nearest the
    # diode wins. Until 2026-10-19 the inner contacts and a stub to the left
    # were D+'s by name, which held only while D+ took the left channel.
    diodo = {r: _par_no_diodo([q for q in por_rede[r] if q not in fila[r]]) for r in PAR}
    sobe: dict[str, float] = {}
    lados: dict[str, float] = {}
    if all(diodo.values()):
        xe = {r: diodo[r][0][2] for r in PAR}
        melhor_s = None
        for ca in fila[PAR[0]]:
            for cb in fila[PAR[1]]:
                if abs(abs(ca[2] - cb[2]) - 0.5) > 0.05:
                    continue
                if (ca[2] < cb[2]) != (xe[PAR[0]] < xe[PAR[1]]):
                    continue
                custo = abs(ca[2] - xe[PAR[0]]) + abs(cb[2] - xe[PAR[1]])
                if melhor_s is None or custo < melhor_s[0]:
                    melhor_s = (custo, ca[2], cb[2])
        if melhor_s is not None:
            sobe = {PAR[0]: melhor_s[1] - MP.ORIGEM[0], PAR[1]: melhor_s[2] - MP.ORIGEM[0]}
        esquerda = min(PAR, key=lambda r: diodo[r][1][2])
        lados = {r: (-0.40 if r == esquerda else 0.40) for r in PAR}

    for rede in PAR:
        a, b = fila[rede]
        xa, xb = a[2] - MP.ORIGEM[0], b[2] - MP.ORIGEM[0]
        yr = a[3] - MP.ORIGEM[1]
        yb, yt = yr + a[5], yr - a[5]      # bottom and top of the pad
        cells: set = set()
        for px in (xa, xb):
            cells.add((0, *g.cel(px, yr)))

        # 1. the tie under the connector
        if rede == PAR[0]:
            va, vb = (xa - 0.35, yb + 1.45), (xb + 0.30, yb + 1.65)
        else:
            va, vb = (xa - 0.65, yb + 2.70), (xb, yb + 2.70)
        for px, v in ((xa, va), (xb, vb)):
            _mao(g, segmentos, 0, [(px, yb - 0.30), v], 0.15, rede, FOLGA_USB, cells)
            vias.append((v[0], v[1], rede))
            g.via(v[0], v[1], rede, FOLGA_USB)
            for c in range(NC):
                cells.add((c, *g.cel(v[0], v[1])))
        _mao(g, segmentos, 1, [va, vb], 0.15, rede, FOLGA_USB, cells)

        par_d = _par_no_diodo([q for q in por_rede[rede] if q not in (a, b)])
        if par_d is None:
            ligados[rede] = cells
            continue
        ent, sai = par_d
        xd = ent[2] - MP.ORIGEM[0]
        y_ent, y_sai = ent[3] - MP.ORIGEM[1], sai[3] - MP.ORIGEM[1]
        h_pad = ent[5]

        # 2. the rise from the inner contact: a diagonal, then a vertical
        x_in = sobe.get(rede, xa if abs(xa - centro) < abs(xb - centro) else xb)
        _mao(g, segmentos, 0, [(x_in, yt - 0.03), (xd, y_ent + 0.60),
                               (xd, y_ent + h_pad - 0.05)],
             0.15, rede, FOLGA_USB, cells)
        # 3. straight through the diode
        _mao(g, segmentos, 0, [(xd, y_ent), (xd, y_sai)], 0.15, rede,
             FOLGA_USB, cells)
        # 4. out the far side, clear of the row's clearance, and the two
        # stubs open up to 1.3 mm apart: at 0.5 mm the reservation rings of
        # the two nets overlap and the search cannot even start from them
        lado = lados.get(rede, -0.40 if rede == PAR[0] else 0.40)
        fim = (xd + lado, y_sai - h_pad - 1.30)
        # only the END of the exit stub is seeded: seeding its every cell
        # let the search leave from its corner and the last 0.45 mm hung
        # loose, which the DRC reports as a dangling track
        _mao(g, segmentos, 0, [(xd, y_sai), (xd, y_sai - h_pad - 0.35),
                               (xd + lado, y_sai - h_pad - 0.85), fim],
             0.15, rede, FOLGA_USB, set())
        cells.add((0, *g.cel(*fim)))
        ligados[rede] = cells

    # the CC lines cross the diode the same way; otherwise they are
    # ordinary nets and the search routes the rest of them
    for rede in ("CC1", "CC2"):
        par_d = _par_no_diodo(por_rede.get(rede, []))
        if par_d is None:
            continue
        ent, sai = par_d
        xd = ent[2] - MP.ORIGEM[0]
        cells = set()
        _mao(g, segmentos, 0, [(xd, ent[3] - MP.ORIGEM[1]),
                               (xd, sai[3] - MP.ORIGEM[1])],
             0.15, rede, FOLGA, cells)
        ligados[rede] = cells
    return ligados


def leque(g: Grade, segmentos: list, vias: list, por_rede) -> dict:
    """Escape every signal pad of the LGA matrices in LEQUE before anything
    is routed: a short stub on the front to a via at the centre of a free
    diagonal between four pads, the dog-bone a person draws under a BGA.

    Routed one net at a time, the radio module's inner pads left on the
    front between their neighbours, 0,55 mm apart, and every such track
    shut the way out of the pads beside it: on 2026-10-19 the signals that
    cross the board took the module's exits, and its own keys, console,
    buzzer and SWD lines found none. Here every signal pad gets its exit
    first, the pads with the fewest free diagonals first. Each diagonal
    centre is 0,51 mm from the four pads' corners, which leaves the via
    0,28 mm of clearance to them.

    Returns {(x, y) of the pad, as in por_rede: the cells of its via}, the
    piece of the net the search starts from and lands on.
    """
    saida: dict[tuple[float, float], set] = {}
    outras = [(q[1], q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1], q[4], q[5])
              for rede, qs in por_rede.items() for q in qs]
    for _ref, pads in sorted(PADS_DO_LEQUE.items()):
        # the matrix is the square pads; the rectangular ones on the edge
        # rows leave outward on their own
        matriz = [q for q in pads if abs(q[4] - q[5]) < 0.02]
        if len(matriz) < 4:
            continue
        xs = sorted({round(q[2], 3) for q in matriz})
        ys = sorted({round(q[3], 3) for q in matriz})
        mx = min(b - a for a, b in zip(xs, xs[1:])) / 2
        my = min(b - a for a, b in zip(ys, ys[1:])) / 2

        def livres(q) -> list[tuple[float, float]]:
            x, y = q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1]
            saida_q = []
            # upward first: the receiver, the display and the flash are up
            for dx, dy in ((-mx, -my), (mx, -my), (-mx, my), (mx, my)):
                ix, iy = g.cel(x + dx, y + dy)
                vx, vy = g.pos(ix, iy)
                if not g.dentro(ix, iy) or not g.cabe_via(ix, iy, q[0]):
                    continue
                if not longe_dos_pads(g, ix, iy):
                    continue
                if not g.corredor_livre(0, (x, y), (vx, vy), q[0]):
                    continue
                outras_q = [o for o in outras if (o[1], o[2]) != (x, y)]
                if not longe_das_ilhas(outras_q, 0, (x, y), (vx, vy), LARGURA, True):
                    continue
                saida_q.append((vx, vy))
            return saida_q

        pendentes = [q for q in matriz if q[0] and q[0] != "GND"]
        while pendentes:
            opcoes = {id(q): livres(q) for q in pendentes}
            pendentes.sort(key=lambda q: len(opcoes[id(q)]))
            q = pendentes.pop(0)
            if not opcoes[id(q)]:
                continue                 # no diagonal left: the search tries
            x, y = q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1]
            vx, vy = opcoes[id(q)][0]
            _mao(g, segmentos, 0, [(x, y), (vx, vy)], LARGURA, q[0], folga_de(q[0]), set())
            vias.append((vx, vy, q[0]))
            g.via(vx, vy, q[0], folga_de(q[0]))
            ix, iy = g.cel(vx, vy)
            saida[(round(q[2], 4), round(q[3], 4))] = {(c, ix, iy) for c in range(NC)}
    return saida


def podar_soltas(segmentos: list, vias: list, todos: list) -> int:
    """Drop every track end that touches nothing, until none is left.

    A hand-drawn stub the search did not use, a run the neck logic cut at a
    field boundary and never continued - each leaves a copper spur with a
    free end, and the DRC lists every one of them as a dangling track. An
    end is anchored when it sits on a pad of its own net (allowing the
    track's own half width, as KiCad does), on a via of its own net, or on
    the end of another segment of the net on the same layer. Anything else
    is pruned, and pruning one can free the next, so it goes round again.
    """
    orx, ory = MP.ORIGEM
    pads_por_rede: dict[str, list] = {}
    for nome, idx, x, y, hw, hh in todos:
        if nome:
            pads_por_rede.setdefault(nome, []).append((idx, x - orx, y - ory, hw, hh))
    removidos = 0
    while True:
        pontas: dict[tuple, int] = {}
        for p0, p1, cam, rede, _w in segmentos:
            for p in (p0, p1):
                k = (cam, rede, round(p[0], 3), round(p[1], 3))
                pontas[k] = pontas.get(k, 0) + 1
        vias_rede: dict[str, set] = {}
        for vx, vy, rede in vias:
            vias_rede.setdefault(rede, set()).add((round(vx, 3), round(vy, 3)))

        por_camada: dict[tuple, list] = {}
        for p0, p1, cam, rede, w in segmentos:
            por_camada.setdefault((cam, rede), []).append((p0, p1, w))

        def sobre_segmento(p, cam, rede, w):
            """The end lies ON another segment of the net, not just at its end.

            The search may finish on any cell the net already owns, and that
            is often the middle of an earlier run - the second pad of a
            three-pad net joins the first run wherever it is nearest. That
            is a T-junction, and KiCad reads it as connected. Counting only
            endpoints pruned whole routes: the USB D- run to the module went
            piece by piece, from the module back to the tie it had joined.
            """
            for q0, q1, w2 in por_camada.get((cam, rede), ()):
                dx, dy = q1[0] - q0[0], q1[1] - q0[1]
                comp2 = dx * dx + dy * dy
                if comp2 < 1e-12:
                    continue
                tt = ((p[0] - q0[0]) * dx + (p[1] - q0[1]) * dy) / comp2
                if tt < -1e-6 or tt > 1 + 1e-6:
                    continue
                px, py = q0[0] + tt * dx, q0[1] + tt * dy
                if math.hypot(p[0] - px, p[1] - py) <= (w + w2) / 2 + 0.02:
                    # the segment's own ends are not "another" segment
                    if (abs(p[0] - q0[0]) < 1e-6 and abs(p[1] - q0[1]) < 1e-6) or                             (abs(p[0] - q1[0]) < 1e-6 and abs(p[1] - q1[1]) < 1e-6):
                        continue
                    return True
            return False

        def ancorada(p, cam, rede, w):
            if pontas.get((cam, rede, round(p[0], 3), round(p[1], 3)), 0) > 1:
                return True
            if (round(p[0], 3), round(p[1], 3)) in vias_rede.get(rede, ()):
                return True
            for idx, x, y, hw, hh in pads_por_rede.get(rede, ()):
                if idx >= 0 and idx != cam:
                    continue
                # the track's end cap only has to overlap the pad, as KiCad
                # judges it, hence the half width and a little slack
                if (abs(p[0] - x) <= hw + w / 2 + 0.02
                        and abs(p[1] - y) <= hh + w / 2 + 0.02):
                    return True
            return sobre_segmento(p, cam, rede, w)

        solto = None
        for i, (p0, p1, cam, rede, w) in enumerate(segmentos):
            if not ancorada(p0, cam, rede, w) or not ancorada(p1, cam, rede, w):
                solto = i
                break
        if solto is None:
            return removidos
        p0, p1, cam, rede, w = segmentos[solto]
        if removidos < 8:
            print(f"    podado {rede} {CAMADAS[cam]} "
                  f"({p0[0]:.2f};{p0[1]:.2f})->({p1[0]:.2f};{p1[1]:.2f})")
        del segmentos[solto]
        removidos += 1


def uma_passagem(arv, numeros, todos, por_rede, caixa_fp, prioridade,
                 antes: list | tuple = (), rasgar: bool = False,
                 historico: dict | None = None, isentos: dict | None = None,
                 desejos: dict | None = None):
    """One routing attempt with a given order. Returns what came out.

    `antes` are nets routed right after PRIMEIRO, ahead of everything else:
    the ones that failed in the pass before. `historico` and `isentos` are
    the history of the passes before (HISTORICO); when `desejos` is a dict,
    it comes back with the cells each net that failed would have needed and
    found held by others.
    """
    isentos = dict(isentos or {})
    ACABAMENTO.clear()
    g = base(arv, todos)
    segmentos: list[tuple] = []
    vias: list[tuple] = []
    falhas: list[str] = []
    falharam: list[str] = []

    def emitir(caminho_cel, rede, larg, larg_pad=None, campo=None):
        i = 0
        while i < len(caminho_cel) - 1:
            a = caminho_cel[i]
            j = i + 1
            if caminho_cel[j][0] != a[0]:
                x, y = g.pos(a[1], a[2])
                vias.append((x, y, rede))
                g.via(x, y, rede, folga_de(rede))
                i = j
                continue
            d = (caminho_cel[j][1] - a[1], caminho_cel[j][2] - a[2])
            while j + 1 < len(caminho_cel) and caminho_cel[j + 1][0] == a[0] and \
                    (caminho_cel[j + 1][1] - caminho_cel[j][1],
                     caminho_cel[j + 1][2] - caminho_cel[j][2]) == d:
                j += 1
            # The neck lasts while the track is still inside the part's
            # pad field, not just for the first run: a 0.4 mm track two
            # segments out of a 0.4 mm pitch QFN is still between its pads.
            # Necking the whole net instead took VBAT, which carries the
            # 800 mA charging current, to 0.2 mm end to end, because the fuel
            # gauge's WLP bump is 0.2 mm wide.
            #
            # And the neck ends where the FIELD ends, not where the straight
            # run ends. Deciding one width for the whole run by its first
            # point is how VBAT left the MAX17262 at 0.2 mm and stayed there
            # for 9.9 mm across the board, 0.02 mm under what IPC-2221 asks
            # for its 800 mA: the run happened to begin inside the field. So
            # the run is cut at the boundary and each piece gets its own
            # width, which is what a person draws by hand.
            # `campo` is one field, or a list of them (ligar())
            def no_campo(k: int) -> bool:
                if larg_pad is None or not campo:
                    return False
                px, py = g.pos(caminho_cel[k][1], caminho_cel[k][2])
                lista = campo if isinstance(campo[0], (tuple, list)) else (campo,)
                return any(c[0] <= px <= c[2] and c[1] <= py <= c[3] for c in lista)

            k0 = i
            while k0 < j:
                # a piece is narrow when EITHER of its ends is in the field,
                # so the wide copper never starts inside it
                estreito = no_campo(k0) or no_campo(k0 + 1)
                k1 = k0 + 1
                while k1 < j and (no_campo(k1) or no_campo(k1 + 1)) == estreito:
                    k1 += 1
                p0 = g.pos(caminho_cel[k0][1], caminho_cel[k0][2])
                p1 = g.pos(caminho_cel[k1][1], caminho_cel[k1][2])
                w = larg_pad if estreito else larg
                segmentos.append((p0, p1, a[0], rede, w))
                g.trilha(a[0], p0, p1, w, rede, folga_de(rede))
                k0 = k1
            i = j

    # The hand-drawn USB stretch goes in BEFORE the ground stitching: its
    # vias are placed, not searched, and a stitching via already sitting
    # there is not checked against them. One landed 0.10 mm from a tie via.
    pre_ligados = ligar_usb(g, segmentos, vias, por_rede)
    # the ground bridges (PONTES), drawn as they are
    for a, b, w in PONTES:
        qa, qb = PAD_POR_NOME.get(a), PAD_POR_NOME.get(b)
        if qa is None or qb is None or qa[0] != qb[0] or qa[1] != qb[1]:
            continue
        p0 = (qa[2] - MP.ORIGEM[0], qa[3] - MP.ORIGEM[1])
        p1 = (qb[2] - MP.ORIGEM[0], qb[3] - MP.ORIGEM[1])
        segmentos.append((p0, p1, qa[1], qa[0], w))
        g.trilha(qa[1], p0, p1, w, qa[0])
    # the escapes of the module's matrix next, before the ground vias take
    # the diagonals; their copper is never torn up
    n_s0, n_v0 = len(segmentos), len(vias)
    saidas_leque = leque(g, segmentos, vias, por_rede)
    fixos_ids = (frozenset(map(id, segmentos[n_s0:])), frozenset(map(id, vias[n_v0:])))
    def alcance(r: str) -> float:
        xs = [q[2] for q in por_rede[r]]
        ys = [q[3] for q in por_rede[r]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))

    ordem = [r for r in PRIMEIRO if r in por_rede]
    ordem += [r for r in antes if r in por_rede and r not in ordem
              and r not in NAO_ROTEAR and r != "GND"]
    resto = [r for r in por_rede
             if r not in ordem and r not in NAO_ROTEAR and r != "GND"]
    if prioridade == "alimentacao":
        # the power rails first, the longest of them first, and then the
        # signals short first: the rails are wide and span the board, and
        # routed last they found it full (2026-10-19, 40 of the 69 loose
        # items were rails)
        trilhos = sorted((r for r in resto if e_alimentacao(r)), key=alcance, reverse=True)
        resto = trilhos + sorted((r for r in resto if not e_alimentacao(r)), key=alcance)
    else:
        resto.sort(key=alcance, reverse=(prioridade != "curtas"))

    n_ok = 0
    fechou = [False]
    caminhos: dict[str, frozenset] = {}

    def rotear(rede: str, macio: dict | None = None,
               desejo_out: dict | None = None) -> tuple[int, list, set]:
        """Every connection of one net, on the grid as it stands: the RF
        lines and the USB pair, which have rules of their own (the others go
        through ligar() and completar()).

        With `macio`, a connection the strict search cannot make is searched
        again over the copper that may be torn up, and nothing is drawn at
        all: what comes back is the set of nets in the way, and in
        `desejo_out` the cells they hold on that way.
        """
        ok, fs, bloq = 0, [], set()
        pads = por_rede[rede]
        if len(pads) < 2:
            return ok, fs, bloq
        # A track may not be wider than the pad it lands on. A 0.4 mm
        # power track ending on a 0.25 mm pad sticks out on both sides and
        # lands 0.100 mm from the neighbouring pad - seven of the nineteen
        # clearance errors. This is the neck-down a person draws by hand.
        #
        # Per CONNECTION, not per net. Taking the narrowest pad of the whole
        # net shrank all of VBAT_SYS to 0.2 mm because the TPS7A02's input
        # pad is that wide - and that pad draws microamps while the rest of
        # the rail carries the 800 mA charging current.
        estreitos = [min(2 * q[4], 2 * q[5]) for q in pads]
        if rede in PAR:
            # Numa fileira de 0,5 mm de passo quem decide a largura NAO e a
            # propria ilha, e a vizinha: entre duas ilhas de 0,30 mm a 0,5 de
            # passo sobram 0,20 mm, e a classe USB pede 0,20 de isolamento de
            # cada lado. Com a grade de 0,15 mm a trilha ainda sai ate 0,075
            # fora do centro da ilha. Medindo so a propria ilha, a trilha de
            # 0,207 passou a 0,1965 mm da vizinha e a DRC acusou tres vezes.
            estreitos = [min(e, LARGURA) for e in estreitos]
        # How far the neck has to last: out of the pad field of the package
        # the pad belongs to, and not one millimetre further. Measuring it as
        # a radius over everything within 6 mm - which is what this did - made
        # the field 4.9 mm wide around the fuel gauge, because the parts
        # crowded around it counted as if they were its own pins, and VBAT
        # left it at 0.2 mm and stayed there for 4.35 mm against the 0.22 mm
        # IPC-2221 asks of its 800 mA. The field is the box of the FOOTPRINT's
        # own pads, plus the clearance the neck exists to respect.
        campos = [caixa_fp.get((round(q[2], 4), round(q[3], 4))) for q in pads]
        celulas = []
        for _n, idx, x, y, _hw, _hh in pads:
            c0, c1 = g.cel(x - MP.ORIGEM[0], y - MP.ORIGEM[1])
            celulas.append({(c, c0, c1)
                            for c in (range(NC) if idx < 0 else (idx,))})
        # Quando a rede ja tem uma ligacao feita a mao - o par do USB-C -,
        # a busca tem de PARTIR dela, nao juntar as duas coisas num monte so:
        # semear `feito` com a ligacao feita e com a primeira ilha da lista
        # diz que as duas estao ligadas entre si, e elas nao estao. Foi assim
        # que o par ficou com as amarracoes desenhadas e nenhuma trilha ate o
        # modulo, enquanto o contador dizia que so faltava uma ligacao.
        pre = pre_ligados.get(rede, set())
        if pre:
            for i_pre, cl in enumerate(celulas):
                if cl & pre:
                    if i_pre:
                        celulas.insert(0, celulas.pop(i_pre))
                        estreitos.insert(0, estreitos.pop(i_pre))
                        campos.insert(0, campos.pop(i_pre))
                    break
        feito = set(celulas[0]) | pre
        for k, alvo in enumerate(celulas[1:], start=1):
            if alvo & feito:
                continue
            larg = largura(rede)
            # Half a grid step under the pad's width, not the pad's width:
            # the pad's centre is not on the grid, so the neck runs up to
            # 0.075 mm off it and a track as wide as the pad pokes out on
            # one side. On the fuel gauge's 0.2 mm WLP bumps a 0.2 mm neck
            # stuck out 0.05 mm and sat 0.125 mm from the next bump's track.
            larg_pad = max(LARGURA, min(larg, estreitos[k] - PASSO / 2))
            q = pads[k]
            # the pad field in grid coordinates; without one, the pad's own
            # copper, which still has to be escaped
            b = campos[k] or (q[2] - q[4], q[3] - q[5], q[2] + q[4], q[3] + q[5])
            campo = (b[0] - MP.ORIGEM[0] - FOLGA, b[1] - MP.ORIGEM[1] - FOLGA,
                     b[2] - MP.ORIGEM[0] + FOLGA, b[3] - MP.ORIGEM[1] + FOLGA)
            # the RF line stays on the front layer: a via in the middle of
            # it is a stub, and 7.2 of the module datasheet forbids stubs by
            # name. The second track of a pair is pulled towards the first.
            # every pad field of this net, in cell coordinates: inside one
            # of them the escape may be as narrow as the pad
            campos_cel = []
            for q in pads:
                b = caixa_fp.get((round(q[2], 4), round(q[3], 4)))
                if not b:
                    b = (q[2] - q[4], q[3] - q[5], q[2] + q[4], q[3] + q[5])
                a0 = g.cel(b[0] - MP.ORIGEM[0] - FOLGA, b[1] - MP.ORIGEM[1] - FOLGA)
                a1 = g.cel(b[2] - MP.ORIGEM[0] + FOLGA, b[3] - MP.ORIGEM[1] + FOLGA)
                campos_cel.append((a0[0], a0[1], a1[0], a1[1]))
            so_camada = 0 if rede in SO_FRENTE else None
            camadas = (0, I_BCU) if so_por_fora(rede) else None
            perto = None
            if rede == PAR[1] and PAR[0] in caminhos:
                perto = caminhos[PAR[0]]
            p = None
            isento = isentos.get(rede, frozenset())
            for folga in (100, 350):
                p = a_estrela(g, rede, next(iter(alvo)), feito, folga,
                              so_camada=so_camada, perto_de=perto,
                              campos=campos_cel, larg_estreita=larg_pad,
                              camadas=camadas, custo_extra=historico,
                              isento=isento)
                if p:
                    break
            if p is None and macio is not None:
                # the soft search: over the copper that may be torn up, to
                # learn whose it is; nothing is drawn
                p = a_estrela(g, rede, next(iter(alvo)), feito, 350,
                              so_camada=so_camada, perto_de=perto,
                              campos=campos_cel, larg_estreita=larg_pad,
                              macio=macio, camadas=camadas)
                if p is not None:
                    bloq |= no_caminho(g, rede, p, macio)
                    if desejo_out is not None:
                        desejo_out.setdefault(rede, set()).update(
                            celulas_ocupadas(g, rede, p, macio))
                    feito |= set(p)
                    continue
            if p is None:
                fs.append(f"{rede}: nao roteou")
                continue
            if macio is None:
                emitir(p, rede, larg, larg_pad, campo)
            feito |= set(p)
            ok += 1
            if rede == PAR[0]:
                # the cells its partner should hug: the path itself and one
                # step around it, which at a 0.15 mm grid is the pair pitch
                viz = set()
                for _c, cx_, cy_ in p:
                    for dx in (-2, -1, 0, 1, 2):
                        for dy in (-2, -1, 0, 1, 2):
                            viz.add((cx_ + dx, cy_ + dy))
                caminhos[rede] = frozenset(viz)
        return ok, fs, bloq

    # --- every other net, connection by connection (2026-10-19) ----------
    # A net routed whole goes in one go, and the order of the nets decides
    # who gets the room by a chip's pins: 3V0 spans the board, so routed
    # short-first it came last, and the 1,6 mm from pin 28 of the nPM1300 to
    # its own capacitor found a track of SD3V0 in the way. So the nets are
    # cut into the edges of their minimum spanning tree, and every edge of
    # every net goes in one list, shortest first: what joins a pin to its
    # capacitor goes before what crosses the board. A net keeps its pieces
    # (`pedacos`): the pads and copper already joined, from which the next
    # connection may start anywhere and on which it may land anywhere.
    dados: dict[str, dict] = {}
    pedacos: dict[str, list] = {}

    def dados_de(rede: str) -> dict:
        """The pads of a net, the cell of each, and its neck, worked out
        once. The neck is one width for the net, the narrowest its fine
        pitch pads ask for, and it holds only inside the fields of those
        pads: the search and emitir() then agree on where and how narrow
        the copper is, wherever the connection starts and ends."""
        d = dados.get(rede)
        if d is not None:
            return d
        pads = por_rede[rede]
        larg = largura(rede)
        celulas = []
        campos_mm = []
        campos_cel = []
        necks = []
        caixas = []
        for q in pads:
            c0, c1 = g.cel(q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1])
            celulas.append({(c, c0, c1) for c in (range(NC) if q[1] < 0 else (q[1],))})
            neck = min(2 * q[4], 2 * q[5]) - PASSO / 2
            if neck >= larg:
                continue                 # this pad takes the full width
            necks.append(neck)
            caixas.append(caixa_fp.get((round(q[2], 4), round(q[3], 4))) or
                          (q[2] - q[4], q[3] - q[5], q[2] + q[4], q[3] + q[5]))
        # The neck is the signal width, not the pad's width less half a step:
        # anything wider than LARGURA has the grid ask for the next cell over
        # to be free as well (_disco_off), and between two pins of a 0,5 mm
        # QFN it never is. A 0,25 mm VSYS could not leave PVDD, nor VBUS,
        # VBAT_SYS and 3V0 the pins of the nPM1300, although the copper fits
        # (2026-10-19). The neck stays inside the package's field, and AL2 of
        # the dry run takes it for what it is.
        larg_pad = LARGURA if necks else larg
        # The neck lasts until the full width is as far from the package's
        # pads as the neck was at the edge of their field: the clearance,
        # half of what the track widens by, and a step of grid. Ending it at
        # the clearance alone, a 0,4 mm rail could not leave the TPS7A02: at
        # the field's edge its full width still sat in the thermal pad's
        # halo, and VBAT_SYS never reached the capacitor 1,2 mm away - nor
        # VSYS, VBUS and 3V0 the pins of the nPM1300 (2026-10-19).
        margem = FOLGA + (larg - larg_pad) / 2 + PASSO
        for b in caixas:
            m = (b[0] - MP.ORIGEM[0] - margem, b[1] - MP.ORIGEM[1] - margem,
                 b[2] - MP.ORIGEM[0] + margem, b[3] - MP.ORIGEM[1] + margem)
            if m not in campos_mm:
                campos_mm.append(m)
                a0 = g.cel(m[0], m[1])
                a1 = g.cel(m[2], m[3])
                campos_cel.append((a0[0], a0[1], a1[0], a1[1]))
        d = {"pads": pads, "celulas": celulas, "larg": larg, "larg_pad": larg_pad,
             "campos_mm": campos_mm, "campos_cel": campos_cel}
        dados[rede] = d
        return d

    def pedacos_de(rede: str) -> list:
        """[pad indices, cells] of every piece of the net; at first, one
        piece per pad, which for a pad of the module's matrix is the via of
        its escape (leque())."""
        if rede not in pedacos:
            d = dados_de(rede)
            pedacos[rede] = [
                [{i}, set(saidas_leque.get((round(q[2], 4), round(q[3], 4)), c))]
                for i, (q, c) in enumerate(zip(d["pads"], d["celulas"]))]
        return pedacos[rede]

    # The power band and the signal corridor beside it, in cells, and the
    # signal nets that cross the band (CORREDOR_SINAIS)
    zona_e = next(r_ for nome_, r_, _c, _s in M.ZONES if nome_ == "ZONA_ENERGIA")
    fb0, fb1 = g.cel(zona_e[0], zona_e[1]), g.cel(zona_e[2], zona_e[3])
    fk0 = g.cel(CORREDOR_SINAIS[0], CORREDOR_SINAIS[1])
    fk1 = g.cel(CORREDOR_SINAIS[2], CORREDOR_SINAIS[3])
    faixa_cel = ((fb0[0], fb0[1], fb1[0], fb1[1]), (fk0[0], fk0[1], fk1[0], fk1[1]),
                 CUSTO_FAIXA_ENERGIA)

    def atravessa(rede: str) -> bool:
        if (e_alimentacao(rede) or rede in NAO_ROTEAR or rede in PAR
                or rede in PRIMEIRO or rede == "GND"):
            return False
        ys = [q[3] - MP.ORIGEM[1] for q in por_rede[rede]]
        return min(ys) < zona_e[1] and max(ys) > zona_e[3]

    atravessam = {r for r in por_rede if atravessa(r)}

    # the cells beside the signals that cross the band already routed:
    # the next one is pulled into the same bus
    barramento: set = set()

    def ligar(rede: str, origem: list, destino: set, macio: dict | None = None,
              desejo_out: dict | None = None, bloq: set | None = None,
              orcamento: int = ORCAMENTO):
        """One connection, from any cell of the piece `origem` to any cell of
        `destino`. Returns the path, or None; with `macio`, the path over
        the copper that may be torn up, never drawn."""
        d = dados_de(rede)
        camadas = (0, I_BCU) if so_por_fora(rede) else None
        isento = isentos.get(rede, frozenset())
        faixa = faixa_cel if rede in atravessam else None
        frente = CUSTO_FRENTE_TRAVESSIA if rede in atravessam else 0.0
        perto = frozenset(barramento) if rede in atravessam and barramento else None
        comeco = next(iter(origem[1]))
        p = None
        folgas = (100, 350) if orcamento <= ORCAMENTO else (900,)
        for folga in folgas:
            p = a_estrela(g, rede, comeco, destino, folga, orcamento=orcamento,
                          campos=d["campos_cel"], larg_estreita=d["larg_pad"],
                          camadas=camadas, custo_extra=historico,
                          isento=isento, inicios=origem[1], faixa=faixa,
                          custo_frente=frente, perto_de=perto)
            if p:
                break
        if p is None and macio is not None:
            p = a_estrela(g, rede, comeco, destino, 350,
                          campos=d["campos_cel"], larg_estreita=d["larg_pad"],
                          macio=macio, camadas=camadas, inicios=origem[1],
                          faixa=faixa, custo_frente=frente)
            if p is not None:
                if bloq is not None:
                    bloq |= no_caminho(g, rede, p, macio)
                if desejo_out is not None:
                    desejo_out.setdefault(rede, set()).update(
                        celulas_ocupadas(g, rede, p, macio))
            return p
        if p is not None and macio is None:
            emitir(p, rede, d["larg"], d["larg_pad"], d["campos_mm"])
        return p

    def arvore(rede: str) -> list[tuple[float, int, int]]:
        """The edges of the minimum spanning tree of the net's pads, by the
        distance between pad centres (Prim)."""
        pads = dados_de(rede)["pads"]
        n = len(pads)
        if n < 2:
            return []
        dentro = {0}
        arestas = []
        perto = {j: (math.hypot(pads[j][2] - pads[0][2], pads[j][3] - pads[0][3]), 0)
                 for j in range(1, n)}
        while perto:
            j = min(perto, key=lambda k: perto[k][0])
            dist, i = perto.pop(j)
            arestas.append((dist, i, j))
            dentro.add(j)
            for k in perto:
                dk = math.hypot(pads[k][2] - pads[j][2], pads[k][3] - pads[j][3])
                if dk < perto[k][0]:
                    perto[k] = (dk, j)
        return arestas

    def ligar_aresta(rede: str, i: int, j: int) -> bool:
        ps = pedacos_de(rede)
        pi = next(pc for pc in ps if i in pc[0])
        pj = next(pc for pc in ps if j in pc[0])
        if pi is pj:
            return True
        p = ligar(rede, pi, pj[1])
        if p is None:
            return False
        pi[0] |= pj[0]
        pi[1] |= pj[1] | set(p)
        ps.remove(pj)
        return True

    def completar(rede: str, macio: dict | None = None,
                  desejo_out: dict | None = None,
                  orcamento: int = ORCAMENTO) -> tuple[int, list, set]:
        """Join every piece of the net still apart to the biggest one,
        nearest first. With `macio`, on a copy and nothing drawn."""
        ok, fs, bloq = 0, [], set()
        ps = pedacos_de(rede)
        if macio is not None:
            ps = [[set(a), set(b)] for a, b in ps]
        if len(ps) < 2:
            return ok, fs, bloq
        pads = dados_de(rede)["pads"]
        ps.sort(key=lambda pc: -len(pc[0]))
        principal, soltos = ps[0], ps[1:]
        ficaram = []

        def distancia(pc) -> float:
            return min(math.hypot(pads[a][2] - pads[b][2], pads[a][3] - pads[b][3])
                       for a in pc[0] for b in principal[0])

        while soltos:
            soltos.sort(key=distancia)
            peca = soltos.pop(0)
            p = ligar(rede, peca, principal[1], macio, desejo_out, bloq, orcamento)
            if p is None:
                fs.append(f"{rede}: nao roteou")
                ficaram.append(peca)
                continue
            principal[0] |= peca[0]
            principal[1] |= peca[1] | set(p)
            if macio is None:
                ok += 1
        if macio is None:
            pedacos[rede] = [principal] + ficaram
        return ok, fs, bloq

    def guiar(rede: str, a: str, b: str, pontos: list) -> bool:
        """One connection finished by hand (GUIAS): from pad `a` through the
        points to pad `b`, every stretch searched, all drawn or none."""
        qa, qb = PAD_POR_NOME.get(a), PAD_POR_NOME.get(b)
        if rede not in por_rede or qa is None or qb is None:
            return False
        d = dados_de(rede)
        ps = pedacos_de(rede)
        ia = next((i for i, q in enumerate(d["pads"]) if q is qa), None)
        ib = next((i for i, q in enumerate(d["pads"]) if q is qb), None)
        if ia is None or ib is None:
            return False
        pa = next(pc for pc in ps if ia in pc[0])
        pb = next(pc for pc in ps if ib in pc[0])
        if pa is pb:
            return True
        camadas = (0, I_BCU) if so_por_fora(rede) else None
        partida = pa[1]
        trechos = []
        for pt in list(pontos) + [None]:
            if pt is None:
                alvo = pb[1]
            else:
                ix, iy = g.cel(pt[0], pt[1])
                cs = (CAMADAS.index(pt[2]),) if len(pt) > 2 else (camadas or tuple(range(NC)))
                alvo = {(c, ix, iy) for c in cs}
            p = a_estrela(g, rede, next(iter(partida)), alvo, 350,
                          campos=d["campos_cel"], larg_estreita=d["larg_pad"],
                          camadas=camadas, inicios=partida)
            if p is None:
                print(f"    guia de {rede} parou antes de {pt}", flush=True)
                return False
            trechos.append(p)
            partida = {p[-1]}
        for p in trechos:
            emitir(p, rede, d["larg"], d["larg_pad"], d["campos_mm"])
        pa[0] |= pb[0]
        pa[1] |= pb[1] | {k for p in trechos for k in p}
        ps.remove(pb)
        return True

    def rotear_rede(rede: str, macio: dict | None = None,
                    desejo_out: dict | None = None) -> tuple[int, list, set]:
        if rede in NAO_ROTEAR or rede in PAR:
            return rotear(rede, macio, desejo_out)
        return completar(rede, macio, desejo_out)

    def refazer_grade() -> None:
        """The grid again from the board and the copper in the lists, as
        the pass drew it: the same marks, in the same object."""
        novo = base(arv, todos)
        for p0_, p1_, c_, r_, w_ in segmentos:
            novo.trilha(c_, p0_, p1_, w_, r_, folga_de(r_))
        for x_, y_, r_ in vias:
            novo.via(x_, y_, r_, folga_de(r_))
        if fechou[0]:
            fechar_sob_gnss(novo)
        g.t, g.v, g.fixo, g.postas = novo.t, novo.v, novo.fixo, novo.postas

    # The RF lines go FIRST, not last: their width and their path are the
    # only ones that are not negotiable, and they have to leave the
    # receiver's pin before anything else takes the room. The area under the
    # receiver is closed to foreign signals once they, the switching nodes
    # and the ground stubs are drawn.
    ok_rede: dict[str, int] = {}
    falhas_rede: dict[str, list] = {}
    rf = [r for r in NAO_ROTEAR if r in por_rede]
    for rede in rf:
        ok_rede[rede], falhas_rede[rede], _b = rotear(rede)
    # then the switching nodes and the USB pair (PRIMEIRO), whole, and only
    # after them the ground stubs: a switching node is a millimetre or two
    # from the pin to its inductor, and once a via could sit 0.2 mm from a
    # pad, the ground via of the pin next door took that millimetre and
    # BUCK2_SW failed (2026-10-19)
    for rede in [r for r in ordem if r in PRIMEIRO]:
        ok_rede[rede], falhas_rede[rede], _b = rotear_rede(rede)
    sem_lugar: list[tuple] = []
    n_gnd = terra(g, por_rede, segmentos, vias, falhas, sem_lugar)
    outras_gnd = [(q[1], q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1], q[4], q[5])
                  for rede, qs in por_rede.items() if rede != "GND" for q in qs]
    # The ground pads with no room for a straight stub get a bent one, found
    # by the same search as any track, to the nearest spot where a via fits.
    # Until 2026-10-18 they were simply reported: twelve of the 64 items the
    # DRC found unconnected were ground pads in the crowded power corner.
    for idx, bx, by, hw, hh in sem_lugar:
        larg_g = max(LARGURA, min(LARGURA_ALIM, 2 * hw, 2 * hh))
        c0, c1 = g.cel(bx, by)
        lim = int(round(RAIO_TERRA / PASSO))
        g.ko_extra = "GND" if no_keepout_da_antena(bx, by) else None
        p = a_estrela(g, "GND", (idx, c0, c1), set(), lim, so_camada=idx,
                      campos=[(c0 - lim, c1 - lim, c0 + lim, c1 + lim)],
                      larg_estreita=larg_g, fim_via=True)
        g.ko_extra = None
        if p is not None:
            pts = [g.pos(c[1], c[2]) for c in p]
            if not all(longe_das_ilhas(outras_gnd, idx, pts[k], pts[k + 1], larg_g,
                                       k == len(pts) - 2)
                       for k in range(len(pts) - 1)):
                p = None
        if p is None:
            falhas.append(f"GND: sem lugar para a via ao lado de ({bx:.1f}; {by:.1f})")
            continue
        emitir(p, "GND", larg_g)
        vx, vy = g.pos(p[-1][1], p[-1][2])
        vias.append((vx, vy, "GND"))
        g.via(vx, vy, "GND")
        n_gnd += 1
    falhas_terra = list(falhas)
    fechou[0] = True
    fechar_sob_gnss(g)
    for rede in [r for r in ordem if r not in PRIMEIRO]:
        ok_rede[rede], falhas_rede[rede], _b = rotear_rede(rede)
    for rede in resto:
        ok_rede[rede], falhas_rede[rede] = 0, []
    # then the connections finished by hand (GUIAS)
    guiadas: set = set()
    for rede, a, b, pontos in GUIAS:
        if rede in resto:
            guiadas.add(rede)
            if guiar(rede, a, b, pontos):
                ok_rede[rede] += 1
    # then the signals that cross the power band, whole, by the corridor
    travessia = sorted((r for r in resto if r in atravessam), key=alcance)
    for rede in travessia:
        ok, falhas_rede[rede], _b = completar(rede)
        ok_rede[rede] += ok
        for pc in pedacos.get(rede, []):
            for _c, cx_, cy_ in pc[1]:
                for dx in (-3, -2, -1, 0, 1, 2, 3):
                    for dy in (-3, -2, -1, 0, 1, 2, 3):
                        barramento.add((cx_ + dx, cy_ + dy))
    # then the edges of every other net in one list, shortest first; with the
    # rails' edges ahead of the signals' in the "alimentacao" order
    arestas = []
    for rede in resto:
        if rede in atravessam:
            continue
        for dist, i, j in arvore(rede):
            arestas.append((dist, rede, i, j))
    if prioridade == "alimentacao":
        arestas.sort(key=lambda a: (not e_alimentacao(a[1]), a[0]))
    else:
        arestas.sort(key=lambda a: a[0])
    for _dist, rede, i, j in arestas:
        if ligar_aresta(rede, i, j):
            ok_rede[rede] += 1
    # and whatever is still apart, net by net in the order of the pass
    for rede in resto:
        ok, fs, _b = completar(rede)
        ok_rede[rede] += ok
        falhas_rede[rede] = fs
    ordem_total = rf + ordem + resto

    def afrouxar_e_refazer() -> None:
        """Every net still loose, with its rule eased (ACABAMENTO), one
        net at a time and the big budget; its copper so far stays."""
        for rede_a in [r for r in ordem_total if falhas_rede.get(r)
                       and r not in NAO_ROTEAR and r not in PAR]:
            if not afrouxar_regra(rede_a):
                continue
            dados.pop(rede_a, None)
            ok_a, fs_a, _b = completar(rede_a, orcamento=ORCAMENTO_FINAL)
            ok_rede[rede_a] += ok_a
            falhas_rede[rede_a] = fs_a
            print(f"  {rede_a} com {ACABAMENTO[rede_a][0]} mm: "
                  f"{len(fs_a)} ligacoes ainda soltas", flush=True)

    afrouxar_e_refazer()

    # Rip-up and reroute, since 2026-10-19: one pass in order leaves the nets
    # that came late with no way through, and no order fixes that for all of
    # them (the failed-first passes made it worse). Ground, the USB pair and
    # the RF lines are never torn up: their copper is placed by rule.
    if rasgar:
        fixas = {"GND"} | set(NAO_ROTEAR) | set(PAR) | guiadas
        t_rrr = time.time()
        n_tent = n_aceito = n_lateral = 0
        tabu: set = set()
        antes_total = sum(len(v) for v in falhas_rede.values())
        for _rodada in range(RRR_RODADAS):
            houve = False
            macio = pintar_macio(segmentos, vias, fixas, fixos_ids)
            for rede in [r for r in ordem_total if falhas_rede.get(r) and r not in fixas]:
                if time.time() - t_rrr > RRR_SEGUNDOS:
                    break
                if not falhas_rede.get(rede):
                    continue
                _o, _f, bloq = rotear_rede(rede, macio=macio)
                bloq -= fixas
                bloq.discard(rede)
                if not bloq or len(bloq) > RRR_MAX_BLOQ:
                    continue
                afetadas = [rede] + sorted(bloq, key=alcance, reverse=True)
                antes_n = sum(len(falhas_rede.get(r, [])) for r in afetadas)
                # what to go back to: the copper, the pieces and the grid as
                # they are, so a refused try costs a copy and not a redraw
                guarda = (list(segmentos), list(vias))
                guarda_p = {r: [[set(a), set(b)] for a, b in pedacos[r]]
                            for r in afetadas if r in pedacos}
                grade = (g.t.copy(), g.v.copy(), set(g.fixo), set(g.postas))
                segmentos[:] = [s for s in segmentos
                                if s[3] not in afetadas or id(s) in fixos_ids[0]]
                vias[:] = [v for v in vias if v[2] not in afetadas or id(v) in fixos_ids[1]]
                for r in afetadas:
                    pedacos.pop(r, None)
                refazer_grade()
                novo_ok, novo_f = {}, {}
                for r in afetadas:
                    novo_ok[r], novo_f[r], _b = rotear_rede(r)
                n_tent += 1
                depois_n = sum(len(v) for v in novo_f.values())
                # a sideways change is taken once for the same nets, or two
                # of them undo each other forever
                chave = (rede, frozenset(afetadas))
                lateral = (depois_n == antes_n and n_lateral < RRR_LATERAIS
                           and chave not in tabu and not novo_f.get(rede))
                if depois_n < antes_n or lateral:
                    if lateral:
                        n_lateral += 1
                        tabu.add(chave)
                    ok_rede.update(novo_ok)
                    falhas_rede.update(novo_f)
                    n_aceito += 1
                    houve = True
                    # the copper changed: who holds which cell, again
                    macio = pintar_macio(segmentos, vias, fixas, fixos_ids)
                else:
                    segmentos[:], vias[:] = guarda
                    g.t, g.v, g.fixo, g.postas = grade
                    for r in afetadas:
                        pedacos.pop(r, None)
                    pedacos.update(guarda_p)
            if not houve or time.time() - t_rrr > RRR_SEGUNDOS:
                break
        # the last try, with the whole board and the big budget
        for rede in [r for r in ordem_total if falhas_rede.get(r) and r not in fixas
                     and r not in NAO_ROTEAR and r not in PAR]:
            ok, fs, _b = completar(rede, orcamento=ORCAMENTO_FINAL)
            ok_rede[rede] += ok
            falhas_rede[rede] = fs
        depois_total = sum(len(v) for v in falhas_rede.values())
        print(f"  rasgar e refazer: {n_tent} tentativas, {n_aceito} aceitas "
              f"({n_lateral} de lado), {antes_total} -> {depois_total} falhas, "
              f"{time.time() - t_rrr:.0f} s", flush=True)

    afrouxar_e_refazer()
    # The geometry has the last word. A via the grid let through too close to
    # a track of another net (vias_perto) sends the via's net back to the
    # search, whole, on a grid that now holds everyone: a connection the DRC
    # calls unconnected is honest, a clearance error is not (2026-10-19)
    intocaveis = {"GND"} | set(NAO_ROTEAR) | set(PAR) | guiadas
    for _volta in range(4):
        maus = {rv for rv, _r, _d, _p in vias_perto(segmentos, vias)} - intocaveis
        if not maus:
            break
        for rede in sorted(maus):
            segmentos[:] = [s_ for s_ in segmentos
                            if s_[3] != rede or id(s_) in fixos_ids[0]]
            vias[:] = [v_ for v_ in vias if v_[2] != rede or id(v_) in fixos_ids[1]]
            pedacos.pop(rede, None)
        refazer_grade()
        for rede in sorted(maus):
            ok_rede[rede], falhas_rede[rede], _b = rotear_rede(rede)
            print(f"  {rede} refeita: a via ficava perto demais de outra rede", flush=True)
    falhas[:] = falhas_terra + [f for r in ordem_total for f in falhas_rede.get(r, [])]
    falharam[:] = [r for r in ordem_total for _f in falhas_rede.get(r, [])]
    n_ok = sum(ok_rede.values())

    if desejos is not None:
        # what each net that failed would have needed, on the copper as the
        # pass left it and before the stitching vias take what is free
        fixas_h = {"GND"} | set(NAO_ROTEAR) | set(PAR) | guiadas
        macio_h = pintar_macio(segmentos, vias, fixas_h, fixos_ids)
        for rede in [r for r in ordem_total if falhas_rede.get(r) and r not in fixas_h]:
            rotear_rede(rede, macio=macio_h, desejo_out=desejos)

    n_cost = costurar(g, vias)
    n_cost += costurar_rf(g, vias, segmentos)
    n_malha = costurar_area(g, vias)
    podar_soltas(segmentos, vias, todos)
    return segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost, n_malha


def pintar_macio(segmentos, vias, fixas: set,
                 fixos_ids: tuple = (frozenset(), frozenset())) -> dict:
    """cell -> the nets whose copper claims it, among those that may be torn
    up, painted with the radii Grade.trilha and Grade.via use. A cell that
    copper of a net in `fixas`, or copper in `fixos_ids` (the ids of the
    segments and vias leque() drew), also claims is left out: it stays
    hard."""
    dono: dict = {}
    duro: set = set()

    def disco(c, x, y, offs, rede, fixo=False):
        cx, cy = int(round(x / PASSO)), int(round(y / PASSO))
        for dx, dy in offs:
            k = (c, cx + dx, cy + dy)
            if fixo or rede in fixas:
                duro.add(k)
            else:
                dono.setdefault(k, set()).add(rede)

    cache: dict[float, list] = {}

    def offs_de(raio: float) -> list:
        if raio not in cache:
            n = int(math.ceil(raio / PASSO))
            cache[raio] = [(dx, dy) for dx in range(-n, n + 1) for dy in range(-n, n + 1)
                           if math.hypot(dx * PASSO, dy * PASSO) <= raio + 1e-9]
        return cache[raio]

    for s in segmentos:
        p0, p1, c, rede, w = s
        offs = offs_de(round(w / 2 + folga_de(rede) + LARGURA / 2, 4))
        n = max(1, int(math.ceil(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / (PASSO / 2))))
        for i in range(n + 1):
            f = i / n
            disco(c, p0[0] + (p1[0] - p0[0]) * f, p0[1] + (p1[1] - p0[1]) * f, offs, rede,
                  id(s) in fixos_ids[0])
    for v in vias:
        x, y, rede = v
        offs = offs_de(round(VIA_D / 2 + folga_de(rede) + LARGURA / 2, 4))
        for c in range(NC):
            disco(c, x, y, offs, rede, id(v) in fixos_ids[1])
    for k in duro:
        dono.pop(k, None)
    return {k: frozenset(v) for k, v in dono.items()}


def no_caminho(g: Grade, rede: str, caminho, macio: dict) -> set:
    """The nets a soft path runs over: the owners of every cell around it
    that the strict search would have refused."""
    off = _disco_off(extra_de(rede))
    redes: set = set()
    for c, ix, iy in caminho:
        for dx, dy in off:
            kk = (c, ix + dx, iy + dy)
            d = g.t.get(kk)
            if d is not None and d != rede and kk in macio:
                redes |= macio[kk]
    return redes


def celulas_ocupadas(g: Grade, rede: str, caminho, macio: dict) -> set:
    """The cells round a soft path that the copper of others holds, grown by
    the distance at which another net's centre line still claims them: where
    the history of the next pass charges (HISTORICO)."""
    off = _disco_off(extra_de(rede))
    presas = set()
    for c, ix, iy in caminho:
        for dx, dy in off:
            kk = (c, ix + dx, iy + dy)
            d = g.t.get(kk)
            if d is not None and d != rede and kk in macio:
                presas.add(kk)
    raio = int(math.ceil((LARGURA + FOLGA) / PASSO))
    saida = set()
    for c, ix, iy in presas:
        for dx in range(-raio, raio + 1):
            for dy in range(-raio, raio + 1):
                saida.add((c, ix + dx, iy + dy))
    return saida


def _dist_ponto_seg(p, q0, q1) -> float:
    vx, vy = q1[0] - q0[0], q1[1] - q0[1]
    L = vx * vx + vy * vy
    t = 0.0 if L == 0 else max(0.0, min(1.0, ((p[0] - q0[0]) * vx + (p[1] - q0[1]) * vy) / L))
    return math.hypot(p[0] - (q0[0] + t * vx), p[1] - (q0[1] + t * vy))


def vias_perto(segmentos, vias) -> list[tuple[str, str, float, tuple]]:
    """(net of the via, net of the track, distance, via) for every via that
    sits closer to a track of another net than the clearance of the two."""
    saida = []
    for (x, y, rv) in vias:
        for (p0, p1, c, r, w) in segmentos:
            if r == rv:
                continue
            exigido = VIA_D / 2 + w / 2 + max(folga_de(r), folga_de(rv)) - 0.01
            if abs(p0[0] - x) > exigido + abs(p1[0] - p0[0]) + 0.01 or                     abs(p0[1] - y) > exigido + abs(p1[1] - p0[1]) + 0.01:
                continue
            d = _dist_ponto_seg((x, y), p0, p1) - VIA_D / 2 - w / 2
            if d < max(folga_de(r), folga_de(rv)) - 0.01:
                saida.append((rv, r, d, (x, y)))
    return saida


def conferir(segmentos, vias) -> list[str]:
    """Does what came out actually keep its distance? Ask the geometry.

    The grid is a model of the board and a model can be wrong. This checks
    the RESULT: every pair of segments on the same layer, every pair of vias,
    every segment against every via, by distance between the real shapes. It
    is what turns "the router thinks it is fine" into a number, and it runs
    before the board is written, so a defect in the grid shows up here and
    not two minutes later in the DRC.
    """
    def dist_seg(a0, a1, b0, b1) -> float:
        def pp(p, q0, q1):
            vx, vy = q1[0] - q0[0], q1[1] - q0[1]
            L = vx * vx + vy * vy
            t = 0.0 if L == 0 else max(0.0, min(1.0, ((p[0] - q0[0]) * vx +
                                                      (p[1] - q0[1]) * vy) / L))
            return math.hypot(p[0] - (q0[0] + t * vx), p[1] - (q0[1] + t * vy))
        d1 = (a1[0] - a0[0], a1[1] - a0[1])
        d2 = (b1[0] - b0[0], b1[1] - b0[1])
        den = d1[0] * d2[1] - d1[1] * d2[0]
        if abs(den) > 1e-12:
            t = ((b0[0] - a0[0]) * d2[1] - (b0[1] - a0[1]) * d2[0]) / den
            u = ((b0[0] - a0[0]) * d1[1] - (b0[1] - a0[1]) * d1[0]) / den
            if -1e-9 <= t <= 1 + 1e-9 and -1e-9 <= u <= 1 + 1e-9:
                return 0.0
        return min(pp(a0, b0, b1), pp(a1, b0, b1), pp(b0, a0, a1), pp(b1, a0, a1))

    problemas: list[str] = []
    for i, (p0, p1, c, r, w) in enumerate(segmentos):
        for (q0, q1, c2, r2, w2) in segmentos[i + 1:]:
            if c != c2 or r == r2:
                continue
            exigido = w / 2 + w2 / 2 + max(folga_de(r), folga_de(r2)) - 0.01
            d = dist_seg(p0, p1, q0, q1)
            if d < exigido:
                problemas.append(
                    f"{r} e {r2} na camada {CAMADAS[c]} a {d:.3f} mm, "
                    f"pedem {exigido:.3f} (em {p0[0]:.1f}; {p0[1]:.1f})")
    for i, (x, y, r) in enumerate(vias):
        for (x2, y2, r2) in vias[i + 1:]:
            d = math.hypot(x - x2, y - y2)
            if d < VIA_D + 0.12:
                problemas.append(f"via {r} e via {r2} a {d:.3f} mm "
                                 f"(em {x:.1f}; {y:.1f})")
    # and every via against every track of another net, which the docstring
    # promised and the code never did: a via of NTC_SOLAR 0.124 mm from a
    # VBAT_SYS track went through here unseen, and the DRC found it
    for rv, r, d, (x, y) in vias_perto(segmentos, vias):
        problemas.append(f"via {rv} a {d:.3f} mm da trilha {r} (em {x:.1f}; {y:.1f})")
    return problemas


def escrever(caminho, texto, numeros, segmentos, vias) -> None:
    linhas = []
    for (p0, p1, c, rede, larg) in segmentos:
        a = MP.P_(*p0)
        b = MP.P_(*p1)
        if abs(a[0] - b[0]) < 1e-9 and abs(a[1] - b[1]) < 1e-9:
            continue
        linhas.append(
            f'\t(segment\n\t\t(start {a[0]:.4f} {a[1]:.4f})\n'
            f'\t\t(end {b[0]:.4f} {b[1]:.4f})\n\t\t(width {larg})\n'
            f'\t\t(layer "{CAMADAS[c]}")\n\t\t(net {numeros.get(rede, 0)})\n'
            f'\t\t(uuid "{MP.uid("seg", a, b, c)}")\n\t)')
    for (x, y, rede) in vias:
        q = MP.P_(x, y)
        linhas.append(
            f'\t(via\n\t\t(at {q[0]:.4f} {q[1]:.4f})\n\t\t(size {VIA_D})\n'
            f'\t\t(drill {VIA_FURO})\n\t\t(layers "F.Cu" "B.Cu")\n'
            f'\t\t(net {numeros.get(rede, 0)})\n'
            f'\t\t(uuid "{MP.uid("via", q)}")\n\t)')
    texto = sem_cobre(texto)
    fim = texto.rstrip()
    assert fim.endswith(")")
    caminho.write_text(fim[:-1].rstrip() + "\n" + "\n".join(linhas) + "\n)\n",
                       encoding="utf-8", newline="\n")


def sem_cobre(texto: str) -> str:
    """Take out the tracks and vias a previous run left behind.

    Without this, routing appends: run it twice and the board carries both
    sets of tracks, every net on top of itself and across its neighbours.
    That is how a board the router believed had 534 segments reached the file
    with 1576, and why the DRC then reported 206 crossings that the router
    had never drawn. Routing has to be something you can run again.
    """
    saida = []
    i = 0
    n = len(texto)
    while i < n:
        if texto.startswith("(segment", i) or texto.startswith("(via", i):
            d, j = 0, i
            while j < n:
                if texto[j] == '"':
                    j += 1
                    while j < n and texto[j] != '"':
                        j += 2 if texto[j] == "\\" else 1
                elif texto[j] == "(":
                    d += 1
                elif texto[j] == ")":
                    d -= 1
                    if d == 0:
                        break
                j += 1
            # and the indentation of the line it started on
            while saida and saida[-1] in " \t":
                saida.pop()
            i = j + 1
            while i < n and texto[i] in " \t\r\n":
                i += 1
            saida.append("\n")
            continue
        saida.append(texto[i])
        i += 1
    return "".join(saida)


def main() -> int:
    caminho = HERE / "gnssbike.kicad_pcb"
    texto = caminho.read_text(encoding="utf-8")
    arv = fp_load.parse(texto)
    # the numbers the FILE uses: nets.py may have moved on since make_pcb
    numeros = fp_load.redes_da_placa(arv)
    todos, por_rede, caixa_fp = pads_da_placa(arv)

    # Several passes, and the best is kept. The first routes the short nets
    # first; the second the power rails first, longest first, and then the
    # signals short first; any pass after those (PASSAGENS_EXTRA) puts
    # whatever failed in the one before at the front. Until 2026-10-18 there
    # were two passes, long nets first and short nets first, and the comment
    # promised the failed-first passes without the code doing them; the
    # long-first order lost every time it was measured (113 and 124 failures
    # against 83), and it is gone.
    melhor = None
    melhor_desejos: dict = {}
    antes: list[str] = []
    sem_ganho = 0
    ordens = ["curtas", "alimentacao"] + ["curtas"] * PASSAGENS_EXTRA
    for i_pass, ordem_pass in enumerate(ordens):
        desejos: dict = {}
        r = uma_passagem(arv, numeros, todos, por_rede, caixa_fp, ordem_pass, antes,
                         desejos=desejos if HISTORICO_PASSAGENS else None)
        r = r + (ordem_pass,)
        segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost, n_malha = r[:8]
        print(f"  passagem {i_pass + 1} ({ordem_pass}), {len(antes)} redes na frente: "
              f"{n_ok} ligacoes, {len(falhas)} falhas", flush=True)
        if melhor is None or len(falhas) < len(melhor[2]):
            melhor = r
            melhor_desejos = desejos
            sem_ganho = 0
        else:
            sem_ganho += 1
        if not falhas or sem_ganho >= 2:
            break
        if i_pass >= 1:
            # after the two orders, the failed-first passes (PASSAGENS_EXTRA)
            novas = list(dict.fromkeys(falharam))
            antes = novas + [r_ for r_ in antes if r_ not in novas]
    melhor_ordem = melhor[-1]
    # the history passes (HISTORICO), from the cells the best pass fought over
    historico: dict = {}
    isentos: dict = {}
    hist_do_melhor: tuple[dict, dict] = ({}, {})
    desejos = melhor_desejos
    for i_h in range(HISTORICO_PASSAGENS):
        if not melhor[2]:
            break
        for rede, cels in desejos.items():
            for k in cels:
                historico[k] = historico.get(k, 0.0) + HISTORICO_PESO
            isentos[rede] = isentos.get(rede, frozenset()) | frozenset(cels)
        desejos = {}
        r = uma_passagem(arv, numeros, todos, por_rede, caixa_fp, melhor_ordem, (),
                         historico=historico, isentos=isentos, desejos=desejos)
        print(f"  historico {i_h + 1} ({melhor_ordem}): {r[5]} ligacoes, "
              f"{len(r[2])} falhas, {len(historico)} celulas mais caras", flush=True)
        if len(r[2]) < len(melhor[2]):
            melhor = r + (melhor_ordem,)
            hist_do_melhor = (dict(historico), dict(isentos))
    # the best order once more, now with the rip-up and reroute at its end
    r = uma_passagem(arv, numeros, todos, por_rede, caixa_fp, melhor_ordem, (), rasgar=True,
                     historico=hist_do_melhor[0], isentos=hist_do_melhor[1])
    print(f"  {melhor_ordem} com rasgar e refazer: {r[5]} ligacoes, {len(r[2])} falhas",
          flush=True)
    if len(r[2]) <= len(melhor[2]):
        melhor = r + (melhor_ordem,)
    segmentos, vias, falhas, _f, n_gnd, n_ok, n_cost, n_malha = melhor[:8]

    ruins = conferir(segmentos, vias)
    print(f"  conferencia geometrica: {len(ruins)} pares perto demais")
    for r in ruins[:8]:
        print(f"    {r}")

    escrever(caminho, texto, numeros, segmentos, vias)

    print(f"{len(segmentos)} segmentos, {len(vias)} vias")
    print(f"  {n_gnd} pads de terra com via ao plano, "
          f"{n_cost} vias de costura na borda, {n_malha} na malha da area")
    print(f"  {n_ok} ligacoes roteadas, {len(NAO_ROTEAR)} redes deixadas de fora "
          f"({', '.join(sorted(NAO_ROTEAR))})")
    if falhas:
        # Grouped by net, not the first twelve lines. Printed flat, the list
        # was 134 ground via failures with the signal nets buried behind
        # them - USB_DP and USB_DM came out with zero segments and nothing in
        # the report said so.
        import collections as _c
        por_rede_falha = _c.Counter(f.split(":", 1)[0] for f in falhas)
        print(f"  NAO ROTEADO: {len(falhas)} em {len(por_rede_falha)} redes")
        for rede, n in por_rede_falha.most_common():
            exemplo = next(f for f in falhas if f.startswith(rede + ":"))
            print(f"    {rede}: {n}x  ({exemplo.split(': ', 1)[1]})")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
