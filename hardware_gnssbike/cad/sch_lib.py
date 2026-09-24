#!/usr/bin/env python3
"""A writer for KiCad 8 schematics, and a router that keeps wires off the parts.

The owner asked for a schematic drawn with real connection lines and no net
labels, with the wires separated so that none runs across a component. That is
two jobs: emitting the file, which is mechanical, and routing, which is not.

Emitting. A KiCad schematic carries its own symbol definitions in lib_symbols,
so nothing here depends on a library being installed. Each part becomes a
rectangle with its pins on the four sides, at the pitch KiCad uses, 1.27 mm.
Symbol space has y upward and sheet space has y downward, so a pin at symbol
(px, py) on a part placed at (X, Y) lands on the sheet at (X + px, Y - py).

Routing. Every net is a tree grown pin by pin with an A* on the 1.27 mm grid:

  - a part's rectangle is an obstacle, so no wire crosses a component;
  - a wire already drawn for the SAME net is free to join, which is what makes
    a branch instead of a second parallel run, and where it joins mid-wire a
    junction dot is written;
  - a wire of ANOTHER net may be CROSSED, at a right angle, because two wires
    crossing without a dot is how a schematic says "these do not connect", and
    forbidding it would make most sheets unroutable. What is forbidden is
    sharing: running along the same line as another net, ending on it, or
    crossing at a point where it turns. KiCad joins collinear wires into one
    net, so a shared run is a short circuit that nothing on the drawing shows;
  - turning costs a little, so runs come out straight instead of staircased.
"""

from __future__ import annotations

import hashlib
import heapq
import math
from dataclasses import dataclass, field

GRID = 1.27
PIN_LEN = 2.54
TEXT = 1.27


def uid(*parts: object) -> str:
    h = hashlib.sha256("|".join(str(p) for p in parts).encode()).hexdigest()
    return f"{h[0:8]}-{h[8:12]}-4{h[13:16]}-8{h[17:20]}-{h[20:32]}"


def snap(v: float) -> float:
    return round(v / GRID) * GRID


def esc(s: str) -> str:
    """Quote a string for the s-expression.

    One unescaped quote inside a note and KiCad refuses the whole file, while
    the file still reads as balanced parentheses, so nothing warns about it.
    The notes on this sheet quote the documents, and the documents use
    quotation marks, so this is not a theoretical case: it happened.
    """
    return s.replace("\\", "\\\\").replace('"', '\\"')


@dataclass(frozen=True)
class Pin:
    number: str
    name: str
    etype: str = "passive"
    side: str = "L"  # L, R, T, B


