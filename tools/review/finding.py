#!/usr/bin/env python3
"""Print findings of the register by id.  Usage: finding.py <review dir> <id> [<id> ...]"""
# From the design review tools of the 35.esp32-s3_power_meter project (MIT, same author).
import json
import pathlib
import sys

register = json.loads((pathlib.Path(sys.argv[1]) / "out" / "register.json").read_text(encoding="utf-8"))
wanted = set(sys.argv[2:])
for row in register:
    if row["id"] in wanted:
        print(json.dumps(row, indent=1))
        wanted.discard(row["id"])
for missing in sorted(wanted):
    print("not in the register:", missing)
