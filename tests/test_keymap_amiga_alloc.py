#!/usr/bin/env python3
"""test_keymap_amiga_alloc.py -- the Amiga build of config/keymap.c allocates with exec, not the C library.

  python3 tests/test_keymap_amiga_alloc.py build/amiga/keymap.o

config/keymap.c picks its allocator by platform: AllocVec/FreeVec on the Amiga, malloc/free on the
host. vbcc defines neither AMIGA nor __amigaos__, so the Amiga branch was never taken and Amarchy and
vtcon-handler got vc.lib's malloc. That malloc carves blocks out of 16 KB puddles that free() keeps
until the program exits: a keymap whose parse outgrew the first puddle (the dist keymap) held one
more 16 KB puddle for the life of the commodity (rig 4: 65432 B resident against 48952 B for an
empty keymap). The object is the vbcc build of keymap.c; it must not reference the C library's
malloc or free, and must reach exec (SysBase) for AllocVec. Exit 1 on failure.
"""
import pathlib, struct, sys

HUNK_UNIT, HUNK_NAME, HUNK_CODE, HUNK_DATA, HUNK_BSS = 0x3E7, 0x3E8, 0x3E9, 0x3EA, 0x3EB
HUNK_RELOC32, HUNK_EXT, HUNK_SYMBOL, HUNK_DEBUG, HUNK_END = 0x3EC, 0x3EF, 0x3F0, 0x3F1, 0x3F2
HUNK_DREL32, HUNK_RELOC32SHORT = 0x3F7, 0x3FC


def externals(path):
    """The names an Amiga hunk object references (HUNK_EXT reference entries)."""
    d = pathlib.Path(path).read_bytes()
    p, refs = 0, set()

    def u32(q):
        return struct.unpack('>I', d[q:q + 4])[0]

    while p + 4 <= len(d):
        t = u32(p) & 0x3FFFFFFF
        p += 4
        if t in (HUNK_UNIT, HUNK_NAME, HUNK_CODE, HUNK_DATA, HUNK_DEBUG):
            p += 4 + 4 * u32(p)
        elif t == HUNK_BSS:
            p += 4
        elif t in (HUNK_RELOC32, HUNK_DREL32):
            while u32(p):
                p += 8 + 4 * u32(p)
            p += 4
        elif t == HUNK_RELOC32SHORT:
            start = p
            while struct.unpack('>H', d[p:p + 2])[0]:
                p += 4 + 2 * struct.unpack('>H', d[p:p + 2])[0]
            p += 2
            p += (p - start) % 4
        elif t in (HUNK_EXT, HUNK_SYMBOL):
            while u32(p):
                w = u32(p)
                kind, n = w >> 24, w & 0xFFFFFF
                name = d[p + 4:p + 4 + 4 * n].rstrip(b'\0').decode('latin-1')
                p += 4 + 4 * n
                if t == HUNK_SYMBOL or kind < 0x80:     # a definition: one value
                    p += 4
                elif kind == 0x82 or kind == 0x84:      # common: size, then the references
                    if kind == 0x82:
                        p += 4
                    refs.add(name)
                    p += 4 + 4 * u32(p)
                else:                                   # a reference: count, offsets
                    refs.add(name)
                    p += 4 + 4 * u32(p)
            p += 4
        elif t == HUNK_END:
            pass
        else:
            sys.exit('test_keymap_amiga_alloc: unknown hunk 0x%X at %d in %s' % (t, p - 4, path))
    return refs


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    refs = externals(sys.argv[1])
    bad = sorted(r for r in refs if r in ('_malloc', '_free', '_calloc', '_realloc'))
    ok = not bad and '_SysBase' in refs
    print('%s keymap.c (vbcc) allocator: references %s' % ('[OK]' if ok else '[FAIL]', ', '.join(sorted(refs)) or 'nothing'))
    if bad:
        print('  the C library allocator (%s) keeps 16 KB puddles until exit: keymap.c must use AllocVec' % ', '.join(bad))
    elif not ok:
        print('  no _SysBase: the exec allocator is not used')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
