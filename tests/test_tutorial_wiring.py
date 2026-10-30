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
        self.assertRegex(H, r"case MENU_DEMO: case MENU_CLAUDE: case MENU_PI: case MENU_AGENT: case MENU_SET_AGENT_CLAUDE: case MENU_SET_AGENT_PI: case MENU_TUTORIAL:\n\s*tab_command")
        self.assertRegex(H, r"case MENU_TUTORIAL:\n\s*if \(!tab_spawn\(c, from->profile, from, TAB_TUTORIAL, 0\)\)")
        self.assertIn("enum { TAB_SHELL = 0, TAB_DEMO, TAB_CLAUDE, TAB_TUTORIAL, TAB_PI };", (ROOT / "handler/tabprog.h").read_text())
        rows = H.split("static const struct tab_prog tab_progs[] = {")[1].split("};")[0]
        self.assertEqual(len(re.findall(r"^\s*\{ ", rows, re.M)), 5)  # shell, demo, claude, tutorial, pi: the enum's order
        self.assertRegex(rows, r'\{ TABPROG_TUTORIAL_FILE, TABPROG_TUTORIAL_CMD, "T:UP-Term-tutorial"')
        # the tab must start the next lesson: with no argument the tutorial lists and exits, and the tab closes
        self.assertIn('#define TABPROG_TUTORIAL_CMD "UP-Term:bin/vsh UP-Term:bin/tutorial next"', (ROOT / "handler/tabprog.h").read_text())
        self.assertIn("tabprog_script(script, sizeof(script), p->pre, line)", H)


