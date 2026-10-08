#!/usr/bin/env python3
"""umask_rig.py -- vsh's umask builtin reaches the commands vsh starts.

Installs ixemul.library (IXEMUL=, default the build295 one) as ixpty_rig
does and runs tests/amiga/ixumask through vsh: a mask set with `umask 077`
must be the mask of the ixemul program (vsh hands it over as the local
variable UMASK; ixemul's ix_open.c reads it), the default is 0022, and the
mask is kept for the next command. --orig runs on the original library (A/B). The rig must be up;
`make build/amiga/vsh build/amiga/ixumask` first."""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"

def main():
    for name in ("vsh", "ixumask"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    ixpty_rig.use_ixemul("--orig" in sys.argv)
    ok = True
    # (command line, how often the output must show each text)
    for line, wants in (("VTC:ixumask", {"[UMASK] 0022": 1}),
                        ("umask 077; VTC:ixumask", {"[UMASK] 0077": 1, "[CREATE] 0600": 1}),
                        ("umask 077; VTC:ixumask; VTC:ixumask", {"[UMASK] 0077": 2}),
                        ("umask 027; VTC:ixumask", {"[UMASK] 0027": 1})):
        rc, out = ixpty_rig.run('VTC:vsh -c "%s"' % line, 60)
        sys.stdout.write("%s -> %s" % (line, out))
        for text, n in wants.items():
            ok = ok and out.count(text) == n
    print('umask_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL', ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
