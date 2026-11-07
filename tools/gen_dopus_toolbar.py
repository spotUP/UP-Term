#!/usr/bin/env python3
"""Flat Lucide toolbar bank for Directory Opus 5.104 (task B7).

usage: gen_dopus_toolbar.py <table> <lucide.ttf> <upstream Buttons/Toolbar> <out dir>

Writes <out>/Images/<name> (IFF ILBM brushes, FORM ILBM / BMHD / CMAP of 4 pens / BODY, pens 0-3, drawn by the
existing Lucide path of gen_lucide_icons: coverage, level) and <out>/Buttons/Amarchy-toolbar: the upstream default
toolbar bank with only two differences, the image paths (dopus5:images/X -> dopus5:images/Amarchy/X) and the
Borderless Buttons flag (BTNWF_BORDERLESS, dopus5.h CFG_BTNW flags). The glyph map is the table file
(`<image name> <lucide glyph>` per line), nothing is named here.

Bank format (dopus5 source include/dopus/dopus5.h CFG_BTNW, config/): FORM EPUS { BTNW, then BUTN and FUNC chunks }.
BUTN chunks carry NUL terminated strings (image path, button name); only the image paths change, so each BUTN
size and the FORM size are recomputed (chunks stay padded to even)."""
import json
import pathlib
import re
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import gen_lucide_icons as gl  # noqa: E402
from fontTools.ttLib import TTFont  # noqa: E402

SIZE = 24                      # upstream brush size, measured on DOpus5/Images/*.small (BMHD 24x24)
DEPTH = 2                      # pens 0-3
# CMAP of pens 0..3: ground (transparent), glyph, glyph at 33%, accent. The default theme (Tokyo Night) colours: DOpus
# remaps a brush to the screen with its CMAP, so these do not follow a later theme change.
CMAP = ((0x1A, 0x1B, 0x26), (0xA9, 0xB1, 0xD6), (0x41, 0x48, 0x68), (0x7A, 0xA2, 0xF7))
PEN_OF_LEVEL = (0, 2, 1, 1)    # level 0 ground, 1 (33%) dim, 2 and 3 glyph
OLD_PREFIX = b"dopus5:images/"
NEW_PREFIX = b"dopus5:images/Amarchy/"
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


def chunk(tag, data):
    return tag + struct.pack(">L", len(data)) + data + (b"\0" if len(data) & 1 else b"")


def ilbm(w, h, pens):
    """FORM ILBM of h rows of w pens 0..3 (uncompressed, masking 2: pen 0 transparent, as upstream's brushes)."""
    art = ["".join(chr(p) for p in row) for row in pens]
    body = gl.mkicon.planes(art, DEPTH, {chr(i): i for i in range(4)})
    # mkicon.planes is plane after plane; ILBM wants each row's planes together
    wb = ((w + 15) // 16) * 2
    rows = [[body[(p * h + y) * wb:(p * h + y + 1) * wb] for p in range(DEPTH)] for y in range(h)]
    body = b"".join(b"".join(r) for r in rows)
    bmhd = struct.pack(">HHhhBBBBHBBhh", w, h, 0, 0, DEPTH, 2, 0, 0, 0, 44, 44, w, h)
    cmap = b"".join(bytes(c) for c in CMAP)
    return chunk(b"FORM", b"ILBM" + chunk(b"BMHD", bmhd) + chunk(b"CMAP", cmap) + chunk(b"BODY", body))


def glyph(font, cp):
    cov = gl.coverage(font, cp, SIZE, SIZE)
    return ilbm(SIZE, SIZE, [[PEN_OF_LEVEL[gl.level(cov[y][x])] for x in range(SIZE)] for y in range(SIZE)])


def rewrite_bank(src):
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
            data = re.sub(re.escape(OLD_PREFIX), NEW_PREFIX, data, flags=re.I)
        out.append(chunk(tag, data))
    if seen != 1:
        raise SystemExit("bank has %d BTNW chunks" % seen)
    return chunk(b"FORM", b"EPUS" + b"".join(out))


def main():
    if len(sys.argv) != 5:
        raise SystemExit(__doc__)
    table, ttf, bank, dest = sys.argv[1:]
    font = TTFont(ttf)
    by_name = json.loads((pathlib.Path(ttf).parent / "codepoints.json").read_text())
    dest = pathlib.Path(dest)
    (dest / "Images").mkdir(parents=True, exist_ok=True)
    (dest / "Buttons").mkdir(parents=True, exist_ok=True)
    for name, g in parse_map(table):
        if g not in by_name:
            raise SystemExit("%s: unknown Lucide glyph: %s" % (table, g))
        (dest / "Images" / name).write_bytes(glyph(font, by_name[g]))
    (dest / "Buttons" / "Amarchy-toolbar").write_bytes(rewrite_bank(pathlib.Path(bank).read_bytes()))
    print("[OK] %d images and Buttons/Amarchy-toolbar in %s" % (len(parse_map(table)), dest))


if __name__ == "__main__":
    main()
