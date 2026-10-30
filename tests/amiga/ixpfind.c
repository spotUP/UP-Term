/* ixpfind -- probes for ixemul's kill() pid lookup (pfind), used by
 * tools/rig/pfind_rig.py.
 *
 * ixpfind addr NAME   print the exec address of the task NAME (FindTask), decimal
 * ixpfind self        kill(getpid(), 0): must give 0
 * ixpfind wait       write getpid() to RAM:pfind.pid, sleep 60 (a live ixemul process)
 * ixpfind list       every other task: "address name" (the rig kill-tests each)
 * ixpfind native NAME kill(<address of the native task NAME>, 0) inside this
 *                     process: must give -1/ESRCH at once (a native task is no
 *                     ixemul process); pfind used to walk off into the task's
 *                     tc_TrapData and hang */
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <proto/exec.h>

long __stack = 65536;
const char stackcookie[] = "$STACK:65536\n";

int main(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "";
    struct Task *t;
    if (!strcmp(m, "self")) {
        int r = kill(getpid(), 0);
        printf("self: kill rc %d errno %d\n", r, r ? errno : 0);
        return r == 0 ? 0 : 1;
    }
    if (!strcmp(m, "wait")) { /* a live ixemul process for a second process to signal */
        FILE *f = fopen("RAM:pfind.pid", "w");
        if (f) { fprintf(f, "%lu\n", (unsigned long)getpid()); fclose(f); }
        sleep(60);
        return 0;
    }
    if (!strcmp(m, "list")) { /* every task in TaskWait and TaskReady except this one: "address name" */
        struct Node *n;
        struct Task *all[64], *me = FindTask(0);
        int i, k = 0;
        Forbid();
        for (n = SysBase->TaskWait.lh_Head; n->ln_Succ && k < 64; n = n->ln_Succ)
            if (n != &me->tc_Node) all[k++] = (struct Task *)n;
        for (n = SysBase->TaskReady.lh_Head; n->ln_Succ && k < 64; n = n->ln_Succ)
            if (n != &me->tc_Node) all[k++] = (struct Task *)n;
        Permit();
        for (i = 0; i < k; i++)
            printf("%lu %s\n", (unsigned long)all[i], all[i]->tc_Node.ln_Name ? all[i]->tc_Node.ln_Name : "?");
        return 0;
    }
    if (argc > 2 && (!strcmp(m, "addr") || !strcmp(m, "native"))) {
        Forbid();
        t = FindTask((STRPTR)argv[2]);
        Permit();
        if (!t) { printf("none\n"); return 5; }
        if (!strcmp(m, "addr")) { printf("%lu\n", (unsigned long)t); return 0; }
        {
            int r = kill((pid_t)t, 0);
            printf("native: kill rc %d errno %d (ESRCH %d)\n", r, r ? errno : 0, ESRCH);
            return (r == -1 && errno == ESRCH) ? 0 : 1;
        }
    }
    return 20;
}
