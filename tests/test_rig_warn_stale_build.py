#!/usr/bin/env python3
"""rig.py start rebuilds stale build/amiga programs (ensure_built) instead of only warning; make rc 2 warns. make faked."""
import pathlib, shutil, subprocess, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class Stale(unittest.TestCase):
    def setUp(self):
        self.t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, self.t, ignore_errors=True)
        (self.t / "build/amiga").mkdir(parents=True)
        self.bin = self.t / "build/amiga/foo"
        self.bin.write_bytes(b"x")
        p = mock.patch.object(rig, "ROOT", self.t)
        p.start()
        self.addCleanup(p.stop)
        self.calls = []

    def fake(self, rc):
        def run(cmd, **kw):
            self.calls.append(cmd)
            # the grouped `make -q` and the per-target one answer rc; the build itself (no -q) succeeds
            return subprocess.CompletedProcess(cmd, rc if "-q" in cmd else 0, "", "boom")
        return run

    def test_stale_is_rebuilt(self):
        with mock.patch("builtins.print"):
            self.assertEqual(rig.ensure_built([self.bin], self.fake(1)), ["build/amiga/foo"])
        built = [c for c in self.calls if "buildq" in c[0]]
        self.assertEqual([c[1:] for c in built][0][-3:], ["--", "make", "build/amiga/foo"])
        self.assertNotIn(["make", "build/amiga/foo"], self.calls)  # never bare

    def test_current_is_not_built(self):
        self.assertEqual(rig.ensure_built([self.bin], self.fake(0)), [])
        self.assertFalse([c for c in self.calls if "buildq" in c[0]])

    def test_rc2_warns_not_silent(self):
        with mock.patch("builtins.print") as pr:
            self.assertEqual(rig.ensure_built([self.bin], self.fake(2)), [])
        self.assertTrue(pr.call_args[0][0].startswith("[WARN] make -q failed"), pr.call_args)

    def test_setup_builds_instead_of_warning(self):
        self.assertFalse(hasattr(rig, "warn_stale_build"))
        self.assertIn("ensure_built([src for src, dst in program_pairs(RIG", pathlib.Path(rig.__file__).read_text())


if __name__ == "__main__":
    unittest.main()
