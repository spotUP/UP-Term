#!/usr/bin/env python3
"""C:Agent "<task>" and /ask <task> reach tab_spawn with the task (plan OMARCHY-AI A4).

handler/vtcon_handler.c is Amiga-only code (no host harness runs it), so the host checks the wiring in its
text, the way tests/test_tutorial_wiring.py does: the slash row, the slash_run case, the TM_COMMAND message
that carries the task to the host window, host_command's MENU_AGENT case, and the tab script that quotes it.
The quoting and the command lines are run by tests/test_tabprog.c and tests/test_slash.c. The live spawn
(a tab whose agent starts on the task) is a rig check.
"""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
H = (ROOT / "handler/vtcon_handler.c").read_text(encoding="latin-1")


class AskWiring(unittest.TestCase):
    def test_slash_ask_row_and_enum(self):
        self.assertIn("SLASH_ASK", (ROOT / "handler/slash.h").read_text())
        self.assertRegex((ROOT / "handler/slash.c").read_text(), r'\{ "ask", SL_ARG, SLASH_ASK, 0, 0, "\[TASK\]"')

    def test_slash_run_sends_the_task_as_a_menu_agent_command(self):
        self.assertRegex(H, r"case SLASH_ASK:[^\n]*\n\s*tab_command_arg\(c, MENU_AGENT, cmd\.arg\);")

    def test_the_task_rides_the_tab_message_to_the_host(self):
        self.assertRegex(H, r"char task\[200\];[^\n]*\n")  # struct tab_msg
        self.assertIn("copy_str(t->task, arg ? arg : \"\", sizeof(t->task));", H)
        self.assertIn("host_command(c, t->what, t->from, t->task);", H)
        self.assertIn("host_command(c, what, c, arg);", H)  # the host's own window: no message

    def test_menu_agent_hands_the_task_to_tab_spawn(self):
        self.assertRegex(H, r"tab_spawn\(c, from->profile, from, tabprog_agent_tab\(amconf_agent\(&ap\)\), task\)")

    def test_the_tab_script_quotes_the_task_onto_the_agent_command(self):
        self.assertIn("tabprog_task_cmd(line, sizeof(line), p->cmd, m->task)", H)
        self.assertIn("tabprog_script(script, sizeof(script), p->pre, line)", H)


class AgentToolWiring(unittest.TestCase):
    def test_the_tool_sends_ask_through_the_shared_window_route(self):
        c = (ROOT / "install/agent.c").read_text()
        self.assertIn('strcpy(line, "/ask");', c)
        self.assertIn("upterm_window_command(line, ans, sizeof(ans))", c)
        self.assertIn("long __stack = 16384;", c)
        self.assertIn('"$STACK: 16384"', c)
        self.assertIn("qmark_line(", c)
        # one copy of the DoPkt route: C:UPTerm uses the same function
        u = (ROOT / "handler/upterm.c").read_text()
        self.assertIn("upterm_window_command(line, ans, sizeof(ans))", u)
        self.assertNotIn("DoPkt(", u)

    def test_agent_is_built_shipped_installed_and_removed_with_the_base(self):
        mk = (ROOT / "Makefile").read_text()
        self.assertRegex(mk, r"\$\(BUILD\)/amiga/Agent: install/agent\.c")
        self.assertRegex(mk, r"(?m)^amiga:.*\$\(BUILD\)/amiga/Agent")
        self.assertRegex(mk, r"cp [^\n]*\$\(BUILD\)/amiga/Agent [^\n]*\$\(KIT\)/Files/")
        self.assertIn("tools           Files/Agent", (ROOT / "dist/parts.txt").read_text())
        self.assertIn("\nCopy Agent C:Agent CLONE\n", (ROOT / "dist/install.dos").read_text())
        self.assertRegex((ROOT / "dist/Uninstall").read_text(), r"Delete >NIL: [^\n]*\bC:Agent\b")


if __name__ == "__main__":
    unittest.main()
