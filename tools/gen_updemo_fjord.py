#!/usr/bin/env python3
"""Make dist/updemo/fjord.png, the photo UPDemo's "kitty picture" scene sends as a kitty PNG (f=100).

Usage: python3 -I tools/gen_updemo_fjord.py demo/media/fjord.jpg dist/updemo/fjord.png [width height]

The source is picsum.photos id 1015 (a Norwegian fjord, Unsplash licence: free to use), see demo/media/SOURCE.txt.
It is cropped to the target aspect and resized (default 160 x 100: small enough for a 68020 to decode and
for a 640 x 256 screen). Needs Pillow. The JPEG is data: read it with `python3 -I`.
"""
import sys

from PIL import Image


def main():
    src, out = sys.argv[1], sys.argv[2]
    w = int(sys.argv[3]) if len(sys.argv) > 3 else 160
    h = int(sys.argv[4]) if len(sys.argv) > 4 else 100
    im = Image.open(src).convert("RGB")
    want = w / h
    cw, ch = im.size
    if cw / ch > want:
        nw = round(ch * want)
        im = im.crop(((cw - nw) // 2, 0, (cw - nw) // 2 + nw, ch))
    else:
        nh = round(cw / want)
        im = im.crop((0, (ch - nh) // 2, cw, (ch - nh) // 2 + nh))
    im.resize((w, h), Image.LANCZOS).save(out, "PNG", optimize=True)
    print(w, "x", h)


main()
