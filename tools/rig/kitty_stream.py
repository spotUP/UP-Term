#!/usr/bin/env python3
"""kitty_stream.py OUTDIR -- writes kitty graphics test streams for the rig (K12):
kitty-png.bin (f=100, a 32x16 PNG of coloured stripes) and kitty-rgb.bin (f=24
raw RGB, same picture). Type either into an XCON: window."""
import base64, struct, sys, zlib, os

W, H = 32, 16
COLS = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]


def pixels():
    return b"".join(bytes(COLS[(y // 4) % 4]) * W for y in range(H))


def png():
    raw = b"".join(b"\0" + pixels()[y * W * 3:(y + 1) * W * 3] for y in range(H))
    def chunk(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def stream(ctrl, data):
    b = base64.b64encode(data)
    parts = [b[i:i + 4096] for i in range(0, len(b), 4096)]
    out = b""
    for i, p in enumerate(parts):
        m = 1 if i < len(parts) - 1 else 0
        head = ctrl + ",m=%d" % m if i == 0 else "m=%d" % m
        out += b"\x1b_G" + head.encode() + b";" + p + b"\x1b\\"
    return out + b"\r\n"


out = sys.argv[1]
os.makedirs(out, exist_ok=True)
open(os.path.join(out, "kitty-png.bin"), "wb").write(stream("a=T,f=100,t=d", png()))
open(os.path.join(out, "kitty-rgb.bin"), "wb").write(stream("a=T,f=24,t=d,s=%d,v=%d" % (W, H), pixels()))
