#!/usr/bin/env python3
"""tools/gen_lucide_icons.py (plan 2026-11-01-amarchy-filled-icon-set.md, F4): table parse, unknown name fails, quantise
levels, the selected image's plate, planar round trip through mkicon.planes, the 32x16 variant, the lucide.dat layout.
The Phosphor Fill font is the pinned build/phosphor/ one (make phosphor); without it the font tests are skipped, REQUIRE=1 fails instead."""
import json
import os
import pathlib
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_lucide_icons as g  # noqa: E402

TTF = ROOT / "build" / "phosphor" / "Phosphor-Fill.ttf"
HAVE = TTF.exists() or bool(os.environ.get("REQUIRE"))
NAMES = {"folder", "hard-drive", "wrench"}
ENTRY = g.NAME_LEN + 4


def unplane(data, w, h, depth=g.DEPTH):
    """pen number rows of planar bytes (the inverse of mkicon.planes)."""
    words = w // 16
    pens = [[0] * w for _ in range(h)]
    for p in range(depth):
        for y in range(h):
            for i in range(words):
                (v,) = struct.unpack_from(">H", data, ((p * h + y) * words + i) * 2)
                for b in range(16):
                    pens[y][i * 16 + b] |= ((v >> (15 - b)) & 1) << p
    return pens


class Table(unittest.TestCase):
    def parse(self, text, names=NAMES):
        with tempfile.TemporaryDirectory() as d:
            p = pathlib.Path(d) / "t"
            p.write_text(text)
            return g.parse_table(p, names)

    def test_rows_comments_and_all_match_kinds(self):
        rows = self.parse("; c\ndef_drawer = folder ; tail\nSYS:Prefs/Font=wrench\ntype:disk = hard-drive\n")
        self.assertEqual(rows, [("def_drawer", "folder"), ("SYS:Prefs/Font", "wrench"), ("type:disk", "hard-drive")])

    def test_unknown_icon_name_fails(self):
        with self.assertRaises(SystemExit) as e:
            self.parse("def_drawer = no-such-icon\n")
        self.assertIn("no-such-icon", str(e.exception))

    def test_bad_line_and_duplicate_fail(self):
        for text in ("def_drawer folder\n", "def_a = folder\ndef_a = wrench\n", "= folder\n", ""):
            with self.assertRaises(SystemExit):
                self.parse(text)

    @unittest.skipUnless(HAVE, "make phosphor first")
    def test_shipped_table_parses_against_the_codepoint_file(self):
        cp = ROOT / "build" / "phosphor" / "codepoints.json"
        rows = g.parse_table(ROOT / "dist" / "lucide-icons", set(json.loads(cp.read_text())))
        got = {m for m, _ in rows}
        for need in ("def_disk", "def_drawer", "def_tool", "def_project", "def_trashcan", "def_RAM",
                     "type:disk", "type:drawer", "type:tool", "type:project", "type:garbage", "type:kick", "type:device"):
            self.assertIn(need, got)


SUBSTITUTES = {   # lucide name -> phosphor name for the 7 accepted substitutes (plan section 7); a silent swap shows here
    "cable": "plugs", "hard-drive-download": "hard-drives", "layout-dashboard": "squares-four", "memory-stick": "memory",
    "menu": "list", "monitor-cog": "monitor", "notebook-pen": "notebook"}


@unittest.skipUnless(HAVE, "make phosphor first")
class Pin(unittest.TestCase):
    CP = json.loads((ROOT / "build" / "phosphor" / "codepoints.json").read_text()) if (ROOT / "build" / "phosphor" / "codepoints.json").exists() else {}

    def test_every_table_name_resolves_and_the_set_is_the_accepted_one(self):
        rows = g.parse_table(ROOT / "dist" / "lucide-icons", set(self.CP))      # an unknown name aborts here
        names = {n for _, n in rows}
        self.assertEqual(len(rows), 81)
        self.assertEqual(len(names), 50)                                         # 51 Lucide names, monitor and monitor-cog share one glyph
        for sub in SUBSTITUTES.values():
            self.assertIn(sub, names)
        for must in ("gear-six", "vinyl-record", "text-aa", "terminal-window", "speaker-high", "file-xls", "film-strip"):
            self.assertIn(must, names)

    def test_codepoint_extraction_is_deterministic_and_complete(self):
        sys.path.insert(0, str(ROOT / "tools"))
        import phosphor_codepoints as pc
        css = (ROOT / "build" / "phosphor" / "style.css").read_text()
        a, b = pc.parse(css), pc.parse(css)
        self.assertEqual(json.dumps(a), json.dumps(b))
        self.assertEqual(len(a), 1530)
        self.assertEqual(a["gear-six"], 0xE272)
        self.assertEqual(a, self.CP)
        self.assertEqual(list(a), sorted(a))

    def test_extraction_rejects_a_stylesheet_without_rules(self):
        sys.path.insert(0, str(ROOT / "tools"))
        import phosphor_codepoints as pc
        with self.assertRaises(SystemExit):
            pc.parse(".ph.ph-x:before { content: \"\\e001\"; }")


