#!/usr/bin/env python3
"""Diff the xterm personality against pyte on every stream in tests/streams/.

Each stream is fed to build/vtdump and to pyte at the size in its name
(<name>.<cols>x<rows>.bin, default 80x24); the text grids and the cursor
must match. pyte is a reference, not an oracle: a known pyte defect is
listed in KNOWN_PYTE with the reason, never silently skipped.
"""
import pathlib, re, subprocess, sys
import pyte

ROOT = pathlib.Path(__file__).resolve().parent.parent
KNOWN_PYTE = {
}

class RefScreen(pyte.Screen):
    """pyte with the one crash it has on real traffic removed: private SGR
    (CSI > 4 ; 2 m, xterm modifyOtherKeys, which vim sends) is not SGR."""
    def select_graphic_rendition(self, *attrs, private=False):
        if private:
            return
        super().select_graphic_rendition(*attrs)

def run(path, cols, rows):
    data = path.read_bytes()
    out = subprocess.run([str(ROOT / "build/vtdump"), str(cols), str(rows)],
                         input=data, capture_output=True, check=True).stdout.decode("utf-8")
    lines = out.split("\n")
    ours = lines[:rows]
    oattr = [[tuple(int(v) for v in cell.split(",")) for cell in lines[rows + y].split(" ")]
             for y in range(rows)]
    ocur = lines[2 * rows]
    scr = RefScreen(cols, rows)
    st = pyte.ByteStream(scr)
    st.feed(data)
    ref = [scr.display[y] for y in range(rows)]
    rcur = "@%d,%d" % (min(scr.cursor.x, cols - 1), scr.cursor.y)
    bad_attr = []
    for y in range(rows):
        for x in range(cols):
            c = scr.buffer[y][x]
            want = (colour(c.fg), colour(c.bg), bool(c.bold), bool(c.reverse), bool(c.underscore))
            fg, bg, a = oattr[y][x]
            got = (fg, bg, bool(a & 1), bool(a & 0x20), bool(a & 8))
            if want != got:
                bad_attr.append((x, y, got, want))
    return ours, ocur, ref, rcur, bad_attr

NAMES = ["black", "red", "green", "brown", "blue", "magenta", "cyan", "white"]
DEFAULT, RGB = 0x100, 0x8000

def colour(v):
    """pyte's colour value in the engine's encoding."""
    if v == "default":
        return DEFAULT
    if v in NAMES:
        return NAMES.index(v)
    if v.startswith("bright") and v[6:] in NAMES:
        return 8 + NAMES.index(v[6:])
    if v.startswith("bright") and v[6:] == "yellow":
        return 11
    r, g, b = int(v[0:2], 16), int(v[2:4], 16), int(v[4:6], 16)
    hexv = v.lower()
    if hexv in PALETTE:
        return PALETTE.index(hexv)
    return RGB | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)

import pyte.graphics
PALETTE = [c.lower() for c in pyte.graphics.FG_BG_256]

def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    fails = 0
    streams = sorted((ROOT / "tests/streams").glob("*.bin"))
    for p in streams:
        if only and only not in p.name:
            continue
        m = re.search(r"\.(\d+)x(\d+)\.bin$", p.name)
        cols, rows = (int(m.group(1)), int(m.group(2))) if m else (80, 24)
        ours, ocur, ref, rcur, bad_attr = run(p, cols, rows)
        bad = [y for y in range(rows) if ours[y] != ref[y]]
        if bad or ocur != rcur or bad_attr:
            if p.name in KNOWN_PYTE:
                print("[KNOWN] %s: %s" % (p.name, KNOWN_PYTE[p.name]))
                continue
            fails += 1
            print("[FAIL] %s" % p.name)
            for y in bad[:6]:
                print("  row %2d ours [%s]\n         pyte [%s]" % (y, ours[y].rstrip(), ref[y].rstrip()))
            if ocur != rcur:
                print("  cursor ours %s pyte %s" % (ocur, rcur))
            for x, y, got, want in bad_attr[:6]:
                print("  cell %d,%d ours %s pyte %s  (fg, bg, bold, inverse, underline)" % (x, y, got, want))
            if len(bad_attr) > 6:
                print("  ... %d cells differ in colour or attributes" % len(bad_attr))
        else:
            print("[OK] %s" % p.name)
    print("%d streams, %d failed" % (len(streams), fails))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
