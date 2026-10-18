"""dist/upkg on the Mac: its logic against a fake release (upkg plan P4.1).

The script's Amiga paths (T:upkg, UP-Term:, ENVARC:upkg/...) are plain relative names on the
Mac, so it runs here in a scratch directory with sh, curl (file:// for the release), the Mac's
bsdtar for the lha archives and stubs for Installer and Execute. The stub Installer runs the
archive's Install as a shell script; the stub Execute runs the Uninstall copy. vsh runs the same
script on the rig (tools/rig/upkg_rig.py); this test holds the decisions: what is installed,
what is refused, what an update picks.
"""
import hashlib, os, pathlib, shutil, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
UPKG = ROOT / 'dist' / 'upkg'
HEADER = '# upkg index 1: id archive version needs-base record size sha256\n'

STUBS = {
    # the archive's Install, as the stub Installer runs it: argv is Install APPNAME <archive> ...
    'Installer': '#!/bin/sh\necho "Installer $*" >>"$UPLOG"\nsh "./$1"\n',
    'Execute': '#!/bin/sh\necho "Execute $*" >>"$UPLOG"\nsh "$1"\n',
}


@unittest.skipUnless(shutil.which('lha') and shutil.which('sha256sum'), 'needs lha and sha256sum')
class Upkg(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.root = self.tmp / 'amiga'
        self.release = self.tmp / 'release'
        self.bin = self.tmp / 'bin'
        for d in (self.root, self.release, self.bin):
            d.mkdir()
        for name, text in STUBS.items():
            (self.bin / name).write_text(text)
            (self.bin / name).chmod(0o755)
        self.log = self.tmp / 'log'
        self.lines = []
        self.base('1.0')

    def tearDown(self):
        shutil.rmtree(self.tmp)

    def base(self, version):
        # the base's record is UP-Term: itself: on the Mac one file named UP-Term:VERSIONS
        (self.root / 'UP-Term:VERSIONS').write_text('UP-Term %s abc 2026-10-18\n' % version)

    def package(self, pid, archive, version, needs, record, finish=True):
        """An lha archive in the release whose Install writes its record, and its index line."""
        src = self.tmp / 'src' / archive
        src.mkdir(parents=True)
        rec = self.root / record
        install = 'mkdir -p "%s"\necho "%s %s def 2026-10-18" >"%s/VERSIONS"\n' % (rec, archive, version, rec)
        install += 'printf "rm -rf \\"%s\\"\\n" >"%s/Uninstall"\n' % (rec, rec)
        (src / 'Install').write_text(install if finish else 'exit 0\n')
        subprocess.run(['lha', '-aq', str(self.release / (archive + '.lha')), archive], cwd=src.parent,
                       check=True, stdout=subprocess.DEVNULL)
        data = (self.release / (archive + '.lha')).read_bytes()
        self.lines.append(' '.join((pid, archive, version, needs, record, str(len(data)),
                                    hashlib.sha256(data).hexdigest())))
        (self.release / 'upkg-index.txt').write_text(HEADER + '\n'.join(self.lines) + '\n')

    def upkg(self, *args, url_in_shell=True):
        env = dict(os.environ, PATH='%s:%s' % (self.bin, os.environ['PATH']), UPLOG=str(self.log))
        if url_in_shell:
            env['UPKGURL'] = self.release.as_uri()
        p = subprocess.run(['sh', str(UPKG)] + list(args), cwd=self.root, env=env, text=True,
                           stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        return p.returncode, p.stdout

    def recorded(self, record):
        v = self.root / record / 'VERSIONS'
        return v.read_text().split()[1] if v.exists() else None

    def test_install_runs_the_packages_installer_and_its_record_says_the_index_version(self):
        self.package('nvim', 'UP-Term-nvim', '1.0', '1.0', 'UP-Term:pkg/UP-Term-nvim')
        rc, out = self.upkg('install', 'nvim')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.recorded('UP-Term:pkg/UP-Term-nvim'), '1.0')
        self.assertIn('Installer Install APPNAME UP-Term-nvim', self.log.read_text())
        self.assertFalse((self.root / 'T:upkg').exists(), 'T:upkg left behind')

    def test_the_url_override_set_with_setenv_is_read_from_env(self):
        # vsh takes only HOME, USER, HOST and HOSTNAME from ENV: into the shell, so after
        # `SetEnv UPKGURL <url>` the script sees no $UPKGURL: it must read ENV:UPKGURL itself
        self.package('nvim', 'UP-Term-nvim', '1.1', '1.0', 'UP-Term:pkg/UP-Term-nvim')
        (self.root / 'ENV:UPKGURL').write_text(self.release.as_uri() + '\n')
        rc, out = self.upkg('list', url_in_shell=False)
        self.assertEqual(rc, 0, out)
        self.assertIn('UP-Term-nvim', out)

    def test_list_shows_installed_and_available(self):
        self.package('nvim', 'UP-Term-nvim', '1.1', '1.0', 'UP-Term:pkg/UP-Term-nvim')
        self.package('python', 'Python3', '1.0', '-', 'ENVARC:upkg/Python3')
        (self.root / 'UP-Term:pkg/UP-Term-nvim').mkdir(parents=True)
        (self.root / 'UP-Term:pkg/UP-Term-nvim/VERSIONS').write_text('UP-Term-nvim 1.0 x y\n')
        rc, out = self.upkg('list')
        self.assertEqual(rc, 0, out)
        rows = {l.split()[0]: l.split()[1:3] for l in out.splitlines()[1:]}
        self.assertEqual(rows, {'UP-Term-nvim': ['1.0', '1.1'], 'Python3': ['-', '1.0']})

    def test_a_package_needing_a_newer_base_is_refused_before_any_download(self):
        self.package('nvim', 'UP-Term-nvim', '2.0', '1.10', 'UP-Term:pkg/UP-Term-nvim')
        self.base('1.9')
        rc, out = self.upkg('install', 'UP-Term-nvim')
        self.assertEqual(rc, 1, out)
        self.assertIn('needs UP-Term 1.10', out)
        self.assertFalse(self.log.exists(), 'the Installer ran')

    def test_a_download_whose_sha256_differs_from_the_index_installs_nothing(self):
        self.package('python', 'Python3', '1.0', '-', 'ENVARC:upkg/Python3')
        lha = self.release / 'Python3.lha'
        data = bytearray(lha.read_bytes())
        data[-1] ^= 0xff
        lha.write_bytes(bytes(data))
        rc, out = self.upkg('install', 'Python3')
        self.assertEqual(rc, 1, out)
        self.assertIn('sha256', out)
        self.assertIsNone(self.recorded('ENVARC:upkg/Python3'))
        self.assertFalse(self.log.exists(), 'the Installer ran')

    def test_an_installer_left_early_is_reported_as_not_installed(self):
        self.package('python', 'Python3', '1.0', '-', 'ENVARC:upkg/Python3', finish=False)
        rc, out = self.upkg('install', 'python')
        self.assertEqual(rc, 1, out)
        self.assertIn('not installed', out)

    def test_update_installs_only_older_installed_add_ons(self):
        self.package('nvim', 'UP-Term-nvim', '1.2', '1.0', 'UP-Term:pkg/UP-Term-nvim')
        self.package('python', 'Python3', '1.0', '-', 'ENVARC:upkg/Python3')
        self.package('claude', 'Claude', '1.1', '-', 'ENVARC:upkg/Claude')
        for rec, line in (('UP-Term:pkg/UP-Term-nvim', 'UP-Term-nvim 1.1'), ('ENVARC:upkg/Claude', 'Claude 1.1')):
            (self.root / rec).mkdir(parents=True)
            (self.root / rec / 'VERSIONS').write_text(line + ' x y\n')
        rc, out = self.upkg('update')
        self.assertEqual(rc, 0, out)
        self.assertEqual(self.recorded('UP-Term:pkg/UP-Term-nvim'), '1.2')
        self.assertIsNone(self.recorded('ENVARC:upkg/Python3'), 'update installed a package that was not there')
        self.assertEqual(self.log.read_text().count('Installer '), 1)

    def test_the_base_is_not_installed_from_inside_itself(self):
        self.package('base', 'UP-Term', '1.1', '-', 'UP-Term:')
        rc, out = self.upkg('install', 'UP-Term')
        self.assertEqual(rc, 1, out)
        self.assertIn('own archive', out)

    def test_remove_runs_a_copy_of_the_records_uninstall_without_the_network(self):
        self.package('python', 'Python3', '1.0', '-', 'ENVARC:upkg/Python3')
        self.assertEqual(self.upkg('install', 'Python3')[0], 0)
        (self.release / 'upkg-index.txt').unlink()
        rc, out = self.upkg('remove', 'Python3')
        self.assertEqual(rc, 0, out)
        self.assertIsNone(self.recorded('ENVARC:upkg/Python3'))
        self.assertIn('Execute T:upkg/Uninstall', self.log.read_text())

    def test_a_release_without_an_index_is_an_error_not_an_empty_list(self):
        rc, out = self.upkg('list')
        self.assertEqual(rc, 1, out)
        self.assertIn('cannot download', out)


    def curl_exits(self, rc):
        # the kit curl's two failures seen on the rig: 22 for an HTTP error (a 404 with AmiSSL working),
        # 20 when it cannot start (amisslmaster.library missing: curl 8.22 opens it for http too)
        (self.bin / 'curl').write_text('#!/bin/sh\nexit %d\n' % rc)
        (self.bin / 'curl').chmod(0o755)

    def test_a_file_missing_from_the_release_is_not_blamed_on_amissl(self):
        self.curl_exits(22)
        rc, out = self.upkg('list')
        self.assertEqual(rc, 1, out)
        self.assertIn('not in the release', out)
        self.assertNotIn('AmiSSL', out)

    def test_a_curl_that_cannot_start_points_at_the_amissl_part(self):
        self.curl_exits(20)
        rc, out = self.upkg('list')
        self.assertEqual(rc, 1, out)
        self.assertIn('AmiSSL part', out)

if __name__ == '__main__':
    unittest.main()