class Levels(unittest.TestCase):
    def test_quantise_edges(self):
        self.assertEqual([g.level(c) for c in (0, 0.19, 0.2, 0.49, 0.5, 0.83, 0.84, 1.0)], [0, 0, 1, 1, 2, 2, 3, 3])


class Images(unittest.TestCase):
    def grid(self, w, h):
        cov = [[0.0] * w for _ in range(h)]
        cov[h // 2][w // 2] = 1.0
        cov[h // 2][w // 2 + 1] = 0.3
        return cov

    def test_normal_image_pens_and_transparent_ground(self):
        for w, h in g.VARIANTS:
            n, _ = g.images(self.grid(w, h), w, h)
            pens = unplane(n, w, h)
            self.assertEqual(pens[h // 2][w // 2], 33)
            self.assertEqual(pens[h // 2][w // 2 + 1], 32)
            self.assertEqual(sum(1 for r in pens for v in r if v != g.PEN_GROUND), 2)
            self.assertEqual(len(n), g.DEPTH * h * (w // 16) * 2)

    def test_selected_image_has_the_selbg_plate_and_selfg_glyph(self):
        for w, h in g.VARIANTS:
            _, s = g.images(self.grid(w, h), w, h)
            pens = unplane(s, w, h)
            self.assertEqual(pens[h // 2][w // 2], 34)
            self.assertEqual(pens[h // 2][w // 2 + 1], 42)
            self.assertEqual(pens[0][0], g.PEN_GROUND)                       # the plate is rounded: the corner stays transparent
            self.assertEqual(pens[h // 2][1], 43)
            self.assertEqual({v for r in pens for v in r}, {g.PEN_GROUND, 34, 42, 43})

    def test_every_emitted_pen_is_a_reserved_pen_or_transparent(self):
        for w, h in g.VARIANTS:
            n, s = g.images([[(x + y) % 11 / 10.0 for x in range(w)] for y in range(h)], w, h)
            for data in (n, s):
                self.assertTrue({v for r in unplane(data, w, h) for v in r} <= {0} | set(range(32, 48)))

    def test_both_variants_are_square_and_tall_pixel_sizes(self):
        self.assertEqual(g.VARIANTS, ((32, 32), (32, 16)))


@unittest.skipUnless(HAVE, "build/phosphor/Phosphor-Fill.ttf missing: make phosphor")
class Font(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        with tempfile.TemporaryDirectory() as d:
            t = pathlib.Path(d) / "t"
            t.write_text("def_drawer = folder\ndef_disk = hard-drive\nSYS:Prefs/Font = folder\n")
            cls.key, cls.icons, cls.recs, cls.font = g.build(t, TTF)
        cls.data = g.pack(cls.key, cls.icons, cls.recs)

    def test_dat_layout_and_index(self):
        d = self.data
        self.assertEqual(d[:4], b"LUCI")
        ver, count, depth, nvar, _ = struct.unpack_from(">HHHHL", d, 4)
        self.assertEqual((ver, count, depth, nvar), (1, 3, 6, 2))
        self.assertEqual(struct.unpack_from(">HHHH", d, 16), (32, 32, 32, 16))
        base = 24
        ents = {}
        for i in range(count):
            raw = d[base + i * ENTRY: base + i * ENTRY + g.NAME_LEN]
            ents[raw.rstrip(b"\0").decode()] = struct.unpack_from(">L", d, base + i * ENTRY + g.NAME_LEN)[0]
        self.assertEqual(sorted(ents), ["SYS:Prefs/Font", "def_disk", "def_drawer"])
        self.assertEqual(ents["SYS:Prefs/Font"], ents["def_drawer"])        # one record per icon, shared
        sq = g.DEPTH * 32 * 4
        rec = self.recs["folder"]
        self.assertEqual(len(rec), 2 * sq + 2 * (sq // 2))
        off = ents["def_drawer"]
        self.assertEqual(d[off:off + len(rec)], rec)
        self.assertEqual(len(d), base + count * ENTRY + sum(len(r) for r in self.recs.values()))

    def test_round_trip_both_variants_and_glyph_is_drawn(self):
        r = self.recs["folder"]
        pos = 0
        for w, h in g.VARIANTS:
            size = g.DEPTH * h * (w // 16) * 2
            n, s = r[pos:pos + size], r[pos + size:pos + 2 * size]
            pos += 2 * size
            pn, ps = unplane(n, w, h), unplane(s, w, h)
            self.assertEqual(g.mkicon.planes(["".join(chr(v) for v in row) for row in pn], g.DEPTH, g.PENS), n)
            self.assertTrue({33} <= {v for row in pn for v in row} <= {g.PEN_GROUND, 32, 37, 33})
            self.assertTrue({34, 43} <= {v for row in ps for v in row})
            self.assertGreater(sum(1 for row in pn for v in row if v), w * h // 16)
        self.assertEqual(pos, len(r))

    def test_cutouts_stay_colour_zero_no_second_tone(self):
        """owner 2026-11-01: two-tone none; the hole of the gear is the ground pen 44 (never pen 0, a black box) in the normal image (the backdrop shows through)."""
        cp = json.loads((TTF.parent / "codepoints.json").read_text())["gear-six"]
        for w, h in g.VARIANTS:
            n, _ = g.images(g.coverage(self.font, cp, w, h), w, h)
            pens = unplane(n, w, h)
            self.assertLessEqual({v for r in pens for v in r}, {g.PEN_GROUND, 32, 37, 33})
            ys = range(h)
            # the centre of the gear: a run of transparent pixels with glyph ink on its left and right (the hole)
            row = pens[h // 2]
            self.assertEqual(row[w // 2], g.PEN_GROUND)
            self.assertTrue(any(row[:w // 2]) and any(row[w // 2:]))

    def test_tall_variant_is_the_square_one_averaged_over_row_pairs(self):
        sq = g.coverage(self.font, json.loads((TTF.parent / "codepoints.json").read_text())["folder"], 32, 32)
        tall = g.coverage(self.font, json.loads((TTF.parent / "codepoints.json").read_text())["folder"], 32, 16)
        for y in range(16):
            for x in range(32):
                self.assertAlmostEqual(tall[y][x], (sq[2 * y][x] + sq[2 * y + 1][x]) / 2, delta=0.08)


LUCIDE_TTF = ROOT / "build" / "lucide" / "lucide.ttf"


@unittest.skipUnless(HAVE and (LUCIDE_TTF.exists() or os.environ.get("REQUIRE")), "make phosphor lucide first")
class TwoSets(unittest.TestCase):
    """One generator run emits both sets: Filled (Phosphor Fill, lucide.dat) and Outline (Lucide, outline.dat)."""

    def test_one_run_writes_both_dats_with_the_same_layout_and_different_glyphs(self):
        import subprocess
        with tempfile.TemporaryDirectory() as d:
            a, b = pathlib.Path(d) / "lucide.dat", pathlib.Path(d) / "outline.dat"
            subprocess.run([sys.executable, str(ROOT / "tools" / "gen_lucide_icons.py"),
                            str(ROOT / "dist" / "lucide-icons"), str(TTF), str(a),
                            str(ROOT / "dist" / "lucide-icons-outline"), str(LUCIDE_TTF), str(b)],
                           check=True, capture_output=True)
            fa, fb = a.read_bytes(), b.read_bytes()
        self.assertEqual(fa[:4], b"LUCI")
        self.assertEqual(fb[:4], b"LUCI")
        self.assertNotEqual(fa, fb)                                # the two sets differ
        self.assertGreater(len(fb), 20000)

    def test_a_partial_triple_is_refused(self):
        import subprocess
        r = subprocess.run([sys.executable, str(ROOT / "tools" / "gen_lucide_icons.py"), "a", "b"], capture_output=True)
        self.assertNotEqual(r.returncode, 0)


if __name__ == "__main__":
    unittest.main()
