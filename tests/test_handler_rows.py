#!/usr/bin/env python3
"""Each /setup-style row table in vtcon_handler.c holds every row its function writes.

The outline-antialias row (plan 2026-10-22-handler-rasterizer.md R4.4) made /setup write nine rows
into `slash_row rows[8]`: a stack overrun on the Amiga that no host test runs into. For each
`slash_row rows[N];` the rows written after it (`rows[n].name =`, up to the function's closing brace)
must number N or fewer."""
import os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
src = open(os.path.join(ROOT, "handler/vtcon_handler.c")).read()
tables = list(re.finditer(r"slash_row rows\[(\d+)\];", src))
bad = []
for m in tables:
    end = src.find("\n}", m.end())
    written = len(re.findall(r"rows\[n\]\.name\s*=", src[m.end():end]))
    if written > int(m.group(1)):
        bad.append("line %d: rows[%s], %d written" % (src.count("\n", 0, m.start()) + 1, m.group(1), written))
assert tables, "no slash_row tables found"
assert not bad, "row table too small: %s" % "; ".join(bad)
print("handler rows: %d row tables hold their rows" % len(tables))
