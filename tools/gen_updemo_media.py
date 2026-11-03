#!/usr/bin/env python3 -I
"""Write the kit's demo media next to earth.anim in dist/updemo/.

Usage: python3 -I tools/gen_updemo_media.py [dist/updemo]

kanim-stream.bin: the kitty animation stream, from tools/rig/kitty_anim_rig.build_stream (the one generator the
rig tool also uses). Show it with `Type UP-Term:share/updemo/kanim-stream.bin`.
photo.png: a 24-bit RGB sample picture (smooth colour gradients plus rings, drawn here, no outside image), for
`icat UP-Term:share/updemo/photo.png`.
"""
import math, pathlib, sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE / 'rig'))
import kitty_anim_rig as k

W, H = 160, 100


def photo_rgb():
    px = bytearray()
    for y in range(H):
        for x in range(W):
            d = math.hypot(x - W / 2, y - H / 2)
            ring = 0.5 + 0.5 * math.cos(d / 5.0)
            px += bytes((int(255 * x / (W - 1) * (0.6 + 0.4 * ring)),
                         int(255 * y / (H - 1) * (0.6 + 0.4 * ring)),
                         int(255 * (0.5 + 0.5 * math.sin((x + y) / 23.0)) * (0.5 + 0.5 * ring))))
    return bytes(px)


def main():
    out = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else HERE.parent / 'dist/updemo')
    out.mkdir(parents=True, exist_ok=True)
    (out / 'kanim-stream.bin').write_bytes(k.build_stream(100))
    (out / 'photo.png').write_bytes(k.png(W, H, photo_rgb()))


if __name__ == '__main__':
    main()
