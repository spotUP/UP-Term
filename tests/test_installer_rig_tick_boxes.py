"""installer_rig ticks only the Installer window's own checkboxes, never another window's buttons."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

# A --no-dopus rig: the plain Workbench window (scrollbar buttons 3..6) sits behind the Installer
# (rig 10, 2026-10-29: the tick-all pass read ids 3..6 from it and failed with KeyError: 5).
TREE = '''W 0 0 11 640x245 "Workbench"
G 3 620 30 18x10 button - ""
G 4 620 40 18x10 button - ""
G 5 600 245 18x10 button - ""
G 6 610 245 18x10 button - ""
W 1 152 60 496x210 active "Install UP-Term (0% done)"
G 2 170 100 26x11 toggle - "Base kit"
G 7 170 115 26x11 toggle selected "Directory Opus filetype"
G 8 170 130 26x11 toggle disabled "Apps"
G 90 281 250 238x16 button - "Proceed"
G 91 402 250 238x16 button - "Abort Install"
'''


class TickBoxes(unittest.TestCase):
    def test_only_the_installer_windows_unticked_boxes(self):
        import installer_rig as irg
        title, gads = irg.installer_page(TREE)
        self.assertEqual(title, "Install UP-Term (0% done)")
        self.assertEqual([gid for gid, _ in irg.unticked_boxes(gads)], [2])


if __name__ == "__main__":
    unittest.main()
