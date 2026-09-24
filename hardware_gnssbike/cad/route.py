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
  RF       is NOT routed here. RF_IN and RF_ANT need a controlled 50 ohm
           width, and that width does not exist until the fabricator gives
           the stackup - which 04-pcb-e-caixa.md already carries as an open
           item. They are left for the person who has that number.

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
CUSTO_CURVA = 0.7
# A maze search that cannot get through explores everything it is allowed to
# before saying so, and on a 367 x 647 grid over two layers that is nearly a
# million cells for one net that was never going to route. The budget turns a
# hopeless search into two seconds instead of half a minute; what it costs is
# that a genuinely tortuous path may be given up on, and that shows in the
# report as a net left unrouted rather than as a wrong board.
ORCAMENTO = 90000

BLOQUEADO = "\x00"          # a net name no net can have: blocked for everyone

# Nets left for a person: the 50 ohm width does not exist yet.
NAO_ROTEAR = {"RF_IN", "RF_ANT"}
# Nets that go first and have to stay short: the switching loops.
PRIMEIRO = ["BUCK1_SW", "BUCK2_SW", "SW_DCDC", "SW_DCDC_L", "USB_DP", "USB_DM"]


ALIMENTACAO = ("GND", "VSYS", "VBAT", "VBAT_SYS", "VBAT_CELULA", "VBUS",
               "VBUSOUT", "3V0", "1V8", "SD3V0", "3V3BL", "VBCKP", "VINT")


def e_alimentacao(rede: str) -> bool:
    return rede in ALIMENTACAO or rede.startswith(("3V0_", "1V8_", "SD3V0_"))


def largura(rede: str) -> float:
    if rede.startswith("USB_D"):
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
        x, y = self.pos(ix, iy)
        if x < BORDA_COBRE or y < BORDA_COBRE or \
                x > M.W - BORDA_COBRE or y > M.H - BORDA_COBRE:
            return False
        # the notch under the module's antenna is not board: copper there is
        # copper hanging in the air, and the DRC calls it what it is
        for nome, (rx0, ry0, rx1, ry1), _c, _s in M.ZONES:
            if nome != "RECORTE_ANTENA_MODULO":
                continue
            if rx0 - BORDA_COBRE < x < rx1 + BORDA_COBRE and \
                    ry0 - BORDA_COBRE < y < ry1 + BORDA_COBRE:
                return False
        r = M.RADIUS_DRAWING
        for cx, cy in ((r, r), (M.W - r, r), (r, M.H - r), (M.W - r, M.H - r)):
            fora_x = x < r if cx < M.W / 2 else x > M.W - r
            fora_y = y < r if cy < M.H / 2 else y > M.H - r
            if fora_x and fora_y and math.hypot(x - cx, y - cy) > r - BORDA_COBRE:
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


def pads_da_placa(arv) -> tuple[list[tuple], dict[str, list[tuple]]]:
    """Every pad: (net, layer index or -1 for through, x, y, half w, half h).

    The half sizes are the rotated ones: a 1.5 x 0.7 pad turned 90 degrees is
    0.7 x 1.5, and marking it the other way round is how a track ends up
    running through a pad it should have gone around.
    """
    todos = []
    por_rede: dict[str, list[tuple]] = {}
    for f in fp_load.kids(arv, "footprint"):
        at = fp_load.kid(f, "at")
        fx, fy = float(at[1]), float(at[2])
        ang = math.radians(float(at[3])) if len(at) > 3 else 0.0
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
            if nome:
                por_rede.setdefault(nome, []).append(item)
    return todos, por_rede


def a_estrela(g: Grade, rede: str, inicio: tuple[int, int, int],
              alvos: set[tuple[int, int, int]], folga: int = 200,
              orcamento: int = ORCAMENTO):
    """Shortest path from one cell to any target, changing layer at a cost."""
    if inicio in alvos:
        return [inicio]
    off = _disco_off(extra_de(rede))
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
        vizinhos = [(c, ix + 1, iy), (c, ix - 1, iy), (c, ix, iy + 1),
                    (c, ix, iy - 1)]
        vizinhos += [(k, ix, iy) for k in range(NC) if k != c]
        for v in vizinhos:
            if v in veio:
                continue
            if not (bx0 <= v[1] <= bx1 and by0 <= v[2] <= by1):
                continue
            if not g.dentro(v[1], v[2]) or not g.livre_t(v, rede, off):
                continue
            if v[0] != c:
                # a layer change is a via: every via here goes right through
                # the board, so it needs room for its pad on EVERY layer and
                # for its hole
                if not g.cabe_via(v[1], v[2], rede):
                    continue
                if not g.livre_t((v[0], ix, iy), rede, off):
                    continue
            passo = CUSTO_VIA if v[0] != c else 1.0
            if ant is not None and v[0] == c and ant[0] == c:
                d1 = (ix - ant[1], iy - ant[2])
                d2 = (v[1] - ix, v[2] - iy)
                if d1 != d2:
                    passo += CUSTO_CURVA
            novo = custo + passo
            if novo < melhor.get(v, float("inf")):
                melhor[v] = novo
                h = abs(v[1] - tx) + abs(v[2] - ty)
                heapq.heappush(fila, (novo + h, novo, v, atual))
    return None


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
    passo = 3.0
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


