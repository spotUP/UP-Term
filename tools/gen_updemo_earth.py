#!/usr/bin/env python3
"""Pack an animated GIF into dist/updemo/earth.anim, the file UPDemo's kitty animation scene plays.

Usage: python3 -I tools/gen_updemo_earth.py demo/media/earth.gif dist/updemo/earth.anim [size]

Format (big endian): "UPANIM1\\n", u32 frame count, then per frame u32 gap in
milliseconds, u32 PNG length, the PNG (size x size, RGBA: the globe disc fills the
square, everything outside it is alpha 0 so the fjord photo shows through). The demo sends each
PNG as a kitty graphics frame (f=100), the gaps as a=a commands, and the loop.
Needs Pillow. The GIF is data: read it with `python3 -I`.
"""
import io
import struct
import sys

from PIL import Image, ImageDraw, ImageSequence

# the globe disc inside the 400 x 400 NASA frames, measured over all 44 frames (centre x, centre y, radius)
DISC = (193.5, 198.0, 168.0)


def disc_mask(size):
    """alpha mask of the disc, antialiased by 4x supersampling; 255 inside, 0 outside"""
    cx, cy, r = DISC
    big = Image.new("L", (size * 4, size * 4), 0)
    ImageDraw.Draw(big).ellipse([0, 0, size * 4 - 1, size * 4 - 1], fill=255)
    return big.resize((size, size), Image.LANCZOS), (cx - r, cy - r, cx + r, cy + r)


def main():
    src, out = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 160
    frames = []
    for f in ImageSequence.Iterator(Image.open(src)):
        gap = f.info.get("duration", 90) or 90
        mask, box = disc_mask(size)
        g = f.convert("RGB").crop(tuple(int(round(v)) for v in box)).resize((size, size), Image.LANCZOS)
        g.putalpha(mask)
        buf = io.BytesIO()
        g.save(buf, "PNG", optimize=True)
        frames.append((gap, buf.getvalue()))
    with open(out, "wb") as fh:
        fh.write(b"UPANIM1\n" + struct.pack(">I", len(frames)))
        for gap, png in frames:
            fh.write(struct.pack(">II", gap, len(png)) + png)
    print(len(frames), "frames", size, "px")


main()
