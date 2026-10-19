"""Finish the routing one connection at a time, on the board route.py left.

route.py routes the whole board in passes and leaves what it could not.
This takes the board as it is, finds every net whose copper is still in
more than one piece, and joins the pieces one connection at a time (the
owner, 2026-10-19: one track at a time, and change the rule until it is
100%). First the copper drawn by hand goes in (TIRAR, MAO and VIAS_MAO,
where the grid's 0,15 mm steps have no room) and the connections with a
guide go through their points (GUIAS). Then each loose connection is tried

1. as any track of its net: the width of its class, the layers it may use,
   the whole board as the search box;
2. with its net's rule eased: the narrowest width IPC-2221 allows its
   current on an outer layer, outer layers only (route.afrouxar_regra),
   and then the inner layer too, at the width IPC-2221 asks there
   (afrouxar_por_dentro);
3. through whoever is in the way: the search runs again over the copper
   that may move (route.pintar_macio), the copper of one blocking net on
   that way comes out (or of all of them together), the connection goes
   in, and the blocking nets are joined again by 1 and 2;
4. along a corridor: every piece of copper near the way the soft search
   found comes out, the connection goes along it, and the rest is joined
   again;
5. with the two or three nets that fight for one channel torn up whole
   and joined again in each order.

A change stays only if nothing is left loose that was not loose before.
At the end, what KiCad's DRC would call dangling is cut back (podar).
Joined and dangling are measured the way KiCad measures them (pecas,
_corte), so the count here is the DRC's. Ground (it reaches the plane
through its vias and the pours), the RF lines and the USB pair are never
moved. Run after route.py, on the board or on a copy of it:

    python acabar.py [placa.kicad_pcb]
"""
from __future__ import annotations

import math
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import fp_load  # noqa: E402
import make_pcb as MP  # noqa: E402
import route as R  # noqa: E402

TOL = 0.011                 # mm: two copper ends this close are joined
JANELA = 150                # cells round the two ends the search may use (22 mm)
FIXAS = {"GND"} | set(R.NAO_ROTEAR) | set(R.PAR)


def _sem_aspas(v: str) -> str:
    return v.strip('"')


def ler_cobre(arv, numeros: dict) -> tuple[list, list]:
    """Segments and vias of the board file, in route.py's terms."""
    por_num = {n: r for r, n in numeros.items()}
    segs, vias = [], []
    for s in fp_load.kids(arv, "segment"):
        a = fp_load.kid(s, "start")
        b = fp_load.kid(s, "end")
        w = float(fp_load.kid(s, "width")[1])
        lay = _sem_aspas(fp_load.kid(s, "layer")[1])
        net = por_num.get(int(fp_load.kid(s, "net")[1]), "")
        segs.append(((float(a[1]) - MP.ORIGEM[0], float(a[2]) - MP.ORIGEM[1]),
                     (float(b[1]) - MP.ORIGEM[0], float(b[2]) - MP.ORIGEM[1]),
                     R.CAMADAS.index(lay), net, w))
    for v in fp_load.kids(arv, "via"):
        at = fp_load.kid(v, "at")
        net = por_num.get(int(fp_load.kid(v, "net")[1]), "")
        vias.append((float(at[1]) - MP.ORIGEM[0], float(at[2]) - MP.ORIGEM[1], net))
    return segs, vias


def grade(arv, todos, segs, vias) -> R.Grade:
    """The router's grid with the pads, the rules and every piece of copper."""
    g = R.base(arv, todos)
    for p0, p1, c, r, w in segs:
        g.trilha(c, p0, p1, w, r, R.folga_de(r))
    for x, y, r in vias:
        g.via(x, y, r, R.folga_de(r))
    R.fechar_sob_gnss(g)
    return g


def _dentro(px, py, q) -> bool:
    x, y = q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1]
    if id(q) in R.PADS_REDONDOS:
        # a round pad is round: the module's LGA pads and the test points
        return math.hypot(px - x, py - y) <= min(q[4], q[5]) + TOL
    return abs(px - x) <= q[4] + TOL and abs(py - y) <= q[5] + TOL


def pecas(rede: str, pads: list, segs: list, vias: list) -> list[list[tuple]]:
    """The connected pieces of one net, as lists of ("p"|"s"|"v", index)."""
    itens = ([("p", i) for i in range(len(pads))]
             + [("s", i) for i, s in enumerate(segs) if s[3] == rede]
             + [("v", i) for i, v in enumerate(vias) if v[2] == rede])
    pai = {it: it for it in itens}

    def raiz(it):
        while pai[it] != it:
            pai[it] = pai[pai[it]]
            it = pai[it]
        return it

    def unir(a, b):
        pai[raiz(a)] = raiz(b)

    so_s = [it for it in itens if it[0] == "s"]
    so_v = [it for it in itens if it[0] == "v"]
    # Joined as KiCad joins them: copper of the net that overlaps. A track's
    # end inside the pad of a via (a via drawn by hand sits off the grid,
    # 0,05 mm from where the search ended) or on another track of the net
    for it in so_s:
        p0, p1, c, _r, w = segs[it[1]]
        for ponta in (p0, p1):
            for i, q in enumerate(pads):
                if (q[1] < 0 or q[1] == c) and _dentro(ponta[0], ponta[1], q):
                    unir(it, ("p", i))
            for jt in so_v:
                x, y, _r2 = vias[jt[1]]
                if math.hypot(ponta[0] - x, ponta[1] - y) <= R.VIA_D / 2:
                    unir(it, jt)
            for jt in so_s:
                if jt == it:
                    continue
                q0, q1, c2, _r2, w2 = segs[jt[1]]
                if c2 == c and R._dist_ponto_seg(ponta, q0, q1) <= max(TOL, min(w, w2) / 2):
                    unir(it, jt)
    for jt in so_v:
        x, y, _r = vias[jt[1]]
        for i, q in enumerate(pads):
            if _dentro(x, y, q):
                unir(jt, ("p", i))
        # a via anywhere along a track of the net, not only at its end: the
        # other run that dropped it there ends in the middle of this one
        for it in so_s:
            q0, q1, _c, _r2, _w = segs[it[1]]
            if R._dist_ponto_seg((x, y), q0, q1) <= TOL:
                unir(jt, it)
    # and a track that runs through a pad of the net, anywhere along it
    for it in so_s:
        p0, p1, c, _r, w = segs[it[1]]
        n = max(1, int(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / 0.05))
        for i, q in enumerate(pads):
            if not (q[1] < 0 or q[1] == c):
                continue
            if any(_dentro(p0[0] + (p1[0] - p0[0]) * k / n, p0[1] + (p1[1] - p0[1]) * k / n, q)
                   for k in range(n + 1)):
                unir(it, ("p", i))
    grupos: dict = {}
    for it in itens:
        grupos.setdefault(raiz(it), []).append(it)
    return list(grupos.values())


