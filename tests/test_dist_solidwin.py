#!/usr/bin/env python3
"""DIST-SOLIDWIN: the kit carries Thomas Rapp's SolidWin 1.12 and the Install offers it.
  - dist/solidwin holds the unchanged binary (md5 of the author's archive), its icon and SOURCE.txt;
    dist/parts.txt, the Makefile's dist-base and LICENSES.txt know it;
  - Install.installer asks "Solid window moving (SolidWin)", ticked by default (every OS version);
  - install.dos copies C:SolidWin and writes the marked User-Startup block (as wasabid);
  - Uninstall deletes C:SolidWin by its marker; the block goes with unstartup.sh's UP-Term blocks."""
import hashlib, pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi

DIR = ROOT / 'dist/solidwin'
INSTALLER = (ROOT / 'dist/Install.installer').read_text(encoding='latin-1')
DOS = mi.build(ROOT / 'dist/install.dos', 'base')
UNINSTALL = (ROOT / 'dist/Uninstall').read_text(encoding='latin-1')
MD5 = '72960c492bce02d91f557f3e5ef58887'


def option():
    i = INSTALLER.index('(set #solidwin\n')
    return INSTALLER[i:INSTALLER.index('(if (in #solidwin 0)', i)]


class SolidWin(unittest.TestCase):
    def test_binary_unchanged_and_listed_in_the_kit_sources(self):
        data = (DIR / 'SolidWin').read_bytes()
        self.assertEqual(hashlib.md5(data).hexdigest(), MD5)
        self.assertIn(b'$VER: SolidWin 1.12 (25.10.2025)', data)
        self.assertTrue((DIR / 'SolidWin.info').is_file())
        src = (DIR / 'SOURCE.txt').read_text()
        for needle in (MD5, 'solidwin.lha', '1.12'):
            self.assertIn(needle, src)
        self.assertRegex((ROOT / 'dist/parts.txt').read_text(), r'(?m)^boot\s+Files/solidwin$')
        self.assertIn('cp -R dist/solidwin $(KIT)/Files/solidwin', (ROOT / 'Makefile').read_text())
        self.assertIn('SolidWin 1.12', (ROOT / 'dist/LICENSES.txt').read_text(encoding='latin-1'))

    def test_option_default_ticked_and_passed_to_install_dos(self):
        o = option()
        self.assertIn('(choices "Solid window moving (SolidWin)")', o)
        self.assertIn('(default 1)', o)
        self.assertNotIn('execmajor', o)  # no OS-version condition: ticked on 3.0, 3.1 and 3.2
        self.assertIn('(set #name "SOLIDWIN" #val #solidwin) (P_OPT)', INSTALLER)
        self.assertIn('(set #solidwin (getenv "UPTOPTSOLIDWIN"))', INSTALLER)
        self.assertIn('SOLIDWIN/S', DOS.splitlines()[0])
        self.assertIn('If EXISTS ENV:UPTOPTSOLIDWIN', DOS)

    def test_start_line_is_installed_once_in_a_marked_block(self):
        p = DOS[DOS.index('If "$optsolidwin" EQ "SOLIDWIN"'):]
        p = p[:p.index('MakeDir >NIL: ENVARC:vsh')]
        self.assertIn('Copy solidwin/SolidWin C:SolidWin CLONE', p)
        self.assertIn('Search >NIL: S:User-Startup "C:SolidWin"', p)
        self.assertIn('Echo >>S:User-Startup ";BEGIN UP-Term SolidWin"', p)
        self.assertIn('Echo >>S:User-Startup "Run >NIL: C:SolidWin"', p)
        self.assertIn('Echo >>S:User-Startup ";END UP-Term SolidWin"', p)

    def test_uninstall_strips_block_and_file(self):
        i = UNINSTALL.index('ENVARC:up-term/solidwin')
        self.assertIn('Delete >NIL: C:SolidWin QUIET', UNINSTALL[i:i + 120])
        # the block is named ";BEGIN UP-Term ...": unstartup.sh without a name strips it
        self.assertIn("';BEGIN UP-Term'|';BEGIN UP-Term '*) skip=1",
                      (ROOT / 'dist/unstartup.sh').read_text())


if __name__ == '__main__':
    unittest.main()
