#!/usr/bin/env python3
"""Collect every finding on file into one register and a compact list.

Usage: known.py <review dir> [<workflow output json> ...]

Reads out/findings/*.json (what each reviewer wrote), the verdicts in
out/verdicts/ (<id>--<lens>.json, or any file holding {"items": [...]} with one
item per id) and, when given, the output files of the workflows. Writes

    out/register.json       every finding with a stable id and its status
    out/known-findings.md   compact list for reviewers who must not repeat them
"""
# From the design review tools of the 35.esp32-s3_power_meter project (MIT, same author).

from __future__ import annotations

import json
import pathlib
import re
import sys

REVIEW = pathlib.Path(sys.argv[1])
OUTPUTS = [pathlib.Path(p) for p in sys.argv[2:]]
RANK = {"blocker": 0, "major": 1, "minor": 2, "info": 3, "none": 4}


def load(path: pathlib.Path):
    raw = path.read_text(encoding="utf-8")
    data = json.loads(raw)
    result = data.get("result", data) if isinstance(data, dict) else data
    if isinstance(result, str):
        result = json.loads(result)
    return result


# Verdicts from the workflow outputs: id -> dict
status: dict[str, dict] = {}
for path in OUTPUTS:
    if not path.exists() or path.stat().st_size == 0:
        continue
    result = load(path)
    for part in result.get("parts", []) or []:
        for f in part.get("findings", []):
            status[f["id"]] = {"status": f.get("status"), "severity": f.get("severity"),
                               "corrected_claim": f.get("corrected_claim", ""), "verdicts": f.get("verdicts", []),
                               "title": f.get("title"), "from": f.get("from")}
    for item in result.get("verified", []) or []:
        status[item["id"]] = {"status": item.get("status"), "severity": item.get("severity"),
                              "corrected_claim": item.get("corrected_claim", ""), "verdicts": item.get("verdicts", []),
                              "title": item.get("title"), "from": item.get("from", "")}

# Verdicts written by the verifiers themselves, settled the way the workflows do:
# a majority of confirmed or partly against refuted, the others count as open.
RANK_UP = {"none": 0, "info": 1, "minor": 2, "major": 3, "blocker": 4}
written: dict[str, list[dict]] = {}
for path in sorted((REVIEW / "out" / "verdicts").glob("*.json")):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:
        print("unreadable verdict:", path.name, exc)
        continue
    if isinstance(data, dict) and isinstance(data.get("items"), list):  # a batch: one item per id
        for it in data.get("items", []):
            written.setdefault(it.get("id", ""), []).append({"lens": "batch", **it})
    elif "--" in path.stem:
        fid, lens = path.stem.rsplit("--", 1)
        written.setdefault(fid, []).append({"lens": lens, **data})
for fid, vs in written.items():
    if fid in status or not fid:
        continue
    yes = [v for v in vs if v.get("verdict") in ("confirmed", "partly")]
    no = [v for v in vs if v.get("verdict") == "refuted"]
    open_ = len(vs) - len(yes) - len(no)
    st = "open"
    if len(yes) > len(no) and len(yes) >= open_:
        st = "confirmed"
    elif len(no) > len(yes) and len(no) >= open_:
        st = "refuted"
    pool = sorted((v.get("severity") for v in (yes if st == "confirmed" else vs) if v.get("severity") in RANK_UP),
                  key=lambda s: -RANK_UP[s])
    corrected = [v.get("corrected_claim") for v in vs if v.get("corrected_claim")]
    status[fid] = {"status": st, "severity": pool[0] if pool else None,
                   "corrected_claim": corrected[-1] if corrected else "",
                   "verdicts": [{"lens": v.get("lens"), "verdict": v.get("verdict"), "severity": v.get("severity"),
                                 "reasoning": (v.get("reasoning") or "")[:700]} for v in vs],
                   "title": None, "from": ""}

