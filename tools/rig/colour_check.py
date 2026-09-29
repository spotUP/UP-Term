#!/usr/bin/env python3
"""colour_check.py -- how close 24-bit colours come out on the rig's screen.

Opens an XCON window, Types VTC:grad320.txt (4 rows of 80 cells, each a
different 48;2;r;g;b background; the colours in VTC:grad320.json),
screenshots it and samples the middle of every cell. Prints the mean and
worst distance (sqrt of summed squared channel differences, 0..441) and
how many distinct colours reached the screen. The rig must be up.
"""
import ast, os, struct, sys, time, zlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

HERE = os.path.dirname(os.path.abspath(__file__))
VTC = os.path.join(HERE, "../../build/rig/vtc")
OUT = os.path.join(HERE, "../../build/rig/shots/colour_check.png")

def pixels(path):
    b = open(path, 'rb').read(); i = 8; idat = b''
    while i < len(b):
        n = struct.unpack('>I', b[i:i + 4])[0]; t = b[i + 4:i + 8]; d = b[i + 8:i + 8 + n]
        if t == b'IHDR':
            w, h = struct.unpack('>II', d[:8])
        if t == b'IDAT':
            idat += d
        i += 12 + n
    raw = zlib.decompress(idat); stride = 1 + w * 3
    return w, h, lambda x, y: tuple(raw[y * stride + 1 + x * 3:y * stride + 4 + x * 3])

def main():
    want = ast.literal_eval(open(os.path.join(VTC, "grad320.json")).read())
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/640/200/colour check/CLOSE"')
    time.sleep(4)
    ami.req(0x08, bytes([4]) + b'Type VTC:grad320n.txt\r'); time.sleep(4)
    ami.main(['shot', OUT])
    w, h, px = pixels(OUT)
    # the text area: the window's black inside starts under the title bar;
    # the cells start two text lines down (New Shell, the Type line)
    yi = next(y for y in range(14, h) if px(20, y) == (0, 0, 0))
    y0 = yi + 2 * 8
    x0 = next(x for x in range(0, 40) if px(x, yi + 2) == (0, 0, 0))
    cw, ch = 16, 8   # topaz 8: a cell is two characters wide
    errs, seen = [], set()
    for i, (r, g, b) in enumerate(want):
        row, col = divmod(i, 32)
        got = px(x0 + col * cw + cw // 2, y0 + row * ch + ch // 2)
        seen.add(got)
        errs.append(((got[0] - r) ** 2 + (got[1] - g) ** 2 + (got[2] - b) ** 2) ** 0.5)
    print("cells %d  cell %dx%d at %d,%d" % (len(want), cw, ch, x0, y0))
    print("mean error %.1f  worst %.1f  distinct colours on screen %d" % (
        sum(errs) / len(errs), max(errs), len(seen)))

if __name__ == '__main__':
    main()
