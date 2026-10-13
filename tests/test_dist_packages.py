#!/usr/bin/env python3
"""dist/packages.txt, the one table of the kit's archives (tools/pkgs.py), on the host."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkmanifest as mm
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
        self.assertEqual(pkgs.get('nvim', 'archive'), 'UP-Term-nvim')
        self.assertEqual(pkgs.get('claude', 'handed'), 'Files/Claude,Files/ClaudeCode')
        with self.assertRaises(SystemExit):
            pkgs.get('nope', 'archive')
        with self.assertRaises(SystemExit):
            pkgs.get('base', 'nope')


if __name__ == '__main__':
    unittest.main()
