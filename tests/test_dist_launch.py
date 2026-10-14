#!/usr/bin/env python3
"""The built add-on archives hold the quick-launch icons (W-I1): build/dist/<archive>/Files/launch/<name> and
<name>.info, listed in the archive's lha and in its MANIFEST under the launch part, the .info an IconX project
icon (default tool C:IconX, WINDOW= an XCON: window with a close gadget). Run by `make dist-check` after
`make dist`; without the staged archives it fails (K2 dist-check rule: a missing archive is an error)."""
import pathlib, subprocess, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkgs

LAUNCHERS = {'pi': ['amiga-pi'], 'nvim': ['Neovim'], 'python': ['Python 3'], 'claude': ['Claude']}
ARCHIVE = {r['id']: r['archive'] for r in pkgs.rows()}


class BuiltArchives(unittest.TestCase):
    def test_each_archive_holds_the_launcher_and_its_icon(self):
        for pid, names in LAUNCHERS.items():
            a = ARCHIVE[pid]
            kit = ROOT / 'build/dist' / a / 'Files/launch'
            lst = subprocess.run(['lha', 'l', str(ROOT / 'build' / (a + '.lha'))], capture_output=True, text=True, check=True).stdout
            man = (ROOT / 'build/dist' / a / 'Files/MANIFEST').read_text(encoding='latin-1')
            for n in names:
                for f in (n, n + '.info'):
                    self.assertTrue((kit / f).is_file() and (kit / f).stat().st_size > 0, '%s lacks launch/%s' % (a, f))
                    self.assertIn(' %s/Files/launch/%s\n' % (a, f), lst + '\n', '%s.lha lacks launch/%s' % (a, f))
                    self.assertRegex(man, r'(?m)^launch \d+ [0-9a-f]{8} Files/launch/%s$' % __import__('re').escape(f))
                info = (kit / (n + '.info')).read_bytes()
                self.assertEqual(info[:2], b'\xe3\x10', 'not a Workbench icon')
                self.assertIn(b'C:IconX', info)
                self.assertIn(b'WINDOW=XCON:0/12/', info)
                self.assertIn(b'/CLOSE', info)
                self.assertNotIn(b'DELAY=', info, '%s: DELAY=0 keeps the window open for ever (rig 3)' % n)
                self.assertEqual(info.count(b'WINDOW='), 1, '%s: more than one WINDOW tooltype' % n)

    def test_the_install_script_in_the_archive_copies_what_the_archive_holds(self):
        for pid, names in LAUNCHERS.items():
            dos = (ROOT / 'build/dist' / ARCHIVE[pid] / 'Files/install.dos').read_text(encoding='latin-1')
            for n in names:
                self.assertIn('"launch/%s" "launch/%s.info"' % (n, n), dos)
            un = (ROOT / 'build/dist' / ARCHIVE[pid] / 'Uninstall').read_text(encoding='latin-1')
            for n in names:
                self.assertIn('UP-Term:%s.info' % n, un.replace('"', ''))


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
