#!/usr/bin/env python3
"""tools/gen_unifont.py: the page files it writes (ledger U2). Run by make test.

The golden pages in tests/unifont/golden/ are also what the C suite `unifont`
reads, so the converter and the renderer agree on one layout. Beside the
byte-for-byte comparison, the layout is checked field by field here, so a
golden regenerated from a broken converter does not pass by itself."""
import pathlib
import shutil
import struct
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_unifont  # noqa: E402

FIXTURE = ROOT / "tests/unifont/fixture.hex"
GOLDEN = ROOT / "tests/unifont/golden"
OUT = ROOT / "build/test-unifont"


def glyph_at(page, lo):
    """(bytes, wide) of code point lo in a page file, read by the layout."""
    present, wide = page[8:40], page[40:72]
    groups = struct.unpack(">8H", page[72:88])
    if not present[lo >> 3] & (0x80 >> (lo & 7)):
        return None, False
    off = groups[lo >> 5]
    for k in range(lo & ~31, lo):
        if present[k >> 3] & (0x80 >> (k & 7)):
            off += 32 if wide[k >> 3] & (0x80 >> (k & 7)) else 16
    w = bool(wide[lo >> 3] & (0x80 >> (lo & 7)))
    return page[off:off + (32 if w else 16)], w


class Converter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shutil.rmtree(OUT, ignore_errors=True)
        cls.pages = gen_unifont.convert(FIXTURE, OUT)

    def test_pages_equal_the_golden_bytes(self):
        names = sorted(p.name for p in OUT.iterdir())
        self.assertEqual(names, sorted(p.name for p in GOLDEN.iterdir()))
        for n in names:
            self.assertEqual((OUT / n).read_bytes(), (GOLDEN / n).read_bytes(), n)

    def test_private_use_and_beyond_the_bmp_are_left_out(self):
        self.assertEqual(sorted(self.pages), [0x04, 0x20, 0x26, 0x4E, 0xFF])

    def test_header_names_its_page_and_count(self):
        p = (OUT / "4E").read_bytes()
        self.assertEqual(p[:4], b"UFP1")
        self.assertEqual(p[4], 0x4E)
        self.assertEqual(p[5], 1)
        self.assertEqual(struct.unpack(">H", p[6:8])[0], 2)
        self.assertEqual(len(p), 88 + 2 * 32)

    def test_narrow_glyph_rows_are_the_hex_bytes(self):
        g, wide = glyph_at((OUT / "04").read_bytes(), 0x16)  # U+0416
        self.assertFalse(wide)
        self.assertEqual(g, bytes.fromhex("0000000049492A2A1C1C2A2A49490000"))
        g, wide = glyph_at((OUT / "04").read_bytes(), 0x36)  # U+0436, the next group
        self.assertEqual(g, bytes.fromhex("00000000000049492A1C1C2A49490000"))

    def test_wide_glyph_after_another_in_its_group(self):
        g, wide = glyph_at((OUT / "4E").read_bytes(), 0x2D)  # U+4E2D after U+4E00
        self.assertTrue(wide)
        self.assertEqual(g[:6], bytes.fromhex("008000800080"))
        self.assertEqual(g[6:8], bytes.fromhex("3FFE"))

    def test_mixed_page_offsets(self):
        p = (OUT / "FF").read_bytes()  # U+FF01 wide, U+FFFD narrow
        g, wide = glyph_at(p, 0xFD)
        self.assertFalse(wide)
        self.assertEqual(g, bytes.fromhex("0000007E665A5A7A76767E76767E0000"))
        self.assertEqual(len(p), 88 + 32 + 16)

    def test_absent_code_point(self):
        self.assertEqual(glyph_at((OUT / "04").read_bytes(), 0x17), (None, False))

    def test_a_rerun_removes_pages_the_hex_no_longer_has(self):
        stale = OUT / "AB"
        stale.write_bytes(b"x")
        gen_unifont.convert(FIXTURE, OUT)
        self.assertFalse(stale.exists())


if __name__ == "__main__":
    r = unittest.main(exit=False, verbosity=0).result
    n = r.testsRun
    bad = len(r.failures) + len(r.errors)
    print("[%s] gen_unifont: %d checks, %d failed" % ("OK" if not bad else "FAIL", n, bad))
    sys.exit(1 if bad else 0)
