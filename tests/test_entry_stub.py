#!/usr/bin/env python3
"""Every binary built without a C startup (vlink -nostdlib, no crt) must start
with handler/handler_start.s: AmigaDOS runs the first byte of the first code
hunk, so a compiler-ordered first function becomes the entry point and the
handler hangs (vtcon-handler, 79872bd/d1a2e89).

Two checks per binary: (1) the Makefile rule that links it names
handler_start.o before any other object; (2) when the binary is built, its
first code hunk starts with JMP abs.l (4EF9), which no compiled function does.
(up-console.device starts with a RomTag, not a handler entry: not covered.)"""
import os, re, struct, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BUILD = os.path.join(ROOT, 'build', 'amiga')
STARTLESS = ['vtcon-handler', 'pty-handler']
HUNK_HEADER, HUNK_CODE = 0x3F3, 0x3E9
fails = 0

def check(ok, what):
    global fails
    print('%s %s' % ('[OK]' if ok else '[FAIL]', what))
    if not ok:
        fails += 1

def link_line(mk, name):
    """The vlink invocation of the rule that builds build/amiga/<name>."""
    m = re.search(r'^\$\(BUILD\)/amiga/%s:.*?\n((?:\t.*\n?)+)' % re.escape(name), mk, re.M)
    if not m:
        return None
    body = m.group(1).replace('\\\n', ' ')
    for line in body.split('\n'):
        if 'vlink' in line and '-nostdlib' in line:
            return line
    return None

def first_code_word(path):
    d = open(path, 'rb').read()
    if struct.unpack('>I', d[:4])[0] != HUNK_HEADER:
        return None
    off = 4
    while struct.unpack('>I', d[off:off + 4])[0] != 0:   # resident library names
        off += 4 + 4 * struct.unpack('>I', d[off:off + 4])[0]
    off += 4
    n, first, last = struct.unpack('>III', d[off:off + 12])
    off += 12 + 4 * n
    tag = struct.unpack('>I', d[off:off + 4])[0] & 0x3FFFFFFF
    if tag != HUNK_CODE:
        return None
    return struct.unpack('>H', d[off + 8:off + 10])[0]

mk = open(os.path.join(ROOT, 'Makefile')).read()
for name in STARTLESS:
    ln = link_line(mk, name)
    check(ln is not None, '%s: its vlink -nostdlib line is found in the Makefile' % name)
    if ln:
        objs = re.findall(r'(\S+\.o)\b', ln)
        check(bool(objs) and os.path.basename(objs[0]) == 'handler_start.o',
              '%s: handler_start.o is linked first (got %s)' % (name, objs[0] if objs else 'nothing'))
    path = os.path.join(BUILD, name)
    if os.path.exists(path):
        w = first_code_word(path)
        check(w == 0x4EF9, '%s: the first instruction of the binary is JMP abs.l (%s)' %
              (name, 'none' if w is None else '%04X' % w))
    else:
        print('[INFO] %s not built: binary check skipped' % name)
sys.exit(1 if fails else 0)
