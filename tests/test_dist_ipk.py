"""The .ipk feed (upkg plan P10.4): tools/pkgs.py ipk and feed.

Structure and determinism run on a small staged tree in a scratch build drawer with its own
packages table. The feed of the real build (build/ipk, from `make dist-ipk`) is held to
dist/packages.txt and to the files beside it: skipped when it was not built, required with
IPK_REQUIRE=1 (make dist-check). A version bumped in packages.txt without rebuilding fails it.
"""
import gzip, hashlib, io, os, pathlib, shutil, subprocess, sys, tarfile, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import pkgs

TABLE = """\
base      UP-Term       1.0  -    -   UP-Term:                  -  vtcon
nvim      up-nvim  2.1  1.0  80  UP-Term:pkg/up-nvim  -  vtcon
claude    Claude        1.1  -    -   ENVARC:upkg/up-claude        -  vtcon
"""


def stage(build, archive):
    kit = build / 'dist' / archive
    (kit / 'Files/sub').mkdir(parents=True)
    (kit / 'Files/install.dos').write_bytes(b'; install\n')
    (kit / 'Files/sub/prog').write_bytes(b'\x00' * 3000)
    os.chmod(kit / 'Files/sub/prog', 0o755)
    (kit / 'Uninstall').write_bytes(b'; uninstall\n')


def split_ar(data):
    assert data[:8] == b'!<arch>\n'
    out, i = [], 8
    while i < len(data):
        h = data[i:i + 60]
        assert h[58:60] == b'`\n', h
        size = int(h[48:58])
        out.append((h[:16].decode().strip().rstrip('/'), data[i + 60:i + 60 + size]))
        i += 60 + size + size % 2
    return out


def stanzas(text):
    return [dict(l.split(': ', 1) for l in b.splitlines()) for b in text.strip().split('\n\n')]


def check_feed(build, table):
    """Raise AssertionError unless <build>/ipk/Packages.gz says what the table and the files say."""
    out = pathlib.Path(build) / 'ipk'
    feed = stanzas(gzip.decompress((out / 'Packages.gz').read_bytes()).decode())
    rows = pkgs.addons(table)
    assert [s['Package'] for s in feed] == [r['archive'] for r in rows], 'packages differ'
    for s, r in zip(feed, rows):
        assert s['Version'] == r['version'], '%s: feed %s, packages.txt %s: rebuild with make dist-ipk' % (
            r['archive'], s['Version'], r['version'])
        data = (out / s['Filename']).read_bytes()
        assert int(s['Size']) == len(data), s['Filename']
        assert s['SHA256sum'] == hashlib.sha256(data).hexdigest(), s['Filename']


