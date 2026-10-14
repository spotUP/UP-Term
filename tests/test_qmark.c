/* tty/qmark.h: the lone question mark every Amiga command answers with its
 * template. The argument string is what dos.library's GetArgStr() returns:
 * the text after the command name, a newline at its end. */
#include "harness.h"
#include "../tty/qmark.h"

void suite_qmark(void);
void suite_qmark(void)
{
    char *one[] = { "ps", "?", 0 };
    char *two[] = { "ps", "?", "x", 0 };
    char *word[] = { "ps", "??", 0 };
    char *none[] = { "ps", 0 };
    char *dash[] = { "ps", "-?", 0 };
    char *empty[] = { "ps", "", 0 };

    /* the cases AmigaDOS hands over */
    CHECK(qmark_line("?"));
    CHECK(qmark_line("?\n"));
    CHECK(qmark_line(" ?\n"));
    CHECK(qmark_line("  ? \n"));
    CHECK(qmark_line("\t?\t\n"));
    CHECK(qmark_line("?\r\n"));
    /* not a lone ?: the command parses these itself */
    CHECK(!qmark_line(""));
    CHECK(!qmark_line("\n"));
    CHECK(!qmark_line("   \n"));
    CHECK(!qmark_line("x ?\n"));
    CHECK(!qmark_line("? x\n"));
    CHECK(!qmark_line("?x\n"));
    CHECK(!qmark_line("??\n"));
    CHECK(!qmark_line("\"?\"\n"));
    CHECK(!qmark_line("FILES=?\n"));
    CHECK(!qmark_line(0));

    CHECK(qmark_argv(2, one));
    CHECK(!qmark_argv(3, two));
    CHECK(!qmark_argv(2, word));
    CHECK(!qmark_argv(2, dash));
    CHECK(!qmark_argv(2, empty));
    CHECK(!qmark_argv(1, none));
    CHECK(!qmark_argv(0, none));
    CHECK(!qmark_argv(2, 0));
}
