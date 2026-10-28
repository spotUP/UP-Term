"""installer_rig.drive answers Install-AmiSSL's own pages (no Proceed 90) instead of waiting 2700 s."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

# UITREE lines as recorded on rig 3 (build/rig3/installer/fullsuite.pages.txt), 2026-10-18
ASK = '''W 0 152 195 496x210 active "Install-AmiSSL (0% done)"
G 0 625 195 24x14 sys:depth - ""
G 0 152 195 496x13 sys:drag - ""
G 1 402 369 238x16 custom - ""
G 2 160 369 238x16 custom - ""
G 91 402 386 238x16 custom - ""
G 100 160 386 238x16 custom - ""
'''
DONE = '''W 0 152 195 496x210 active "Install-AmiSSL (100% done)"
G 0 152 195 496x13 sys:drag - ""
G 90 281 386 238x16 custom - ""
'''
WORK = '''W 0 152 195 496x210 active "Install-AmiSSL (50% done)"
G 0 152 195 496x13 sys:drag - ""
G 91 402 386 238x16 custom - ""
'''


class OwnAsk(unittest.TestCase):
    def answer(self, tree):
        import installer_rig as irg
        return irg.own_ask_answer(next(iter(irg.gadgets(tree).values())))

    def test_ask_page_is_answered_with_the_left_affirmative_button(self):
        self.assertEqual(self.answer(ASK), 2)

    def test_proceed_and_work_pages_are_not_ask_pages(self):
        self.assertIsNone(self.answer(DONE))
        self.assertIsNone(self.answer(WORK))


class RebootQuestion(unittest.TestCase):
    """P_END's "Reboot now?" must be answered Later, matched by its words and not by a gadget list."""
    def test_reboot_text_with_later_gadget_clicks_later(self):
        import installer_rig as irg
        self.assertEqual(irg.reboot_later({1: 0, 2: 0, 91: 0, 100: 0}, "Reboot now? UP-Term's handlers"), 1)
        self.assertEqual(irg.reboot_later({1: 0, 90: 0, 91: 0}, "Reboot now?"), 1)

    def test_reboot_text_without_a_later_button_is_never_clicked(self):
        import installer_rig as irg
        self.assertEqual(irg.reboot_later({90: 0, 91: 0}, "Reboot now?"), 0)
        self.assertEqual(irg.reboot_later({90: 0, 91: 0}, "Reboot now? Reboot | Later"), 91)

    def test_other_pages_are_not_reboot_pages(self):
        import installer_rig as irg
        self.assertIsNone(irg.reboot_later({90: 0, 91: 0}, "Proceed with the copy"))


if __name__ == "__main__":
    unittest.main()
