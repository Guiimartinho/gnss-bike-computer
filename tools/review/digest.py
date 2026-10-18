#!/usr/bin/env python3
"""Turn the KiCad XML netlist of the board into review material.

Usage: digest.py <netlist.xml> <kicad project dir> <output dir> [<parts.csv>]

Everything written here is derived by program from the files KiCad exported,
so a reviewer can rely on it for "what is connected to what". The optional
parts.csv (written by extras.py from hardware_gnssbike/cad/parts.py) adds the
LCSC number and whether the pin numbering was read off the datasheet.

Writes components.csv, nets.md, lint.md, bom.md, index.json, one file per
sheet in sheets/ and one file per part type in parts/.

Adapted from the design review tools of the 35.esp32-s3_power_meter project
(MIT, same author). Changes: the symbols of this project are embedded one per
instance (lib_id "gnssbike:<ref>"), so part types are grouped by value and
footprint, not by symbol; lint.md is computed here.
"""

from __future__ import annotations

import collections
import csv
import json
import pathlib
import re
import sys
import xml.etree.ElementTree as ET

SRC = pathlib.Path(sys.argv[1])
PROJECT = pathlib.Path(sys.argv[2])
OUT = pathlib.Path(sys.argv[3])
PARTS_CSV = pathlib.Path(sys.argv[4]) if len(sys.argv) > 4 else None

BIG_NET = 9  # nets with more nodes than this are summarized in pin tables
DRIVERS = {"output", "power_out", "bidirectional", "tri_state", "passive", "open_collector", "open_emitter"}


def natural(ref: str):
    m = re.match(r"([A-Za-z#]+)(\d+)", ref)
    return (m.group(1), int(m.group(2))) if m else (ref, 0)


def slug(text: str) -> str:
    return re.sub(r"[^A-Za-z0-9]+", "-", text).strip("-").lower()


def prefix_of(ref: str) -> str:
    m = re.match(r"[A-Za-z]+", ref)
    return m.group(0) if m else ref


root = ET.parse(SRC).getroot()

libpins: dict[tuple[str, str], dict[str, tuple[str, str]]] = {}
for lp in root.find("libparts"):
    pins: dict[str, tuple[str, str]] = {}
    block = lp.find("pins")
    if block is not None:
        for pin in block:
            pins[pin.get("num")] = (pin.get("name") or "", pin.get("type") or "")
    libpins[(lp.get("lib"), lp.get("part"))] = pins

extra: dict[str, dict] = {}
if PARTS_CSV and PARTS_CSV.exists():
    with PARTS_CSV.open(encoding="utf-8") as fh:
        for row in csv.DictReader(fh):
            extra[row["ref"]] = row

comps: dict[str, dict] = {}
for c in root.find("components"):
    ls = c.find("libsource")
    fields_el = c.find("fields")
    fields = {f.get("name"): (f.text or "") for f in fields_el} if fields_el is not None else {}
    props = {p.get("name"): (p.get("value") or "") for p in c.findall("property")}
    ref = c.get("ref")
    ex = extra.get(ref, {})
    comps[ref] = {
        "ref": ref,
        "value": c.findtext("value") or "",
        "footprint": c.findtext("footprint") or "",
        "lib": ls.get("lib"),
        "part": ls.get("part"),
        "desc": fields.get("Description") or ls.get("description") or "",
        "sheet": c.find("sheetpath").get("names").strip("/") or "(root)",
        "sheetfile": props.get("Sheetfile", ""),
        "lcsc": ex.get("lcsc", ""),
        "confirmed": ex.get("confirmed", ""),
        "dnp": "dnp" in props or "exclude_from_bom" in props,
    }

nets: dict[str, list[tuple[str, str, str, str]]] = {}
pin_net: dict[tuple[str, str], str] = {}
for n in root.find("nets"):
    nodes = [(x.get("ref"), x.get("pin"), x.get("pinfunction") or "", x.get("pintype") or "") for x in n]
    nodes.sort(key=lambda t: (natural(t[0]), t[1]))
    nets[n.get("name")] = nodes
    for ref, pin, _f, _t in nodes:
        pin_net[(ref, pin)] = n.get("name")


