#!/usr/bin/env python3
"""test_rig_refresh_missing_dist.py -- the rig refresh skips a missing dist userland file and warns once.

  python3 -I tests/test_rig_refresh_missing_dist.py

RIG-REFRESH-CTAGS: refresh_programs/build_drift read USERLAND_SRC/<name> for every name userland_names() listed; a name
gone by then (build/dist rebuilt: .../userland/bin/ctags) died with FileNotFoundError. Now the file is skipped and one
[WARN] line names it. Temp tree, no rig. Exit 1 on failure."""
import contextlib, io, pathlib, shutil, sys, tempfile, unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'tools' / 'rig'))
import paths
import rig

class MissingDist(unittest.TestCase):
    def test_refresh_skips_missing(self):
        d = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, d, ignore_errors=True)
        built, vtc, src = d / 'built', d / 'vtc', d / 'userland'
        built.mkdir(); src.mkdir()
        bin_ = vtc / paths.UPTERM_DRAWER / 'bin'
        bin_.mkdir(parents=True)
        (src / 'here').write_bytes(b'new')
        (bin_ / 'here').write_bytes(b'old')
        (bin_ / 'ctags').write_bytes(b'old')
        out = io.StringIO()
        # patched per test and restored on exit: a module-level stub polluted every later test in the process
        with mock.patch.object(rig, 'USERLAND_SRC', src), \
             mock.patch.object(rig, 'userland_names', lambda: ['ctags', 'here']), \
             mock.patch.object(rig, 'ensure_built', lambda files, run=None: []), \
             mock.patch.object(rig, 'ROOT', d), contextlib.redirect_stdout(out):
            done = rig.refresh_programs(vtc, built)
            rig.refresh_programs(vtc, built)
        warn = [l for l in out.getvalue().splitlines() if l.startswith('[WARN]')]
        self.assertTrue(len(warn) == 1 and 'ctags' in warn[0], warn)
        self.assertEqual((bin_ / 'here').read_bytes(), b'new')
        self.assertTrue(any('here' in n for n in done), done)


if __name__ == '__main__':
    unittest.main()
