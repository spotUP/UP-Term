#!/usr/bin/env python3
"""The strike fonts the kit installs (dist/fonts): each size's declared
tf_Baseline is the row its letters stand on -- the bottom row of 'H'.
TopazPro 16 said 6 (topaz 8's) while its letters stand on row 13: the
underline went through the letters and outline glyphs sat 7 rows high
(owner 2026-10-05: "fix the font file")."""
import glob, os, struct, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
HUNK_CODE = b'\x00\x00\x03\xe9'
TF = 4 + 54            # moveq/rts, then the DiskFontHeader's dfh_TF


def font(path):
    d = open(path, 'rb').read()
    i = d.find(HUNK_CODE)
    n = struct.unpack('>I', d[i + 4:i + 8])[0]
    code = d[i + 8:i + 8 + n * 4]
    ysize, _style, _flags, _xs, base, _b, _a, lo, hi = struct.unpack('>HBBHHHHBB', code[TF + 20:TF + 34])
    chardata, modulo, loc = struct.unpack('>IHI', code[TF + 34:TF + 44])
    return code, ysize, base, lo, chardata, modulo, loc


def bottom_row(path, ch):
    code, ysize, _base, lo, chardata, modulo, loc = font(path)
    off, width = struct.unpack('>HH', code[loc + (ord(ch) - lo) * 4:loc + (ord(ch) - lo) * 4 + 4])
    last = -1
    for y in range(ysize):
        for x in range(width):
            if code[chardata + y * modulo + (off + x) // 8] >> (7 - (off + x) % 8) & 1:
                last = y
    return last


class Baselines(unittest.TestCase):
    def test_every_size_declares_the_row_its_letters_stand_on(self):
        sizes = [p for p in glob.glob(os.path.join(ROOT, 'dist/fonts/*/*')) if os.path.basename(p).isdigit()]
        self.assertTrue(sizes)
        for p in sizes:
            with self.subTest(font=os.path.relpath(p, ROOT)):
                self.assertEqual(font(p)[2], bottom_row(p, 'H'))


if __name__ == '__main__':
    unittest.main()
