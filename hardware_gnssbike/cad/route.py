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
  power    uses the wider track of the Alimentacao net class.
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

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import footprints as FPS  # noqa: E402,F401
import fp_load  # noqa: E402
import make_dxf as M  # noqa: E402
import make_pcb as MP  # noqa: E402

PASSO = 0.15                # routing grid, mm. 0.3 does not fit between
                            # the balls of the module's LGA: the free band
                            # between two columns is 0.45 mm wide
LARGURA = 0.15              # default track width
LARGURA_ALIM = 0.4          # power track width
LARGURA_USB = 0.2
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
FOLGA_USB = 0.21            # 0.2 of the USB class, plus the grid
# A via next to a pad has to leave more than the electrical clearance: the
# two solder mask openings grow about 0.05 mm each and the web of mask left
# between them has to be wide enough to survive, or the DRC reports a mask
# bridge - 41 of them on the last board. 0.35 mm of copper leaves about
# 0.25 mm of mask, which is the usual minimum.
FOLGA_MASCARA = 0.35
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
CUSTO_CURVA = 0.7             # virar 90 graus
CUSTO_CURVA_45 = 0.2          # virar 45: meia virada, meio custo
# A maze search that cannot get through explores everything it is allowed to
# before saying so, and on a 367 x 647 grid over two layers that is nearly a
# million cells for one net that was never going to route. The budget turns a
# hopeless search into two seconds instead of half a minute; what it costs is
# that a genuinely tortuous path may be given up on, and that shows in the
# report as a net left unrouted rather than as a wrong board.
ORCAMENTO = 90000

BLOQUEADO = "\x00"          # a net name no net can have: blocked for everyone

# The RF nets. They are routed LAST and at the calculated 50 ohm width, on
# the front layer only, with the ground pour beside them and ground vias
# along both sides: a via in the middle of an RF run is a stub, and a stub is
# what section 7.2 of the module datasheet forbids by name.
# RF_UFL and RF_CHIP are the two branches after the choice jumper: both are
# 50 ohm line and both are routed at the RF width on the front layer, even
# though only one is ever fitted. The unfitted one ends at an open pad.
NAO_ROTEAR = {"RF_IN", "RF_ANT", "RF_UFL", "RF_CHIP"}
SO_FRENTE = NAO_ROTEAR
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


