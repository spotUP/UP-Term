#!/usr/bin/env python3
"""JetBrains Mono as an Amiga bitmap strike font (JetBrainsMono.font 16), for the Amarchy drawn menu on
square-pixel screens (Omarchy's font). Usage: gen_jbmono.py JetBrainsMono-Regular.ttf OUTDIR
writes OUTDIR/JetBrainsMono.font and OUTDIR/JetBrainsMono/16.

Characters 32-255 (Latin-1), fixed width, ysize 16. The face is drawn at 12 px (ascent 1020 + descent 300 of
1000 units = 15.84 rows, so the cell is 16 high; the advance 600 units = 7.2 px, so the cell is 7 wide),
each glyph on its own 7x16 cell, anti-aliased and cut at THRESHOLD so a 1.1 px stem is one clean pixel
column. The file layout is the one dist/fonts/IBM/16 has: a loadseg-able hunk holding a DiskFontHeader,
the TextFont, its name, the strike (CharData), and the CharLoc table (lo..hi plus the default glyph)."""
import os, struct, sys
from PIL import Image, ImageDraw, ImageFont

PX = 12                 # pixel size of the face: 16-row cell
YSIZE = 16
LO, HI = 32, 255
THRESHOLD = 96          # of 255; keeps JetBrains Mono's ~1.1 px stems whole at 12 px
NAME = 'JetBrainsMono.font'


def cell_width(font):
    return int(round(font.getlength('M')))


def glyphs(ttf):
    f = ImageFont.truetype(ttf, PX, layout_engine=ImageFont.Layout.BASIC)
    asc, _desc = f.getmetrics()
    w = cell_width(f)
    out = {}
    for cp in range(LO, HI + 1):
        im = Image.new('L', (w, YSIZE), 0)
        if cp not in (0x7f,) and not 0x80 <= cp <= 0x9f and cp != 0xa0 and cp != 0xad:
            ImageDraw.Draw(im).text((0, asc), chr(cp), font=f, fill=255, anchor='ls')
        out[cp] = [[1 if im.getpixel((x, y)) >= THRESHOLD else 0 for x in range(w)] for y in range(YSIZE)]
    return out, w, asc


def build(ttf):
    gl, w, asc = glyphs(ttf)
    n = HI - LO + 1
    total = n * w
    modulo = (total + 15) // 16 * 2
    strike = bytearray(modulo * YSIZE)
    for i, cp in enumerate(range(LO, HI + 1)):
        for y in range(YSIZE):
            for x in range(w):
                if gl[cp][y][x]:
                    b = i * w + x
                    strike[y * modulo + b // 8] |= 0x80 >> (b % 8)
    name = NAME.encode() + b'\0'
    TF = 58
    name_off = TF + 52
    chardata = (name_off + len(name) + 3) // 4 * 4
    loc = chardata + len(strike)
    locs = b''.join(struct.pack('>HH', i * w, w) for i in range(n))
    locs += struct.pack('>HH', 0, w)                      # the default glyph: the first cell (space)
    code = bytearray()
    code += b'\x70\xff\x4e\x75'                           # moveq #-1,d0 ; rts
    code += struct.pack('>IIBBI', 0, 0, 12, 0, name_off)  # dfh LN: succ, pred, NT_FONT, pri, name
    code += struct.pack('>HHI', 0x0f80, 0, 0) + name.ljust(32, b'\0')   # FileID, Revision, Segment, Name
    assert len(code) == TF
    code += struct.pack('>IIBBI', 0, 0, 0, 0, name_off)   # TextFont mn_Node
    code += struct.pack('>IH', 0, 0)                      # ReplyPort, Length
    code += struct.pack('>HBBHHHHBB', YSIZE, 0, 0x40, w, asc - 1, 1, 0, LO, HI)
    code += struct.pack('>IHIII', chardata, modulo, loc, 0, 0)
    assert len(code) == name_off
    code += name.ljust(chardata - name_off, b'\0')
    code += strike + locs
    code += b'\0' * (-len(code) % 4)
    size = len(code) // 4
    d = struct.pack('>IIIIII', 0x3f3, 0, 1, 0, 0, size)
    d += struct.pack('>II', 0x3e9, size) + bytes(code)
    relocs = [14, TF + 10, TF + 34, TF + 40]
    d += struct.pack('>III', 0x3ec, len(relocs), 0) + b''.join(struct.pack('>I', r) for r in relocs) + struct.pack('>I', 0)
    d += struct.pack('>I', 0x3f2)
    return d, w, asc


def contents():
    e = b'JetBrainsMono/16'.ljust(256, b'\0') + struct.pack('>HBB', YSIZE, 0, 0x42)
    return struct.pack('>HH', 0x0f00, 1) + e


def main(ttf, outdir):
    d, w, asc = build(ttf)
    os.makedirs(os.path.join(outdir, 'JetBrainsMono'), exist_ok=True)
    open(os.path.join(outdir, 'JetBrainsMono', '16'), 'wb').write(d)
    open(os.path.join(outdir, NAME), 'wb').write(contents())
    print('JetBrainsMono 16: cell %dx%d, baseline row %d' % (w, YSIZE, asc - 1))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
