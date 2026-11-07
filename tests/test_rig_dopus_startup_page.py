"""RIG-DOPUS-STARTUP-PAGE: `rig.py dopus` on the sys.hdf clone answers the Installer's "Modify Startup-Sequence File" page.

The Installer takes the file to edit from the boot volume, which is the rig's wrapper BOOTX:s/startup-sequence; Proceed on
it ended the Installer with "Problem with file Startup-Sequence in line 540" and no ;BEGIN DOpus5 block (3 of 3 runs, rig 94,
2026-11-07). The driver now types S:Startup-Sequence into the page's string gadget (92) before Proceed (90). The drawer page
(also has gadget 92, plus 89, 93, 94) still gets VTCX:.
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import dopus_rig as d

STARTUP = {g: ("h%d" % g, "") for g in (0, 1, 2, 90, 91, 92, 96, 97, 98, 99, 100)}
DRAWER = {g: ("h%d" % g, "") for g in (0, 89, 90, 91, 92, 93, 94, 96, 97, 98, 99, 100)}


class StartupPage(unittest.TestCase):
    def answer(self, gads):
        typed = []
        with mock.patch.object(d.inst, "type_into", lambda h, g, text: typed.append((g, text)) or True):
            return d.answer("", "Install DOpus5 (88% done)", gads), typed

    def test_startup_page_points_the_installer_at_the_real_startup_sequence(self):
        self.assertEqual(self.answer(STARTUP), ([90], [(92, "S:Startup-Sequence")]))

    def test_drawer_page_still_gets_vtcx(self):
        self.assertEqual(self.answer(DRAWER), ([90], [(92, "VTCX:")]))


if __name__ == "__main__":
    unittest.main()
