#!/usr/bin/env python3
"""aadither_rig.py -- R6.A3: antialiased outline glyph edges over a DITHERED cell background on a screen of 16 pens.

Rig on `rig.py aga` (16 pens), handler installed, VTC:SymbolsNerdFontMono.ttf present (outline_rig.py fetches it).
A window prints icons from the outline font over a 48;5;244 background (a grey no pen holds: the cell is dithered)
in white, with profile `outline-blend-dest = on|off`. Writes the screenshot as raw RGB (<out>.rgb, 4 bytes
w,h big-endian then pixels) and PNG (<out>.png). The measurement is tools/rig/aadither_measure.py.

  UPTERM_RIG=<n> python3 tools/rig/aadither_rig.py <on|off> <out-prefix> [bg-index]
"""
import os, pathlib, shutil, struct, sys, time, zlib
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / 'vtc'
FACE = 'SymbolsNerdFontMono'
ICONS = [0xE0A0, 0xF07B, 0xF121, 0xE7A8, 0xF015, 0xE712, 0xF09B, 0xF1D3]
WX, WY, WW, WH = 0, 20, 400, 100


def png(path, w, h, px):
    raw = b''.join(b'\0' + bytes(px[y * w * 3:(y + 1) * w * 3]) for y in range(h))
    def ch(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d))
    pathlib.Path(path).write_bytes(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
                                   + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))


def main():
    mode, out = sys.argv[1], sys.argv[2]
    bg = sys.argv[3] if len(sys.argv) > 3 else '244'
    shutil.copyfile(ROOT / 'build/amiga/UPTerm', VTC / 'UPTerm')
    # two leading blanks (the tile alone), then the icons with a blank between each, all on the background
    line = '\x1b[38;5;231;48;5;%sm  %s \x1b[m\n' % (bg, ' '.join(chr(i) for i in ICONS))
    (VTC / 'aad-line.txt').write_bytes(line.encode('utf-8'))
    (VTC / 'aad.conf').write_text('[profile default]\nfont-fallback = %s\noutline-blend-dest = %s\n' % (FACE, mode))
    c.run('MakeDir ENV:up-term QUIET')
    c.run('MakeDir RAM:f1fonts QUIET')
    c.run('Assign FONTS: RAM:f1fonts ADD')
    c.run('Copy VTC:%s.ttf RAM:f1fonts/ QUIET' % FACE, 120)
    c.run('Copy VTC:aad.conf ENV:up-term/up-term QUIET')
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/aad%s/CLOSE/XTERM"' % (WX, WY, WW, WH, mode))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 200) * kx), int((WY + 60) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:aad-line.txt')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(6)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    px = b[8:]
    pathlib.Path(out + '.rgb').write_bytes(struct.pack('>HH', w, h) + bytes(px))
    png(out + '.png', w, h, px)
    ami.req(0x08, bytes([4]) + b'UPTerm state outline-aa >VTC:aad-state.txt')
    ami.key(0x44)
    time.sleep(3)
    print('mode', mode, 'shot', w, h, 'state outline-aa', (VTC / 'aad-state.txt').read_text().split()[:1] if (VTC / 'aad-state.txt').exists() else None)
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)


main()
