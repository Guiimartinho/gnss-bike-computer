#!/usr/bin/env python3
"""Extra review material: where each part sits on the board, and what the
generator knows about each part.

Usage: extras.py <board.kicad_pcb> <cad dir> <output dir>

Writes placement.csv (reference, side, x, y, rotation, footprint, pad count,
read from the board file) and parts.csv (reference, value, footprint, LCSC
number, whether the pin numbering was read off the datasheet, the note of the
part), read from parts.py of hardware_gnssbike/cad. The board is the one the
chain placed; it may not be routed.

Adapted from the design review tools of the 35.esp32-s3_power_meter project
(MIT, same author): the reference map of that project is not needed here,
because the generator and the drawing use the same reference designators.
"""

from __future__ import annotations

import csv
import pathlib
import re
import sys

BOARD = pathlib.Path(sys.argv[1])
CAD = pathlib.Path(sys.argv[2]).resolve()
OUT = pathlib.Path(sys.argv[3])
OUT.mkdir(parents=True, exist_ok=True)

# ---------------------------------------------------------------- placement
text = BOARD.read_text(encoding="utf-8")
blocks = re.split(r"\n\t\(footprint ", text)[1:]
rows = []
for block in blocks:
    name = re.match(r'"([^"]+)"', block)
    layer = re.search(r'\(layer "([^"]+)"\)', block)
    at = re.search(r"\(at ([-\d.]+) ([-\d.]+)(?: ([-\d.]+))?\)", block)
    ref = re.search(r'\(property "Reference" "([^"]+)"', block)
    if not (name and layer and at and ref):
        continue
    rows.append([ref.group(1), "top" if layer.group(1).startswith("F.") else "bottom",
                 at.group(1), at.group(2), at.group(3) or "0", name.group(1), len(re.findall(r"\n\t\t\(pad ", block))])


def natural(ref: str):
    m = re.match(r"([A-Za-z]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


rows.sort(key=lambda r: natural(r[0]))
with (OUT / "placement.csv").open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["ref", "side", "x_mm", "y_mm", "rotation_deg", "footprint", "pads"])
    w.writerows(rows)

# ------------------------------------------------------------------ parts.py
sys.path.insert(0, str(CAD))
import parts as P  # noqa: E402  (the generator's own list of parts)

with (OUT / "parts.csv").open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["ref", "value", "footprint", "lcsc", "confirmed", "note"])
    for ref in sorted(P.PARTS, key=natural):
        p = P.PARTS[ref]
        w.writerow([ref, p.value, p.footprint, p.lcsc, "no" if ref in P.UNCONFIRMED else "yes", " ".join(p.note.split())])

print(f"{len(rows)} footprints on the board, {len(P.PARTS)} parts in parts.py, "
      f"{len(P.UNCONFIRMED)} with the pin numbering not confirmed: {', '.join(sorted(P.UNCONFIRMED, key=natural))}")
