#!/usr/bin/env python3
"""RIG-IPREFS-SYSREQ: a rig boot never opens a window while IPrefs resets the Workbench screen, and a start that
still meets the "close all windows" System Request fails after a bounded number of Retry clicks.
Old boot: S:Startup-Sequence ran User-Startup (Amarchy bar) and C:LoadWB (Directory Opus backdrop) straight after
C:IPrefs; Intuition's reset found them and asked, and the start clicked Retry for ever."""
import pathlib, sys, tempfile, types, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig

STOCK = """C:AddDataTypes REFRESH QUIET

IF "`C:Avail TOTAL`" GE "1250000" VAL
  C:LoadResource LIBS:diskfont.library
  C:IPrefs
  IF NOT $Language EQ "english"
    CD SYS:
  EndIF
Else
  C:IPrefs
EndIF

C:ConClip
IF EXISTS S:User-Startup
  Execute S:User-Startup
EndIF
C:LoadWB
EndCLI >NIL:
"""


class IPrefsGate(unittest.TestCase):
    def test_every_iprefs_is_followed_by_the_wait_before_user_startup(self):
        out = rig.iprefs_gate_startup(STOCK).split("\n")
        idx = [i for i, l in enumerate(out) if l.strip() == "C:IPrefs"]
        self.assertEqual(len(idx), 2)
        for i in idx:
            self.assertEqual(out[i + 1].strip(), "C:IPrefsWait")
        self.assertLess(max(idx) + 1, out.index("  Execute S:User-Startup"))
        self.assertLess(max(idx) + 1, out.index("C:LoadWB"))
        self.assertEqual(out[idx[0] + 1][:2], "  ")   # keeps the IF block's indent

    def test_no_iprefs_line_is_refused(self):
        with self.assertRaises(ValueError):
            rig.iprefs_gate_startup("C:LoadWB\n")

    def test_the_clone_chain_runs_the_gate(self):
        src = (ROOT / "tools/rig/rig.py").read_text()
        a = src.index("    elif not OS and not OS32:\n        fresh_sys_hdf()")
        self.assertIn("sys_hdf_iprefs_wait(RIG / \"sys.hdf\")", src[a:a + 900])


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
