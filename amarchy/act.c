#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "act.h"
#include "../claude/util.h"
#include "../handler/frontscr.h"
#include "leaf.h"

/* Clone the path list of this process's CLI (a chain of {BPTR next; BPTR lock}
 * nodes) for the child, which owns and frees it. 0 when there is no CLI. */
static BPTR dup_path(void)
{
    struct CommandLineInterface *cli = Cli();
    BPTR head = 0, *tail = &head, p;
    if (!cli)
        return 0;
    for (p = cli->cli_CommandDir; p; ) {
        BPTR *src = (BPTR *)BADDR(p);
        BPTR *n = (BPTR *)AllocVec(2 * sizeof(BPTR), MEMF_PUBLIC | MEMF_CLEAR);
        if (!n)
            break;
        n[1] = DupLock(src[1]);
        *tail = MKBADDR(n);
        tail = &n[0];
        p = src[0];
    }
    return head;
}

static void free_path(BPTR p)
{
    while (p) {
        BPTR *n = (BPTR *)BADDR(p);
        BPTR next = n[0];
        if (n[1])
            UnLock(n[1]);
        FreeVec(n);
        p = next;
    }
}

int act_run(const char *cmd)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    BPTR path = dup_path();
    LONG rc;
    if (!cmd || !*cmd)
        return 0;
    rc = SystemTags((STRPTR)cmd, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                    NP_StackSize, 32768, path ? NP_Path : TAG_IGNORE, path, TAG_END);
    if (rc == -1) {                 /* not started: the files and path are still ours */
        if (in)
            Close(in);
        if (out)
            Close(out);
        free_path(path);
        return 0;
    }
    return 1;
}

/* AMB3: a window opens on the public screen in front when the hotkey fired (a DOpus screen, say), not on
 * the default one. out gets spec plus /SCREEN name; a front screen without a public name leaves the spec
 * as it is. */
static void spec_on_front(char *out, int cap, const char *spec)
{
    char name[64];
    if (!front_screen_name(name, sizeof name))
        name[0] = 0;
    if (!lf_screen_spec(out, cap, spec, name))
        cl_copy(out, spec, cap);
}

/* Like act_run, but the command's input and output are a new XCON: window (the quick menu and
 * the key viewer draw on a terminal). The window is borderless, centred, in front, sized
 * cols x rows cells, and closes when the command ends or when it is deactivated. */
int act_in_window(const char *cmd, int cols, int rows)
{
    char spec[96], full[160], num[12];
    BPTR w, path;
    LONG rc;
    cl_copy(spec, "XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS ", sizeof(spec));
    cl_ltoa(cols, num);
    cl_cat(spec, num, sizeof(spec));
    cl_cat(spec, "x", sizeof(spec));
    cl_ltoa(rows, num);
    cl_cat(spec, num, sizeof(spec));
    spec_on_front(full, sizeof full, spec);
    w = Open((STRPTR)full, MODE_NEWFILE);
    path = dup_path();
    if (!cmd || !*cmd || !w) {
        if (w)
            Close(w);
        free_path(path);
        return 0;
    }
    rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE,
                    NP_StackSize, 65536, path ? NP_Path : TAG_IGNORE, path, TAG_END);
    if (rc == -1) {
        Close(w);
        free_path(path);
        return 0;
    }
    return 1;
}

int act_rexx(const char *arg)
{
    char line[160];
    int i = 0;
    const char *p = "RX \"";
    while (*p && i < (int)sizeof(line) - 2)
        line[i++] = *p++;
    for (p = arg; p && *p && i < (int)sizeof(line) - 2; p++)
        line[i++] = *p == '"' ? '\'' : *p;
    line[i++] = '"';
    line[i] = 0;
    return act_run(line);
}

int act_open_uterm(void)
{
    char spec[128], line[160];
    spec_on_front(spec, sizeof spec, LF_UTERM_SPEC);
    cl_copy(line, "NewShell \"", sizeof line);
    cl_cat(line, spec, sizeof line);
    cl_cat(line, "\"", sizeof line);
    return act_run(line);
}
