#!/usr/bin/env python3
"""B7, the one reachability test of tools/gen_dopus_toolbar.py: the generator's own command line (what `make
dopus-toolbar` runs) turns the md5-checked upstream Buttons/Toolbar of DOpus5_104_os3.lha and dist/dopus-toolbar.txt
into Images/<name> and Buttons/Amarchy-toolbar. Asserts: every image path the bank names exists in the drawer, each
image decodes as ILBM at the measured upstream size (its set's size) with depth 3 and has glyph pixels (the sentinel: the
generator ran), the Borderless flag is set, and the bank differs from upstream only in the image paths and that flag.
DT-1: size sets 16, 24, 32 x square and tall pixel, depth 3, one bank per set.
DT-8: glyphs come from the Lucide SVG source on the pixel grid (tools/svg_raster.py): the share of partial pixels
(levels 1-2 over all glyph pixels) of every set's images stays under its ceiling (font-outline baseline measured
2026-11-08: 16 0.56, 16t 0.66, 24 0.34, 24t 0.47, 32 0.36, 32t 0.43; SVG path 0.35, 0.35, 0.20, 0.31, 0.20, 0.31), and
a straight horizontal or vertical stroke is full level only, its round caps aside.
DT-9: two families, "outline-<size>" from Lucide and "filled-<size>" from Phosphor Fill (the default), twelve sets and
banks; the filled share measured 2026-11-08: 16 0.10, 16t 0.15, 24 0.08, 24t 0.12, 32 0.07, 32t 0.09. The default bank
checked byte for byte against upstream is DOPUS_TOOLBAR_SET (amarchy/menu_theme.h), filled-24.
Run: python3 -I tests/test_dopus_toolbar.py   (skips when the vendored archive, build/lucide or build/phosphor is absent)"""
import hashlib, pathlib, re, struct, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools' / 'rig'))
import ilbm_decode, paths, svg_raster, gen_lucide_icons

ARCHIVE = pathlib.Path(paths.VENDOR) / 'dopus5-5.104' / 'DOpus5_104_os3.lha'
MD5 = '0800b7b3bf02c83bd45a84d038ee483e'
TGZ = ROOT / 'build' / 'lucide' / 'lucide-static-1.66.0.tgz'
TTF = ROOT / 'build' / 'phosphor' / 'Phosphor-Fill.ttf'
TABLE = ROOT / 'dist' / 'dopus-toolbar.txt'
CEILING = {'outline-16': 0.40, 'outline-16t': 0.40, 'outline-24': 0.25, 'outline-24t': 0.36, 'outline-32': 0.25, 'outline-32t': 0.36,
           'filled-16': 0.15, 'filled-16t': 0.20, 'filled-24': 0.12, 'filled-24t': 0.17, 'filled-32': 0.12, 'filled-32t': 0.14}
DEFAULT = re.search(r'#define DOPUS_TOOLBAR_SET "([^"]+)"', (ROOT / 'amarchy' / 'menu_theme.h').read_text()).group(1)
FLAGS_OFF = 12 + 8 + 128   # FORM header, BTNW chunk header, CFG_BTNW flags (dopus5.h)


def chunks(buf):
    pos, out = 12, []
    while pos < len(buf):
        tag, size = buf[pos:pos + 4], struct.unpack('>L', buf[pos + 4:pos + 8])[0]
        out.append((tag, buf[pos + 8:pos + 8 + size]))
        pos += 8 + size + (size & 1)
    return out


@unittest.skipUnless(ARCHIVE.exists() and TGZ.exists() and TTF.exists(),
                     'vendored DOpus archive, build/lucide or build/phosphor missing (make lucide phosphor)')
