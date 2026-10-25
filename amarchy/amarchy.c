/* Amarchy: the Left Amiga key commodity (plan 2026-10-22-amarchy-commodity.md, Phase C).
 * Usage: Run >NIL: Amarchy [CX_PRIORITY n]     ARexx port AMARCHY: RELOAD, SHOW, QUIT, RUN <row>, STATUS. */
#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <dos/notify.h>
#include <rexx/storage.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>
#include <intuition/intuitionbase.h>
#include <proto/intuition.h>
#include <string.h>
#include "cx.h"
#include "cxdesc.h"
#include "act.h"
#include "leaf.h"
#include "wblaunch.h"
#include "menu_core.h"
#include "atiming.h"
#include "../claude/util.h"
#include "../config/keymap_load.h"
#include "../config/upconf.h"
#include "../handler/upactive.h"
#include "../tty/qmark.h"
#include "keymap_default.inc"

struct RxsLib *RexxSysBase;

/* cmd begins with word, any case */
static int starts(const char *cmd, const char *word)
{
    while (*word) {
        if ((*cmd++ | 0x20) != (*word++ | 0x20))
            return 0;
    }
    return 1;
}
long __stack = 16384;

static const char *notify_paths[] = {
    "ENV:up-term/keymap", "ENVARC:up-term/keymap", "ENV:up-term/keymap.user", "ENVARC:up-term/keymap.user",
    "ENV:up-term/up-term", "ENVARC:up-term/up-term"
};
#define NNOTIFY 6
static struct NotifyRequest nr[NNOTIFY];
static int nr_on[NNOTIFY];
static struct MsgPort *rexx_port, *notify_port;
static keymap g_km;
static long g_hits;
static int g_quit;

/* The directory Amarchy was started from (amarchy-menu ships beside it): UP-Term:bin is not on
 * every command path, so the helper runs by its full path. Resolved once at startup. */
static char g_dir[128];

static void dir_resolve(void)
{
    BPTR l = GetProgramDir();
    g_dir[0] = 0;
    if (l && !NameFromLock(l, (STRPTR)g_dir, sizeof(g_dir)))
        g_dir[0] = 0;
}

/* The key viewer's window, in cells (a fixed size: its text does not change with the node). */
#define VIEWER_COLS 76
#define VIEWER_ROWS 22

/* the menu-style setting (UC_MENU_*), read from the upconf at startup and on every reload */
static int g_style;

static void style_load(void)
{
    static const char *const path[2] = { "ENV:up-term/up-term", "ENVARC:up-term/up-term" };
    int i;
    g_style = UC_MENU_GRAPHICAL;
    for (i = 0; i < 2; i++) {
        BPTR f = Open((STRPTR)path[i], MODE_OLDFILE);
        char *buf;
        LONG got = 0;
        if (!f)
            continue;
        buf = AllocVec(UC_MAX_FILE + 1, MEMF_PUBLIC);
        if (buf)
            got = Read(f, buf, UC_MAX_FILE);
        Close(f);
        if (buf && got > 0)
            g_style = upconf_menu_style(buf, got);
        if (buf)
            FreeVec(buf);
        return;
    }
}

/* menu: / viewer / othermenu: / otherviewer: the program the setting names (or the other one), the
 * text one in its own terminal window sized from the tree, the graphical one in its own window. */
static int act_menu_kind(int kind, const char *node)
{
    char line[KM_ID + 160];
    int text = 0, cols = VIEWER_COLS, rows = VIEWER_ROWS;
    if (!cx_menu_cmd(line, sizeof(line), g_dir, kind, node, g_style, &text))
        return 0;
    if (!text)
        return act_run(line);
    if (kind == KA_MENU || kind == KA_OTHERMENU) {
        keymap full;
        /* g_km is compacted (no [menu] rows, keymap_compact): the size comes from the full table,
         * loaded for this open and released again, the table amarchy-menu will show; the rows follow the terminal the hotkey was pressed over (the
         * commodity is no UP-Term window: self 0) */
        memset(&full, 0, sizeof full);
        keymap_load(&full, km_default_text);
        mn_osd_size_t(&full, strcmp(node, "root") ? node : "", &cols, &rows, upactive_terminal_present(0));
        keymap_free(&full);
    }
    return act_in_window(line, cols, rows);
}

/* A decimal number after optional blanks and sign, 0 when there is none (atoi). */
static int parse_int(const char *s)
{
    int v = 0, neg = 0;
    while (*s == ' ' || *s == '\t')
        s++;
    if (*s == '-' || *s == '+')
        neg = *s++ == '-';
    while (*s >= '0' && *s <= '9')
        v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}

