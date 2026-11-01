/* jobprobe: RJ.1 proof of the job runner (amarchy/job_dos.c). Runs each command given on the command line (default:
 * `Echo one`, then `Search NOFILE x`) through jb_start, polls until the end marker or 15 s, and prints every jc
 * line, the state and the return code. Build: make build/amiga/jobprobe. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "../../../amarchy/job_core.h"
#include "../../../amarchy/job_dos.h"

static jc j;

static int run(const char *cmd)
{
    int i, n, shown = 0, polls = 0;
    char foot[JOB_FOOT_MAX + 16];
    printf("== %s\n", cmd);
    if (!jb_start(&j, cmd, 0, "probe")) {
        printf("jb_start failed\n");
        return 0;
    }
    while (!j.ended && polls < 75) {       /* 75 x 200 ms ticks = 15 s */
        ULONG m = jb_sigmask();
        if (m)
            Wait(m);
        polls++;
        jb_poll(&j);
    }
    jb_poll(&j);
    n = jc_count(&j);
    for (i = 0; i < n; i++)
        printf("line %d: [%s]\n", i, jc_line(&j, i)), shown++;
    jc_footer(&j, foot, sizeof foot);
    printf("ended=%d state=%d rc=%d footer=[%s] polls=%d lines=%d\n", j.ended, j.state, j.rc, foot, polls, shown);
    jb_end();
    return 1;
}

int main(int argc, char **argv)
{
    int i;
    if (argc < 2) {
        run("Echo one");
        run("Search NOFILE x");
    } else
        for (i = 1; i < argc; i++)
            run(argv[i]);
    return 0;
}
