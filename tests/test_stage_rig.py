#!/usr/bin/env python3
"""stage_rig on the host (no emulator): a rig copy that differs from the build is restaged and verified, a
copy that still differs after the put stops the script, and amarchy_rig.main stages before any row runs."""
import pathlib, sys, tempfile, unittest
from unittest import mock
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


class Default(unittest.TestCase):
    def test_default_set_stages_amarchy_gui(self):
        d = pathlib.Path(tempfile.mkdtemp())
        for n in stage_rig.DEFAULT:
            (d / n).write_bytes(n.encode())
        stage_rig.BUILD, rig = d, {}
        res = stage_rig.stage_build(build=False, dest="VTC:Apps/bin", read=rig.get,
                                    put=lambda l, r: rig.__setitem__(r, pathlib.Path(l).read_bytes()))
        self.assertEqual(res.get("amarchy-gui"), "restaged")
        self.assertEqual(rig["VTC:Apps/bin/amarchy-gui"], b"amarchy-gui")


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


class ClashRig(unittest.TestCase):
    def test_row_table_loads_and_stage_build_runs_first(self):
        import amarchy_clash_rig as m
        self.assertEqual([n for n, _ in m.ROWS],
                         ["setup", "swedish_table", "screen_m_n", "window_drag", "thirdparty_key", "disable", "cleanup"])
        rows = m.table()
        self.assertGreaterEqual(len(rows), 30)
        self.assertEqual([r[0] for r in rows if r[2] is None], [])     # every shipped chord has a Swedish raw code
        calls = []
        keep = list(m.ROWS)
        with mock.patch.object(m.stage_rig, "stage_build", lambda *a, **kw: calls.append("stage") or {}), \
                mock.patch.object(m, "OUT", pathlib.Path(tempfile.mkdtemp())), \
                mock.patch.object(sys, "argv", ["amarchy_clash_rig.py", "--fresh", "--no-build"]):
            m.ROWS[:] = [("probe", lambda: calls.append("row") or (True, "x"))]
            try:
                self.assertEqual(m.main(), 0)
            finally:
                m.ROWS[:] = keep
        self.assertEqual(calls, ["stage", "row"])


class RequireFresh(unittest.TestCase):
    def run_q(self, codes):
        def run(cmd, **kw):
            return mock.Mock(returncode=codes[cmd[-1]], stdout="", stderr="")
        return run

    def test_stale_target_is_named(self):
        run = self.run_q({"build/amiga/Amarchy": 1, "build/amiga/amarchy-gui": 0})
        with self.assertRaises(stage_rig.StageError) as e:
            stage_rig.require_fresh(("Amarchy", "amarchy-gui"), run)
        self.assertIn("[ERROR] stale: build/amiga/Amarchy", str(e.exception))
        self.assertNotIn("amarchy-gui", str(e.exception))

    def test_current_passes(self):
        stage_rig.require_fresh(("Amarchy",), self.run_q({"build/amiga/Amarchy": 0}))

    def test_make_error_is_not_stale(self):
        with self.assertRaises(stage_rig.StageError) as e:
            stage_rig.require_fresh(("Amarchy",), self.run_q({"build/amiga/Amarchy": 2}))
        self.assertNotIn("stale", str(e.exception))

    def test_setup_no_build_stops_before_any_copy(self):
        import amarchy_gui_rig as g
        with mock.patch.object(sys, "argv", ["x", "--setup", "--no-build"]), \
                mock.patch.object(g.stage_rig, "require_fresh", side_effect=stage_rig.StageError("[ERROR] stale: f")), \
                mock.patch.object(g.c, "run") as run, mock.patch.object(g.ami, "put") as put:
            with self.assertRaises(stage_rig.StageError):
                g.setup()
        run.assert_not_called()
        put.assert_not_called()

    # KIT-AMARCHY-FLAT: --setup stages the build's amarchy-flat and proves Amarchy's boot start loaded it
    def _setup_rig(self, flat_survives_break=False, status_word="on"):
        """A fake rig: Status lists the kit Amarchy and its amarchy-flat until Break; Run Amarchy brings both back."""
        procs = {2: "UP-Term:bin/Amarchy", 3: "UP-Term:bin/amarchy-flat BOOT"}
        cmds = []

        def run(cmd, timeout=None):
            cmds.append(cmd)
            if cmd == "Status":
                return 0, "".join("Process %d: %s\n" % kv for kv in sorted(procs.items()))
            if cmd.startswith("Break "):
                n = int(cmd.split()[1])
                if "amarchy-flat" not in procs.get(n, "") or not flat_survives_break:
                    procs.pop(n, None)
            if cmd.startswith("Run >NIL: VTCX:Apps/bin/Amarchy"):
                procs[4] = "VTCX:Apps/bin/Amarchy"
                if not any("amarchy-flat" in v for v in procs.values()):
                    procs[5] = "UP-Term:bin/amarchy-flat BOOT"
            if cmd.endswith("amarchy-flat STATUS"):
                return 0, "flat=%s calls=3 chained=0\n" % status_word
            return 0, ""
        return run, cmds

    def _run_setup(self, run):
        import amarchy_gui_rig as g
        g.bad = 0
        with mock.patch.object(sys, "argv", ["x", "--setup"]), mock.patch.object(g.stage_rig, "make") as make, \
                mock.patch.object(g.c, "run", side_effect=run), mock.patch.object(g.ami, "put") as put, \
                mock.patch.object(g.amarchy_window_rig, "port", return_value=("ok", "")), \
                mock.patch.object(g.time, "sleep"):
            ok = g.setup()
        return g, ok, make, put

    def test_setup_stages_and_proves_fresh_flat(self):
        run, cmds = self._setup_rig()
        g, ok, make, put = self._run_setup(run)
        self.assertIn("amarchy-flat", make.call_args[0][0])
        self.assertIn("build/amiga/amarchy-flat", stage_rig.TARGETS["amarchy-flat"])
        self.assertIn("VTCX:Apps/bin/amarchy-flat", [c.args[1] for c in put.call_args_list])
        self.assertIn("VTCX:Apps/bin/amarchy-flat STATUS", cmds)
        self.assertTrue(ok)
        self.assertEqual(g.bad, 0)

    def test_setup_fails_when_old_flat_stays_loaded(self):
        run, cmds = self._setup_rig(flat_survives_break=True)
        g, ok, make, put = self._run_setup(run)
        self.assertFalse(ok)
        self.assertEqual(g.bad, 1)
        put.assert_not_called()

    def test_setup_fails_when_flat_held(self):
        run, cmds = self._setup_rig(status_word="held")
        g, ok, make, put = self._run_setup(run)
        self.assertFalse(ok)


if __name__ == "__main__":
    unittest.main()
