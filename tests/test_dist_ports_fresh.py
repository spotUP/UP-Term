"""make dist refuses a ports kit staged before the ports' last checked build.

The v1.2 feed build of 2026-10-20 shipped the old upkg: upterm-ports rebuilt opkg (its
build/state/opkg.checked moved) but nobody ran make kit-stage, and dist only tested that
build/kit/userland/SOURCES.txt existed. Checked here through make itself, on a temporary
PORTS_KIT and PORTS_STATE: dist-src (the cheapest dist target, no prerequisites) stops at the
guard when a .checked stamp is newer than SOURCES.txt; ports-kit-check passes once it is not.
"""
import os, pathlib, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def make(target, kit, state):
    return subprocess.run(['make', '-s', '-C', str(ROOT), target, 'PORTS_KIT=%s' % kit, 'PORTS_STATE=%s' % state],
                          capture_output=True, text=True)


class PortsKitFresh(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = pathlib.Path(self.tmp.name)
        self.kit, self.state = base / 'kit', base / 'state'
        self.kit.mkdir()
        self.state.mkdir()
        (self.kit / 'SOURCES.txt').write_text('opkg 0.9.0\n')
        self.stamp = self.state / 'opkg.checked'
        self.stamp.touch()

    def tearDown(self):
        self.tmp.cleanup()

    def age(self, path, seconds):
        t = path.stat().st_mtime - seconds
        os.utime(path, (t, t))

    def test_stale_kit_stops_dist(self):
        self.age(self.kit / 'SOURCES.txt', 3600)
        r = make('dist-src', self.kit, self.state)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('dist-src: %s is older than these checked builds' % self.kit, r.stdout)
        self.assertIn(str(self.stamp), r.stdout)

    def test_fresh_kit_passes(self):
        self.age(self.stamp, 3600)
        r = make('ports-kit-check', self.kit, self.state)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_missing_kit_stops(self):
        (self.kit / 'SOURCES.txt').unlink()
        r = make('ports-kit-check', self.kit, self.state)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('no %s/SOURCES.txt' % self.kit, r.stdout)


if __name__ == '__main__':
    unittest.main()
