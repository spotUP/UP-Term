"""Which Installer page dumps the rig's layout check judges, on the host (no rig).

fresh-all failed "LAYOUT text in the bottom border strip (y 204..205) on [None]" (rig 9 2026-10-29): the
dump caught the last page half drawn, its buttons in the bottom strip before their gadgets were added.
"""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as irg

HALF = '''S 0 800x600 depth=24 "Workbench Screen"
W 0 152 195 496x210 active "VTC:distkit/Install (100% done) - Press 'Esc' to Abort"
G 0 625 195 24x14 sys:depth - ""
G 0 152 195 496x13 sys:drag - ""
W 2 0 14 800x586 - ""
'''
WHOLE = '''S 0 800x600 depth=24 "Workbench Screen"
W 0 152 195 496x210 active "VTC:distkit/Install (100% done)"
G 0 625 195 24x14 sys:depth - ""
G 0 152 195 496x13 sys:drag - ""
G 91 402 386 238x16 custom - ""
G 90 160 386 238x16 custom - ""
W 2 0 14 800x586 - ""
'''


class LayoutJudged(unittest.TestCase):
    def test_a_half_drawn_page_without_its_buttons_is_not_judged(self):
        self.assertFalse(irg.layout_judged(HALF, 'page'))

    def test_a_whole_page_with_proceed_and_abort_is_judged(self):
        self.assertTrue(irg.layout_judged(WHOLE, 'page'))

    def test_the_make_new_drawer_dialog_is_not_judged(self):
        self.assertFalse(irg.layout_judged(WHOLE, 'dialog'))


if __name__ == "__main__":
    unittest.main()
