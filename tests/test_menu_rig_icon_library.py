"""app_icons needs icon.library V44 (icon_gfx.c ig_open). On the default KS 3.1 rig (icon.library 40) the
drawn menu keeps the generic glyph and ENV:AMARCHY_ICON_LOADS stays 0, which showed as 3 of 10 failed checks
that looked like a drawing bug (and were blamed on commit 51e0abfd). The row now names the missing library."""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import amarchy_menu_rig as m


class IconLibrary(unittest.TestCase):
    def major(self, rc, out):
        with mock.patch.object(m.c, "run", return_value=(rc, out)):
            return m.icon_library_major()

    def test_versions(self):
        self.assertEqual(self.major(0, "icon.library 40.1\n"), 40)
        self.assertEqual(self.major(0, "icon.library 47.4\n"), 47)
        self.assertEqual(self.major(10, ""), 0)

    def test_row_stops_below_v44(self):
        with mock.patch.object(m, "ensure_terminal"), mock.patch.object(m.c, "run", return_value=(0, "icon.library 40.1")):
            ok, msg = m.r_app_icons()
        self.assertFalse(ok)
        self.assertIn("V44", msg)


if __name__ == "__main__":
    unittest.main()