def largura(rede: str) -> float:
    if rede in NAO_ROTEAR:
        return LARGURA_RF
    if rede.startswith("USB_D"):
        # LARGURA_USB_CALC is the geometry that gives 90 ohm differential on
        # this stack-up: 0.352 mm at a 0.2 mm gap. It does not fit the
        # connector - a USB-C receptacle has 0.5 mm pitch pads, leaving 0.2 mm
        # between two of them, and a 0.352 mm track with the USB class's
        # 0.2 mm clearance cannot leave the pad field at all.
        #
        # A neck gets it out of the pad field - emitir() cuts one and the
        # search knows about it - but not across the board: at 0.352 mm with
        # the USB class's 0.2 mm of clearance there is no channel from the
        # receptacle in one bottom corner to the module in the other, and
        # both halves of the pair come out unrouted. Worse, the two pads of
        # each signal on a Type-C are the SAME signal on opposite rows, so
        # joining them means crossing the connector's own pad field.
        #
        # So: the pair is routed at the width that fits, and the gap is
        # REPORTED rather than hidden - US1 in the dry-run prints the routed
        # width against the 0.352 mm that 90 ohm needs on this stack-up. It
        # is a pair that has to be finished by hand, and saying so is the
        # honest version of a pair that silently is not 90 ohm.
        return LARGURA_USB
    if e_alimentacao(rede):
        return LARGURA_ALIM
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
             respeitar_fixo: bool = True) -> None:
        """Reserve the cells of an axis-aligned rectangle, inflated already."""
        # floor and ceil, not round: cel() rounds to the nearest cell and
        # can land up to half a step INSIDE the rectangle, leaving the outer
        # 0.075 mm of the clearance unmarked
        ix0 = int(math.floor((x - hw) / PASSO))
        iy0 = int(math.floor((y - hh) / PASSO))
        ix1 = int(math.ceil((x + hw) / PASSO))
        iy1 = int(math.ceil((y + hh) / PASSO))
        for ix in range(ix0, ix1 + 1):
            for iy in range(iy0, iy1 + 1):
                self._por(mapa, (camada, ix, iy), rede, respeitar_fixo)

    def _disco(self, mapa: dict, camada: int, x: float, y: float,
               raio: float, rede: str, respeitar_fixo: bool = True) -> None:
        n = int(math.ceil(raio / PASSO))
        cx, cy = self.cel(x, y)
        for dx in range(-n, n + 1):
            for dy in range(-n, n + 1):
                if math.hypot(dx * PASSO, dy * PASSO) > raio + 1e-9:
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
            rede: str) -> None:
        """A pad, as the rectangle it is, into both maps.

        A track has to keep FOLGA from the copper, so its centre line has to
        keep FOLGA + half a track. A via has to keep FOLGA from the copper
        with its 0.45 mm pad, AND 0.2 mm from the copper with its 0.25 mm
        hole; the first is the larger, so it decides.
        """
        for c in camadas:
            self._ret(self.t, c, x, y, hw + FOLGA + LARGURA / 2,
                      hh + FOLGA + LARGURA / 2, rede)
        # a via goes through: a pad on any layer blocks it on all of them
        # respeitar_fixo=False on purpose. In the track map that exemption
        # is what lets a net reach its own pad; in the VIA map there is no
        # such thing to protect, and keeping it there let a ground stub drop
        # a via 0.075 mm from a neighbouring pad - eleven of the board's
        # nineteen clearance errors.
        for c in range(NC):
            self._ret(self.v, c, x, y, hw + FOLGA_MASCARA + VIA_D / 2,
                      hh + FOLGA_MASCARA + VIA_D / 2, rede,
                      respeitar_fixo=False)

    def trilha(self, camada: int, p0, p1, larg: float, rede: str) -> None:
        """Reserve a run of track on both maps, along its whole length."""
        n = max(1, int(math.ceil(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / (PASSO / 2))))
        rt = larg / 2 + FOLGA + LARGURA / 2
        rv = larg / 2 + FOLGA + VIA_D / 2
        for i in range(n + 1):
            f = i / n
            x = p0[0] + (p1[0] - p0[0]) * f
            y = p0[1] + (p1[1] - p0[1]) * f
            self._disco(self.t, camada, x, y, rt, rede)
            for c in range(NC):
                self._disco(self.v, c, x, y, rv, rede, respeitar_fixo=False)

    def via(self, x: float, y: float, rede: str) -> None:
        """Reserve a via: its pad on both layers, and room for the next one.

        Two vias have to keep their holes 0.2 mm apart and their pads 0.2 mm
        apart; the pads decide, so the next via centre stays VIA_D + FOLGA
        away.
        """
        self.postas.add(self.cel(x, y))
        for c in range(NC):
            self._disco(self.t, c, x, y, VIA_D / 2 + FOLGA + LARGURA / 2, rede)
            self._disco(self.v, c, x, y, VIA_D + FOLGA, rede,
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
            if d is not None and d != rede:
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
    for f in fp_load.kids(arv, "footprint"):
        at = fp_load.kid(f, "at")
        fx, fy = float(at[1]), float(at[2])
        ang = math.radians(float(at[3])) if len(at) > 3 else 0.0
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
            passante = any(c.startswith("*") for c in camadas) or p[2] != "smd"
            if passante:
                idx = -1
            elif any("B.Cu" in c for c in camadas):
                idx = I_BCU
            else:
                idx = 0
            rede = fp_load.kid(p, "net")
            nome = rede[2] if rede else ""
            item = (nome, idx, gx, gy, hw, hh)
            todos.append(item)
            meus.append((gx, gy, hw, hh))
            if nome:
                por_rede.setdefault(nome, []).append(item)
        if meus:
            b = (min(q[0] - q[2] for q in meus), min(q[1] - q[3] for q in meus),
                 max(q[0] + q[2] for q in meus), max(q[1] + q[3] for q in meus))
            for gx, gy, _hw, _hh in meus:
                caixa_fp[(round(gx, 4), round(gy, 4))] = b
    return todos, por_rede, caixa_fp


def a_estrela(g: Grade, rede: str, inicio: tuple[int, int, int],
              alvos: set[tuple[int, int, int]], folga: int = 200,
              orcamento: int = ORCAMENTO, so_camada: int | None = None,
              perto_de: frozenset | None = None,
              campos: list | None = None, larg_estreita: float = LARGURA):
    """Shortest path from one cell to any target, changing layer at a cost.

    `campos` are the pad fields of this net, in CELL coordinates. Inside one
    of them the track is allowed to be as narrow as the pad it is leaving,
    and the search has to know that or it never finds the way out: emitir()
    already cut the neck, but it only ever saw paths the search had already
    found. With one width for the whole search the USB pair at its calculated
    0.352 mm could not leave a 0.5 mm pitch receptacle at all, and came out
    with zero segments.
    """
    if inicio in alvos:
        return [inicio]
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
    tx = sum(t[1] for t in alvos) / len(alvos)
    ty = sum(t[2] for t in alvos) / len(alvos)
    bx0 = min([inicio[1]] + [t[1] for t in alvos]) - folga
    bx1 = max([inicio[1]] + [t[1] for t in alvos]) + folga
    by0 = min([inicio[2]] + [t[2] for t in alvos]) - folga
    by1 = max([inicio[2]] + [t[2] for t in alvos]) + folga

    fila = [(abs(inicio[1] - tx) + abs(inicio[2] - ty), 0.0, inicio, None)]
    veio: dict[tuple, tuple | None] = {}
    melhor = {inicio: 0.0}
    while fila:
        _f, custo, atual, ant = heapq.heappop(fila)
        if atual in veio:
            continue
        veio[atual] = ant
        if len(veio) > orcamento:
            return None
        if atual in alvos:
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
            if g.livre_t((c, ix + dx, iy), rede, _off(ix + dx, iy)) and                     g.livre_t((c, ix, iy + dy), rede, _off(ix, iy + dy)):
                vizinhos.append((c, ix + dx, iy + dy))
        if so_camada is None:
            vizinhos += [(k, ix, iy) for k in range(NC) if k != c]
        for v in vizinhos:
            if v in veio:
                continue
            if not (bx0 <= v[1] <= bx1 and by0 <= v[2] <= by1):
                continue
            if not g.dentro(v[1], v[2]) or                     not g.livre_t(v, rede, _off(v[1], v[2])):
                continue
            if v[0] != c:
                # a layer change is a via: every via here goes right through
                # the board, so it needs room for its pad on EVERY layer and
                # for its hole
                if not g.cabe_via(v[1], v[2], rede):
                    continue
                if not g.livre_t((v[0], ix, iy), rede, _off(ix, iy)):
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
    """
    zona_gnss = None
    for nome, r, _c, _s in M.ZONES:
        if nome == "ZONA_GNSS_MAX-F10S":
            zona_gnss = r
    if zona_gnss is None:
        return 0
    ix0, iy0 = g.cel(zona_gnss[0], zona_gnss[1])
    ix1, iy1 = g.cel(zona_gnss[2], zona_gnss[3])
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
        g.pad(range(NC) if idx < 0 else (idx,), x - MP.ORIGEM[0],
              y - MP.ORIGEM[1], hw, hh, nome or BLOQUEADO)

    fx, fy = M.FUROS_DOC[0]
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
            for ix in range(ix0, ix1 + 1):
                for iy in range(iy0, iy1 + 1):
                    g.t[(c, ix, iy)] = BLOQUEADO
        # a via is 0.45 mm wide: its CENTRE has to stay that much further out
        # of a keep-out, or the pad of it reaches inside
        folga_v = int(math.ceil((VIA_D / 2 + FOLGA) / PASSO))
        for c in range(NC):
            for ix in range(ix0 - folga_v, ix1 + folga_v + 1):
                for iy in range(iy0 - folga_v, iy1 + folga_v + 1):
                    g.v[(c, ix, iy)] = BLOQUEADO
    return g


def terra(g: Grade, por_rede, segmentos, vias, falhas) -> int:
    """A stub from every ground pad to a via down into the plane.

    The stub is axis-aligned and the whole of it is checked before anything
    is drawn: a stub that leaves on a free bearing but crosses a neighbour's
    pad on the way is exactly the sort of short a plane is supposed to save
    you from.
    """
    n_gnd = 0
    for _nome, idx, x, y, hw, hh in por_rede.get("GND", []):
        if idx < 0:
            continue                     # a through pad already meets the plane
        bx, by = x - MP.ORIGEM[0], y - MP.ORIGEM[1]
        posto = None
        # the four sides first, then the corners: a via straight out of the
        # pad gives the shortest loop, and the diagonal is what saves the
        # pads in the crowded corner by the power supply
        dirs = ((0, 1), (0, -1), (1, 0), (-1, 0),
                (0.7071, 0.7071), (0.7071, -0.7071),
                (-0.7071, 0.7071), (-0.7071, -0.7071))
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
                posto = (vx, vy)
                break
            if posto:
                break
        if posto is None:
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
    return n_gnd


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


def uma_passagem(arv, numeros, todos, por_rede, caixa_fp, prioridade):
    """One routing attempt with a given order. Returns what came out."""
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
                g.via(x, y, rede)
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
            def no_campo(k: int) -> bool:
                if larg_pad is None or campo is None:
                    return False
                px, py = g.pos(caminho_cel[k][1], caminho_cel[k][2])
                return (campo[0] <= px <= campo[2] and
                        campo[1] <= py <= campo[3])

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
                g.trilha(a[0], p0, p1, w, rede)
                k0 = k1
            i = j

    n_gnd = terra(g, por_rede, segmentos, vias, falhas)

    def alcance(r: str) -> float:
        xs = [q[2] for q in por_rede[r]]
        ys = [q[3] for q in por_rede[r]]
        return (max(xs) - min(xs)) + (max(ys) - min(ys))

    ordem = [r for r in PRIMEIRO if r in por_rede]
    resto = [r for r in por_rede
             if r not in ordem and r not in NAO_ROTEAR and r != "GND"]
    resto.sort(key=alcance, reverse=(prioridade != "curtas"))

    n_ok = 0
    fechou = [False]
    caminhos: dict[str, frozenset] = {}
    # The RF lines go FIRST, not last: their width and their path are the
    # only ones that are not negotiable, and they have to leave the
    # receiver's pin before anything else takes the room. The area under the
    # receiver is closed to foreign signals right after they are drawn.
    rf = [r for r in NAO_ROTEAR if r in por_rede]
    for rede in rf + ordem + resto:
        pads = por_rede[rede]
        if len(pads) < 2:
            continue
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
        feito = set(celulas[0])
        if rede not in NAO_ROTEAR and not fechou[0]:
            fechou[0] = True
            fechar_sob_gnss(g)
        for k, alvo in enumerate(celulas[1:], start=1):
            if alvo & feito:
                continue
            larg = largura(rede)
            larg_pad = max(LARGURA, min(larg, estreitos[k]))
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
            perto = None
            if rede == PAR[1] and PAR[0] in caminhos:
                perto = caminhos[PAR[0]]
            p = None
            for folga in (100, 350):
                p = a_estrela(g, rede, next(iter(alvo)), feito, folga,
                              so_camada=so_camada, perto_de=perto,
                              campos=campos_cel, larg_estreita=larg_pad)
                if p:
                    break
            if p is None:
                falhas.append(f"{rede}: nao roteou")
                falharam.append(rede)
                continue
            emitir(p, rede, larg, larg_pad, campo)
            feito |= set(p)
            n_ok += 1
            if rede == PAR[0]:
                # the cells its partner should hug: the path itself and one
                # step around it, which at a 0.15 mm grid is the pair pitch
                viz = set()
                for _c, cx_, cy_ in p:
                    for dx in (-2, -1, 0, 1, 2):
                        for dy in (-2, -1, 0, 1, 2):
                            viz.add((cx_ + dx, cy_ + dy))
                caminhos[rede] = frozenset(viz)

    n_cost = costurar(g, vias)
    n_cost += costurar_rf(g, vias, segmentos)
    n_malha = costurar_area(g, vias)
    return segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost, n_malha


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
    numeros, _por_pad = MP.redes()
    todos, por_rede, caixa_fp = pads_da_placa(arv)

    # Several passes. Whatever failed goes to the front of the next one,
    # so a run that could not find a way through gets the empty board next
    # time. A pass costs about a minute; the board settles in three or four.
    melhor = None
    for nome in ("compridas", "curtas"):
        r = uma_passagem(arv, numeros, todos, por_rede, caixa_fp, nome)
        segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost, n_malha = r
        print(f"  ordem {nome} primeiro: {n_ok} ligacoes, {len(falhas)} falhas",
              flush=True)
        if melhor is None or len(falhas) < len(melhor[2]):
            melhor = r
        if not falhas:
            break
    segmentos, vias, falhas, _f, n_gnd, n_ok, n_cost, n_malha = melhor

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
