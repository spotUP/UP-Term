#!/usr/bin/env python3
"""ixnamelen_rig.py -- a create whose name the handler shortens fails.

RAM: on OS 3.1 keeps the first 30 characters of a name; git's 38-character
loose-object names landed under 30 and broke the object store. Installs
ixemul.library (IXEMUL=, default the ixemul-vtcon build295 one, with its
ixnet) first in LIBS: as ixpty_rig does, runs tests/amiga/ixnamelen on
RAM: (open with O_CREAT, mkdir, rename, link, symlink of a 38-character
name: ENAMETOOLONG, nothing shortened left behind, a file already under
the 30-character name untouched) and passes on [OK]. --orig runs it on the
rig's own DH0:Libs ixemul (the A/B: that one fails). The rig must be up;
`make build/amiga/ixnamelen` first."""
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"

def main():
    orig = '--orig' in sys.argv[1:]
    shutil.copyfile(ROOT / "build/amiga/ixnamelen", VTC / "ixnamelen")
    ixpty_rig.use_ixemul(orig)
    rc, out = ixpty_rig.run('VTC:ixnamelen', 60)
    sys.stdout.write(out)
    ok = rc == 0 and '[OK]' in out and 'FAIL' not in out
    print('ixnamelen_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL',
          'DH0:Libs' if orig else ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
