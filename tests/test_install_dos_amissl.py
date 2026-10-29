#!/usr/bin/env python3
"""INSTALL-DOS-NO-AMISSL: the Shell install (dist/install.dos, no STEP) installs AmiSSL like the kit's
Install does (P_AMISSL in dist/Install.installer): version check, then Install-AmiSSL through Installer
from the kit's AmiSSL drawer, with the same arguments; a failure is said and the base install goes on."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi

DOS = mi.build(ROOT / 'dist/install.dos', 'base')
INSTALLER = (ROOT / 'dist/Install.installer').read_text(encoding='latin-1')
ARGS = 'Installer SCRIPT Install-AmiSSL APPNAME AmiSSL MINUSER AVERAGE DEFUSER AVERAGE NOLOG'


def libs_part():
    return DOS[DOS.index('Lab libs'):DOS.index('Lab mount')]


class InstallDosAmiSSL(unittest.TestCase):
    def test_same_invocation_as_p_amissl(self):
        self.assertIn('"Installer SCRIPT Install-AmiSSL APPNAME AmiSSL "', INSTALLER)
        self.assertIn('"MINUSER AVERAGE DEFUSER AVERAGE NOLOG', INSTALLER)
        self.assertIn(ARGS, libs_part())

    def test_version_check_then_run_from_the_kit_drawer_then_recheck(self):
        p = libs_part()
        chk = 'Version >NIL: LIBS:amisslmaster.library 5'
        self.assertEqual(p.count(chk), 2)
        self.assertLess(p.index(chk), p.index('CD "' ))
        i = p.index(ARGS)
        self.assertLess(p.index(chk), p.index('/AmiSSL"'))
        self.assertLess(p.index('/AmiSSL"'), i)
        self.assertLess(i, p.rindex(chk))

    def test_only_without_step_and_failure_is_reported_not_fatal(self):
        p = libs_part()
        self.assertRegex(p, r'If "\{?STEP\}?" EQ ""|If "" EQ ""')
        self.assertIn('[ERROR] AmiSSL was not installed', p)
        self.assertIn('Install-AmiSSL', p[p.index('[ERROR] AmiSSL was not installed'):][:300])
        self.assertNotIn('Quit 20', p[p.index(ARGS):])


if __name__ == '__main__':
    unittest.main()
