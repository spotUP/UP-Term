#!/usr/bin/env python3
"""BOOTX:go waits for ENV: before its first ENV: use (RIG-ENV-RACE, RIG-ENV-UNASSIGNED, 2026-10-28).
BOOTX:s/startup-sequence Runs go beside DH0:S/Startup-Sequence, which assigns ENV: only after SetPatch;
go's SetEnv lines could run first and raise "Please insert volume ENV:". A boot that never got ENV:
(the 68LC060 SetPatch Guru) now fails the start check instead of leaving a rig with no ENV:."""
import pathlib, re, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


def env_users(lines):
    # SetEnv/GetEnv/Unsetenv write or read ENV:, as does any line naming ENV:
    return [i for i, l in enumerate(lines) if not l.lstrip().startswith(";") and (
            re.match(r"\s*C:(Set|Get|Unset)Env\b", l, re.I) or re.search(r"\bENV:", l))]


class EnvWait(unittest.TestCase):
    def test_go_waits_for_env_before_its_first_env_use(self):
        lines = rig.GO.splitlines()
        loop = lines.index("Lab envwait")
        self.assertEqual(lines[loop + 1:loop + 6], ["C:Assign >NIL: ENV: EXISTS", "If WARN", "  C:Wait 1",
                                                  "  Skip envwait BACK", "EndIf"])
        users = [i for i in env_users(lines) if not loop <= i <= loop + 5]
        self.assertTrue(users, "go uses ENV: (SetEnv TERMINFO, wasabi.key)")
        self.assertLess(loop, min(users), "the wait loop comes before every ENV: use")

    def test_start_check_needs_env(self):
        self.assertIn("ENV", rig.BOOT_ASSIGNS)


if __name__ == "__main__":
    unittest.main()
