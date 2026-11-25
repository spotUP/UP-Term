#!/usr/bin/env python3
"""RIG-LATE-REQ: retry_wb_reset stopped looking 20 s after the Workbench turned RTG, so a System Request raised later
on the RTG 'Directory Opus' screen (IPrefs, ~boot+65 s) was never seen and start returned 0. It now watches until
LATE_REQ_WATCH s after the switch, fails the start on one, and keeps the window list in paths.REPORTS."""
import pathlib, sys, tempfile, types, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

RTG = bytes([1, 0, 0x03, 0x20, 0x02, 0x58, 24])   # 800x600
SYSREQ = "\n".join(['S 0 800x600 depth=24 "Directory Opus"', 'W 0 150 250 496x82 active "System Request"',
                    'G 1 158 312 74x16 custom - "Retry"', 'G 0 554 312 84x16 custom - "Cancel"'])


def run(tree_at, screen=lambda t: RTG):
    """retry_wb_reset on screen(t) (default RTG) with a fake clock (+5 s a call); tree_at(t) is the UITREE text at time t."""
    now = [0.0]

    def clock():
        now[0] += 5
        return now[0]
    fake = types.SimpleNamespace(req=lambda op: screen(now[0]) if op == 0x0A else tree_at(now[0]).encode(),
                                 pointer_scale=lambda: (1, 1), script=lambda *a: (_ for _ in ()).throw(AssertionError("clicked")))
    saved = (sys.modules.get("ami"), rig.RIG, rig.paths.REPORTS, rig.time.sleep, rig.time.time)
    sys.modules["ami"] = fake
    rig.RIG = pathlib.Path(tempfile.mkdtemp())
    rig.paths.REPORTS = rig.RIG / "reports"
    rig.time.sleep, rig.time.time = (lambda s: None), clock
    try:
        rig.retry_wb_reset()
        return None, rig.RIG, rig.paths.REPORTS, now[0]
    except SystemExit as e:
        return str(e), rig.RIG, rig.paths.REPORTS, now[0]
    finally:
        sys.modules["ami"], rig.RIG, rig.paths.REPORTS, rig.time.sleep, rig.time.time = saved


class LateSysReq(unittest.TestCase):
    def test_a_system_request_60s_after_rtg_fails_the_start_and_is_reported(self):
        err, rigdir, reports, _ = run(lambda t: SYSREQ if t > 60 else "")
        self.assertIn("RIG-LATE-REQ", err or "")
        self.assertIn('"System Request"', err)
        self.assertIn("late System Request", (rigdir / "retry.log").read_text())
        self.assertIn('"Directory Opus"', (reports / "late-sysreq.log").read_text())

    def test_a_quiet_rtg_boot_returns_after_the_watch(self):
        err, rigdir, reports, end = run(lambda t: "")
        self.assertIsNone(err)
        self.assertGreater(end, rig.LATE_REQ_WATCH)
        self.assertIn("RTG 800x600", (rigdir / "retry.log").read_text())
        self.assertFalse((reports / "late-sysreq.log").exists())


NOSCREEN = bytes([0])   # ami.py _screens with no screen open: 0 rows, read as 0x0


class NoScreen(unittest.TestCase):
    def test_no_screen_is_not_rtg_and_fails_after_the_wait(self):
        err, rigdir, _, end = run(lambda t: "", screen=lambda t: NOSCREEN)
        self.assertIn("no screen open after %d s" % rig.WB_SCREEN_WAIT, err or "")
        self.assertGreaterEqual(end, rig.WB_SCREEN_WAIT)
        self.assertNotIn("RTG", (rigdir / "retry.log").read_text())

    def test_screen_opening_late_inside_the_wait_is_waited_for(self):
        err, rigdir, _, end = run(lambda t: "", screen=lambda t: RTG if t > 90 else NOSCREEN)
        self.assertIsNone(err)
        self.assertGreater(end, 90 + rig.LATE_REQ_WATCH)
        self.assertIn("RTG 800x600", (rigdir / "retry.log").read_text())


if __name__ == "__main__":
    unittest.main()
