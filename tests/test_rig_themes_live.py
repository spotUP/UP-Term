#!/usr/bin/env python3
"""RIG-NO-THEMES-AGAIN: a rig started before `rig.py install` never ran the boot's theme copy (it runs only at boot), so
rig 8 (2026-11-01) had no UP-Term:Amarchy/Themes. Staging must also fill the live drawer UP-Term: points at."""
import io, contextlib, pathlib, sys, tempfile, unittest
from unittest import mock
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import paths, rig


class RigThemesLive(unittest.TestCase):
    def test_stage_fills_the_drawer_upterm_points_at_without_a_boot(self):
        tmp = pathlib.Path(tempfile.mkdtemp())
        root, vtc = tmp / "root", tmp / "vtc"
        (root / "dist/amarchy-themes").mkdir(parents=True)
        (root / "dist/amarchy-themes/nord.theme").write_text("name=nord\n")
        vtc.mkdir()
        with mock.patch.object(rig, "ROOT", root), contextlib.redirect_stdout(io.StringIO()):
            rig.stage_amarchy_assets(vtc)
        self.assertEqual((vtc / paths.UPTERM_DRAWER / "Amarchy/Themes/nord.theme").read_text(), "name=nord\n")
        self.assertEqual((vtc / "amarchy-themes/nord.theme").read_text(), "name=nord\n")


if __name__ == "__main__":
    unittest.main()
