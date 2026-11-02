#!/usr/bin/env python3
"""kitty_rgb_rig.py -- K41: kitty RGB pictures draw in true colour on a deep RTG screen.
Types a stream into an XCON: window: (1) a 100x80 raw RGB picture (f=24, a=T, no c=/r=, so 1:1 pixels) whose
8000 pixels are all different colours, (2, 3) a 40x32 RGBA picture, 50% red (alpha 128), over blue-background cells (z=1, C=1: the cells keep their background) and
over the plain window background (K40b software blend). Takes a screenshot (<out>.png) and measures it in Python.
PASS case 1: the picture is found in the shot, >= 90% of the source's distinct colours appear in its rectangle and
  >= 99% of its pixels equal the source RGB. The 8-bit index path cannot show more than 255 colours.
PASS cases 2 and 3: the blended pixel equals (255*a + bg*(255-a) + 127) / 255 per channel (+-1), bg sampled next to it.
  UPTERM_RIG=<n> python3 tools/rig/kitty_rgb_rig.py <out-prefix>
Needs the rig on the 800x600x24 RTG screen (`rig.py rtg`); the script prints the shot size.

AGA mode (K41 AGA half): same raw f=32 stream (picture 1 as RGBA with alpha 255, plus the two 50% alpha pictures),
shot of an aga256 screen (640x256 visible), crop = the XCON window rectangle (0,12,640,240: it holds the picture from the cell origin), written as PNG.
  UPTERM_RIG=<n> python3 tools/rig/kitty_rgb_rig.py aga save <out.png>             (reference run: build before K38)
  UPTERM_RIG=<n> python3 tools/rig/kitty_rgb_rig.py aga compare <ref.png> <out.png>  (PASS = pixel-equal and <= 256 colours)
"""
import base64, pathlib, struct, sys, time, zlib
sys.path.insert(0, __file__.rsplit('/', 1)[0])
import paths
import ami, condev_rig as c
from colour_check import pixels as png_pixels   # PNG reader (RGB8), shared with the colour check

VTC = paths.RIG / 'vtc'
WX, WY, WW, WH = 0, 20, 780, 500
AX, AY, AW, AH = 0, 12, 640, 240   # AGA window: the aga256 shot is 640x256
PW, PH = 100, 80      # picture 1
TW, TH = 40, 32       # picture 2
ALPHA = 128


def src_px(x, y):
    return (32 + x * 2, 32 + y * 2, (x * 3 + y * 5) % 200 + 30)


def apc(ctrl, data):
    b = base64.b64encode(data)
    out, first = b'', True
    for i in range(0, len(b), 4096):
        part = b[i:i + 4096]
        more = 1 if i + 4096 < len(b) else 0
        out += b'\x1b_G' + ((ctrl + ',') if first else '').encode() + b'm=%d;' % more + part + b'\x1b\\'
        first = False
    return out


def png(w, h, rgb):
    raw = b''.join(b'\0' + rgb[y * w * 3:(y + 1) * w * 3] for y in range(h))

    def chunk(t, d):
        return struct.pack('>I', len(d)) + t + d + struct.pack('>I', zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw)) + chunk(b'IEND', b''))


def crop_rgb(px, w, x0, y0, cw, ch):
    return b''.join(px[((y0 + y) * w + x0) * 3:((y0 + y) * w + x0 + cw) * 3] for y in range(ch))


def aga_main(args):
    mode = args[0]
    ref = args[1] if mode == 'compare' else None
    out = args[-1]
    pic1 = b''.join(bytes(src_px(x, y)) + b'\xff' for y in range(PH) for x in range(PW))
    pic2 = bytes((255, 0, 0, ALPHA)) * (TW * TH)
    fill = b''.join(b'\x1b[%d;48H' % r + b' ' * 11 for r in range(2, 7))
    stream = (b'\x1b[2J\x1b[44m' + fill + b'\x1b[0m\x1b[2;2H' + apc('a=T,f=32,s=%d,v=%d,i=1,q=2' % (PW, PH), pic1)
              + b'\x1b[2;50H' + apc('a=T,f=32,s=%d,v=%d,i=2,z=1,C=1,c=5,r=4,q=2' % (TW, TH), pic2)
              + b'\x1b[2;70H' + apc('a=T,f=32,s=%d,v=%d,i=3,q=2' % (TW, TH), pic2) + b'\x1b[30;1H')
    w, h, px = shoot(stream, (AX, AY, AW, AH))
    win = crop_rgb(px, w, AX, AY, AW, AH)
    pathlib.Path(out).write_bytes(png(AW, AH, win))
    cols = len({win[i:i + 3] for i in range(0, len(win), 3)})
    print('aga shot', w, h, 'crop', (AX, AY, AW, AH), 'distinct colours', cols, 'saved', out)
    ok = True
    if ref:
        rw, rh, rat = png_pixels(ref)
        if (rw, rh) != (AW, AH):
            print('compare FAIL: reference size', rw, rh)
            ok = False
        else:
            diff = [(x, y) for y in range(AH) for x in range(AW) if rat(x, y) != tuple(win[(y * AW + x) * 3:(y * AW + x) * 3 + 3])]
            print('compare: differing pixels', len(diff), 'of', AW * AH)
            for x, y in diff[:8]:
                print('  (%d,%d) ref %s new %s' % (x, y, rat(x, y), tuple(win[(y * AW + x) * 3:(y * AW + x) * 3 + 3])))
            ok = not diff
        ok = ok and cols <= 256
        print('distinct colours %d <= 256: %s' % (cols, cols <= 256))
    print('PASS' if ok else 'FAIL')
    finish()
    sys.exit(0 if ok else 1)


