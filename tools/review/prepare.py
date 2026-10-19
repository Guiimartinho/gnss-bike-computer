#!/usr/bin/env python3
"""Build the material of a schematic review from the KiCad project.

Usage: python tools/review/prepare.py [<review dir>]
       (default: .claude/reviews/<today>-esquematico, which git ignores)

Nothing under the repository changes: the KiCad project is copied into the
review folder and every kicad-cli run works on the copy (kicad-cli writes a
.kicad_prl beside the project it opens). Steps:

  1. copy the project (sheets, project file, board, rules) to kicad/project/
     and the generator sources (parts.py, nets.py, footprints.py, blocos.py,
     sheets.py) to kicad/generator/, where the designer's intent and the
     datasheet pages it cites can be read;
  2. export the XML netlist, the PDF of the schematic and one PNG per page;
  3. run the ERC with every severity and the DRC with schematic parity, and
     summarize both;
  4. write placement.csv and parts.csv (extras.py) and the digests of the
     netlist (digest.py) under out/;
  5. copy the datasheets the repository keeps to ds/_repo/;
  6. rebuild the register of findings (known.py) from out/findings/.

Run it again after the schematic changes: everything is rebuilt except the
findings, cards, reports and verdicts that reviewers wrote.
"""

from __future__ import annotations

import collections
import datetime
import json
import os
import pathlib
import shutil
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parents[1]
CAD = REPO / "hardware_gnssbike" / "cad"
DATASHEETS = REPO / "hardware_gnssbike" / "datasheets"
KICAD_CLI = pathlib.Path(os.environ.get("KICAD_CLI", r"D:\KiCAD\bin\kicad-cli.exe"))
GENERATOR = ("parts.py", "nets.py", "footprints.py", "blocos.py", "sheets.py")

review = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else \
    REPO / ".claude" / "reviews" / f"{datetime.date.today().isoformat()}-esquematico"
review = review.resolve()
kicad = review / "kicad"
project = kicad / "project"
out = review / "out"
for d in (project, kicad / "generator", kicad / "pages", out / "findings", review / "ds" / "_repo"):
    d.mkdir(parents=True, exist_ok=True)


def run(cmd: list[str], what: str) -> subprocess.CompletedProcess:
    r = subprocess.run([str(c) for c in cmd], capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(f"FAILED: {what} (exit {r.returncode})\n{r.stdout[-1500:]}\n{r.stderr[-1500:]}")
        sys.exit(1)
    return r


# 1. copies -----------------------------------------------------------------
copied = 0
for f in list(CAD.glob("*.kicad_sch")) + [CAD / "gnssbike.kicad_pro", CAD / "gnssbike.kicad_pcb", CAD / "gnssbike.kicad_dru"]:
    shutil.copy2(f, project / f.name)
    copied += 1
for name in GENERATOR:
    shutil.copy2(CAD / name, kicad / "generator" / name)
root_sch = project / "gnssbike.kicad_sch"
board = project / "gnssbike.kicad_pcb"
print(f"project: {copied} files copied to {project}")

# 2. exports ----------------------------------------------------------------
run([KICAD_CLI, "sch", "export", "netlist", "--format", "kicadxml", "-o", kicad / "netlist.xml", root_sch], "netlist")
run([KICAD_CLI, "sch", "export", "pdf", "-o", kicad / "schematic.pdf", root_sch], "schematic PDF")
import fitz  # noqa: E402  (PyMuPDF)

for old in (kicad / "pages").glob("page-*.png"):
    old.unlink()
doc = fitz.open(kicad / "schematic.pdf")
for i, page in enumerate(doc, start=1):
    page.get_pixmap(dpi=110).save(kicad / "pages" / f"page-{i:02d}.png")
print(f"schematic: {doc.page_count} pages rendered to kicad/pages/")

# 3. ERC and DRC ------------------------------------------------------------
run([KICAD_CLI, "sch", "erc", "--severity-all", "--format", "json", "-o", kicad / "erc.json", root_sch], "ERC")
run([KICAD_CLI, "sch", "erc", "--severity-all", "--format", "report", "-o", kicad / "erc.rpt", root_sch], "ERC report")
erc = json.loads((kicad / "erc.json").read_text(encoding="utf-8"))
count = collections.Counter()
for sheet in erc.get("sheets", []):
    for v in sheet.get("violations", []):
        count[(v.get("severity"), v.get("type"))] += 1
print("ERC (every severity):", sum(count.values()), "violations")
for (sev, kind), n in sorted(count.items()):
    print(f"  {sev:8} {kind:32} {n}")

run([KICAD_CLI, "pcb", "drc", "--severity-all", "--schematic-parity", "--format", "json", "-o", kicad / "drc.json", board], "DRC")
drc = json.loads((kicad / "drc.json").read_text(encoding="utf-8"))
dcount = collections.Counter(v.get("severity") for v in drc.get("violations", []))
print(f"DRC: {dict(dcount)} violations, {len(drc.get('unconnected_items', []))} unconnected items, "
      f"{len(drc.get('schematic_parity', []))} schematic parity issues")

# 4. digests ----------------------------------------------------------------
py = [sys.executable, "-I"]
print(run(py + [HERE / "extras.py", board, CAD, out], "extras").stdout.strip())
print(run(py + [HERE / "digest.py", kicad / "netlist.xml", project, out, out / "parts.csv"], "digest").stdout.strip())

# 5. datasheets kept in the repository ----------------------------------------
n = 0
for f in DATASHEETS.iterdir():
    if f.is_file() and f.name != "README.md":
        shutil.copy2(f, review / "ds" / "_repo" / f.name)
        n += 1
print(f"datasheets: {n} files copied to ds/_repo/ (read only; reviewers download the rest to ds/<key>/)")

# 6. register of findings -------------------------------------------------------
print(run(py + [HERE / "known.py", review], "known").stdout.strip())
print(f"review folder: {review}")
