#!/usr/bin/env python3
"""dist/packages.txt, the one table of the kit's archives (tools/pkgs.py), on the host."""
import hashlib, pathlib, re, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkmanifest as mm
import mkreadme as mr
import pkgs

ROWS = pkgs.lha_rows()     # the archives; git, perl, vim and bitchx are upkg-only (tests/test_dist_slim.py)
PARTS = mm.read_parts((ROOT / 'dist/parts.txt').read_text())


def ver(v):
    return tuple(int(x) for x in v.split('.'))


class Packages(unittest.TestCase):
    def test_the_table_is_not_empty_and_the_first_row_is_the_base(self):
        self.assertEqual(ROWS[0]['id'], 'base')
        self.assertEqual(ROWS[0]['needs-base'], '-')

    def test_every_row_has_every_column(self):
        for r in ROWS:
            self.assertEqual(sorted(r), sorted(pkgs.COLUMNS))
            for c, v in r.items():
                self.assertNotEqual(v, '', '%s %s' % (r['id'], c))

    def test_a_row_with_a_missing_column_is_refused(self):
        with self.assertRaises(SystemExit):
            pkgs.parse('base  UP-Term  1.0  -  -  UP-Term:\n')

    def test_ids_and_archives_are_unique(self):
        for c in ('id', 'archive'):
            vals = [r[c] for r in ROWS]
            self.assertEqual(len(vals), len(set(vals)), c)

    def test_versions_are_major_dot_minor(self):
        for r in ROWS:
            self.assertRegex(r['version'], r'^\d+\.\d+$', r['id'])
            if r['needs-base'] != '-':
                self.assertRegex(r['needs-base'], r'^\d+\.\d+$', r['id'])

    def test_no_add_on_needs_a_base_newer_than_the_base(self):
        base = ver(ROWS[0]['version'])
        for r in ROWS[1:]:
            if r['needs-base'] != '-':
                self.assertLessEqual(ver(r['needs-base']), base, r['id'])

    def test_the_record_drawer_follows_needs_base(self):
        for r in ROWS[1:]:
            want = ('UP-Term:pkg/%s' if r['needs-base'] != '-' else 'ENVARC:upkg/%s') % r['archive']
            self.assertEqual(r['record'], want, r['id'])
        self.assertEqual(ROWS[0]['record'], 'UP-Term:')

    def test_ixemul_is_a_version_or_empty(self):
        for r in ROWS:
            self.assertRegex(r['ixemul'], r'^(-|\d+)$', r['id'])
            if r['needs-base'] != '-':
                self.assertIn(r['ixemul'], ('-', '80'), r['id'] + ': the base brings the patched 80')

    def test_a_stand_alone_archive_has_no_prefix_and_none_of_the_bases(self):
        """K2-1 and the owner 2026-10-13: Claude, amiga-pi and Python3 install on a Workbench with no
        UP-Term, under their own names; only what needs the base (Neovim) is named UP-Term-*."""
        for r in ROWS[1:]:
            if r['needs-base'] == '-':
                self.assertFalse(r['archive'].lower().startswith('up-term'), r['id'])
            else:
                self.assertTrue(r['archive'].startswith('up-'), r['id'])
        self.assertEqual({r['id'] for r in ROWS[1:] if r['needs-base'] == '-'}, {'claude', 'dopus', 'pi', 'python'})
        self.assertEqual([r['archive'] for r in ROWS], ['up-term', 'up-claude', 'up-pi', 'up-nvim', 'up-python', 'up-dopus'])

    def test_a_stand_alone_archive_with_an_ixemul_version_carries_the_library(self):
        for r in ROWS[1:]:
            if r['needs-base'] == '-' and r['ixemul'] != '-':
                dos = (ROOT / 'dist/addons' / r['id'] / 'install.dos').read_text(encoding='latin-1')
                self.assertIn('Lab ixemul', dos, r['id'])
                self.assertIn('LIBS:ixemul.library %s' % r['ixemul'], dos, r['id'])

    def test_handed_paths_are_kit_paths_the_base_no_longer_lists(self):
        """K2-2 had them in parts.txt (the single kit); the split moved them to the add-on archives"""
        kit_paths = {p for _, p, _ in PARTS}
        seen = set()
        for r in ROWS:
            if r['handed'] == '-':
                continue
            for p in r['handed'].split(','):
                self.assertTrue(p.startswith('Files/'), '%s: %s' % (r['id'], p))
                self.assertNotIn(p, kit_paths, '%s: %s is still in the base parts.txt' % (r['id'], p))
                self.assertNotIn(p, seen, 'handed twice: ' + p)
                seen.add(p)

    def test_the_cli_answers_what_the_makefile_asks(self):
        self.assertEqual(pkgs.get('nvim', 'archive'), 'up-nvim')
        self.assertEqual(pkgs.get('python', 'archive'), 'up-python')
        self.assertEqual(pkgs.get('claude', 'handed'), 'Files/Claude')
        with self.assertRaises(SystemExit):
            pkgs.get('nope', 'archive')
        with self.assertRaises(SystemExit):
            pkgs.get('base', 'nope')


