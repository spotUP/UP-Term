#!/usr/bin/env python3
"""SHELL-PATH-RC10 (2026-11-03): rig.py's Path line in S:Shell-Startup named VTC:gg/bin whenever VTC: was
assigned, so a rig without that drawer printed "Path failed returncode 10" in every new Shell. The line is
now run only if the drawer exists, and an earlier block on a rig disk is rewritten."""
import pathlib, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools/rig"))
import rig


class ShellPath(unittest.TestCase):
    def test_path_line_is_inside_a_drawer_exists_guard(self):
        b = rig.SHELL_STARTUP_PATH
        self.assertLess(b.index("  If EXISTS VTC:gg/bin"), b.index("    Path >NIL: VTC:gg/bin ADD"))

    def test_earlier_blocks_are_rewritten_and_the_new_one_is_stable(self):
        for old in rig.OLD_SHELL_PATH_BLOCKS:
            lines = ["Prompt x"] + old + [";BEGIN UP-Term upkg"]
            new = rig.upgrade_shell_path(lines)
            self.assertEqual(new, ["Prompt x"] + rig.SHELL_STARTUP_PATH + [";BEGIN UP-Term upkg"])
            self.assertEqual(rig.upgrade_shell_path(new), new)


if __name__ == "__main__":
    unittest.main()