def celulas(g: R.Grade, peca: list, pads: list, segs: list, vias: list) -> set:
    """Every grid cell a new track of the net may start from or end on."""
    out = set()
    for tipo, i in peca:
        if tipo == "p":
            q = pads[i]
            x, y = q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1]
            ix0, iy0 = g.cel(x - q[4], y - q[5])
            ix1, iy1 = g.cel(x + q[4], y + q[5])
            cs = range(R.NC) if q[1] < 0 else (q[1],)
            for c in cs:
                for ix in range(ix0, ix1 + 1):
                    for iy in range(iy0, iy1 + 1):
                        px, py = g.pos(ix, iy)
                        if id(q) in R.PADS_REDONDOS:
                            if math.hypot(px - x, py - y) <= min(q[4], q[5]) - R.LARGURA / 2:
                                out.add((c, ix, iy))
                        elif abs(px - x) <= q[4] and abs(py - y) <= q[5]:
                            out.add((c, ix, iy))
                out.add((c,) + g.cel(x, y))
        elif tipo == "s":
            p0, p1, c, _r, _w = segs[i]
            n = max(1, int(math.ceil(math.hypot(p1[0] - p0[0], p1[1] - p0[1]) / (R.PASSO / 2))))
            for k in range(n + 1):
                f = k / n
                out.add((c,) + g.cel(p0[0] + (p1[0] - p0[0]) * f, p0[1] + (p1[1] - p0[1]) * f))
        else:
            x, y, _r = vias[i]
            for c in range(R.NC):
                out.add((c,) + g.cel(x, y))
    return out


def campos(g: R.Grade, rede: str, pads: list, caixa_fp: dict) -> tuple:
    """Width, neck width and the neck fields of a net, as dados_de() has them."""
    larg = R.largura(rede)
    caixas = []
    for q in pads:
        if min(2 * q[4], 2 * q[5]) - R.PASSO / 2 < larg:
            caixas.append(caixa_fp.get((round(q[2], 4), round(q[3], 4))) or
                          (q[2] - q[4], q[3] - q[5], q[2] + q[4], q[3] + q[5]))
    larg_pad = R.LARGURA if caixas else larg
    margem = R.FOLGA + (larg - larg_pad) / 2 + R.PASSO
    mm, cel = [], []
    for b in caixas:
        m = (b[0] - MP.ORIGEM[0] - margem, b[1] - MP.ORIGEM[1] - margem,
             b[2] - MP.ORIGEM[0] + margem, b[3] - MP.ORIGEM[1] + margem)
        if m not in mm:
            mm.append(m)
            a0, a1 = g.cel(m[0], m[1]), g.cel(m[2], m[3])
            cel.append((a0[0], a0[1], a1[0], a1[1]))
    return larg, larg_pad, mm, cel


def emitir(g: R.Grade, caminho: list, rede: str, larg: float, larg_pad: float,
           campos_mm: list, segs: list, vias: list) -> None:
    """A path of cells as track and vias, necked inside the fields, like
    uma_passagem.emitir() draws it, and marked on the grid."""
    def no_campo(k: int) -> bool:
        px, py = g.pos(caminho[k][1], caminho[k][2])
        return any(c[0] <= px <= c[2] and c[1] <= py <= c[3] for c in campos_mm)

    i = 0
    while i < len(caminho) - 1:
        a = caminho[i]
        j = i + 1
        if caminho[j][0] != a[0]:
            x, y = g.pos(a[1], a[2])
            vias.append((x, y, rede))
            g.via(x, y, rede, R.folga_de(rede))
            i = j
            continue
        d = (caminho[j][1] - a[1], caminho[j][2] - a[2])
        while j + 1 < len(caminho) and caminho[j + 1][0] == a[0] and \
                (caminho[j + 1][1] - caminho[j][1], caminho[j + 1][2] - caminho[j][2]) == d:
            j += 1
        k0 = i
        while k0 < j:
            estreito = no_campo(k0) or no_campo(k0 + 1)
            k1 = k0 + 1
            while k1 < j and (no_campo(k1) or no_campo(k1 + 1)) == estreito:
                k1 += 1
            p0 = g.pos(caminho[k0][1], caminho[k0][2])
            p1 = g.pos(caminho[k1][1], caminho[k1][2])
            w = larg_pad if estreito else larg
            segs.append((p0, p1, a[0], rede, w))
            g.trilha(a[0], p0, p1, w, rede, R.folga_de(rede))
            k0 = k1
        i = j


def buscar(g, rede, de, para, larg_pad, campos_cel, macio=None):
    """The search between two sets of cells, in a box JANELA cells round
    them: a connection that fails here takes seconds, not the minutes of the
    whole board."""
    camadas = (0, R.I_BCU) if R.so_por_fora(rede) else None
    return R.a_estrela(g, rede, next(iter(de)), para, JANELA, orcamento=R.ORCAMENTO,
                       campos=campos_cel, larg_estreita=larg_pad, camadas=camadas,
                       inicios=de, macio=macio)


def soltas(por_rede, segs, vias) -> dict:
    """net -> its pieces, for every net in more than one piece."""
    out = {}
    for rede, pads in por_rede.items():
        if rede in FIXAS or len(pads) < 2:
            continue
        ps = pecas(rede, pads, segs, vias)
        if len(ps) > 1:
            out[rede] = ps
    return out


def afrouxar_por_dentro(rede: str) -> bool:
    """The second easing: the net may take the inner layer too, at the
    width IPC-2221 asks there for its current (CORRENTE of the dry run),
    rounded up to 0,05 mm. VSYS and VBUS, both kept outside, met on the
    right side of the nPM1300 with their capacitors the other way round from
    their pins, and the back already held the VSYS trunk and the VBUS way to
    the USB: one of the two has to cross inside (2026-10-19)."""
    import dry_run_pcb as D
    if rede not in D.CORRENTE or rede in R.NAO_ROTEAR:
        return False
    if R.ACABAMENTO.get(rede, (0.0, True))[1] is False and rede in R.ACABAMENTO:
        return False
    w = max(R.LARGURA, math.ceil(round(D.largura_ipc(D.CORRENTE[rede][0], interna=True) / 0.01, 6)) * 0.01)
    if w > 0.45:
        return False                     # wider than that has no room inside
    R.ACABAMENTO[rede] = (round(max(w, R.largura(rede) if rede not in R.ACABAMENTO else 0.0), 3), False)
    return True


