#!/usr/bin/env python3
"""Pack an animated GIF into dist/updemo/earth.anim, the file UPDemo's kitty animation scene plays.

Usage: python3 -I tools/gen_updemo_earth.py demo/media/earth.gif dist/updemo/earth.anim [size]

Format (big endian): "UPANIM1\\n", u32 frame count, then per frame u32 gap in
milliseconds, u32 PNG length, the PNG (size x size, RGB). The demo sends each
PNG as a kitty graphics frame (f=100), the gaps as a=a commands, and the loop.
Needs Pillow. The GIF is data: read it with `python3 -I`.
"""
import io
import struct
import sys

from PIL import Image, ImageSequence


def main():
    src, out = sys.argv[1], sys.argv[2]
    size = int(sys.argv[3]) if len(sys.argv) > 3 else 160
    frames = []
    for f in ImageSequence.Iterator(Image.open(src)):
        gap = f.info.get("duration", 90) or 90
        g = f.convert("RGB").resize((size, size), Image.LANCZOS)
        buf = io.BytesIO()
        g.save(buf, "PNG", optimize=True)
        frames.append((gap, buf.getvalue()))
    with open(out, "wb") as fh:
        fh.write(b"UPANIM1\n" + struct.pack(">I", len(frames)))
        for gap, png in frames:
            fh.write(struct.pack(">II", gap, len(png)) + png)
    print(len(frames), "frames", size, "px")


main()
