#!/usr/bin/env python3
"""GNU Unifont (.hex, plain or .gz) -> UP-Term's glyph pages (ledger U2,
thoughts/shared/plans/2026-10-04-u2-unifont.md).

One file per 256-code-point page that has a glyph, named by the page
(the code point >> 8) in upper-case hex: two digits in the BMP ("4E" holds
U+4E00-U+4EFF), three in plane 1 ("1F6" holds U+1F600-U+1F6FF, the
emoticons). The renderer (render/unifont.c) reads them; the layout, all
big-endian:

  0   4  "UFP2" (UFP1 had the page in one byte and a version byte)
  4   2  page (the code point >> 8)
  6   2  glyphs in the page
  8  32  present bits: code point lo is bit 0x80 >> (lo & 7) of byte lo >> 3
  40 32  wide bits, the same way (a 16x16 glyph; else 8x16)
  72 16  8 x u16: the offset of the first glyph at or after each 32-code-point
         group (lo & ~31), from the start of the file
  88     the glyphs in code-point order: 16 bytes (8x16, a byte a row) or 32
         bytes (16x16, two bytes a row, left byte first)

Plane 1 only U+1F000-U+1FAFF: the emoji and pictographic symbol blocks
(Mahjong and domino tiles, playing cards, enclosed alphanumerics and
ideographs, the pictographs, emoticons, transport, alchemical, geometric
shapes extended, arrows-C, the supplemental and extended-A pictographs),
from Unifont Upper's glyphs in the _all file -- the rest of plane 1 (old
scripts) and the other planes are left out to keep the kit small. Left out
too: the private use area U+E000-U+F8FF (Unifont's _all file carries the
ConScript glyphs there; Nerd Font icons live in it, and those are the outline
font's). Output is deterministic: the same .hex gives the same bytes.

usage: gen_unifont.py <unifont.hex[.gz]> <out-dir>
"""
import gzip
import pathlib
import struct
import sys

MAGIC = b"UFP2"
HEADER = 88
PLANE1 = (0x1F000, 0x1FAFF)  # the emoji blocks taken from plane 1


def wanted(cp):
    """1 for a code point the pages carry: the BMP but its private use area
    and surrogates, and plane 1's emoji blocks."""
    if cp <= 0xFFFF:
        return not (0xE000 <= cp <= 0xF8FF or 0xD800 <= cp <= 0xDFFF)
    return PLANE1[0] <= cp <= PLANE1[1]


def read_hex(path):
    """{code point: bytes} from a Unifont .hex file (the code points wanted)."""
    p = pathlib.Path(path)
    raw = gzip.open(p, "rt") if p.suffix == ".gz" else open(p, "rt")
    glyphs = {}
    with raw as f:
        for n, line in enumerate(f, 1):
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            cp_s, _, bits = line.partition(":")
            cp = int(cp_s, 16)
            if not wanted(cp):
                continue
            if len(bits) not in (32, 64):
                raise SystemExit("%s:%d: U+%04X is %d hex digits, not 32 or 64" % (path, n, cp, len(bits)))
            glyphs[cp] = bytes.fromhex(bits)
    return glyphs


def page_name(page):
    """The page's file name: "4E" in the BMP, "1F6" in plane 1."""
    return "%02X" % page if page < 0x100 else "%03X" % page


def page_bytes(page, glyphs):
    """The page file for high byte `page`, or None when it has no glyph."""
    present = bytearray(32)
    wide = bytearray(32)
    groups = []
    body = bytearray()
    count = 0
    for lo in range(256):
        if lo % 32 == 0:
            groups.append(HEADER + len(body))
        g = glyphs.get(page << 8 | lo)
        if g is None:
            continue
        present[lo >> 3] |= 0x80 >> (lo & 7)
        if len(g) == 32:
            wide[lo >> 3] |= 0x80 >> (lo & 7)
        body += g
        count += 1
    if not count:
        return None
    head = MAGIC + struct.pack(">HH", page, count) + bytes(present) + bytes(wide)
    head += struct.pack(">8H", *groups)
    assert len(head) == HEADER
    return head + bytes(body)


def convert(hex_path, out_dir):
    glyphs = read_hex(hex_path)
    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for pat in ("[0-9A-F][0-9A-F]", "[0-9A-F][0-9A-F][0-9A-F]"):
        for old in out.glob(pat):
            old.unlink()  # a page the new .hex no longer has must not linger
    pages = {}
    for page in list(range(256)) + list(range(PLANE1[0] >> 8, (PLANE1[1] >> 8) + 1)):
        b = page_bytes(page, glyphs)
        if b is not None:
            (out / page_name(page)).write_bytes(b)
            pages[page] = len(b)
    return pages


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pages = convert(sys.argv[1], sys.argv[2])
    total = sum(pages.values())
    plane1 = sum(n for p, n in pages.items() if p >= 0x100)
    print("[OK] %d pages, %d bytes (largest %d, smallest %d; plane 1: %d pages, %d bytes) in %s" %
          (len(pages), total, max(pages.values()), min(pages.values()),
           sum(1 for p in pages if p >= 0x100), plane1, sys.argv[2]))


if __name__ == "__main__":
    main()
