#!/usr/bin/env python3
"""stage_rig on the host (no emulator): a rig copy that differs from the build is restaged and verified, a
copy that still differs after the put stops the script, and amarchy_rig.main stages before any row runs."""
import pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import stage_rig


class Stage(unittest.TestCase):
    def setUp(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "Amarchy").write_bytes(b"new build")
        stage_rig.BUILD = d
        self.rig, self.puts = {}, []

    def read(self, p):
        return self.rig.get(p)

    def put(self, local, remote):
        self.puts.append(remote)
        self.rig[remote] = pathlib.Path(local).read_bytes()

    def stage(self, **kw):
        return stage_rig.stage_build(("Amarchy",), build=False, dest="VTC:Apps/bin", read=self.read, put=self.put, **kw)

    def test_stale_copy_is_restaged(self):
        self.rig["VTC:Apps/bin/Amarchy"] = b"hand copied earlier"
        self.assertEqual(self.stage(), {"Amarchy": "restaged"})
        self.assertEqual(self.rig["VTC:Apps/bin/Amarchy"], b"new build")

    def test_missing_copy_is_staged(self):
        self.assertEqual(self.stage(), {"Amarchy": "restaged"})

    def test_current_copy_is_left(self):
        self.rig["VTC:Apps/bin/Amarchy"] = b"new build"
        self.assertEqual(self.stage(), {"Amarchy": "current"})
        self.assertEqual(self.puts, [])

    def test_a_put_that_does_not_land_fails_loudly(self):
        self.rig["VTC:Apps/bin/Amarchy"] = b"stale"
        self.put = lambda local, remote: None
        with self.assertRaises(stage_rig.StageError):
            self.stage()


class Wired(unittest.TestCase):
    def test_amarchy_rig_stages_before_any_row(self):
        import amarchy_rig
        calls = []
        amarchy_rig.stage_rig.stage_build = lambda **kw: calls.append("stage") or {}
        amarchy_rig.ROWS[:] = [("probe", lambda: calls.append("row") or (True, "x"))]
        amarchy_rig.OUT = pathlib.Path(tempfile.mkdtemp())
        sys.argv = ["amarchy_rig.py", "--fresh", "--no-build"]
        self.assertEqual(amarchy_rig.main(), 0)
        self.assertEqual(calls, ["stage", "row"])


if __name__ == "__main__":
    unittest.main()
