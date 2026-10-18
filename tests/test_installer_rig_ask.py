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


if __name__ == "__main__":
    unittest.main()
