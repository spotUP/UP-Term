"""gridfit_rig icon mode: stem_verdict passes U+E0A0 (its left line is broken by the font design, rows 3-5 soft) and
fails an unsnapped stem (two columns at half coverage over every row). Pure function, no emulator."""
import pathlib, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))
import gridfit_rig  # noqa: E402

# U+E0A0 as measured on rig 35 (icons3.out): '#' full pen, '.' background, 'o' intermediate
E0A0 = ["##..o.", "##oo#o", "##..#.", "#o.oo.", "oooo..", "o#....", "##....", "##...."]
UNSNAPPED = ["..oo.."] * 8


class StemVerdict(unittest.TestCase):
    def test_e0a0_passes(self):
        self.assertEqual(gridfit_rig.stem_verdict(E0A0)[3], "SNAPPED")

    def test_unsnapped_stem_fails(self):
        self.assertEqual(gridfit_rig.stem_verdict(UNSNAPPED)[3], "SOFT")


if __name__ == "__main__":
    unittest.main()
