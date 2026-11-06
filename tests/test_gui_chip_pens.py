"""TE-COLOUR-LIST-ALL-SWATCHES source guard (evidence: thoughts/shared/research/s61-evidence/te-chip-pens.md).

amarchy/gui.c drew each colour chip (colour rows, and the boxes of the theme preview) with vr_pen_obtain, RectFill and an
immediate ReleasePen. A released pen is free for the next colour, so every chip drawn with the pen number showed the last
colour (rig 172, 3.2 Workbench, 256 colours: seven chips, one colour). The chips draw through the frame's held pens
(pen_cache, pc_of) and release them at the start of the next full redraw. Nothing in gui.c may obtain a pen for a chip and
release it in the same function; the only ReleasePen calls are pens_release and the held-pen release callback."""
import pathlib, re, unittest

GUI = pathlib.Path(__file__).resolve().parent.parent / "amarchy" / "gui.c"


def functions(text):
    """(name, body) of each top-level function: a line that starts in column 0 with a type and a name( up to the closing brace line."""
    out = []
    for m in re.finditer(r'^(?:static\s+)?[\w\s\*]+?\b(\w+)\s*\([^;{]*\)\s*\n\{\n(.*?)^\}\n', text, re.S | re.M):
        out.append((m.group(1), m.group(2)))
    return out


class ChipPens(unittest.TestCase):
    def test_no_obtain_then_release_in_one_function(self):
        fns = functions(GUI.read_text())
        self.assertIn('fill_rgb', [n for n, _ in fns])      # the parser finds the functions it must judge
        bad = [n for n, b in fns if 'vr_pen_obtain' in b and 'ReleasePen' in b]
        self.assertEqual([], bad, 'a pen is obtained and released in one function: %s' % bad)

    def test_chips_use_the_held_pens(self):
        text = GUI.read_text()
        self.assertEqual(2, len(re.findall(r'pc_of\(&g->(?:chip|prev)_pens', text)))
        self.assertEqual(2, len(re.findall(r'pc_theme_changed\(&g->(?:chip|prev)_pens', text)))


if __name__ == '__main__':
    unittest.main()
