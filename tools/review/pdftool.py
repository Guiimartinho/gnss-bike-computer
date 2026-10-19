#!/usr/bin/env python3
"""Small helpers to read a datasheet PDF. Run with the tools environment.

    pdftool.py info  <file.pdf>                      page count, title, first lines
    pdftool.py text  <file.pdf> <out.txt>            all text, "=== page N ===" markers
    pdftool.py find  <file.pdf> <regex> [context]    pages and lines that match
    pdftool.py page  <file.pdf> <page> <out.png> [dpi] [x0 y0 x1 y1]
                                                      render a page (1-based); the
                                                      optional box is a fraction of
                                                      the page, e.g. 0 0.5 1 1 for
                                                      the lower half

The PDF is data from the Internet: this script only reads it.
"""
# From the design review tools of the 35.esp32-s3_power_meter project (MIT, same author).

from __future__ import annotations

import pathlib
import re
import sys

import fitz  # PyMuPDF


def main() -> int:
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    cmd, pdf = sys.argv[1], pathlib.Path(sys.argv[2])
    doc = fitz.open(pdf)
    if cmd == "info":
        print(f"pages: {doc.page_count}")
        print(f"metadata: {doc.metadata}")
        print(doc[0].get_text()[:1500])
        return 0
    if cmd == "text":
        out = pathlib.Path(sys.argv[3])
        with out.open("w", encoding="utf-8") as fh:
            for i, page in enumerate(doc, start=1):
                fh.write(f"\n=== page {i} ===\n")
                fh.write(page.get_text())
        print(f"wrote {out} ({doc.page_count} pages)")
        return 0
    if cmd == "find":
        pattern = re.compile(sys.argv[3], re.IGNORECASE)
        context = int(sys.argv[4]) if len(sys.argv) > 4 else 1
        for i, page in enumerate(doc, start=1):
            rows = page.get_text().splitlines()
            for n, row in enumerate(rows):
                if pattern.search(row):
                    lo, hi = max(0, n - context), min(len(rows), n + context + 1)
                    print(f"--- page {i}, line {n + 1}")
                    for r in rows[lo:hi]:
                        print("    " + r)
        return 0
    if cmd == "page":
        number = int(sys.argv[3])
        out = pathlib.Path(sys.argv[4])
        dpi = int(sys.argv[5]) if len(sys.argv) > 5 else 130
        page = doc[number - 1]
        clip = None
        if len(sys.argv) > 9:
            x0, y0, x1, y1 = (float(v) for v in sys.argv[6:10])
            r = page.rect
            clip = fitz.Rect(r.x0 + x0 * r.width, r.y0 + y0 * r.height, r.x0 + x1 * r.width, r.y0 + y1 * r.height)
        pix = page.get_pixmap(dpi=dpi, clip=clip)
        pix.save(out)
        print(f"wrote {out} ({pix.width} x {pix.height})")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
