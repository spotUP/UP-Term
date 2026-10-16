#!/usr/bin/env python3
"""__builtin_mul_overflow(size_t n, 4, &ptrdiff_t) keeps its test of the high word (gnulib
xalloc_oversized; coreutils cp/du/ls printed "memory exhausted" from hash.c).

n * 4 in DImode is `add.l lo,lo; addx.l hi,hi` twice. addx.l clears Z only for a non-zero
result and otherwise leaves Z alone, so after it Z does not describe the high word. bbb's mask
tracker (-fbbb=e) treated the DImode shift as a shift of the high register alone, put a
REG_BIT_MASK note on `tst.l hi`, and final's compare elimination dropped the tst: `addx.l d2,d2;
jne`, so ov(31) returned 1.

For each flag set: compile the repro at -O2 with the installed compiler ($AGCC, default
~/opt/amiga/bin/m68k-amigaos-gcc). No conditional branch may follow an addx directly. When
vamos is on PATH, the linked program (-noixemul) must also exit 0. Skipped when the compiler is
absent."""
import os, re, shutil, subprocess, sys, tempfile

AGCC = os.environ.get('AGCC') or os.path.expanduser('~/opt/amiga/bin/m68k-amigaos-gcc')
CASES = [[], ['-m68020'], ['-m68060']]
SRC = r'''#include <stddef.h>
#include <stdbool.h>
__attribute__((noinline)) bool ov(size_t n) { ptrdiff_t c; return __builtin_mul_overflow(n, 4, &c); }
int main(void) {
  volatile size_t a = 0, b = 31, c = 0x1fffffff, d = 0x20000000, e = (size_t)-1;
  if (ov(a)) return 1;
  if (ov(b)) return 2;
  if (ov(c)) return 3;
  if (!ov(d)) return 4;
  if (!ov(e)) return 5;
  return 0;
}
'''
INSN = re.compile(r'^\s+([a-z][a-z0-9.]*)\b')


def branch_after_addx(asm):
    """Lines 'addx..; jcc' in function _ov (a branch reading flags an addx left)."""
    bad, prev, inside = [], None, False
    for line in asm.splitlines():
        if re.match(r'^_ov:', line):
            inside = True
            continue
        if inside and re.match(r'^_\w+:', line):
            break
        m = INSN.match(line) if inside else None
        if not m:
            continue
        op = m.group(1)
        if prev and prev.startswith('addx') and re.match(r'^j(ne|eq|lt|ge|gt|le|mi|pl|cc|cs|hi|ls)', op):
            bad.append('%s; %s' % (prev, op))
        prev = op
    return bad


def main():
    if not os.access(AGCC, os.X_OK):
        print('[INFO] %s not installed: mul_overflow cc test skipped' % AGCC)
        return 0
    vamos = shutil.which('vamos')
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 'ov.c')
        open(src, 'w').write(SRC)
        for flags in CASES:
            name = ' '.join(flags) or '(no CPU flag)'
            r = subprocess.run([AGCC, '-O2', '-S', '-o', '-'] + flags + [src], capture_output=True, text=True)
            if r.returncode != 0 or '_ov:' not in r.stdout:
                fails += 1
                print('[ERROR] %s: compile exit %d: %s' % (name, r.returncode, r.stderr.strip()[-300:]))
                continue
            if 'addx' not in r.stdout:
                print('[INFO] %s: no addx in ov(), the DImode shift is coded otherwise' % name)
            bad = branch_after_addx(r.stdout)
            if bad:
                fails += 1
                print('[ERROR] %s: branch on flags left by addx, high word never tested: %s' % (name, bad))
                continue
            msg = 'every branch after the addx pair follows a tst/cmp'
            if vamos:
                exe = os.path.join(d, 'ov')
                r = subprocess.run([AGCC, '-O2', '-noixemul'] + flags + [src, '-o', exe], capture_output=True, text=True)
                cpu = ['-C', '68020'] if flags else []
                v = subprocess.run([vamos] + cpu + [exe], capture_output=True, text=True, timeout=120) \
                    if r.returncode == 0 else None
                if v is None or v.returncode != 0:
                    fails += 1
                    print('[ERROR] %s: link exit %d, vamos exit %s (1..5 = case that failed)'
                          % (name, r.returncode, v.returncode if v else '-'))
                    continue
                msg += ', vamos run exits 0'
            print('[OK] %s: %s' % (name, msg))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
