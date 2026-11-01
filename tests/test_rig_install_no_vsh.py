#!/usr/bin/env python3
"""test_rig_install_no_vsh.py -- rig.py install without build/amiga/vsh exits with one clear message.

  python3 -I tests/test_rig_install_no_vsh.py

stage_upterm copied vsh unguarded and died with a raw FileNotFoundError. require_vsh() must exit non-zero naming
the missing file and the make target, before anything is staged. Exit 1 on failure."""
import pathlib, sys, tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
import rig

with tempfile.TemporaryDirectory() as t:
    dest = pathlib.Path(t) / 'UP-Term'
    vsh = pathlib.Path(t) / 'build/amiga/vsh'
    try:
        rig.stage_upterm(dest, vsh)
    except SystemExit as e:
        msg = str(e.code)
        if not (str(vsh) in msg and 'make build/amiga/vsh' in msg):
            sys.exit('FAIL: message does not name the file and the make target: %r' % msg)
        if dest.exists():
            sys.exit('FAIL: staging began before the vsh check (%s exists)' % dest)
        print('OK missing vsh exits with: %s' % msg)
    except BaseException as e:
        sys.exit('FAIL: raised %s instead of a clear exit' % type(e).__name__)
    else:
        sys.exit('FAIL: no exit for a missing vsh')
