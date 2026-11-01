#!/usr/bin/env python3
"""Filled icons for the flat Workbench (plan 2026-11-01-amarchy-filled-icon-set.md F4, earlier L3): dist/lucide-icons
(match = phosphor-name) and the Phosphor Fill icon font -> lucide.dat (file name kept), the planar images the on-Amiga applier splices into .info files.

Reuses, writes no rasteriser of its own: tools/gen_menu_icons.py (Flatten, raster: outline -> 64x64 coverage, SUB=4),
tools/gen_emoji.py (area: coverage averaged to the icon size) and tools/mkicon.py (planes: the planar packer).

Pens (plan section 3): 16 reserved pens 32..47. The glyph has 4 levels of coverage: transparent (ground pen 44: the backdrop colour; classic planar images have no alpha, pen 0 would be a black box) under 20%, then
33%, 66%, 100%. The normal image draws them in FG (pens 32, 37, 33); the selected image draws them in SELFG (42, 38, 34)
on a rounded square of SELBG (43), so the antialiased edge has a known background.

lucide.dat (big endian):
  header   "LUCI" u16 version(1) u16 count u16 depth(6) u16 variants(2) u32 reserved(0)       16 bytes
  variants variants x (u16 width, u16 height)       square pixels 32x32, then tall pixels 32x16 (640x256 non-laced hires)
  index    count x (char name[48] NUL padded, u32 offset of the record from the file start), sorted by name
  record   for each variant in order: normal image planes, then selected image planes (depth planes, each height rows of
           width/16 words; pen 0 is transparent)

usage: gen_lucide_icons.py (<table> <font.ttf> <out.dat>)...   one triple per set: Filled (lucide.dat, Phosphor Fill) and
Outline (outline.dat, Lucide); codepoints.json, the icon names, beside each font (make phosphor, make lucide)"""
import json
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_emoji  # noqa: E402
import gen_menu_icons  # noqa: E402
import mkicon  # noqa: E402
from fontTools.ttLib import TTFont  # noqa: E402

VARIANTS = ((32, 32), (32, 16))
DEPTH = 6
VERSION = 1
NAME_LEN = 48
FIRST_LEVEL = 0.20                      # coverage under this is transparent
LEVELS = (0.50, 0.835)                  # upper edges of levels 1 and 2 (33%, 66%); above is level 3 (100%)
PEN_GROUND = 44                         # spare pen 12: always the backdrop ground colour (iconpen_core fills spares with ground)
PEN_NORMAL = (PEN_GROUND, 32, 37, 33)   # pen of level 0..3: ground, FG 33%, FG 66%, FG 100%
PEN_SELECTED = (PEN_GROUND, 42, 38, 34) # ground, SELFG 33%, 66%, 100%
PEN_SELBG = 43
PENS = {chr(i): i for i in range(64)}   # art character -> pen: chr(pen), for mkicon.planes


def parse_table(path, names):
    """[(match, icon name)] of the table; `names` is the set of icon names of the font."""
    rows, seen = [], set()
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.split(";", 1)[0].strip()
        if not line:
            continue
        m = re.fullmatch(r"(\S+)\s*=\s*([a-z0-9-]+)", line)
        if not m:
            raise SystemExit("%s: bad line: %s" % (path, line))
        match, name = m.groups()
        ok = (re.fullmatch(r"def_[A-Za-z0-9_]+", match) or re.fullmatch(r"type:(disk|drawer|tool|project|garbage|kick|device)", match)
              or re.fullmatch(r"[^:/\s]+:?[^\s]*", match))
        if not ok or len(match) > 200:
            raise SystemExit("%s: bad match: %s" % (path, match))
        if name not in names:
            raise SystemExit("%s: unknown icon: %s" % (path, name))
        if match in seen:
            raise SystemExit("%s: duplicate match: %s" % (path, match))
        seen.add(match)
        rows.append((match, name))
    if not rows:
        raise SystemExit("%s: no rows" % path)
    return rows


