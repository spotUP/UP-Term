/* ps -- the tasks and processes of the machine (Unix tool ports 3.3): CLI processes first with the command they
 * run, then the other tasks by name; their priority and state. No CPU column: that is top's (a sampler). */
#include <stdio.h>
#include <string.h>
#include "proc_core.h"
#include "tasks_amiga.h"
#include "../tty/qmark.h"

/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 8192;
const char ps_stack_cookie[] = "$STACK: 8192";

#define MAX_TASKS 256

int main(int argc, char **argv)
{
    static pc_task t[MAX_TASKS];
    char line[160];
    int n, i;
    if (qmark_argv(argc, argv)) { /* `ps ?`, `ps -h` and `ps --help` show the usage and end */
        printf("usage: ps\nLists the tasks and processes: CLI number, priority, state, type, name\n"
               "(a CLI process shows the command it runs). top shows their CPU use.\n");
        return 0;
    }
    n = pa_snapshot(t, MAX_TASKS);
    pc_sort_ps(t, n);
    pc_ps_header(line, sizeof(line), 0);
    printf("%s\n", line);
    for (i = 0; i < n; i++) {
        pc_ps_row(line, sizeof(line), &t[i], 0);
        printf("%s\n", line);
    }
    return 0;
}
