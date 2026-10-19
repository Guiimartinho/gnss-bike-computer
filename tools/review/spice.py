#!/usr/bin/env python3
"""Run an ngspice deck with the ngspice library that ships with KiCad.

Usage: python spice.py <deck.cir> [--psa]

The deck must hold its own ``.control`` ... ``.endc`` section (run the
analysis, then ``print``, ``meas`` or ``wrdata``). Everything ngspice prints
goes to standard output. ``--psa`` turns on the PSpice compatibility mode,
needed by most vendor models (.lib files written for PSpice).

Relative paths inside the deck (``.include``, ``wrdata``) are resolved from
the directory of the deck.
"""
# From the design review tools of the 35.esp32-s3_power_meter project (MIT, same author).

from __future__ import annotations

import ctypes
import os
import pathlib
import sys

KICAD = pathlib.Path(os.environ.get("KICAD_ROOT", "D:/KiCAD"))


def main() -> int:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    flags = {a for a in sys.argv[1:] if a.startswith("--")}
    if len(args) != 1:
        print(__doc__)
        return 2
    deck = pathlib.Path(args[0]).resolve()
    if not deck.exists():
        print(f"deck not found: {deck}")
        return 2

    os.add_dll_directory(str(KICAD / "bin"))
    os.chdir(deck.parent)
    ng = ctypes.CDLL(str(KICAD / "bin" / "ngspice.dll"))

    lines: list[str] = []

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
    def on_char(text, _ident, _user):
        line = text.decode("utf-8", "replace")
        for prefix in ("stdout ", "stderr "):
            if line.startswith(prefix):
                line = line[len(prefix):]
                break
        lines.append(line)
        print(line, flush=True)
        return 0

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_char_p, ctypes.c_int, ctypes.c_void_p)
    def on_stat(_text, _ident, _user):
        return 0

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_int, ctypes.c_bool, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)
    def on_exit(_status, _immediate, _quit, _ident, _user):
        return 0

    @ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_bool, ctypes.c_int, ctypes.c_void_p)
    def on_bg(_running, _ident, _user):
        return 0

    ng.ngSpice_Init(on_char, on_stat, on_exit, None, None, on_bg, None)
    ng.ngSpice_Command.argtypes = [ctypes.c_char_p]

    def command(text: str) -> int:
        return ng.ngSpice_Command(text.encode("utf-8"))

    models = KICAD / "lib" / "ngspice"
    for cm in ("spice2poly", "analog", "digital", "xtradev", "xtraevt", "table"):
        path = models / f"{cm}.cm"
        if path.exists():
            command(f"codemodel {path.as_posix()}")
    if "--psa" in flags:
        command("set ngbehavior=psa")
    command(f"source {deck.as_posix()}")
    failed = any("error" in line.lower() and "tolerance" not in line.lower() for line in lines)
    return 1 if failed else 0


if __name__ == "__main__":
    code = main()
    sys.stdout.flush()
    os._exit(code)
