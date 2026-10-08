#!/usr/bin/env python3
"""stackext_rig.py -- the signal mask across ixemul's stack extension.

Installs ixemul.library (IXEMUL=, default the build295 one; IXNET= goes with
it, default the ixnet.library of the same build tree) into VTC:ixp6, puts it
first in LIBS:, runs tests/amiga/ixstackext (built with -fstack-extend; deep recursion makes
ixemul extend the stack) and passes when the program prints [OK]. Regression test for atomic_on() in ixemul's
library/stackextend.c: gcc 16 -O2 dropped the store of the saved mask and
main() started with stack garbage as its signal mask. The rig must be up;
`make build/amiga/ixstackext` first."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"

def main():
    shutil.copyfile(ROOT / "build/amiga/ixstackext", VTC / "ixstackext")
    ixpty_rig.use_ixemul()
    rc, out = ixpty_rig.run('VTC:ixstackext', 60)
    sys.stdout.write(out)
    ok = rc == 0 and '[OK]' in out and 'FAIL' not in out
    print('stackext_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL', ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
