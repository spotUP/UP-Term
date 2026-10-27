"""amarchy_menu_rig refuses to run rows on a rig whose VTC: differs from the build (ledger MENU-RIG-STALE-GUARD).

Rig 4 printed "[WARN] N file(s) in VTC: differ from the build" and the menu rig ran its rows on the stale programs
anyway. The guard restages, asks rig.py's build_drift, and main() returns nonzero before the first row.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


class StaleGuard(unittest.TestCase):
    def test_current_vtc_passes_after_restaging(self):
        staged = []
        self.assertIsNone(m.stale_guard(lambda: staged.append(1), drift=lambda vtc: []))
        self.assertEqual(staged, [1])

    def test_drift_names_files_and_fix(self):
        err = m.stale_guard(lambda: None, drift=lambda vtc: ["Apps/bin/Amarchy", "ixemul.library"])
        self.assertIn("Apps/bin/Amarchy", err)
        self.assertIn("ixemul.library", err)
        self.assertIn("rig.py install", err)

    def test_main_runs_no_row_on_drift(self):
        ran = []
        with mock.patch.object(m, "stale_guard", return_value="[ERROR] stale"), \
             mock.patch.object(m, "ROWS", [("x", lambda: ran.append(1))]), \
             mock.patch.object(m, "run_row", side_effect=lambda f: ran.append(1) or (True, "")), \
             mock.patch.object(sys, "argv", ["amarchy_menu_rig.py", "--no-build"]), \
             mock.patch.object(m, "OUT", mock.MagicMock()):
            self.assertEqual(m.main(), 2)
        self.assertEqual(ran, [])


if __name__ == "__main__":
    unittest.main()
