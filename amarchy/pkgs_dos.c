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

#define PK_CMD "upkg list --tsv"
#define PK_TEXT_MAX (1536L * 1024L)   /* about 4000 rows of up to 380 bytes (PK_ROWS_MAX) */

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
    pk_rows l;
    char *buf = malloc(PK_TEXT_MAX);
    long len = buf ? pk_dos_read(buf, PK_TEXT_MAX) : -1;
    l.n = l.over = 0;
    l.r = 0;
    if (len > 0)
        pk_parse_all(buf, len, &l);
    free(buf);
    ix_add_pkg_rows(x, &l);                  /* a failed upkg stores an empty list: the views show their one note */
    return 1;
}
