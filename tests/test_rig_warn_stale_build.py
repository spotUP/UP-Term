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
            # the one `make -n` pass answers as make would for rc 0 current, 1 stale, 2 no rule; the build itself succeeds
            if "-n" not in cmd:
                return subprocess.CompletedProcess(cmd, 0, "", "")
            out = {0: "", 1: "Must remake target `build/amiga/foo'.\n", 2: ""}[rc]
            err = "make: *** No rule to make target `build/amiga/foo'.  Stop.\n" if rc == 2 else ""
            return subprocess.CompletedProcess(cmd, 2 if rc == 2 else 0, out, err)
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
        self.assertTrue(pr.call_args[0][0].startswith("[WARN] make -n failed"), pr.call_args)

    def test_current_costs_one_make_q(self):
        bins = [self.bin] + [self.t / ("build/amiga/p%03d" % i) for i in range(166)]
        for b in bins[1:]:
            b.write_bytes(b"x")
        rig.ensure_built(bins, self.fake(0))
        self.assertEqual(len(self.calls), 1)

    def test_setup_builds_instead_of_warning(self):
        self.assertFalse(hasattr(rig, "warn_stale_build"))
        self.assertIn("ensure_built([src for src, dst in program_pairs(RIG", pathlib.Path(rig.__file__).read_text())


class OnePass(unittest.TestCase):
    """RIG-BLACK-SCREEN: one make parse for all targets, and parallel starts serialize on build/rig-build.lock."""
    def test_one_make_call_for_many_targets(self):
        import paths
        calls = []
        def run(cmd, **kw):
            calls.append(cmd)
            return subprocess.CompletedProcess(cmd, 2, "Must remake target `b'.\nMust remake target 'x.o'.\n",
                                               "make: *** No rule to make target `c'.\nFailed to remake target file `c'.\n")
        seen = []
        self.assertEqual(paths.stale_targets(["a", "b", "c"], run, on_error=lambda t, r: seen.append(t)), ["b"])
        self.assertEqual((len(calls), seen), (1, ["c"]))

    def test_missing_prerequisite_is_an_error_for_its_target(self):
        import paths
        seen = []   # GNU make -n -k names the target only in "needed by" / "Target ... not remade"
        run = lambda cmd, **kw: subprocess.CompletedProcess(cmd, 2, "Must remake target `b'.\n",
            "make: *** No rule to make target `/x/ixpipe-handler.c', needed by `h'.\nTarget `h' not remade because of errors.\n")
        self.assertEqual(paths.stale_targets(["h", "b"], run, on_error=lambda t, r: seen.append(t)), ["b"])
        self.assertEqual(seen, ["h"])

    def test_make_error_without_target_lines_is_an_error_for_all(self):
        import paths
        seen = []
        run = lambda cmd, **kw: subprocess.CompletedProcess(cmd, 2, "", "Makefile:1: *** missing separator.  Stop.")
        self.assertEqual(paths.stale_targets(["a", "b"], run, on_error=lambda t, r: seen.append(t)), [])
        self.assertEqual(seen, ["a", "b"])

    def test_build_lock_is_exclusive(self):
        import fcntl, paths
        t = pathlib.Path(tempfile.mkdtemp())
        self.addCleanup(shutil.rmtree, t, ignore_errors=True)
        with paths.build_lock(t):
            with open(t / "build/rig-build.lock") as f:
                with self.assertRaises(OSError):
                    fcntl.flock(f, fcntl.LOCK_EX | fcntl.LOCK_NB)


if __name__ == "__main__":
    unittest.main()
