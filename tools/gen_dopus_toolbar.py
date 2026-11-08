#!/usr/bin/env python3
"""Flat Lucide toolbar bank for Directory Opus 5.104 (task B7).

usage: gen_dopus_toolbar.py <table> <lucide-static.tgz> <upstream Buttons/Toolbar> <out dir>

Writes <out>/Images/<set>/<name> (IFF ILBM brushes, FORM ILBM / BMHD / CMAP of 8 pens / BODY, pens 0-6, drawn from the
Lucide SVG source (package/icons/<glyph>.svg in the lucide-static tarball) by tools/svg_raster.py on the pixel grid
(DT-8: design grid to whole pixels, stroke STROKE_PX, straight strokes whole pixels), levels by gen_lucide_icons.level) and <out>/Buttons/Amarchy-toolbar-<set> (one per size set 16, 16t, 24, 24t, 32, 32t): the upstream default
toolbar bank with only two differences, the image paths (dopus5:images/X -> dopus5:images/Amarchy/<set>/X) and the
Borderless Buttons flag (BTNWF_BORDERLESS, dopus5.h CFG_BTNW flags). The glyph map is the table file
(`<image name> <lucide glyph>` per line), nothing is named here.

Bank format (dopus5 source include/dopus/dopus5.h CFG_BTNW, config/): FORM EPUS { BTNW, then BUTN and FUNC chunks }.
BUTN chunks carry NUL terminated strings (image path, button name); only the image paths change, so each BUTN
size and the FORM size are recomputed (chunks stay padded to even)."""
import pathlib
import re
import struct
import sys
import tarfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_lucide_icons as gl  # noqa: E402
import svg_raster  # noqa: E402

# DT-1: size sets. SIZES are the brush widths (upstream's is 24, measured on DOpus5/Images/*.small); each set is square
# (width x width) or tall pixel (width x width/2, for a 2:1 pixel aspect, as gen_lucide_icons.VARIANTS (32, 16)).
# Theme apply picks the set (amarchy/dopustb_core.c dt_pick_set, which mirrors set_name below).
SIZES = (16, 24, 32)
DEPTH = 3                      # 8 pens
# DT-8 (owner 2026-11-08): the pen in target columns per width; tall sets get half as many rows (svg_raster.coverage).
STROKE_PX = {16: 1, 24: 2, 32: 2}
# Pens: 0 ground (transparent), 1-3 glyph at 33/66/100 percent, 4-6 accent at 33/66/100 percent, 7 ground. Levels map
# 1:1 to pens 1-3.
PEN_OF_LEVEL = (0, 1, 2, 3)
# Placeholder CMAP (Tokyo Night, the blend of amarchy/iconpen_core.c ip_colours). DOpus remaps a brush to the screen by
# its CMAP; theme apply rewrites these 24 bytes from ip_colours itself (dopustb_core.c dt_cmap), so nothing here is a
# second blend: these literals only serve until the first theme apply.
CMAP = ((0x1A, 0x1B, 0x26), (0x49, 0x4D, 0x60), (0x78, 0x7E, 0x9A), (0xA9, 0xB1, 0xD6), (0x3A, 0x48, 0x6B), (0x59, 0x74, 0xB0), (0x7A, 0xA2, 0xF7), (0x1A, 0x1B, 0x26))
OLD_PREFIX = b"dopus5:images/"
BTNW_FLAGS_OFF = 128           # CFG_BTNW: name[32] IBox[8] font[80] pad[3] size[1] columns[2] rows[2] flags[4] = 132 bytes
BORDERLESS = 0x10              # BTNWF_BORDERLESS = 1 << 4


def parse_map(path):
    rows = []
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.split(";", 1)[0].strip()
        if line:
            m = re.fullmatch(r"(\S+)\s+([a-z0-9-]+)", line)
            if not m:
                raise SystemExit("%s: bad line: %s" % (path, line))
            rows.append(m.groups())
    if not rows:
        raise SystemExit("%s: no rows" % path)
    return rows


def set_name(width, tall):
    return "%d%s" % (width, "t" if tall else "")


