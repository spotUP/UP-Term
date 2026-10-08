#!/usr/bin/env python3
"""cube_rig.py -- the xterm 256-colour cube and grey ramp in an XCON window
on the rig (RTG, 24-bit), every cell compared with the xterm palette
(levels 0/95/135/175/215/255, greys 8 + 10n). Indexed colours took a pen
each from the screen's colour map until it ran out (~150), then "best
pen" gave Workbench colours: 60 of 240 cells wrong. Rig up, `make amiga`,
VTC:vsh and the handler installed; the kit must not be installed (its
DOSDrivers XCON would mount L:vtcon-handler first)."""
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
    ami.main(['shot', str(SHOT)])
    return check(Image.open(SHOT).convert('RGB'), 300, 600)

def check(im, y0, y1):
    """Find the cube (its first cell, colour 16 = 0/0/95) at x 12 between
    rows y0 and y1 and compare all 240 cells; prints ok/FAIL, 0 on a pass."""
    top = next((y for y in range(y0, y1) if im.getpixel((12, y))[:2] < (20, 20)
                and abs(im.getpixel((12, y))[2] - 95) < 20), None)
    if top is None:
        print('FAIL no cube found'); return 1
    while im.getpixel((12, top - 1)) == im.getpixel((12, top)):
        top -= 1
    bad = []
    for i in range(16, 256):
        row, col = ((i - 16) // 36, (i - 16) % 36) if i < 232 else (6, (i - 232) * 2)
        got = im.getpixel((col * 8 + 4, top + row * 8 + 4))
        if max(abs(a - b) for a, b in zip(got, expect(i))) > 12:
            bad.append((i, got, expect(i)))
    print('%s %d of 240 cells as the xterm palette' % ('ok' if not bad else 'FAIL', 240 - len(bad)))
    for b in bad[:8]:
        print('  cell %d: %s, want %s' % b)
    return 1 if bad else 0

if __name__ == '__main__':
    sys.exit(main())
