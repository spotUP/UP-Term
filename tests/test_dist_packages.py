#!/usr/bin/env python3
"""dist/packages.txt, the one table of the kit's archives (tools/pkgs.py), on the host."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkmanifest as mm
import mkreadme as mr
import pkgs

ROWS = pkgs.rows()
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
            want = ('UP-Term:pkg/%s' if r['needs-base'] != '-' else 'ENVARC:uppkg/%s') % r['archive']
            self.assertEqual(r['record'], want, r['id'])
        self.assertEqual(ROWS[0]['record'], 'UP-Term:')

    def test_ixemul_is_80_or_empty(self):
        for r in ROWS:
            self.assertIn(r['ixemul'], ('-', '80'), r['id'])

    def test_handed_paths_are_kit_paths_of_parts_txt(self):
        kit_paths = {p for _, p, _ in PARTS}
        seen = set()
        for r in ROWS:
            if r['handed'] == '-':
                continue
            for p in r['handed'].split(','):
                self.assertIn(p, kit_paths, '%s: %s' % (r['id'], p))
                self.assertNotIn(p, seen, 'handed twice: ' + p)
                seen.add(p)

    def test_the_cli_answers_what_the_makefile_asks(self):
        self.assertEqual(pkgs.get('nvim', 'archive'), 'UP-Term-nvim')
        self.assertEqual(pkgs.get('claude', 'handed'), 'Files/Claude,Files/ClaudeCode')
        with self.assertRaises(SystemExit):
            pkgs.get('nope', 'archive')
        with self.assertRaises(SystemExit):
            pkgs.get('base', 'nope')


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
            self.assertIn('comm/term/UP-Term.lha >= ' + r['needs-base'], req, r['id'])
        bumped = [dict(r, **{'needs-base': '1.3'}) if r['id'] == 'pi' else dict(r) for r in ROWS]
        self.assertIn('UP-Term.lha >= 1.3', mr.fields(mr.build('pi', table=bumped)[1])['Requires'])

    def test_the_directories_are_the_owners_choice(self):
        want = {'base': 'comm/term', 'src': 'comm/term', 'claude': 'comm/misc', 'pi': 'comm/misc',
                'nvim': 'text/edit', 'python': 'dev/lang'}
        for i, d in want.items():
            self.assertEqual(mr.fields(mr.build(i)[1])['Type'], d, i)

    def test_licences_are_mit_for_pi_base_and_claude(self):
        # owner 2026-10-13: amiga-pi and UP-Term's own code are MIT
        for i in ('pi', 'base', 'claude'):
            self.assertIn('Licence: MIT', mr.build(i)[1], i)
            self.assertNotIn('Licence: PLACEHOLDER', mr.build(i)[1], i)

    def test_release_refuses_a_readme_with_a_placeholder(self):
        import subprocess
        for i, rc in (('base', 1), ('pi', 1)):    # the uploader address is a placeholder in every readme
            r = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/mkreadme.py'), i, '--release'],
                               capture_output=True, text=True)
            self.assertEqual(r.returncode, rc, i)
            self.assertIn('PLACEHOLDER', r.stderr)
        r = subprocess.run([sys.executable, '-I', str(ROOT / 'tools/mkreadme.py'), 'pi'], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0)

    def test_every_value_is_filled(self):
        for i in self.IDS:
            self.assertNotRegex(mr.build(i)[1], r'@[A-Z]+@')


if __name__ == '__main__':
    unittest.main()