def level(cov):
    """0..3 of a coverage 0..1 (transparent, 33%, 66%, 100%)."""
    if cov < FIRST_LEVEL:
        return 0
    return 1 if cov < LEVELS[0] else 2 if cov < LEVELS[1] else 3


def coverage(font, cp, w, h):
    """h rows of w coverages 0..1 of the glyph (code point cp) averaged from the 64x64 raster to w x h."""
    rw, rh, px = gen_menu_icons.raster(font, cp)
    out = gen_emoji.area(rw, rh, px, w, h)
    return [[out[y * w + x][3] / 255.0 for x in range(w)] for y in range(h)]


def rounded(w, h, x, y):
    """True when pixel (x, y) is inside the rounded square of the selection plate (corner radius 1/8 of the width,
    in the image's own pixels, so on the tall variant it is half as many rows)."""
    rx = w / 8.0
    ry = h / 8.0
    cx = min(max(x + 0.5, rx), w - rx)
    cy = min(max(y + 0.5, ry), h - ry)
    return ((x + 0.5 - cx) / rx) ** 2 + ((y + 0.5 - cy) / ry) ** 2 <= 1.0


def images(cov, w, h):
    """(normal, selected) planar bytes of a coverage grid."""
    normal = ["".join(chr(PEN_NORMAL[level(cov[y][x])]) for x in range(w)) for y in range(h)]
    selected = []
    for y in range(h):
        row = ""
        for x in range(w):
            lv = level(cov[y][x])
            row += chr(PEN_SELECTED[lv] if lv else PEN_SELBG if rounded(w, h, x, y) else PEN_GROUND)
        selected.append(row)
    return mkicon.planes(normal, DEPTH, PENS), mkicon.planes(selected, DEPTH, PENS)


def record(font, cp):
    out = b""
    for w, h in VARIANTS:
        n, s = images(coverage(font, cp, w, h), w, h)
        out += n + s
    return out


def build(table, ttf):
    font = TTFont(ttf)
    by_name = json.loads((pathlib.Path(ttf).parent / "codepoints.json").read_text())   # icon name -> code point, beside the font
    rows = parse_table(table, set(by_name))
    key = {}
    for match, name in rows:
        key[match] = name
    icons = sorted(set(key.values()))
    recs = {n: record(font, by_name[n]) for n in icons}
    return key, icons, recs, font


def pack(key, icons, recs):
    """lucide.dat: one record per icon; the index maps each table match to its icon's record."""
    head = 16 + 4 * len(VARIANTS) + (NAME_LEN + 4) * len(key)
    offs, pos = {}, head
    for n in icons:
        offs[n] = pos
        pos += len(recs[n])
    out = b"LUCI" + struct.pack(">HHHHL", VERSION, len(key), DEPTH, len(VARIANTS), 0)
    for w, h in VARIANTS:
        out += struct.pack(">HH", w, h)
    for match in sorted(key):
        b = match.encode("latin-1")
        if len(b) >= NAME_LEN:
            raise SystemExit("match too long for the index: %s" % match)
        out += b.ljust(NAME_LEN, b"\0") + struct.pack(">L", offs[key[match]])
    assert len(out) == head
    return out + b"".join(recs[n] for n in icons)


def main():
    args = sys.argv[1:]
    if not args or len(args) % 3:
        raise SystemExit(__doc__)
    for i in range(0, len(args), 3):
        table, ttf, dest = args[i:i + 3]
        key, icons, recs, _ = build(table, ttf)
        data = pack(key, icons, recs)
        tmp = pathlib.Path(dest + ".tmp")
        tmp.parent.mkdir(parents=True, exist_ok=True)
        tmp.write_bytes(data)
        tmp.replace(dest)
        print("[OK] %d targets, %d icons, %d bytes in %s" % (len(key), len(icons), len(data), dest))


if __name__ == "__main__":
    main()
