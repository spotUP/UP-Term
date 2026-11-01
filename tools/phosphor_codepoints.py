#!/usr/bin/env python3
r"""Phosphor style.css -> codepoints.json (icon name -> code point), the shape tools/gen_lucide_icons.py reads.
Rules look like `.ph-fill.ph-gear-six:before { content: "\e272"; }`. Sorted by name, so two runs give identical bytes.
usage: phosphor_codepoints.py <style.css> <out codepoints.json>"""
import json
import pathlib
import re
import sys


def parse(css):
    out = {}
    for name, hexcp in re.findall(r'\.ph-fill\.ph-([a-z0-9-]+):before\s*\{\s*content:\s*"\\([0-9a-fA-F]+)"', css):
        out[name] = int(hexcp, 16)
    if not out:
        raise SystemExit("no .ph-fill.ph-NAME rules found")
    return dict(sorted(out.items()))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    cps = parse(pathlib.Path(sys.argv[1]).read_text())
    pathlib.Path(sys.argv[2]).write_text(json.dumps(cps, indent=0, sort_keys=True) + "\n")
    print("[OK] %d icons in %s" % (len(cps), sys.argv[2]))


if __name__ == "__main__":
    main()
