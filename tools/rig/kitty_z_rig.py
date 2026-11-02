#!/usr/bin/env python3
"""kitty_z_rig.py -- K30: a kitty picture under the text (z<0) through the handler.
Types a stream into an XCON: window: a 64x16 solid red picture with z=-1 at row 3 column 3 (C=1: the cursor
stays), then the text "MMMMMMMM" over it. Takes a screenshot (<out>.png) and measures, inside the red picture's
box, the red pixels and the pixels of any other colour (the glyphs), and the same for the first cell alone.
PASS: the box holds red and glyph pixels, and the first cell holds both (the M over the picture). Before K30 the
picture blanked the cells, so the box was red only.
  UPTERM_RIG=<n> python3 tools/rig/kitty_z_rig.py <out-prefix>
"""
import base64, pathlib, struct, sys, time, zlib
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c

VTC = paths.RIG / 'vtc'
W = H = 16
WX, WY, WW, WH = 0, 20, 400, 200


def stripes(cols):
    return b''.join(bytes(cols[(y // 4) % 2]) * W for y in range(H))


def apc(ctrl, data):
    return b'\x1b_G' + ctrl.encode() + b';' + base64.b64encode(data) + b'\x1b\\'


def png(w, h, rgb):
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def main():
    out = sys.argv[1]
    pic = bytes((255, 0, 0)) * (64 * 16)
    stream = (b'\x1b[2J\x1b[3;3H'
              + apc('a=T,f=24,s=64,v=16,i=1,p=1,z=-1,C=1,q=2', pic)
              + b'MMMMMMMM\x1b[8;1H')
    (VTC / 'kz-stream.bin').write_bytes(stream)
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/kz/CLOSE/XTERM"' % (WX, WY, WW, WH))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 200) * kx), int((WY + 100) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:kz-stream.bin')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(6)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    px = bytes(b[8:8 + w * h * 3])
    pathlib.Path(out + '.png').write_bytes(png(w, h, px))
    def red(p):
        return p[0] > 180 and p[1] < 80 and p[2] < 80
    xs, ys = [], []
    for i in range(0, len(px) - 2, 3):
        if red(px[i:i + 3]):
            xs.append((i // 3) % w)
            ys.append((i // 3) // w)
    print('shot', w, h)
    if not xs:
        print('FAIL no red picture')
        return
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    def count(ax, ay, bx, by):
        r = o = 0
        cols = {}
        for y in range(ay, by + 1):
            for x in range(ax, bx + 1):
                p = px[(y * w + x) * 3:(y * w + x) * 3 + 3]
                if red(p):
                    r += 1
                else:
                    o += 1
                    cols[tuple(p)] = cols.get(tuple(p), 0) + 1
        return r, o, cols
    r, o, cols = count(x0, y0, x1, y1)
    print('picture box x %d..%d y %d..%d: red %d glyph %d glyph colours %s' % (x0, x1, y0, y1, r, o, sorted(cols.items(), key=lambda kv: -kv[1])[:3]))
    cw = (x1 - x0 + 1) // 8  # the picture is 8 cells wide (64 px at 8 px a cell)
    r1, o1, _ = count(x0, y0, x0 + cw - 1, y1)
    print('first cell x %d..%d: red %d glyph %d' % (x0, x0 + cw - 1, r1, o1))
    print('PASS' if r1 and o1 and o else 'FAIL')
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)


main()
