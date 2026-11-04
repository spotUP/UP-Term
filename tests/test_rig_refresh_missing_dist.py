#!/usr/bin/env python3
"""test_rig_refresh_missing_dist.py -- the rig refresh skips a missing dist userland file and warns once.

  python3 -I tests/test_rig_refresh_missing_dist.py

RIG-REFRESH-CTAGS: refresh_programs/build_drift read USERLAND_SRC/<name> for every name userland_names() listed; a name
gone by then (build/dist rebuilt: .../userland/bin/ctags) died with FileNotFoundError. Now the file is skipped and one
[WARN] line names it. Temp tree, no rig. Exit 1 on failure."""
import contextlib, io, pathlib, shutil, sys, tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
import paths
import rig

d = pathlib.Path(tempfile.mkdtemp())
try:
    built, vtc, src = d / 'built', d / 'vtc', d / 'userland'
    built.mkdir(); src.mkdir()
    bin_ = vtc / paths.UPTERM_DRAWER / 'bin'
    bin_.mkdir(parents=True)
    (src / 'here').write_bytes(b'new')
    (bin_ / 'here').write_bytes(b'old')
    (bin_ / 'ctags').write_bytes(b'old')
    rig.USERLAND_SRC = src
    rig.userland_names = lambda: ['ctags', 'here']      # ctags listed, then gone from disk
    rig.ensure_built = lambda files, run=None: []
    rig.ROOT = d
    out = io.StringIO()
    try:
        with contextlib.redirect_stdout(out):
            done = rig.refresh_programs(vtc, built)
            rig.refresh_programs(vtc, built)
    except FileNotFoundError as e:
        sys.exit('FAIL: refresh crashed on a missing dist file: %s' % e)
    warn = [l for l in out.getvalue().splitlines() if l.startswith('[WARN]')]
    if len(warn) != 1 or 'ctags' not in warn[0]:
        sys.exit('FAIL: want exactly one [WARN] naming ctags, got %r' % warn)
    if (bin_ / 'here').read_bytes() != b'new' or not any('here' in n for n in done):
        sys.exit('FAIL: the present file was not refreshed (%r)' % done)
finally:
    shutil.rmtree(d)
print('OK')