def shoot(stream, geom=(WX, WY, WW, WH)):
    gx, gy, gw, gh = geom
    (VTC / 'kr-stream.bin').write_bytes(stream)
    c.run('Run >NIL: NewShell "XCON:%d/%d/%d/%d/kr/CLOSE/XTERM"' % (gx, gy, gw, gh))
    time.sleep(4)
    kx, ky = ami.pointer_scale()
    ami.script(('move', int((gx + gw // 2) * kx), int((gy + gh // 2) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    ami.req(0x08, bytes([4]) + b'Type VTC:kr-stream.bin')
    time.sleep(0.5)
    ami.key(0x44)
    time.sleep(8)
    b = ami.req(0x07)
    w, h = struct.unpack('>HH', b[2:6])
    return w, h, bytes(b[8:8 + w * h * 3])


def finish():
    ami.req(0x08, bytes([4]) + b'EndCLI')
    ami.key(0x44)
    time.sleep(3)


def main():
    if sys.argv[1] == 'aga':
        aga_main(sys.argv[2:])
    out = sys.argv[1]
    pic1 = b''.join(bytes(src_px(x, y)) for y in range(PH) for x in range(PW))
    pic2 = bytes((255, 0, 0, ALPHA)) * (TW * TH)
    fill = b''.join(b'\x1b[%d;58H' % r + b' ' * 11 for r in range(2, 7))
    stream = (b'\x1b[2J\x1b[44m' + fill + b'\x1b[0m\x1b[2;2H' + apc('a=T,f=24,s=%d,v=%d,i=1,q=2' % (PW, PH), pic1)
              + b'\x1b[2;60H' + apc('a=T,f=32,s=%d,v=%d,i=2,z=1,C=1,c=5,r=4,q=2' % (TW, TH), pic2)
              + b'\x1b[2;80H' + apc('a=T,f=32,s=%d,v=%d,i=3,q=2' % (TW, TH), pic2) + b'\x1b[40;1H')
    w, h, px = shoot(stream)
    pathlib.Path(out + '.png').write_bytes(png(w, h, px))
    print('shot', w, h)
    at = lambda x, y: tuple(px[(y * w + x) * 3:(y * w + x) * 3 + 3])
    ok = True

    # case 1: find the origin: first row of the source (100 distinct pixels) must match in the shot
    first = [src_px(x, 0) for x in range(PW)]
    org = None
    for y in range(h):
        row = [at(x, y) for x in range(w)]
        for x in range(0, w - PW):
            if row[x] == first[0] and row[x:x + PW] == first:
                org = (x, y)
                break
        if org:
            break
    if not org:
        print('case 1 FAIL: picture first row not found in the shot')
        ok = False
    else:
        want = [src_px(x, y) for y in range(PH) for x in range(PW)]
        got = [at(org[0] + x, org[1] + y) for y in range(PH) for x in range(PW)]
        exact = sum(1 for a, g in zip(want, got) if a == g)
        shown = len(set(want) & set(got))
        pe, pc = 100.0 * exact / len(want), 100.0 * shown / len(set(want))
        c1 = pe >= 99 and pc >= 90
        print('case 1 origin %s distinct source %d shown %d (%.2f%%) exact pixels %d/%d (%.2f%%) %s'
              % (org, len(set(want)), shown, pc, exact, len(want), pe, 'PASS' if c1 else 'FAIL'))
        ok &= c1

    # case 2/3: 50% red pictures; cell width = (pic 2 column - pic 1 column) / 58 columns is 8 px (checked
    # below by the measured box). Case 2 sits on cells with SGR 44 background (blue), case 3 on the default
    # (black) window background. The expected pixel is the K40b formula; bg is read from a cell beside the picture.
    if org:
        for name, col, bgdx in (('case 2 (on blue cells)', 60, TW + 4), ('case 3 (on default background)', 80, TW + 4)):
            ox = org[0] + (col - 2) * 8
            bg = at(ox + bgdx, org[1] + 2)
            want_px = tuple(((255 if k == 0 else 0) * ALPHA + bg[k] * (255 - ALPHA) + 127) // 255 for k in range(3))
            cnt = {}
            for y in range(org[1], org[1] + TH):
                for x in range(ox, ox + TW):
                    p = at(x, y)
                    cnt[p] = cnt.get(p, 0) + 1
            best = max(cnt.items(), key=lambda kv: kv[1])
            cx = best[1] >= TW * TH * 0.99 and all(abs(best[0][k] - want_px[k]) <= 1 for k in range(3))
            print('%s: bg %s expected %s measured %s on %d of %d pixels %s'
                  % (name, bg, want_px, best[0], best[1], TW * TH, 'PASS' if cx else 'FAIL'))
            ok &= cx
    else:
        ok = False
    print('PASS' if ok else 'FAIL')
    finish()
    sys.exit(0 if ok else 1)


main()
