"""TE-COLOUR-LIST-ALL-SWATCHES source guard (evidence: thoughts/shared/research/s61-evidence/te-chip-pens.md).

amarchy/gui.c drew each colour chip (colour rows, and the boxes of the theme preview) with vr_pen_obtain, RectFill and an
immediate ReleasePen. A released pen is free for the next colour, so every chip drawn with the pen number showed the last
colour (rig 172, 3.2 Workbench, 256 colours: seven chips, one colour). The chips draw through the frame's held pens
(pen_cache, pc_of) and release them at the start of each list draw (the frame). Nothing in gui.c may obtain a pen for a chip and
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

    def test_release_is_at_the_frame_start_in_draw_list(self):
        """A scroll calls draw_list only; 24 distinct chip colours with the release in draw_full alone fill the cache
        and later chips vanish (PC_MAX). The frame boundary is draw_list: release first, then rows and preview."""
        body = dict(functions(GUI.read_text()))
        dl = body['draw_list']
        rel = dl.find('chip_pens_release(g)')
        self.assertGreaterEqual(rel, 0, 'draw_list does not release the chip pens')
        self.assertLess(rel, dl.find('draw_row(g'), 'the release must come before the first row')
        self.assertLess(rel, dl.find('draw_preview(g)'), 'the release must come before the preview')
        self.assertLess(dl.find('paint_begin(g)'), rel)
        self.assertGreater(dl.find('if (g->await)'), -1)
        self.assertLess(dl.find('if (g->await)'), rel, 'nothing is repainted while awaiting: no release then')
        self.assertNotIn('chip_pens_release(g)', body['draw_full'], 'draw_full calls draw_list: one release per frame')
        self.assertIn('chip_pens_release(g)', body['pens_release'])

    def test_pens_are_only_taken_inside_a_list_draw(self):
        """pc_of on the held pens sits in draw_row (rows) and fill_rgb (preview); both are reached from draw_list, and a
        single-row redraw only repaints a row the last list draw already held a pen for."""
        fns = dict(functions(GUI.read_text()))
        users = sorted(n for n, b in fns.items() if re.search(r'pc_of\(&g->(?:chip|prev)_pens', b))
        self.assertEqual(['draw_row', 'fill_rgb'], users)


if __name__ == '__main__':
    unittest.main()
