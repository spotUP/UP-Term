#!/usr/bin/env python3
"""GNU Unifont (.hex, plain or .gz) -> UP-Term's glyph pages (ledger U2,
thoughts/shared/plans/2026-10-04-u2-unifont.md).

One file per 256-code-point page of the BMP that has a glyph, named by the
page's high byte in two upper-case hex digits ("4E" holds U+4E00-U+4EFF).
The renderer (render/unifont.c) reads them; the layout, all big-endian:

  0   4  "UFP1"
  4   1  page (the code point's high byte)
  5   1  version, 1
  6   2  glyphs in the page
  8  32  present bits: code point lo is bit 0x80 >> (lo & 7) of byte lo >> 3
  40 32  wide bits, the same way (a 16x16 glyph; else 8x16)
  72 16  8 x u16: the offset of the first glyph at or after each 32-code-point
         group (lo & ~31), from the start of the file
  88     the glyphs in code-point order: 16 bytes (8x16, a byte a row) or 32
         bytes (16x16, two bytes a row, left byte first)

Left out: the private use area U+E000-U+F8FF (Unifont's _all file carries the
ConScript glyphs there; Nerd Font icons live in it, and those are the outline
font's) and anything beyond U+FFFF. Output is deterministic: the same .hex
gives the same bytes.

usage: gen_unifont.py <unifont.hex[.gz]> <out-dir>
"""
import gzip
import pathlib
import struct
import sys

MAGIC = b"UFP1"
VERSION = 1
HEADER = 88


def read_hex(path):
    """{code point: bytes} from a Unifont .hex file (BMP, PUA left out)."""
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
            if cp > 0xFFFF or 0xE000 <= cp <= 0xF8FF or 0xD800 <= cp <= 0xDFFF:
                continue
            if len(bits) not in (32, 64):
                raise SystemExit("%s:%d: U+%04X is %d hex digits, not 32 or 64" % (path, n, cp, len(bits)))
            glyphs[cp] = bytes.fromhex(bits)
    return glyphs


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
    head = MAGIC + struct.pack(">BBH", page, VERSION, count) + bytes(present) + bytes(wide)
    head += struct.pack(">8H", *groups)
    assert len(head) == HEADER
    return head + bytes(body)


def convert(hex_path, out_dir):
    glyphs = read_hex(hex_path)
    out = pathlib.Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)
    for old in out.glob("[0-9A-F][0-9A-F]"):
        old.unlink()  # a page the new .hex no longer has must not linger
    pages = {}
    for page in range(256):
        b = page_bytes(page, glyphs)
        if b is not None:
            (out / ("%02X" % page)).write_bytes(b)
            pages[page] = len(b)
    return pages


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    pages = convert(sys.argv[1], sys.argv[2])
    total = sum(pages.values())
    print("[OK] %d pages, %d bytes (largest %d, smallest %d) in %s" %
          (len(pages), total, max(pages.values()), min(pages.values()), sys.argv[2]))


if __name__ == "__main__":
    main()