class Renames(unittest.TestCase):
    """dist/package-renames.txt (old new) and packages.txt are the same story: every new name is a package
    of the table, no old name is any row's archive or record, and the base never appears."""
    def test_every_new_name_is_a_package_and_no_old_name_is(self):
        archives = {r['archive'] for r in pkgs.rows()}
        pairs = pkgs.renames()
        self.assertEqual(len(pairs), 7)
        for old, new in pairs:
            self.assertIn(new, archives)
            self.assertNotIn(old, archives)
            self.assertNotIn(old, {r['record'].rstrip('/').split('/')[-1] for r in pkgs.rows()})
            self.assertNotEqual(old, new)
        self.assertEqual(len({o for o, _ in pairs}), len(pairs), 'an old name twice')
        self.assertEqual(len({n for _, n in pairs}), len(pairs), 'a new name twice')
        self.assertNotIn('UP-Term', [n for _, n in pairs] + [o for o, _ in pairs])

    def test_every_add_on_of_the_table_was_renamed_and_its_record_ends_in_its_new_name(self):
        renamed = {n for _, n in pkgs.renames()}
        for r in pkgs.rows():
            if r['id'] == 'base':
                self.assertEqual(pkgs.old_record(r), [])
                continue
            self.assertTrue(r['record'].rstrip('/').endswith('/' + r['archive']), r['id'])
            if r['archive'] in pkgs.NEW_PACKAGES:      # born as up-<x>: no old record to migrate
                self.assertNotIn(r['archive'], renamed, r['id'])
                self.assertEqual(pkgs.old_record(r), [], r['id'])
                continue
            self.assertIn(r['archive'], renamed, r['id'])
            (old, new), = pkgs.old_record(r)
            self.assertEqual(new, r['record'].rstrip('/'))
            self.assertNotEqual(old, new)

    def test_a_malformed_line_is_refused(self):
        with tempfile.TemporaryDirectory() as d:
            f = pathlib.Path(d) / 'r.txt'
            f.write_text('only-one-word\n')
            with self.assertRaises(SystemExit):
                pkgs.renames(f)


