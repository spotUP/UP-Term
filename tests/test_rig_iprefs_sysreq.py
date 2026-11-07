#!/usr/bin/env python3
"""RIG-IPREFS-SYSREQ: a fresh rig boot raises no "Intuition is attempting to reset the Workbench screen" System Request,
and a start that still meets it fails after a bounded number of Retry clicks.
Old clone: the A1200 master's DEVS:Monitors/REPLAY (FPGA Replay P96 driver) failed on FS-UAE and printed to the boot
console before C:IPrefs; that opened the 'AmigaDOS' window on the PAL Workbench screen, so IPrefs' reset to the RTG
screenmode.prefs found a window open, and DOpus' backdrop and the Amarchy bar kept every Retry failing."""
import contextlib, io, pathlib, sys, tempfile, types, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

class NoBoardMonitors(unittest.TestCase):
    LISTING = ("Monitors   DIR  ----rwed  27.09.2026\n  PAL  2936  ----rwed\n  PAL.info  356  ----rwed\n"
               "  REPLAY  2720  ----rwed\n  REPLAY.info  526  ----rwed\n  uaegfx  1640  ----rwed\n"
               "  uaegfx.info  500  ----rw-d\n")

    def test_clone_drops_the_replay_driver_that_opens_the_boot_window(self):
        calls = []

        def run(argv, **kw):
            calls.append(argv[2:])
            return types.SimpleNamespace(stdout=self.LISTING if argv[2] == "list" else "", returncode=0)
        saved = rig.subprocess.run
        rig.subprocess.run = run
        try:
            rig.sys_hdf_monitors(pathlib.Path("sys.hdf"))
        finally:
            rig.subprocess.run = saved
        deleted = sorted(c[1] for c in calls if c[0] == "delete")
        self.assertEqual(deleted, ["Devs/Monitors/REPLAY", "Devs/Monitors/REPLAY.info"])   # PAL and uaegfx stay

    def test_the_clone_chain_drops_them(self):
        src = (ROOT / "tools/rig/rig.py").read_text()
        a = src.index("    elif not OS and not OS32:\n        fresh_sys_hdf()")
        self.assertIn("sys_hdf_monitors(RIG / \"sys.hdf\")", src[a:a + 900])


HEAD = "464f524d000000365052454650524844000000060000000000005343524d0000001c" + "00" * 16
AGA16 = bytes.fromhex(HEAD + "00029000" + "02800100" + "0400" + "0001")
AGA256 = bytes.fromhex(HEAD + "00029000" + "02800100" + "0800" + "0001")
RTG = bytes.fromhex(HEAD + "50041303" + "ffffffff" + "0018" + "0001")
RTG360 = bytes.fromhex(HEAD + "50041303" + "02800168" + "0018" + "0001")


class NativeScreenMode(unittest.TestCase):
    """W43-NATIVE-SCREENMODE: a saved chipset ScreenMode (--os, rig.py aga|aga256) has no RTG switch to wait for."""

    def test_display_id_and_nativeness(self):
        self.assertEqual(rig.screenmode_display_id(AGA16), 0x00029000)
        self.assertEqual(rig.screenmode_display_id(RTG), 0x50041303)
        self.assertIsNone(rig.screenmode_display_id(b"FORM"))
        self.assertTrue(rig.screenmode_is_native(AGA16) and rig.screenmode_is_native(AGA256))
        self.assertFalse(rig.screenmode_is_native(RTG) or rig.screenmode_is_native(RTG360))

    def test_os_rig_with_native_prefs_does_not_hunt_for_rtg(self):
        saved = (sys.modules.get("ami"), rig.RIG, rig.OS, rig.time.sleep)
        sys.modules["ami"] = types.SimpleNamespace(req=lambda op: self.fail("polled the screen"))
        rig.RIG = pathlib.Path(tempfile.mkdtemp())
        rig.OS = "32"
        rig.time.sleep = lambda s: None
        prefs = rig.RIG / "os-32/Prefs/Env-Archive/Sys/screenmode.prefs"
        prefs.parent.mkdir(parents=True)
        prefs.write_bytes(AGA256)
        out = io.StringIO()
        try:
            with contextlib.redirect_stdout(out):
                rig.retry_wb_reset()
        finally:
            log = (rig.RIG / "retry.log").read_text()
            sys.modules["ami"], rig.RIG, rig.OS, rig.time.sleep = saved
        self.assertIn("native", log)
        self.assertNotIn("W43", out.getvalue())


