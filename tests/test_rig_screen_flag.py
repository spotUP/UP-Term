"""start --screen aga|aga256|rtg: the first boot is on that mode, no `rig.py aga` reboot mid-test (ST2 2026-11-27:
Super Type's rigtest switched rig 30 to AGA mid-test and the reboot froze it)."""
import inspect, pathlib, subprocess, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import rig


class ScreenFlag(unittest.TestCase):
    def test_written_before_the_clone(self):
        src = inspect.getsource(rig._start)
        self.assertLess(src.index("SCREENMODE_NAME.write_text(SCREEN)"), src.index("fresh_sys_hdf()"))

    def test_rig_aga_replaces_the_last_start_screen(self):
        src = inspect.getsource(rig.screenmode)
        self.assertIn('["--screen", name]', src)

    def test_bad_values_refused(self):
        for args in (["--screen", "pal"], ["--a500", "--screen", "rtg"]):
            r = subprocess.run([sys.executable, str(ROOT / "tools/rig/rig.py"), "status"] + args,
                               capture_output=True, text=True, timeout=60)
            self.assertNotEqual(r.returncode, 0, args)
            self.assertIn("--screen", r.stderr)


if __name__ == "__main__":
    unittest.main()
