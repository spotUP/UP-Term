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
    got = vsh_command_line(argv);
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
    got = vsh_command_line(argv);
    CHECK_STR(got, "TO=T:t1 URL=\"https://x?a=1&b=2\" QUIET");
    free(got);
}

void suite_cmdline(void)
{
    keywords();
    plain();
    several();
}
