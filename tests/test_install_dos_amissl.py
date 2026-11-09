"""INSTALL-DOS-NO-AMISSL / INSTALL-AMISSL-STALL: AmiSSL is installed by Files/install-amissl.dos (dist/install-amissl.dos),
which asks nothing, from both the kit's Install (P_AMISSL in dist/Install.installer) and the Shell install
(dist/install.dos, no STEP). AmiSSL's own Install-AmiSSL is an Installer script with a page at every level: nested in
a full install it stopped the run at its welcome page, so no caller may start it."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi

DOS = mi.build(ROOT / 'dist/install.dos', 'base')
INSTALLER = (ROOT / 'dist/Install.installer').read_text(encoding='latin-1')
SCRIPT = (ROOT / 'dist/install-amissl.dos').read_text(encoding='latin-1')
CHK = 'Version >NIL: LIBS:amisslmaster.library 5'


def libs_part():
    return DOS[DOS.index('Lab libs'):DOS.index('Lab copy-coreutils')]


def p_amissl():
    i = INSTALLER.index('(procedure P_AMISSL')
    return INSTALLER[i:INSTALLER.index('(P_AMISSLCHECK)\n', i)]


class InstallDosAmiSSL(unittest.TestCase):
    def test_no_caller_starts_the_interactive_installer(self):
        for body in (libs_part(), p_amissl()):
            self.assertNotIn('Installer SCRIPT Install-AmiSSL', body)
        self.assertNotIn('Installer SCRIPT', SCRIPT)

    def test_shell_install_runs_the_script_between_check_and_recheck(self):
        p = libs_part()
        run = 'Execute "{DIR}/install-amissl.dos" DIR "{DIR}" $optcpu060'
        self.assertEqual(p.count(CHK), 2)
        self.assertLess(p.index(CHK), p.index(run))
        self.assertLess(p.index(run), p.rindex(CHK))

    def test_only_without_step_and_failure_is_reported_not_fatal(self):
        p = libs_part()
        self.assertRegex(p, r'If "\{?STEP\}?" EQ ""|If "" EQ ""')
        self.assertIn('[ERROR] AmiSSL was not installed', p)
        self.assertNotIn('Quit 20', p[p.index('install-amissl.dos'):])

    def test_installer_p_amissl_executes_the_script_with_cpu_then_rechecks(self):
        run = p_amissl()
        self.assertIn('(tackon #files "install-amissl.dos")', run)
        self.assertIn('#cpu060', run)
        # AMISSL-SHELL-WINDOW: (run "Execute ...") opens a Shell window, (execute) does not
        self.assertIn('(execute "T:UPTAMISSL")', run)
        self.assertNotIn('(run "Execute T:UPTAMISSL")', run)

    def test_script_installs_libs_certs_startup_block_and_session_assign(self):
        for need in ('.KEY DIR/A,CPU060/S', 'amisslmaster.library', 'AmiSSL/68060', 'AmiSSL/68020-40',
                     '/AmiSSL/Certs', ';BEGIN AmiSSL', ';END AmiSSL', 'Assign LIBS: AmiSSL:Libs ADD',
                     'Path AmiSSL: ADD'):
            self.assertIn(need, SCRIPT)
        self.assertLess(SCRIPT.index('Copy "{DIR}/AmiSSL/Libs/AmigaOS3/amisslmaster.library"'), SCRIPT.index(CHK))
        code = '\n'.join(l for l in SCRIPT.splitlines() if not l.startswith(';'))
        self.assertNotRegex(code, r'Assign\s+\S+:\s*\n')

    def test_script_survives_the_missing_library_check(self):
        # Version on a missing library ends with rc 20, above Execute's default FailAt 10: without FailAt 21 the script
        # stops at the check, before the session Assign LIBS: (INSTALL-CHECK30-AMISSL-PREBOOT, 3.2 clean image)
        code = [l for l in SCRIPT.splitlines() if not l.startswith(';')]
        self.assertIn('FailAt 21', code)
        self.assertLess(code.index('FailAt 21'), code.index(CHK))

    def test_script_is_in_the_kit(self):
        self.assertIn('cp dist/install-amissl.dos $(KIT)/Files/install-amissl.dos', (ROOT / 'Makefile').read_text())
        self.assertRegex((ROOT / 'dist/parts.txt').read_text(), r'(?m)^-\s+Files/install-amissl\.dos$')

    def test_installer_p_anx_starts_script_with_execute_not_run(self):
        # P-ANX-RUN-EXECUTE: same fix as AMISSL-SHELL-WINDOW
        anx = INSTALLER[INSTALLER.index('(procedure P_ANX'):]
        anx = anx[:anx.index('\n(procedure ', 1)] if '\n(procedure ' in anx[1:] else anx
        self.assertIn('(execute "T:UPTANX")', anx)
        self.assertNotIn('(run "Execute T:UPTANX")', anx)


if __name__ == '__main__':
    unittest.main()