def comp_pins(c: dict) -> list[tuple[str, str, str, str]]:
    """(pin number, pin name, pin type, net) for every pin of the symbol."""
    rows = []
    known = libpins.get((c["lib"], c["part"]), {})
    numbers = set(known) | {p for (r, p) in pin_net if r == c["ref"]}

    def order(num: str):
        m = re.match(r"([A-Za-z]*)(\d+)", num)
        return (m.group(1), int(m.group(2))) if m else (num, 0)

    for num in sorted(numbers, key=order):
        name, kind = known.get(num, ("", ""))
        rows.append((num, name, kind, pin_net.get((c["ref"], num), "** NOT IN ANY NET **")))
    return rows


def describe_node(ref: str, pin: str, func: str) -> str:
    c = comps.get(ref)
    label = f"{ref}.{pin}"
    name = func.rsplit("_", 1)[0] if func else ""
    if name and name not in ("~", pin):
        label += f"({name})"
    if c and prefix_of(ref) in ("R", "C", "L", "D", "FB", "JP", "RT") and c["value"]:
        label += f"[{c['value']}]"
    return label


def net_summary(net: str, skip: str | None = None) -> str:
    nodes = nets.get(net, [])
    others = [t for t in nodes if t[0] != skip]
    if len(nodes) > BIG_NET:
        ics = sorted({t[0] for t in others if not t[0].startswith(("C", "R", "TP", "#"))}, key=natural)
        return f"({len(nodes)} nodes; non-passive parts: {', '.join(ics[:14])}{' ...' if len(ics) > 14 else ''})"
    return ", ".join(describe_node(r, p, f) for r, p, f, _t in others) or "(nothing else)"


def read_notes(sheetfile: str) -> list[str]:
    path = PROJECT / sheetfile
    if not path.exists():
        return []
    text = path.read_text(encoding="utf-8")
    found = re.findall(r'\(text\s+"((?:[^"\\]|\\.)*)"', text)
    return [f.replace("\\n", "\n").replace('\\"', '"') for f in found]


def tag(c: dict) -> str:
    """Short marks for the part tables: LCSC number and unconfirmed pinout."""
    marks = []
    if c["lcsc"]:
        marks.append(c["lcsc"])
    if c["confirmed"] == "no":
        marks.append("PINOUT NOT CONFIRMED")
    return ", ".join(marks)


OUT.mkdir(parents=True, exist_ok=True)
(OUT / "sheets").mkdir(exist_ok=True)
(OUT / "parts").mkdir(exist_ok=True)

# ---------------------------------------------------------------- components
with (OUT / "components.csv").open("w", newline="", encoding="utf-8") as fh:
    w = csv.writer(fh)
    w.writerow(["ref", "value", "footprint", "sheet", "lcsc", "pinout_confirmed", "description", "pins"])
    for ref in sorted(comps, key=natural):
        c = comps[ref]
        pins = "; ".join(f"{n}={net}" for n, _nm, _k, net in comp_pins(c))
        w.writerow([ref, c["value"], c["footprint"], c["sheet"], c["lcsc"], c["confirmed"], c["desc"], pins])

# ------------------------------------------------------------------- nets.md
lines = ["# Nets of the board (from the KiCad netlist)", "",
         f"{len(nets)} nets, {len(comps)} components. Node format: REF.pin(pin name)[value] <pin type>.", ""]
for name in sorted(nets):
    nodes = nets[name]
    sheets = sorted({comps[r]["sheet"] for r, *_ in nodes if r in comps})
    lines.append(f"## `{name}` ({len(nodes)} nodes; sheets: {', '.join(sheets)})")
    lines.append("")
    lines.append(", ".join(describe_node(r, p, f) + (f" <{t}>" if t and t != "passive" else "") for r, p, f, t in nodes))
    lines.append("")
(OUT / "nets.md").write_text("\n".join(lines), encoding="utf-8")

# ------------------------------------------------------------------- lint.md
single = [n for n, nodes in nets.items() if len(nodes) == 1 and not n.startswith("unconnected-")]
unconnected: dict[str, list[str]] = collections.defaultdict(list)
for n, nodes in nets.items():
    for r, p, f, t in nodes:
        if n.startswith("unconnected-") or "no_connect" in t:
            unconnected[r].append(f"{p}({f or '-'}, {t})")
