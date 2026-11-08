#!/usr/bin/env python3
"""DIST-ICONLIB: the kit carries Peter Keunecke's IconLib and the Install offers it.
  - dist/iconlib holds the two builds (46.4 for OS 3.0/3.1, 51.4 for 3.2), the guide and the licence page,
    and dist/parts.txt, the Makefile's dist-base and LICENSES.txt know them;
  - Install.installer asks "PeterK icon library": ticked on 3.0/3.1, unticked on 3.2 (exec 47);
  - install.dos keeps LIBS:icon.library.amarchy-backup before it copies, only when none exists,
    and picks the 51.4 build only on exec 47; Uninstall puts the backup back."""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import mkinstaller as mi

DIR = ROOT / 'dist/iconlib'
FIX = ROOT / 'tests/fixtures'
# the arguments install.dos hands instssblock.sh for the two blocks it inserts
REMLIB = ('RemLib', 'SetPatch', 'If EXISTS LIBS:icon.library', '   RemLib >NIL: icon.library', 'EndIf')
FLAT = ('Amarchy-Flat', 'IPrefs', 'If EXISTS SYS:UP-Term/bin/amarchy-flat', '  Run >NIL: SYS:UP-Term/bin/amarchy-flat BOOT', 'EndIf')
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

    def test_remlib_is_in_the_kit_and_credited(self):
        self.assertEqual((DIR / 'RemLib').read_bytes()[:4], b'\x00\x00\x03\xf3')  # an Amiga executable
        self.assertTrue((DIR / 'RemLib.doc').is_file())
        self.assertIn('Thomas Rapp', (DIR / 'SOURCE.txt').read_text(encoding='latin-1'))
        self.assertIn('RemLib', (ROOT / 'dist/LICENSES.txt').read_text(encoding='latin-1'))

    def test_remlib_only_on_30_31_and_block_before_setpatch(self):
        p = DOS[DOS.index('If "$opticonlib" EQ "ICONLIB"'):]
        p = p[:p.index('UnSet upiconlib')]
        gate = p.index('If "$upiconlib" EQ iconlib/icon.library.os30')
        self.assertLess(p.index('Version >NIL: exec.library 47'), gate)  # 3.2 switches to os32 before the gate
        for line in ('Copy iconlib/RemLib C:RemLib CLONE', 'vsh instssblock.sh RemLib SetPatch',
                     'Copy T:Startup-Sequence.up-term S:Startup-Sequence CLONE QUIET'):
            self.assertGreater(p.index(line), gate)

    def test_instssblock_remlib_inserts_before_setpatch_once_and_unstartup_strips_it(self):
        import subprocess, tempfile
        seq = 'Assign >NIL: LIBS: SYS:Libs\n  c:SetPatch >NIL: QUIET\nAddBuffers df0: 20\nSetPatch\n'
        with tempfile.TemporaryDirectory() as d:
            def sh(script, *args):
                src = re.sub(r'(?<![A-Za-z])[ST]:', d + '/', pathlib.Path(script).read_text())
                return subprocess.run(['sh', '-c', src, 'x', *args], capture_output=True, text=True, check=True)
            (pathlib.Path(d) / 'Startup-Sequence').write_text(seq)
            sh(ROOT / 'dist/instssblock.sh', *REMLIB)
            out = (pathlib.Path(d) / 'Startup-Sequence.up-term').read_text()
            block = ';BEGIN UP-Term RemLib\nIf EXISTS LIBS:icon.library\n   RemLib >NIL: icon.library\nEndIf\n;END UP-Term RemLib\n'
            self.assertEqual(out, 'Assign >NIL: LIBS: SYS:Libs\n' + block + '  c:SetPatch >NIL: QUIET\nAddBuffers df0: 20\nSetPatch\n')
            (pathlib.Path(d) / 'Startup-Sequence').write_text(out)
            (pathlib.Path(d) / 'Startup-Sequence.up-term').unlink()
            sh(ROOT / 'dist/instssblock.sh', *REMLIB)  # second run: block present, writes nothing
            self.assertFalse((pathlib.Path(d) / 'Startup-Sequence.up-term').exists())
            sh(ROOT / 'dist/unstartup.sh', '', d + '/Startup-Sequence')
            self.assertEqual((pathlib.Path(d) / 'Startup-Sequence.up-term').read_text(), seq)

    def _ins(self, d, *args):
        import subprocess
        src = re.sub(r'(?<![A-Za-z])[ST]:', d + '/', (ROOT / 'dist/instssblock.sh').read_text())
        subprocess.run(['sh', '-c', src, 'x', *args], capture_output=True, text=True, check=True)
        return pathlib.Path(d) / 'Startup-Sequence.up-term'

    def test_instssblock_flat_goes_before_iprefs_on_stock_31_and_322_sequences_idempotently(self):
        """Amarchy-Flat is inserted directly before the (C:)IPrefs line of the real stock sequences (after
        AddDataTypes), once, and unstartup.sh strips it by name and by the plain call, byte for byte."""
        import tempfile
        for fixture, iprefs in (('Startup-Sequence.ks31', 'C:IPrefs'), ('Startup-Sequence.ks322', 'IPrefs')):
            seq = (FIX / fixture).read_text()
            with tempfile.TemporaryDirectory() as d:
                (pathlib.Path(d) / 'Startup-Sequence').write_text(seq)
                out = self._ins(d, *FLAT).read_text()
                block = ';BEGIN UP-Term Amarchy-Flat\n' + '\n'.join(FLAT[2:]) + '\n;END UP-Term Amarchy-Flat\n'
                self.assertEqual(out, seq.replace(iprefs + '\n', block + iprefs + '\n', 1), fixture)
                lines = out.splitlines()
                self.assertTrue(lines[lines.index(';END UP-Term Amarchy-Flat') + 1].endswith('IPrefs'))
                self.assertIn('AddDataTypes', lines[lines.index(';BEGIN UP-Term Amarchy-Flat') - 1])
                (pathlib.Path(d) / 'Startup-Sequence').write_text(out)
                self.assertFalse(self._ins(d, *FLAT).exists(), 'second run must write nothing')
                for strip in (('Amarchy-Flat',), ('',)):
                    import subprocess
                    src = re.sub(r'(?<![A-Za-z])[ST]:', d + '/', (ROOT / 'dist/unstartup.sh').read_text())
                    subprocess.run(['sh', '-c', src, 'x', *strip, d + '/Startup-Sequence'], check=True, capture_output=True)
                    self.assertEqual((pathlib.Path(d) / 'Startup-Sequence.up-term').read_text(), seq, fixture)

    def test_instssblock_flat_is_a_noop_without_iprefs_and_remlib_block_leaves_flat_alone(self):
        import tempfile
        seq = 'Assign >NIL: LIBS: SYS:Libs\nAddDataTypes REFRESH QUIET\nConClip\n'
        with tempfile.TemporaryDirectory() as d:
            (pathlib.Path(d) / 'Startup-Sequence').write_text(seq)
            self.assertFalse(self._ins(d, *FLAT).exists())
            # a RemLib block present does not stop the Flat block (the guard is per block name)
            (pathlib.Path(d) / 'Startup-Sequence').write_text((FIX / 'Startup-Sequence.ks31').read_text())
            with_remlib = self._ins(d, *REMLIB).read_text()
            (pathlib.Path(d) / 'Startup-Sequence').write_text(with_remlib)
            (pathlib.Path(d) / 'Startup-Sequence.up-term').unlink()
            out = self._ins(d, *FLAT).read_text()
            self.assertIn(';BEGIN UP-Term RemLib', out)
            self.assertIn(';BEGIN UP-Term Amarchy-Flat', out)

    def test_uninstall_strips_block_and_removes_our_remlib_only(self):
        self.assertIn('$unvsh "$unst" "" S:Startup-Sequence', UNINSTALL)
        i = UNINSTALL.index('If EXISTS ENVARC:up-term/remlib')
        self.assertIn('Delete >NIL: C:RemLib QUIET', UNINSTALL[i:i + 80])
        self.assertIn('Echo "REMLIB" >ENVARC:up-term/remlib', DOS)


if __name__ == '__main__':
    unittest.main()