register = []
for path in sorted((REVIEW / "out" / "findings").glob("*.json")):
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except Exception as exc:  # a reviewer wrote broken JSON
        print("unreadable:", path.name, exc)
        continue
    source = path.stem
    key = re.sub(r"^(part|cross|dim|sheet|scen|crit|known)-", "", source)
    prefix = {"part": "a", "cross": "x", "dim": "d", "sheet": "s", "scen": "w", "crit": "c", "known": "k"}.get(source.split("-", 1)[0], "f")
    for i, f in enumerate(data.get("findings", []), start=1):
        fid = f"{key}-{prefix}{i}"
        st = status.get(fid, {})
        if st and st.get("title") and st["title"] != f.get("title"):
            # The file and the returned object differ in order: match by title inside the same source.
            same = [v for k, v in status.items() if k.startswith(key + "-") and v.get("title") == f.get("title")]
            st = same[0] if same else {}
        register.append({
            "id": fid, "source": source, "title": f.get("title", ""), "category": f.get("category", ""),
            "refs": f.get("refs", []), "nets": f.get("nets", []),
            "reported": f.get("severity", ""), "severity": st.get("severity") or f.get("severity", ""),
            "status": st.get("status") or "not-verified",
            "claim": f.get("claim", ""), "corrected_claim": st.get("corrected_claim", ""),
            "evidence_schematic": f.get("evidence_schematic", ""), "evidence_source": f.get("evidence_source", ""),
            "consequence": f.get("consequence", ""), "fix": f.get("fix", ""), "confidence": f.get("confidence", ""),
            "verdicts": st.get("verdicts", []),
        })

# Findings that exist only in a workflow output (cross-check findings of the parts audit).
seen = {r["id"] for r in register}
for fid, st in status.items():
    if fid not in seen and "-x" in fid:
        register.append({"id": fid, "source": "cross-" + fid.rsplit("-x", 1)[0], "title": st.get("title") or "", "category": "",
                         "refs": [], "nets": [], "reported": st.get("severity", ""), "severity": st.get("severity", ""),
                         "status": st.get("status") or "not-verified", "claim": st.get("title") or "",
                         "corrected_claim": st.get("corrected_claim", ""), "evidence_schematic": "", "evidence_source": "",
                         "consequence": "", "fix": "", "confidence": "", "verdicts": st.get("verdicts", [])})

register.sort(key=lambda r: (RANK.get(r["severity"], 9), r["source"], r["id"]))
(REVIEW / "out" / "register.json").write_text(json.dumps(register, indent=1), encoding="utf-8")


def clip(text: str, n: int) -> str:
    text = " ".join((text or "").split())
    return text if len(text) <= n else text[:n].rstrip() + " ..."


lines = ["# Findings already on file", "",
         "Do not report these again. Refer to one by its id if your work confirms, corrects or contradicts it.",
         "Full text of each: out/register.json (search the id). Status: confirmed, refuted, open (reviewers disagree),",
         "not-verified (nobody has checked it yet).", ""]
for sev in ("blocker", "major", "minor"):
    rows = [r for r in register if r["severity"] == sev and r["status"] != "refuted"]
    lines += [f"## {sev} ({len(rows)})", ""]
    for r in rows:
        claim = r["corrected_claim"] or r["claim"]
        lines.append(f"- [{r['id']}] ({r['status']}) {clip(r['title'], 150)} | refs: {', '.join(r['refs'][:8])} | {clip(claim, 260)}")
    lines.append("")
refuted = [r for r in register if r["status"] == "refuted"]
lines += [f"## refuted by the verification ({len(refuted)}): not problems, do not raise them again without new evidence", ""]
lines += [f"- [{r['id']}] {clip(r['title'], 160)}" for r in refuted]
info = [r for r in register if r["severity"] == "info" and r["status"] != "refuted"]
lines += ["", f"## info ({len(info)}): titles only", ""]
lines += [f"- [{r['id']}] {clip(r['title'], 150)}" for r in info]
(REVIEW / "out" / "known-findings.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

count: dict[str, int] = {}
for r in register:
    k = f"{r['severity']}/{r['status']}"
    count[k] = count.get(k, 0) + 1
print(len(register), "findings on file")
for k in sorted(count, key=lambda s: (RANK.get(s.split("/")[0], 9), s)):
    print(f"  {k}: {count[k]}")