def uma_passagem(arv, numeros, todos, por_rede, prioridade):
    """One routing attempt with a given order. Returns what came out."""
    g = base(arv, todos)
    segmentos: list[tuple] = []
    vias: list[tuple] = []
    falhas: list[str] = []
    falharam: list[str] = []

    def emitir(caminho_cel, rede, larg, larg_pad=None, ponto=None,
               raio_pad=0.0):
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
            p0 = g.pos(a[1], a[2])
            p1 = g.pos(caminho_cel[j][1], caminho_cel[j][2])
            # The neck lasts while the track is still inside the part's
            # pad field, not just for the first run: a 0.4 mm track two
            # segments out of a 0.4 mm pitch QFN is still between its pads.
            # Necking the whole net instead took VBAT, which carries the
            # 800 mA charging current, to 0.2 mm end to end, because the fuel
            # gauge's WLP bump is 0.2 mm wide.
            w = larg
            if larg_pad is not None and ponto is not None:
                d0 = math.hypot(p0[0] - ponto[0], p0[1] - ponto[1])
                if d0 <= raio_pad:
                    w = larg_pad
            segmentos.append((p0, p1, a[0], rede, w))
            g.trilha(a[0], p0, p1, w, rede)
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
    for rede in ordem + resto:
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
        # how far the neck has to last: the reach of the part the pad belongs
        # to, taken as the spread of this net's pads that share its footprint
        raios = []
        for q in pads:
            perto = [w for w in todos
                     if abs(w[2] - q[2]) < 6 and abs(w[3] - q[3]) < 6]
            raios.append(max([math.hypot(w[2] - q[2], w[3] - q[3])
                              for w in perto] + [0.8]) * 0.5 + 0.8)
        celulas = []
        for _n, idx, x, y, _hw, _hh in pads:
            c0, c1 = g.cel(x - MP.ORIGEM[0], y - MP.ORIGEM[1])
            celulas.append({(c, c0, c1)
                            for c in (range(NC) if idx < 0 else (idx,))})
        feito = set(celulas[0])
        for k, alvo in enumerate(celulas[1:], start=1):
            if alvo & feito:
                continue
            larg = largura(rede)
            larg_pad = max(LARGURA, min(larg, estreitos[k]))
            q = pads[k]
            ponto = (q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1])
            # out to the far corner of the part, so the neck covers the whole
            # pad field of a fine-pitch package
            raio_pad = raios[k]
            p = None
            for folga in (100, 350):
                p = a_estrela(g, rede, next(iter(alvo)), feito, folga)
                if p:
                    break
            if p is None:
                falhas.append(f"{rede}: nao roteou")
                falharam.append(rede)
                continue
            emitir(p, rede, larg, larg_pad, ponto, raio_pad)
            feito |= set(p)
            n_ok += 1

    n_cost = costurar(g, vias)
    return segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost


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
    todos, por_rede = pads_da_placa(arv)

    # Several passes. Whatever failed goes to the front of the next one,
    # so a run that could not find a way through gets the empty board next
    # time. A pass costs about a minute; the board settles in three or four.
    melhor = None
    for nome in ("compridas", "curtas"):
        r = uma_passagem(arv, numeros, todos, por_rede, nome)
        segmentos, vias, falhas, falharam, n_gnd, n_ok, n_cost = r
        print(f"  ordem {nome} primeiro: {n_ok} ligacoes, {len(falhas)} falhas",
              flush=True)
        if melhor is None or len(falhas) < len(melhor[2]):
            melhor = r
        if not falhas:
            break
    segmentos, vias, falhas, _f, n_gnd, n_ok, n_cost = melhor

    ruins = conferir(segmentos, vias)
    print(f"  conferencia geometrica: {len(ruins)} pares perto demais")
    for r in ruins[:8]:
        print(f"    {r}")

    escrever(caminho, texto, numeros, segmentos, vias)

    print(f"{len(segmentos)} segmentos, {len(vias)} vias")
    print(f"  {n_gnd} pads de terra com via ao plano, "
          f"{n_cost} vias de costura na borda")
    print(f"  {n_ok} ligacoes roteadas, {len(NAO_ROTEAR)} redes deixadas de fora "
          f"({', '.join(sorted(NAO_ROTEAR))})")
    if falhas:
        print(f"  NAO ROTEADO: {len(falhas)}")
        for f in falhas[:12]:
            print(f"    {f}")
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
