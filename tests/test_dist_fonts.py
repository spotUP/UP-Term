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


def rows_of(path, code_point):
    """first and last row with a pixel set in the glyph at code_point"""
    code, ysize, _base, lo, chardata, modulo, loc = font(path)
    off, width = struct.unpack('>HH', code[loc + (code_point - lo) * 4:loc + (code_point - lo) * 4 + 4])
    ys = [y for y in range(ysize) for x in range(width)
          if code[chardata + y * modulo + (off + x) // 8] >> (7 - (off + x) % 8) & 1]
    return min(ys), max(ys)


class MiddleDot(unittest.TestCase):
    def test_latin1_middle_dot_stands_between_the_x_height_and_the_baseline(self):
        """amiga-pi's start banner separates its key hints with U+00B7 (Latin-1 0xB7, the font's own glyph). TopazPro 16
        drew it on rows 2-5, above the letters' x-height: the owner saw an apostrophe (G1, rig 3, 2026-10-13)."""
        p = os.path.join(ROOT, 'dist/fonts/TopazPro/16')
        top, bottom = rows_of(p, 0xB7)
        x_top, base = rows_of(p, ord('x'))
        self.assertGreaterEqual(top, x_top)
        self.assertLess(bottom, base)
        self.assertLess(rows_of(p, 0x27)[1], top)        # and below the apostrophe's rows


class OriginalTopazProIsReplaced(unittest.TestCase):
    """Owner 2026-10-23: a FONTS:TopazPro/16 that is byte for byte the dS-tprf.lha original (baseline 6,
    no middle dot) was kept by the Install, so the owner's Shell showed the unfixed font. The original
    is ours with these 9 bytes changed (offset, original byte); the Install knows it by size and
    CRC-32 (upsame) and replaces it, marked ours."""
    ORIGINAL_BYTES = {117: 6, 745: 12, 971: 12, 1197: 12, 1423: 12, 1875: 0, 2101: 0, 2327: 0, 2553: 0}
    DOS = open(os.path.join(ROOT, 'dist/install.dos')).read()

    def original(self):
        d = bytearray(open(os.path.join(ROOT, 'dist/fonts/TopazPro/16'), 'rb').read())
        for off, b in self.ORIGINAL_BYTES.items():
            d[off] = b
        return bytes(d)

    def fonts_block(self):
        a = self.DOS.index('Lab fonts')
        return self.DOS[a:self.DOS.index('If NOT EXISTS FONTS:IBM/8', a)]

    def test_the_install_names_the_originals_size_and_crc_and_ours_differs(self):
        import zlib
        o = self.original()
        self.assertIn('upsame >NIL: FONTS:TopazPro/16 %d %08x' % (len(o), zlib.crc32(o)), self.fonts_block())
        ours = open(os.path.join(ROOT, 'dist/fonts/TopazPro/16'), 'rb').read()
        self.assertNotEqual(zlib.crc32(o), zlib.crc32(ours))

    def test_the_original_has_baseline_6_and_ours_13(self):
        import tempfile
        with tempfile.NamedTemporaryFile() as f:
            f.write(self.original()); f.flush()
            self.assertEqual(font(f.name)[2], 6)
        self.assertEqual(font(os.path.join(ROOT, 'dist/fonts/TopazPro/16'))[2], 13)

    def test_a_match_replaces_the_font_and_writes_the_marker_and_nothing_else_does(self):
        b = self.fonts_block()
        # the marker test comes first: ours from an earlier kit; else the original is looked for
        marker = b.index('If EXISTS ENVARC:up-term/font-TopazPro-16')
        els = b.index('Else', marker)
        same = b.index('upsame >NIL: FONTS:TopazPro/16')
        ifok = b.index('If NOT WARN', same)
        copy = b.index('Copy fonts/TopazPro/16 FONTS:TopazPro/16 CLONE QUIET', ifok)
        echo = b.index('Echo >ENVARC:up-term/font-TopazPro-16 "installed by UP-Term"', copy)
        self.assertLess(els, same); self.assertLess(same, ifok); self.assertLess(ifok, copy); self.assertLess(copy, echo)
        self.assertEqual(b.count('Copy fonts/TopazPro/16 FONTS:TopazPro/16'), 3)   # new, marked, original

    def test_the_helper_is_built_shipped_and_compares_size_and_crc(self):
        mk = open(os.path.join(ROOT, 'Makefile')).read()
        self.assertIn('cp $(BUILD)/amiga/upsame $(KIT)/Files/upsame', mk)
        self.assertIn('$(BUILD)/amiga/upsame:', mk)
        src = open(os.path.join(ROOT, 'install/upsame.c')).read()
        self.assertIn('0xEDB88320UL', src)
        self.assertIn('FILE/A,SIZE/A/N,CRC/A', src)


if __name__ == '__main__':
    unittest.main()
