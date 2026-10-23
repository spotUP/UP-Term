#!/usr/bin/env python3
"""Amarchy's documentation and release wiring (plan 2026-10-22-amarchy-commodity.md, Phase F).

Run by `make dist-check` (after `make dist`: the built base kit is read) and by `make test ONLY=packages`
style runs from a checkout. A missing piece fails: the README's key list is the generated one, the lesson,
the man page, the kit rows (Amarchy, amarchy-menu, keymap in dist/parts.txt and the base MANIFEST), the
S:User-Startup block and the RULES.md rows."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import keymap_doc

KIT = ROOT / 'build/dist/up-term'


def read(p):
    return (ROOT / p).read_text(encoding='latin-1')


class AmarchyDocs(unittest.TestCase):
    def test_uninstall_quits_a_running_amarchy_before_deleting_the_drawer(self):
        un = read('dist/Uninstall')
        quit_at = un.index('Status >ENV:upw COM UP-Term:bin/Amarchy')
        self.assertIn('Break $upw C', un[quit_at:quit_at + 200])
        self.assertLess(quit_at, un.index('Delete >NIL: "$updest" ALL QUIET'))
        self.assertIn('Run >NIL: UP-Term:bin/Amarchy', read('dist/install.dos'))  # the startup block unstartup.sh strips

    def test_readme_key_list_is_the_generated_one(self):
        m = re.search(r'\n  \[keys\]\n(.*?)  \[end keys\]\n', read('dist/README.txt'), re.S)
        self.assertIsNotNone(m, 'README.txt has no AMARCHY key list')
        self.assertEqual(m.group(1), keymap_doc.text(), 'README key list is stale: python3 tools/keymap_doc.py')

    def test_readme_names_the_arexx_commands(self):
        r = read('dist/README.txt')
        for w in ('AMARCHY', 'STATUS', 'RELOAD', 'RUN n', 'SHOW', 'QUIT', 'Exchange', 'keymap.user'):
            self.assertIn(w, r)

    def test_lesson_and_man_page_exist(self):
        self.assertIn('Amarchy', read('dist/tutorial-lessons/14-amarchy-keys.lesson'))
        self.assertLessEqual(len('14-amarchy-keys.lesson'), 30)
        self.assertIn('.Nm Amarchy', read('man/amarchy.1'))

    def test_install_rows_and_startup_block(self):
        parts = read('dist/parts.txt')
        for f in ('Amarchy', 'amarchy-menu', 'amarchy-gui', 'keymap'):
            self.assertRegex(parts, r'(?m)^tools\s+Files/%s$' % f)
        d = read('dist/install.dos')
        self.assertIn('Copy Amarchy amarchy-menu amarchy-gui amarchy-index UP-Term:bin CLONE', d)
        self.assertIn('Run >NIL: UP-Term:bin/Amarchy', d)
        self.assertIn(';BEGIN UP-Term Amarchy', d)
        self.assertIn(';BEGIN UP-Term Amarchy index', d)  # its own block: an Update adds it once
        self.assertEqual(d.count('Run >NIL: UP-Term:bin/amarchy-index'), 1)

    def test_rules_commands_rows(self):
        r = read('RULES.md')
        for w in ('ONLY=keymap', 'amarchy_rig.py', 'keymap_rig.py', 'amarchy_menu_rig.py'):
            self.assertIn(w, r)

    @unittest.skipUnless((KIT / 'Files/MANIFEST').is_file(), 'built kit missing: make dist-base')
    def test_built_base_kit_holds_amarchy(self):
        man = (KIT / 'Files/MANIFEST').read_text(encoding='latin-1')
        for f in ('Amarchy', 'amarchy-menu', 'amarchy-gui', 'keymap'):
            self.assertTrue((KIT / 'Files' / f).is_file(), 'kit lacks Files/' + f)
            self.assertRegex(man, r'(?m)^tools \d+ [0-9a-f]{8} Files/%s$' % f)
        self.assertTrue((KIT / 'Files/userland/share/man/man1/amarchy.1').is_file())
        self.assertTrue((KIT / 'Files/userland/share/tutorial/14-amarchy-keys.lesson').is_file())


if __name__ == '__main__':
    unittest.main()
