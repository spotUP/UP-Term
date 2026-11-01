#!/usr/bin/env python3
"""tutorial_rig.Screen finds the tab's text lines by pixels. The tab is drawn at twice topaz 8's height for most of the
tutorial, but at lesson 12 step 5 the text sits at the plain 8 pixel pitch (s35 probe on rig 4, stable for 4 s). A
Screen that assumes 16 pixel lines reads the wrong pixel rows there, task_rows() never lists the short `up<pid>` row
and the drag picks "Return:" instead. word_span() must name the word at the end of the line above the prompts at BOTH
pitches. Synthetic framebuffer: every glyph row differs from its neighbours, as real topaz rows do."""
import pathlib, struct, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import tutorial_rig as t

SW, X0, Y0, COLS, H = 640, 4, 22, 77, 200
INK = b"\xff\xff\xff"


def glyph_ink(ch, r, x):
    return ch != " " and ((ord(ch) * 31 + r * 17 + x * 7 + (ord(ch) >> 2) * r) % 5) < 2


def screen(lines, pitch, cursor=True):
    """lines: text, top to bottom; the last is the prompt with its cursor block. pitch 16 doubles every glyph row."""
    b = bytearray(8 + SW * (Y0 + H) * 3)
    b[2:4] = struct.pack(">H", SW)
    scale = pitch // 8
    for n, text in enumerate(lines):
        top = Y0 + 2 + n * pitch
        last = n == len(lines) - 1
        for c, ch in enumerate(text + (" " if last else "")):
            cursor_cell = cursor and last and c == len(text)
            for r in range(8 if cursor_cell or ch in "pgy" else 7):   # row 7: the cursor block, or a descender
                for x in range(8):
                    if cursor_cell or (r == 7 and x in (3, 4)) or (r < 7 and glyph_ink(ch, r, x)):
                        for d in range(scale):
                            o = 8 + ((top + r * scale + d) * SW + X0 + c * 8 + x) * 3
                            b[o:o + 3] = INK
    s = object.__new__(t.Screen)
    s.x, s.y, s.w, s.h, s.sw, s.b = 0, 0, SW, Y0 + H, SW, bytes(b)
    s.x0, s.y0, s.cols = X0, Y0, COLS
    s.bg = b"\x00\x00\x00"
    s.line, s.gh = t.LINE, t.LINE - 4
    getattr(s, "set_pitch", lambda: None)()    # the constructor's last step
    return s


TEXT = ["Amiga C puts the selection on the clipboard, then paste it with Right Amiga V.",
        "Task: Drag the mouse over this word, press Right Amiga C, then press Return:",
        "up273825984",
        "tutorial> ", "tutorial> ", "tutorial>"]
WORD = "up273825984"


class ScreenPitch(unittest.TestCase):
    def check(self, pitch, cursor=True):
        s = screen(TEXT, pitch, cursor)
        self.assertEqual(s.line, pitch)
        y, px0, px1 = s.word_span(0)
        self.assertEqual((px0, px1), (X0, X0 + len(WORD) * 8))
        word_top = Y0 + 2 + 2 * pitch
        self.assertTrue(word_top <= y < word_top + pitch, (y, word_top))
        self.assertEqual(s.task_rows()[0][1], len(WORD))

    def test_plain_pitch(self):
        self.check(8)

    def test_double_pitch(self):
        self.check(16)

    def test_cursor_blink_does_not_shift_the_grid(self):
        for pitch in (8, 16):
            self.check(pitch, cursor=False)
            self.assertEqual(screen(TEXT, pitch, True).task_rows(), screen(TEXT, pitch, False).task_rows(), pitch)

    def test_reprompted_at_both_pitches(self):
        for pitch in (8, 16):
            self.assertTrue(screen(TEXT, pitch).reprompted(), pitch)
            self.assertFalse(screen(TEXT[:3] + ["tutorial>"], pitch).reprompted(), pitch)


if __name__ == "__main__":
    unittest.main()
