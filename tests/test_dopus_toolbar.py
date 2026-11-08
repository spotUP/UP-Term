#!/usr/bin/env python3
"""B7, the one reachability test of tools/gen_dopus_toolbar.py: the generator's own command line (what `make
dopus-toolbar` runs) turns the md5-checked upstream Buttons/Toolbar of DOpus5_104_os3.lha and dist/dopus-toolbar.txt
into Images/<name> and Buttons/Amarchy-toolbar. Asserts: every image path the bank names exists in the drawer, each
image decodes as ILBM at the measured upstream size (its set's size) with depth 3 and has glyph pixels (the sentinel: the
generator ran), the Borderless flag is set, and the bank differs from upstream only in the image paths and that flag.
DT-1: size sets 16, 24, 32 x square and tall pixel, depth 3, one bank per set.
Run: python3 -I tests/test_dopus_toolbar.py   (skips when the vendored archive or build/lucide is absent)"""
import hashlib, pathlib, re, struct, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
sys.path.insert(0, str(ROOT / 'tools' / 'rig'))
import ilbm_decode, paths

ARCHIVE = pathlib.Path(paths.VENDOR) / 'dopus5-5.104' / 'DOpus5_104_os3.lha'
MD5 = '0800b7b3bf02c83bd45a84d038ee483e'
TTF = ROOT / 'build' / 'lucide' / 'lucide.ttf'
TABLE = ROOT / 'dist' / 'dopus-toolbar.txt'
FLAGS_OFF = 12 + 8 + 128   # FORM header, BTNW chunk header, CFG_BTNW flags (dopus5.h)


def chunks(buf):
    pos, out = 12, []
    while pos < len(buf):
        tag, size = buf[pos:pos + 4], struct.unpack('>L', buf[pos + 4:pos + 8])[0]
        out.append((tag, buf[pos + 8:pos + 8 + size]))
        pos += 8 + size + (size & 1)
    return out


@unittest.skipUnless(ARCHIVE.exists() and TTF.exists(), 'vendored DOpus archive or build/lucide missing (make lucide)')
class DopusToolbar(unittest.TestCase):
    def test_generated_bank_is_complete_and_flat(self):
        self.assertEqual(hashlib.md5(ARCHIVE.read_bytes()).hexdigest(), MD5)
        with tempfile.TemporaryDirectory() as t:
            t = pathlib.Path(t)
            subprocess.run(['lha', '-xqw=' + str(t / 'up'), str(ARCHIVE), 'DOpus5/Buttons/Toolbar'], check=True)
            up = (t / 'up' / 'DOpus5' / 'Buttons' / 'Toolbar').read_bytes()
            subprocess.run([sys.executable, '-I', str(ROOT / 'tools' / 'gen_dopus_toolbar.py'), str(TABLE), str(TTF),
                            str(t / 'up' / 'DOpus5' / 'Buttons' / 'Toolbar'), str(t / 'out')], check=True)
            bank = (t / 'out' / 'Buttons' / 'Amarchy-toolbar-24').read_bytes()
            drawer = t / 'out' / 'Images' / '24'

            # every image the bank names exists, decodes at the upstream size and carries glyph pixels
            paths_in_bank = set()
            for tag, data in chunks(bank):
                if tag == b'BUTN':
                    paths_in_bank |= set(re.findall(rb'dopus5:images/[^\0]+', data, re.I))
            self.assertEqual(len(paths_in_bank), 22)
            for p in paths_in_bank:
                self.assertTrue(p.lower().startswith(b'dopus5:images/amarchy/24/'), p)
                f = drawer / p.decode().split('/')[-1]
                self.assertTrue(f.exists(), f)
                w, h, depth, _, cmap, rows = ilbm_decode.decode(f.read_bytes())
                self.assertEqual((w, h), (24, 24))
                self.assertEqual(depth, 3)
                self.assertEqual(len(cmap), 3 * 8)
                self.assertGreater(sum(1 for r in rows for v in r if v), 20, f)   # not blank
                self.assertTrue(any(v == 3 for r in rows for v in r), f)   # levels map 1:1 to pens: full glyph is pen 3
                self.assertTrue(set(v for r in rows for v in r) <= {0, 1, 2, 3}, f)   # no accent pen in the glyph levels

            # DT-1: every size set exists, square and tall pixel (half the height), one bank per set, bank paths name the set
            for name, w, h in [('16', 16, 16), ('16t', 16, 8), ('24', 24, 24), ('24t', 24, 12), ('32', 32, 32), ('32t', 32, 16)]:
                b = (t / 'out' / 'Buttons' / ('Amarchy-toolbar-' + name)).read_bytes()
                self.assertIn(('dopus5:images/Amarchy/%s/' % name).encode(), b)
                w2, h2, d2, _, _, rows2 = ilbm_decode.decode((t / 'out' / 'Images' / name / 'Directory').read_bytes() if (t / 'out' / 'Images' / name / 'Directory').exists() else next((t / 'out' / 'Images' / name).iterdir()).read_bytes())
                self.assertEqual((w2, h2, d2), (w, h, 3), name)
                self.assertTrue(any(v for r in rows2 for v in r), name)

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
                self.assertEqual(bd.replace(b'dopus5:images/Amarchy/24/', b'dopus5:images/'), ud)


class InstallScript(unittest.TestCase):
    """dist/addons/dopus/install.dos carries one installer body: one CHECKFRAG include, one Lab dopus, and the single-bank
    name Amarchy-toolbar (no set) only in the Delete of the old bank."""
    def test_body_once_and_old_bank_only_deleted(self):
        t = (ROOT / 'dist' / 'addons' / 'dopus' / 'install.dos').read_text()
        self.assertEqual(t.count('@include dist/common/@CHECKFRAG@'), 1)
        self.assertEqual(t.count('Lab dopus'), 1)
        self.assertEqual(t.count('record.dos'), 1)
        for line in t.splitlines():
            if re.search(r'Amarchy-toolbar(?![-\w])', line) and not line.lstrip().startswith(';'):
                self.assertIn('Delete', line, line)


if __name__ == '__main__':
    unittest.main()