def sets():
    """[(set name, width, height)]: 16, 16t, 24, 24t, 32, 32t."""
    return [(set_name(w, t), w, w // 2 if t else w) for w in SIZES for t in (False, True)]


def chunk(tag, data):
    return tag + struct.pack(">L", len(data)) + data + (b"\0" if len(data) & 1 else b"")


def ilbm(w, h, pens):
    """FORM ILBM of h rows of w pens 0..7 (uncompressed, masking 2: pen 0 transparent, as upstream's brushes)."""
    art = ["".join(chr(p) for p in row) for row in pens]
    body = gl.mkicon.planes(art, DEPTH, {chr(i): i for i in range(1 << DEPTH)})
    # mkicon.planes is plane after plane; ILBM wants each row's planes together
    wb = ((w + 15) // 16) * 2
    rows = [[body[(p * h + y) * wb:(p * h + y + 1) * wb] for p in range(DEPTH)] for y in range(h)]
    body = b"".join(b"".join(r) for r in rows)
    bmhd = struct.pack(">HHhhBBBBHBBhh", w, h, 0, 0, DEPTH, 2, 0, 0, 0, 44, 44, w, h)
    cmap = b"".join(bytes(c) for c in CMAP)
    return chunk(b"FORM", b"ILBM" + chunk(b"BMHD", bmhd) + chunk(b"CMAP", cmap) + chunk(b"BODY", body))


def glyph(svg, w, h):
    cov = svg_raster.coverage(svg, w, h, STROKE_PX[w])
    return ilbm(w, h, [[PEN_OF_LEVEL[gl.level(cov[y][x])] for x in range(w)] for y in range(h)])


def rewrite_bank(src, name):
    """The upstream bank with the image drawer and the Borderless flag changed, nothing else."""
    if src[:4] != b"FORM" or src[8:12] != b"EPUS":
        raise SystemExit("not an Opus bank (FORM EPUS)")
    out, pos, seen = [], 12, 0
    while pos < len(src):
        tag, size = src[pos:pos + 4], struct.unpack(">L", src[pos + 4:pos + 8])[0]
        data = src[pos + 8:pos + 8 + size]
        pos += 8 + size + (size & 1)
        if tag == b"BTNW":
            flags = struct.unpack(">L", data[BTNW_FLAGS_OFF:BTNW_FLAGS_OFF + 4])[0] | BORDERLESS
            data = data[:BTNW_FLAGS_OFF] + struct.pack(">L", flags) + data[BTNW_FLAGS_OFF + 4:]
            seen += 1
        elif tag == b"BUTN":
            data = re.sub(re.escape(OLD_PREFIX), b"dopus5:images/Amarchy/" + name.encode() + b"/", data, flags=re.I)
        out.append(chunk(tag, data))
    if seen != 1:
        raise SystemExit("bank has %d BTNW chunks" % seen)
    return chunk(b"FORM", b"EPUS" + b"".join(out))


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    table, tgz, bank, dest = sys.argv[1:]
    with tarfile.open(tgz) as tf:
        by_name = {pathlib.PurePosixPath(m.name).stem: m for m in tf.getmembers()
                   if m.isfile() and re.fullmatch(r"package/icons/[a-z0-9-]+\.svg", m.name)}
        rows = parse_map(table)
        for name, g in rows:
            if g not in by_name:
                raise SystemExit("%s: unknown Lucide glyph: %s" % (table, g))
        svgs = {g: tf.extractfile(by_name[g]).read().decode() for _, g in rows}
    dest = pathlib.Path(dest)
    (dest / "Buttons").mkdir(parents=True, exist_ok=True)
    upstream = pathlib.Path(bank).read_bytes()
    for sname, w, h in sets():
        (dest / "Images" / sname).mkdir(parents=True, exist_ok=True)
        for name, g in rows:
            (dest / "Images" / sname / name).write_bytes(glyph(svgs[g], w, h))
        (dest / "Buttons" / ("Amarchy-toolbar-" + sname)).write_bytes(rewrite_bank(upstream, sname))
    print("[OK] %d images x %d sets and Buttons/Amarchy-toolbar-<set> in %s" % (len(rows), len(sets()), dest))


if __name__ == "__main__":
    main()
