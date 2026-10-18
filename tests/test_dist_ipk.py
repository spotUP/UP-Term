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
nvim      UP-Term-nvim  2.1  1.0  80  UP-Term:pkg/UP-Term-nvim  -  vtcon
claude    Claude        1.1  -    -   ENVARC:upkg/Claude        -  vtcon
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
    rows = pkgs.rows(table)
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
        for a in ('UP-Term', 'UP-Term-nvim', 'Claude'):
            stage(self.build, a)
        pkgs.ipk(self.build, self.table)
        pkgs.write_feed(self.build, self.table)
        self.out = self.build / 'ipk'

    def test_ipk_is_an_ar_archive_with_control_and_data_at_the_right_paths(self):
        f = self.out / 'UP-Term-nvim_2.1_m68k-amigaos.ipk'
        names = subprocess.run(['ar', 't', str(f)], capture_output=True, text=True, check=True).stdout.split()
        names = [n.rstrip('/') for n in names]
        self.assertEqual(names, ['debian-binary', 'control.tar.gz', 'data.tar.gz'])
        m = dict(split_ar(f.read_bytes()))
        self.assertEqual(m['debian-binary'], b'2.0\n')
        ctl = tarfile.open(fileobj=io.BytesIO(m['control.tar.gz']))
        self.assertEqual(sorted(ctl.getnames()), ['.', './control', './postinst', './prerm'])
        text = ctl.extractfile('./control').read().decode()
        for line in ('Package: UP-Term-nvim', 'Version: 2.1', 'Architecture: m68k-amigaos',
                     'Depends: UP-Term (>= 1.0)', 'Installed-Size: 3'):
            self.assertIn(line + '\n', text)
        post = ctl.extractfile('./postinst').read().decode()
        self.assertIn('Execute UP-Term:pkg/stage/UP-Term-nvim/Files/install.dos UP-Term:pkg/stage/UP-Term-nvim/Files\n', post)
        self.assertIn('Execute UP-Term:pkg/UP-Term-nvim/Uninstall\n', ctl.extractfile('./prerm').read().decode())
        data = tarfile.open(fileobj=io.BytesIO(m['data.tar.gz']))
        top = './pkg/stage/UP-Term-nvim'
        for n in (top + '/Uninstall', top + '/Files/install.dos', top + '/Files/sub/prog'):
            self.assertIn(n, data.getnames())
        self.assertEqual(data.getmember(top + '/Files/sub/prog').mode, 0o755)
        self.assertEqual(data.getmember(top + '/Uninstall').mode, 0o644)
        self.assertNotIn('Depends', gzip_control(self.out / 'Claude_1.1_m68k-amigaos.ipk'))

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


def gzip_control(f):
    m = dict(split_ar(f.read_bytes()))
    return tarfile.open(fileobj=io.BytesIO(m['control.tar.gz'])).extractfile('./control').read().decode()


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
