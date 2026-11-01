"""make dist refuses a ports kit staged before the ports' last checked build.

The v1.2 feed build of 2026-10-20 shipped the old upkg: upterm-ports rebuilt opkg (its
build/state/opkg.checked moved) but nobody ran make kit-stage, and dist only tested that
build/kit/userland/SOURCES.txt existed. Checked here through make itself, on a temporary
PORTS_KIT and PORTS_STATE: dist-src (the cheapest dist target, no prerequisites) stops at the
guard when a .checked stamp is newer than SOURCES.txt; ports-kit-check passes once it is not.
"""
import os, pathlib, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]


def make(target, kit, state, ports):
    return subprocess.run(['make', '-s', '-C', str(ROOT), target, 'PORTS_KIT=%s' % kit, 'PORTS_STATE=%s' % state,
                           'UPTERM_PORTS=%s' % ports], capture_output=True, text=True)


def git(repo, *args):
    return subprocess.run(['git', '-C', str(repo), '-c', 'user.name=t', '-c', 'user.email=t@t', *args],
                          capture_output=True, text=True, check=True).stdout.strip()


class PortsKitFresh(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        base = pathlib.Path(self.tmp.name)
        self.kit, self.state = base / 'kit', base / 'state'
        self.ports = base / 'ports'
        self.ports.mkdir()
        git(self.ports, 'init', '-q', '-b', 'main')
        git(self.ports, 'commit', '-q', '--allow-empty', '-m', 'one')
        git(self.ports, 'update-ref', 'refs/remotes/origin/main', 'HEAD')
        self.head = git(self.ports, 'rev-parse', 'HEAD')
        self.kit.mkdir()
        self.state.mkdir()
        (self.kit / 'SOURCES.txt').write_text('opkg 0.9.0\n')
        self.stage(self.head)
        self.stamp = self.state / 'opkg.checked'
        self.stamp.touch()

    def tearDown(self):
        self.tmp.cleanup()

    def stage(self, text):
        (self.kit / 'STAGED-FROM').write_text(text + '\n')

    def age(self, path, seconds):
        t = path.stat().st_mtime - seconds
        os.utime(path, (t, t))

    def test_stale_kit_stops_dist(self):
        self.age(self.kit / 'SOURCES.txt', 3600)
        r = make('dist-src', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('dist-src: %s is older than these checked builds' % self.kit, r.stdout)
        self.assertIn(str(self.stamp), r.stdout)

    def test_fresh_kit_passes(self):
        self.age(self.stamp, 3600)
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertEqual(r.returncode, 0, r.stdout + r.stderr)

    def test_missing_kit_stops(self):
        (self.kit / 'SOURCES.txt').unlink()
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('no %s/SOURCES.txt' % self.kit, r.stdout)

    def test_kit_without_stamp_stops(self):
        (self.kit / 'STAGED-FROM').unlink()
        self.age(self.stamp, 3600)
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('staged before the stamp', r.stdout)

    def test_stamp_of_another_commit_stops(self):
        self.age(self.stamp, 3600)
        git(self.ports, 'commit', '-q', '--allow-empty', '-m', 'two')
        git(self.ports, 'update-ref', 'refs/remotes/origin/main', 'HEAD')
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('staged from %s but' % self.head, r.stdout)
        self.assertIn(git(self.ports, 'rev-parse', 'HEAD'), r.stdout)

    def test_dirty_stamp_stops(self):
        self.age(self.stamp, 3600)
        self.stage(self.head + '-dirty')
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('%s-dirty' % self.head, r.stdout)
        self.assertIn('uncommitted', r.stdout)

    def test_commit_off_origin_main_stops(self):
        self.age(self.stamp, 3600)
        git(self.ports, 'commit', '-q', '--allow-empty', '-m', 'branch only')
        self.stage(git(self.ports, 'rev-parse', 'HEAD'))
        r = make('ports-kit-check', self.kit, self.state, self.ports)
        self.assertNotEqual(r.returncode, 0)
        self.assertIn('not on the ports origin/main', r.stdout)


if __name__ == '__main__':
    unittest.main()


class GuardBeforeWipe(unittest.TestCase):
    """dist-base checked the ports kit only after rm -rf $(KIT): a plain make dist without the kit
    stopped at the guard with build/dist/up-term already deleted (2026-10-28)."""

    def test_guard_runs_before_the_kit_is_wiped(self):
        mk = (ROOT / 'Makefile').read_text()
        recipe = mk[mk.index('\ndist-base:'):]
        recipe = recipe[:recipe.index('\n\n')]
        guard = recipe.index('$(call PORTS_KIT_FRESH,dist)')
        self.assertLess(guard, recipe.index('rm -rf $(KIT)'))
