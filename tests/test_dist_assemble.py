#!/usr/bin/env python3
"""tools/mkinstaller.py: the scripts of a package, assembled from dist/ and dist/common/.
(K2-3 proved the base's assembled Install, install.dos and Uninstall byte for byte equal
to the files before the shared parts moved out; K2-4 changed the base, so the copies went.)"""
import pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi


class Assemble(unittest.TestCase):
    def test_every_name_in_the_base_scripts_has_a_value(self):
        for name in ('Install.installer', 'install.dos', 'Uninstall'):
            self.assertNotRegex(mi.build(ROOT / 'dist' / name, 'base'), r'@[A-Z0-9]+@', name)

    def test_the_templates_do_include_the_shared_parts(self):
        for name, frag in (('Install.installer', 'procs.installer'), ('install.dos', 'record.dos'),
                           ('Uninstall', 'uninstall-var.dos')):
            self.assertIn('; @include dist/common/%s\n' % frag, (ROOT / 'dist' / name).read_text(encoding='latin-1'))

    def _tmp(self, text):
        d = pathlib.Path(tempfile.mkdtemp())
        p = d / 't'
        p.write_text(text, encoding='latin-1')
        return p

    def test_an_unknown_name_stops_the_tool(self):
        with self.assertRaises(SystemExit) as e:
            mi.assemble(self._tmp('a @NOPE@ b\n'), {})
        self.assertIn('@NOPE@', str(e.exception))

    def test_an_include_that_includes_itself_stops_the_tool(self):
        p = self._tmp('x\n')
        p.write_text('; @include %s\n' % p, encoding='latin-1')
        with self.assertRaises(SystemExit) as e:
            mi.assemble(p, {}, ())
        self.assertIn('includes itself', str(e.exception))

    def test_values_fill_names_in_the_text(self):
        self.assertEqual(mi.sub('@ID@/@ARCHIVE@', {'ID': 'nvim', 'ARCHIVE': 'UP-Term-nvim'}), 'nvim/UP-Term-nvim')

    def test_an_add_on_gets_its_record_and_options_drawers_from_the_table(self):
        v = mi.read_values('nvim')
        self.assertEqual(v['RECORD'], 'UP-Term:pkg/UP-Term-nvim')
        self.assertEqual(v['OPTS'], 'UP-Term:pkg/UP-Term-nvim/opts')
        self.assertEqual((v['BASEMAJOR'], v['BASEMINOR']), ('1', '0'))
        self.assertEqual(mi.read_values('base')['OPTS'], 'ENVARC:up-term/opts')
        self.assertEqual(mi.read_values('nvim')['RECORDPRE'], 'none.dos')

    def test_the_base_gets_the_add_ons_uninstall_blocks_and_search_lines_from_the_table(self):
        v = mi.read_values('base')
        for e in ('Claude', 'pi', 'nvim', 'python'):
            self.assertIn('If EXISTS UP-Term:pkg/UP-Term-%s/Uninstall\n  Execute UP-Term:pkg/UP-Term-%s/Uninstall\nEndIf' % (e, e),
                          v['ADDONUNINSTALL'])
        # Files/Claude finds Files/ClaudeCode by substring: one search for both
        self.assertEqual(v['HANDEDSEARCH'].count('Search >NIL: UP-Term:MANIFEST'), 4)
        self.assertNotIn('Files/ClaudeCode', v['HANDEDSEARCH'])
        self.assertEqual(mi.read_values('nvim')['ADDONUNINSTALL'], '')

    def test_an_unknown_package_stops_the_tool(self):
        with self.assertRaises(SystemExit):
            mi.read_values('nope')


if __name__ == '__main__':
    unittest.main()
