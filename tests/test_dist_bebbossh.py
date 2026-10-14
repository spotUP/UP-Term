#!/usr/bin/env python3
"""K6: BebboSSH 1.46.1 ships BUILT BY UP-TERM from the 1.45 source + the 19 patches, not as the fork's binaries.

- dist/net/bebbossh-1.46.1 holds the source tarball (GPL), the 19 patches in a gap-free order, SOURCE.txt, the licence and
  no binary; the tarball is the one SHA256SUMS names;
- `make dist` stages build/bebbossh/out over that drawer (the Makefile line and the build script's patch count);
- the staged kit (build/dist/UP-Term/Files/net/bebbossh-1.46.1, when `make dist` ran): our seven binaries, each with the
  checksum its SHA256SUMS lists, the programs and libraries with a $VER of 1.46.1. Skipped when no kit is staged,
  unless BEBBOSSH_REQUIRE=1 (make dist-check)."""
import hashlib, os, pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
DRAWER = ROOT / 'dist/net/bebbossh-1.46.1'
KIT = ROOT / 'build/dist/UP-Term/Files/net/bebbossh-1.46.1'
BINARIES = ['bebbossh', 'bebboscp', 'bebbosshd', 'bebbosshkeygen',
            'libcryptossh.library', 'libcryptossh.library020', 'libcryptossh.library060']


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


class RepoDrawer(unittest.TestCase):
    def test_the_fork_binaries_are_not_in_the_repo(self):
        for b in BINARIES:
            self.assertFalse((DRAWER / b).exists(), b)

    def test_patches_are_numbered_one_to_nineteen_without_a_gap(self):
        names = sorted(p.name for p in (DRAWER / 'patches').glob('*.patch'))
        self.assertEqual([int(n[:4]) for n in names], list(range(1, 20)))

    def test_the_source_tarball_is_the_one_the_sums_name(self):
        sums = dict((l.split()[1], l.split()[0]) for l in (DRAWER / 'SHA256SUMS').read_text().splitlines())
        self.assertEqual(sums['./bebbossh-1.45-src.tar.bz2'], sha(DRAWER / 'bebbossh-1.45-src.tar.bz2'))
        for b in BINARIES:
            self.assertNotIn('./' + b, sums, 'the repo sums list a binary that is not in the repo')

    def test_the_build_is_wired_into_dist(self):
        mk = (ROOT / 'Makefile').read_text()
        self.assertRegex(mk, r'(?m)^dist-base: amiga bebbossh ')
        self.assertIn('cp $(BUILD)/bebbossh/out/* $(KIT)/Files/net/bebbossh-1.46.1/', mk)
        self.assertIn('[ "$n" = 19 ]', (ROOT / 'tools/build_bebbossh.sh').read_text())
        self.assertIn('6.5.0b', (DRAWER / 'SOURCE.txt').read_text())


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

    def test_the_binaries_say_1_46_1(self):
        for b in BINARIES[:4]:
            self.assertTrue(re.search(rb'\$VER: 1\.46\.1 ', (KIT / b).read_bytes()), b)
        for b in BINARIES[4:]:
            self.assertTrue(re.search(rb'libcryptossh 1\.46 ', (KIT / b).read_bytes()), b)


if __name__ == '__main__':
    unittest.main()
