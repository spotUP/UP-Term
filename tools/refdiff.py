#!/usr/bin/env python3
"""Diff the xterm personality against two reference emulators.

Every stream in tests/streams/ (<name>.<cols>x<rows>.bin, default 80x24) is
fed to build/vtdump (ours), build/vterm_dump (libvterm, neovim's terminal)
and pyte. Text, cursor, colours and bold/inverse/underline are compared.

The engine must match libvterm. pyte is the second opinion: where the two
references disagree the case is listed in DISAGREE with what xterm's
ctlseqs says and which one we follow -- never silently skipped.
"""
import pathlib, re, subprocess, sys
import pyte

ROOT = pathlib.Path(__file__).resolve().parent.parent
# stream name -> reason; the engine follows libvterm on these, pyte differs.
DISAGREE = {
}

class RefScreen(pyte.Screen):
    """pyte with the one crash it has on real traffic removed: private SGR
    (CSI > 4 ; 2 m, xterm modifyOtherKeys, which vim sends) is not SGR."""
    def select_graphic_rendition(self, *attrs, private=False):
        if private:
            return
        super().select_graphic_rendition(*attrs)

def dump(tool, data, cols, rows):
    out = subprocess.run([str(ROOT / "build" / tool), str(cols), str(rows)],
                         input=data, capture_output=True, check=True).stdout.decode("utf-8", "replace")
    lines = out.split("\n")
    text = lines[:rows]
    attr = []
    for y in range(rows):
        row = []
        for cell in lines[rows + y].split(" "):
            fg, bg, a = (int(v) for v in cell.split(","))
            row.append((fg, bg, bool(a & 1), bool(a & 0x20), bool(a & 8)))
        attr.append(row)
    return text, lines[2 * rows], attr

def compare(a, b, cols, rows):
    """Differences between two dumps: (rows with other text, cursor pair, cells)."""
    bad = [y for y in range(rows) if a[0][y] != b[0][y]]
    cells = [(x, y, a[2][y][x], b[2][y][x]) for y in range(rows) for x in range(cols)
             if a[2][y][x] != b[2][y][x]]
    return bad, (a[1], b[1]) if a[1] != b[1] else None, cells

def pyte_dump(data, cols, rows):
    scr = RefScreen(cols, rows)
    st = pyte.ByteStream(scr)
    st.feed(data)
    text = [scr.display[y] for y in range(rows)]
    cur = "@%d,%d" % (min(scr.cursor.x, cols - 1), scr.cursor.y)
    attr = [[(colour(c.fg), colour(c.bg), bool(c.bold), bool(c.reverse), bool(c.underscore))
             for c in (scr.buffer[y][x] for x in range(cols))] for y in range(rows)]
    return text, cur, attr

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

def report(label, diff, ours, ref):
    bad, cur, cells = diff
    for y in bad[:6]:
        print("  row %2d ours    [%s]\n         %-7s [%s]" % (y, ours[0][y].rstrip(), label, ref[0][y].rstrip()))
    if cur:
        print("  cursor ours %s %s %s" % (cur[0], label, cur[1]))
    for x, y, got, want in cells[:4]:
        print("  cell %d,%d ours %s %s %s  (fg, bg, bold, inverse, underline)" % (x, y, got, label, want))
    if len(cells) > 4:
        print("  ... %d cells differ in colour or attributes" % len(cells))

def main():
    only = sys.argv[1] if len(sys.argv) > 1 else None
    fails = ran = 0
    for p in sorted((ROOT / "tests/streams").glob("*.bin")):
        if only and only not in p.name:
            continue
        ran += 1
        m = re.search(r"\.(\d+)x(\d+)\.bin$", p.name)
        cols, rows = (int(m.group(1)), int(m.group(2))) if m else (80, 24)
        data = p.read_bytes()
        ours = dump("vtdump", data, cols, rows)
        vt = dump("vterm_dump", data, cols, rows)
        py = pyte_dump(data, cols, rows)
        dv = compare(ours, vt, cols, rows)
        dp = compare(ours, py, cols, rows)
        vt_ok = not (dv[0] or dv[1] or dv[2])
        py_ok = not (dp[0] or dp[1] or dp[2])
        if vt_ok and py_ok:
            print("[OK] %s" % p.name)
        elif vt_ok:
            if p.name in DISAGREE:
                print("[OK] %s (pyte differs: %s)" % (p.name, DISAGREE[p.name]))
            else:
                fails += 1
                print("[FAIL] %s: matches libvterm, pyte differs and DISAGREE has no entry" % p.name)
                report("pyte", dp, ours, py)
        else:
            fails += 1
            print("[FAIL] %s: differs from libvterm%s" % (p.name, "" if not py_ok else " (pyte agrees with us)"))
            report("libvterm", dv, ours, vt)
    print("%d streams, %d failed" % (ran, fails))
    return 1 if fails else 0

if __name__ == "__main__":
    sys.exit(main())
