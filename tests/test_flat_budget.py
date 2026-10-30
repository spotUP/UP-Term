#!/usr/bin/env python3
"""test_flat_budget.py -- FLAT-C3 caller-context budget of the amarchy-flat sysiclass dispatcher.

  python3 tests/test_flat_budget.py build/amiga/flat_asm/flat.s build/amiga/flat_asm/flat_glyph.s build/amiga/flat_asm/flat_core.s

The dispatcher entries (c2_disp_reg, c2_disp_stk) run on the stack of whichever task is drawing a window: a CLI program
under `Stack 4096`, or input.device. From vbcc's own assembly (vc -S, the `; stacksize=N` line it writes per function,
saved registers plus locals) this checks
  1. every entry function's own frame is <= ENTRY_MAX bytes,
  2. every function reachable from an entry has a frame <= HELPER_MAX bytes and the deepest call chain (frames plus a
     4 byte return address per call) is <= CHAIN_MAX bytes,
  3. nothing reachable calls a libc or compiler-runtime symbol: only functions of these objects, `jsr (aN)` through the
     saved hook / fill callback, and OS library calls `jsr -N(a6)`.
Needs the Amiga toolchain (make flat-budget-asm); wired into dist-check. Exit 1 on a miss."""
import re, sys

ENTRY_MAX = 64
HELPER_MAX = 80
CHAIN_MAX = 384
ENTRIES = ('_c2_disp_reg', '_c2_disp_stk')
ALLOWED_EXTERN = re.compile(r'^_flat_[a-z0-9_]+$')


def parse(paths):
    """{label: (frame, [call targets])} over every function of the given assembly files. vbcc writes `; stacksize=`
    for only some functions, so the frame is read from the prologue itself: the locals (`subq/sub/lea ...,a7` before
    the register save) plus the saved registers (`movem.l lK,-(a7)`; vbcc defines `lK+2 equ <bytes pushed>`)."""
    funcs = {}
    for path in paths:
        lines = open(path).read().split('\n')
        equ = {}
        for s in lines:
            m = re.match(r'^(l\d+)\s+equ\s+(\d+)$', s)
            if m:
                equ[m.group(1)] = int(m.group(2))
        label, calls, frame, want, prologue = None, [], 0, False, False
        def close():
            if label:
                funcs[label] = (frame, calls)
        for s in lines:
            if s.startswith('\tcnop\t0,4'):
                close()
                want, label, calls, frame, prologue = True, None, [], 0, False
                continue
            if want and re.match(r'^[A-Za-z_][A-Za-z_0-9]*$', s):
                label, want, prologue = s, False, True
                continue
            if not label:
                continue
            if prologue:
                m = re.match(r'^\t(?:subq\.w|sub\.w|sub\.l|subq\.l)\t#(\d+),a7$', s)
                if m:
                    frame += int(m.group(1))
                    continue
                m = re.match(r'^\tlea\t-(\d+)\(a7\),a7$', s)
                if m:
                    frame += int(m.group(1))
                    continue
                m = re.match(r'^\tmovem\.l\tl(\d+),-\(a7\)$', s)
                if m:
                    frame += equ.get('l%d' % (int(m.group(1)) + 2), 0)
                    prologue = False
                    continue
                if re.match(r'^\t(?:link|move\.l\t[ad]\d,-\(a7\))', s):
                    frame += 4 if not s.startswith('\tlink') else 0
                    continue
                prologue = False
            m = re.match(r'^\t(?:jsr|bsr(?:\.[wl])?|jmp)\t(.*)$', s)
            if m:
                calls.append(m.group(1).strip())
        close()
    return funcs


def main(paths):
    funcs = parse(paths)
    bad = []
    for e in ENTRIES:
        if e not in funcs:
            print('FAIL: entry %s not found in %s (is it still exported?)' % (e, paths))
            return 1
    seen = {}

    def chain(f, stack):
        if f in stack:
            bad.append('recursion through %s' % f)
            return 0
        size, calls = funcs[f]
        worst = 0
        for t in calls:
            if re.match(r'^-?\d+\(a\d\)$', t) or re.match(r'^\(a\d\)$', t):
                continue                              # OS library call (a6) or a callback through a register
            if t in funcs:
                worst = max(worst, 4 + chain(t, stack + [f]))
            elif not ALLOWED_EXTERN.match(t):
                bad.append('%s calls %s (not an object function, register call or OS call)' % (f, t))
        seen[f] = size
        return size + worst

    total = {}
    for e in ENTRIES:
        total[e] = chain(e, [])
        if funcs[e][0] > ENTRY_MAX:
            bad.append('%s own frame %d > %d' % (e, funcs[e][0], ENTRY_MAX))
        if total[e] > CHAIN_MAX:
            bad.append('%s deepest chain %d > %d' % (e, total[e], CHAIN_MAX))
    for f, size in sorted(seen.items()):
        if size > HELPER_MAX:
            bad.append('%s frame %d > %d' % (f, size, HELPER_MAX))
    for e in ENTRIES:
        print('flat budget: %s own frame %d, deepest chain %d' % (e, funcs[e][0], total[e]))
    print('flat budget: reachable frames %s' % sorted(seen.values(), reverse=True))
    for b in bad:
        print('FAIL: ' + b)
    print('flat budget: %s' % ('FAIL' if bad else 'PASS'))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
