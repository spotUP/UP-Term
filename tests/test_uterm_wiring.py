#!/usr/bin/env python3
"""Patina opens a new UP-Term window through patina/uterm_dos.c, never a bare NewShell (s77, 2026-11-13).

open:uterm and the rich UI's UP-Term row ran `NewShell "XCON:..."` with no FROM: the window was the AmigaDOS
Shell, not vsh (no banner, no logo). The Claude row with no terminal open first opened that shell and then asked
it for a Claude tab, so two tabs opened. patina/leaf.c and patina/act.c are Amiga-only, so the host checks the
wiring in their text, as tests/test_agent_wiring.py does for the handler. The lines themselves (the vsh FROM
script, CELLS, the menu item's program) are run by tests/test_tabprog.c and tests/test_keymap.c.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LEAF = (ROOT / "patina/leaf.c").read_text(encoding="latin-1")
ACT = (ROOT / "patina/act.c").read_text(encoding="latin-1")
UTERM = (ROOT / "patina/uterm_dos.c").read_text(encoding="latin-1")


def code(text):
    """The text without its C comments (they name NewShell when they tell the history)."""
    return re.sub(r"/\*.*?\*/", "", text, flags=re.S)


class UtermWiring(unittest.TestCase):
    def test_no_bare_newshell_in_the_menus_or_the_actions(self):
        for name, text in (("patina/leaf.c", LEAF), ("patina/act.c", ACT)):
            self.assertNotIn("NewShell", code(text), name + " builds a NewShell line itself")

    def test_open_uterm_and_act_open_uterm_build_their_line_in_uterm_dos(self):
        self.assertRegex(code(LEAF), r"uterm_line\(line, sizeof line, spec, prog, task\)")
        self.assertRegex(code(ACT), r"uterm_line\(line, sizeof line, spec, TAB_SHELL, 0\)")

    def test_a_program_row_with_no_terminal_opens_the_program_window(self):
        body = code(LEAF)
        self.assertIn("lf_menu_program(", body)
        self.assertRegex(body, r"prog != TAB_SHELL\)\s*\n?\s*return open_uterm\(prog,")

    def test_the_shell_window_runs_vsh_from_a_script(self):
        body = code(UTERM)
        self.assertIn("tabprog_shell_script(", body)
        self.assertRegex(body, r"tabprog_window_line\(line, \(unsigned long\)cap, spec, 0, 0, name\)")


if __name__ == "__main__":
    unittest.main()
