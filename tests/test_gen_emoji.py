#!/usr/bin/env python3
"""tools/gen_emoji.py: the colour emoji pages it writes (ledger U4). Run by make test.

The fixture is drawn here (no Twemoji art in the tree): a few 72 px PNGs of
each kind the release has -- RGBA, palette at 4 and 2 bits with tRNS, every
row filter -- packed as a release archive. The golden pages it gives
(tests/emoji/golden) are what the C suite `emoji` reads, so the converter
and the renderer agree on one layout; beside the byte comparison the
layout is decoded field by field here, so a golden regenerated from a
broken converter does not pass by itself. UPDATE=1 rewrites the goldens."""
import io
import os
import pathlib
import shutil
import struct
import sys
import tarfile
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_emoji  # noqa: E402

GOLDEN = ROOT / "tests/emoji/golden"
OUT = ROOT / "build/test-emoji"

YELLOW, BROWN = (255, 204, 77, 255), (102, 69, 0, 255)
GREEN, WHITE = (120, 177, 89, 255), (255, 255, 255, 255)
CLEAR = (0, 0, 0, 0)


# ---- the fixture -----------------------------------------------------------------

def png(w, h, rows, ctype, depth=8, plte=None, trns=None):
    """A PNG of raw sample rows (lists of ints), each row with its own filter
    (0-4 in turn), so the converter's unfiltering is exercised."""
    chans = {2: 3, 3: 1, 6: 4}[ctype]
    bpp = max(1, chans * depth // 8)
    raw = bytearray()
    prev = bytearray((w * chans * depth + 7) // 8)
    for y, samples in enumerate(rows):
        if depth == 8:
            line = bytearray(samples)
        else:
            per = 8 // depth
            line = bytearray(len(prev))
            for x, v in enumerate(samples):
                line[x // per] |= v << ((per - 1 - x % per) * depth)
        f = y % 5
        enc = bytearray(len(line))
        for i in range(len(line)):
            a = line[i - bpp] if i >= bpp else 0
            b = prev[i]
            c = prev[i - bpp] if i >= bpp else 0
            pred = (0, a, b, (a + b) >> 1, gen_emoji._paeth(a, b, c))[f]
            enc[i] = (line[i] - pred) & 0xFF
        raw += bytes([f]) + enc
        prev = line

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    out = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, 0))
    if plte:
        out += chunk(b"PLTE", bytes(v for c in plte for v in c))
    if trns:
        out += chunk(b"tRNS", bytes(trns))
    return out + chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b"")


