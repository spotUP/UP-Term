"""LEGW-W2: the Amarchy-Flat start entry is a marked block in S:Startup-Sequence (before IPrefs), not in S:User-Startup.
Install writes it through the one inserter (instssblock.sh) with the absolute install path, unstartup.sh strips it by
name (and a plain strip of the sequence removes it), the base Uninstall runs the strips, and the kit ships the binary."""
import pathlib, re, subprocess, tempfile, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
DOS = (ROOT / "dist/install.dos").read_text()
BLOCK = [";BEGIN UP-Term Amarchy-Flat", "If EXISTS Work:UP-Term/bin/amarchy-flat",
         "  Run >NIL: Work:UP-Term/bin/amarchy-flat BOOT", "EndIf", ";END UP-Term Amarchy-Flat"]


def run(script, args, t):
    s = (ROOT / "dist" / script).read_text().replace("S:", "S_").replace("T:", "T_").replace("ENV:", "E_")
    r = subprocess.run(["sh", "-c", s, "x", *args], cwd=t, capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    return r


class FlatStartupBlock(unittest.TestCase):
    def test_install_writes_no_flat_line_to_user_startup(self):
        self.assertNotRegex(DOS, r"Echo >>S:User-Startup \"[^\n]*amarchy-flat")

    def test_install_inserts_the_block_before_iprefs_with_the_absolute_path(self):
        self.assertRegex(DOS, r'vsh instssblock\.sh Amarchy-Flat IPrefs "If EXISTS \$upflatdir/bin/amarchy-flat" '
                              r'"  Run >NIL: \$upflatdir/bin/amarchy-flat BOOT" "EndIf"')
        self.assertIn("ENVARC:up-term/Dir", DOS)

    def test_install_still_strips_the_old_user_startup_block(self):
        self.assertIn('vsh unstartup.sh "Amarchy-Flat"', DOS)

    def test_kit_ships_the_binary(self):
        self.assertRegex((ROOT / "dist/parts.txt").read_text(), r"(?m)^tools\s+Files/amarchy-flat$")
        mk = (ROOT / "Makefile").read_text()
        self.assertIn("$(BUILD)/amiga/amarchy-flat $(BUILD)/amiga/amnotify", mk)
        self.assertRegex(DOS, r"(?m)^Copy Amarchy .*\bamarchy-flat\b.* UP-Term:bin CLONE")

    def test_uninstall_strips_the_block_from_both_files(self):
        u = (ROOT / "dist/Uninstall").read_text()
        self.assertIn('$unvsh "$unst" Amarchy-Flat', u)  # an old kit's User-Startup copy
        self.assertIn("S:Startup-Sequence", u)

    def test_unstartup_strips_the_block_from_the_sequence_by_name_and_plainly(self):
        keep = "; stock\nEcho keep\n"
        text = keep + "\n".join(BLOCK) + "\nIPrefs\n"
        with tempfile.TemporaryDirectory() as t:
            pathlib.Path(t, "S_Startup-Sequence").write_text(text)
            run("unstartup.sh", ["Amarchy-Flat", "S_Startup-Sequence"], t)
            self.assertEqual(pathlib.Path(t, "T_Startup-Sequence.up-term").read_text(), keep + "IPrefs\n")
            run("unstartup.sh", ["", "S_Startup-Sequence"], t)
            self.assertEqual(pathlib.Path(t, "T_Startup-Sequence.up-term").read_text(), keep + "IPrefs\n")


if __name__ == "__main__":
    unittest.main()
