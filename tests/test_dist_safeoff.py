#!/usr/bin/env python3
"""dist/S/safe-off names only commands the kit installs (plan 2026-10-29-amarchy-p4-capture-notify-power.md R4.12).
Every command word in the script is an AmigaDOS script keyword or a program that install.dos copies to UP-Term:bin or C:
(Reboot, amnotify); the script itself ships in the S drawer and is copied to UP-Term:S."""
import pathlib, re, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
KEYWORDS = {"if", "else", "endif"}
SCRIPT = (ROOT / "dist/S/safe-off").read_text()
DOS = (ROOT / "dist/install.dos").read_text()
PARTS = (ROOT / "dist/parts.txt").read_text()


def words():
    out = []
    for line in SCRIPT.splitlines():
        line = line.strip()
        if line and not line.startswith(";"):
            out.append(line.split()[0])
    return out


class SafeOff(unittest.TestCase):
    def test_only_installed_commands(self):
        copy = [l for l in map(str.strip, DOS.splitlines()) if l.startswith("Copy") and ("UP-Term:bin" in l or "C:" in l)]
        installed = {"reboot", "amnotify"}
        for w in words():
            if w.lower() in KEYWORDS:
                continue
            self.assertIn(w.lower(), installed, w)
            self.assertTrue(any(re.search(r"\b%s\b" % re.escape(w), l, re.I) for l in copy), "install.dos copies %s" % w)

    def test_uses_dry_run_and_both_outcomes(self):
        self.assertIn("Reboot DRYRUN", SCRIPT)
        self.assertIn("Safe to switch off", SCRIPT)
        self.assertIn("Not all disks answered", SCRIPT)
        self.assertIn("BODYFILE T:safeoff.txt", SCRIPT)

    def test_script_is_shipped_and_installed(self):
        self.assertIn("Files/S", PARTS)
        self.assertIn("UP-Term:S", DOS)


if __name__ == "__main__":
    unittest.main()
