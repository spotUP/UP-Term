#!/usr/bin/env python3
"""Two adjacent zero stores of different sizes stay two stores (zlib deflate at -O2: git loose
objects had a stray 0x00 before the adler32 trailer, "inflate: data stream error").

m68k.md's peephole2 "combine clr if possible" merged `clr.w x(a0); clr.l x+2(a0)` into
`clr.l x(a0)`: it checked only the first store's mode, so the merged store cleared four of the
six bytes. zlib's bi_windup does `s->bi_buf = 0; s->bi_valid = 0;` (ush then int, adjacent), so
bi_valid kept its low word and the next bi_flush wrote an extra byte.

For each flag set: compile the repro at -O2 with the installed compiler ($AGCC, default
~/opt/amiga/bin/m68k-amigaos-gcc). f() must still clear offset 14 with a store that covers four
bytes. When vamos is on PATH (not for -m68080), the linked program (-noixemul) must also exit 0.
Skipped when the compiler is absent."""
import os, re, shutil, subprocess, sys, tempfile

AGCC = os.environ.get('AGCC') or os.path.expanduser('~/opt/amiga/bin/m68k-amigaos-gcc')
CASES = [[], ['-m68020'], ['-m68060'], ['-m68080']]
SRC = r'''struct S { char pad[12]; unsigned short b; int v; };
__attribute__((noinline)) void f(struct S *s) { s->b = 0; s->v = 0; }
int main(void) {
  struct S s;
  __builtin_memset(&s, 0xff, sizeof s);
  f(&s);
  if (s.b) return 1;
  if (s.v) return 2;
  return 0;
}
'''


def body(asm, fn):
    """Instruction lines of function fn."""
    out, inside = [], False
    for line in asm.splitlines():
        if re.match(r'^_%s:' % fn, line):
            inside = True
            continue
        if inside and re.match(r'^_\w+:', line):
            break
        if inside and line.startswith('\t') and not line.strip().startswith('.'):
            out.append(line.strip())
    return out


def covers_v(insns):
    """True when some store writes all of s->v (offset 14..17)."""
    for i in insns:
        m = re.match(r'^(clr|move)\.([lq])\s.*\((\d+),a\d\)$', i)
        if m and int(m.group(3)) in (14,) + ((10, 12, 14) if m.group(2) == 'q' else ()):
            return True
    return False


def main():
    if not os.access(AGCC, os.X_OK):
        print('[INFO] %s not installed: clr merge test skipped' % AGCC)
        return 0
    vamos = shutil.which('vamos')
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 'clr.c')
        open(src, 'w').write(SRC)
        for flags in CASES:
            name = ' '.join(flags) or '(no CPU flag)'
            r = subprocess.run([AGCC, '-O2', '-S', '-o', '-'] + flags + [src], capture_output=True, text=True)
            if r.returncode != 0 or '_f:' not in r.stdout:
                fails += 1
                print('[ERROR] %s: compile exit %d: %s' % (name, r.returncode, r.stderr.strip()[-300:]))
                continue
            insns = body(r.stdout, 'f')
            if not covers_v(insns):
                fails += 1
                print('[ERROR] %s: s->v (offset 14) not fully cleared: %s' % (name, '; '.join(insns)))
                continue
            msg = 's->v cleared by its own long store'
            if vamos and '-m68080' not in flags:
                exe = os.path.join(d, 'clr')
                r = subprocess.run([AGCC, '-O2', '-noixemul'] + flags + [src, '-o', exe], capture_output=True, text=True)
                cpu = ['-C', '68020'] if flags else []
                v = subprocess.run([vamos] + cpu + [exe], capture_output=True, text=True, timeout=120) \
                    if r.returncode == 0 else None
                if v is None or v.returncode != 0:
                    fails += 1
                    print('[ERROR] %s: link exit %d, vamos exit %s (1 = s->b, 2 = s->v not cleared)'
                          % (name, r.returncode, v.returncode if v else '-'))
                    continue
                msg += ', vamos run exits 0'
            print('[OK] %s: %s' % (name, msg))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
