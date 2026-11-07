#!/usr/bin/env python3
"""test_amarchy_frames.py -- no Amarchy function opens a stack frame above CEILING bytes.

  python3 tests/test_amarchy_frames.py [build/amiga/Amarchy ...]

Why: Amarchy's restart used 16780 bytes of a 16384 stack (rig 177) because a handful of functions each held
a frame of 4-9 KB (an am_prefs by value is 4640 bytes). The stack a program declares ($STACK cookie,
tests/test_amarchy_stack.py) only holds when the deepest call chain is short of it, and the chain is a sum
of frames: a ceiling on one frame is the invariant that can be checked on the built binary. 4096 bytes is
about 4 tool-sized buffers of a path (512): enough for every ordinary function here, and any frame above it
is a buffer that belongs in the module's static scratch or on the heap.

The vbcc programs carry no symbol hunk, so a frame is named by its code-hunk offset. A frame opens with
link.w a5,#-N (4E55 disp16), link.l a5,#-N (480D disp32), lea -N(sp),sp (4FEF disp16) or suba.l #N,sp
(9FFC imm32); the scan reads only the code hunks (HUNK_CODE), so data bytes of the same value are not seen.
Exit 1 when any program has a frame above the ceiling."""
import pathlib, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
CEILING = 4096
PROGRAMS = ('Amarchy', 'amarchy-gui', 'amarchy-flat', 'amarchy-index', 'amarchy-icons', 'AmTile')


def code_hunks(d):
    """(hunk number, bytes) of every HUNK_CODE of a hunk executable."""
    u = lambda p: struct.unpack('>I', d[p:p + 4])[0]
    p = 4
    while u(p):
        p += 4 + 4 * u(p)
    p += 4
    first, last = u(p + 4), u(p + 8)
    p += 12 + 4 * (last - first + 1)
    n = first
    out = []
    while p < len(d):
        t = u(p) & 0x3FFFFFFF
        p += 4
        if t in (0x3E9, 0x3EA, 0x3F1):
            ln = 4 * (u(p) & 0x3FFFFFFF)
            if t == 0x3E9:
                out.append((n, d[p + 4:p + 4 + ln]))
            p += 4 + (ln if t != 0x3F1 else ln)
        elif t == 0x3EB:
            p += 4
        elif t == 0x3EC:
            while u(p):
                p += 8 + 4 * u(p)
            p += 4
        elif t in (0x3F7, 0x3F8):                       # 16-bit entries (count, hunk, offsets...), padded to a long
            h = lambda q: struct.unpack('>H', d[q:q + 2])[0]
            while h(p):
                p += 4 + 2 * h(p)
            p += 2
            p = (p + 3) & ~3
        elif t == 0x3F0:
            while u(p):
                p += 8 + 4 * (u(p) & 0xFFFFFF)
            p += 4
        elif t == 0x3F2:
            n += 1
        else:
            break
    return out


def frames(code):
    """(offset, size) for every frame opening in a code hunk, word aligned."""
    res = []
    for o in range(0, len(code) - 6, 2):
        w = code[o:o + 2]
        if w == b'\x4e\x55':                            # link.w a5,#-N
            n = -struct.unpack('>h', code[o + 2:o + 4])[0]
        elif w == b'\x48\x0d':                          # link.l a5,#-N
            n = -struct.unpack('>i', code[o + 2:o + 6])[0]
        elif w == b'\x4f\xef':                          # lea -N(sp),sp
            n = -struct.unpack('>h', code[o + 2:o + 4])[0]
        elif w == b'\x9f\xfc':                          # suba.l #N,sp
            n = struct.unpack('>i', code[o + 2:o + 6])[0]
        else:
            continue
        if n > 0:
            res.append((o, n))
    return res


def scan(path, ceiling=CEILING):
    """['name: frame N at offset X', ...] for each frame above the ceiling."""
    bad = []
    for n, code in code_hunks(pathlib.Path(path).read_bytes()):
        for o, size in frames(code):
            if size > ceiling:
                bad.append('%s: frame %d at offset hunk %d+0x%x' % (pathlib.Path(path).name, size, n, o))
    return bad


def main():
    paths = [pathlib.Path(a) for a in sys.argv[1:]] or [ROOT / 'build' / 'amiga' / p for p in PROGRAMS]
    fails = []
    for p in paths:
        if not p.exists():
            print('[SKIP] %s not built' % p.name)
            continue
        fails += scan(p)
    for f in fails:
        print('[FAIL] ' + f)
    print('amarchy_frames: %d over %d bytes' % (len(fails), CEILING))
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
