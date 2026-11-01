"""userland_rig reboots a hung rig with a --purpose window title.

On 2026-10-25 the htop agent saw userland_rig restart a hung rig with
`rig.py start --max` and no --purpose: the new FS-UAE window carried no
session or task, against the rule that every rig window says whose it is.
"""
import pathlib, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import userland_rig


class RestartPurpose(unittest.TestCase):
    def test_the_hang_reboot_passes_the_purpose(self):
        calls = []
        clock = iter(range(0, 10000, 100))
        m, sp, tm = userland_rig, userland_rig.subprocess, userland_rig.time
        saved = (m.run, m.VTC, m.PURPOSE, sp.run, tm.time, tm.sleep)
        with tempfile.TemporaryDirectory() as d:
            try:
                m.VTC = pathlib.Path(d)
                for sub in ("out/p", "userland/p"):
                    (m.VTC / sub).mkdir(parents=True)
                m.run = lambda *a, **k: None
                m.PURPOSE = "s1: task"
                sp.run = lambda argv, *a, **k: calls.append(argv)
                tm.time = lambda: next(clock)
                tm.sleep = lambda _: None
                verdict = m.run_watched("p", "case", limit=1)
            finally:
                m.run, m.VTC, m.PURPOSE, sp.run, tm.time, tm.sleep = saved
        self.assertTrue(verdict.startswith("HANG"))
        start = [c for c in calls if "start" in c]
        self.assertEqual(len(start), 1)
        self.assertIn("--purpose", start[0], "the rebooted rig window has no purpose title")
        self.assertEqual(start[0][start[0].index("--purpose") + 1], "s1: task")


if __name__ == "__main__":
    unittest.main()