def juntar(g, rede, pads, caixa_fp, segs, vias, pode_afrouxar=True,
           maximo: int = 99) -> bool:
    """The pieces of one net into one, by 1 and then 2, `maximo`
    connections at most. True if it is whole, or if `maximo` were made."""
    feitas = 0
    for tentativa in (0, 1, 2):
        if tentativa == 1:
            if not pode_afrouxar or not R.afrouxar_regra(rede):
                continue
        if tentativa == 2:
            if not pode_afrouxar or not afrouxar_por_dentro(rede):
                return False
        while True:
            ps = pecas(rede, pads, segs, vias)
            if len(ps) < 2 or feitas >= maximo:
                return True
            larg, larg_pad, campos_mm, campos_cel = campos(g, rede, pads, caixa_fp)
            cels = [celulas(g, p, pads, segs, vias) for p in ps]
            # the biggest piece first, to the nearest other
            ordem = sorted(range(len(ps)), key=lambda i: -len(cels[i]))
            a = ordem[0]
            alvos = set().union(*(cels[i] for i in ordem[1:]))
            p = buscar(g, rede, cels[a], alvos, larg_pad, campos_cel)
            if p is None:
                break
            emitir(g, p, rede, larg, larg_pad, campos_mm, segs, vias)
            feitas += 1
    return False


# The connections finished by hand, where the search alone fights for one
# channel with the nets already in it: (net, "REF.pad", points). The copper
# of every other net within ABRIR of a point comes out, the pad is joined
# through the points to the biggest piece of its net, and the nets that came
# out are joined again. A point may carry a layer name ("F.Cu", "In2.Cu",
# "B.Cu"); two in a row on the same spot with two layers make a via.
GUIAS: list[tuple[str, str, list[tuple]]] = [
    # PVDD of the nPM1300, boxed between its two switching pins: a via 0,144
    # mm from the corner of pin 3, down to the VSYS of the back
    ("VSYS", "U101.4", [(13.95, 46.2, "F.Cu"), (13.95, 46.2, "B.Cu")]),
    # (pin 20 of the nPM1300 and the C1 bump of the MAX17262 are in MAO: the
    # grid had no room for either)
    # the 3V0 side of the module's jumper, under it to the inner layer
    ("3V0", "JP102.1", [(23.55, 91.8, "F.Cu"), (23.55, 91.8, "In2.Cu")]),
    # DIS_SW of the harvester: up out of the pin and left above the pins,
    # to the divider beside it
    ("DIS_SW", "U103.22", [(25.8, 40.8, "F.Cu"), (22.95, 40.8, "F.Cu")]),
]
ABRIR = 0.7


# KiCad's tests are strict and in whole nanometres: two tracks 0.15 wide
# whose centre lines are 0.150 mm apart only touch edge to edge, and are not
# joined; in floating point that came out 3e-15 inside (2026-10-19)
NM = 1e-6


def _alcance(ponta, w: float, c: int, tipo: str, item) -> float:
    """How far the copper of `item` reaches round `ponta`, the end of a
    track w wide on layer c, as KiCad's dangling test takes it (the item's
    shape within w/2 of the point): > 0 when it takes the end in."""
    if tipo == "s":
        q0, q1, c2, _r, w2 = item
        if c2 != c:
            return -1.0
        return (w + w2) / 2 - R._dist_ponto_seg(ponta, q0, q1) - NM
    if tipo == "v":
        return (R.VIA_D + w) / 2 - math.hypot(ponta[0] - item[0], ponta[1] - item[1]) - NM
    if not (item[1] < 0 or item[1] == c):
        return -1.0
    x, y = item[2] - MP.ORIGEM[0], item[3] - MP.ORIGEM[1]
    if id(item) in R.PADS_REDONDOS:
        return min(item[4], item[5]) + w / 2 - math.hypot(ponta[0] - x, ponta[1] - y) - NM
    dx, dy = abs(ponta[0] - x) - item[4], abs(ponta[1] - y) - item[5]
    fora = math.hypot(max(dx, 0.0), max(dy, 0.0)) if max(dx, dy) > 0 else max(dx, dy)
    return w / 2 - fora - NM


def _perto_de(tipo: str, item, ponta) -> float:
    """KiCad's getMinDist, which picks the end an item counts for when it
    takes in both: from a track's nearer end (not from its line), or from a
    via's or a pad's centre."""
    if tipo == "s":
        return min(math.hypot(ponta[0] - item[0][0], ponta[1] - item[0][1]),
                   math.hypot(ponta[0] - item[1][0], ponta[1] - item[1][1]))
    if tipo == "v":
        return math.hypot(ponta[0] - item[0], ponta[1] - item[1])
    return math.hypot(ponta[0] - (item[2] - MP.ORIGEM[0]), ponta[1] - (item[3] - MP.ORIGEM[1]))


def _pe(p, a, b) -> tuple:
    """The point of segment a-b nearest p."""
    dx, dy = b[0] - a[0], b[1] - a[1]
    n = dx * dx + dy * dy
    u = 0.0 if n == 0 else min(max(((p[0] - a[0]) * dx + (p[1] - a[1]) * dy) / n, 0.0), 1.0)
    return (round(a[0] + u * dx, 4), round(a[1] + u * dy, 4))


def _pontas(p0, p1, c, w, outros) -> tuple[int, int]:
    """How many items take in each end of a track from p0 to p1, counted
    as KiCad's dangling test counts them: an item that takes in both ends
    counts for one only, the one nearer its own ends (_perto_de)."""
    n0 = n1 = 0
    for tipo, it in outros:
        a0, a1 = _alcance(p0, w, c, tipo, it), _alcance(p1, w, c, tipo, it)
        if a0 > 0 and a1 > 0:
            if _perto_de(tipo, it, p0) < _perto_de(tipo, it, p1):
                n0 += 1
            else:
                n1 += 1
        elif a0 > 0:
            n0 += 1
        elif a1 > 0:
            n1 += 1
    return n0, n1