class WindowStateWiring(unittest.TestCase):
    """The /state counters (plan 2026-10-19-tutorial-window-lesson P1-P4). The formatter and the bump
    rules are host-tested in tests/test_slash.c; this pins that each hook point in the Amiga-only
    handler calls them. That the hooks RUN (a real Left Amiga T, menu pick, /help, mouse copy, paste)
    only the rig can show."""

    def test_p1_each_action_has_its_bump_point(self):
        self.assertIn("slash_state_bump(&c->st, STATE_TAB_OPENED, 0, 0);\n                tab_activate(c, c->ntabs - 1);", H)
        self.assertRegex(H, r"static void tab_switch\(con \*c, int i\)\n\{\n[^}]*slash_state_bump\(&c->st, STATE_SWITCH")
        for call in ("tab_switch(c, (c->active + 1) % c->ntabs)", "tab_switch(c, (c->active + c->ntabs - 1) % c->ntabs)",
                     "tab_switch(c, im->Code - 1)", "tab_switch(c, (int)((LONG)(im->MouseX"):
            self.assertIn(call, H)
        self.assertRegex(H, r"stat_note\(c, STATE_MENU, \(LONG\)GTMENUITEM_USERDATA\(it\), 0\);\n\s*r = menu_do\(")  # menu_do runs the pick (keymap rows share it)
        self.assertRegex(H, r"slash_run\(c, line, l, ans, 4096, 1\)\) \{\n\s*slash_typed_note\(c, line, l\);")
        self.assertIn('strcmp(name, "state")', H)  # /state does not count itself
        self.assertRegex(H, r"h_clipped\(void \*u, int what\)\n\{\n\s*stat_note\(\(con \*\)u, what == VTWIN_CLIP_COPY \? STATE_COPY : STATE_PASTE")
        self.assertRegex(H, r"h_knob, h_clipped \};")
        w = (ROOT / "render/vtwin.c").read_text(encoding="latin-1")
        copy = w.split("static void copy_selection(vtwin *w)")[1].split("\n}\n")[0]
        paste = w.split("static void paste(vtwin *w)")[1].split("\n}\n")[0]
        self.assertEqual(copy.count("VTWIN_CLIP_COPY"), 1)   # once per copy, after the clipboard write
        self.assertLess(copy.index("clip_write"), copy.index("VTWIN_CLIP_COPY"))
        self.assertEqual(paste.count("VTWIN_CLIP_PASTE"), 1)  # once per paste, outside the character loop
        self.assertGreater(paste.index("VTWIN_CLIP_PASTE"), paste.index("FreeVec(text)"))

    def test_p2_host_pushes_and_tab_keeps_a_copy(self):
        self.assertIn("TM_KNOB, TM_STAT, TM_STATE };", H)
        push = H.split("static void stat_push(con *c)")[1].split("\n}\n")[0]
        self.assertIn("tab_msg(c, TM_STATE)", push)
        self.assertIn("PutMsg(c->tab_list[i]->tab_port", push)  # no wait: a PutMsg, freed by tab_replies
        self.assertRegex(H, r"case TM_STATE:\n\s*c->st = t->st;")                  # the tab's read-only copy
        self.assertRegex(H, r"case TM_STAT:\n\s*stat_note\(c, t->what, t->stat_arg, t->stat_name\);")  # a tab's action reaches the host
        note = H.split("static void stat_note(con *c, int ev, long arg, const char *name)\n{")[1].split("\n}\n")[0]
        self.assertIn("if (c->is_tab)", note)
        self.assertIn("PutMsg(c->host_pub", note)
        # every change of the tabs or the active one pushes
        act = H.split("static void tab_activate(con *c, int i)")[1].split("\n}\n")[0]
        self.assertIn("stat_push(c)", act)
        self.assertRegex(H, r"tab_insets\(c\);\n\s*stat_push\(c\); /\* tabs and active changed \*/")

    def test_p3_p4_state_is_answered_from_the_window_block(self):
        self.assertRegex(H, r"case SLASH_STATE:[^\n]*\n[^\n]*\n\s*if \(!c->is_tab\)\n\s*stat_fill\(c\);\n(?:\s*c->st\.[a-z_]+ = [^\n]*\n)*\s*return slash_state_text\(&c->st, cmd.arg, ans, cap\) < 0 \? 2 : 1;")
        # C:UPTerm sends its arguments as "/<args>", so `UPTerm state KEY` is the typed /state KEY
        # (test_slash.c parses it); an answer of 2 comes back as RETURN_ERROR (5)
        u = (ROOT / "handler/upterm.c").read_text()
        self.assertIn("line[0] = '/';", u)
        self.assertIn("return r == 1 ? RETURN_OK : RETURN_ERROR;", u)
        self.assertIn("UPTerm state [KEY]", u)

    def test_the_copy_step_asks_for_right_amiga_c_while_copy_on_select_is_off(self):
        # a drag only selects: copy_on_select is off by default and Right Amiga C copies. Lesson 12
        # said "drag over a word and it is copied at once", the copies counter never moved and the
        # step could not pass (rig 4, 2026-10-19)
        w = (ROOT / "render/vtwin.c").read_text(encoding="latin-1")
        self.assertIn("w->copy_on_select = 0;", w)
        self.assertIn("else if (w->copy_on_select && !w->no_clipboard)", w)
        lesson = (ROOT / "dist/tutorial-lessons/12-the-up-term-window.lesson").read_text()
        step = lesson.split("copies0=")[1].split("\n? ", 1)[1].split("\n?", 1)[0]
        self.assertIn("Right Amiga C", step.split("\n")[0])  # the task line
        self.assertNotIn("copied at once", lesson)

    def test_upterm_reaches_its_window_from_a_command_substitution(self):
        # the lessons check `$(UPTerm state KEY)`: vsh runs it with Input() NIL: and Output() the
        # capture pipe (shell/vsh.c, the job streams), so neither reaches the window and a tab
        # answered "not an UP-Term window" (rig 4, lesson 12, 2026-10-19). The third try is the
        # process's console, "*" at pr_ConsoleTask, which the runner inherits from vsh.
        u = (ROOT / "handler/upcmd.h").read_text()  # the route C:UPTerm and C:Agent share
        loop = u.split("    for (i = 0;")[1].split("\n    }\n")[0]
        self.assertIn("i < 3", loop)
        self.assertIn('Open((STRPTR)"*", MODE_OLDFILE)', loop)
        self.assertIn("Close(h);", loop)


if __name__ == "__main__":
    unittest.main()
