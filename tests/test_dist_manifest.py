#!/usr/bin/env python3
"""The kit's Files/MANIFEST (tools/mkmanifest.py) and dist/parts.txt, on the host: the
lines the Installer's update compares (installer update plan, U1 U2)."""
import os, pathlib, re, sys, tempfile, unittest, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkmanifest as mm
import mkinstaller as mi

PARTS = mm.read_parts((ROOT / "dist/parts.txt").read_text())
DOS = mi.build(ROOT / "dist/install.dos", "base")
INSTALLER = mi.build(ROOT / "dist/Install.installer", "base")


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
        self.assertIn("copy-coreutils 0 00000000 copy#Files/coreutils/bin#bin", lines)
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


class HandedLines(unittest.TestCase):
    """The base's manifest names the paths it hands to the add-on archives, from dist/packages.txt
    (kit-split D7): the line an Update over the single kit reads to leave those files alone."""
    def test_the_base_hands_over_every_add_ons_paths_in_sorted_order(self):
        k = kit({"Files/vsh": b"v"})
        lines = mm.manifest(k, mm.read_parts("system  Files/vsh\n"), "", mm.handed_paths('base')).splitlines()
        want = ["handed 0 00000000 handed#" + p for p in
                ("Files/Claude", "Files/amiga-pi", "Files/nvim", "Files/python3")]
        self.assertEqual([l for l in lines if l.startswith("handed ")], sorted(want, key=lambda l: l.split(' ', 3)[3].encode()))
        paths = [l.split(' ', 3)[3].encode() for l in lines]
        self.assertEqual(paths, sorted(paths))

    def test_an_add_on_hands_nothing_over(self):
        self.assertEqual(mm.handed_paths('nvim'), [])

    def test_the_handed_paths_are_not_in_the_base_kit(self):
        """handed paths come from packages.txt; no file of the base kit's parts.txt is under one"""
        handed = mm.handed_paths('base')
        self.assertTrue(handed)
        for _, path, _ in PARTS:
            for h in handed:
                self.assertFalse(path == h or path.startswith(h + '/'), "%s is handed over but the base kit lists %s" % (h, path))


# KIT=build/dist/<archive>: make dist-check runs this once for each archive
KITDIR = pathlib.Path(os.environ.get("KIT", ROOT / "build/dist/UP-Term"))
if not KITDIR.is_absolute():
    KITDIR = ROOT / KITDIR


@unittest.skipUnless((KITDIR / "Files/MANIFEST").exists(), "no built kit (make dist)")
class BuiltKit(unittest.TestCase):
    def test_every_file_of_the_archive_is_in_its_manifest_with_its_crc(self):
        lines = {l.split(" ", 3)[3]: l.split(" ", 3) for l in (KITDIR / "Files/MANIFEST").read_text(encoding="latin-1").splitlines()}
        for f in KITDIR.rglob("*"):
            rel = f.relative_to(KITDIR).as_posix()
            if f.is_file() and rel != "Files/MANIFEST":
                self.assertIn(rel, lines, rel)
                self.assertEqual(int(lines[rel][2], 16), zlib.crc32(f.read_bytes()), rel)

    def test_the_manifest_has_a_line_for_every_part_of_install_dos(self):
        dos = (KITDIR / "Files/install.dos").read_text(encoding="latin-1")
        text = (KITDIR / "Files/MANIFEST").read_text(encoding="latin-1")
        for name, _ in mm.sections(dos):
            self.assertIn(" install.dos#%s\n" % name, text)

    def test_only_the_base_hands_paths_over_and_holds_none_of_them(self):
        if KITDIR.name != "UP-Term":
            self.assertNotIn("handed#", (KITDIR / "Files/MANIFEST").read_text(encoding="latin-1"))
            self.skipTest("an add-on")
        kit = KITDIR
        lines = (kit / "Files/MANIFEST").read_text(encoding="latin-1").splitlines()
        for h in mm.handed_paths('base'):
            self.assertFalse((kit / h).exists(), h)
            self.assertIn("handed 0 00000000 handed#" + h, lines)
        self.assertFalse((kit / "ClaudeCode").exists())
        self.assertTrue((kit / "Files/UP-Term.version").read_text().startswith("$VER: UP-Term 1.0 ("))


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
