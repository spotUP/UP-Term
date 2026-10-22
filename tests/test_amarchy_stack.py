#!/usr/bin/env python3
"""test_amarchy_stack.py -- amarchy-menu asks for a big stack (curses viewer and menu froze rig 4 without it).

  python3 tests/test_amarchy_stack.py [build/amiga/amarchy-menu]

Same check as tmux-amiga/tests/test_stack_cookie.py: the built binary holds a "$STACK: N" cookie (read by vsh,
the Shell and Workbench) with N >= MIN, and ixemul's `__stack` long (the stack ixemul runs main on) at N >= MIN. Exit 1 on a miss.
Needs the Amiga build (make amarchy-menu); wired into dist-check."""
import pathlib, re, struct, sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
MIN = 262144


def stack_symbol(d):
    """The long that the hunk file's symbol table puts at `___stack` (the C `__stack`), or None. ixemul's own
    default also defines the symbol, so the value, not the name, shows that menu.c set it."""
    u = lambda p: struct.unpack('>I', d[p:p + 4])[0]
    p = 4
    while u(p):
        p += 4 + 4 * u(p)
    p += 4
    first, last = u(p + 4), u(p + 8)
    p += 12 + 4 * (last - first + 1)
    body = None                                         # file offset of the current hunk's bytes
    while p < len(d):
        t = u(p) & 0x3FFFFFFF
        p += 4
        if t in (0x3E9, 0x3EA, 0x3F1):
            body = p + 4
            p += 4 + 4 * u(p)
        elif t == 0x3EB:
            body = None
            p += 4
        elif t in (0x3EC, 0x3F7, 0x3F8):                # relocs: (count, hunk, offsets...) until a 0 count
            while u(p):
                p += 8 + 4 * u(p)
            p += 4
        elif t == 0x3F0:                                # symbols: (name longs, name, value) until 0
            while u(p):
                n = (u(p) & 0xFFFFFF) * 4
                name = d[p + 4:p + 4 + n].rstrip(b'\0')
                val = u(p + 4 + n)
                p += 8 + n
                if name == b'___stack' and body is not None:
                    return u(body + val)
            p += 4
        elif t == 0x3F2:
            pass
        else:
            return None
    return None


def main():
    path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'build' / 'amiga' / 'amarchy-menu'
    data = path.read_bytes()
    fails = []
    m = re.search(rb'\$STACK: ?(\d+)', data)
    if not m:
        fails.append('%s holds no $STACK cookie: vsh starts it on a small shell stack' % path.name)
    elif int(m.group(1)) < MIN:
        fails.append('%s asks for %s bytes of stack, below %d' % (path.name, m.group(1).decode(), MIN))
    v = stack_symbol(data)
    if v is None or v < MIN:
        fails.append('%s sets ixemul __stack to %s, below %d: main runs on the default stack' % (path.name, v, MIN))
    for f in fails:
        print('[FAIL] ' + f)
    print('amarchy_stack: %d failed' % len(fails))
    sys.exit(1 if fails else 0)


main()
