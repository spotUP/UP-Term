#include <stdlib.h>
#include "harness.h"
#include "../shell/vsh_cmdline.h"

/* one argument -> the command line vsh hands a native command */
static void one(const char *arg, const char *want)
{
    char *argv[2];
    char *got;
    argv[0] = (char *)arg;
    argv[1] = 0;
    got = vsh_command_line(argv, 0);
    CHECK_STR(got, want);
    free(got);
}

/* ReadArgs matches a keyword only on an unquoted item: `aget TO=T:t1 URL="..."` (AmigaShell's own form) */
static void keywords(void)
{
    one("TO=T:t1", "TO=T:t1");
    one("URL=https://x?a=1&b=2", "URL=\"https://x?a=1&b=2\"");
    one("x=y z", "x=\"y z\"");
    one("-D=1", "-D=1");
    one("--opt=val", "--opt=val");
    one("FROM/A=x", "FROM/A=x");
    one("KEY=", "KEY=\"\"");
}

/* a word whose prefix is no keyword stays one quoted item */
static void plain(void)
{
    one("a b=c", "\"a b=c\"");
    one("=x", "\"=x\"");
    one("hello", "hello");
    one("", "\"\"");
    one("a b", "\"a b\"");
    one("say \"hi\"", "\"say *\"hi*\"\"");
    one("p\np", "\"p*Np\"");
    one("a*b", "\"a**b\"");
}

static void several(void)
{
    char *argv[4];
    char *got;
    argv[0] = "TO=T:t1";
    argv[1] = "URL=https://x?a=1&b=2";
    argv[2] = "QUIET";
    argv[3] = 0;
    got = vsh_command_line(argv, 0);
    CHECK_STR(got, "TO=T:t1 URL=\"https://x?a=1&b=2\" QUIET");
    free(got);
}

/* S7b-1b: a word quoted in the source is one quoted item, never a bare KEY= keyword (`Echo "rc=0"`) */
static void quoted_words(void)
{
    char *argv[5];
    char *got;
    char q[5] = { 0, 1, 0, 1, 0 };
    argv[0] = "Echo";
    argv[1] = "rc=0";
    argv[2] = "TO=T:t1";
    argv[3] = "URL=https://x";
    argv[4] = 0;
    got = vsh_command_line(argv, q);
    CHECK_STR(got, "Echo \"rc=0\" TO=T:t1 \"URL=https://x\"");
    free(got);
    got = vsh_command_line(argv, 0);   /* no flags: every KEY=value stays a keyword */
    CHECK_STR(got, "Echo rc=0 TO=T:t1 URL=https://x");
    free(got);
}

/* S7c-1: `Echo "<x>"` must reach C:Echo as text; an unquoted < or > on the command line is the Shell's redirection
 * ("Echo: unable to open redirection file"). */
static void angle_brackets_stay_text(void)
{
    one("<x>", "\"<x>\"");
    one(">y", "\">y\"");
    one("a>b", "\"a>b\"");
}

void suite_cmdline(void)
{
    angle_brackets_stay_text();
    quoted_words();
    keywords();
    plain();
    several();
}
