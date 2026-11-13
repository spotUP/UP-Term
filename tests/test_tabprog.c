/* handler/tabprog.h: a program tab's script, and the window it needs. */
#include <string.h>
#include "harness.h"
#include "../handler/tabprog.h"

static void a_failed_demo_keeps_its_tab_open_until_return(void)
{
    char s[256];
    CHECK(tabprog_script(s, sizeof(s), "", "C:UPDemo"));
    CHECK(strstr(s, "FailAt 2147483647\nC:UPDemo\n") == s);
    /* UPDemo's "the window is WxH, it needs 76x20" (rc 10) stays readable */
    CHECK(strstr(s, "C:UPDemo\nIf WARN\nAsk \"Press Return to close this tab.\"\nEndIf\nEndCLI >NIL:\n") != 0);
}

static void claude_tab_keeps_its_stack_line(void)
{
    char s[256];
    CHECK(tabprog_script(s, sizeof(s), "Stack 32768\n", "C:Claude"));
    CHECK(strstr(s, "FailAt 2147483647\nStack 32768\nC:Claude\nIf WARN\n") == s);
}

static void agent_names_pick_their_tab(void)
{
    CHECK_INT(tabprog_agent_tab("claude"), TAB_CLAUDE);
    CHECK_INT(tabprog_agent_tab("pi"), TAB_PI);
    CHECK_INT(tabprog_agent_tab("PI"), TAB_PI);
    CHECK_INT(tabprog_agent_tab(""), TAB_CLAUDE);
    CHECK_INT(tabprog_agent_tab("bogus"), TAB_CLAUDE);
}

/* /ask TASK and C:Agent TASK: the agent's command line carries the task as one quoted argument; both
 * agents read a positional prompt (claude/main_amiga.c header "Claude [options] [prompt]", amiga-pi args.c) */
static void agent_task_is_one_quoted_argument(void)
{
    char l[200], s[300];
    CHECK(tabprog_task_cmd(l, sizeof(l), "C:Claude", "fix \"the\" bug"));
    CHECK_STR(l, "C:Claude \"fix *\"the*\" bug\"");
    CHECK(tabprog_task_cmd(l, sizeof(l), "C:Claude", "a\nb *x*")); /* newline *N, * doubled: vsh_put_word, shared with vsh_command_line */
    CHECK_STR(l, "C:Claude \"a*Nb **x**\"");
    CHECK(tabprog_task_cmd(l, sizeof(l), "C:amiga-pi", "list the files"));
    CHECK_STR(l, "C:amiga-pi \"list the files\"");
    CHECK(tabprog_script(s, sizeof(s), "Stack 32768\n", l));
    CHECK(strstr(s, "Stack 32768\nC:amiga-pi \"list the files\"\n") != 0);
    CHECK(tabprog_task_cmd(l, sizeof(l), "C:Claude", 0)); /* no task: the bare agent, as Super + L */
    CHECK_STR(l, "C:Claude");
    CHECK(tabprog_task_cmd(l, sizeof(l), "C:Claude", ""));
    CHECK_STR(l, "C:Claude");
    CHECK(!tabprog_task_cmd(l, 12, "C:Claude", "too long for it")); /* refused, never cut */
}

static void tutorial_tab_runs_the_tutorial_under_vsh(void)
{
    char s[256];
    /* Help > Shell tutorial and /tutorial (MENU_TUTORIAL) spawn this tab */
    CHECK(tabprog_script(s, sizeof(s), "", TABPROG_TUTORIAL_CMD));
    CHECK_STR(s, "FailAt 2147483647\nUP-Term:bin/vsh UP-Term:bin/tutorial next\n"
                 "If WARN\nAsk \"Press Return to close this tab.\"\nEndIf\nEndCLI >NIL:\n");
    CHECK_STR(TABPROG_TUTORIAL_FILE, "UP-Term:bin/tutorial");
}

static void a_script_too_long_for_the_buffer_is_refused(void)
{
    char s[40];
    CHECK(!tabprog_script(s, sizeof(s), "", "C:UPDemo"));
}

static void a_small_window_grows_to_the_demo_and_the_tab_bar(void)
{
    int c, r;
    /* 60x18, no tab bar yet (it takes 2 rows when the demo's tab opens) */
    CHECK(tabprog_grow(60, 18, 76, 20, 2, &c, &r));
    CHECK_INT(c, 76);
    CHECK_INT(r, 22);
    /* 80x20 with no bar: wide enough, but the bar would leave 18 rows */
    CHECK(tabprog_grow(80, 20, 76, 20, 2, &c, &r));
    CHECK_INT(c, 80);
    CHECK_INT(r, 22);
}

