/* ps -- the tasks and processes of the machine (Unix tool ports 3.3): CLI processes first with the command they
 * run, then the other tasks by name; their priority and state. No CPU column: that is top's (a sampler). */
#include <stdio.h>
#include <string.h>
#include "proc_core.h"
#include "tasks_amiga.h"

#define MAX_TASKS 256

int main(int argc, char **argv)
{
    static pc_task t[MAX_TASKS];
    char line[160];
    int n, i;
    if (argc > 1 && (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") || !strcmp(argv[1], "?"))) {
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