class DopusToolbar(unittest.TestCase):
    def test_generated_bank_is_complete_and_flat(self):
        self.assertEqual(hashlib.md5(ARCHIVE.read_bytes()).hexdigest(), MD5)
        with tempfile.TemporaryDirectory() as t:
            t = pathlib.Path(t)
            subprocess.run(['lha', '-xqw=' + str(t / 'up'), str(ARCHIVE), 'DOpus5/Buttons/Toolbar'], check=True)
            up = (t / 'up' / 'DOpus5' / 'Buttons' / 'Toolbar').read_bytes()
            subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'gen_dopus_toolbar.py'), str(TABLE), str(TGZ), str(TTF),
                            str(t / 'up' / 'DOpus5' / 'Buttons' / 'Toolbar'), str(t / 'out')], check=True)
            self.assertEqual(DEFAULT, 'filled-24')
            bank = (t / 'out' / 'Buttons' / ('Amarchy-toolbar-' + DEFAULT)).read_bytes()
            drawer = t / 'out' / 'Images' / DEFAULT

            # every image the bank names exists, decodes at the upstream size and carries glyph pixels
            paths_in_bank = set()
            for tag, data in chunks(bank):
                if tag == b'BUTN':
                    paths_in_bank |= set(re.findall(rb'dopus5:images/[^\0]+', data, re.I))
            self.assertEqual(len(paths_in_bank), 22)
            for p in paths_in_bank:
                self.assertTrue(p.lower().startswith(('dopus5:images/amarchy/%s/' % DEFAULT).encode()), p)
                f = drawer / p.decode().split('/')[-1]
                self.assertTrue(f.exists(), f)
                w, h, depth, _, cmap, rows = ilbm_decode.decode(f.read_bytes())
                self.assertEqual((w, h), (24, 24))
                self.assertEqual(depth, 3)
                self.assertEqual(len(cmap), 3 * 8)
                self.assertGreater(sum(1 for r in rows for v in r if v), 20, f)   # not blank
                self.assertTrue(any(v == 3 for r in rows for v in r), f)   # levels map 1:1 to pens: full glyph is pen 3
                self.assertTrue(set(v for r in rows for v in r) <= {0, 1, 2, 3}, f)   # no accent pen in the glyph levels

            # DT-1 / DT-9: every size set of both families exists, square and tall pixel (half the height), one bank per
            # set, bank paths name the set, and nothing else is written
            want = [(f + '-' + s, w, h) for f in ('outline', 'filled')
                    for s, w, h in [('16', 16, 16), ('16t', 16, 8), ('24', 24, 24), ('24t', 24, 12), ('32', 32, 32), ('32t', 32, 16)]]
            self.assertEqual(sorted(p.name for p in (t / 'out' / 'Images').iterdir()), sorted(n for n, _, _ in want))
            self.assertEqual(sorted(p.name for p in (t / 'out' / 'Buttons').iterdir()), sorted('Amarchy-toolbar-' + n for n, _, _ in want))
            for name, w, h in want:
                b = (t / 'out' / 'Buttons' / ('Amarchy-toolbar-' + name)).read_bytes()
                self.assertIn(('dopus5:images/Amarchy/%s/' % name).encode(), b)
                w2, h2, d2, _, _, rows2 = ilbm_decode.decode((t / 'out' / 'Images' / name / 'Directory').read_bytes() if (t / 'out' / 'Images' / name / 'Directory').exists() else next((t / 'out' / 'Images' / name).iterdir()).read_bytes())
                self.assertEqual((w2, h2, d2), (w, h, 3), name)
                self.assertTrue(any(v for r in rows2 for v in r), name)
                # DT-8: partial-pixel share of the whole set under its ceiling (the font path is over every one)
                part = tot = 0
                for f in (t / 'out' / 'Images' / name).iterdir():
                    lv = [v for r in ilbm_decode.decode(f.read_bytes())[5] for v in r]
                    tot += sum(1 for v in lv if v)
                    part += sum(1 for v in lv if v in (1, 2))
                self.assertLess(part / tot, CEILING[name], name)

            # Borderless Buttons set; nothing else of the BTNW chunk changed
            self.assertEqual(struct.unpack('>L', up[FLAGS_OFF:FLAGS_OFF + 4])[0] & 0x10, 0)
            self.assertEqual(struct.unpack('>L', bank[FLAGS_OFF:FLAGS_OFF + 4])[0], struct.unpack('>L', up[FLAGS_OFF:FLAGS_OFF + 4])[0] | 0x10)
            self.assertEqual(struct.unpack('>L', bank[4:8])[0], len(bank) - 8)
            # same chunks in the same order, same payload once the drawer and the flag are put back
            uc, bc = chunks(up), chunks(bank)
            self.assertEqual([c[0] for c in uc], [c[0] for c in bc])
            for (tag, ud), (_, bd) in zip(uc, bc):
                if tag == b'BTNW':
                    ud = ud[:128] + struct.pack('>L', struct.unpack('>L', ud[128:132])[0] | 0x10) + ud[132:]
                self.assertEqual(bd.replace(('dopus5:images/Amarchy/%s/' % DEFAULT).encode(), b'dopus5:images/'), ud)