def blocks(fn):
    """72x72 pixels from fn(bx, by) over 8x8 blocks of 9 pixels: on a 9-pixel
    grid both sizes (4.5 and 9 source pixels a pixel) average no two blocks."""
    return [fn(x // 9, y // 9) for y in range(72) for x in range(72)]


def face(x, y):
    """A disc with two eyes, RGBA: its edge is antialiased by the averaging."""
    dx, dy = x - 35.5, y - 35.5
    if dx * dx + dy * dy > 34 * 34:
        return CLEAR
    if 22 <= y < 32 and (20 <= x < 28 or 44 <= x < 52):
        return BROWN
    return YELLOW


FACE = [face(x, y) for y in range(72) for x in range(72)]
CHECK = blocks(lambda bx, by: CLEAR if bx in (0, 7) or by in (0, 7) else
               WHITE if (bx, by) in ((2, 4), (3, 5), (4, 4), (5, 3)) else GREEN)
CHECK_PAL = [(0, 0, 0), GREEN[:3], WHITE[:3]]
STAR = blocks(lambda bx, by: YELLOW if 2 <= bx <= 5 and 2 <= by <= 5 else
              BROWN if bx == by else CLEAR)
STAR_PAL = [(0, 0, 0), YELLOW[:3], BROWN[:3]]
# 64 colours far apart: no 16 of them stay within MAX_ERR, so 8-bit indices
ROCKET = blocks(lambda bx, by: ((bx * 36) & 255, (by * 36) & 255, ((bx ^ by) * 36 + 18) & 255, 255))


def pal_rows(px, pal, transparent=0):
    index = {c[:3]: i for i, c in enumerate(pal)}
    return [[transparent if px[y * 72 + x][3] == 0 else index[px[y * 72 + x][:3]] for x in range(72)]
            for y in range(72)]


def rgba_rows(px):
    return [[v for p in px[y * 72:(y + 1) * 72] for v in p] for y in range(72)]


def fixture_files():
    return {
        "1f600": png(72, 72, rgba_rows(FACE), 6),
        "2705": png(72, 72, pal_rows(CHECK, CHECK_PAL), 3, 4, CHECK_PAL, [0]),
        "2b50": png(72, 72, pal_rows(STAR, STAR_PAL), 3, 2, STAR_PAL, [0, 255, 255]),
        "1f680": png(72, 72, [[v for p in ROCKET[y * 72:(y + 1) * 72] for v in p[:3]] for y in range(72)], 2),
        "263a": png(72, 72, rgba_rows(FACE), 6),          # narrow: left out
        "1f44d-1f3fb": png(72, 72, rgba_rows(FACE), 6),   # a sequence: left out
    }


def make_archive(path):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(path, "w:gz") as tar:
        for name, data in sorted(fixture_files().items()):
            info = tarfile.TarInfo("twemoji-0.0.0/assets/72x72/%s.png" % name)
            info.size = len(data)
            info.mtime = 0
            tar.addfile(info, io.BytesIO(data))
        info = tarfile.TarInfo("twemoji-0.0.0/assets/svg/1f600.svg")  # not a 72 px PNG: ignored
        info.size = 4
        tar.addfile(info, io.BytesIO(b"<svg"))


# ---- the layout, read independently --------------------------------------------------

def glyph_at(page, lo):
    """The glyph record of code point lo in a page file, by the layout."""
    present = page[12:44]
    groups = struct.unpack(">8I", page[44:76])
    if not present[lo >> 3] & (0x80 >> (lo & 7)):
        return None
    off = groups[lo >> 5]
    for k in range(lo & ~31, lo):
        if present[k >> 3] & (0x80 >> (k & 7)):
            bits, n = page[off], page[off + 1] + 1
            off += 2 + 4 * n + 384 * bits // 8
    bits, n = page[off], page[off + 1] + 1
    return page[off:off + 2 + 4 * n + 384 * bits // 8]


class Converter(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        shutil.rmtree(OUT, ignore_errors=True)
        make_archive(OUT / "twemoji.tar.gz")
        cls.pages, cls.glyphs = gen_emoji.convert(OUT / "twemoji.tar.gz", OUT / "pages")
        if os.environ.get("UPDATE"):
            shutil.rmtree(GOLDEN, ignore_errors=True)
            shutil.copytree(OUT / "pages", GOLDEN)

    def test_pages_equal_the_golden_bytes(self):
        names = sorted(p.name for p in (OUT / "pages").iterdir())
        self.assertEqual(names, sorted(p.name for p in GOLDEN.iterdir()))
        for n in names:
            self.assertEqual((OUT / "pages" / n).read_bytes(), (GOLDEN / n).read_bytes(), n)

    def test_only_two_cell_single_code_points_are_taken(self):
        self.assertEqual(sorted(self.glyphs), [0x2705, 0x2B50, 0x1F600, 0x1F680])
        self.assertEqual(sorted(self.pages), [0x27, 0x2B, 0x1F6])
        self.assertNotIn(0x263A, self.glyphs)   # one cell in the engine: never drawn in colour
        self.assertNotIn(0x1F44D, self.glyphs)  # only in a sequence's file name

    def test_widths_are_the_engines(self):
        w = gen_emoji.width_fn()
        self.assertEqual([w(c) for c in (0xE9, 0x263A, 0x2705, 0x4E00, 0x1F600, 0x1F3FB, 0x200D)],
                         [1, 1, 2, 2, 2, 2, 0])

    def test_png_reader_gives_the_pixels_of_each_kind(self):
        f = fixture_files()
        self.assertEqual(gen_emoji.read_png(f["1f600"]), (72, 72, FACE))
        self.assertEqual(gen_emoji.read_png(f["2705"])[2], CHECK)  # 4-bit palette, tRNS
        self.assertEqual(gen_emoji.read_png(f["2b50"])[2], STAR)   # 2-bit palette
        self.assertEqual(gen_emoji.read_png(f["1f680"])[2], ROCKET)  # RGB

    def test_header_names_page_count_length_and_groups(self):
        p = (OUT / "pages/1F6").read_bytes()
        self.assertEqual(p[:4], b"UCE1")
        page, count, length = struct.unpack(">HHI", p[4:12])
        self.assertEqual((page, count, length), (0x1F6, 2, len(p)))
        groups = struct.unpack(">8I", p[44:76])
        self.assertEqual(groups[0], 76)                     # U+1F600 first
        self.assertEqual(groups[4], 76 + len(glyph_at(p, 0x00)))  # U+1F680 starts group 4
        self.assertEqual(groups[7], len(p))                 # past the last glyph

    def test_few_colours_round_trip_exactly_at_four_bits(self):
        for page, lo, art in (("27", 0x05, CHECK), ("2B", 0x50, STAR)):
            g = glyph_at((OUT / "pages" / page).read_bytes(), lo)
            bits, pal, images = gen_emoji.decode_glyph(g)
            self.assertEqual(bits, 4)
            self.assertLessEqual(len(pal), 16)
            for (W, H), im in zip(gen_emoji.SIZES, images):
                self.assertEqual(im, gen_emoji.area(72, 72, art, W, H), (page, W, H))
        bits, pal, images = gen_emoji.decode_glyph(glyph_at((OUT / "pages/27").read_bytes(), 0x05))
        self.assertEqual(images[0][0], CLEAR)                # the corner
        self.assertEqual(images[0][8 * 16 + 4], WHITE)       # the check mark, block (2, 4)
        self.assertEqual(images[1][4 * 16 + 4], WHITE)       # the same block at 8 rows
        self.assertEqual(images[0][3 * 16 + 12], GREEN)

    def test_many_far_colours_keep_them_exactly_at_eight_bits(self):
        g = glyph_at((OUT / "pages/1F6").read_bytes(), 0x80)
        bits, pal, images = gen_emoji.decode_glyph(g)
        self.assertEqual((bits, len(pal)), (8, 64))
        for (W, H), im in zip(gen_emoji.SIZES, images):
            self.assertEqual(im, gen_emoji.area(72, 72, ROCKET, W, H))

    def test_antialiased_art_stays_within_the_error_bound(self):
        g = glyph_at((OUT / "pages/1F6").read_bytes(), 0x00)
        bits, pal, images = gen_emoji.decode_glyph(g)
        self.assertEqual(bits, 4)
        arts = [gen_emoji.area(72, 72, FACE, W, H) for W, H in gen_emoji.SIZES]
        self.assertGreater(len(set(arts[0] + arts[1])), 16)  # the edges' shades, both sizes: quantised
        for art, im in zip(arts, images):
            self.assertLessEqual(max(gen_emoji.error(a, b) for a, b in zip(art, im)), gen_emoji.MAX_ERR)
        self.assertEqual(images[0][0], CLEAR)
        self.assertEqual(images[0][8 * 16 + 8], YELLOW)

    def test_same_archive_same_bytes(self):
        again, _ = gen_emoji.convert(OUT / "twemoji.tar.gz", OUT / "again")
        for page in again:
            n = gen_emoji.page_name(page)
            self.assertEqual((OUT / "again" / n).read_bytes(), (OUT / "pages" / n).read_bytes())


if __name__ == "__main__":
    unittest.main(verbosity=1)
