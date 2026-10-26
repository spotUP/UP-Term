#!/usr/bin/env python3
"""The kit's Files/MANIFEST (tools/mkmanifest.py) and dist/parts.txt, on the host: the
lines the Installer's update compares (installer update plan, U1 U2)."""
import os, pathlib, re, sys, tempfile, unittest, zlib
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import mkmanifest as mm
import mkinstaller as mi
import pkgs

PARTS_TEXT = (ROOT / "dist/parts.txt").read_text()
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

    def test_the_tutorial_is_in_the_base_kit_and_installs_with_the_userland(self):
        """plan L-6: bin/tutorial is a copy-userland file, its lessons are userland, slim.txt moves neither"""
        k = kit({"Files/userland/bin/tutorial": b"t", "Files/userland/share/tutorial/01-where-am-i.lesson": b"l"})
        lines = mm.manifest(k, PARTS, "").splitlines()
        self.assertIn("copy-userland 1 %08x Files/userland/bin/tutorial" % zlib.crc32(b"t"), lines)
        self.assertIn("userland 1 %08x Files/userland/share/tutorial/01-where-am-i.lesson" % zlib.crc32(b"l"), lines)
        self.assertNotIn("tutorial", (ROOT / "dist/slim.txt").read_text())
        self.assertNotIn("tutorial", " ".join(mm.handed_paths('base')))
        mk = (ROOT / "Makefile").read_text()
        self.assertLess(mk.index("python3 tools/slim.py remove $(KIT)/Files/userland"),
                        mk.index("cp dist/tutorial $(KIT)/Files/userland/bin/tutorial"))
        self.assertIn("cp dist/tutorial-lessons/*.lesson $(KIT)/Files/userland/share/tutorial/", mk)

    def test_the_dopus_launchers_are_in_the_base_kit_and_the_filetype_names_them(self):
        """D13: the filetype runs UP-Term:bin/uterm-edit and uterm-view; both ship (and their two helpers) as userland
        bin files, are AmigaDOS scripts that open the XCON: window on the front screen, and hand the file to vsh by ENV:"""
        import sys
        sys.path.insert(0, str(ROOT / "tools"))
        import gen_dopus_filetype as g
        mk = (ROOT / "Makefile").read_text()
        data = g.generate()
        for n in ("uterm-edit", "uterm-view", "uterm-start", "uterm-open"):
            self.assertTrue((ROOT / "dist" / n).is_file(), n)
            self.assertRegex(mk, r"(?m)^\tcp (?:\S+ )*dist/%s (?:\S+ )*\$\(KIT\)/Files/userland/bin/$" % n)
        self.assertLess(mk.index("python3 tools/slim.py remove $(KIT)/Files/userland"), mk.index("cp dist/uterm-edit"))
        k = kit({"Files/userland/bin/uterm-edit": b"e", "Files/userland/bin/uterm-open": b"o"})
        lines = mm.manifest(k, PARTS, "").splitlines()
        self.assertIn("copy-userland 1 %08x Files/userland/bin/uterm-edit" % zlib.crc32(b"e"), lines)
        for n, mode in (("uterm-edit", "edit"), ("uterm-view", "view")):
            self.assertIn(b'Execute UP-Term:bin/' + n.encode() + b' "%s"', data)
            s = (ROOT / "dist" / n).read_text()
            self.assertIn(".KEY FILE/A", s)
            self.assertIn("SetEnv UPTLAUNCHCMD " + mode + "\n", s)
            self.assertIn('SetEnv UPTLAUNCHFILE "{FILE}"', s)
            self.assertIn('NewShell "XCON:0/20/640/300/UP-Term/CLOSE/FRONT" FROM UP-Term:bin/uterm-start', s)
        start = (ROOT / "dist/uterm-start").read_text()
        self.assertIn("UP-Term:bin/vsh UP-Term:bin/uterm-open", start)
        self.assertTrue(start.rstrip().endswith("EndCLI"))
        self.assertRegex(start, r"(?m)^Stack [1-9][0-9]*$")
        op = (ROOT / "dist/uterm-open").read_text()
        self.assertIn('exec less "$f"', op)
        self.assertIn('exec ${EDITOR:-nvim} "$f"', op)

    def test_upkg_browse_is_inside_upkg_not_a_script_in_the_kit(self):
        """`upkg browse` runs in the upkg binary (opkg patch 0025): the kit carries no bin/upkg-browse script"""
        self.assertFalse((ROOT / "dist/upkg-browse").exists())
        self.assertNotIn("upkg-browse", (ROOT / "Makefile").read_text())

    def test_make_amiga_builds_every_program_dist_base_copies_into_the_kit(self):
        """A rig run with --no-build after `make amiga` stages what the kit ships: no program may be built only by
        dist-base (a stale amarchy-gui failed the window-groups W3 step). amarchy-menu needs the ports toolchain
        and libupkgui, so `make amiga` leaves it out."""
        mk = (ROOT / "Makefile").read_text().replace("\\\n", " ")
        amiga = set(re.search(r"^amiga:(.*)$", mk, re.M).group(1).split())
        copies = [l for l in mk.splitlines() if l.startswith("\tcp ") and l.rstrip().endswith("$(KIT)/Files/")]
        shipped = {w for l in copies for w in l.split() if w.startswith("$(BUILD)/amiga/")}
        self.assertIn("$(BUILD)/amiga/amarchy-gui", shipped)
        self.assertEqual(shipped - amiga, {"$(BUILD)/amiga/amarchy-menu"})

    def test_every_program_a_keymap_run_row_starts_is_shipped(self):
        """dist/keymap rows `run:<program> ...` start a program by name: a name the kit does not ship
        opens nothing (`run:upkg-browse` after the script was deleted: Install add-ons did nothing).
        Shipped = the dist scripts the Makefile copies into userland/bin, plus the ports' programs
        (upkg, tmux, nvim: built by upterm-ports into the same bin) and the ROM/system command Reboot.
        A volume-qualified name (C:..., SYS:...) is a path, not a bare name, and is skipped."""
        mk = (ROOT / "Makefile").read_text()
        shipped = set(re.findall(r"cp dist/(\S+) \$\(KIT\)/Files/userland/bin/", mk))
        shipped |= {"upkg", "tmux", "nvim"}      # ports' userland/bin
        shipped |= {"Reboot"}                    # system command
        started = set()
        for line in (ROOT / "dist/keymap").read_text().splitlines():
            if line.lstrip().startswith(";"):
                continue
            for m in re.finditer(r"(?:=|\|)\s*run:(\S+)", line):
                if ":" not in m.group(1) and not m.group(1).startswith("'"):
                    started.add(m.group(1))
        self.assertTrue(started, "no run: rows found: the parser is stale")
        self.assertEqual(sorted(started - shipped), [])

    def test_every_name_the_kit_installs_fits_amigados_30_characters(self):
        """OFS/FFS keep 30 characters: a longer name is cut (05-pipes-and-redirection.lesson lost its
        .lesson and the tutorial could not see it). The lessons, dist's own files and parts.txt's paths."""
        names = [p.name for p in (ROOT / "dist/tutorial-lessons").iterdir()]
        names += [p.name for p in (ROOT / "dist").iterdir() if p.is_file()]
        names += [part.split()[1].rsplit("/", 1)[-1] for part in PARTS_TEXT.splitlines() if part.strip() and not part.startswith("#")]
        self.assertTrue(names)
        self.assertEqual([n for n in names if len(n) > 30], [])

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
                ("Files/Claude", "Files/amiga-pi", "Files/nvim", "Files/python3", "Files/userland/bin/git",
                 "Files/userland/bin/perl", "Files/userland/bin/vim", "Files/userland/share/git-core",
                 "Files/userland/share/vim", "Files/userland/bin/BitchX", "Files/userland/lib/bx", "Files/userland/vbcc")]
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
KITDIR = pathlib.Path(os.environ.get("KIT", ROOT / "build/dist/up-term"))
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
        if KITDIR.name != "up-term":
            self.assertNotIn("handed#", (KITDIR / "Files/MANIFEST").read_text(encoding="latin-1"))
            self.skipTest("an add-on")
        kit = KITDIR
        lines = (kit / "Files/MANIFEST").read_text(encoding="latin-1").splitlines()
        for h in mm.handed_paths('base'):
            self.assertFalse((kit / h).exists(), h)
            self.assertIn("handed 0 00000000 handed#" + h, lines)
        self.assertFalse((kit / "ClaudeCode").exists())
        self.assertTrue((kit / "Files/UP-Term.version").read_text().startswith(
            "$VER: UP-Term %s (" % pkgs.get('base', 'version')))

    def test_the_base_manifest_lists_the_amissl_drawer_without_the_sdk(self):
        if KITDIR.name != "up-term":
            self.skipTest("an add-on")
        text = (KITDIR / "Files/MANIFEST").read_text(encoding="latin-1")
        for f in ("Install-AmiSSL", "Libs/AmigaOS3/amisslmaster.library", "Doc/AmiSSL.doc"):
            self.assertIn(" Files/AmiSSL/" + f, text)
        self.assertNotIn("Files/AmiSSL/Developer", text)


class PartsTable(unittest.TestCase):
    def test_the_kit_list_holds_upkg_conf_and_not_the_retired_script(self):
        """P10.5: upkg is the opkg binary from the userland drawer; the kit ships its configuration"""
        kit_paths = {k for _, k, _ in PARTS}
        self.assertIn('Files/upkg.conf', kit_paths)
        self.assertNotIn('Files/upkg', kit_paths)

    def test_every_part_named_is_a_part_of_install_dos(self):
        labs = set(re.findall(r'(?m)^Lab (\S+)', DOS))
        self.assertEqual({p for p, _, _ in PARTS} - labs - {'-'}, set())

    def test_every_file_the_makefile_copies_into_files_has_a_part(self):
        """A cp into $(KIT)/Files/ with no parts.txt line stops make dist at kitsize (Files/upsame did)"""
        mk = (ROOT / 'Makefile').read_text()
        dests = re.findall(r'(?m)^\tcp (?:-\S+ )*\S+ \$\(KIT\)/(Files/[^\s/$*]+)$', mk)
        self.assertIn('Files/upsame', dests)
        self.assertEqual([d for d in dests if mm.part_of(d, PARTS) is None], [])

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
