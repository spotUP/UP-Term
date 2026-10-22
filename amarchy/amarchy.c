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
#include <string.h>
#include "cx.h"
#include "cxdesc.h"
#include "act.h"
#include "../claude/util.h"
#include "../config/keymap_load.h"
#include "../handler/upactive.h"
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
    "ENV:up-term/keymap", "ENVARC:up-term/keymap", "ENV:up-term/keymap.user", "ENVARC:up-term/keymap.user"
};
#define NNOTIFY 4
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

/* the amarchy-menu helper (full path) with args, in its own terminal window */
static int run_helper(const char *args)
{
    char line[KM_ID + 160];
    if (!cx_helper_cmd(line, sizeof(line), g_dir, args))
        return 0;
    return act_in_window(line);
}

/* menu:<node> -> the helper in its own terminal window */
static int act_menu(const char *node)
{
    char args[KM_ID + 16];
    cl_copy(args, "menu ", sizeof(args));
    cl_cat(args, node[0] ? node : "root", sizeof(args));
    return run_helper(args);
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
    switch (b->kind) {
    case KA_RUN:   ok = act_run(b->arg); break;
    case KA_REXX:  ok = act_rexx(b->arg); break;
    case KA_OPEN:  ok = act_open_uterm(); break;
    case KA_MENU:  ok = act_menu(b->arg); break;
    case KA_VIEWER: ok = run_helper("viewer"); break;
    default:       ok = 1; break;
    }
    return ok;
}

static void reload(void)
{
    if (keymap_reload(&g_km))   /* parses into a heap scratch, copies only on success */
        cx_install(&g_km);   /* a bad edit leaves the old set active */
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
            run_helper("viewer");
        else
            rc = 10;
        m->rm_Result1 = rc;
        m->rm_Result2 = 0;
        if (out[0] && (m->rm_Action & RXFF_RESULT) && RexxSysBase)
            m->rm_Result2 = (long)CreateArgstring((STRPTR)out, strlen(out));
        ReplyMsg((struct Message *)m);
    }
}

int main(void)
{
    ULONG sigs, cxs, rxs, nts;
    int row, ev;
    struct RDArgs *rda;
    LONG args[1] = { 0 };
    int pri = 0;
    if ((rda = ReadArgs((STRPTR)"CX_PRIORITY/K/N", args, 0))) {
        if (args[0])
            pri = (int)*(LONG *)args[0];
        FreeArgs(rda);
    }
    dir_resolve();
    keymap_load(&g_km, km_default_text);
    if (!cx_open(pri)) {
        cx_close();
        return 5;   /* already running (it was told), or commodities.library is missing */
    }
    RexxSysBase = (struct RxsLib *)OpenLibrary((STRPTR)"rexxsyslib.library", 0);
    Forbid();
    rexx_port = FindPort((STRPTR)"AMARCHY") ? 0 : CreateMsgPort();
    if (rexx_port) {
        rexx_port->mp_Node.ln_Name = (char *)"AMARCHY";
        AddPort(rexx_port);
    }
    Permit();
    notify_port = CreateMsgPort();
    if (notify_port)
        notify_start();
    cx_install(&g_km);
    cxs = cx_sigmask();
    rxs = rexx_port ? 1UL << rexx_port->mp_SigBit : 0;
    nts = notify_port ? 1UL << notify_port->mp_SigBit : 0;
    while (!g_quit) {
        sigs = Wait(cxs | rxs | nts | SIGBREAKF_CTRL_C);
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
    cx_close();
    return 0;
}
