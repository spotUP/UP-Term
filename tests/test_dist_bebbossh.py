#!/usr/bin/env python3
"""K6: BebboSSH 1.46.1 ships BUILT BY UP-TERM from the 1.45 source + the 21 patches, not as the fork's binaries.

- dist/net/bebbossh-1.46.1 holds the source tarball (GPL), the 21 patches in a gap-free order, SOURCE.txt, the licence and
  no binary; the tarball is the one SHA256SUMS names;
- `make dist` stages build/bebbossh/out over that drawer (the Makefile line and the build script's patch count);
- the staged kit (build/dist/up-term/Files/net/bebbossh-1.46.1, when `make dist` ran): our six binaries, each with the
  checksum its SHA256SUMS lists, the programs and libraries with a $VER of 1.46.1. Skipped when no kit is staged,
  unless BEBBOSSH_REQUIRE=1 (make dist-check)."""
import hashlib, os, pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DRAWER = ROOT / 'dist/net/bebbossh-1.46.1'
KIT = ROOT / 'build/dist/up-term/Files/net/bebbossh-1.46.1'
BINARIES = ['bebbossh', 'bebboscp', 'bebbosshd', 'bebbosshkeygen',
            'libcryptossh.library020', 'libcryptossh.library060']
PLAIN = 'libcryptossh.library'  # the 68000 build: the kit needs a 68020, nothing installs it, so it is not built or shipped


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


class RepoDrawer(unittest.TestCase):
    def test_the_fork_binaries_are_not_in_the_repo(self):
        for b in BINARIES:
            self.assertFalse((DRAWER / b).exists(), b)

    def test_patches_are_numbered_one_to_twentytwo_without_a_gap(self):
        names = sorted(p.name for p in (DRAWER / 'patches').glob('*.patch'))
        self.assertEqual([int(n[:4]) for n in names], list(range(1, 23)))

    def test_the_window_size_patches_ask_vtcon_and_follow_it(self):
        pd = DRAWER / 'patches'
        g = (pd / '0020-gwinsz-console-size.patch').read_text()
        self.assertIn('0x7657', g)
        self.assertIn('getConsoleWinsize', g)
        f = (pd / '0021-follow-window-size.patch').read_text()
        self.assertIn('followWindowSize', f)
        self.assertIn('sendWindowResize', f)
        u = (pd / '0022-utf8-in-vtcon-window.patch').read_text()
        self.assertIn('getConsoleWinsize(&winCols, &winRows)', u)
        self.assertIn('charsetConv = false', u)

    def test_the_source_tarball_is_the_one_the_sums_name(self):
        sums = dict((l.split()[1], l.split()[0]) for l in (DRAWER / 'SHA256SUMS').read_text().splitlines())
        self.assertEqual(sums['./bebbossh-1.45-src.tar.bz2'], sha(DRAWER / 'bebbossh-1.45-src.tar.bz2'))
        for b in BINARIES:
            self.assertNotIn('./' + b, sums, 'the repo sums list a binary that is not in the repo')

    def test_the_build_is_wired_into_dist(self):
        mk = (ROOT / 'Makefile').read_text()
        # a prerequisite of dist-base, wherever it stands among the others
        self.assertRegex(mk, r'(?m)^dist-base:.* bebbossh ')
        self.assertIn('cp $(BUILD)/bebbossh/out/* $(KIT)/Files/net/bebbossh-1.46.1/', mk)
        self.assertIn('[ "$n" = 22 ]', (ROOT / 'tools/build_bebbossh.sh').read_text())
        self.assertIn('6.5.0b', (DRAWER / 'SOURCE.txt').read_text())


