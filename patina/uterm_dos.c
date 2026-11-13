#include <exec/types.h>
#include <dos/dos.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "uterm_dos.h"
#include "../handler/tabprog.h"
#define UPASSIGN_DOS
#include "../config/upassign.h"

/* scripts of their own (T:Patina-uterm.00 .. 15): a name a shell still
 * reads refuses MODE_NEWFILE, so two windows opened close together never
 * share one, as the handler's tab scripts (T:UP-Term-tab.NN) */
#define UTERM_SCRIPTS 16

static int write_file(const char *name, const char *text)
{
    BPTR f = Open((STRPTR)name, MODE_NEWFILE);
    if (!f)
        return 0;
    FPuts(f, (STRPTR)text);
    Close(f);
    return 1;
}

static int shell_line(char *line, int cap, const char *spec)
{
    static int next;
    char text[200], name[24];
    const char *vsh = upassign_vsh(); /* UP-Term:bin/vsh, else C:vsh of an older install */
    int i;
    if (!vsh) /* no vsh: the AmigaDOS Shell, as before */
        return tabprog_window_line(line, (unsigned long)cap, spec, 0, 0, 0);
    if (!tabprog_shell_script(text, sizeof text, 0, vsh))
        return 0;
    for (i = 0; i < UTERM_SCRIPTS; i++) {
        int n = (next + i) % UTERM_SCRIPTS;
        strcpy(name, "T:Patina-uterm.");
        name[15] = (char)('0' + n / 10);
        name[16] = (char)('0' + n % 10);
        name[17] = 0;
        if (write_file(name, text)) {
            next = n + 1;
            return tabprog_window_line(line, (unsigned long)cap, spec, 0, 0, name);
        }
    }
    return 0;
}

int uterm_line(char *line, int cap, const char *spec, int prog, const char *task)
{
    const struct tab_prog *p;
    char cmd[480], script[640];
    BPTR lock;
    if (prog <= TAB_SHELL || prog >= (int)(sizeof tab_progs / sizeof tab_progs[0]))
        return shell_line(line, cap, spec);
    p = &tab_progs[prog];
    if (!(lock = Lock((STRPTR)p->file, SHARED_LOCK)))
        return -1;
    UnLock(lock);
    if (!tabprog_task_cmd(cmd, sizeof cmd, p->cmd, task) || !tabprog_script(script, sizeof script, p->pre, cmd) ||
        !write_file(p->script, script))
        return 0;
    return tabprog_window_line(line, (unsigned long)cap, spec, p->cols, p->rows, p->script);
}

const char *uterm_missing(int prog)
{
    if (prog <= TAB_SHELL || prog >= (int)(sizeof tab_progs / sizeof tab_progs[0]))
        return "";
    return tab_progs[prog].missing;
}
