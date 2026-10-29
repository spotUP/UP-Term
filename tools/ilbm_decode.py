#!/usr/bin/env python3
"""Independent IFF ILBM decoder (no code shared with amarchy/shot_core.c), used by tests/test_shot_core.c and the
rig rows. Prints "W H DEPTH CAMG" then one line per pixel row of space-separated colour register numbers.
Handles uncompressed and ByteRun1 BODYs, 1..8 planes, no mask plane."""
import struct
import sys


def decode(buf):
    if buf[:4] != b"FORM" or buf[8:12] != b"ILBM":
        raise ValueError("not FORM ILBM")
    if struct.unpack(">I", buf[4:8])[0] != len(buf) - 8:
        raise ValueError("FORM size %d, file %d" % (struct.unpack(">I", buf[4:8])[0], len(buf)))
    pos, bmhd, camg, body, cmap = 12, None, 0, None, b""
    while pos + 8 <= len(buf):
        tag, size = buf[pos:pos + 4], struct.unpack(">I", buf[pos + 4:pos + 8])[0]
        data = buf[pos + 8:pos + 8 + size]
        if len(data) != size:
            raise ValueError("chunk %r truncated" % tag)
        if tag == b"BMHD":
            bmhd = struct.unpack(">HHhhBBBxHBBhh", data)
        elif tag == b"CAMG":
            camg = struct.unpack(">I", data)[0]
        elif tag == b"CMAP":
            cmap = data
        elif tag == b"BODY":
            body = data
        pos += 8 + size + (size & 1)
    w, h, _, _, depth, mask, comp = bmhd[:7]
    rb = (w + 15) // 16 * 2
    rows, at = [], 0
    for _ in range(h):
        planes = []
        for _p in range(depth):
            if comp == 1:
                # unpack consumes exactly one plane row; find how much input it used
                row, used = unpack_len(body, at, rb)
                at += used
            else:
                row = body[at:at + rb]
                at += rb
            planes.append(row)
        px = []
        for x in range(w):
            v = 0
            for p in range(depth):
                if planes[p][x >> 3] & (0x80 >> (x & 7)):
                    v |= 1 << p
            px.append(v)
        rows.append(px)
    if at != len(body):
        raise ValueError("BODY has %d bytes, rows used %d" % (len(body), at))
    return w, h, depth, camg, cmap, rows


def unpack_len(data, at, want):
    out = bytearray()
    i = at
    while len(out) < want:
        n = data[i]
        i += 1
        if n < 128:
            out += data[i:i + n + 1]
            i += n + 1
        elif n > 128:
            out += bytes([data[i]]) * (257 - n)
            i += 1
    if len(out) != want:
        raise ValueError("packed row overruns")
    return bytes(out), i - at


if __name__ == "__main__":
    w, h, depth, camg, cmap, rows = decode(open(sys.argv[1], "rb").read())
    print(w, h, depth, camg)
    for r in rows:
        print(" ".join(map(str, r)))
