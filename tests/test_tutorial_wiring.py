#!/usr/bin/env python3
"""Help > Shell tutorial and /tutorial reach the tab spawn (plan L-7, L-T3).

handler/vtcon_handler.c is Amiga-only code, so the host checks the wiring in its text: the menu row
sits directly after "Slash commands" with MENU_TUTORIAL, the menu dispatch and host_command send that id to
tab_spawn(..., TAB_TUTORIAL), and the TAB_TUTORIAL row of tab_progs is built from the tabprog.h constants that
tests/test_tabprog.c pins (the exact command line). tests/test_slash.c pins /tutorial -> MENU_TUTORIAL. The
spawn itself runs on the rig (plan L-R).
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
H = (ROOT / "handler/vtcon_handler.c").read_text(encoding="latin-1")


class TutorialWiring(unittest.TestCase):
    def test_menu_item_follows_slash_commands(self):
        self.assertRegex(
            H,
            r'"Slash commands",[^\n]*MENU_HELP_SLASH \},\n\s*\{ NM_ITEM, \(STRPTR\)"Shell tutorial", 0, 0, 0, \(APTR\)MENU_TUTORIAL \}\n\};',
        )
        self.assertIn("#define MENU_HELP_ITEMS 4", H)  # the strip copies this many rows

    def test_menu_id_reaches_tab_spawn_with_the_tutorial_row(self):
        self.assertRegex(H, r"case MENU_DEMO: case MENU_CLAUDE: case MENU_TUTORIAL:\n\s*tab_command")
        self.assertRegex(H, r"case MENU_TUTORIAL:\n\s*if \(!tab_spawn\(c, from->profile, from, TAB_TUTORIAL\)\)")
        self.assertIn("enum { TAB_SHELL = 0, TAB_DEMO, TAB_CLAUDE, TAB_TUTORIAL };", H)
        rows = H.split("static const struct tab_prog tab_progs[] = {")[1].split("};")[0]
        self.assertEqual(len(re.findall(r"^\s*\{ ", rows, re.M)), 4)  # shell, demo, claude, tutorial: the enum's order
        self.assertRegex(rows, r'\{ TABPROG_TUTORIAL_FILE, TABPROG_TUTORIAL_CMD, "T:UP-Term-tutorial"')
        self.assertIn("tabprog_script(script, sizeof(script), p->pre, p->cmd)", H)


if __name__ == "__main__":
    unittest.main()
