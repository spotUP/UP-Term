#!/usr/bin/env python3
"""s75: the themes renamed to fit 30 characters. The one alias table is pc_theme_alias in prefs/prefs_core.c
(prefs_theme_file and prefs_theme_index apply it, so /theme, Prefs, Amarchy and the live theme list all map old names).
Every new name is a themes/*.conf, no old name still is, and every theme file name is 30 characters or shorter."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = (ROOT / 'prefs/prefs_core.c').read_text()
TABLE = re.findall(r'\{ "([^"]+)", "([^"]+)" \},', SRC[SRC.index('pc_theme_alias'):])
THEMES = {p.stem for p in (ROOT / 'themes').glob('*.conf')}


class ThemeAliases(unittest.TestCase):
    def test_table_matches_the_theme_drawer(self):
        self.assertEqual(len(TABLE), 7)
        for old, new in TABLE:
            self.assertGreater(len(old + '.conf'), 30, old)
            self.assertLessEqual(len(new + '.conf'), 30, new)
            self.assertIn(new, THEMES)
            self.assertNotIn(old, THEMES)
            self.assertTrue((ROOT / 'themes' / (new + '.yaml')).is_file(), new)

    def test_no_theme_file_over_30(self):
        self.assertEqual([n for n in THEMES if len(n + '.conf') > 30], [])


if __name__ == '__main__':
    unittest.main()
