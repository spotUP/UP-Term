#!/usr/bin/env python3
"""CP2: pkg_rows.py check names the drifted file and row; --write refuses an id that already has rows.
Run: python3 -I tools/test_pkg_rows.py   (UPTERM_PORTS=<dir>; skips when upterm-ports is absent)"""
import contextlib, io, pathlib, shutil, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools' / 'rig'))
import paths
import pkg_rows

PORTS = paths.ports()


def run(argv):
    out = io.StringIO()
    with contextlib.redirect_stdout(out):
        try:
            rc = pkg_rows.main(argv)
        except SystemExit as e:
            rc = e.code
    return rc, out.getvalue()


# bitchx is the feed package these cases drive; a ports checkout on a branch older than its feed fields
# (b832339) makes pkg_rows answer "no _FEED_ID" with rc 0, which is no drift check at all
RECIPE = PORTS / 'pkgs/bitchx/recipe.mk'
HAS_FEED = RECIPE.exists() and 'bitchx_FEED_ID' in RECIPE.read_text(errors='replace')


@unittest.skipUnless((PORTS / 'Makefile').exists(), 'upterm-ports not found (set UPTERM_PORTS)')
@unittest.skipUnless(HAS_FEED, 'upterm-ports at %s has no bitchx_FEED_ID (a branch before b832339): set UPTERM_PORTS to a current checkout' % PORTS)
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


class UncoveredInstalls(unittest.TestCase):
    def test_shell_loop_variable_path_is_not_a_missing_row(self):
        cmd = 'cp x /sys/u/bin/git && for h in a b; do printf y > /sys/u/bin/$$h; done && cp x /sys/u/bin/other'
        old = pkg_rows.read_field, pkg_rows.read_make
        pkg_rows.read_field = lambda *a: cmd
        pkg_rows.read_make = lambda p, v: {'SYSROOT': '/sys', 'PREFIX': '/u'}[v]
        try:
            self.assertEqual(pkg_rows.uncovered_installs('.', 'p', ['bin/git'], []), ['bin/other'])
        finally:
            pkg_rows.read_field, pkg_rows.read_make = old


if __name__ == '__main__':
    unittest.main()