/* The STATUS reply: hits=N rows=N enabled=N active=N */
static void status_text(char *out, long cap)
{
    char num[12];
    static const char *const key[4] = { "hits=", " rows=", " enabled=", " active=" };
    long val[4];
    int i;
    val[0] = g_hits;
    val[1] = cx_nrows();
    val[2] = cx_enabled();
    val[3] = upactive_count();
    out[0] = 0;
    for (i = 0; i < 4; i++) {
        cl_cat(out, key[i], cap);
        cl_ltoa(val[i], num);
        cl_cat(out, num, cap);
    }
}

/* Run the action of row `row` (hotkey or ARexx RUN): one place, one counter. */
static int do_row(int row)
{
    const km_bind *b = cx_row(row);
    int ok = 0;
    if (!b)
        return 0;
    g_hits++;
    if (b->kind == KA_MENU || b->kind == KA_VIEWER || b->kind == KA_OTHERMENU || b->kind == KA_OTHERVIEWER)
        at_mark("hotkey");   /* opt-in: AMARCHY_TIMING */
    switch (b->kind) {
    case KA_RUN:
    case KA_DETACH: ok = act_run(b->arg); break;
    case KA_REXX:  ok = act_rexx(b->arg); break;
    case KA_OPEN:  ok = act_open_uterm(); break;
    case KA_MENU: case KA_VIEWER: case KA_OTHERMENU: case KA_OTHERVIEWER:
        ok = act_menu_kind(b->kind, b->arg);
        break;
    default:       ok = 1; break;
    }
    return ok;
}

static void reload(void)
{
    style_load();
    cx_detach();   /* the gate must not read the table that is about to be released */
    keymap_reload_compact(&g_km, cx_row_installed);   /* a bad edit leaves the old table */
    cx_install(&g_km);
}

static void notify_start(void)
{
    int i;
    for (i = 0; i < NNOTIFY; i++) {
        nr[i].nr_Name = (STRPTR)notify_paths[i];
        nr[i].nr_Flags = NRF_SEND_MESSAGE;
        nr[i].nr_stuff.nr_Msg.nr_Port = notify_port;
        nr_on[i] = StartNotify(&nr[i]) ? 1 : 0;
    }
}

static void notify_stop(void)
{
    int i;
    struct Message *m;
    for (i = 0; i < NNOTIFY; i++)
        if (nr_on[i])
            EndNotify(&nr[i]);
    while (notify_port && (m = GetMsg(notify_port)) != 0)
        ReplyMsg(m);
}

static void rexx_serve(void)
{
    struct RexxMsg *m;
    while ((m = (struct RexxMsg *)GetMsg(rexx_port)) != 0) {
        const char *cmd = (const char *)m->rm_Args[0];
        char out[96];
        long rc = 0;
        out[0] = 0;
        if (starts(cmd, "STATUS"))
            status_text(out, sizeof(out));
        else if (starts(cmd, "RELOAD"))
            reload();
        else if (starts(cmd, "RUN"))
            rc = do_row(parse_int(cmd + 3)) ? 0 : 5;
        else if (starts(cmd, "QUIT"))
            g_quit = 1;
        else if (starts(cmd, "SHOW"))
            act_menu_kind(KA_VIEWER, "");
        else
            rc = 10;
        m->rm_Result1 = rc;
        m->rm_Result2 = 0;
        if (out[0] && (m->rm_Action & RXFF_RESULT) && RexxSysBase)
            m->rm_Result2 = (long)CreateArgstring((STRPTR)out, strlen(out));
        ReplyMsg((struct Message *)m);
    }
}

/* WBStartup launches of index rows (Workbench's job, which Directory Opus does not do): the launcher
 * (amarchy-gui, amarchy-menu) sends lf_launch to port LF_LAUNCH_PORT and goes; the program's message,
 * segment and locks stay here, on a reply port that lives as long as this process, until the program
 * replies when it ends. */
struct launch_msg { struct Message msg; lf_launch b; };
static struct MsgPort *launch_port, *wb_reply;
static int launches_out;

static void launch_serve(void)
{
    struct launch_msg *m;
    while ((m = (struct launch_msg *)GetMsg(launch_port)) != 0) {
        const char *path = m->b.path;
        char err[LF_LAUNCH_ERR];
        err[0] = 0;
        m->b.path[LF_LAUNCH_PATH - 1] = 0;
        if (!wb_reply)
            m->b.rc = -1, strcpy(m->b.err, "no reply port");
        else if (wbl_start(&path, 1, wb_reply, err, sizeof err) == 0) {
            m->b.rc = 0;
            launches_out++;
        } else {
            m->b.rc = -1;
            err[LF_LAUNCH_ERR - 1] = 0;
            strcpy(m->b.err, err);
        }
        ReplyMsg(&m->msg);
    }
}

