#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "act.h"
#include "../config/keymap.h"
#include "../claude/util.h"
#include "../handler/frontscr.h"
#include "leaf.h"
#include "termenv.h"
#include "menu_open.h"
#include "cxdesc.h"
#define SEGSTACK_DOS
#include "../config/segstack.h"

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
    path = dup_path();
    termenv_ensure();
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

/* ---- the warm menu server (plan 2026-10-27-amarchy-warm-menu-server.md) ---- */
static mo_srv g_srv;
static struct {
    const char *dir;
    char line[KM_ID + 160];
    int cols, rows;
} g_open;

/* the cold path, used when the server is missing, dead or wedged: a menu process of its own */
static void srv_spawn_cold(void *user, const ms_req *r)
{
    (void)user;
    (void)r;
    act_in_window(g_open.line, g_open.cols, g_open.rows);
}

/* `amarchy-menu serve`, async, no console, named "amarchy-menu" (the liveness check reads the name), on the
 * stack the program's own $STACK cookie asks for (RULES.md rule 6). */
static void srv_start_proc(void *user)
{
    char path[160], line[176];
    ULONG stack = 262144;
    BPTR in, out, p, seg;
    LONG rc;
    (void)user;
    cl_copy(path, g_open.dir, sizeof path);
    if (path[0] && path[strlen(path) - 1] != ':' && path[strlen(path) - 1] != '/')
        cl_cat(path, "/", sizeof path);
    cl_cat(path, MS_TASK_NAME, sizeof path);
    seg = LoadSeg((STRPTR)path);
    if (seg) {
        ULONG v = seg_stack(seg);
        if (v > stack)
            stack = v;
        UnLoadSeg(seg);
    }
    cl_copy(line, "\"", sizeof line);
    cl_cat(line, path, sizeof line);
    cl_cat(line, "\" serve", sizeof line);
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((STRPTR)"NIL:", MODE_NEWFILE);
    p = dup_path();
    rc = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                    NP_Name, (ULONG)MS_TASK_NAME, NP_StackSize, stack, p ? NP_Path : TAG_IGNORE, p, TAG_END);
    if (rc == -1) {
        if (in)
            Close(in);
        if (out)
            Close(out);
        free_path(p);
    }
}

int act_menu_server_start(const char *dir)
{
    if (!g_srv.reply)
        g_srv.reply = CreateMsgPort();
    if (!g_srv.reply)
        return 0;
    g_srv.user = 0;
    g_srv.spawn = srv_spawn_cold;
    g_srv.start = srv_start_proc;
    g_open.dir = dir;
    srv_start_proc(0);
    g_srv.st.started = 1;
    g_srv.st.last_start = 0;
    {
        ms_ops ops;
        mo_srv_ops(&ops, &g_srv);
        g_srv.st.last_start = ops.now(ops.ctx);
    }
    return 1;
}

void act_menu_server_quit(void)
{
    if (!g_srv.reply)
        return;
    mo_close_open();                /* a menu showing at quit ends first */
    mo_srv_quit(&g_srv);
    if (!g_srv.out)
        DeleteMsgPort(g_srv.reply);
    g_srv.reply = 0;
}

int act_menu_open(const char *dir, int kind, const char *node, const char *line, int cols, int rows)
{
    ms_ops ops;
    ms_req r;
    if (!g_srv.reply)
        g_srv.reply = CreateMsgPort();
    if (!g_srv.reply)
        return act_in_window(line, cols, rows);
    g_srv.user = 0;
    g_srv.spawn = srv_spawn_cold;
    g_srv.start = srv_start_proc;
    g_open.dir = dir;
    cl_copy(g_open.line, line, sizeof g_open.line);
    g_open.cols = cols;
    g_open.rows = rows;
    window_spec(g_srv.spec, sizeof g_srv.spec, cols, rows);
    r.kind = kind == KA_VIEWER ? MS_VIEWER : MS_MENU;
    r.node = kind == KA_VIEWER || !strcmp(node, "root") ? "" : node;
    mo_srv_ops(&ops, &g_srv);
    ms_open(&ops, &r);
    return 1;
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
