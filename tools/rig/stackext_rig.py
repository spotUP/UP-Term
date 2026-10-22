#!/usr/bin/env python3
"""stackext_rig.py -- the signal mask across ixemul's stack extension.

Installs ixemul.library (IXEMUL=, default the build295 one; IXNET= goes with
it, default the ixnet.library of the same build tree) into VTC:ixp6, puts it
first in LIBS:, runs tests/amiga/ixstackext (built with -fstack-extend; deep recursion makes
ixemul extend the stack) and passes when the program prints [OK]. Regression test for atomic_on() in ixemul's
library/stackextend.c: gcc 16 -O2 dropped the store of the saved mask and
main() started with stack garbage as its signal mask. Second case: ixexecstack
vfork+execve's the native stackprobe256k ($STACK: 262144); passes when it
runs on >= 262144 bytes under the default Stack (ixemul's compatible_startup).
The rig must be up; `make build/amiga/ixstackext build/amiga/ixexecstack
build/amiga/stackprobe` first."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import os, pathlib, re, shutil, sys
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
    # ixemul's execve of a native program honours its "$STACK: 262144" cookie
    # (compatible_startup in stdlib/execve.c): vfork + execve of stackprobe256k
    # under the rig's default Stack must give it at least that much.
    for n in ("ixexecstack", "stackprobe256k"):
        shutil.copyfile(ROOT / "build/amiga" / n, VTC / n)
    rc, out = ixpty_rig.run('Stack\nVTC:ixexecstack VTC:stackprobe256k', 60)
    sys.stdout.write(out)
    m = re.search(r'^stack (\d+)', out, re.M)
    got = int(m.group(1)) if m else 0
    print('stackext_rig: native exec stack %d (need >= 262144)' % got)
    ok = ok and got >= 262144
    print('stackext_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL', ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
