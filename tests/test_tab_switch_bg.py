#!/usr/bin/env python3
"""A tab switch paints the theme background once, never pen 0 (owner, 2026-10-23: "when i click a tab
it first draws a white bg color and then black").

render/vtwin.c is Amiga-only, so the host checks its text. vtwin_show (the tab handed the window) and
vtwin_set_font must not EraseRect the text area: EraseRect fills with the RastPort's BgPen, pen 0, which
is white on a Workbench palette, and vr_redraw then fills the very same rectangle with the theme
background (vr_render.bg_ink). One fill, the theme's.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
V = (ROOT / "render/vtwin.c").read_text(encoding="latin-1")
R = (ROOT / "render/amiga_render.c").read_text(encoding="latin-1")


def body(src, head):
    i = src.index(head)
    return src[i:src.index("\n}\n", i)]


class TabSwitchBackground(unittest.TestCase):
    def test_no_erase_rect_in_vtwin(self):
        self.assertNotIn("EraseRect(", re.sub(r"/\*.*?\*/", "", V, flags=re.S))

    def test_show_redraws_the_text_area_through_the_renderer(self):
        self.assertIn("vr_redraw(&w->r);", body(V, "void vtwin_show(vtwin *w, int on)"))

    def test_vr_redraw_fills_the_text_area_with_the_theme_background(self):
        b = body(R, "void vr_redraw(vr_render *r)")
        self.assertRegex(b, r"fill\(r, w->BorderLeft, top, [^;]*r->bg_ink\);")


if __name__ == "__main__":
    unittest.main()
