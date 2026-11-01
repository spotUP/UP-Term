#!/usr/bin/env python3
"""DIST-ICONLIB: the kit carries Peter Keunecke's IconLib and the Install offers it.
  - dist/iconlib holds the two builds (46.4 for OS 3.0/3.1, 51.4 for 3.2), the guide and the licence page,
    and dist/parts.txt, the Makefile's dist-base and LICENSES.txt know them;
  - Install.installer asks "PeterK icon library": ticked on 3.0/3.1, unticked on 3.2 (exec 47);
  - install.dos keeps LIBS:icon.library.amarchy-backup before it copies, only when none exists,
    and picks the 51.4 build only on exec 47; Uninstall puts the backup back."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi

DIR = ROOT / 'dist/iconlib'
INSTALLER = (ROOT / 'dist/Install.installer').read_text(encoding='latin-1')
DOS = mi.build(ROOT / 'dist/install.dos', 'base')
UNINSTALL = (ROOT / 'dist/Uninstall').read_text(encoding='latin-1')


def option():
    i = INSTALLER.index('(set #iconlibos32')
    return INSTALLER[i:INSTALLER.index('; Directory Opus 5: asked only', i)]


class IconLib(unittest.TestCase):
    def test_libraries_and_licence_in_the_kit_sources(self):
        os30 = (DIR / 'icon.library.os30').read_bytes()
        os32 = (DIR / 'icon.library.os32').read_bytes()
        self.assertIn(b'$VER:icon.library 46.4.595 (68020)', os30)
        self.assertIn(b'$VER:icon.library 51.4.602 (TC020)', os32)
        self.assertIn(b'Important Restrictions', (DIR / 'IconLib.html').read_bytes())
        self.assertTrue((DIR / 'IconLib.guide').is_file() and (DIR / 'SOURCE.txt').is_file())
        self.assertRegex((ROOT / 'dist/parts.txt').read_text(), r'(?m)^libs\s+Files/iconlib$')
        self.assertIn('cp -R dist/iconlib $(KIT)/Files/iconlib', (ROOT / 'Makefile').read_text())
        self.assertIn('IconLib 46.4', (ROOT / 'dist/LICENSES.txt').read_text(encoding='latin-1'))

    def test_option_default_ticked_on_30_31_unticked_on_32(self):
        o = option()
        self.assertIn('(choices "PeterK icon library")', o)
        self.assertIn('(>= #execmajor 47)', o)
        self.assertIn('(default (if #iconlibos32 0 1))', o)
        self.assertIn('(set #name "ICONLIB" #val #iconlib) (P_OPT)', INSTALLER)

    def test_exec_major_is_set_before_the_page(self):
        req = (ROOT / 'dist/common/requirements.installer').read_text(encoding='latin-1')
        self.assertIn('(set #execmajor (/ (getversion "exec.library" (resident)) 65536))', req)

    def test_backup_precedes_copy_and_is_kept_once(self):
        p = DOS[DOS.index('If "$opticonlib" EQ "ICONLIB"'):]
        p = p[:p.index('UnSet upiconlib')]
        bak = 'Copy LIBS:icon.library LIBS:icon.library.amarchy-backup CLONE'
        cp = 'Copy $upiconlib LIBS:icon.library CLONE'
        self.assertLess(p.index('If NOT EXISTS LIBS:icon.library.amarchy-backup'), p.index(bak))
        self.assertLess(p.index(bak), p.index(cp))
        self.assertIn('iconlib/icon.library.os30', p)
        self.assertLess(p.index('Version >NIL: exec.library 47'), p.index('iconlib/icon.library.os32'))

    def test_uninstall_restores_the_backup(self):
        self.assertIn('Copy LIBS:icon.library.amarchy-backup LIBS:icon.library CLONE', UNINSTALL)


if __name__ == '__main__':
    unittest.main()
