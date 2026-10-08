#!/usr/bin/env python3
"""cube_rig.py -- the xterm 256-colour cube and grey ramp in an XCON window
on the rig (RTG, 24-bit), every cell compared with the xterm palette
(levels 0/95/135/175/215/255, greys 8 + 10n). Indexed colours took a pen
each from the screen's colour map until it ran out (~150), then "best
pen" gave Workbench colours: 60 of 240 cells wrong. Rig up, `make amiga`,
VTC:vsh and the handler installed; the kit must not be installed (its
DOSDrivers XCON would mount L:vtcon-handler first).

The cell size is read from the shot, not assumed: on the rig's RTG screen
(square pixels) topaz is drawn as TopazPro 16, 8x16 cells, and a check
that stepped 8 pixels a row read row 0 again for every row and passed only
its 36 cells ("FAIL 36 of 240", read as 36 wrong; the colours were right).
The shot is retaken until the cube passes or CUBE_WAIT seconds go by: the
sh loop of colors.sh was still drawing the grey ramp at 4 s."""
import os, pathlib, shutil, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
from PIL import Image

ROOT = pathlib.Path(__file__).resolve().parents[2]
SHOT = paths.RIG / "shots/cube.png"
LV = [0, 95, 135, 175, 215, 255]

def expect(i):
    if i < 232:
        k = i - 16
        return (LV[k // 36], LV[(k // 6) % 6], LV[k % 6])
    g = 8 + (i - 232) * 10
    return (g, g, g)

def main():
    shutil.copyfile(ROOT / "tests/amiga/colors.sh", paths.RIG / "vtc/colors.sh")
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/300/780/280/cube/CLOSE"')
    time.sleep(4)
    for s in ('VTC:vsh', 'source VTC:colors.sh'):
        ami.req(0x08, bytes([4]) + s.encode()); time.sleep(0.4); ami.key(0x44); time.sleep(4)
    rc = shoot_check(SHOT, 300, 600, float(os.environ.get("CUBE_WAIT", "30")))
    if "--keep" not in sys.argv:
        for s in ('exit', 'endcli'):  # vsh, then the window's shell: it closes
            ami.req(0x08, bytes([4]) + s.encode()); time.sleep(0.4); ami.key(0x44); time.sleep(2)
    return rc

def shoot_check(shot, y0, y1, wait):
    """Screenshots into shot until check passes or wait seconds have gone
    by (the cube is drawn by a shell loop: slow, slower still in tmux or
    screen); prints and returns the last check's result."""
    end = time.time() + wait
    while True:
        ami.main(['shot', str(shot)])
        im = Image.open(shot).convert('RGB')
        bad = cells_wrong(im, y0, y1)
        if bad == [] or time.time() >= end:  # None: no cube drawn yet
            return report(bad)
        time.sleep(2)

def run_len(im, x, y, dx, dy):
    """How many pixels from (x, y) on in direction (dx, dy) have its colour."""
    c, n = im.getpixel((x, y)), 0
    while 0 <= x < im.width and 0 <= y < im.height and im.getpixel((x, y)) == c:
        x, y, n = x + dx, y + dy, n + 1
    return n

def cells_wrong(im, y0, y1, x=12):
    """The cube's wrong cells as (index, got, want), or None when there is
    no cube. It is found by its second cell, colour 17 (0/0/95; 16 is
    black, as the background), crossing column x between rows y0 and y1;
    that cell's run of pixels gives the cell size (8x8 for topaz 8, 8x16
    for TopazPro 16 on square pixels) and the cube's left edge."""
    top = next((y for y in range(y0, y1) if im.getpixel((x, y))[:2] < (20, 20)
                and abs(im.getpixel((x, y))[2] - 95) < 20), None)
    if top is None:
        return None
    top -= run_len(im, x, top, 0, -1) - 1
    h = run_len(im, x, top, 0, 1)
    left = x - run_len(im, x, top, -1, 0) + 1
    w = run_len(im, left, top, 1, 0)
    x0 = left - w
    bad = []
    for i in range(16, 256):
        row, col = ((i - 16) // 36, (i - 16) % 36) if i < 232 else (6, (i - 232) * 2)
        got = im.getpixel((x0 + col * w + w // 2, top + row * h + h // 2))
        if max(abs(a - b) for a, b in zip(got, expect(i))) > 12:
            bad.append((i, got, expect(i)))
    return bad

def report(bad):
    if bad is None:
        print('FAIL no cube found'); return 1
    print('%s %d of 240 cells as the xterm palette, %d wrong' % ('ok' if not bad else 'FAIL', 240 - len(bad), len(bad)))
    for b in bad[:8]:
        print('  cell %d: %s, want %s' % b)
    return 1 if bad else 0

def check(im, y0, y1):
    """Compare all 240 cells of the cube in im (see cells_wrong); prints
    ok/FAIL, 0 on a pass."""
    return report(cells_wrong(im, y0, y1))

if __name__ == '__main__':
    sys.exit(main())
