"""amarchy-menu leaves no stale WBRESET client slot (source guard).

menu.c registers with the WBRESET table through wbr_report, only inside serve_one (the `serve` task, one slot for its
whole life). The table holds WBR_MAX clients, so a server that ends without wbr_client_drop leaves a dead entry behind,
and after WBR_MAX runs no new client can register. serve() has one end after the message loop (the early returns 20 and
5 come before any registration), so the drop must sit between the loop's DeleteMsgPort and its final `return 0;`.
"""
import pathlib, re, unittest

MENU = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "menu.c"


class MenuWbrDrop(unittest.TestCase):
    def test_serve_end_drops_the_client(self):
        src = MENU.read_text(errors="replace")
        m = re.search(r"^static int serve\(void\)\n.*?^    DeleteMsgPort\(port\);\n(.*?)^    return 0;\n\}", src, re.S | re.M)
        self.assertIsNotNone(m, "menu.c serve() has no final DeleteMsgPort ... return 0;")
        self.assertIn("wbr_client_drop()", m.group(1), "amarchy-menu serve exits without wbr_client_drop: stale table entry")


if __name__ == "__main__":
    unittest.main()
