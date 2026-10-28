#!/usr/bin/env python3
"""CP2: pkg_rows.py check names the drifted file and row; --write refuses an id that already has rows.
Run: python3 -I tools/test_pkg_rows.py   (UPTERM_PORTS=<dir>; skips when upterm-ports is absent)"""
import contextlib, io, os, pathlib, shutil, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools' / 'rig'))
import paths
import pkg_rows

PORTS = pathlib.Path(os.environ.get('UPTERM_PORTS') or paths.repo('upterm-ports'))


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        try:
            rc = pkg_rows.main(argv)
        except SystemExit as e:
            rc = e.code
    return rc, out.getvalue()


@unittest.skipUnless((PORTS / 'Makefile').exists(), 'upterm-ports not found (set UPTERM_PORTS)')
class Check(unittest.TestCase):
    def copy(self, d):
        for n in ('dist', 'Makefile'):
            (shutil.copytree if (ROOT / n).is_dir() else shutil.copy)(ROOT / n, pathlib.Path(d) / n)

    def test_changed_slim_row_exits_one_naming_it(self):
        with tempfile.TemporaryDirectory() as d:
            self.copy(d)
            p = pathlib.Path(d) / 'dist/slim.txt'
            p.write_text(p.read_text().replace('bitchx  lib/bx', 'bitchx  lib/bxx'))
            rc, out = run(['check', 'bitchx', '--ports', str(PORTS), '--root', d])
            self.assertEqual(rc, 1)
            self.assertIn('dist/slim.txt: row missing: bitchx  lib/bx', out)

    def test_write_refuses_an_existing_id(self):
        with tempfile.TemporaryDirectory() as d:
            self.copy(d)
            rc, _ = run(['emit', 'bitchx', '--ports', str(PORTS), '--root', d, '--write'])
            self.assertIn('already has rows', str(rc))


if __name__ == '__main__':
    unittest.main()