class Readme(unittest.TestCase):
    """K2-9: the Aminet readme of every archive (dist/aminet/<id>.readme.in, tools/mkreadme.py)."""
    IDS = [r['id'] for r in ROWS] + ['src']

    def test_every_archive_has_a_readme_with_every_field_in_order(self):
        for i in self.IDS:
            _, text = mr.build(i)
            self.assertEqual(tuple(mr.fields(text)), mr.FIELDS, i)
            self.assertTrue(all(mr.fields(text)[f] for f in mr.FIELDS), i)

    def test_short_is_at_most_40_characters(self):
        for i in self.IDS:
            short = mr.fields(mr.build(i)[1])['Short']
            self.assertLessEqual(len(short), 40, '%s: %r' % (i, short))

    def test_version_is_the_one_of_packages_txt(self):
        for r in ROWS:
            self.assertEqual(mr.fields(mr.build(r['id'])[1])['Version'], r['version'])
        bumped = [dict(r, version='2.7') if r['id'] == 'nvim' else dict(r) for r in ROWS]
        self.assertEqual(mr.fields(mr.build('nvim', table=bumped)[1])['Version'], '2.7')
        self.assertEqual(mr.fields(mr.build('src', table=bumped)[1])['Version'], ROWS[0]['version'])

    def test_an_add_on_requires_the_base_at_the_version_packages_txt_names(self):
        for r in ROWS[1:]:
            req = mr.fields(mr.build(r['id'])[1])['Requires']
            if r['needs-base'] == '-':
                self.assertNotIn('up-term.lha', req, r['id'] + ': stand-alone')
            else:
                self.assertIn('comm/term/up-term.lha >= ' + r['needs-base'], req, r['id'])
        bumped = [dict(r, **{'needs-base': '1.3'}) if r['id'] == 'nvim' else dict(r) for r in ROWS]
        self.assertIn('up-term.lha >= 1.3', mr.fields(mr.build('nvim', table=bumped)[1])['Requires'])

    def test_the_directories_are_the_owners_choice(self):
        want = {'base': 'comm/term', 'src': 'comm/term', 'claude': 'comm/misc', 'pi': 'comm/misc',
                'nvim': 'text/edit', 'python': 'dev/lang', 'git': 'dev/misc', 'perl': 'dev/lang', 'vim': 'text/edit', 'bitchx': 'comm/irc', 'vbcc': 'dev/c'}
        for i, d in want.items():
            self.assertEqual(mr.fields(mr.build(i)[1])['Type'], d, i)

    def test_licences_are_mit_for_pi_base_and_claude(self):
        # owner 2026-10-13: amiga-pi and UP-Term's own code are MIT
        for i in ('pi', 'base', 'claude'):
            self.assertIn('Licence: MIT', mr.build(i)[1], i)
            self.assertNotIn('Licence: PLACEHOLDER', mr.build(i)[1], i)

    def test_release_refuses_a_readme_with_a_placeholder(self):
        import subprocess, tempfile, shutil
        with tempfile.TemporaryDirectory() as d:
            shutil.copytree(ROOT / 'tools', pathlib.Path(d) / 'tools')
            shutil.copytree(ROOT / 'dist', pathlib.Path(d) / 'dist')
            f = pathlib.Path(d) / 'dist/aminet/pi.readme.in'
            f.write_text(f.read_text().replace('spotup@gmail.com', 'UPLOADER-ADDRESS-PLACEHOLDER'))
            run = lambda *a: subprocess.run([sys.executable, '-I', str(pathlib.Path(d) / 'tools/mkreadme.py'), 'pi', *a],
                                            capture_output=True, text=True)
            r = run('--release')
            self.assertEqual(r.returncode, 1)
            self.assertIn('PLACEHOLDER', r.stderr)
            self.assertEqual(run().returncode, 0)

    def test_no_readme_has_a_replaces_line_or_a_placeholder(self):
        for i in self.IDS:
            self.assertNotIn('Replaces', mr.fields(mr.build(i)[1]), i)
            self.assertNotIn('PLACEHOLDER', mr.build(i)[1], i)

    def test_every_value_is_filled(self):
        for i in self.IDS:
            self.assertNotRegex(mr.build(i)[1], r'@[A-Z]+@')


class BaseArchiveName(unittest.TestCase):
    """Owner 2026-10-20: every published file is up-*, the base too (up-term.lha, up-term-src.lha). The product, the
    UP-Term: assign and its record stay UP-Term."""

    def test_the_base_archive_is_up_term_and_its_record_is_still_the_assign(self):
        base = ROWS[0]
        self.assertEqual((base['archive'], base['record']), ('up-term', 'UP-Term:'))

    def test_no_file_of_the_repository_names_the_old_archives(self):
        old = re.compile(r'UP-Term(-src)?\.lha')
        bad = []
        for sub in ('Makefile', 'RULES.md', 'tools', 'dist', 'tests'):
            top = ROOT / sub
            for f in ([top] if top.is_file() else sorted(top.rglob('*'))):
                if not f.is_file() or f.suffix in ('.pyc', '.lha', '.png', '.info') or f.name == pathlib.Path(__file__).name:
                    continue
                try:
                    text = f.read_text(encoding='latin-1')
                except OSError:
                    continue
                if old.search(text):
                    bad.append(str(f.relative_to(ROOT)))
        self.assertEqual(bad, [])

    def test_the_source_archive_is_up_term_src(self):
        self.assertEqual(mr.row('src')['archive'], 'up-term-src')
        self.assertEqual(pkgs.SOURCE_ARCHIVE, 'up-term-src')
        self.assertIn('build/up-term-src.lha', (ROOT / 'Makefile').read_text())


