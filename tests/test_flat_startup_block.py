"""FLAT-C4: the ;BEGIN Amarchy-Flat startup block. Install writes exactly these lines, unstartup.sh strips only that
block (every other line byte for byte), the base Uninstall runs the by-name strip, and the kit ships the binary."""
import pathlib, re, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOS = (ROOT / "dist/install.dos").read_text()
BLOCK = [";BEGIN Amarchy-Flat", "Assign >NIL: UP-Term: EXISTS", "If NOT WARN",
         "  Run >NIL: UP-Term:bin/amarchy-flat BOOT", "EndIf", ";END Amarchy-Flat"]


class FlatStartupBlock(unittest.TestCase):
    def echoed(self):
        return [m.group(1) for m in re.finditer(r'(?m)^\s*Echo >>S:User-Startup "(.*)"$', DOS)]

    def test_install_writes_the_block_exactly(self):
        e = self.echoed()
        i = e.index(";BEGIN Amarchy-Flat")
        self.assertEqual(e[i:i + len(BLOCK)], BLOCK)

    def test_install_strips_an_old_copy_first_and_guards_the_add(self):
        self.assertIn('Search >NIL: S:User-Startup "  Run >NIL: UP-Term:bin/amarchy-flat BOOT"', DOS)
        self.assertIn('vsh unstartup.sh "Amarchy-Flat"', DOS)

    def test_kit_ships_the_binary(self):
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^tools\s+Files/amarchy-flat$")
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("$(BUILD)/amiga/amarchy-flat $(BUILD)/amiga/amnotify", mk)
        self.assertRegex(DOS, r"(?m)^Copy Amarchy .*\bamarchy-flat\b.* UP-Term:bin CLONE")

    def test_uninstall_strips_the_block_by_name(self):
        self.assertIn('$unvsh "$unst" Amarchy-Flat', (ROOT / "dist/Uninstall").read_text())

    def test_unstartup_by_name_removes_only_the_block(self):
        keep = "; user\n;BEGIN UP-Term Amarchy\nRun >NIL: UP-Term:bin/Amarchy\n;END UP-Term Amarchy\nEcho keep\n"
        text = keep + "\n".join(BLOCK) + "\n" + "Echo after\n"
        s = (ROOT / "dist/unstartup.sh").read_text().replace("S:", "S_").replace("T:", "T_").replace("ENV:", "E_")
        with tempfile.TemporaryDirectory() as t:
            pathlib.Path(t, "S_User-Startup").write_text(text)
            r = subprocess.run(["sh", "-c", s, "x", "Amarchy-Flat"], cwd=t, capture_output=True, text=True)
            self.assertEqual(r.returncode, 0, r.stderr)
            self.assertEqual(pathlib.Path(t, "T_User-Startup.up-term").read_text(), keep + "Echo after\n")
            # the plain call (every UP-Term block) leaves the stand-alone block alone
            r = subprocess.run(["sh", "-c", s, "x"], cwd=t, capture_output=True, text=True)
            self.assertIn(";BEGIN Amarchy-Flat", pathlib.Path(t, "T_User-Startup.up-term").read_text())


if __name__ == "__main__":
    unittest.main()