static void launch_replied(void)
{
    struct Message *m;
    while ((m = GetMsg(wb_reply)) != 0) {
        wbl_free((struct WBStartup *)m);
        launches_out--;
    }
}

/* act.c reads the front screen (AMB3) from it */
struct IntuitionBase *IntuitionBase;

int main(void)
{
    ULONG sigs, cxs, rxs, nts, lps, wrs;
    int row, ev;
    struct RDArgs *rda;
    LONG args[1] = { 0 };
    int pri = 0;
    if (qmark_line((const char *)GetArgStr())) { /* `Amarchy ?` shows the template and ends */
        PutStr((STRPTR)"CX_PRIORITY/K/N\n");
        return 0;
    }
    if ((rda = ReadArgs((STRPTR)"CX_PRIORITY/K/N", args, 0))) {
        if (args[0])
            pri = (int)*(LONG *)args[0];
        FreeArgs(rda);
    }
    dir_resolve();
    style_load();
    keymap_load_compact(&g_km, km_default_text, cx_row_installed);
    if (!cx_open(pri)) {
        cx_close();
        return 5;   /* already running (it was told), or commodities.library is missing */
    }
    IntuitionBase = (struct IntuitionBase *)OpenLibrary((STRPTR)"intuition.library", 36);
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    Forbid();
    rexx_port = FindPort((STRPTR)"AMARCHY") ? 0 : CreateMsgPort();
    if (rexx_port) {
        rexx_port->mp_Node.ln_Name = (char *)"AMARCHY";
        AddPort(rexx_port);
    }
    Permit();
    wb_reply = CreateMsgPort();
    Forbid();
    launch_port = wb_reply && !FindPort((STRPTR)LF_LAUNCH_PORT) ? CreateMsgPort() : 0;
    if (launch_port) {
        launch_port->mp_Node.ln_Name = (char *)LF_LAUNCH_PORT;
        AddPort(launch_port);
    }
    Permit();
    notify_port = CreateMsgPort();
    if (notify_port)
        notify_start();
    cx_install(&g_km);
    cxs = cx_sigmask();
    rxs = rexx_port ? 1UL << rexx_port->mp_SigBit : 0;
    nts = notify_port ? 1UL << notify_port->mp_SigBit : 0;
    lps = launch_port ? 1UL << launch_port->mp_SigBit : 0;
    wrs = wb_reply ? 1UL << wb_reply->mp_SigBit : 0;
    while (!g_quit) {
        sigs = Wait(cxs | rxs | nts | lps | wrs | SIGBREAKF_CTRL_C);
        if (sigs & SIGBREAKF_CTRL_C)
            break;
        if (sigs & cxs)
            while ((ev = cx_next(&row)) != CXE_NONE) {
                if (ev == CXE_KEY)
                    do_row(row);
                else if (ev == CXE_UNIQUE)
                    reload();
                else if (ev == CXE_KILL)
                    g_quit = 1;
            }
        if (sigs & rxs)
            rexx_serve();
        if (sigs & lps)
            launch_serve();
        if (sigs & wrs)
            launch_replied();
        if (sigs & nts) {
            struct Message *m;
            while ((m = GetMsg(notify_port)) != 0)
                ReplyMsg(m);
            reload();
        }
    }
    notify_stop();
    if (notify_port)
        DeleteMsgPort(notify_port);
    if (rexx_port) {
        RemPort(rexx_port);
        rexx_serve();
        DeleteMsgPort(rexx_port);
    }
    if (RexxSysBase)
        CloseLibrary((struct Library *)RexxSysBase);
    if (IntuitionBase)
        CloseLibrary((struct Library *)IntuitionBase);
    cx_close();
    /* Quit (Exchange Remove, Ctrl-C, QUIT): no new launch, and the process stays until every program it
     * started has replied its WBStartup message, so no reply goes to a deleted port. The commodity is
     * already gone from Exchange and its hotkeys are released. */
    if (launch_port) {
        RemPort(launch_port);
        launch_serve();             /* requests that came in before the port went still get started */
        DeleteMsgPort(launch_port);
    }
    while (launches_out > 0) {
        Wait(1UL << wb_reply->mp_SigBit);
        launch_replied();
    }
    if (wb_reply)
        DeleteMsgPort(wb_reply);
    return 0;
}
