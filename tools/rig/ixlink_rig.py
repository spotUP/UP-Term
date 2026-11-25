#!/usr/bin/env python3
"""ixlink_rig.py -- link() fails with an errno callers fall back on, never EIO.

mutt's safe_rename falls back to rename() only on EXDEV, ENOSYS, EPERM,
ENOTSUP, EOPNOTSUPP; ixemul's link() gave EIO for a bare relative target
(ledger IXEMUL-LINK). Installs ixemul.library (IXEMUL=, default the
ixemul-vtcon build295 one) first in LIBS: as ixpty_rig does, runs
tests/amiga/ixlink in VTC:, SYS:T and RAM:T (absolute and slash-less
link, plus the raw MakeLink IoErr) and passes on [OK]. --orig runs it on
the rig's own DH0:Libs ixemul. The rig must be up; `make build/amiga/ixlink`
first."""
import os, pathlib, shutil, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"

def main():
    orig = '--orig' in sys.argv[1:]
    shutil.copyfile(ROOT / "build/amiga/ixlink", VTC / "ixlink")
    ixpty_rig.use_ixemul(orig)
    rc, out = ixpty_rig.run('VTC:ixlink', 60)
    sys.stdout.write(out)
    ok = rc == 0 and '[OK]' in out and 'FAIL' not in out
    print('ixlink_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL',
          'DH0:Libs' if orig else ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