def _corte(i, segs, outros) -> tuple | None:
    """What to do with track i when KiCad would call it dangling: None when
    both ends are taken in, else ("cortar", new track) back to a point
    along it that other copper of the net touches, or ("tirar",) when no
    such cut leaves both ends taken in. The cut points are tried from the
    farthest to the nearest, each with KiCad's own count (_pontas): a short
    spur whose two ends sit in one via is dangling, a spur cut back to the
    end of the track that holds it is not."""
    p0, p1, c, r, w = segs[i]
    n0, n1 = _pontas(p0, p1, c, w, outros)
    if n0 and n1:
        return None
    fixa, solta = (p0, p1) if n1 == 0 else (p1, p0)
    comp = math.hypot(solta[0] - fixa[0], solta[1] - fixa[1])
    if comp < 1e-6:
        return ("tirar",)
    ux, uy = (solta[0] - fixa[0]) / comp, (solta[1] - fixa[1]) / comp
    cortes = []
    for tipo, it in outros:
        if tipo == "s":
            if it[2] != c:
                continue
            pts, lim = (it[0], it[1]), (w + it[4]) / 2
        elif tipo == "v":
            pts, lim = ((it[0], it[1]),), (w + R.VIA_D) / 2
        else:
            if not (it[1] < 0 or it[1] == c):
                continue
            pts, lim = ((it[2] - MP.ORIGEM[0], it[3] - MP.ORIGEM[1]),), None
        for q in pts:
            d = min(max((q[0] - fixa[0]) * ux + (q[1] - fixa[1]) * uy, 0.0), comp)
            pt = (round(fixa[0] + ux * d, 4), round(fixa[1] + uy * d, 4))
            if lim is None:
                if _alcance(pt, w, c, "p", it) <= w / 2:
                    continue                  # not inside the pad itself
            elif math.hypot(pt[0] - q[0], pt[1] - q[1]) >= lim - NM:
                continue
            if 0.01 <= d < comp - 1e-6:
                cortes.append((d, pt))
    for d, pt in sorted(cortes, reverse=True):
        a, b = _pontas(fixa, pt, c, w, outros)
        if a and b:
            return ("cortar", (fixa, pt, c, r, w))
    # the loose end may sit on a track KiCad counted for the other end
    # (it takes in both, and its own ends are nearer the other): a short
    # jog from the end of one track onto the middle of another. The jog
    # then goes the shortest way, square onto it (SENS_SCL, 2026-10-19)
    for tipo, it in outros:
        if tipo == "s" and _alcance(solta, w, c, tipo, it) > 0 and _alcance(fixa, w, c, tipo, it) > 0:
            pe = _pe(fixa, it[0], it[1])
            if math.hypot(pe[0] - fixa[0], pe[1] - fixa[1]) > 0.01:
                return ("cortar", (fixa, pe, c, r, w))
    return ("tirar",)


def podar(por_rede, segs, vias) -> int:
    """Take out what KiCad's DRC calls dangling: a track end no copper of
    its net takes in goes back to the last point along the track that other
    copper of the net touches (or the track comes out, when nothing does),
    and a via joined to the copper of one layer only comes out. One change
    at a time, kept only if its net is in no more pieces than before. Ground
    and the fixed nets are left alone: their vias meet the pours."""
    feitos = 0
    recusados: set = set()
    while True:
        da_rede: dict = {}
        for i, t in enumerate(segs):
            da_rede.setdefault(t[3], []).append(i)
        mudanca = None
        for i, t in enumerate(segs):
            r = t[3]
            if r in FIXAS or t in recusados:
                continue
            outros = ([("s", segs[j]) for j in da_rede[r] if j != i]
                      + [("v", v) for v in vias if v[2] == r]
                      + [("p", q) for q in por_rede.get(r, [])])
            acao = _corte(i, segs, outros)
            if acao is not None:
                mudanca = (r, t, acao)
                break
        if mudanca is None:
            for v in vias:
                r = v[2]
                if r in FIXAS or v in recusados:
                    continue
                camadas = set()
                for j in da_rede.get(r, []):
                    t = segs[j]
                    if R._dist_ponto_seg((v[0], v[1]), t[0], t[1]) <= (R.VIA_D + t[4]) / 2 + 1e-6:
                        camadas.add(t[2])
                for q in por_rede.get(r, []):
                    if _alcance((v[0], v[1]), R.VIA_D, q[1] if q[1] >= 0 else 0, "p", q) > 0:
                        camadas.update((q[1],) if q[1] >= 0 else (-1, -2))
                if len(camadas) < 2:
                    mudanca = (r, v, ("via",))
                    break
        if mudanca is None:
            return feitos
        r, item, acao = mudanca
        if acao[0] == "cortar" and acao[1] == item:
            recusados.add(item)
            continue
        antes = len(pecas(r, por_rede.get(r, []), segs, vias))
        guarda = (list(segs), list(vias))
        if acao[0] == "via":
            vias.remove(item)
        else:
            k = segs.index(item)
            if acao[0] == "tirar":
                del segs[k]
            else:
                segs[k] = acao[1]
        if len(pecas(r, por_rede.get(r, []), segs, vias)) > antes:
            segs[:], vias[:] = guarda
            recusados.add(item)
            continue
        feitos += 1