class IpkTest(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.tmp)
        self.build = self.tmp / 'build'
        self.table = self.tmp / 'packages.txt'
        self.table.write_text(TABLE)
        for a in ('up-nvim', 'Claude'):
            stage(self.build, a)
        pkgs.ipk(self.build, self.table)
        pkgs.write_feed(self.build, self.table)
        self.out = self.build / 'ipk'

    def test_ipk_is_an_ar_archive_with_control_and_data_at_the_right_paths(self):
        f = self.out / 'up-nvim_2.1_m68k-amigaos.ipk'
        names = subprocess.run(['ar', 't', str(f)], capture_output=True, text=True, check=True).stdout.split()
        names = [n.rstrip('/') for n in names]
        self.assertEqual(names, ['debian-binary', 'control.tar.gz', 'data.tar.gz'])
        m = dict(split_ar(f.read_bytes()))
        self.assertEqual(m['debian-binary'], b'2.0\n')
        ctl = tarfile.open(fileobj=io.BytesIO(m['control.tar.gz']))
        self.assertEqual(sorted(ctl.getnames()), ['.', './control', './postinst', './preinst', './prerm'])
        text = ctl.extractfile('./control').read().decode()
        for line in ('Package: up-nvim', 'Version: 2.1', 'Architecture: m68k-amigaos',
                     'Installed-Size: 3'):
            self.assertIn(line + '\n', text)
        self.assertNotIn('Depends', text)
        post = ctl.extractfile('./postinst').read().decode()
        self.assertIn('Execute T:upkg-stage/up-nvim/Files/install.dos T:upkg-stage/up-nvim/Files\n', post)
        self.assertIn('rm -rf T:upkg-stage/up-nvim\n', post)
        pre = ctl.extractfile('./preinst').read().decode()
        self.assertIn('head -n 1 UP-Term:VERSIONS', pre)
        self.assertIn('if older "$base" 1.0; then', pre)
        self.assertIn('exit 1', pre)
        self.assertIn('Execute UP-Term:pkg/up-nvim/Uninstall\n', ctl.extractfile('./prerm').read().decode())
        data = tarfile.open(fileobj=io.BytesIO(m['data.tar.gz']))
        top = './T/upkg-stage/up-nvim'
        for n in (top + '/Uninstall', top + '/Files/install.dos', top + '/Files/sub/prog'):
            self.assertIn(n, data.getnames())
        self.assertEqual(data.getmember(top + '/Files/sub/prog').mode, 0o755)
        self.assertEqual(data.getmember(top + '/Uninstall').mode, 0o644)
        self.assertTrue(all(n in ('.', './T', './T/upkg-stage') or n.startswith('./T/upkg-stage/') for n in data.getnames()))
        no_need = tarfile.open(fileobj=io.BytesIO(dict(split_ar((self.out / 'Claude_1.1_m68k-amigaos.ipk').read_bytes()))['control.tar.gz']))
        self.assertNotIn('./preinst', no_need.getnames())

    def ctl_of(self, ipk):
        m = dict(split_ar((self.out / ipk).read_bytes()))
        return tarfile.open(fileobj=io.BytesIO(m['control.tar.gz'])), tarfile.open(fileobj=io.BytesIO(m['data.tar.gz']))

    def test_questions_are_asked_and_passed_to_install_dos(self):
        """P10.6: Files/questions makes postinst run upkg-ask and pass each answer as install.dos's keyword"""
        kit = self.build / 'dist/Claude/Files'
        (kit / 'install.dos').write_bytes(b'.KEY DIR/A,STEP/K,REMOTE/K\n; install\n')
        (kit / 'questions').write_bytes(b'# asked by upkg-ask\nREMOTE|string|Host and port|<ENVARC:Claude/remote|\n')
        pkgs.ipk(self.build, self.table)
        ctl, data = self.ctl_of('Claude_1.1_m68k-amigaos.ipk')
        post = ctl.extractfile('./postinst').read().decode()
        st = 'T:upkg-stage/Claude'
        self.assertIn('if [ -f UP-Term:bin/upkg-ask ] && vsh UP-Term:bin/upkg-ask %s/Files/questions %s/answers Claude; then\n'
                      '    . %s/answers\n    set -- REMOTE "$REMOTE"\nfi\n' % (st, st, st), post)
        self.assertIn('Execute %s/Files/install.dos %s/Files "$@"\n' % (st, st), post)
        self.assertIn('./T/upkg-stage/Claude/Files/questions', data.getnames())
        # an add-on with no questions keeps the plain Execute line
        post = self.ctl_of('up-nvim_2.1_m68k-amigaos.ipk')[0].extractfile('./postinst').read().decode()
        self.assertNotIn('upkg-ask', post)

    def test_a_question_that_is_no_install_dos_keyword_is_refused(self):
        (self.build / 'dist/Claude/Files/questions').write_bytes(b'HOST|string|Host|x|\n')
        with self.assertRaises(SystemExit) as e:
            pkgs.ipk(self.build, self.table)
        self.assertIn('HOST is not a keyword', str(e.exception))

    def test_prerm_warns_and_continues_when_the_uninstall_record_is_missing(self):
        # an add-on installed before the Uninstall copy was fixed has no record: the prerm must not fail the upgrade
        for ipk, rec in (('up-nvim_2.1_m68k-amigaos.ipk', 'UP-Term:pkg/up-nvim/Uninstall'),
                         ('Claude_1.1_m68k-amigaos.ipk', None)):
            m = dict(split_ar((self.out / ipk).read_bytes()))
            prerm = tarfile.open(fileobj=io.BytesIO(m['control.tar.gz'])).extractfile('./prerm').read().decode()
            lines = prerm.splitlines()
            rec = rec or next(w for w in prerm.split() if w.endswith('/Uninstall'))
            self.assertIn('if [ -f %s ]; then' % rec, lines)
            self.assertLess(lines.index('if [ -f %s ]; then' % rec), lines.index('    Execute ' + rec))
            self.assertTrue(any(l.lstrip().startswith('echo "warning: ' + rec) for l in lines))
            self.assertEqual(lines[-1], 'exit 0')

    def test_the_base_is_not_a_package(self):
        self.assertFalse(list(self.out.glob('UP-Term_*')))
        self.assertEqual([s['Package'] for s in stanzas(gzip.decompress((self.out / 'Packages.gz').read_bytes()).decode())],
                         ['up-nvim', 'Claude'])
        self.assertNotIn('Depends', (self.out / 'Packages').read_text())

    def test_rebuild_is_byte_equal(self):
        before = {p.name: p.read_bytes() for p in self.out.iterdir()}
        pkgs.ipk(self.build, self.table)
        pkgs.write_feed(self.build, self.table)
        self.assertEqual(before, {p.name: p.read_bytes() for p in self.out.iterdir()})

    def test_feed_matches_table_and_files(self):
        check_feed(self.build, self.table)

    def test_feed_fails_when_a_version_changes_without_rebuilding(self):
        self.table.write_text(TABLE.replace('2.1', '2.2'))
        with self.assertRaises(AssertionError) as e:
            check_feed(self.build, self.table)
        self.assertIn('rebuild', str(e.exception))

    def test_feed_fails_when_a_file_changes_after_the_feed(self):
        with open(self.out / 'Claude_1.1_m68k-amigaos.ipk', 'ab') as f:
            f.write(b'x')
        with self.assertRaises(AssertionError):
            check_feed(self.build, self.table)

    def test_feed_of_an_unbuilt_ipk_is_refused(self):
        (self.out / 'Claude_1.1_m68k-amigaos.ipk').unlink()
        with self.assertRaises(SystemExit):
            pkgs.feed(self.build, self.table)