static void a_big_window_is_left_alone(void)
{
    int c, r;
    CHECK(!tabprog_grow(80, 48, 76, 20, 2, &c, &r));
    CHECK(!tabprog_grow(76, 20, 76, 20, 0, &c, &r)); /* the bar is up already */
    CHECK_INT(c, 76);
    CHECK_INT(r, 20);
    CHECK(!tabprog_grow(40, 10, 0, 0, 0, &c, &r));   /* Claude: no size */
}

/* s77: open:uterm ran a bare NewShell, the AmigaDOS Shell with no vsh banner; a Patina window now starts as a
 * shell tab does, vsh from its FROM script, and the window closes with vsh */
static void a_new_uterm_window_runs_vsh_from_its_script(void)
{
    char text[200], line[256];
    CHECK(tabprog_shell_script(text, sizeof text, 0, "UP-Term:bin/vsh"));
    CHECK_STR(text, "FailAt 2147483647\nUP-Term:bin/vsh\nEndCLI >NIL:\n");
    CHECK(tabprog_shell_script(text, sizeof text, "Work:", 0));
    CHECK_STR(text, "FailAt 2147483647\nCD \"Work:\"\n");          /* no vsh: the AmigaDOS Shell stays */
    CHECK(!tabprog_shell_script(text, 20, 0, "UP-Term:bin/vsh"));
    CHECK(tabprog_window_line(line, sizeof line, "XCON:0/20/640/300/UP-Term/CLOSE", 0, 0, "T:Patina-uterm.00"));
    CHECK_STR(line, "NewShell \"XCON:0/20/640/300/UP-Term/CLOSE\" FROM T:Patina-uterm.00");
    CHECK(tabprog_window_line(line, sizeof line, "XCON:x", 0, 0, 0));
    CHECK_STR(line, "NewShell \"XCON:x\"");
    CHECK(!tabprog_window_line(line, 20, "XCON:0/20/640/300/UP-Term/CLOSE", 0, 0, 0));
}

/* s77: the Demo tour in a window of its own opens at the demo's grid, as its tab grows to it */
static void a_program_window_opens_at_its_grid(void)
{
    char line[256];
    CHECK(tabprog_window_line(line, sizeof line, "XCON:x", tab_progs[TAB_DEMO].cols, tab_progs[TAB_DEMO].rows,
                              tab_progs[TAB_DEMO].script));
    CHECK_STR(line, "NewShell \"XCON:x/CELLS 76x20\" FROM T:UP-Term-demo");
    CHECK(tabprog_window_line(line, sizeof line, "XCON:x", 120, 5, 0));
    CHECK_STR(line, "NewShell \"XCON:x/CELLS 120x5\"");
}

/* s77: UP-Term > Claude with no terminal open opened a shell tab and then a Claude tab; the menu item names
 * the one program its window runs, the same table as the handler's tabs */
static void menu_items_name_their_program(void)
{
    CHECK_INT(tabprog_menu_tab(MENU_CLAUDE, 0), TAB_CLAUDE);
    CHECK_INT(tabprog_menu_tab(MENU_PI, 0), TAB_PI);
    CHECK_INT(tabprog_menu_tab(MENU_DEMO, 0), TAB_DEMO);
    CHECK_INT(tabprog_menu_tab(MENU_TUTORIAL, 0), TAB_TUTORIAL);
    CHECK_INT(tabprog_menu_tab(MENU_AGENT, "pi"), TAB_PI);
    CHECK_INT(tabprog_menu_tab(MENU_AGENT, 0), TAB_CLAUDE);
    CHECK_INT(tabprog_menu_tab(MENU_COPY, 0), TAB_SHELL);
    CHECK_STR(tab_progs[TAB_CLAUDE].pre, "Stack 32768\n");
}

void suite_tabprog(void)
{
    a_failed_demo_keeps_its_tab_open_until_return();
    claude_tab_keeps_its_stack_line();
    agent_names_pick_their_tab();
    agent_task_is_one_quoted_argument();
    tutorial_tab_runs_the_tutorial_under_vsh();
    a_script_too_long_for_the_buffer_is_refused();
    a_small_window_grows_to_the_demo_and_the_tab_bar();
    a_big_window_is_left_alone();
    a_new_uterm_window_runs_vsh_from_its_script();
    a_program_window_opens_at_its_grid();
    menu_items_name_their_program();
}
