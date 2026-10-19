#!/usr/bin/env python3
"""Writes tests/kitty_png_vectors.h: small real PNG files for the engine's own
decoder (engine/png.c) and the 0xAARRGGBB words each must give. Every filter
type 0-4 is used, IDAT is split where noted. Run: python3 tools/png_vectors.py"""
import base64, struct, sys, zlib

def chunk(t, d):
    c = struct.pack(">I", len(d)) + t + d
    return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)

def paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c

def filt(rows, bpp, order):
    out, prev = b"", bytes(len(rows[0]))
    for y, row in enumerate(rows):
        f = order[y % len(order)]
        o = bytearray()
        for i, x in enumerate(row):
            a = row[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            p = [0, a, b, (a + b) // 2, paeth(a, b, c)][f]
            o.append((x - p) & 255)
        out += bytes([f]) + bytes(o)
        prev = row
    return out

def png(w, h, depth, ctype, rows, bpp, order=(0, 1, 2, 3, 4), plte=None, trns=None,
        split=False, interlace=0, raw=None):
    z = zlib.compress(raw if raw is not None else filt(rows, bpp, order))
    s = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, interlace))
    if plte: s += chunk(b"PLTE", plte)
    if trns: s += chunk(b"tRNS", trns)
    if split:
        k = len(z) // 2
        s += chunk(b"IDAT", z[:k]) + chunk(b"IDAT", z[k:])
    else:
        s += chunk(b"IDAT", z)
    return s + chunk(b"IEND", b"")

def pack(vals, depth):
    bits = "".join(format(v, "0%db" % depth) for v in vals)
    bits += "0" * (-len(bits) % 8)
    return bytes(int(bits[i:i + 8], 2) for i in range(0, len(bits), 8))

def rowb(r):
    return bytes(sum(r, ()))

V = []  # (name, png bytes, w, h, expected words)

# type 0 depth 8, 3 x 5, filters 0..4
g = [[10 * (3 * y + x) + 5 for x in range(3)] for y in range(5)]
V.append(("grey8", png(3, 5, 8, 0, [bytes(r) for r in g], 1), 3, 5,
          [0xFF000000 | v * 0x010101 for r in g for v in r]))
# type 0 depth 1, 10 x 2 (a row is two bytes), filters 2, 3
g1 = [[(x + y) % 2 for x in range(10)] for y in range(2)]
V.append(("grey1", png(10, 2, 1, 0, [pack(r, 1) for r in g1], 1, order=(2, 3)), 10, 2,
          [0xFF000000 | v * 0xFFFFFF for r in g1 for v in r]))
# type 0 depth 16 with tRNS (the key 0x1234 is clear), 2 x 1: high byte kept
V.append(("grey16", png(2, 1, 16, 0, [struct.pack(">HH", 0x8001, 0x1234)], 2, trns=struct.pack(">H", 0x1234)),
          2, 1, [0xFF808080, 0x00121212]))
# type 2 depth 8, 3 x 5, filters 0..4, tRNS colour key (1,2,3) at (0,0)
c = [[(7 * (3 * y + x) + 1, 11 * (3 * y + x) + 2, 13 * (3 * y + x) + 3) for x in range(3)] for y in range(5)]
c[0][0] = (1, 2, 3)
V.append(("rgb8", png(3, 5, 8, 2, [rowb(r) for r in c], 3, trns=struct.pack(">HHH", 1, 2, 3)), 3, 5,
          [(0 if p == (1, 2, 3) else 0xFF) << 24 | p[0] << 16 | p[1] << 8 | p[2] for r in c for p in r]))
# type 2 depth 16, 2 x 2: the high bytes, no key
c16 = [[(0xFF00, 0x0080, 0x40FF), (0x1234, 0xABCD, 0x0001)], [(0, 0, 0), (0xFFFF, 0xFFFF, 0xFFFF)]]
V.append(("rgb16", png(2, 2, 16, 2, [b"".join(struct.pack(">HHH", *p) for p in r) for r in c16], 6, order=(4, 4)), 2, 2,
          [0xFF000000 | (p[0] >> 8) << 16 | (p[1] >> 8) << 8 | p[2] >> 8 for r in c16 for p in r]))
# type 3 depth 2, 5 x 2, PLTE of 4, tRNS of 2 entries (the rest opaque), IDAT split in two
pal = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255)]
ix = [[0, 1, 2, 3, 0], [3, 2, 1, 0, 3]]
al = [0, 128]
V.append(("pal2", png(5, 2, 2, 3, [pack(r, 2) for r in ix], 1, order=(1, 4), plte=bytes(sum(pal, ())),
                      trns=bytes(al), split=True), 5, 2,
          [(al[i] if i < 2 else 255) << 24 | pal[i][0] << 16 | pal[i][1] << 8 | pal[i][2] for r in ix for i in r]))
# type 4 depth 8, 3 x 2, filters 3, 4
ga = [[(40 * x + 20 * y, 255 - 100 * x) for x in range(3)] for y in range(2)]
V.append(("grey_alpha8", png(3, 2, 8, 4, [rowb(r) for r in ga], 2, order=(3, 4)), 3, 2,
          [p[1] << 24 | p[0] * 0x010101 for r in ga for p in r]))
# type 6 depth 8, 3 x 5, filters 0..4, split IDAT
rg = [[(50 * x, 40 * y, 250 - 12 * (3 * y + x), 255 - 30 * y) for x in range(3)] for y in range(5)]
V.append(("rgba8", png(3, 5, 8, 6, [rowb(r) for r in rg], 4, split=True), 3, 5,
          [p[3] << 24 | p[0] << 16 | p[1] << 8 | p[2] for r in rg for p in r]))
# the engine entry point: 2 x 2 type 2, channels 0 and 255 only (exact on the colour cube)
e = [[(255, 0, 0), (0, 255, 0)], [(0, 0, 255), (255, 255, 255)]]
V.append(("rgb_cube", png(2, 2, 8, 2, [rowb(r) for r in e], 3), 2, 2,
          [0xFF000000 | p[0] << 16 | p[1] << 8 | p[2] for r in e for p in r]))
# interlaced (Adam7): not supported, w and h are the header's
V.append(("interlaced", png(2, 2, 8, 2, None, 3, interlace=1, raw=bytes(14)), 2, 2, []))
# a signature and a header, then zlib that is not
bad = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 2, 2, 8, 2, 0, 0, 0)) + chunk(b"IDAT", b"not zlib at all") + chunk(b"IEND", b"")
V.append(("broken", bad, 2, 2, []))

o = ["/* Generated by tools/png_vectors.py: real PNG files and their pixels. */"]
for n, d, w, h, ex in V:
    o.append("static const unsigned char png_%s[] = {%s};" % (n, ",".join(str(b) for b in d)))
    o.append('static const char png_%s_b64[] = "%s";' % (n, base64.b64encode(d).decode()))
    o.append("static const unsigned long png_%s_px[] = {%s};" % (n, ",".join("0x%08XUL" % p for p in ex) or "0"))
open(sys.argv[1] if len(sys.argv) > 1 else "tests/kitty_png_vectors.h", "w").write("\n".join(o) + "\n")
