#!/usr/bin/env python3
"""The 68080 move2.l load pair (-m68080 -mexperimental) never pairs a load whose address
register the first load overwrites, and never pairs volatile loads.

m68k.md's peephole2 "combine two move.l into move2.l (n,ax),r1:r2" merged
`move.l (a0),a0; move.l (4,a0),d1` into `move2.l (a0),a0:d1`, so the second load read the
old a0 + 4 instead of the new one (a linked-list walk read the wrong node's field). It also
merged two volatile reads into one move2.l. The fix adds !reg_overlap_mentioned_p (load 1's
destination, load 2's mem) and !MEM_VOLATILE_P on both mems (also on the store pair).

Compile the repro at -O2 with the installed compiler ($AGCC, default
~/opt/amiga/bin/m68k-amigaos-gcc) and read the asm: chase() may not hold a move2.l whose
first destination is its base register, vol() may not hold a move2.l at all, and plain()
must still hold one (the optimisation is kept). Skipped when the compiler is absent."""
import os, re, subprocess, sys, tempfile

AGCC = os.environ.get('AGCC') or os.path.expanduser('~/opt/amiga/bin/m68k-amigaos-gcc')
FLAGS = ['-O2', '-m68080', '-mexperimental']
SRC = r'''struct node { struct node *next; int val; };
struct pair { int a; int b; };
int chase(struct node *p, int n)
{
  int s = 0;
  while (n--) { int v; p = p->next; v = p->val; __asm__ volatile ("" : "+d" (v)); s += v; }
  return s;
}
int vol(volatile int *p) { int a = p[0]; int b = p[1]; return a * b; }
int plain(struct pair *p)
{
  int a = p->a, b = p->b;
  __asm__ ("" : "+d" (a), "+d" (b));
  return a - b;
}
'''
MOVE2 = re.compile(r'^\s+move2\.l\s+(\S+)')
FUNC = re.compile(r'^_(\w+):')
BASE = re.compile(r'(a[0-7]|sp)\)')


def move2_loads(asm):
    """(function, line, base register, first destination) for each move2.l load."""
    out, func = [], None
    for line in asm.splitlines():
        m = FUNC.match(line)
        if m:
            func = m.group(1)
            continue
        m = MOVE2.match(line)
        if not m or ')' not in m.group(1):
            continue
        op = m.group(1)
        src, _, dst = op.rpartition('),')
        if not dst:  # a store pair: r1:r2,(n,ax)
            continue
        b = BASE.search(src + ')')
        out.append((func, line.strip(), b.group(1) if b else '', dst.split(':')[0]))
    return out


def main():
    if not os.access(AGCC, os.X_OK):
        print('[INFO] %s not installed: move2 overlap test skipped' % AGCC)
        return 0
    with tempfile.TemporaryDirectory() as d:
        src = os.path.join(d, 'move2.c')
        open(src, 'w').write(SRC)
        r = subprocess.run([AGCC, '-S', '-o', '-'] + FLAGS + [src], capture_output=True, text=True)
    if r.returncode != 0 or '_chase:' not in r.stdout:
        print('[ERROR] compile exit %d: %s' % (r.returncode, r.stderr.strip()[-300:]))
        return 1
    loads = move2_loads(r.stdout)
    fails = 0
    for func, line, base, dst in loads:
        if base and dst == base:
            fails += 1
            print('[ERROR] %s: move2.l overwrites its own base before the second load: %s' % (func, line))
        if func == 'vol':
            fails += 1
            print('[ERROR] vol: volatile loads paired into one move2.l: %s' % line)
    if not any(f == 'plain' for f, _, _, _ in loads):
        fails += 1
        print('[ERROR] plain: the safe adjacent load pair is no longer a move2.l')
    if not fails:
        print('[OK] move2.l pairs plain loads only: no base overwrite, no volatile pair')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
