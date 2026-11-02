"""gridfit_rig.anchor_rows: the anchor cell includes a top row too faint to be a full pen. Pure function, synthetic pixels."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import gridfit_rig  # noqa: E402

BG, WHITE, FAINT, EDGE = b"\0\0\0", b"\xff\xff\xff", b"\xaa\xaa\xaa", b"\x66\x88\xbb"


def column(pens, top=3):
    rows = {top + i: p for i, p in enumerate(pens)}
    return lambda x, y: rows.get(y, BG)


class AnchorRows(unittest.TestCase):
    def test_faint_top_row_is_in_the_cell(self):
        # rig 35 E0B0 left column: faint, six full pens, edge pen -> 8 rows
        at = column([FAINT] + [WHITE] * 6 + [EDGE])
        self.assertEqual(gridfit_rig.anchor_rows(at, BG, 0, 4), (3, 10))

    def test_full_top_row_unchanged(self):
        at = column([WHITE] * 8)
        self.assertEqual(gridfit_rig.anchor_rows(at, BG, 0, 3), (3, 10))


if __name__ == "__main__":
    unittest.main()
