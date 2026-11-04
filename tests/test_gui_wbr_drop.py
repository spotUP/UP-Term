"""amarchy-gui leaves no stale WBRESET client slot (source guard).

gui.c registers with the WBRESET table through wbr_report (first at window open). The table holds WBR_MAX clients, so a gui
that exits without wbr_client_drop leaves a dead entry behind, and after WBR_MAX runs no new client can register. Every
exit after the registration goes through the `done:` label (normal end, window-open failure, out of memory), so the drop
must sit between that label and its `return rc;`.
"""
import pathlib, re, unittest

GUI = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "gui.c"


class GuiWbrDrop(unittest.TestCase):
    def test_done_block_drops_the_client(self):
        src = GUI.read_text(errors="replace")
        m = re.search(r"^done:\n(.*?)^\s*return rc;", src, re.S | re.M)
        self.assertIsNotNone(m, "gui.c has no done: block ending in return rc;")
        self.assertIn("wbr_client_drop()", m.group(1), "amarchy-gui exits without wbr_client_drop: stale table entry")


if __name__ == "__main__":
    unittest.main()
