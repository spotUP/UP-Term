#!/usr/bin/env python3
"""sin(x) and cos(x) of one argument link against ixemul's libm at -O2 (gcc fuses the pair into
sincos -> cexpi -> a call to cexp, which ixemul's libm lacked: cpython's link failed on it).

Compiles tests/ixemul_sincos.c (the fused pair, and the same two functions through volatile
pointers so they stay separate calls; main compares them and prints PASS) with the installed
compiler ($AGCC, default ~/opt/amiga/bin/m68k-amigaos-gcc) for the default and
-m68020 -m68881 flag sets. The link must succeed and the object must really reference cexp (else
gcc did not fuse and the test proves nothing). The check that the numbers are equal runs on the
Amiga: `python3 tools/rig/ami.py put` the binary (built to $SINCOS_OUT) and exec it; PASS is
printed. Skipped when the compiler is absent."""
import os, re, subprocess, sys, tempfile

AGCC = os.environ.get('AGCC') or os.path.expanduser('~/opt/amiga/bin/m68k-amigaos-gcc')
NM = os.path.join(os.path.dirname(AGCC), 'm68k-amigaos-nm')
SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'ixemul_sincos.c')
CASES = [[], ['-m68020']]  # -m68881 inlines fsincos: no cexp


def main():
    if not os.access(AGCC, os.X_OK):
        print('[INFO] %s not installed: ixemul sincos test skipped' % AGCC)
        return 0
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        for flags in CASES:
            name = ' '.join(flags) or '(no CPU flag)'
            obj = os.path.join(d, 's.o')
            exe = os.environ.get('SINCOS_OUT') if flags == CASES[0] and os.environ.get('SINCOS_OUT') else os.path.join(d, 's')
            c = subprocess.run([AGCC, '-mcrt=ixemul', '-O2'] + flags + ['-c', SRC, '-o', obj], capture_output=True, text=True)
            fused = c.returncode == 0 and re.search(r' U _?cexp\w*$', subprocess.run([NM, obj], capture_output=True, text=True).stdout, re.M)
            r = subprocess.run([AGCC, '-mcrt=ixemul', '-O2'] + flags + [SRC, '-o', exe, '-lm'], capture_output=True, text=True)
            if r.returncode == 0 and fused:
                print('[OK] -mcrt=ixemul -O2 %s: sin+cos fused into cexp and linked' % name)
            else:
                fails += 1
                undef = sorted(set(re.findall(r"undefined reference to `([^']+)'", r.stderr)))
                print('[ERROR] -O2 %s: compile %d, link %d, cexp referenced %s, undefined %s'
                      % (name, c.returncode, r.returncode, bool(fused), undef or '-'))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
