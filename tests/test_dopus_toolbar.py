#!/usr/bin/env python3
"""B7, the one reachability test of tools/gen_dopus_toolbar.py: the generator's own command line (what `make
dopus-toolbar` runs) turns the md5-checked upstream Buttons/Toolbar of DOpus5_104_os3.lha and dist/dopus-toolbar.txt
into Images/<name> and Buttons/Amarchy-toolbar. Asserts: every image path the bank names exists in the drawer, each
image decodes as ILBM at the measured upstream size (24x24) with depth <= 2 and has glyph pixels (the sentinel: the
generator ran), the Borderless flag is set, and the bank differs from upstream only in the image paths and that flag.
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
            bank = (t / 'out' / 'Buttons' / 'Amarchy-toolbar').read_bytes()
            drawer = t / 'out' / 'Images'

            # every image the bank names exists, decodes at the upstream size and carries glyph pixels
            paths_in_bank = set()
            for tag, data in chunks(bank):
                if tag == b'BUTN':
                    paths_in_bank |= set(re.findall(rb'dopus5:images/[^\0]+', data, re.I))
            self.assertEqual(len(paths_in_bank), 22)
            for p in paths_in_bank:
                self.assertTrue(p.lower().startswith(b'dopus5:images/amarchy/'), p)
                f = drawer / p.decode().split('/')[-1]
                self.assertTrue(f.exists(), f)
                w, h, depth, _, cmap, rows = ilbm_decode.decode(f.read_bytes())
                self.assertEqual((w, h), (24, 24))
                self.assertLessEqual(depth, 2)
                self.assertEqual(len(cmap), 3 * 4)
                self.assertGreater(sum(1 for r in rows for v in r if v), 20, f)   # not blank
                self.assertTrue(any(v == 1 for r in rows for v in r), f)

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
                self.assertEqual(bd.replace(b'dopus5:images/Amarchy/', b'dopus5:images/'), ud)


if __name__ == '__main__':
    unittest.main()
