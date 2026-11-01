#!/usr/bin/env python3
"""Host preview of lucide.dat (plan 2026-10-31-amarchy-lucide-icons.md): a PNG contact sheet of every icon at its real Amiga
pixel size, scaled x4 nearest-neighbour, in the real pens (amarchy/iconpen_core.c ip_colours: stored pen 32+slot, slot colour
blended from the theme background toward FG / SELFG), on the Amarchy backdrop (DK_MUTED). Per icon: square 32x32 normal and
selected, then tall 32x16 normal and selected. Pixel decode reuses tools/gen_lucide_icons.py (layout, pen tables) and
tools/mkicon.py planes' inverse; no rasteriser here.

usage: preview_lucide_icons.py <lucide.dat> <out.png>      (build lucide.dat: gen_lucide_icons.py dist/lucide-icons build/lucide/lucide.ttf out)"""
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_lucide_icons as g  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

SCALE = 4
# default theme tokyo-night (amarchy/gthemes_core.c gt_builtin); pens as ip_colours; slot numbers from iconpen_core.h
BG, FG, SELECTION, BACKDROP = 0x1a1b26, 0xa9b1d6, 0x292e42, 0x414868
SLOT_FG33, SLOT_FG100, SLOT_SELFG100, SLOT_FG66, SLOT_SELFG66, SLOT_SELFG33, SLOT_SELBG = 0, 1, 2, 5, 6, 10, 11
TEXT = (200, 205, 225)
PAGE = (14, 14, 20)


def rgb(c):
    return ((c >> 16) & 255, (c >> 8) & 255, c & 255)


def blend(a, b, pct):
    return tuple((x * (100 - pct) + y * pct + 50) // 100 for x, y in zip(rgb(a), rgb(b)))


def pen_colours():
    """stored pen -> RGB; pen 0 (transparent) is the backdrop."""
    p = {0: rgb(BACKDROP)}
    for slot, c in ((SLOT_FG33, blend(BG, FG, 33)), (SLOT_FG66, blend(BG, FG, 66)), (SLOT_FG100, blend(BG, FG, 100)),
                    (SLOT_SELFG33, blend(SELECTION, FG, 33)), (SLOT_SELFG66, blend(SELECTION, FG, 66)),
                    (SLOT_SELFG100, blend(SELECTION, FG, 100)), (SLOT_SELBG, rgb(SELECTION))):
        p[32 + slot] = c
    return p


def unplane(data, off, w, h):
    words = w // 16
    pens = [[0] * w for _ in range(h)]
    for pl in range(g.DEPTH):
        for y in range(h):
            for i in range(words):
                (v,) = struct.unpack_from(">H", data, off + ((pl * h + y) * words + i) * 2)
                for b in range(16):
                    pens[y][i * 16 + b] |= ((v >> (15 - b)) & 1) << pl
    return pens


def parse(data):
    if data[:4] != b"LUCI":
        raise SystemExit("not a lucide.dat")
    ver, count, depth, nvar, _ = struct.unpack_from(">HHHHL", data, 4)
    if (ver, depth) != (g.VERSION, g.DEPTH):
        raise SystemExit("unexpected version/depth %d/%d" % (ver, depth))
    var = [struct.unpack_from(">HH", data, 16 + 4 * i) for i in range(nvar)]
    base = 16 + 4 * nvar
    idx = []
    for i in range(count):
        name, off = struct.unpack_from(">%dsL" % g.NAME_LEN, data, base + i * (g.NAME_LEN + 4))
        idx.append((name.rstrip(b"\0").decode("latin-1"), off))
    return var, idx


def tile(pens, pal):
    im = Image.new("RGB", (len(pens[0]), len(pens)))
    im.putdata([pal.get(p, (255, 0, 255)) for row in pens for p in row])   # unknown pen = magenta
    return im.resize((im.width * SCALE, im.height * SCALE), Image.NEAREST)


def sheet(data, out, cols=3):
    var, idx = parse(data)
    pal = pen_colours()
    by_off = {}
    for name, off in idx:
        by_off.setdefault(off, []).append(name)
    pad, label_h = 12, 56
    cw = sum(w * SCALE for w, _ in var) * 2 + pad * (2 * len(var) + 1)
    ch = max(h for _, h in var) * SCALE + label_h + pad
    offs = sorted(by_off)
    rows = (len(offs) + cols - 1) // cols
    img = Image.new("RGB", (cols * cw, rows * ch), PAGE)
    d = ImageDraw.Draw(img)
    for n, off in enumerate(offs):
        x0, y0 = (n % cols) * cw, (n // cols) * ch
        d.rectangle([x0 + 2, y0 + 2, x0 + cw - 3, y0 + ch - 3], fill=rgb(BACKDROP))
        x, pos = x0 + pad, off
        for w, h in var:
            for _ in range(2):                                  # normal, selected
                img.paste(tile(unplane(data, pos, w, h), pal), (x, y0 + pad))
                pos += w // 8 * h * g.DEPTH
                x += w * SCALE + pad
        names = by_off[off]
        d.text((x0 + pad, y0 + ch - label_h + 2), "%d icon %s" % (n + 1, ", ".join(names)[:70]), fill=TEXT)
        if len(", ".join(names)) > 70:
            d.text((x0 + pad, y0 + ch - label_h + 16), ", ".join(names)[70:140], fill=TEXT)
    img.save(out)
    return len(offs), len(idx), var


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    icons, targets, var = sheet(pathlib.Path(sys.argv[1]).read_bytes(), sys.argv[2])
    print("[OK] %d icons, %d targets, variants %s -> %s" % (icons, targets, var, sys.argv[2]))


if __name__ == "__main__":
    main()
