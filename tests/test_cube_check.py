#!/usr/bin/env python3
"""tools/rig/cube_rig.check on the host, against drawn cubes (no emulator):
the cube in 8x16 cells (TopazPro 16, what the rig's RTG screen draws topaz
as) passes when every cell is the xterm palette -- the check stepped 8
pixels a row, read row 0 for every row and failed 204 of 240 right cells
("FAIL 36 of 240") -- and a wrong or missing cell still fails."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
from PIL import Image
import cube_rig
cube_rig.print = lambda *a, **k: None  # its ok/FAIL lines are for the rig log


def draw(cw, ch, x0=4, top=362, wrong=None, last=255):
    """A shot with the cube as tests/amiga/colors.sh draws it: 36 cells a
    row for 16-231, then the 24 greys two cells each, black around it."""
    im = Image.new('RGB', (800, 600), (0, 0, 0))
    for i in range(16, last + 1):
        row, col, n = ((i - 16) // 36, (i - 16) % 36, 1) if i < 232 else (6, (i - 232) * 2, 2)
        c = wrong[1] if wrong and wrong[0] == i else cube_rig.expect(i)
        im.paste(c, (x0 + col * cw, top + row * ch, x0 + (col + n) * cw, top + (row + 1) * ch))
    return im


class CubeCheck(unittest.TestCase):
    def test_the_cube_in_8x16_cells_passes(self):
        self.assertEqual(cube_rig.cells_wrong(draw(8, 16), 300, 600), [])
        self.assertEqual(cube_rig.check(draw(8, 16), 300, 600), 0)

    def test_the_cube_in_8x8_cells_passes(self):
        self.assertEqual(cube_rig.cells_wrong(draw(8, 8, top=40), 30, 560), [])

    def test_a_wrong_cell_in_8x16_cells_fails(self):
        bad = cube_rig.cells_wrong(draw(8, 16, wrong=(190, (238, 153, 0))), 300, 600)
        self.assertEqual([b[0] for b in bad], [190])

    def test_a_grey_ramp_still_being_drawn_fails(self):
        bad = cube_rig.cells_wrong(draw(8, 16, last=246), 300, 600)
        self.assertEqual([b[0] for b in bad], list(range(247, 256)))

    def test_no_cube_is_reported(self):
        self.assertIsNone(cube_rig.cells_wrong(Image.new('RGB', (800, 600)), 300, 600))
        self.assertEqual(cube_rig.check(Image.new('RGB', (800, 600)), 300, 600), 1)


    def test_shots_are_retaken_until_the_cube_is_drawn(self):
        """no cube yet, then a ramp still being drawn, then the whole cube:
        the first shot without a cube ended the wait (screen_rig failed at once)"""
        import tempfile
        from unittest import mock
        shots = [Image.new('RGB', (800, 600)), draw(8, 16, last=240), draw(8, 16)]
        taken = []
        def shot(a):
            taken.append(a); shots[min(len(taken), len(shots)) - 1].save(a[1])
        with mock.patch.object(cube_rig.ami, 'main', shot), mock.patch.object(cube_rig.time, 'sleep'), \
                tempfile.TemporaryDirectory() as d:
            self.assertEqual(cube_rig.shoot_check(pathlib.Path(d) / 'c.png', 300, 600, 60), 0)
        self.assertEqual(len(taken), 3)


if __name__ == '__main__':
    unittest.main()
