#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <string.h>
#include "act.h"
#include "../config/keymap.h"
#include "../claude/util.h"
#include "../handler/frontscr.h"
#include "leaf.h"
#include "termenv.h"
#include "menu_open.h"
#include "taskalive.h"
#include "cxdesc.h"
#include "proclaunch.h"

int act_run(const char *cmd)
{
    BPTR in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    BPTR out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    BPTR path = pl_dup_path();
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
        pl_free_path(path);
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

/* The XCON: spec of the small Amarchy window: borderless, centred, in front, cols x rows cells, closing when
 * deactivated, on the front public screen. The one copy: act_in_window and the D5 notice line use it. */
static void window_spec(char *full, int cap, int cols, int rows)
{
    char spec[96], num[12];
    cl_copy(spec, "XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS ", sizeof(spec));
    cl_ltoa(cols, num);
    cl_cat(spec, num, sizeof(spec));
    cl_cat(spec, "x", sizeof(spec));
    cl_ltoa(rows, num);
    cl_cat(spec, num, sizeof(spec));
    spec_on_front(full, cap, spec);
}

/* Like act_run, but the command's input and output are a new XCON: window (the quick menu and
 * the key viewer draw on a terminal). The window is borderless, centred, in front, sized
 * cols x rows cells, and closes when the command ends or when it is deactivated. */
int act_in_window(const char *cmd, int cols, int rows)
{
    char full[160];
    BPTR w, path;
    LONG rc;
    window_spec(full, sizeof full, cols, rows);
    w = Open((STRPTR)full, MODE_NEWFILE);
    path = pl_dup_path();
    termenv_ensure();
    if (!cmd || !*cmd || !w) {
        if (w)
            Close(w);
        pl_free_path(path);
        return 0;
    }
    rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE,
                    NP_StackSize, 65536, path ? NP_Path : TAG_IGNORE, path, TAG_END);
    if (rc == -1) {
        Close(w);
        pl_free_path(path);
        return 0;
    }
    return 1;
}

/* ---- the warm menu servers (plans 2026-10-27-amarchy-warm-menu-server.md, 2026-10-27-amarchy-gui-warm.md):
 * [1] the text menu (`amarchy-menu serve`), [0] the drawn menu (`amarchy-gui serve`), one mechanism ---- */
static mo_srv g_srv[2];
static const char *g_srv_dir;
static struct {
    char line[KM_ID + 160];
    int cols, rows;
} g_open;

/* the cold path, used when the server is missing, dead or wedged: a menu process of its own (the text menu
 * in a terminal window, the drawn menu opens its own) */
static void srv_spawn_cold(void *user, const ms_req *r)
{
    (void)r;
    if (user == (void *)&g_srv[1])
        act_in_window(g_open.line, g_open.cols, g_open.rows);
    else
        act_run(g_open.line);
}

/* `<program> serve`, async, no console. System() names the process "Background CLI" whatever NP_Name says, so
 * the liveness check (taskalive.c) knows the server by the program it runs (mo_srv.task). On the
 * stack the program's own $STACK cookie asks for (RULES.md rule 6). */
static void srv_path(const mo_srv *s, char *path, int n)
{
    cl_copy(path, g_srv_dir, n);
    if (path[0] && path[strlen(path) - 1] != ':' && path[strlen(path) - 1] != '/')
        cl_cat(path, "/", n);
    cl_cat(path, s->task, n);
}

/* The server program's DateStamp on disk: ms_open compares it with the one taken at the server's start, so a
 * program replaced by upkg or `rig.py install` ends the old server and starts one from the new file. */
static long srv_stamp_file(void *user)
{
    char path[160];
    srv_path((const mo_srv *)user, path, sizeof path);
    return ta_file_stamp(path);
}

static void srv_start_proc(void *user)
{
    const mo_srv *s = (const mo_srv *)user;
    char path[160], line[176];
    srv_path(s, path, sizeof path);
    cl_copy(line, "\"", sizeof line);
    cl_cat(line, path, sizeof line);
    cl_cat(line, "\" serve", sizeof line);
    pl_start(line, path, 262144, s->task);
}

/* the instance for a style, its reply port made on first use; 0 when no port can be made */
static mo_srv *srv_get(int text)
{
    mo_srv *s = &g_srv[text ? 1 : 0];
    if (!s->reply)
        s->reply = CreateMsgPort();
    if (!s->reply)
        return 0;
    ms_server_names(text, &s->port, &s->task);
    s->user = s;
    s->spawn = srv_spawn_cold;
    s->start = srv_start_proc;
    s->stamp = srv_stamp_file;
    return s;
}

int act_menu_server_start(const char *dir)
{
    int text, ok = 1;
    g_srv_dir = dir;
    for (text = 0; text < 2; text++) {
        mo_srv *s = srv_get(text);
        ms_ops ops;
        if (!s) {
            ok = 0;
            continue;
        }
        srv_start_proc(s);
        s->st.started = 1;
        s->st.bin_stamp = srv_stamp_file(s);
        mo_srv_ops(&ops, s);
        s->st.last_start = ops.now(ops.ctx);
    }
    return ok;
}

void act_menu_server_quit(void)
{
    int i;
    mo_close_open();                /* a menu showing at quit ends first */
    for (i = 0; i < 2; i++) {
        mo_srv *s = &g_srv[i];
        if (!s->reply)
            continue;
        mo_srv_quit(s);
        if (!s->out)
            DeleteMsgPort(s->reply);
        s->reply = 0;
    }
}

static int menu_open_req(const char *dir, int text, int kind, const char *node, const char *line, int cols, int rows,
                         int anchored, int ax, int ay)
{
    ms_ops ops;
    ms_req r;
    mo_srv *s;
    g_srv_dir = dir;
    s = srv_get(text);
    if (!s)
        return text ? act_in_window(line, cols, rows) : act_run(line);
    cl_copy(g_open.line, line, sizeof g_open.line);
    g_open.cols = cols;
    g_open.rows = rows;
    window_spec(s->spec, sizeof s->spec, cols, rows);
    r.kind = kind == KA_VIEWER ? MS_VIEWER : MS_MENU;
    r.node = kind == KA_VIEWER || !strcmp(node, "root") ? "" : node;
    r.anchored = anchored;
    r.ax = ax;
    r.ay = ay;
    mo_srv_ops(&ops, s);
    ms_open(&ops, &r);
    return 1;
}

int act_menu_open(const char *dir, int text, int kind, const char *node, const char *line, int cols, int rows)
{
    return menu_open_req(dir, text, kind, node, line, cols, rows, 0, 0, 0);
}

int act_menu_open_at(const char *dir, int text, int kind, const char *node, const char *line, int cols, int rows,
                     int ax, int ay)
{
    return menu_open_req(dir, text, kind, node, line, cols, rows, 1, ax, ay);
}

int act_rexx(const char *arg)
{
    char line[160], spec[160], note[480];
    if (!lf_rexx_command(line, sizeof line, arg))
        return 0;
    /* a dopus-*.rexx script runs inside one RX program that opens the notice window only when the script
     * returned 5 (D5); asynchronous like every other script, so a busy DOpus never stalls the hotkeys */
    if (!lf_rexx_notice(arg, 5))
        return act_run(line);
    window_spec(spec, sizeof spec, 36, 3);
    if (!lf_rexx_notice_line(note, sizeof note, arg, spec))
        return act_run(line);
    return act_run(note);
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