class Bounded(unittest.TestCase):
    def test_retry_gives_up_and_names_the_requester(self):
        tree = "\n".join(['S 0 640x256 depth=2 "Workbench Screen"', 'W 0 0 0 496x82 active "System Request"',
                          'G 1 8 62 74x16 custom - "Retry"', 'G 0 404 62 84x16 custom - "Cancel"'])
        clicks = []
        fake = types.SimpleNamespace(
            req=lambda op: bytes([1, 0, 2, 128, 1, 0, 2]) if op == 0x0A else tree.encode(),
            pointer_scale=lambda: (1, 1), script=lambda *a: clicks.append(a))
        saved = (sys.modules.get("ami"), rig.RIG, rig.time.sleep)
        sys.modules["ami"] = fake
        rig.RIG = pathlib.Path(tempfile.mkdtemp())
        rig.time.sleep = lambda s: None
        try:
            with self.assertRaises(SystemExit) as cm:
                rig.retry_wb_reset()
        finally:
            sys.modules["ami"], rig.RIG, rig.time.sleep = saved
        self.assertEqual(len(clicks), rig.WB_RESET_RETRIES)
        self.assertIn("System Request", str(cm.exception))

    def test_a_volume_request_is_not_clicked_and_is_named(self):
        # RIG-VOLREQ-MISREAD: an AssignWedge "Volume Request" offers Retry too; rig.py clicked it three times and
        # called it IPrefs' System Request. It must click nothing and name the requester it saw.
        tree = "\n".join(['S 0 640x256 depth=2 "Workbench Screen"', 'W 0 0 0 420x90 active "Volume Request"',
                          'G 1 8 70 60x16 custom - "Retry"', 'G 0 80 70 60x16 custom - "Assign"',
                          'G 0 160 70 60x16 custom - "Deny"', 'G 0 340 70 70x16 custom - "Cancel"'])
        fake = types.SimpleNamespace(
            req=lambda op: bytes([1, 0, 2, 128, 1, 0, 2]) if op == 0x0A else tree.encode(),
            pointer_scale=lambda: (1, 1), script=lambda *a: self.fail("clicked a Volume Request"))
        saved = (sys.modules.get("ami"), rig.RIG, rig.time.sleep)
        sys.modules["ami"] = fake
        rig.RIG = pathlib.Path(tempfile.mkdtemp())
        rig.time.sleep = lambda s: None
        try:
            with self.assertRaises(SystemExit) as cm:
                rig.retry_wb_reset()
        finally:
            sys.modules["ami"], rig.RIG, rig.time.sleep = saved
        self.assertIn('"Volume Request" [Retry|Assign|Deny|Cancel]', str(cm.exception))
        self.assertIn("RIG-VOLREQ-MISREAD", str(cm.exception))

    def test_a_640x360_rtg_workbench_is_not_pal(self):
        # W43 false alarm: the rtg360 screenmode (640x360 uaegfx) is 640 wide; it must count as RTG, so no Retry hunt
        # and no "[WARN] the Workbench stayed PAL" line
        now = [0.0]

        def clock():
            now[0] += 5
            return now[0]
        fake = types.SimpleNamespace(req=lambda op: bytes([1, 0, 2, 128, 1, 104, 24]) if op == 0x0A else b"",
                                     pointer_scale=lambda: (1, 1), script=lambda *a: self.fail("clicked"))
        saved = (sys.modules.get("ami"), rig.RIG, rig.time.sleep, rig.time.time)
        sys.modules["ami"] = fake
        rig.RIG = tmp = pathlib.Path(tempfile.mkdtemp())
        rig.time.sleep, rig.time.time = (lambda s: None), clock
        try:
            rig.retry_wb_reset()
        finally:
            sys.modules["ami"], rig.RIG, rig.time.sleep, rig.time.time = saved
        self.assertIn("RTG 640x360", (tmp / "retry.log").read_text())


if __name__ == "__main__":
    unittest.main()
