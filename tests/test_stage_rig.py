#!/usr/bin/env python3
"""stage_rig on the host (no emulator): a rig copy that differs from the build is restaged and verified, a
copy that still differs after the put stops the script, and patina_rig.main stages before any row runs."""
import pathlib, sys, tempfile, unittest
from unittest import mock
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import stage_rig


class Stage(unittest.TestCase):
    def setUp(self):
        d = pathlib.Path(tempfile.mkdtemp())
        (d / "Patina").write_bytes(b"new build")
        stage_rig.BUILD = d
        self.rig, self.puts = {}, []

    def read(self, p):
        return self.rig.get(p)

    def put(self, local, remote):
        self.puts.append(remote)
        self.rig[remote] = pathlib.Path(local).read_bytes()

    def stage(self, **kw):
        return stage_rig.stage_build(("Patina",), build=False, dest="VTC:Apps/bin", read=self.read, put=self.put, **kw)

    def test_stale_copy_is_restaged(self):
        self.rig["VTC:Apps/bin/Patina"] = b"hand copied earlier"
        self.assertEqual(self.stage(), {"Patina": "restaged"})
        self.assertEqual(self.rig["VTC:Apps/bin/Patina"], b"new build")

    def test_missing_copy_is_staged(self):
        self.assertEqual(self.stage(), {"Patina": "restaged"})

    def test_current_copy_is_left(self):
        self.rig["VTC:Apps/bin/Patina"] = b"new build"
        self.assertEqual(self.stage(), {"Patina": "current"})
        self.assertEqual(self.puts, [])

    def test_a_put_that_does_not_land_fails_loudly(self):
        self.rig["VTC:Apps/bin/Patina"] = b"stale"
        self.put = lambda local, remote: None
        with self.assertRaises(stage_rig.StageError):
            self.stage()


class Default(unittest.TestCase):
    def test_default_set_stages_patina_gui(self):
        d = pathlib.Path(tempfile.mkdtemp())
        for n in stage_rig.DEFAULT:
            (d / n).write_bytes(n.encode())
        stage_rig.BUILD, rig = d, {}
        res = stage_rig.stage_build(build=False, dest="VTC:Apps/bin", read=rig.get,
                                    put=lambda l, r: rig.__setitem__(r, pathlib.Path(l).read_bytes()))
        self.assertEqual(res.get("patina-gui"), "restaged")
        self.assertEqual(rig["VTC:Apps/bin/patina-gui"], b"patina-gui")


class Wired(unittest.TestCase):
    def test_patina_rig_stages_before_any_row(self):
        import patina_rig
        calls = []
        patina_rig.stage_rig.stage_build = lambda **kw: calls.append("stage") or {}
        patina_rig.ROWS[:] = [("probe", lambda: calls.append("row") or (True, "x"))]
        patina_rig.OUT = pathlib.Path(tempfile.mkdtemp())
        sys.argv = ["patina_rig.py", "--fresh", "--no-build"]
        self.assertEqual(patina_rig.main(), 0)
        self.assertEqual(calls, ["stage", "row"])


