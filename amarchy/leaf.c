/* Running a leaf (leaf.h). A gcc/ixemul part of amarchy-menu and amarchy-gui, no libupkgui. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include "leaf.h"
#include "wblaunch.h"
#include "termenv.h"
#include "../handler/upreq.h"
#include "../handler/frontscr.h"
#define UPASSIGN_DOS
#include "../config/upassign.h"

/* LF_UTERM_SPEC on the public screen that is in front now: the menu's own window opened there (AMB3). A
 * front screen without a public name leaves the default public screen. */
static void uterm_spec(char *out, int cap)
{
    char name[64];
    if (!front_screen_name(name, sizeof name))
        name[0] = 0;
    if (!lf_screen_spec(out, cap, LF_UTERM_SPEC, name))
        strcpy(out, LF_UTERM_SPEC);
}

/* run: gets what the commodity's open:uterm gives a shell, an UP-Term window of its own, as the
 * program's input and output. system() would run it in the menu's own XCON: window (AMB1: nvim
 * opened inside the OSD), and the window closes when the menu leaves. Asynchronous, so the
 * menu's process does not wait for it and is not its parent console. */
static int run_in_uterm(const char *cmd, const lf_ui *ui)
{
    BPTR w;
    LONG rc;
    char spec[128], line[512];
    if (!cmd || !*cmd)
        return 0;
    if (!lf_shell_command(line, sizeof line, upassign_vsh(), cmd)) {
        fprintf(stderr, "amarchy: %s is not a command line\n", cmd);
        if (ui && ui->refused)
            ui->refused(ui->ctx, cmd, UPREQ_FAILED, "the command line is not valid");
        return UPREQ_FAILED;
    }
    termenv_ensure();
    uterm_spec(spec, sizeof spec);
    w = Open((STRPTR)spec, MODE_NEWFILE);
    if (!w) {
        fprintf(stderr, "amarchy: no UP-Term window for %s\n", cmd);
        if (ui && ui->refused)
            ui->refused(ui->ctx, cmd, UPREQ_NOWINDOW, "");
        return UPREQ_NOWINDOW;
    }
    rc = SystemTags((STRPTR)line, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE,
                    NP_StackSize, 65536, TAG_END);
    if (rc == -1) {                 /* not started: the window is still ours */
        Close(w);
        if (ui && ui->refused)
            ui->refused(ui->ctx, cmd, UPREQ_FAILED, "it could not be started");
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

int lf_open_index(const char *label, const char *path, const lf_ui *ui)
{
    char err[160];
    if (wbl_open(path, err, sizeof err) == 0)
        return 0;
    fprintf(stderr, "amarchy: %s: %s\n", label, err);
    if (ui && ui->refused)
        ui->refused(ui->ctx, label, UPREQ_FAILED, err);
    return UPREQ_FAILED;
}

/* detach: the program runs on its own, with NIL: as input and output and no window anywhere */
static int run_detached(const km_menu *m, const lf_ui *ui)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE), out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    LONG rc = SystemTags((STRPTR)m->arg, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                         NP_StackSize, 32768, TAG_END);
    if (rc == -1) {                 /* not started: the files are still ours */
        if (in)
            Close(in);
        if (out)
            Close(out);
        if (ui && ui->refused)
            ui->refused(ui->ctx, m->label, UPREQ_FAILED, "it could not be started");
        return UPREQ_FAILED;
    }
    return 0;
}

int lf_run(const km_menu *m, const lf_ui *ui)
{
    switch (lf_plan(m->kind)) {
    case LF_DETACH:
        return run_detached(m, ui);
    case LF_WINDOW:
        return run_in_uterm(m->arg, ui);
    case LF_REXX: {
        char line[160];
        snprintf(line, sizeof line, "RX \"%s\"", m->arg);
        return system(line);
    }
    case LF_OPEN: {
        char spec[128], line[160];
        uterm_spec(spec, sizeof spec);
        snprintf(line, sizeof line, "NewShell \"%s\"", spec);
        return system(line);
    }
    default:
        return run_request(m, ui);
    }
}
