#!/usr/bin/env python3
"""RIG-IPREFS-SYSREQ: a fresh rig boot raises no "Intuition is attempting to reset the Workbench screen" System Request,
and a start that still meets it fails after a bounded number of Retry clicks.
Old clone: the A1200 master's DEVS:Monitors/REPLAY (FPGA Replay P96 driver) failed on FS-UAE and printed to the boot
console before C:IPrefs; that opened the 'AmigaDOS' window on the PAL Workbench screen, so IPrefs' reset to the RTG
screenmode.prefs found a window open, and DOpus' backdrop and the Amarchy bar kept every Retry failing."""
import pathlib, sys, tempfile, types, unittest
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


class Bounded(unittest.TestCase):
    def test_retry_gives_up_and_names_the_requester(self):
        tree = "\n".join(['S 0 640x256 depth=2 "Workbench Screen"', 'W 0 0 0 496x82 active "System Request"',
                          'G 1 8 62 74x16 custom - "Retry"', 'G 0 404 62 84x16 custom - "Cancel"'])
        clicks = []
        fake = types.SimpleNamespace(
            req=lambda op: bytes([0, 0, 2, 128]) if op == 0x0A else tree.encode(),
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


if __name__ == "__main__":
    unittest.main()
