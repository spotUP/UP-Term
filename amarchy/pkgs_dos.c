/* pkgs_dos: see pkgs_dos.h. */
#include <stdio.h>
#include <stdlib.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "pkgs_dos.h"
#include "pkgs_core.h"
#include "leaf.h"
#include "proclaunch.h"
#define UPASSIGN_DOS
#include "../config/upassign.h"

#define PK_CMD "upkg list-installed --tsv"
#define PK_TEXT_MAX (64L * 1024L)

/* The upkg output in buf (cap bytes); its length, -1 when upkg could not be run. */
static long pk_dos_read(char *buf, long cap)
{
    char tmp[48], line[512], prog[256];
    BPTR f;
    LONG rc;
    long got = -1;
    snprintf(tmp, sizeof tmp, "T:amarchy-pkgs.%lx", (unsigned long)FindTask(0));   /* one per task */
    if (!lf_shell_command(line, sizeof line, upassign_vsh(), PK_CMD))
        return -1;
    if (!(f = Open((STRPTR)tmp, MODE_NEWFILE)))
        return -1;
    rc = pl_run(line, lf_command_program(line, prog, (int)sizeof prog) ? prog : 0, 32768, f);
    Close(f);
    if (rc == 0 && (f = Open((STRPTR)tmp, MODE_OLDFILE))) {
        got = Read(f, buf, cap);
        Close(f);
    }
    DeleteFile((STRPTR)tmp);
    return got;
}

int pk_dos_load_rows(ix_index *x)
{
    static pk_list l;
    const char *labels[PK_MAX], *launches[PK_MAX];
    char *buf = malloc(PK_TEXT_MAX);
    long len = buf ? pk_dos_read(buf, PK_TEXT_MAX) : -1;
    int i;
    l.n = 0;
    if (len > 0)
        pk_parse(buf, len, &l);
    free(buf);
    for (i = 0; i < l.n; i++) {
        labels[i] = l.name[i];
        launches[i] = l.launch[i];
    }
    return ix_add_pkgs(x, labels, launches, l.n);
}
