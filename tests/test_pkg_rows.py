#!/usr/bin/env python3
"""CP8, the one reachability test of tools/pkg_rows.py: main(['emit', 'bitchx', ...]) asks the ports Makefile
(call counter on the recipe-field reader, sentinel: the print-bitchx_SUMMARY answer is in the output) and
produces the committed rows and files byte for byte. tree (no _FEED_ID) produces none.
Run: python3 -I tests/test_pkg_rows.py   (UPTERM_PORTS=<dir> overrides ../upterm-ports; skips when absent)"""
import contextlib, io, os, pathlib, shutil, subprocess, sys, tempfile, unittest

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


MOCK_MAKEFILE = """SYSROOT := /sys
PREFIX := /UP-Term
fig_FEED_ID := up-fig
fig_SUMMARY := Big letters
fig_BINS := bin/fig
fig_LICENSE := BSD-3-Clause
fig_VERSION := 1
fig_INSTALL_CMD = install -d $(SYSROOT)$(PREFIX)/bin $(SYSROOT)$(PREFIX)/share/fig && install -m 644 f $(SYSROOT)$(PREFIX)/share/fig/
%(data)s
print-%%:
\t@echo '$($*)'
"""


class InstallStepCoverage(unittest.TestCase):
    """D1: a path the recipe's install step puts under UP-Term: that no _BINS/_DATA row covers is left out of the
    package without a word. emit stops and check fails naming it."""

    def mock(self, d, data):
        d = pathlib.Path(d)
        (d / 'Makefile').write_text(MOCK_MAKEFILE % {'data': data})
        (d / 'tools').mkdir()
        shutil.copy(PORTS / 'tools/recipe-var.sh', d / 'tools/recipe-var.sh')
        return d

    def run_cli(self, argv):
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out):
                rc = pkg_rows.main(argv)
        except SystemExit as e:
            rc = str(e)
        return rc, out.getvalue()

    def test_an_installed_drawer_without_data_fails_emit_and_check(self):
        with tempfile.TemporaryDirectory() as d, tempfile.TemporaryDirectory() as out:
            ports = self.mock(d, '')
            rc, _ = self.run_cli(['emit', 'fig', '--ports', str(ports), '--out', out])
            self.assertIn('share/fig', str(rc))
            self.assertIn('fig_DATA', str(rc))
            rc, text = self.run_cli(['check', 'fig', '--ports', str(ports)])
            self.assertEqual(rc, 1)
            self.assertIn('share/fig', text)

    def test_the_drawer_in_data_is_covered(self):
        with tempfile.TemporaryDirectory() as d, tempfile.TemporaryDirectory() as out:
            ports = self.mock(d, 'fig_DATA := share/fig')
            rc, _ = self.run_cli(['emit', 'fig', '--ports', str(ports), '--out', out])
            self.assertEqual(rc, 0)
            self.assertIn('fig  share/fig', (pathlib.Path(out) / 'dist/slim.rows').read_text())


@unittest.skipUnless((PORTS / 'Makefile').exists(), 'upterm-ports not found at %s (set UPTERM_PORTS)' % PORTS)
class Replace(unittest.TestCase):
    """D4: --write refuses an id with rows; --replace rewrites that id's rows in place, every other row byte-identical."""

    def test_replace_rewrites_only_this_ids_rows(self):
        with tempfile.TemporaryDirectory() as d:
            for n in ('dist', 'Makefile'):
                (shutil.copytree if (ROOT / n).is_dir() else shutil.copy)(ROOT / n, pathlib.Path(d) / n)
            before = {n: (pathlib.Path(d) / n).read_text().splitlines(keepends=True) for n in ('dist/packages.txt', 'dist/slim.txt')}
            real = pkg_rows.read_field
            pkg_rows.read_field = lambda p, k, f: 'bin/BitchX2' if f == 'BINS' and k == 'bitchx' else real(p, k, f)
            argv = ['emit', 'bitchx', '--ports', str(PORTS), '--root', d, '--write']
            try:
                with self.assertRaises(SystemExit) as cm:
                    pkg_rows.main(argv)
                self.assertIn('--replace', str(cm.exception))
                self.assertEqual({n: (pathlib.Path(d) / n).read_text().splitlines(keepends=True) for n in before}, before)
                with contextlib.redirect_stdout(io.StringIO()):
                    self.assertEqual(pkg_rows.main(argv + ['--replace']), 0)
            finally:
                pkg_rows.read_field = real
            for n, old in before.items():
                new = (pathlib.Path(d) / n).read_text().splitlines(keepends=True)
                keep = lambda ls: [l for l in ls if l.split()[:1] != ['bitchx']]
                self.assertEqual(keep(new), keep(old), n)
                self.assertEqual(len(new), len(old), n)
            self.assertIn('bitchx  bin/BitchX2', ''.join((pathlib.Path(d) / 'dist/slim.txt').read_text()))
            self.assertNotIn('bitchx  bin/BitchX\n', (pathlib.Path(d) / 'dist/slim.txt').read_text())


class MakeOnly(unittest.TestCase):
    """D3: make test ONLY=pkg_rows builds no C binary (a worktree without the vendor NDK cannot build render_host);
    a full make test still builds all of them."""

    def dry(self, *args):
        return subprocess.run(['make', '-n', 'test'] + list(args), cwd=ROOT, capture_output=True, text=True).stdout

    def test_pkg_rows_row_needs_no_c_prerequisite(self):
        only = self.dry('ONLY=pkg_rows')
        self.assertIn('tests/test_pkg_rows.py', only)
        compiles = lambda out: [l for l in out.splitlines() if ' -o ' in l]
        self.assertEqual(compiles(only), [])


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


@unittest.skipUnless((PORTS / 'Makefile').exists(), 'upterm-ports not found at %s (set UPTERM_PORTS)' % PORTS)
class EveryAddonHasASummary(unittest.TestCase):
    """make dist-ipk writes a Description for every add-on row in dist/packages.txt. Since the summaries moved to the
    recipes (f54aec0a), a recipe with no _SUMMARY stops the whole feed build (2026-10-26: mutt and 15 more had none)."""

    def test_every_addon_resolves_a_summary(self):
        old = os.environ.get('UPTERM_PORTS')
        os.environ['UPTERM_PORTS'] = str(PORTS)
        missing = []
        try:
            for r in pkgs.addons():
                try:
                    if not pkgs.summary(r['id']).strip():
                        missing.append(r['id'])
                except SystemExit:
                    missing.append(r['id'])
        finally:
            if old is None:
                del os.environ['UPTERM_PORTS']
            else:
                os.environ['UPTERM_PORTS'] = old
        self.assertEqual(missing, [])


if __name__ == '__main__':
    unittest.main()