def guiar(arv, todos, por_rede, caixa_fp, rede, nome_pad, pontos, segs, vias):
    """One hand finished connection (GUIAS). Returns the grid, or None if
    it could not be drawn (and then segs and vias are as they were)."""
    q = R.PAD_POR_NOME.get(nome_pad)
    if q is None:
        return None
    # a pad already joined to the rest of its net is left alone
    pads_r = por_rede[rede]
    i_q = next(i for i, p_ in enumerate(pads_r) if p_ is q)
    ps_r = pecas(rede, pads_r, segs, vias)
    if len(ps_r) < 2 or max(ps_r, key=len) is next(p_ for p_ in ps_r if ("p", i_q) in p_):
        return "ja"
    guarda = (list(segs), list(vias))
    tirar = set()
    for pt in pontos:
        x, y = pt[0], pt[1]
        for s_ in segs:
            if (s_[3] != rede and s_[3] not in FIXAS
                    and R._dist_ponto_seg((x, y), s_[0], s_[1]) <= ABRIR + s_[4] / 2):
                tirar.add(s_[3])
        for v in vias:
            if v[2] != rede and v[2] not in FIXAS and math.hypot(v[0] - x, v[1] - y) <= ABRIR + R.VIA_D / 2:
                tirar.add(v[2])
    segs[:] = [s_ for s_ in segs if s_[3] not in tirar or min(
        R._dist_ponto_seg((pt[0], pt[1]), s_[0], s_[1]) for pt in pontos) > ABRIR + s_[4] / 2]
    vias[:] = [v for v in vias if v[2] not in tirar or min(
        math.hypot(v[0] - pt[0], v[1] - pt[1]) for pt in pontos) > ABRIR + R.VIA_D / 2]
    g = grade(arv, todos, segs, vias)
    pads = por_rede[rede]
    larg, larg_pad, campos_mm, campos_cel = campos(g, rede, pads, caixa_fp)
    ps = pecas(rede, pads, segs, vias)
    i_pad = next(i for i, p_ in enumerate(pads) if p_ is q)
    meu = next(p_ for p_ in ps if ("p", i_pad) in p_)
    outros = [p_ for p_ in ps if p_ is not meu]
    if not outros:
        return g
    alvo_final = celulas(g, max(outros, key=len), pads, segs, vias)
    partida = celulas(g, meu, pads, segs, vias)
    camadas = (0, R.I_BCU) if R.so_por_fora(rede) else None
    trechos = []
    for pt in list(pontos) + [None]:
        if pt is None:
            alvo = alvo_final
        else:
            ix, iy = g.cel(pt[0], pt[1])
            cs = (R.CAMADAS.index(pt[2]),) if len(pt) > 2 else (camadas or tuple(range(R.NC)))
            alvo = {(c, ix, iy) for c in cs}
        p = R.a_estrela(g, rede, next(iter(partida)), alvo, JANELA, orcamento=R.ORCAMENTO,
                        campos=campos_cel, larg_estreita=larg_pad, camadas=camadas,
                        inicios=partida)
        if p is None:
            print(f"    guia de {rede} em {nome_pad} parou antes de {pt}", flush=True)
            segs[:], vias[:] = guarda
            return None
        trechos.append(p)
        partida = {p[-1]}
    for p in trechos:
        emitir(g, p, rede, larg, larg_pad, campos_mm, segs, vias)
    for r_ in sorted(tirar):
        juntar(g, r_, por_rede[r_], caixa_fp, segs, vias)
    return g


# Copper drawn by hand, in exact coordinates, where the grid's 0,15 mm steps
# have no room: (net, layer, points, width) polylines and (x, y, net) vias.
# TIRAR lists the tracks of the router that ran there before, by net and
# end points (board mm, 0,01 of tolerance); they go first.
TIRAR: list[tuple[str, tuple, tuple]] = [
    # PWR_SDA reached the C1 bump of the MAX17262 from the upper left and
    # closed the only way out of B1 (PWR_SCL)
    ("PWR_SDA", (15.45, 52.2), (15.75, 52.2)),
    ("PWR_SDA", (15.75, 52.2), (16.2, 52.65)),
    ("PWR_SDA", (16.2, 52.65), (16.2, 52.8)),
    ("PWR_SDA", (16.2, 52.8), (16.35, 52.95)),
    ("PWR_SDA", (16.35, 52.95), (16.65, 52.95)),
    # the right side of the nPM1300: VBUS ran down in front of the VSYS pin
    # and took the only spot for its via; CC2 ran down the column under the
    # pin ends on the inner layer, where the VBUS via goes now
    ("VBUS", (19.8, 46.35), (20.1, 46.35)), ("VBUS", (20.1, 46.35), (20.1, 46.8)),
    ("VBUS", (20.1, 46.8), (20.25, 46.95)), ("VBUS", (20.25, 46.95), (20.55, 47.25)),
    ("VBUS", (20.55, 47.25), (20.55, 47.4)),
    ("CC2", (19.5, 43.8), (19.5, 45.15)), ("CC2", (19.5, 45.15), (19.8, 45.45)),
    ("CC2", (19.8, 48.0), (19.8, 45.45)), ("CC2", (19.8, 48.0), (20.1, 48.3)),
    ("CC2", (20.1, 48.3), (20.1, 57.0)),
    # what route.py's D- left inside the receptacle's box on its way round
    # the left edge (REFAZER keeps the box whole)
    ("USB_DM", (5.25, 89.4), (5.4, 89.4)),
    # under the U.FL's own mask opening (make_pcb.janelas_de_mascara): the
    # ground stub of its upper pad, which has its own via inside the pad
    ("GND", (23.545, 10.925), (23.55, 11.85)),
]
MAO: list[tuple[str, str, list[tuple], float]] = [
    # the two left bumps of the 0,4 mm WLP, one above the other: B1 out to
    # x 16,30 and up to its via, C1 out to x 15,965 and up to its own; at
    # 0,127 mm the closest copper is 0,1365 mm away (the A1 bump)
    ("PWR_SCL", "F.Cu", [(16.6, 52.5), (16.3, 52.5), (16.3, 51.6), (16.35, 51.45)], 0.127),
    ("PWR_SDA", "F.Cu", [(16.6, 52.9), (15.965, 52.9), (15.965, 52.35), (15.45, 52.2)], 0.127),
    # VSYS out of pin 20 to a via on the VSYS that climbs the back to C103;
    # VBUS down a via inside its own pin 21 to the VBUS of the back (C101
    # keeps the via inside its pad)
    ("VSYS", "F.Cu", [(19.6, 46.75), (20.1, 46.75), (20.3, 46.95)], 0.25),
    ("VBUS", "B.Cu", [(19.55, 46.25), (19.55, 46.65), (19.65, 46.8)], 0.175),
    # The USB pair from the protection diode to the module, side by side:
    # 0.15 mm with a 0.2 mm gap, 93.6 ohm differential on the JLC3313 stack
    # (impedancia.py). D- north of D+ the whole way, so the two right turns
    # bring D- to the east pin with no crossing. route.py routed the two
    # apart and D- went round by the left edge of the board: 67.6 mm against
    # 32.4, 35 mm of skew, where TI's layout guide asks 50 mil (SPRAAR7J,
    # table A-1; 2026-10-19). D- runs outside on every turn and lands 1.1 mm
    # further east, so D+ takes it back in a loop between R114 and the
    # mounting pad of J202, where the front is free to y 77.
    ("USB_DM", "F.Cu", [(4.5, 78.7), (4.5, 77.655), (7.205, 74.95), (21.35, 74.95),
                        (21.65, 75.25), (21.65, 76.5)], 0.15),
    ("USB_DP", "F.Cu", [(5.4, 77.25), (7.35, 75.3), (10.055, 75.3), (10.055, 76.315),
                        (10.45, 76.315), (10.45, 75.3), (20.4, 75.3), (20.55, 75.45),
                        (20.55, 76.5)], 0.15),
]
# Nets whose route MAO gives whole: their copper comes out except inside the
# boxes (board mm, x0 y0 x1 y1), where route.py's own hand drawing stays - the
# USB pair from the receptacle through the diode, and the ties between the
# two contacts of each line under the receptacle.
REFAZER: dict[str, list[tuple[float, float, float, float]]] = {
    "USB_DM": [(4.3, 78.6, 6.0, 87.1), (5.0, 88.1, 6.9, 91.4)],
    "USB_DP": [(4.8, 77.2, 6.4, 87.1), (5.7, 88.1, 7.7, 90.3)],
}
VIAS_MAO: list[tuple[float, float, str]] = [(20.3, 46.95, "VSYS"), (19.55, 46.25, "VBUS")]
# vias of the router that come out with TIRAR: (net, x, y)
TIRAR_VIAS: list[tuple[str, float, float]] = [
    # the two ground vias under the U.FL's mask opening
    ("GND", 23.55, 11.85), ("GND", 23.1, 12.45),
]