class Prune(unittest.TestCase):
    """make dist deletes every build/*.lha that dist/packages.txt does not list (the leftovers of renamed packages)"""

    def test_stale_archives_are_deleted_and_listed_ones_and_the_source_archive_stay(self):
        keep = [r['archive'] + '.lha' for r in ROWS] + ['up-term-src.lha']
        stale = ['Claude.lha', 'Python3.lha', 'amiga-pi.lha', 'UP-Term-nvim.lha', 'vtcon.lha']
        with tempfile.TemporaryDirectory() as d:
            for n in keep + stale + ['upkg-index.txt']:
                (pathlib.Path(d) / n).write_bytes(b'x')
            gone = pkgs.prune(d)
            left = sorted(p.name for p in pathlib.Path(d).iterdir())
        self.assertEqual(sorted(gone), sorted(stale))
        self.assertEqual(left, sorted(keep + ['upkg-index.txt']))

    def test_the_old_spelling_of_the_base_archives_is_stale(self):
        """On a case-insensitive disk up-term.lha and UP-Term.lha are one file, so the old spelling is tried alone"""
        with tempfile.TemporaryDirectory() as d:
            for n in ('UP-Term.lha', 'UP-Term-src.lha'):
                (pathlib.Path(d) / n).write_bytes(b'x')
            self.assertEqual(pkgs.prune(d), ['UP-Term-src.lha', 'UP-Term.lha'])
            self.assertEqual(list(pathlib.Path(d).iterdir()), [])

    def test_one_make_dist_leaves_an_index_and_feed_that_dist_check_accepts(self):
        """2026-10-22: `make dist` built no feed, and a later `make dist-ipk` rebuilt the archives
        without the index, so dist-check failed on Packages.gz and then on upkg-index.txt"""
        mk = (ROOT / 'Makefile').read_text()

        def prereqs(target):
            m = re.search(r'(?m)^%s:(.*)$' % re.escape(target), mk)
            self.assertIsNotNone(m, target)
            return m.group(1).split()
        self.assertIn('dist-ipk', prereqs('dist'))
        self.assertIn('dist-index', prereqs('dist-ipk'))

    def test_make_dist_index_runs_it_and_prints_each_name(self):
        mk = (ROOT / 'Makefile').read_text()
        self.assertRegex(mk, r'(?m)^dist-index:.*\n\tpython3 tools/pkgs\.py prune \$\(BUILD\)\n')
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / 'vtcon.lha').write_bytes(b'x')
            out = subprocess.run([sys.executable, str(ROOT / 'tools/pkgs.py'), 'prune', d], capture_output=True, text=True, check=True).stdout
        self.assertIn('vtcon.lha', out)


class Index(unittest.TestCase):
    """upkg's index (tools/pkgs.py index): what upkg on the Amiga downloads and checks"""

    def test_a_line_per_archive_with_its_version_record_size_and_sha256(self):
        with tempfile.TemporaryDirectory() as d:
            for i, r in enumerate(ROWS):
                (pathlib.Path(d) / (r['archive'] + '.lha')).write_bytes(b'x' * (i + 1))
            lines = pkgs.index(d).splitlines()
        self.assertTrue(lines[0].startswith('# upkg index 1'))
        self.assertEqual(len(lines), len(ROWS) + 1)
        for i, (r, line) in enumerate(zip(ROWS, lines[1:])):
            data = b'x' * (i + 1)
            self.assertEqual(line.split(' '), [r['id'], r['archive'], r['version'], r['needs-base'],
                                               r['record'], str(len(data)),
                                               hashlib.sha256(data).hexdigest()])

    def test_a_missing_archive_is_refused(self):
        with tempfile.TemporaryDirectory() as d, self.assertRaises(SystemExit):
            pkgs.index(d)

    def test_no_field_has_a_space(self):
        for r in ROWS:
            for c in ('id', 'archive', 'version', 'needs-base', 'record'):
                self.assertNotIn(' ', r[c], (r['id'], c))


if __name__ == '__main__':
    unittest.main()