@dataclass
class Part:
    """One component: its reference, its value and its pins."""
    ref: str
    value: str
    pins: tuple[Pin, ...]
    footprint: str = ""
    datasheet: str = ""
    note: str = ""
    # filled by layout()
    x: float = 0.0
    y: float = 0.0
    w: float = 0.0
    h: float = 0.0

    @property
    def sym_name(self) -> str:
        return f"gnssbike:{self.ref}"

    def _sides(self) -> dict[str, list[Pin]]:
        out: dict[str, list[Pin]] = {"L": [], "R": [], "T": [], "B": []}
        for p in self.pins:
            out[p.side].append(p)
        return out

    def size(self) -> tuple[float, float]:
        """Body size, from the pin counts and the longest name."""
        s = self._sides()
        rows = max(len(s["L"]), len(s["R"]), 1)
        cols = max(len(s["T"]), len(s["B"]), 1)
        longest_lr = max((len(p.name) for p in s["L"] + s["R"]), default=1)
        longest_tb = max((len(p.name) for p in s["T"] + s["B"]), default=1)
        w = max(cols * 2 * GRID + 2 * GRID,
                (longest_lr * 2 + 4) * GRID,
                len(self.value) * GRID + 2 * GRID,
                6 * GRID)
        h = max(rows * 2 * GRID + 2 * GRID, longest_tb * GRID + 4 * GRID, 4 * GRID)
        # Round up to an even number of grid steps, so that half the body is a
        # whole number of steps and every pin tip lands exactly on the grid.
        # Without this the tips fall between grid points and no wire reaches
        # them: KiCad reads the sheet as fully unconnected.
        step = 2 * GRID
        return (math.ceil(w / step - 1e-9) * step, math.ceil(h / step - 1e-9) * step)

    def pin_local(self) -> dict[str, tuple[float, float, int]]:
        """Pin number to its tip in symbol space, and the pin angle."""
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        s = self._sides()
        out: dict[str, tuple[float, float, int]] = {}
        for i, p in enumerate(s["L"]):
            y = hh - GRID * 2 - i * 2 * GRID
            out[p.number] = (-hw - PIN_LEN, y, 0)
        for i, p in enumerate(s["R"]):
            y = hh - GRID * 2 - i * 2 * GRID
            out[p.number] = (hw + PIN_LEN, y, 180)
        for i, p in enumerate(s["T"]):
            x = -hw + GRID * 2 + i * 2 * GRID
            out[p.number] = (x, hh + PIN_LEN, 270)
        for i, p in enumerate(s["B"]):
            x = -hw + GRID * 2 + i * 2 * GRID
            out[p.number] = (x, -hh - PIN_LEN, 90)
        return out

    def pin_sheet(self) -> dict[str, tuple[float, float]]:
        """Pin number to its tip on the sheet (y downward).

        The result must be exactly on the grid, or a wire drawn to it does not
        connect. size() guarantees it as long as the part itself is placed on
        the grid, which layout() does.
        """
        out = {}
        for n, (px, py, _a) in self.pin_local().items():
            x, y = self.x + px, self.y - py
            if abs(x / GRID - round(x / GRID)) > 1e-6 or abs(y / GRID - round(y / GRID)) > 1e-6:
                raise AssertionError(f"{self.ref} pino {n} fora da grade: ({x}; {y})")
            out[n] = (round(x, 4), round(y, 4))
        return out

    def box(self) -> tuple[float, float, float, float]:
        """Body rectangle on the sheet: x0, y0, x1, y1."""
        w, h = self.size()
        return (self.x - w / 2.0, self.y - h / 2.0, self.x + w / 2.0, self.y + h / 2.0)

    def lib_symbol(self) -> str:
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        loc = self.pin_local()
        out = [f'\t\t(symbol "{self.sym_name}"',
               '\t\t\t(pin_names (offset 0.508))',
               '\t\t\t(exclude_from_sim no)\n\t\t\t(in_bom yes)\n\t\t\t(on_board yes)',
               f'\t\t\t(property "Reference" "{esc(self.ref)}"\n\t\t\t\t(at {-hw:.3f} {hh + 1.27:.3f} 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left bottom))\n\t\t\t)',
               f'\t\t\t(property "Value" "{esc(self.value)}"\n\t\t\t\t(at {-hw:.3f} {-hh - 1.27:.3f} 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left top))\n\t\t\t)',
               f'\t\t\t(property "Footprint" "{esc(self.footprint)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(property "Datasheet" "{esc(self.datasheet)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(property "Description" "{esc(self.note)}"\n\t\t\t\t(at 0 0 0)'
               f'\n\t\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t\t)',
               f'\t\t\t(symbol "{self.ref}_0_1"',
               f'\t\t\t\t(rectangle\n\t\t\t\t\t(start {-hw:.3f} {-hh:.3f})'
               f'\n\t\t\t\t\t(end {hw:.3f} {hh:.3f})'
               '\n\t\t\t\t\t(stroke (width 0.254) (type default))'
               '\n\t\t\t\t\t(fill (type background))\n\t\t\t\t)',
               '\t\t\t)',
               f'\t\t\t(symbol "{self.ref}_1_1"']
        for p in self.pins:
            px, py, ang = loc[p.number]
            out.append(
                f'\t\t\t\t(pin {p.etype} line\n\t\t\t\t\t(at {px:.3f} {py:.3f} {ang})'
                f'\n\t\t\t\t\t(length {PIN_LEN})'
                f'\n\t\t\t\t\t(name "{esc(p.name)}" (effects (font (size 1.016 1.016))))'
                f'\n\t\t\t\t\t(number "{esc(p.number)}" (effects (font (size 1.016 1.016))))'
                '\n\t\t\t\t)')
        out += ['\t\t\t)', '\t\t)']
        return "\n".join(out)

    def instance(self, project: str, path: str) -> str:
        w, h = self.size()
        hw, hh = w / 2.0, h / 2.0
        u = uid("inst", self.ref)
        pins = "\n".join(f'\t\t(pin "{esc(p.number)}" (uuid "{uid("pin", self.ref, p.number)}"))'
                         for p in self.pins)
        return (f'\t(symbol\n\t\t(lib_id "{self.sym_name}")\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} 0)\n\t\t(unit 1)\n'
                '\t\t(exclude_from_sim no)\n\t\t(in_bom yes)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
                f'\t\t(uuid "{u}")\n'
                f'\t\t(property "Reference" "{esc(self.ref)}"\n'
                f'\t\t\t(at {self.x - hw:.3f} {self.y - hh - 1.27:.3f} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left bottom))\n\t\t)\n'
                f'\t\t(property "Value" "{esc(self.value)}"\n'
                f'\t\t\t(at {self.x - hw:.3f} {self.y + hh + 1.27:.3f} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (justify left top))\n\t\t)\n'
                f'\t\t(property "Footprint" "{esc(self.footprint)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'\t\t(property "Datasheet" "{esc(self.datasheet)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'\t\t(property "Description" "{esc(self.note)}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                f'\t\t\t(effects (font (size {TEXT} {TEXT})) (hide yes))\n\t\t)\n'
                f'{pins}\n'
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "{path}"\n\t\t\t\t\t(reference "{self.ref}")\n'
                '\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


@dataclass
class Router:
    """A* on the schematic grid, with the parts as obstacles."""
    parts: list[Part]
    margin: float = GRID
    turn_cost: float = 0.6
    cross_cost: float = 6.0

    blocked: set[tuple[int, int]] = field(default_factory=set)
    used: dict[tuple[int, int], set[str]] = field(default_factory=dict)
    # cell -> net -> the orientations that net occupies there, "H" and/or "V".
    # A cell where a net turns holds both, and no other net may cross there.
    dirs: dict[tuple[int, int], dict[str, set[str]]] = field(default_factory=dict)
    pin_cells: set[tuple[int, int]] = field(default_factory=set)
    segs: list[tuple[tuple[float, float], tuple[float, float], str]] = field(
        default_factory=list)

    def key(self, x: float, y: float) -> tuple[int, int]:
        return (int(round(x / GRID)), int(round(y / GRID)))

    def pos(self, k: tuple[int, int]) -> tuple[float, float]:
        return (k[0] * GRID, k[1] * GRID)

    def build_obstacles(self) -> None:
        self.blocked.clear()
        for p in self.parts:
            x0, y0, x1, y1 = p.box()
            kx0, ky0 = self.key(x0 - self.margin, y0 - self.margin)
            kx1, ky1 = self.key(x1 + self.margin, y1 + self.margin)
            for kx in range(kx0, kx1 + 1):
                for ky in range(ky0, ky1 + 1):
                    self.blocked.add((kx, ky))
        # a pin tip must stay reachable even though it sits next to its body,
        # and no other net may pass over it: a wire touching a pin tip
        # connects to that pin, whatever the drawing looks like
        self.pin_cells.clear()
        for p in self.parts:
            for xy in p.pin_sheet().values():
                self.blocked.discard(self.key(*xy))
                self.pin_cells.add(self.key(*xy))

    def route(self, net: str, a: tuple[float, float],
              targets: set[tuple[int, int]], folga: int = 90) -> list[tuple[int, int]] | None:
        """Shortest path from a to any cell of targets, as a list of cells."""
        start = self.key(*a)
        if start in targets:
            return [start]
        openq: list[tuple[float, float, tuple[int, int], tuple[int, int] | None]] = []
        tx = sum(t[0] for t in targets) / len(targets)
        ty = sum(t[1] for t in targets) / len(targets)
        # Search inside a box around the start and the targets, widened by a
        # detour allowance. Without it A* wanders the whole sheet on the nets
        # that cannot be reached directly, and the run takes tens of minutes.
        bx0 = min([start[0]] + [t[0] for t in targets]) - folga
        bx1 = max([start[0]] + [t[0] for t in targets]) + folga
        by0 = min([start[1]] + [t[1] for t in targets]) - folga
        by1 = max([start[1]] + [t[1] for t in targets]) + folga
        heapq.heappush(openq, (abs(start[0] - tx) + abs(start[1] - ty), 0.0, start, None))
        came: dict[tuple[int, int], tuple[int, int] | None] = {}
        seen: dict[tuple[int, int], float] = {start: 0.0}
        while openq:
            _f, g, cur, prev = heapq.heappop(openq)
            if cur in came:
                continue
            came[cur] = prev
            if cur in targets:
                path = [cur]
                while came[path[-1]] is not None:
                    path.append(came[path[-1]])
                path.reverse()
                return path
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                nxt = (cur[0] + dx, cur[1] + dy)
                if nxt in came:
                    continue
                if not (bx0 <= nxt[0] <= bx1 and by0 <= nxt[1] <= by1):
                    continue
                if nxt in self.blocked and nxt not in targets:
                    continue
                if (nxt in self.pin_cells and nxt not in targets
                        and net not in self.used.get(nxt, set())):
                    continue
                step = 1.0
                owners = self.used.get(nxt, set())
                if owners and net not in owners:
                    # only a clean right-angle crossing of another net is
                    # allowed: never a shared run, never their corner, and
                    # never an end of ours on their wire
                    mine = "H" if dy == 0 else "V"
                    alheio = set()
                    for other, ds in self.dirs.get(nxt, {}).items():
                        if other != net:
                            alheio |= ds
                    if mine in alheio or len(alheio) > 1 or nxt in targets:
                        continue
                    step += self.cross_cost
                if prev is not None and (cur[0] - prev[0], cur[1] - prev[1]) != (dx, dy):
                    step += self.turn_cost
                ng = g + step
                if ng < seen.get(nxt, float("inf")):
                    seen[nxt] = ng
                    h = abs(nxt[0] - tx) + abs(nxt[1] - ty)
                    heapq.heappush(openq, (ng + h, ng, nxt, cur))
        return None

    def add_path(self, net: str, path: list[tuple[int, int]]) -> None:
        for i, k in enumerate(path):
            self.used.setdefault(k, set()).add(net)
            d = self.dirs.setdefault(k, {}).setdefault(net, set())
            for j in (i - 1, i + 1):
                if 0 <= j < len(path):
                    d.add("H" if path[j][1] == k[1] else "V")
            if len(path) == 1:
                d |= {"H", "V"}
        # collapse the cell path into straight segments
        if len(path) < 2:
            return
        run = [path[0]]
        d = (path[1][0] - path[0][0], path[1][1] - path[0][1])
        for i in range(1, len(path)):
            nd = (path[i][0] - path[i - 1][0], path[i][1] - path[i - 1][1])
            if nd != d:
                self.segs.append((self.pos(run[0]), self.pos(path[i - 1]), net))
                run = [path[i - 1]]
                d = nd
        self.segs.append((self.pos(run[0]), self.pos(path[-1]), net))


@dataclass
class PowerPort:
    """A supply or ground symbol: the professional way to carry a rail.

    KiCad joins every power symbol of the same value into one net across the
    whole design, which is what keeps a ground with eighty pins from being
    drawn as eighty lines to one rail.
    """
    net: str
    x: float
    y: float
    ground: bool = False   # graphic points down and the pin faces up
    ref: str = "#PWR"

    @property
    def sym_name(self) -> str:
        return f"power:{self.net}"

    def pin_sheet(self) -> tuple[float, float]:
        return (self.x, self.y)

    def lib_symbol(self) -> str:
        if self.ground:
            # the classic three bars, drawn below the connection point
            art = ('\t\t\t\t(polyline (pts (xy 0 0) (xy 0 -1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.905 -1.27) (xy 1.905 -1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.143 -1.905) (xy 1.143 -1.905))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -0.508 -2.54) (xy 0.508 -2.54))'
                   ' (stroke (width 0) (type default)) (fill (type none)))')
            pin_ang, val_y, val_just = 90, -3.81, "top"
        else:
            art = ('\t\t\t\t(polyline (pts (xy 0 0) (xy 0 1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))\n'
                   '\t\t\t\t(polyline (pts (xy -1.27 1.27) (xy 0 2.54) (xy 1.27 1.27))'
                   ' (stroke (width 0) (type default)) (fill (type none)))')
            pin_ang, val_y, val_just = 270, 3.81, "bottom"
        return (f'\t\t(symbol "{self.sym_name}"\n\t\t\t(power)\n'
                '\t\t\t(pin_names (offset 0))\n'
                '\t\t\t(exclude_from_sim no)\n\t\t\t(in_bom no)\n\t\t\t(on_board yes)\n'
                f'\t\t\t(property "Reference" "{self.ref}"\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                f'\t\t\t(property "Value" "{esc(self.net)}"\n\t\t\t\t(at 0 {val_y} 0)\n'
                f'\t\t\t\t(effects (font (size 1.27 1.27)) (justify {val_just}))\n\t\t\t)\n'
                '\t\t\t(property "Footprint" ""\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                '\t\t\t(property "Datasheet" ""\n\t\t\t\t(at 0 0 0)\n'
                '\t\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t\t)\n'
                f'\t\t\t(symbol "{self.net}_0_1"\n{art}\n\t\t\t)\n'
                f'\t\t\t(symbol "{self.net}_1_1"\n'
                f'\t\t\t\t(pin power_in line\n\t\t\t\t\t(at 0 0 {pin_ang})\n'
                '\t\t\t\t\t(length 0)\n'
                f'\t\t\t\t\t(name "{esc(self.net)}" (effects (font (size 1.27 1.27))))\n'
                '\t\t\t\t\t(number "1" (effects (font (size 1.27 1.27))))\n'
                '\t\t\t\t)\n\t\t\t)\n\t\t)')

    def instance(self, project: str, path: str) -> str:
        u = uid("pwr", self.net, self.x, self.y)
        val_y = self.y + (3.81 if self.ground else -3.81)
        return (f'\t(symbol\n\t\t(lib_id "{self.sym_name}")\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} 0)\n\t\t(unit 1)\n'
                '\t\t(exclude_from_sim no)\n\t\t(in_bom no)\n\t\t(on_board yes)\n\t\t(dnp no)\n'
                f'\t\t(uuid "{u}")\n'
                f'\t\t(property "Reference" "{self.ref}"\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(property "Value" "{esc(self.net)}"\n'
                f'\t\t\t(at {self.x:.3f} {val_y:.3f} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)))\n\t\t)\n'
                f'\t\t(property "Footprint" ""\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(property "Datasheet" ""\n\t\t\t(at {self.x} {self.y} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (hide yes))\n\t\t)\n'
                f'\t\t(pin "1" (uuid "{uid("pwrpin", self.net, self.x, self.y)}"))\n'
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "{path}"\n\t\t\t\t\t(reference "{self.ref}")\n'
                '\t\t\t\t\t(unit 1)\n\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


@dataclass
class HierLabel:
    """A signal leaving the sheet, and the sheet pin it answers to on the root."""
    name: str
    x: float
    y: float
    angle: int = 0     # 0 text to the right, 180 text to the left
    shape: str = "bidirectional"

    def render(self) -> str:
        just = "left" if self.angle == 0 else "right"
        return (f'\t(hierarchical_label "{esc(self.name)}"\n\t\t(shape {self.shape})\n'
                f'\t\t(at {self.x:.3f} {self.y:.3f} {self.angle})\n'
                f'\t\t(effects (font (size 1.27 1.27)) (justify {just}))\n'
                f'\t\t(uuid "{uid("hl", self.name, self.x, self.y)}")\n\t)')


@dataclass
class SheetSymbol:
    """One block on the root sheet."""
    name: str
    filename: str
    x: float
    y: float
    w: float
    h: float
    page: str
    pins: list[tuple[str, float, int, str]] = field(default_factory=list)  # name, y, ang, shape

    @property
    def uuid(self) -> str:
        return uid("sheetsym", self.name)

    def pin_pos(self, name: str) -> tuple[float, float]:
        for n, py, ang, _s in self.pins:
            if n == name:
                return (self.x if ang == 180 else self.x + self.w, py)
        raise KeyError(name)

    def render(self, project: str, root_uuid: str) -> str:
        pins = "\n".join(
            f'\t\t(pin "{esc(n)}" {shape}\n'
            f'\t\t\t(at {self.x if ang == 180 else self.x + self.w:.3f} {py:.3f} {ang})\n'
            f'\t\t\t(effects (font (size 1.27 1.27)) '
            f'(justify {"left" if ang == 180 else "right"}))\n'
            f'\t\t\t(uuid "{uid("sp", self.name, n)}")\n\t\t)'
            for n, py, ang, shape in self.pins)
        return (f'\t(sheet\n\t\t(at {self.x:.3f} {self.y:.3f})\n'
                f'\t\t(size {self.w:.3f} {self.h:.3f})\n'
                '\t\t(stroke (width 0.2) (type solid))\n'
                '\t\t(fill (color 0 0 0 0.0000))\n'
                f'\t\t(uuid "{self.uuid}")\n'
                f'\t\t(property "Sheetname" "{esc(self.name)}"\n'
                f'\t\t\t(at {self.x:.3f} {self.y - 1.0:.3f} 0)\n'
                '\t\t\t(effects (font (size 2.0 2.0) (bold yes)) (justify left bottom))\n\t\t)\n'
                f'\t\t(property "Sheetfile" "{esc(self.filename)}"\n'
                f'\t\t\t(at {self.x:.3f} {self.y + self.h + 1.0:.3f} 0)\n'
                '\t\t\t(effects (font (size 1.27 1.27)) (justify left top))\n\t\t)\n'
                + (pins + "\n" if pins else "") +
                f'\t\t(instances\n\t\t\t(project "{project}"\n'
                f'\t\t\t\t(path "/{root_uuid}"\n\t\t\t\t\t(page "{self.page}")\n'
                '\t\t\t\t)\n\t\t\t)\n\t\t)\n\t)')


class Schematic:
    def __init__(self, project: str, paper: tuple[float, float], *,
                 name: str = "", root_uuid: str | None = None,
                 title: str = "", rev: str = "", date: str = "") -> None:
        self.project = project
        self.paper = paper
        self.uuid = uid("sheet", project, name)
        self.root_uuid = root_uuid          # None means this file IS the root
        self.sheet_symbol_uuid: str | None = None
        self.title, self.rev, self.date = title, rev, date
        self.parts: list[Part] = []
        self.powers: list[PowerPort] = []
        self.labels: list[HierLabel] = []
        self.sheets: list[SheetSymbol] = []
        self.wires: list[tuple[tuple[float, float], tuple[float, float]]] = []
        self.junctions: list[tuple[float, float]] = []
        self.texts: list[tuple[float, float, str, float]] = []

    @property
    def inst_path(self) -> str:
        if self.root_uuid is None:
            return f"/{self.uuid}"
        return f"/{self.root_uuid}/{self.sheet_symbol_uuid}"

    def text(self, x: float, y: float, s: str, size: float = 2.0) -> None:
        self.texts.append((x, y, s, size))

    def render(self) -> str:
        libs_parts = [p.lib_symbol() for p in self.parts]
        vistos: set[str] = set()
        for pw in self.powers:
            if pw.sym_name not in vistos:
                vistos.add(pw.sym_name)
                libs_parts.append(pw.lib_symbol())
        libs = "\n".join(libs_parts)
        insts = "\n".join(
            [p.instance(self.project, self.inst_path) for p in self.parts]
            + [pw.instance(self.project, self.inst_path) for pw in self.powers])
        wires = "\n".join(
            f'\t(wire\n\t\t(pts\n\t\t\t(xy {a[0]:.3f} {a[1]:.3f}) (xy {b[0]:.3f} {b[1]:.3f})\n\t\t)'
            f'\n\t\t(stroke (width 0) (type default))\n\t\t(uuid "{uid("w", a, b)}")\n\t)'
            for a, b in self.wires)
        juncs = "\n".join(
            f'\t(junction\n\t\t(at {x:.3f} {y:.3f})\n\t\t(diameter 0)\n\t\t(color 0 0 0 0)'
            f'\n\t\t(uuid "{uid("j", x, y)}")\n\t)' for x, y in self.junctions)
        texts = "\n".join(
            f'\t(text "{esc(s)}"\n\t\t(exclude_from_sim no)\n\t\t(at {x:.3f} {y:.3f} 0)'
            f'\n\t\t(effects (font (size {sz} {sz})) (justify left bottom))'
            f'\n\t\t(uuid "{uid("tx", x, y, s)}")\n\t)' for x, y, s, sz in self.texts)
        rotulos = "\n".join(lb.render() for lb in self.labels)
        folhas = "\n".join(sh.render(self.project, self.uuid) for sh in self.sheets)
        bloco = ""
        if self.title:
            bloco = (f'\t(title_block\n\t\t(title "{esc(self.title)}")\n'
                     + (f'\t\t(date "{esc(self.date)}")\n' if self.date else "")
                     + (f'\t\t(rev "{esc(self.rev)}")\n' if self.rev else "")
                     + '\t\t(company "GNSS Bike Computer")\n\t)\n')
        parts = [f'(kicad_sch\n\t(version 20231120)\n\t(generator "eeschema")\n'
                 f'\t(generator_version "8.0")\n\t(uuid "{self.uuid}")\n'
                 + (f'\t(paper "{self.paper}")\n' if isinstance(self.paper, str)
                    else f'\t(paper "User" {self.paper[0]} {self.paper[1]})\n')
                 + bloco +
                 f'\t(lib_symbols\n{libs}\n\t)']
        for chunk in (folhas, wires, juncs, rotulos, texts, insts):
            if chunk:
                parts.append(chunk)
        if self.root_uuid is None:
            paginas = '\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n'
            for sh in self.sheets:
                paginas += (f'\t\t(path "/{sh.uuid}"\n\t\t\t(page "{sh.page}")\n\t\t)\n')
            parts.append(f'\t(sheet_instances\n{paginas}\t)\n)')
        else:
            parts.append('\t(sheet_instances\n\t\t(path "/"\n\t\t\t(page "1")\n\t\t)\n\t)\n)')
        return "\n".join(parts) + "\n"
