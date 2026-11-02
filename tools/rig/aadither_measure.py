#!/usr/bin/env python3
"""aadither_measure.py -- R6.A3: every antialiased edge pixel of an icon in an aadither_rig.py screenshot, against the pen
the host model (tools/rig/aadither_expect.c) expects. Pixels are read from the screenshot's RGB and matched to the
16-pen AGA Workbench palette exactly (never nearest: pen 5 f0f0ff and pen 2 ffffff are different pens).

  python3 tools/rig/aadither_measure.py <out-prefix> <expect-binary> <font.ttf> <hex code point>:<x0> ... [y0]
    e.g. aadither_measure.py build/aad build/aadither_expect SymbolsNerdFontMono-Regular.ttf E0A0:20 F07B:44
cc -I . -o build/aadither_expect tools/rig/aadither_expect.c render/ttf.c render/emoji.c render/unifont.c
The cell is 16x8 pixels (an icon is two cells wide; AGA hires PAL pixels are 1:2), baseline 7, nshades 8; the dither
tile is read from the blank cell to the left (period 4 across). Exit 1 when any pixel differs.
"""
import struct, subprocess, sys

# rig 1's 16-colour Workbench (tests/test_render.c wb16)
PAL = [0xaaaaaa, 0x000000, 0xffffff, 0x6688bb, 0x00ff9f, 0xf0f0ff, 0xd2d2e1, 0xebebeb,
       0x4473ff, 0xffff00, 0x7f007f, 0xefaf4f, 0xee4444, 0x55dd55, 0x0044dd, 0xee9900]
Y0 = 50
CW, CH, BASE, RX, RY, NSHADES = 16, 8, 7, 1, 2, 8


def main():
    prefix, expect, font = sys.argv[1:4]
    jobs = [a.split(':') for a in sys.argv[4:]]
    data = open(prefix + '.rgb', 'rb').read()
    w, h = struct.unpack('>HH', data[:4])
    px = data[4:]

    def pen(x, y):
        o = (y * w + x) * 3
        c = int.from_bytes(px[o:o + 3], 'big')
        return PAL.index(c) if c in PAL else -1

    bad = 0
    for cp, x0 in jobs:
        x0 = int(x0)
        tile = [[pen(4 + (x % 4), Y0 + y) for x in range(CW)] for y in range(CH)]
        spec = 'pal 16 ' + ' '.join('%06x' % c for c in PAL) + '\nfg ffffff\n' + \
               '\n'.join(''.join('%x' % t for t in r) for r in tile) + '\n'
        out = subprocess.run([expect, font, cp, str(CW), str(CH), str(BASE), str(RX), str(RY), str(NSHADES)],
                             input=spec, capture_output=True, text=True, check=True).stdout
        n = diff = 0
        for ln in out.splitlines():
            x, y, cov, t, e, _ideal = map(int, ln.split())
            m = pen(x0 + x, Y0 + y)
            n += 1
            if m != e:
                diff += 1
                print('  %s x%d y%d cov%d tile%d expected%d measured%d' % (cp, x, y, cov, t, e, m))
        print('%s: %d edge pixels, %d differ' % (cp, n, diff))
        bad += diff
    sys.exit(1 if bad else 0)


main()
