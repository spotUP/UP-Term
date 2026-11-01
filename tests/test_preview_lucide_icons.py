#!/usr/bin/env python3
"""tools/preview_lucide_icons.py: the decode inverts the generator's planar packer, the pens map to the theme colours, the sheet
is written. Uses a synthetic lucide.dat (no font needed)."""
import pathlib
import struct
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import gen_lucide_icons as g  # noqa: E402
import mkicon  # noqa: E402
import preview_lucide_icons as p  # noqa: E402


def fake_dat():
    recs = b""
    for w, h in g.VARIANTS:
        art = ["".join(chr(32 if (x + y) % 2 else 0) for x in range(w)) for y in range(h)]
        pl = mkicon.planes(art, g.DEPTH, g.PENS)
        recs += pl + pl
    head = 16 + 4 * len(g.VARIANTS) + (g.NAME_LEN + 4)
    out = b"LUCI" + struct.pack(">HHHHL", g.VERSION, 1, g.DEPTH, len(g.VARIANTS), 0)
    for w, h in g.VARIANTS:
        out += struct.pack(">HH", w, h)
    out += b"def_x".ljust(g.NAME_LEN, b"\0") + struct.pack(">L", head)
    return out + recs


class Preview(unittest.TestCase):
    def test_decode_and_sheet(self):
        data = fake_dat()
        var, idx = p.parse(data)
        self.assertEqual(var, [(32, 32), (32, 16)])
        self.assertEqual(idx[0][0], "def_x")
        pens = p.unplane(data, idx[0][1], 32, 32)
        self.assertEqual(pens[0][:3], [0, 32, 0])
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d) / "o.png"
            self.assertEqual(p.sheet(data, str(out))[:2], (1, 1))
            self.assertGreater(out.stat().st_size, 100)

    def test_pens(self):
        pal = p.pen_colours()
        self.assertEqual(pal[0], p.rgb(p.BACKDROP))
        self.assertEqual(pal[32 + p.SLOT_FG100], p.rgb(p.FG))


if __name__ == "__main__":
    unittest.main()
