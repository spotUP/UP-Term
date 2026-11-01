"""installer_rig.DrawerAsks counts the drawer page's arrivals, not its redraws: a destination typed through Make New
Drawer redraws the page, and counting that stopped the master build with "room page repeats" (2026-10-27)."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

DRAWER = {89: None, 90: None, 91: None, 92: None}
MESSAGE = {0: None, 90: None, 91: None}


class DrawerAsks(unittest.TestCase):
    def count(self, pages):
        import installer_rig as irg
        d = irg.DrawerAsks()
        return [d.see(g) for g in pages][-1]

    def test_a_redraw_after_typing_is_the_same_ask(self):
        self.assertEqual(self.count([MESSAGE, DRAWER, DRAWER, DRAWER, MESSAGE]), 1)

    def test_the_page_coming_back_after_another_page_is_a_new_ask(self):
        self.assertEqual(self.count([DRAWER, DRAWER, MESSAGE, DRAWER]), 2)


if __name__ == "__main__":
    unittest.main()
