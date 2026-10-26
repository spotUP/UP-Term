#!/usr/bin/env python3
"""CP8, the one reachability test of tools/pkg_rows.py: main(['emit', 'bitchx', ...]) asks the ports Makefile
(call counter on the recipe-field reader, sentinel: the print-bitchx_SUMMARY answer is in the output) and
produces the committed rows and files byte for byte. tree (no _FEED_ID) produces none.
Run: python3 -I tests/test_pkg_rows.py   (UPTERM_PORTS=<dir> overrides ../upterm-ports; skips when absent)"""
import contextlib, io, os, pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkg_rows, pkgs

PORTS = pathlib.Path(os.environ.get('UPTERM_PORTS', ROOT.parent / 'upterm-ports'))


@unittest.skipUnless((PORTS / 'Makefile').exists(), 'upterm-ports not found at %s (set UPTERM_PORTS)' % PORTS)
class PkgRows(unittest.TestCase):
    def run_main(self, argv, mutate=None):
        calls = []
        real = pkg_rows.read_field

        def counting(ports, pkg, field):
            v = real(ports, pkg, field)
            calls.append((pkg, field, v))
            return mutate(pkg, field, v) if mutate else v
        pkg_rows.read_field = counting
        try:
            out = io.StringIO()
            with contextlib.redirect_stdout(out):
                rc = pkg_rows.main(argv)
        finally:
            pkg_rows.read_field = real
        return rc, out.getvalue(), calls

    def test_emit_bitchx_equals_the_committed_rows(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out, calls = self.run_main(['emit', 'bitchx', '--ports', str(PORTS), '--out', d])
            self.assertEqual(rc, 0)
            # the generator really asked the ports Makefile: the recipe's summary came back through it
            summary = [v for _, f, v in calls if f == 'SUMMARY'][0]
            self.assertEqual(summary, pkgs.summary('bitchx'))
            d = pathlib.Path(d)
            row = (d / 'dist/packages.row').read_text()
            self.assertIn(row, (ROOT / 'dist/packages.txt').read_text())
            slim_rows = (d / 'dist/slim.rows').read_text().splitlines()
            self.assertEqual(len(slim_rows), 4)
            self.assertEqual(slim_rows, [l for l in (ROOT / 'dist/slim.txt').read_text().splitlines() if l.startswith('bitchx ')])
            made = sorted(p.name for p in (d / 'dist/addons/bitchx').iterdir())
            self.assertEqual(len(made), 6)
            for n in made:
                self.assertEqual((d / 'dist/addons/bitchx' / n).read_bytes(), (ROOT / 'dist/addons/bitchx' / n).read_bytes(), n)
            # CP3: a package is data only. No shared-code hunks are made, and the feed Description comes from the recipe.
            self.assertFalse((d / 'pkgs.py.line').exists() or (d / 'Makefile.snippet').exists())
            self.assertNotIn('bitchx', pkgs.SUMMARY)
            self.assertNotIn('dist-bitchx', (ROOT / 'Makefile').read_text().replace('dist-<id>', ''))
            self.assertEqual(pkgs.summary('bitchx'), summary)
            self.assertNotEqual(summary, '')

    def test_a_changed_bins_changes_the_slim_rows(self):
        with tempfile.TemporaryDirectory() as d:
            self.run_main(['emit', 'bitchx', '--ports', str(PORTS), '--out', d],
                          mutate=lambda p, f, v: 'bin/BitchX2' if f == 'BINS' else v)
            rows = (pathlib.Path(d) / 'dist/slim.rows').read_text()
            self.assertIn('bitchx  bin/BitchX2', rows)
            self.assertNotIn('bitchx  bin/BitchX\n', rows)

    def test_tree_has_no_feed_rows(self):
        with tempfile.TemporaryDirectory() as d:
            rc, out, calls = self.run_main(['emit', 'tree', '--ports', str(PORTS), '--out', d])
            self.assertEqual(rc, 0)
            self.assertIn('no feed rows', out)
            self.assertEqual(list(pathlib.Path(d).iterdir()), [])

    def test_check_exits_zero_on_the_committed_tree_and_one_on_drift(self):
        self.assertEqual(self.run_main(['check', 'bitchx', '--ports', str(PORTS)])[0], 0)
        rc, out, _ = self.run_main(['check', 'bitchx', '--ports', str(PORTS)],
                                   mutate=lambda p, f, v: v + 'x' if f == 'LICENSE' else v)
        self.assertEqual(rc, 1)
        self.assertIn('dist/addons/bitchx/', out)


STAGED = ROOT / 'build/dist/up-bitchx'
MAIN_STAGED = pathlib.Path('/Users/spot/Code/UP-Term/vtcon/build/dist/up-bitchx')


class NoPortsCheckout(unittest.TestCase):
    """A recipe-backed package whose recipe cannot be found must fail loudly, never take the archive name as its
    Description (the silent fallback CP3 left behind)."""

    def test_missing_ports_dir_is_an_error_naming_the_id_and_path(self):
        row = [r for r in pkgs.rows() if r['id'] == 'bitchx'][0]
        with tempfile.TemporaryDirectory() as build, tempfile.TemporaryDirectory() as empty:
            (pathlib.Path(build) / 'dist').mkdir()
            src = STAGED if STAGED.exists() else MAIN_STAGED
            if not src.exists():
                self.skipTest('up-bitchx not staged (make dist)')
            (pathlib.Path(build) / 'dist' / row['archive']).symlink_to(src)
            old = os.environ.get('UPTERM_PORTS')
            os.environ['UPTERM_PORTS'] = empty
            try:
                with self.assertRaises(SystemExit) as cm:
                    pkgs.build_ipk(row, build)
            finally:
                if old is None:
                    del os.environ['UPTERM_PORTS']
                else:
                    os.environ['UPTERM_PORTS'] = old
            msg = str(cm.exception)
            self.assertIn('bitchx', msg)
            self.assertIn(empty, msg)


if __name__ == '__main__':
    unittest.main()
