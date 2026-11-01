"""BAR-FONT-MISMATCH: the bar measures (text_w, TextLength on the screen RastPort) and sizes (bar_height of the screen
font) in the screen font, so tw_open must draw the window in that font: a new window RastPort starts in topaz 8
(6 empty rows under the text on the A1200, helvetica 13). Source pin of the glue; the rig check is in
thoughts/shared/research/s28-evidence/bar-font.md."""
import pathlib, re, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent


class BarFont(unittest.TestCase):
    def test_tw_open_sets_window_font_to_screen_font(self):
        s = (ROOT / "amarchy/themewin.c").read_text()
        body = s[s.index("struct Window *tw_open"):s.index("void tw_close")]
        self.assertRegex(body, r"SetFont\(win->RPort,\s*scr->RastPort\.Font\)")
        self.assertLess(body.index("OpenWindowTags"), body.index("SetFont(win->RPort"))

    def test_bar_height_from_core(self):
        s = (ROOT / "amarchy/bar.c").read_text()
        self.assertIn("bar_height(", s)
        self.assertNotRegex(s, r"tf_YSize\s*:\s*8\)\s*\+\s*2")


if __name__ == "__main__":
    unittest.main()
