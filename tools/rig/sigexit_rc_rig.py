#!/usr/bin/env python3
"""sigexit_rc_rig.py -- a program ended by a signal returns rc 128+signal.

Installs ixemul.library (IXEMUL=, default the build295 one; IXNET= goes with
it) as ixpty_rig does and runs tests/amiga/ixsigexit: from AmigaDOS a
self-sent SIGTERM must give rc 143 and SIGINT rc 130, exit(3) still rc 3;
through vsh a background sleeper killed with `kill -TERM` must give
`wait` status 143. Regression test for ix_startup.c: sig_exit ends a
program with W_EXITCODE(0, sig) and WEXITSTATUS of that is 0. Point IXEMUL=
at the old library for the A/B (it must FAIL there). The rig must be up;
`make build/amiga/vsh build/amiga/ixsigexit` first."""
import sys, os, pathlib, shutil
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ixpty_rig

ROOT = pathlib.Path(__file__).resolve().parents[2]
VTC = paths.RIG / "vtc"


def main():
    for name in ("vsh", "ixsigexit"):
        shutil.copyfile(ROOT / "build/amiga" / name, VTC / name)
    ixpty_rig.use_ixemul()
    ok = True
    for label, cmd, want in (
            ("SIGTERM from AmigaDOS", 'VTC:ixsigexit term', 143),
            ("SIGINT from AmigaDOS", 'VTC:ixsigexit int', 130),
            ("exit(3)", 'VTC:ixsigexit exit3', 3)):
        rc, out = ixpty_rig.run(cmd, 60)
        good = rc == want
        ok = ok and good
        print('%s: rc %d (want %d) %s' % (label, rc, want, 'ok' if good else 'FAIL'))
    rc, out = ixpty_rig.run(
        'VTC:vsh -c "VTC:ixsigexit sleep & kill -TERM $!; wait $!; echo rc=$?"', 60)
    good = 'rc=143' in out
    ok = ok and good
    print('SIGTERM through vsh: %s %s' % (out.strip().replace('\n', ' | '), 'ok' if good else 'FAIL (want rc=143)'))
    print('sigexit_rc_rig: %s (ixemul %s)' % ('OK' if ok else 'FAIL', ixpty_rig.IXEMUL))
    return 0 if ok else 1

if __name__ == '__main__':
    sys.exit(main())
