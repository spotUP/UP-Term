/* Running a leaf (leaf.h). A gcc/ixemul part of amarchy-menu and amarchy-gui, no libupkgui. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include "leaf.h"
#include "../handler/upreq.h"

/* run: gets what the commodity's open:uterm gives a shell, an UP-Term window of its own, as the
 * program's input and output. system() would run it in the menu's own XCON: window (AMB1: nvim
 * opened inside the OSD), and the window closes when the menu leaves. Asynchronous, so the
 * menu's process does not wait for it and is not its parent console. */
static int run_in_uterm(const char *cmd)
{
    BPTR w;
    LONG rc;
    if (!cmd || !*cmd)
        return 0;
    w = Open((STRPTR)LF_UTERM_SPEC, MODE_NEWFILE);
    if (!w) {
        fprintf(stderr, "amarchy: no UP-Term window for %s\n", cmd);
        return UPREQ_NOWINDOW;
    }
    rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE,
                    NP_StackSize, 65536, TAG_END);
    if (rc == -1) {                 /* not started: the window is still ours */
        Close(w);
        return UPREQ_FAILED;
    }
    return 0;
}

static int run_request(const km_menu *m, const lf_ui *ui)
{
    /* tab:, menuid: and slash: belong to a terminal window: the handler's request port
     * (handler/upreq.h) takes the action by name and runs it as a key binding would */
    upreq_body r;
    long rc;
    if (!upreq_fill(&r, m->kind, m->arg)) {
        fprintf(stderr, "amarchy: %s is not a terminal action\n", m->arg);
        return UPREQ_UNKNOWN;
    }
    rc = upreq_send(&r);
    if (rc == UPREQ_NEEDARG) {
        /* /bg and the like take a value the row does not carry: ask for it, send again */
        char value[UPREQ_ACTION], act[UPREQ_ACTION];
        strcpy(act, r.action);
        if (!ui->ask_value || !ui->ask_value(ui->ctx, m->label, r.answer, value, sizeof value))
            return 0;                      /* Escape: nothing asked of the terminal */
        if (!upreq_with_arg(&r, act, value)) {
            strcpy(r.answer, "the value is too long");
            rc = UPREQ_FAILED;
        } else
            rc = upreq_send(&r);
    }
    if (rc && ui->refused)
        ui->refused(ui->ctx, m->label, rc, r.answer);
    return (int)rc;
}

int lf_run(const km_menu *m, const lf_ui *ui)
{
    switch (lf_plan(m->kind)) {
    case LF_WINDOW:
        return run_in_uterm(m->arg);
    case LF_REXX: {
        char line[160];
        snprintf(line, sizeof line, "RX \"%s\"", m->arg);
        return system(line);
    }
    case LF_OPEN:
        return system("NewShell \"" LF_UTERM_SPEC "\"");
    default:
        return run_request(m, ui);
    }
}