class StraightStrokes(unittest.TestCase):
    """DT-8, where it lives (svg_raster.coverage at the generator's STROKE_PX): a horizontal and a vertical Lucide
    stroke land on whole pixels at every set. Inside the stroke (between the cap pixels) every pixel is level 3; a
    level 1-2 pixel exists only in the cap pixel at each end; nothing outside the stroke's rows (or columns) is inked."""
    def check(self, svg, w, h, pen, horizontal):
        lv = [[gen_lucide_icons.level(c) for c in r] for r in svg_raster.coverage(svg, w, h, pen)]
        if not horizontal:   # transpose: the vertical stroke read as a horizontal one
            lv = [list(c) for c in zip(*lv)]
        inked = [i for i, r in enumerate(lv) if any(r)]
        cols = [x for x in range(len(lv[0])) if any(r[x] for r in lv)]
        for i in inked:
            self.assertEqual(lv[i][cols[0] + 1:cols[-1]], [3] * (cols[-1] - cols[0] - 1), (w, h, horizontal, lv[i]))
        return len(inked)

    def test_straight_strokes_full_level(self):
        import gen_dopus_toolbar as gd
        for name, fam, w, h in gd.sets():
            if fam != 'outline':   # only the Lucide family is stroked
                continue
            pen = gd.STROKE_PX[w]
            rows = self.check('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor"><path d="M4 12h16"/></svg>', w, h, pen, True)
            cols = self.check('<svg viewBox="0 0 24 24" fill="none" stroke="currentColor"><path d="M12 4v16"/></svg>', w, h, pen, False)
            self.assertEqual(cols, pen, name)                                   # whole columns, the pen's width
            self.assertEqual(rows, pen if h == w else max(1, pen * h // w), name)   # whole rows (tall: half, at least 1)


class InstallScript(unittest.TestCase):
    """dist/addons/dopus/install.dos carries one installer body: one CHECKFRAG include, one Lab dopus, and the old bank
    names (Amarchy-toolbar with no set, and the DT-1 sets 16 ... 32t with no family) only in Deletes of the old banks."""
    def test_body_once_and_old_bank_only_deleted(self):
        t = (ROOT / 'dist' / 'addons' / 'dopus' / 'install.dos').read_text()
        self.assertEqual(t.count('@include dist/common/@CHECKFRAG@'), 1)
        self.assertEqual(t.count('Lab dopus'), 1)
        self.assertEqual(t.count('record.dos'), 1)
        for line in t.splitlines():
            if re.search(r'Amarchy-toolbar(-\d+t?)?(?![-\w])|Amarchy/\d+t?(?![-\w])', line) and not line.lstrip().startswith(';'):
                self.assertIn('Delete', line, line)


if __name__ == '__main__':
    unittest.main()
