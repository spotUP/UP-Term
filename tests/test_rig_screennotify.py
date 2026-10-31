#!/usr/bin/env python3
"""WB-RESET-SCREENNOTIFY: every rig start puts the pinned screennotify.library into VTC:ixp6, the first LIBS: drawer,
so the handler parks its Workbench window when IPrefs resets the screen."""
import pathlib, sys, tempfile, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class StageScreenNotify(unittest.TestCase):
    def test_the_library_lands_in_the_first_libs_drawer(self):
        self.assertTrue(rig.RIG_LIBS[0].endswith("LIBS: VTC:ixp6"))
        with tempfile.TemporaryDirectory() as d:
            vtc = pathlib.Path(d)
            rig.stage_screennotify(vtc)
            lib = vtc / "ixp6/screennotify.library"
            self.assertEqual(lib.read_bytes(),
                             (ROOT / "build/screennotify/ScreenNotify/libs/screennotify.library").read_bytes())
            self.assertEqual(lib.read_bytes()[:4], b"\x00\x00\x03\xf3")   # an AmigaDOS load file

    def test_every_start_stages_it(self):
        src = (ROOT / "tools/rig/rig.py").read_text()
        a = src.index('    stage_ixemul(RIG / "vtc", NOFPU)\n')
        self.assertIn('stage_screennotify(RIG / "vtc")', src[a:a + 200])

    def test_never_committed(self):
        import subprocess
        tracked = subprocess.run(["git", "ls-files"], cwd=ROOT, capture_output=True, text=True).stdout
        self.assertNotIn("screennotify.library", tracked)


if __name__ == "__main__":
    unittest.main()