def _dist_seg_seg(a0, a1, b0, b1) -> float:
    """Shortest distance between two segments, 0 when they cross."""
    def lado(p, q, r):
        return (q[0] - p[0]) * (r[1] - p[1]) - (q[1] - p[1]) * (r[0] - p[0])
    if (lado(a0, a1, b0) * lado(a0, a1, b1) < 0
            and lado(b0, b1, a0) * lado(b0, b1, a1) < 0):
        return 0.0
    return min(R._dist_ponto_seg(a0, b0, b1), R._dist_ponto_seg(a1, b0, b1),
               R._dist_ponto_seg(b0, a0, a1), R._dist_ponto_seg(b1, a0, a1))


def aplicar_mao(segs, vias) -> None:
    """REFAZER and TIRAR out, MAO and VIAS_MAO in, and the copper of every
    other net in the way of the hand copper out too: that net is then in
    pieces, and the finish joins it again round the hand copper."""
    def igual(a, b):
        return abs(a[0] - b[0]) < 0.01 and abs(a[1] - b[1]) < 0.01

    def na_caixa(p, caixas):
        return any(c[0] <= p[0] <= c[2] and c[1] <= p[1] <= c[3] for c in caixas)
    for rede, caixas in REFAZER.items():
        segs[:] = [s_ for s_ in segs if s_[3] != rede or
                   (na_caixa(s_[0], caixas) and na_caixa(s_[1], caixas))]
        vias[:] = [v for v in vias if v[2] != rede or na_caixa(v, caixas)]
    for rede, a, b in TIRAR:
        segs[:] = [s_ for s_ in segs if not (s_[3] == rede and (
            (igual(s_[0], a) and igual(s_[1], b)) or (igual(s_[0], b) and igual(s_[1], a))))]
    for rede, x, y in TIRAR_VIAS:
        vias[:] = [v for v in vias if not (v[2] == rede and abs(v[0] - x) < 0.01 and abs(v[1] - y) < 0.01)]
    for rede, camada, pts, w in MAO:
        c = R.CAMADAS.index(camada)
        for k in range(len(pts) - 1):
            a_, b_ = pts[k], pts[k + 1]
            if not any(s_[3] == rede and s_[2] == c and igual(s_[0], a_) and igual(s_[1], b_)
                       for s_ in segs):
                segs.append((a_, b_, c, rede, w))
    for v in VIAS_MAO:
        if not any(u[2] == v[2] and abs(u[0] - v[0]) < 0.01 and abs(u[1] - v[1]) < 0.01 for u in vias):
            vias.append(v)
    # what of the other nets is too close to the hand copper
    maos = [(tuple(pts[k]), tuple(pts[k + 1]), R.CAMADAS.index(cam), rede, w)
            for rede, cam, pts, w in MAO for k in range(len(pts) - 1)]
    fora: dict[str, int] = {}
    for i, t in enumerate(segs):
        if t[3] in FIXAS:
            continue
        for m in maos:
            if m[3] == t[3] or m[2] != t[2]:
                continue
            lim = m[4] / 2 + t[4] / 2 + max(folga_real(m[3]), folga_real(t[3]))
            if _dist_seg_seg(m[0], m[1], t[0], t[1]) < lim - 1e-6:
                fora[t[3]] = fora.get(t[3], 0) + 1
                segs[i] = None
                break
    segs[:] = [t for t in segs if t is not None]
    for i, v in enumerate(vias):
        if v[2] in FIXAS:
            continue
        for m in maos:
            if m[3] == v[2]:
                continue
            lim = m[4] / 2 + R.VIA_D / 2 + max(folga_real(m[3]), folga_real(v[2]))
            if R._dist_ponto_seg((v[0], v[1]), m[0], m[1]) < lim - 1e-6:
                fora[v[2]] = fora.get(v[2], 0) + 1
                vias[i] = None
                break
    vias[:] = [v for v in vias if v is not None]
    for rede, n in sorted(fora.items()):
        print(f"  {rede}: {n} pedacos no caminho do cobre a mao sairam", flush=True)
    # the RF lines at the width route.py gives them now: same centre line,
    # narrower copper, so nothing gets closer to anything
    for i, t in enumerate(segs):
        if t[3] in R.NAO_ROTEAR and abs(t[4] - R.largura(t[3])) > 1e-6:
            segs[i] = (t[0], t[1], t[2], t[3], R.largura(t[3]))


def folga_real(rede: str) -> float:
    """The clearance the DRC asks of a net's class (gnssbike.kicad_pro)."""
    return 0.2 if rede.startswith("USB_D") else 0.127


def vias_contra_ilhas(todos, segs, vias) -> int:
    """Take out every via closer to a pad of another net than the classes
    of the two allow, with the tracks of its net that end on it: the net is
    then in pieces, and the finish joins it again on the grid, which knows
    each pad's class. A VBUSOUT via 0.175 mm from a USB_DP pad of the module
    (the USB class asks 0.2) came out of route.py before the grid did."""
    tiradas = 0
    for v in list(vias):
        x, y, rv = v
        for q in todos:
            if not q[0] or q[0] == rv:
                continue
            qx, qy = q[2] - MP.ORIGEM[0], q[3] - MP.ORIGEM[1]
            if abs(qx - x) > q[4] + 1.0 or abs(qy - y) > q[5] + 1.0:
                continue
            dx = max(abs(qx - x) - q[4], 0.0)
            dy = max(abs(qy - y) - q[5], 0.0)
            if math.hypot(dx, dy) - R.VIA_D / 2 < max(folga_real(rv), folga_real(q[0])) - 0.002:
                vias.remove(v)
                segs[:] = [s_ for s_ in segs if not (s_[3] == rv and min(
                    math.hypot(s_[0][0] - x, s_[0][1] - y),
                    math.hypot(s_[1][0] - x, s_[1][1] - y)) <= TOL)]
                print(f"  via de {rv} em ({x:.2f}; {y:.2f}) a {math.hypot(dx, dy) - R.VIA_D / 2:.3f} mm "
                      f"da ilha de {q[0]}: saiu", flush=True)
                tiradas += 1
                break
    return tiradas