class InstallScript(unittest.TestCase):
    '''dist/install.dos, the SSH block: an upgrade replaces the library of an earlier UP-Term install (our marker), a
    library without the marker is the user's own and is kept with a note when it is older. (Run on the rig: install_rig.py
    net-upgrade.) Install.installer runs this same script; Uninstall removes only the marked library.'''
    @classmethod
    def setUpClass(cls):
        s = (ROOT / 'dist/install.dos').read_text()
        cls.block = s[s.index('If "$optssh" EQ "SSH"'):s.index('If "$optbebboget" EQ "BEBBOGET"')]

    def test_an_upgrade_over_an_earlier_install_replaces_the_library(self):
        # the old guard "If NOT EXISTS LIBS:libcryptossh.library" around the Copy kept the 1.45 library on an upgrade
        self.assertNotRegex(self.block, r'(?m)^  If NOT EXISTS LIBS:libcryptossh\.library\n    If "\$optcpu060"')

        copy = self.block.index('Copy net/bebbossh-1.46.1/libcryptossh.library060')
        self.assertIn('If "$cslib" EQ "install"', self.block[:copy])
        marker = self.block.index('Set cslib "install"')
        keep = self.block.index('If NOT EXISTS ENVARC:up-term/cryptossh')
        self.assertLess(marker, keep)  # install is the default; only a library without our marker turns it into keep
        self.assertIn('Set cslib "keep"', self.block[keep:copy])

    def test_the_users_own_older_library_is_kept_with_the_replace_command(self):
        keep = self.block[self.block.index('Set cslib "keep"'):self.block.index('If "$cslib" EQ "install"')]
        self.assertIn('Search >NIL: LIBS:libcryptossh.library "libcryptossh (1.4[6-9]', keep)
        self.assertIn('PATTERN', keep)
        self.assertIn('Copy <kit>/Files/net/bebbossh-1.46.1/libcryptossh.library020 LIBS:libcryptossh.library CLONE', keep)
        self.assertIn('Copy <kit>/Files/net/bebbossh-1.46.1/libcryptossh.library060 LIBS:libcryptossh.library CLONE', keep)
        self.assertNotIn('Copy net/', keep)  # the kept branch only says the command, never runs it

    def test_the_marker_is_written_with_every_install_and_uninstall_removes_only_ours(self):
        self.assertIn('Echo >ENVARC:up-term/cryptossh "installed by UP-Term"', self.block)
        un = (ROOT / 'dist/Uninstall').read_text()
        self.assertRegex(un, r'If EXISTS ENVARC:up-term/cryptossh\n  Delete >NIL: LIBS:libcryptossh\.library')

    def test_only_install_dos_installs_the_library(self):
        # Install.installer runs install.dos (its help text only names the library); no add-on or base part copies it
        for p in (ROOT / 'dist').rglob('*'):
            if p.is_file() and 'net/bebbossh' not in str(p) and p.suffix not in ('.lha', '.txt'):
                t = p.read_bytes()
                if p.name not in ('install.dos', 'Uninstall', 'Install.installer') and b'LIBS:libcryptossh' in t:
                    self.fail('%s installs or touches the library' % p)
        self.assertNotRegex((ROOT / 'dist/Install.installer').read_text(), r'\(copyfiles[^)]*cryptossh')


class StagedKit(unittest.TestCase):
    def setUp(self):
        if not KIT.is_dir():
            if os.environ.get('BEBBOSSH_REQUIRE') == '1':
                self.fail('no staged kit: make dist')
            self.skipTest('no staged kit (make dist)')

    def test_the_kit_holds_our_binaries_with_the_checksums_of_the_sums_file(self):
        sums = dict((l.split()[1], l.split()[0]) for l in (KIT / 'SHA256SUMS').read_text().splitlines())
        for b in BINARIES:
            self.assertEqual(sums['./' + b], sha(KIT / b), b)
        self.assertEqual(sums['./bebbossh-1.45-src.tar.bz2'], sha(KIT / 'bebbossh-1.45-src.tar.bz2'))

    def test_the_unused_68000_library_is_not_shipped(self):
        self.assertFalse((KIT / PLAIN).exists())
        self.assertNotIn('./' + PLAIN + '\n', (KIT / 'SHA256SUMS').read_text() + '\n')
        self.assertNotIn('out/' + PLAIN + '"', (ROOT / 'tools/build_bebbossh.sh').read_text())

    def test_the_binaries_say_1_46_1(self):
        for b in BINARIES[:4]:
            self.assertTrue(re.search(rb'\$VER: 1\.46\.1 ', (KIT / b).read_bytes()), b)
        for b in BINARIES[4:]:
            self.assertTrue(re.search(rb'libcryptossh 1\.46 ', (KIT / b).read_bytes()), b)


if __name__ == '__main__':
    unittest.main()
