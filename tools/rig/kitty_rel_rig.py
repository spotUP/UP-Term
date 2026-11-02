#!/usr/bin/env python3
"""kitty_rel_rig.py -- K29: a kitty relative placement through the handler.

Types a stream into an XCON: window: a 16x16 red/yellow parent (i=1,p=1) at the cursor, then a 16x16 green/blue
child (i=2,p=1,P=1,Q=1,H=2,V=1), then the text "<-cursor". Takes a screenshot (<out>.png) and prints coloured-pixel
counts and the boxes they cover.

PASS: the child's top left sits 2 columns right of and 1 row below the parent's top left (diagonally touching the
parent's bottom-right corner for a 2-column parent), and "<-cursor" follows the parent on the parent's row: the
child did not move the cursor.

  UPTERM_RIG=<n> python3 tools/rig/kitty_rel_rig.py <out-prefix>
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
    stream = (b'\x1b[2J\x1b[3;3H'
              + apc('a=T,f=24,s=%d,v=%d,i=1,p=1,q=2' % (W, H), stripes([(255, 0, 0), (255, 255, 0)]))
              + apc('a=T,f=24,s=%d,v=%d,i=2,p=1,P=1,Q=1,H=2,V=1,q=2' % (W, H), stripes([(0, 255, 0), (0, 0, 255)]))
              + b'<-cursor\x1b[8;1H')
    (VTC / 'kr-stream.bin').write_bytes(stream)
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/krel/CLOSE/XTERM"' % (WX, WY, WW, WH))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((WX + 200) * kx), int((WY + 100) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:kr-stream.bin')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(6)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    px = bytes(b[8:8 + w * h * 3])
    pathlib.Path(out + '.png').write_bytes(png(w, h, px))
    tg = {'red': (255, 0, 0), 'yellow': (255, 255, 0), 'green': (0, 255, 0), 'blue': (0, 0, 255)}
    box = {}
    for i in range(0, len(px) - 2, 3):
        p = px[i:i + 3]
        for n, t in tg.items():
            if all(abs(p[j] - t[j]) < 40 for j in range(3)):
                x, y = (i // 3) % w, (i // 3) // w
                b0 = box.get(n, [x, y, x, y, 0])
                box[n] = [min(b0[0], x), min(b0[1], y), max(b0[2], x), max(b0[3], y), b0[4] + 1]
    print('shot', w, h)
    for n, v in box.items():
        print('%-6s pixels %5d box x %d..%d y %d..%d' % (n, v[4], v[0], v[2], v[1], v[3]))
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)


main()
