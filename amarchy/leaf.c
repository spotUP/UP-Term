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
#include "gthemes_dos.h"
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

static int run_request(const km_menu *m, const lf_ui *ui, const char *value)
{
    /* tab:, menuid: and slash: belong to a terminal window: the handler's request port
     * (handler/upreq.h) takes the action by name and runs it as a key binding would */
    upreq_body r;
    long rc;
    if (!upreq_fill(&r, m->kind, m->arg)) {
        fprintf(stderr, "amarchy: %s is not a terminal action\n", m->arg);
        return UPREQ_UNKNOWN;
    }
    if (value) {                         /* asked before the terminal opened */
        char act[UPREQ_ACTION];
        strcpy(act, r.action);
        if (!upreq_with_arg(&r, act, value)) {
            if (ui->refused)
                ui->refused(ui->ctx, m->label, UPREQ_FAILED, "the value is too long");
            return UPREQ_FAILED;
        }
    }
    rc = upreq_send(&r, ui->self_in_mru);
    if (rc == UPREQ_NEEDARG && !value) {
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

static int lister_front(unsigned long handle);

int lf_open_index(const char *label, const char *path, const lf_ui *ui)
{
    char err[160];
    int ci = lf_clip_entry(path);
    long lh = ix_lister_of(path);
    char tname[GT_NAME];
    if (gt_path_name(path, tname, sizeof tname)) {      /* a Style > Theme row: the drawn menu's theme is saved */
        if (gt_dos_select(tname))
            return 0;
        if (ui && ui->refused)
            ui->refused(ui->ctx, label, UPREQ_FAILED, "the theme could not be saved");
        return UPREQ_FAILED;
    }
    if (lh >= 0) {
        int rc = lister_front((unsigned long)lh);
        if (rc && ui && ui->refused)
            ui->refused(ui->ctx, label, UPREQ_FAILED, "Directory Opus did not bring the lister forward");
        return rc;
    }
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

/* One ARexx command to a port and its reply: the command line goes to port as an RXCOMM message and this waits for
 * the reply. Returns the return code, or -1 when the port or rexxsyslib.library is absent. A result string (the
 * reply's argstring, when the receiver gives one) is copied into result[cap] when result is set, else "". */
static long rexx_send(const char *portname, const char *line, char *result, int cap)
{
    struct MsgPort *reply, *port;
    struct RexxMsg *rm;
    struct Library *rxs;
    long rc = -1;
    if (result && cap > 0)
        result[0] = 0;
    rxs = OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    reply = rxs ? CreateMsgPort() : 0;
    if (reply) {
        RexxSysBase = (struct RxsLib *)rxs;
        if ((rm = CreateRexxMsg(reply, 0, 0)) != 0) {
            if ((rm->rm_Args[0] = (STRPTR)CreateArgstring((STRPTR)line, strlen(line))) != 0) {
                rm->rm_Action = RXCOMM | (result ? RXFF_RESULT : 0);
                Forbid();
                port = FindPort((STRPTR)portname);
                if (port)
                    PutMsg(port, (struct Message *)rm);
                Permit();
                if (port) {
                    WaitPort(reply);
                    GetMsg(reply);
                    rc = rm->rm_Result1;
                    if (rc == 0 && result && rm->rm_Result2) {
                        strncpy(result, (const char *)rm->rm_Result2, cap - 1);
                        result[cap - 1] = 0;
                        DeleteArgstring((STRPTR)rm->rm_Result2);
                    }
                }
                DeleteArgstring((STRPTR)rm->rm_Args[0]);
            }
            DeleteRexxMsg(rm);
        }
        DeleteMsgPort(reply);
    }
    if (rxs)
        CloseLibrary(rxs);
    return rc;
}

/* win:<verb>: the commodity does it (amarchy-gui's own window is the active one, the owner's target is the
 * commodity's ticket): send "WIN <verb>" to the ARexx port AMARCHY and wait for the reply. -1 and a line through
 * ui->refused when the port is absent or the verb is refused. */
static int send_win(const km_menu *m, const lf_ui *ui)
{
    char line[KM_ID + 16];
    long rc;
    snprintf(line, sizeof line, "WIN %s", m->arg);
    rc = rexx_send("AMARCHY", line, 0, 0);
    if (rc != 0 && ui && ui->refused)
        ui->refused(ui->ctx, m->label, UPREQ_FAILED,
                    rc < 0 ? "the Amarchy commodity is not running" : "the window command was refused");
    return rc == 0 ? 0 : -1;
}

#define LISTER_MAX 16
void lf_load_listers(ix_index *x)
{
    char ans[256], path[IX_PATH];
    unsigned long h[LISTER_MAX];
    ix_row rows[LISTER_MAX];
    int n, i, got = 0;
    if (!upactive_dopus_present())
        return;                                             /* no DOpus: no rows, no wait */
    if (rexx_send("DOPUS.1", "lister query all", ans, sizeof ans) != 0)
        return;
    n = ix_lister_handles(ans, h, LISTER_MAX);
    for (i = 0; i < n; i++) {
        char q[48];
        snprintf(q, sizeof q, "lister query %lu path", h[i]);
        path[0] = 0;
        if (rexx_send("DOPUS.1", q, path, sizeof path) == 0 && ix_lister_row(h[i], path, &rows[got]))
            got++;
    }
    ix_add_listers(x, rows, got);
}

/* A lister row: bring that lister to the front (handle from `lister query all`). UNVERIFIED on a rig: needs DOpus v5.104. */
static int lister_front(unsigned long handle)
{
    char line[48];
    int step;
    for (step = 0; lf_lister_front_command(line, sizeof line, step, handle); step++)
        if (rexx_send("DOPUS.1", line, 0, 0) != 0)
            return UPREQ_FAILED;
    return 0;
}

/* open:uterm: a new UP-Term window (NewShell on LF_UTERM_SPEC) */
static int open_uterm(void)
{
    char spec[128], line[160];
    uterm_spec(spec, sizeof spec);
    snprintf(line, sizeof line, "NewShell \"%s\"", spec);
    return system(line);
}

/* Style > Terminal with no terminal window open: open one, wait for its request port, then send the row there. */
static int open_then_request(const km_menu *m, const lf_ui *ui, const char *value)
{
    int i, rc = open_uterm();
    if (rc)
        return rc;
    for (i = 0; i < 100 && !upactive_terminal_present(ui->self_in_mru); i++)
        Delay(5);                       /* up to 5 seconds, a tick of 0.1 s */
    if (!upactive_terminal_present(ui->self_in_mru)) {
        if (ui->refused)
            ui->refused(ui->ctx, m->label, UPREQ_NOWINDOW, "");
        return UPREQ_NOWINDOW;
    }
    return run_request(m, ui, value);
}

int lf_run(const km_menu *m, const lf_ui *ui)
{
    const char *usage = upreq_needs_value(m->kind, m->arg);
    switch (lf_plan_ta(m->kind, upactive_terminal_present(ui->self_in_mru), usage != 0)) {
    case LF_ASK_THEN_OPEN_REQUEST: {
        /* the value first: the terminal that opens next takes the focus, and the menu closes with it */
        char value[UPREQ_ACTION];
        if (!ui->ask_value || !ui->ask_value(ui->ctx, m->label, usage, value, sizeof value))
            return 0;                   /* cancelled: no terminal opens */
        return open_then_request(m, ui, value);
    }
    case LF_OPEN_THEN_REQUEST:
        return open_then_request(m, ui, 0);
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
        int rc;
        const char *note;
        if (!lf_rexx_command(line, sizeof line, m->arg))
            return -1;
        rc = system(line);
        note = lf_rexx_notice(m->arg, rc);
        if (note && ui && ui->refused)
            ui->refused(ui->ctx, m->label, UPREQ_FAILED, note);
        return rc;
    }
    case LF_OPEN:
        return open_uterm();
    case LF_WIN:
        return send_win(m, ui);
    case LF_GTHEME:
        if (gt_dos_select(m->arg))
            return 0;
        if (ui && ui->refused)
            ui->refused(ui->ctx, m->label, UPREQ_FAILED, "the theme could not be saved");
        return UPREQ_FAILED;
    default:
        return run_request(m, ui, 0);
    }
}
