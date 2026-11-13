#!/usr/bin/env python3
"""The kit carries every Patina theme the repo defines: install.dos copies the kit's Files/patina-themes to
UP-Term:Patina/Themes, so the dist-base recipe must fill that drawer from dist/patina-themes (it once did not,
and Style > Patina theme listed only the built-in tokyo-night)."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


class PatinaThemes(unittest.TestCase):
    def test_the_repo_defines_more_than_one_theme(self):
        self.assertGreater(len(list((ROOT / 'dist' / 'patina-themes').glob('*.theme'))), 1)

    def test_dist_base_fills_the_drawer_install_dos_copies_from(self):
        dos = (ROOT / 'dist' / 'install.dos').read_text(encoding='latin-1')
        m = re.search(r'^Copy (\S+) UP-Term:Patina/Themes ', dos, re.M)
        self.assertTrue(m, 'install.dos no longer copies the Patina themes')
        mk = (ROOT / 'Makefile').read_text(encoding='latin-1')
        recipe = mk[mk.index('\ndist-base:'):]
        recipe = recipe[:recipe.index('\n\n')]
        self.assertRegex(recipe, r'cp -R dist/patina-themes \$\(KIT\)/Files/%s\b' % re.escape(m.group(1)))

    def test_parts_txt_claims_the_themes_drawer_so_dist_does_not_stop(self):
        # kitsize and mkmanifest stop `make dist` on a kit file no line of dist/parts.txt claims
        claimed = {l.split()[1] for l in (ROOT / 'dist' / 'parts.txt').read_text(encoding='latin-1').splitlines()
                   if l.strip() and not l.startswith('#') and len(l.split()) > 1}
        self.assertIn('Files/patina-themes', claimed)


if __name__ == '__main__':
    unittest.main()