undriven = []
for n, nodes in nets.items():
    types = {t.split("+")[0] for _r, _p, _f, t in nodes}
    if types & {"input", "power_in"} and not types & DRIVERS:
        undriven.append(n)
L = ["# Netlist lint (computed from the KiCad netlist)", "",
     f"## Nets with a single node that are not marked unconnected ({len(single)})", ""]
L += [f"- `{n}`: " + ", ".join(describe_node(r, p, f) for r, p, f, _t in nets[n]) for n in sorted(single)]
L += ["", f"## Pins deliberately left unconnected ({sum(len(v) for v in unconnected.values())})", ""]
L += [f"- {r} [{comps[r]['value']}]: " + ", ".join(v) for r, v in sorted(unconnected.items(), key=lambda kv: natural(kv[0]))]
L += ["", f"## Nets with an input or a power input and no pin type that can drive them ({len(undriven)})", ""]
L += [f"- `{n}`: " + ", ".join(describe_node(r, p, f) + f" <{t}>" for r, p, f, t in nets[n]) for n in sorted(undriven)]
L += ["", "## Supply pins of every IC and module, with the capacitors on each net", ""]
for ref in sorted(comps, key=natural):
    if prefix_of(ref) not in ("U", "Q", "DS", "LS"):
        continue
    c = comps[ref]
    rows = [(num, name, kind, net) for num, name, kind, net in comp_pins(c) if kind.split("+")[0] in ("power_in", "power_out")]
    if not rows:
        continue
    L.append(f"### {ref} [{c['value']}]")
    for num, name, kind, net in rows:
        caps = [f"{r}[{comps[r]['value']}]" for r, _p, _f, _t in nets.get(net, []) if prefix_of(r) == "C"]
        cap_text = ", ".join(caps) if len(caps) <= 12 else f"{len(caps)} capacitors"
        L.append(f"- pin {num} {name} <{kind}> on `{net}`: capacitors {cap_text or 'none'}")
    L.append("")
(OUT / "lint.md").write_text("\n".join(L) + "\n", encoding="utf-8")

# ---------------------------------------------------------------- per sheet
by_sheet: dict[str, list[dict]] = collections.defaultdict(list)
for c in comps.values():
    by_sheet[c["sheet"]].append(c)

sheet_index = []
for sheet, members in sorted(by_sheet.items()):
    members.sort(key=lambda c: natural(c["ref"]))
    sheetfile = members[0]["sheetfile"]
    L = [f"# Sheet: {sheet} ({sheetfile})", "",
         "Derived by program from the netlist KiCad exported. Reference designators are the ones on the drawing.", "",
         f"## Parts ({len(members)})", "",
         "| Ref | Value | Footprint | LCSC / marks | Description on the symbol |", "| --- | --- | --- | --- | --- |"]
    for c in members:
        L.append(f"| {c['ref']} | {c['value']} | {c['footprint']} | {tag(c)} | {c['desc']} |")
    L += ["", "## Connections of two-pin parts", "", "| Ref | Value | Pin 1 net | Pin 2 net |", "| --- | --- | --- | --- |"]
    multi = []
    for c in members:
        rows = comp_pins(c)
        if len(rows) == 2:
            L.append(f"| {c['ref']} | {c['value']} | `{rows[0][3]}` | `{rows[1][3]}` |")
        else:
            multi.append((c, rows))
    L += ["", "## Connections of the other parts, pin by pin", ""]
    for c, rows in multi:
        L += [f"### {c['ref']}: {c['value']} ({c['footprint']})", "",
              "| Pin | Name | Type | Net | Also on this net |", "| --- | --- | --- | --- | --- |"]
        for num, name, kind, net in rows:
            L.append(f"| {num} | {name} | {kind} | `{net}` | {net_summary(net, c['ref'])} |")
        L.append("")
    touched = sorted({net for c in members for _n, _nm, _k, net in comp_pins(c)})
    L += ["## Every net that touches this sheet", ""]
    for net in touched:
        nodes = nets.get(net, [])
        if len(nodes) > 40:
            L.append(f"- `{net}`: {len(nodes)} nodes (see nets.md)")
            continue
        parts = []
        for r, p, f, _t in nodes:
            where = "" if comps[r]["sheet"] == sheet else f" @{comps[r]['sheet']}"
            parts.append(describe_node(r, p, f) + where)
        L.append(f"- `{net}`: " + ", ".join(parts))
    notes = read_notes(sheetfile)
    L += ["", "## Text notes drawn on the sheet", ""]
    L += [f"- {n}" for n in notes] or ["(none)"]
    name = slug(sheet)
    (OUT / "sheets" / f"{name}.md").write_text("\n".join(L) + "\n", encoding="utf-8")
    sheet_index.append({"sheet": sheet, "file": f"sheets/{name}.md", "sheetfile": sheetfile, "parts": len(members),
                        "ics": [c["ref"] + "=" + c["value"] for c in members if prefix_of(c["ref"]) in ("U", "Q")]})

