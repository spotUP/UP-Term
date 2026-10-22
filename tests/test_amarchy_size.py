#!/usr/bin/env python3
"""test_amarchy_size.py -- the Amarchy commodity stays under its resident budget.

  python3 tests/test_amarchy_size.py [build/amiga/Amarchy]

Plan 2026-10-22-amarchy-commodity.md: "under 20 KB resident" = file size + BSS. The commodity is a
single AmigaDOS hunk executable; its header lists the memory size of every hunk, and what a hunk
takes in memory beyond its bytes on disk is the BSS (and zeroed tail). Resident = the file's size
plus the memory sizes that the file does not carry. Exit 1 over the budget (make amarchy-size).
"""
import pathlib, struct, sys

LIMIT = 20480
ROOT = pathlib.Path(__file__).resolve().parents[1]
HUNK_HEADER, HUNK_BSS = 0x3F3, 0x3EB


def resident(path):
    d = pathlib.Path(path).read_bytes()
    n = len(d)

    def u32(p):
        return struct.unpack('>I', d[p:p + 4])[0]

    if u32(0) != HUNK_HEADER:
        raise SystemExit('[ERROR] %s is not an AmigaDOS executable' % path)
    p = 4
    while u32(p):                                       # resident library names: none expected
        p += 4 + 4 * u32(p)
    p += 4
    first, last = u32(p + 4), u32(p + 8)
    p += 12 + 4 * (last - first + 1)                    # the table of hunk sizes
    bss = 0                                             # BSS hunks have no bytes in the file
    while p < n:
        t = u32(p) & 0x3FFFFFFF
        p += 4
        if t in (0x3E9, 0x3EA, 0x3F1):                  # CODE, DATA, DEBUG: a length, then the bytes
            p += 4 + 4 * u32(p)
        elif t == HUNK_BSS:
            bss += 4 * u32(p)
            p += 4
        elif t == 0x3EC:                                # RELOC32: groups of count, hunk, offsets
            while u32(p):
                p += 8 + 4 * u32(p)
            p += 4
        elif t in (0x3F7, 0x3FC):                       # DREL32, RELOC32SHORT: 16-bit groups, long-aligned
            while True:
                c = struct.unpack('>H', d[p:p + 2])[0]
                p += 2
                if not c:
                    break
                p += 2 + 2 * c
            p = (p + 3) & ~3
        elif t == 0x3F0:                                # SYMBOL: groups of name, value
            while u32(p):
                p += 8 + 4 * u32(p)
            p += 4
        elif t != 0x3F2:                                # END
            raise SystemExit('[ERROR] unknown hunk type %x at %d' % (t, p - 4))
    return n, bss, n + bss


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'build/amiga/Amarchy')
    size, bss, total = resident(path)
    print('Amarchy: file %d B + BSS %d B = %d B resident (limit %d)' % (size, bss, total, LIMIT))
    if total > LIMIT:
        print('[ERROR] Amarchy is %d B over its 20 KB resident budget' % (total - LIMIT))
        sys.exit(1)
    print('[OK] under budget')


if __name__ == '__main__':
    main()