def main() -> int:
    caminho = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else R.HERE / "gnssbike.kicad_pcb"
    texto = caminho.read_text(encoding="utf-8")
    arv = fp_load.parse(texto)
    numeros = fp_load.redes_da_placa(arv)
    todos, por_rede, caixa_fp = R.pads_da_placa(arv)
    R.ACABAMENTO.clear()
    segs, vias = ler_cobre(arv, numeros)
    aplicar_mao(segs, vias)
    vias_contra_ilhas(todos, segs, vias)
    # and a track of another net too close to a via loses its stretch there:
    # its net is then in pieces, and the finish joins it on the grid
    for rv, rt, d, (x, y) in R.vias_perto(segs, vias):
        if rt in FIXAS:
            continue
        antes_n = len(segs)
        segs[:] = [s_ for s_ in segs if not (s_[3] == rt and
                   R._dist_ponto_seg((x, y), s_[0], s_[1]) < R.VIA_D / 2 + s_[4] / 2 + 0.127)]
        print(f"  trilha de {rt} a {d:.3f} mm da via de {rv} em ({x:.2f}; {y:.2f}): "
              f"{antes_n - len(segs)} trechos sairam", flush=True)
    g = grade(arv, todos, segs, vias)
    antes = soltas(por_rede, segs, vias)
    n0 = sum(len(v) - 1 for v in antes.values())
    print(f"acabamento: {n0} ligacoes soltas em {len(antes)} redes: "
          + ", ".join(f"{r} {len(v) - 1}" for r, v in sorted(antes.items())), flush=True)
    def total() -> int:
        return sum(len(v) - 1 for v in soltas(por_rede, segs, vias).values())

    for rede, nome_pad, pontos in GUIAS:
        n_antes = total()
        guarda = (list(segs), list(vias), dict(R.ACABAMENTO))
        g2 = guiar(arv, todos, por_rede, caixa_fp, rede, nome_pad, pontos, segs, vias)
        if g2 == "ja":
            continue
        n_depois = total()
        if g2 is not None and n_depois < n_antes:
            g = g2
            print(f"  guia de {rede} em {nome_pad}: {n_antes} -> {n_depois} soltas", flush=True)
        else:
            segs[:], vias[:] = guarda[0], guarda[1]
            R.ACABAMENTO.clear()
            R.ACABAMENTO.update(guarda[2])
            g = grade(arv, todos, segs, vias)
            print(f"  guia de {rede} em {nome_pad}: nao ficou ({n_antes} -> {n_depois})", flush=True)
        R.escrever(caminho, texto, numeros, segs, vias)

    tabu: set = set()
    for _volta in range(40):
        faltam = soltas(por_rede, segs, vias)
        if not faltam:
            break
        mudou = False
        for rede in sorted(faltam):
            pads = por_rede[rede]
            n_antes = total()
            if juntar(g, rede, pads, caixa_fp, segs, vias):
                print(f"  {rede}: inteira", flush=True)
                mudou = True
                continue
            if total() < n_antes:
                print(f"  {rede}: {n_antes} -> {total()}", flush=True)
                mudou = True
            # 3. one connection through whoever is in the way
            larg, larg_pad, campos_mm, campos_cel = campos(g, rede, pads, caixa_fp)
            ps = pecas(rede, pads, segs, vias)
            if len(ps) < 2:
                continue
            cels = [celulas(g, p, pads, segs, vias) for p in ps]
            ordem = sorted(range(len(ps)), key=lambda i: -len(cels[i]))
            macio = R.pintar_macio(segs, vias, FIXAS)
            p = buscar(g, rede, cels[ordem[0]], set().union(*(cels[i] for i in ordem[1:])),
                       larg_pad, campos_cel, macio=macio)
            if p is None:
                print(f"  {rede}: sem caminho nem por cima dos outros", flush=True)
                continue
            no_caminho = set(p)
            bloq = {n for k in p for n in macio.get(k, ())} - {rede} - FIXAS
            n_antes = total()
            # one blocking net at a time, and then all of them together
            grupos = [[b_] for b_ in sorted(bloq, key=lambda n: sum(1 for s_ in segs if s_[3] == n))]
            if 1 < len(bloq) <= 8:
                grupos.append(sorted(bloq))
            for grupo in grupos:
                b = grupo[0] if len(grupo) == 1 else "+".join(grupo)
                guarda = (list(segs), list(vias), dict(R.ACABAMENTO))
                # the blocking net's copper on the way comes out: every piece
                # of it whose clearance the way runs through, not only those
                # whose centre line it crosses - the soft search walks the
                # halo of a track far more often than the track
                fora = set()
                larg_r = R.largura(rede)
                pontos = [(k[0],) + g.pos(k[1], k[2]) for k in no_caminho]
                for i_s, s_ in enumerate(segs):
                    if s_[3] not in grupo:
                        continue
                    lim = s_[4] / 2 + R.FOLGA + larg_r / 2 + R.PASSO
                    if any(c == s_[2] and R._dist_ponto_seg((x, y), s_[0], s_[1]) <= lim
                           for c, x, y in pontos):
                        fora.add(("s", i_s))
                for i_v, v in enumerate(vias):
                    if v[2] not in grupo:
                        continue
                    lim = R.VIA_D / 2 + R.FOLGA + larg_r / 2 + R.PASSO
                    if any(math.hypot(x - v[0], y - v[1]) <= lim for _c, x, y in pontos):
                        fora.add(("v", i_v))
                if not fora:
                    continue
                segs[:] = [s_ for i_s, s_ in enumerate(segs) if ("s", i_s) not in fora]
                vias[:] = [v for i_v, v in enumerate(vias) if ("v", i_v) not in fora]
                g = grade(arv, todos, segs, vias)
                n_rede = len(pecas(rede, pads, segs, vias))
                juntar(g, rede, pads, caixa_fp, segs, vias, maximo=1)
                passou = len(pecas(rede, pads, segs, vias)) < n_rede
                for b_ in grupo:
                    juntar(g, b_, por_rede[b_], caixa_fp, segs, vias)
                n_depois = total()
                # a pair that swapped once never swaps back: the second net
                # finds another way, or goes over somebody else
                chave = frozenset([rede] + grupo)
                if n_depois < n_antes or (n_depois == n_antes and passou and chave not in tabu):
                    tabu.add(chave)
                    print(f"  {rede}: uma ligacao por cima de {b} ({n_antes} -> {n_depois} soltas)",
                          flush=True)
                    mudou = True
                    break
                segs[:], vias[:] = guarda[0], guarda[1]
                R.ACABAMENTO.clear()
                R.ACABAMENTO.update(guarda[2])
                g = grade(arv, todos, segs, vias)
            else:
                # 4. a corridor: every piece of copper within ABRIR of the
                # way the soft search found comes out, the connection goes
                # along it, and everything that came out is joined again
                guarda = (list(segs), list(vias), dict(R.ACABAMENTO))
                pts = [(g.pos(k[1], k[2])[0], g.pos(k[1], k[2])[1]) for k in p]
                pts = pts[::2] + [pts[-1]]
                tirar = set()
                for s_ in segs:
                    if s_[3] != rede and s_[3] not in FIXAS and any(
                            R._dist_ponto_seg(pt, s_[0], s_[1]) <= ABRIR + s_[4] / 2 for pt in pts):
                        tirar.add(s_[3])
                for v in vias:
                    if v[2] != rede and v[2] not in FIXAS and any(
                            math.hypot(v[0] - pt[0], v[1] - pt[1]) <= ABRIR + R.VIA_D / 2 for pt in pts):
                        tirar.add(v[2])
                segs[:] = [s_ for s_ in segs if s_[3] not in tirar or min(
                    R._dist_ponto_seg(pt, s_[0], s_[1]) for pt in pts) > ABRIR + s_[4] / 2]
                vias[:] = [v for v in vias if v[2] not in tirar or min(
                    math.hypot(v[0] - pt[0], v[1] - pt[1]) for pt in pts) > ABRIR + R.VIA_D / 2]
                g = grade(arv, todos, segs, vias)
                juntar(g, rede, pads, caixa_fp, segs, vias, maximo=1)
                for r_ in sorted(tirar):
                    juntar(g, r_, por_rede[r_], caixa_fp, segs, vias)
                n_corr = total()
                if n_corr < n_antes:
                    print(f"  {rede}: por um corredor, tirando {', '.join(sorted(tirar))} "
                          f"({n_antes} -> {n_corr} soltas)", flush=True)
                    mudou = True
                    continue
                segs[:], vias[:] = guarda[0], guarda[1]
                R.ACABAMENTO.clear()
                R.ACABAMENTO.update(guarda[2])
                g = grade(arv, todos, segs, vias)
                # 5. two or three nets fighting for one channel: all of them
                # torn up, whole, and joined again in each order; the order
                # that leaves fewest loose stays, if it is better than now
                if not bloq or len(bloq) > 2:
                    print(f"  {rede}: ainda solta (no caminho: {', '.join(sorted(bloq)) or 'ninguem'})",
                          flush=True)
                    continue
                grupo = [rede] + sorted(bloq)
                guarda = (list(segs), list(vias), dict(R.ACABAMENTO))
                melhor = None
                for ordem_r in ([rede] + sorted(bloq), sorted(bloq) + [rede]):
                    segs[:] = [s_ for s_ in guarda[0] if s_[3] not in grupo]
                    vias[:] = [v for v in guarda[1] if v[2] not in grupo]
                    R.ACABAMENTO.clear()
                    R.ACABAMENTO.update(guarda[2])
                    g = grade(arv, todos, segs, vias)
                    for r_ in ordem_r:
                        juntar(g, r_, por_rede[r_], caixa_fp, segs, vias)
                    n_r = total()
                    if melhor is None or n_r < melhor[0]:
                        melhor = (n_r, list(segs), list(vias), dict(R.ACABAMENTO), ordem_r)
                if melhor[0] < n_antes:
                    segs[:], vias[:] = melhor[1], melhor[2]
                    R.ACABAMENTO.clear()
                    R.ACABAMENTO.update(melhor[3])
                    g = grade(arv, todos, segs, vias)
                    print(f"  {rede}: refeita com {', '.join(sorted(bloq))}, na ordem "
                          f"{' > '.join(melhor[4])} ({n_antes} -> {melhor[0]} soltas)", flush=True)
                    mudou = True
                else:
                    segs[:], vias[:] = guarda[0], guarda[1]
                    R.ACABAMENTO.clear()
                    R.ACABAMENTO.update(guarda[2])
                    g = grade(arv, todos, segs, vias)
                    print(f"  {rede}: ainda solta (no caminho: {', '.join(sorted(bloq))})", flush=True)
        # what each round reached is kept, so a stop loses nothing
        R.escrever(caminho, texto, numeros, segs, vias)
        print(f"  volta {_volta + 1}: {total()} soltas, gravado", flush=True)
        if not mudou:
            break
    # the spurs the rip-ups left are pruned, unless the pruning cuts a
    # connection the finish made: then the spurs stay (a dangling track is a
    # warning in this project's DRC, a loose connection is not 100 %)
    guarda = (list(segs), list(vias))
    n_poda = sum(len(v) - 1 for v in soltas(por_rede, segs, vias).values())
    tirados = podar(por_rede, segs, vias)
    if sum(len(v) - 1 for v in soltas(por_rede, segs, vias).values()) > n_poda:
        segs[:], vias[:] = guarda
        print("  a poda soltaria uma ligacao: as pontas ficam", flush=True)
    else:
        print(f"  poda: {tirados} pontas soltas cortadas e vias de uma camada so tiradas", flush=True)
    depois = soltas(por_rede, segs, vias)
    n1 = sum(len(v) - 1 for v in depois.values())
    ruins = R.conferir(segs, vias)
    print(f"acabamento: {n0} -> {n1} ligacoes soltas; conferencia {len(ruins)}", flush=True)
    for r in ruins[:6]:
        print(f"    {r}")
    for rede, ps in sorted(depois.items()):
        print(f"    solta: {rede} em {len(ps)} pedacos")
    R.escrever(caminho, texto, numeros, segs, vias)
    return 1 if n1 else 0


if __name__ == "__main__":
    sys.exit(main())
