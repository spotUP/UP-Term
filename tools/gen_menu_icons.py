#!/usr/bin/env python3
"""The drawn Amarchy menu's icons (plan 2026-10-23-amarchy-drawn-menu.md, G3): dist/menu-icons
(node id = code point) and Symbols Nerd Font Mono -> a C include with ONE colour-emoji page.

Reuses the emoji converter (tools/gen_emoji.py): each glyph is rasterised here to a white
RGBA image whose alpha is the coverage, and gen_emoji.glyph_bytes averages it to ONE ICON x ICON
image (32x32, amarchy/menu_icon.h MI_SRC; the menu only scales it down, so a 32 px icon is the glyph
drawn at 32 px, not a 16 px one doubled) and gen_emoji.page_bytes packs them into the page file
render/emoji.c reads (ce_page_check_px, ce_page_glyph_px). The glyph of the n-th line is code point U+E000 + n, so
all of them are page "E0". The glyph is fitted to the square, centred, whatever its advance.

usage: gen_menu_icons.py <dist/menu-icons> <SymbolsNerdFontMono.ttf> <out.inc>
The include defines mi_page_data[] (the page file) and mi_ids[] (the node ids, in glyph order)."""
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_emoji  # noqa: E402
from fontTools.pens.basePen import BasePen  # noqa: E402
from fontTools.ttLib import TTFont  # noqa: E402

BASE = 0xE000
ICON = 32             # the icon image, square: amarchy/menu_icon.h MI_SRC
SIZES = ((ICON, ICON),)
RES = 64              # the glyph's raster, square; gen_emoji averages it down to ICON
SUB = 4               # samples a raster pixel, each way
EDGE = 0.04           # blank margin of the square, each side


class Flatten(BasePen):
    """The outline as polygons (curves cut into 8 lines)."""

    def __init__(self, gs):
        super().__init__(gs)
        self.polys, self.cur = [], []

    def _moveTo(self, p):
        self.cur = [p]

    def _lineTo(self, p):
        self.cur.append(p)

    def _curveToOne(self, a, b, c):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8.0
            u = 1 - t
            self.cur.append((u**3 * p0[0] + 3 * u * u * t * a[0] + 3 * u * t * t * b[0] + t**3 * c[0],
                             u**3 * p0[1] + 3 * u * u * t * a[1] + 3 * u * t * t * b[1] + t**3 * c[1]))

    def _qCurveToOne(self, a, b):
        p0 = self.cur[-1]
        for i in range(1, 9):
            t = i / 8.0
            u = 1 - t
            self.cur.append((u * u * p0[0] + 2 * u * t * a[0] + t * t * b[0],
                             u * u * p0[1] + 2 * u * t * a[1] + t * t * b[1]))

    def _closePath(self):
        if len(self.cur) > 2:
            self.polys.append(self.cur)
        self.cur = []

    _endPath = _closePath


def raster(font, cp):
    """(RES, RES, [RGBA]) of the glyph: white, alpha the coverage, fitted and centred."""
    gs = font.getGlyphSet()
    name = font.getBestCmap().get(cp)
    if name is None:
        raise SystemExit("U+%04X is not in the font" % cp)
    pen = Flatten(gs)
    gs[name].draw(pen)
    pts = [p for poly in pen.polys for p in poly]
    if not pts:
        raise SystemExit("U+%04X has no outline" % cp)
    x0, x1 = min(p[0] for p in pts), max(p[0] for p in pts)
    y0, y1 = min(p[1] for p in pts), max(p[1] for p in pts)
    span = max(x1 - x0, y1 - y0)
    scale = RES * (1 - 2 * EDGE) / span
    ox = (RES - (x1 - x0) * scale) / 2 - x0 * scale
    oy = (RES - (y1 - y0) * scale) / 2 - y0 * scale
    n = RES * SUB
    edges = []
    for poly in pen.polys:
        q = [(x * scale + ox, RES - (y * scale + oy)) for x, y in poly]   # y down
        for a, b in zip(q, q[1:] + q[:1]):
            if a[1] != b[1]:
                edges.append((a, b))
    cover = [0] * (RES * RES)
    for sy in range(n):
        y = (sy + 0.5) / SUB
        xs = []
        for a, b in edges:
            if (a[1] <= y < b[1]) or (b[1] <= y < a[1]):
                xs.append((a[0] + (y - a[1]) * (b[0] - a[0]) / (b[1] - a[1]), 1 if b[1] > a[1] else -1))
        xs.sort()
        wind = 0
        for i, (x, d) in enumerate(xs[:-1]):
            wind += d
            if wind != 0:
                lo, hi = x * SUB, xs[i + 1][0] * SUB
                for sx in range(max(0, int(lo + 0.5)), min(n, int(hi + 0.5))):
                    cover[(sy // SUB) * RES + sx // SUB] += 1
    full = SUB * SUB
    px = [(255, 255, 255, min(255, c * 255 // full)) if c else (0, 0, 0, 0) for c in cover]
    return RES, RES, px


def parse_table(path):
    rows = []
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.split(";", 1)[0].strip()
        if not line:
            continue
        m = re.fullmatch(r"([a-z0-9.]+)\s*=\s*U\+([0-9A-Fa-f]{4,6})", line)
        if not m:
            raise SystemExit("%s: bad line: %s" % (path, line))
        rows.append((m.group(1), int(m.group(2), 16)))
    if not rows or len(rows) > 256:
        raise SystemExit("%s: 1 to 256 icons, got %d" % (path, len(rows)))
    return rows


def build(table, ttf):
    rows = parse_table(table)
    font = TTFont(ttf)
    glyphs = {BASE + i: gen_emoji.glyph_bytes(*raster(font, cp), sizes=SIZES) for i, (_, cp) in enumerate(rows)}
    return [r[0] for r in rows], gen_emoji.page_bytes(BASE >> 8, glyphs)


def write_inc(path, ids, page):
    out = ["/* generated by tools/gen_menu_icons.py: do not edit */",
           "static const unsigned char mi_page_data[%d] = {" % len(page)]
    for i in range(0, len(page), 16):
        out.append(" " + ",".join("%d" % b for b in page[i:i + 16]) + ",")
    out.append("};")
    out.append("static const char *const mi_ids[%d] = {" % len(ids))
    out += ['    "%s",' % i for i in ids]
    out.append("};")
    pathlib.Path(path).write_text("\n".join(out) + "\n")


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    ids, page = build(sys.argv[1], sys.argv[2])
    write_inc(sys.argv[3], ids, page)
    print("[OK] %d menu icons, %d bytes in %s" % (len(ids), len(page), sys.argv[3]))


if __name__ == "__main__":
    main()
