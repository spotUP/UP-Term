"""LEGW-W6: the two SYS:WBStartup icons that start Amarchy and the app index at boot.
Runs the Makefile's own mkicon rules and reads back the icon they write: a PROJECT icon (do_Type 4) whose default
tool is UP-Term:bin/<program> (no binary is copied, so an Update leaves no stale twin), with DONOTWAIT, STARTPRI
(Amarchy higher than the indexer) and, for the indexer, a low TOOLPRI. Also: the kit rows and the install/uninstall
wiring (Install copies both icons into SYS:WBStartup, Uninstall deletes them)."""
import pathlib, re, shlex, struct, subprocess, sys, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
MAKEFILE = (ROOT / "Makefile").read_text()
DOS = (ROOT / "dist/install.dos").read_text()
UNINSTALL = (ROOT / "dist/Uninstall").read_text()
PARTS = (ROOT / "dist/parts.txt").read_text()


def read_icon(data):
    """(do_Type, default tool, [tooltypes]) of a DiskObject written by tools/mkicon.py (project icon layout)"""
    assert struct.unpack(">H", data[:2])[0] == 0xE310
    do_type = data[48]
    # the default tool and the tooltype array follow the image planes; both are length-prefixed C strings
    m = re.search(rb"\x00\x00\x00([\x01-\xff])(UP-Term:bin/[A-Za-z-]+)\x00", data)
    tool = m.group(2).decode()
    pos = m.end()
    size = struct.unpack(">L", data[pos:pos + 4])[0]
    pos += 4
    tts = []
    for _ in range(size // 4 - 1):
        n = struct.unpack(">L", data[pos:pos + 4])[0]
        tts.append(data[pos + 4:pos + 4 + n - 1].decode())
        pos += 4 + n
    return do_type, tool, tts


class WbStartupIcons(unittest.TestCase):
    def make(self):
        out = {}
        with tempfile.TemporaryDirectory() as d:
            lines = [l.strip() for l in MAKEFILE.splitlines()
                     if l.strip().startswith("python3 tools/mkicon.py $(KIT)/Files/wbstartup/")]
            self.assertEqual(len(lines), 2, "one Makefile rule per WBStartup icon")
            for line in lines:
                argv = shlex.split(line.replace("$(KIT)", d))
                argv[0] = sys.executable
                argv[1] = str(ROOT / argv[1])
                pathlib.Path(d, "Files/wbstartup").mkdir(parents=True, exist_ok=True)
                subprocess.run(argv, check=True, cwd=ROOT)
                out[pathlib.Path(argv[2]).name] = read_icon(pathlib.Path(argv[2]).read_bytes())
        return out

    def test_generated_icons_carry_donotwait_startpri_toolpri_and_default_tool(self):
        icons = self.make()
        self.assertEqual(set(icons), {"Amarchy.info", "amarchy-index.info"})
        t, tool, tts = icons["Amarchy.info"]
        self.assertEqual((t, tool), (4, "UP-Term:bin/Amarchy"))
        self.assertEqual(tts, ["DONOTWAIT", "STARTPRI=10"])
        t, tool, tts = icons["amarchy-index.info"]
        self.assertEqual((t, tool), (4, "UP-Term:bin/amarchy-index"))
        self.assertEqual(tts, ["DONOTWAIT", "STARTPRI=-10", "TOOLPRI=-5"])
        # Amarchy starts first
        self.assertGreater(int(icons["Amarchy.info"][2][1].split("=")[1]), int(icons["amarchy-index.info"][2][1].split("=")[1]))

    def test_kit_row_install_copy_and_uninstall_delete(self):
        self.assertRegex(PARTS, r"(?m)^desktop\s+Files/wbstartup$")
        i = DOS.index("Lab desktop")
        self.assertIn("Copy wbstartup/Amarchy.info wbstartup/amarchy-index.info SYS:WBStartup CLONE", DOS[i:])
        self.assertIn("Delete >NIL: SYS:WBStartup/Amarchy.info SYS:WBStartup/amarchy-index.info QUIET", UNINSTALL)


if __name__ == "__main__":
    unittest.main()