# ------------------------------------------------------------ per part type
groups: dict[tuple, list[dict]] = collections.defaultdict(list)
for c in comps.values():
    groups[(prefix_of(c["ref"]), c["value"], c["footprint"])].append(c)

part_index = []
bom = ["# Bill of materials as the schematic has it", "",
       "| Qty | Value | Footprint | LCSC | References | Sheets |", "| --- | --- | --- | --- | --- | --- |"]
for (prefix, value, footprint), members in sorted(groups.items(), key=lambda kv: (kv[0][0], natural(kv[1][0]["ref"]))):
    members.sort(key=lambda c: natural(c["ref"]))
    refs = [c["ref"] for c in members]
    sheets = sorted({c["sheet"] for c in members})
    lcsc = sorted({c["lcsc"] for c in members if c["lcsc"]})
    bom.append(f"| {len(members)} | {value} | {footprint} | {', '.join(lcsc)} | {' '.join(refs)} | {', '.join(sheets)} |")
    name = slug(f"{prefix}-{value}-{footprint.split(':')[-1]}")[:70]
    unconfirmed = [c["ref"] for c in members if c["confirmed"] == "no"]
    L = [f"# Part type: {value} (footprint {footprint})", "",
         f"Instances: {', '.join(refs)}. LCSC: {', '.join(lcsc) or '(none)'}."
         + (f" Pin numbering NOT confirmed against the datasheet for {', '.join(unconfirmed)}." if unconfirmed else ""), ""]
    if members[0]["desc"]:
        L += [f"Description on the symbol: {members[0]['desc']}", ""]
    for c in members:
        L += [f"## {c['ref']} on sheet {c['sheet']}", "",
              "| Pin | Name | Type | Net | Also on this net |", "| --- | --- | --- | --- | --- |"]
        for num, pname, kind, net in comp_pins(c):
            L.append(f"| {num} | {pname} | {kind} | `{net}` | {net_summary(net, c['ref'])} |")
        L.append("")
    (OUT / "parts" / f"{name}.md").write_text("\n".join(L) + "\n", encoding="utf-8")
    part_index.append({"slug": name, "prefix": prefix, "value": value, "footprint": footprint,
                       "refs": refs, "sheets": sheets, "lcsc": lcsc, "unconfirmed": unconfirmed,
                       "pins": len(comp_pins(members[0])), "file": f"parts/{name}.md"})
(OUT / "bom.md").write_text("\n".join(bom) + "\n", encoding="utf-8")

(OUT / "index.json").write_text(json.dumps({"sheets": sheet_index, "parts": part_index}, indent=1), encoding="utf-8")

print(f"{len(comps)} components, {len(nets)} nets, {len(by_sheet)} sheets, {len(groups)} part types")
print(f"lint: {len(single)} single-node nets, {sum(len(v) for v in unconnected.values())} pins left unconnected, "
      f"{len(undriven)} nets without a driving pin type")
for p in part_index:
    if p["prefix"] not in ("R", "C", "TP", "JP"):
        print(f"  {p['prefix']:3} x{len(p['refs']):<3} {p['value'][:30]:<30} {p['footprint'].split(':')[-1][:44]}")
print("passive groups (R, C, TP, JP):", sum(1 for p in part_index if p["prefix"] in ("R", "C", "TP", "JP")))