class RenamedPackages(unittest.TestCase):
    """The up-* rename: the real table, every add-on staged, the control and the Packages entry of each renamed
    package say it replaces, conflicts with and provides its old name (opkg then removes the old package
    first); the base UP-Term was never renamed and says nothing."""
    @classmethod
    def setUpClass(cls):
        cls.tmp = pathlib.Path(tempfile.mkdtemp())
        cls.build = cls.tmp / 'build'
        for r in pkgs.addons(pkgs.TABLE):
            stage(cls.build, r['archive'])
        pkgs.ipk(cls.build, pkgs.TABLE)
        cls.packages = pkgs.feed(cls.build, pkgs.TABLE)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp)

    def control(self, r):
        m = dict(split_ar((self.build / 'ipk' / pkgs.ipk_name(r)).read_bytes()))
        return tarfile.open(fileobj=io.BytesIO(m['control.tar.gz'])).extractfile('./control').read().decode()

    def test_the_seven_renames_are_in_every_control_and_in_the_packages_index(self):
        want = {'Claude': 'up-claude', 'amiga-pi': 'up-pi', 'UP-Term-nvim': 'up-nvim', 'Python3': 'up-python',
                'UP-Term-git': 'up-git', 'UP-Term-perl': 'up-perl', 'UP-Term-vim': 'up-vim'}
        self.assertEqual(dict(pkgs.renames()), want)
        by_name = {s['Package']: s for s in stanzas(self.packages)}
        for old, new in want.items():
            r = next(r for r in pkgs.addons(pkgs.TABLE) if r['archive'] == new)
            for text in (self.control(r), self.packages_entry(by_name[new])):
                for field in ('Replaces', 'Conflicts', 'Provides'):
                    self.assertIn('%s: %s\n' % (field, old), text, '%s: %s' % (new, field))
            for s in (dict(l.split(': ', 1) for l in self.control(r).splitlines()), by_name[new]):
                self.assertEqual((s['Replaces'], s['Conflicts'], s['Provides']), (old, old, old))

    def packages_entry(self, stanza):
        return ''.join('%s: %s\n' % kv for kv in stanza.items())

    def test_the_base_and_an_unrenamed_package_carry_none(self):
        base = pkgs.rows()[0]
        self.assertEqual(base['archive'], 'up-term')
        self.assertEqual(pkgs.old_names('up-term'), [])
        self.assertEqual(pkgs.old_record(base), [])
        self.assertEqual(pkgs._rename_fields(base), [])
        # a package of the scratch table that was never renamed
        self.assertEqual(pkgs._rename_fields({'archive': 'Neverrenamed'}), [])


class RealFeed(unittest.TestCase):
    def test_built_feed_equals_packages_txt_and_the_files(self):
        build = ROOT / 'build'
        if not (build / 'ipk/Packages.gz').is_file():
            if os.environ.get('IPK_REQUIRE'):
                self.fail('build/ipk/Packages.gz missing: make dist-ipk')
            self.skipTest('build/ipk not built (make dist-ipk)')
        check_feed(build, pkgs.TABLE)


if __name__ == '__main__':
    unittest.main()
