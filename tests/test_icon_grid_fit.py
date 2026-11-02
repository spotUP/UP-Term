#!/usr/bin/env python3
"""Grid fit of the tall (32x16) icon bake: tools/gen_menu_icons.fit_stems_y, reached through gen_lucide_icons.coverage
(the entry point of gen_lucide_icons.record). (a) a synthetic 1 target pixel bar at a half-pixel offset is 2 half-alpha rows
unfitted, 1 full-alpha row fitted; (b) the 32x32 variant is untouched by the fit; (c) over the real icon sets the number of
rows that have only partial alpha (best pixel between 25% and 75%) drops at 32x16.
Fonts are the pinned build/phosphor and build/lucide ones; without them (c) and (b) are skipped, REQUIRE=1 fails instead."""
import json
import os
import pathlib
import sys
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_emoji  # noqa: E402
import gen_lucide_icons as g  # noqa: E402
import gen_menu_icons as m  # noqa: E402
from fontTools.ttLib import TTFont  # noqa: E402

SETS = (("lucide-icons", "phosphor/Phosphor-Fill.ttf"), ("lucide-icons-outline", "lucide/lucide.ttf"))
HAVE = all((ROOT / "build" / t).exists() for _, t in SETS) or bool(os.environ.get("REQUIRE"))
ROWS = 16


def rows_of(polys, fit):
    """16 rows of 32 coverages of an outline, as coverage() makes them."""
    rw, rh, px = m.raster_polys(polys, ROWS if fit else None)
    out = gen_emoji.area(rw, rh, px, 32, ROWS)
    return [[out[y * 32 + x][3] / 255.0 for x in range(32)] for y in range(ROWS)]


def synthetic_bar():
    """A tall stem (sets the bounding box) and a horizontal bar 1 target row high whose bottom edge is at row 8.5."""
    span = 100.0
    scale = m.RES * (1 - 2 * m.EDGE) / span
    oy = (m.RES - span * scale) / 2
    unit = (m.RES / ROWS) / scale                                # one target row in font units
    ya = ((m.RES - 8.5 * m.RES / ROWS) - oy) / scale             # bottom edge, font units (y up)
    return [[(0, 0), (0, span), (10, span), (10, 0)], [(20, ya), (20, ya + unit), (100, ya + unit), (100, ya)]]


def partial_rows(cov):
    return sum(1 for r in cov if 0.25 <= max(r) <= 0.75)


class Synthetic(unittest.TestCase):
    def bar_rows(self, cov):
        return [max(r[8:30]) for r in cov if max(r[8:30]) > 0.05]

    def test_a_half_pixel_bar_is_one_full_row_when_fitted(self):
        glyph = synthetic_bar()
        before = self.bar_rows(rows_of(glyph, False))
        after = self.bar_rows(rows_of(glyph, True))
        print("synthetic bar rows (alpha), unfitted: %s  fitted: %s" % ([round(a, 2) for a in before], [round(a, 2) for a in after]))
        self.assertEqual(len(before), 2)
        self.assertTrue(all(0.3 < a < 0.7 for a in before))
        self.assertEqual(len(after), 1)
        self.assertGreaterEqual(after[0], 0.95)

    def test_x_is_untouched(self):
        glyph = [[(x, y) for x, y in poly] for poly in synthetic_bar()]
        q = [[(x * 0.5, 64 - y * 0.5) for x, y in poly] for poly in glyph]
        for a, b in zip(q, m.fit_stems_y(q, ROWS)):
            self.assertEqual([p[0] for p in a], [p[0] for p in b])


@unittest.skipUnless(HAVE, "make phosphor lucide first")
class RealFonts(unittest.TestCase):
    def icons(self):
        for table, ttf in SETS:
            font = TTFont(ROOT / "build" / ttf)
            cps = json.loads((ROOT / "build" / ttf).with_name("codepoints.json").read_text())
            names = sorted({n for _, n in g.parse_table(ROOT / "dist" / table, set(cps))})
            for n in names:
                yield font, cps[n]

    def test_32x32_is_byte_identical(self):
        for i, (font, cp) in enumerate(self.icons()):
            if i % 7:
                continue
            rw, rh, px = m.raster(font, cp)
            out = gen_emoji.area(rw, rh, px, 32, 32)
            want = [[out[y * 32 + x][3] / 255.0 for x in range(32)] for y in range(32)]
            self.assertEqual(g.coverage(font, cp, 32, 32), want)

    def test_partial_alpha_rows_drop_at_32x16(self):
        before = after = 0
        for font, cp in self.icons():
            rw, rh, px = m.raster(font, cp)
            out = gen_emoji.area(rw, rh, px, 32, ROWS)
            before += partial_rows([[out[y * 32 + x][3] / 255.0 for x in range(32)] for y in range(ROWS)])
            after += partial_rows(g.coverage(font, cp, 32, ROWS))
        print("rows at partial alpha (best pixel 0.25..0.75), 32x16: before %d, after %d" % (before, after))
        self.assertLess(after, before)


if __name__ == "__main__":
    unittest.main()
