/* pi_search_drv -- amiga-pi's grep and find (claude/search_pi.c) on the
 * host for tests/test_pi_search.py, which checks them against rg and git:
 *   pi_search_drv grep|find '<pi arguments as JSON>'
 * runs in the current directory and prints the rows (or the error, exit 1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "claude/tools.h"
#include "claude/sys_posix.h"

int main(int argc, char **argv)
{
    sys_posix sp;
    cl_sys sys;
    cl_tools t;
    pi_rows r;
    char cwd[512];
    int e;
    if (argc != 3 || !getcwd(cwd, sizeof(cwd)))
        return 2;
    sys_posix_init(&sp, &sys);
    memset(&t, 0, sizeof(t));
    t.sys = &sys;
    strcpy(t.root, cwd);
    strcpy(t.cwd, cwd);
    t.pi_rules = 1;
    if (tools_init(&t))
        return 2;
    e = !strcmp(argv[1], "grep") ? tools_pi_grep(&t, argv[2], (long)strlen(argv[2]), &r)
                                 : tools_pi_find(&t, argv[2], (long)strlen(argv[2]), &r);
    fwrite(r.text.p ? r.text.p : "", 1, (size_t)r.text.n, stdout);
    printf("\n");
    jw_free(&r.text);
    tools_free(&t);
    return e;
}
