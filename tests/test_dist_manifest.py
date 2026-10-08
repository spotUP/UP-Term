#!/usr/bin/env python3
"""The kit's Files/MANIFEST (tools/mkmanifest.py) and dist/parts.txt, on the host: the
lines the Installer's update compares (installer update plan, U1 U2)."""
import pathlib, re, sys, tempfile, unittest, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkmanifest as mm

PARTS = mm.read_parts((ROOT / "dist/parts.txt").read_text())
DOS = (ROOT / "dist/install.dos").read_text(encoding="latin-1")
INSTALLER = (ROOT / "dist/Install.installer").read_text(encoding="latin-1")


def kit(files):
    d = pathlib.Path(tempfile.mkdtemp())
    for name, data in files.items():
        (d / name).parent.mkdir(parents=True, exist_ok=True)
        (d / name).write_bytes(data)
    return d


class Manifest(unittest.TestCase):
    TEXT = "; head\nLab drawer\nEcho a\nLab system\nCopy vsh C:vsh\n"

    def test_lines_name_part_size_crc_and_path_sorted_by_path(self):
        k = kit({"Install": b"i", "Files/vsh": b"vsh!", "Files/UP-Term Prefs": b"p",
                 "Files/coreutils/bin/ls": b"ls", "Files/coreutils/COPYING": b"gpl"})
        parts = mm.read_parts("-  Install\nsystem  Files/vsh\nsystem  Files/UP-Term Prefs\n"
                              "copy-coreutils  Files/coreutils/bin  bin\n-  Files/coreutils\n")
        lines = mm.manifest(k, parts, self.TEXT).splitlines()
        self.assertIn("system 4 %08x Files/vsh" % zlib.crc32(b"vsh!"), lines)
        self.assertIn("system 1 %08x Files/UP-Term Prefs" % zlib.crc32(b"p"), lines)
        self.assertIn("copy-coreutils 2 %08x Files/coreutils/bin/ls" % zlib.crc32(b"ls"), lines)
        self.assertIn("- 3 %08x Files/coreutils/COPYING" % zlib.crc32(b"gpl"), lines)
        self.assertIn("all 0 %08x install.dos#all" % zlib.crc32(b"; head\n"), lines)
        self.assertIn("system 0 %08x install.dos#system" % zlib.crc32(b"Copy vsh C:vsh\n"), lines)
        paths = [l.split(' ', 3)[3].encode() for l in lines]
        self.assertEqual(paths, sorted(paths))

    def test_a_file_no_part_claims_stops_the_build(self):
        k = kit({"Files/vsh": b"v", "Files/newtool": b"n"})
        with self.assertRaises(SystemExit) as e:
            mm.manifest(k, mm.read_parts("system  Files/vsh\n"), "")
        self.assertIn("Files/newtool", str(e.exception))

    def test_the_manifest_itself_is_not_in_it(self):
        k = kit({"Files/vsh": b"v", "Files/MANIFEST": b"old"})
        self.assertNotIn("MANIFEST", mm.manifest(k, mm.read_parts("system  Files/vsh\n"), ""))


class PartsTable(unittest.TestCase):
    def test_every_part_named_is_a_part_of_install_dos(self):
        labs = set(re.findall(r'(?m)^Lab (\S+)', DOS))
        self.assertEqual({p for p, _, _ in PARTS} - labs - {'-'}, set())

    def test_copy_parts_copy_the_drawers_parts_txt_names(self):
        """install.dos's copy-<part> and the Installer's P_COPY copy the same drawer to the same place."""
        copy = {p: (k, d) for p, k, d in PARTS if p.startswith('copy-')}
        for name, body in mm.sections(DOS):
            if not name.startswith('copy-'):
                continue
            src, dst = re.search(r'(?m)^\s*Copy (\S+) UP-Term:(\S+) ALL CLONE QUIET$', body).groups()
            self.assertEqual(copy[name], ('Files/' + src, dst), name)
            self.assertIn('(tackon #files "%s") #to (tackon #dest "%s")' % (src, dst), INSTALLER, name)


if __name__ == '__main__':
    sys.exit(0 if unittest.main(exit=False, verbosity=1).result.wasSuccessful() else 1)