class ClashRig(unittest.TestCase):
    def test_row_table_loads_and_stage_build_runs_first(self):
        import patina_clash_rig as m
        self.assertEqual([n for n, _ in m.ROWS],
                         ["setup", "swedish_table", "screen_m_n", "window_drag", "thirdparty_key", "disable", "cleanup"])
        rows = m.table()
        self.assertGreaterEqual(len(rows), 30)
        self.assertEqual([r[0] for r in rows if r[2] is None], [])     # every shipped chord has a Swedish raw code
        calls = []
        keep = list(m.ROWS)
        with mock.patch.object(m.stage_rig, "stage_build", lambda *a, **kw: calls.append("stage") or {}), \
                mock.patch.object(m, "OUT", pathlib.Path(tempfile.mkdtemp())), \
                mock.patch.object(sys, "argv", ["patina_clash_rig.py", "--fresh", "--no-build"]):
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
        run = self.run_q({"build/amiga/Patina": 1, "build/amiga/patina-gui": 0})
        with self.assertRaises(stage_rig.StageError) as e:
            stage_rig.require_fresh(("Patina", "patina-gui"), run)
        self.assertIn("[ERROR] stale: build/amiga/Patina", str(e.exception))
        self.assertNotIn("patina-gui", str(e.exception))

    def test_helper_maps_rc_0_1_2(self):
        import paths
        seen = []
        run = self.run_q({"a": 0, "b": 1, "c": 2})
        self.assertEqual(paths.stale_targets(["a", "b", "c"], run), ["b"])
        self.assertEqual(paths.stale_targets(["a", "b", "c"], run, on_error=lambda t, r: seen.append(t)), ["b"])
        self.assertEqual(seen, ["c"])

    def test_current_passes(self):
        stage_rig.require_fresh(("Patina",), self.run_q({"build/amiga/Patina": 0}))

    def test_make_error_is_not_stale(self):
        with self.assertRaises(stage_rig.StageError) as e:
            stage_rig.require_fresh(("Patina",), self.run_q({"build/amiga/Patina": 2}))
        self.assertNotIn("stale", str(e.exception))

    def test_setup_no_build_stops_before_any_copy(self):
        import patina_gui_rig as g
        with mock.patch.object(sys, "argv", ["x", "--setup", "--no-build"]), \
                mock.patch.object(g.stage_rig, "require_fresh", side_effect=stage_rig.StageError("[ERROR] stale: f")), \
                mock.patch.object(g.c, "run") as run, mock.patch.object(g.ami, "put") as put:
            with self.assertRaises(stage_rig.StageError):
                g.setup()
        run.assert_not_called()
        put.assert_not_called()

    # KIT-PATINA-FLAT: --setup stages the build's patina-flat and proves Patina's boot start loaded it
    def _setup_rig(self, flat_survives_break=False, status_word="on", flat_pref="on"):
        """A fake rig: Status lists the kit Patina and its patina-flat until Break; Run Patina brings Patina back
        (no patina-flat: Patina does not load it at start); the kit's boot line loads patina-flat only while
        ENV:Patina/prefs turns flat on (patina-flat BOOT)."""
        procs = {2: "UP-Term:bin/Patina", 3: "UP-Term:bin/patina-flat BOOT"}
        cmds = []

        def run(cmd, timeout=None):
            cmds.append(cmd)
            if cmd == "Status":
                return 0, "".join("Process %d: %s\n" % kv for kv in sorted(procs.items()))
            if cmd.startswith("Break "):
                n = int(cmd.split()[1])
                if "patina-flat" not in procs.get(n, "") or not flat_survives_break:
                    procs.pop(n, None)
            if cmd.startswith("Run >NIL: VTCX:Apps/bin/Patina"):
                procs[4] = "VTCX:Apps/bin/Patina"
            if cmd == "Run >NIL: UP-Term:bin/patina-flat BOOT":
                if flat_pref == "on" and not any("patina-flat" in v for v in procs.values()):
                    procs[5] = "UP-Term:bin/patina-flat BOOT"
            if cmd.startswith("Type ENV:Patina/prefs"):
                return 0, "; Patina preferences.\n[profile default]\nflat-system-look = %s\n" % flat_pref
            if cmd.endswith("patina-flat STATUS"):
                if not any("patina-flat" in v for v in procs.values()):
                    return 0, "flat=absent\n"
                return 0, "flat=%s calls=3 chained=0\n" % status_word
            return 0, ""
        return run, cmds

    def _run_setup(self, run):
        import patina_gui_rig as g
        g.bad = 0
        with mock.patch.object(sys, "argv", ["x", "--setup"]), mock.patch.object(g.stage_rig, "make") as make, \
                mock.patch.object(g.c, "run", side_effect=run), mock.patch.object(g.ami, "put") as put, \
                mock.patch.object(g.patina_window_rig, "port", return_value=("ok", "")), \
                mock.patch.object(g.time, "sleep"):
            ok = g.setup()
        return g, ok, make, put

    def test_setup_stages_and_proves_fresh_flat(self):
        run, cmds = self._setup_rig()
        g, ok, make, put = self._run_setup(run)
        self.assertIn("patina-flat", make.call_args[0][0])
        self.assertIn("build/amiga/patina-flat", stage_rig.TARGETS["patina-flat"])
        self.assertIn("VTCX:Apps/bin/patina-flat", [c.args[1] for c in put.call_args_list])
        self.assertIn("VTCX:Apps/bin/patina-flat STATUS", cmds)
        self.assertIn("Run >NIL: UP-Term:bin/patina-flat BOOT", cmds)
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

    def test_setup_passes_with_flat_prefs_off_and_no_resident(self):
        run, cmds = self._setup_rig(flat_pref="off")
        g, ok, make, put = self._run_setup(run)
        self.assertIn("VTCX:Apps/bin/patina-flat STATUS", cmds)
        self.assertTrue(ok)
        self.assertEqual(g.bad, 0)


if __name__ == "__main__":
    unittest.main()
