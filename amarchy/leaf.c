/* Running a leaf (leaf.h). A gcc/ixemul part of amarchy-menu and amarchy-gui, no libupkgui. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <rexx/storage.h>
#include <rexx/rxslib.h>
#include <proto/rexxsyslib.h>
#include "leaf.h"
#include "wblaunch.h"
#include "clips_core.h"
#include "../handler/clip.h"
#include "termenv.h"
#include "../handler/upreq.h"
#include "../handler/upactive.h"
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

/* The row's command line run by the UP-Term shell, so a bare name finds UP-Term:bin on vsh's PATH
 * (UP-Term:bin is not on the AmigaDOS Path, dist/install.dos). 0 after a refusal under label. */
static int shell_line(char *line, int cap, const char *cmd, const char *label, const lf_ui *ui)
{
    if (lf_shell_command(line, cap, upassign_vsh(), cmd))
        return 1;
    fprintf(stderr, "amarchy: %s is not a command line\n", cmd);
    if (ui && ui->refused)
        ui->refused(ui->ctx, label, UPREQ_FAILED, "the command line is not valid");
    return 0;
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
    if (!shell_line(line, sizeof line, cmd, cmd, ui))
        return UPREQ_FAILED;
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
    rc = upreq_send(&r, ui->self_in_mru);
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
            rc = upreq_send(&r, ui->self_in_mru);
    }
    if (rc && ui->refused)
        ui->refused(ui->ctx, m->label, rc, r.answer);
    return (int)rc;
}

int lf_open_index(const char *label, const char *path, const lf_ui *ui)
{
    char err[160];
    int ci = lf_clip_entry(path);
    if (ci != -2) {                     /* a clipboard history row: its text goes back on the clipboard */
        clip_ring r;
        int ok = 0;
        memset(&r, 0, sizeof r);
        clips_load_file(&r, CLIPS_PATH);
        if (ci >= 0 && ci < r.n)
            ok = clip_write(r.text[ci], r.len[ci]);
        clips_free(&r);
        if (ci >= 0 && !ok && ui && ui->refused)
            ui->refused(ui->ctx, label, UPREQ_FAILED, "the clipboard could not be written");
        return ci < 0 || ok ? 0 : UPREQ_FAILED;
    }
    if (wbl_open(path, err, sizeof err) == 0)
        return 0;
    fprintf(stderr, "amarchy: %s: %s\n", label, err);
    if (ui && ui->refused)
        ui->refused(ui->ctx, label, UPREQ_FAILED, err);
    return UPREQ_FAILED;
}

/* detach: the program runs on its own, with NIL: as input and output and no window anywhere. Through
 * the UP-Term shell like run:, so detach:amarchy-index finds the indexer. */
static int start_detached(const char *line, const km_menu *m, const lf_ui *ui)
{
    BPTR in, out;
    LONG rc;
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    rc = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
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

static int run_detached(const km_menu *m, const lf_ui *ui)
{
    char line[512];
    if (!shell_line(line, sizeof line, m->arg, m->label, ui))
        return UPREQ_FAILED;
    return start_detached(line, m, ui);
}

/* viewer: amarchy-gui by its full path from the directory this program was loaded from (the commodity's own
 * launch, amarchy.c dir_resolve + cx_helper_cmd), no shell and no PATH. */
static int run_viewer(const km_menu *m, const lf_ui *ui)
{
    char dir[128], line[256];
    BPTR l = GetProgramDir();
    if (!l || !NameFromLock(l, (STRPTR)dir, sizeof dir) || !lf_viewer_command(line, sizeof line, dir)) {
        if (ui && ui->refused)
            ui->refused(ui->ctx, m->label, UPREQ_FAILED, "the key viewer is not beside this program");
        return UPREQ_FAILED;
    }
    return start_detached(line, m, ui);
}

struct RxsLib *RexxSysBase;   /* opened around each win: send (send_win) */

/* win:<verb>: the commodity does it (amarchy-gui's own window is the active one, the owner's target is the
 * commodity's ticket): send "WIN <verb>" to the ARexx port AMARCHY and wait for the reply. -1 and a line through
 * ui->refused when the port is absent or the verb is refused. */
static int send_win(const km_menu *m, const lf_ui *ui)
{
    struct MsgPort *reply, *port;
    struct RexxMsg *rm;
    struct Library *rxs;
    char line[KM_ID + 16];
    long rc = -1;
    const char *why = "the Amarchy commodity is not running";
    snprintf(line, sizeof line, "WIN %s", m->arg);
    rxs = OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    reply = rxs ? CreateMsgPort() : 0;
    if (reply) {
        RexxSysBase = (struct RxsLib *)rxs;
        if ((rm = CreateRexxMsg(reply, 0, 0)) != 0) {
            if ((rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)line, strlen(line))) != 0) {
                rm->rm_Action = RXCOMM;
                Forbid();
                port = FindPort((STRPTR)"AMARCHY");
                if (port)
                    PutMsg(port, (struct Message *)rm);
                Permit();
                if (port) {
                    WaitPort(reply);
                    GetMsg(reply);
                    rc = rm->rm_Result1;
                    why = "the window command was refused";
                }
                DeleteArgstring((STRPTR)rm->rm_Args[0]);
            }
            DeleteRexxMsg(rm);
        }
        DeleteMsgPort(reply);
    }
    if (rxs)
        CloseLibrary(rxs);
    if (rc != 0 && ui && ui->refused)
        ui->refused(ui->ctx, m->label, UPREQ_FAILED, why);
    return rc == 0 ? 0 : -1;
}

int lf_run(const km_menu *m, const lf_ui *ui)
{
    switch (lf_plan(m->kind)) {
    case LF_DETACH:
        return run_detached(m, ui);
    case LF_SCAN: {
        int rc = run_detached(m, ui);
        return rc ? rc : LF_WATCH;
    }
    case LF_WINDOW:
        return run_in_uterm(m->arg, ui);
    case LF_VIEWER:                     /* the program super+k opens, on its own like detach: */
        return run_viewer(m, ui);
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
    case LF_WIN:
        return send_win(m, ui);
    default:
        return run_request(m, ui);
    }
}
