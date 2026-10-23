#!/usr/bin/env python3
"""The tab bar is drawn after a tab has shown itself (owner, 2026-10-23: "opening more than two tabs
makes the tab bar draw empty until i click it").

handler/vtcon_handler.c is Amiga-only, so the host checks its text. Handing the window to a tab that is
its own process is a handshake: tab_activate draws the bar at once, but the old tab's HIDE is answered
later, then the new tab gets SHOW and lays out and redraws its text area on its own task. (The first
tab added hands over from the host's own session, synchronously, and the bar was drawn last.) The
invariant: the bar is the last thing drawn after the tab's answers to SHOW and INSET, so tab_replies
draws it when such an answer arrives.
"""
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
H = (ROOT / "handler/vtcon_handler.c").read_text(encoding="latin-1")


def body(head):
    i = H.index(head)
    return H[i:H.index("\n}\n", i)]


class TabBarRedraw(unittest.TestCase):
    def test_replies_to_show_and_inset_redraw_the_bar(self):
        b = body("static void tab_replies(con *c)\n{")
        self.assertIn("TM_SHOW", b)
        self.assertIn("TM_INSET", b)
        self.assertRegex(b, r"if \(bar\)\s*bar_draw\(c\);")

    def test_the_bar_is_drawn_after_the_loop_not_before_the_reply(self):
        b = body("static void tab_replies(con *c)\n{")
        self.assertGreater(b.index("bar_draw(c)"), b.index("FreeVec(m);"))

    def test_show_and_inset_are_posted_with_a_reply_port(self):
        # tab_msg sets mn_ReplyPort: the answer reaches tab_replies
        self.assertIn("t->msg.mn_ReplyPort = c->tab_reply;", body("static struct tab_msg *tab_msg(con *c, int type)"))


if __name__ == "__main__":
    unittest.main()
