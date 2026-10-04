/* C:Claude -- a native Claude client for AmigaOS 3.x (68020+), ledger A2.
 *
 *   Claude [PROMPT] [MODEL=name] [EFFORT=level] [URL=url] [ROOT=dir] [PING] [DEBUG] [PLAIN]
 *
 * With no PROMPT: a conversation on a screen of its own in the window, as
 * Claude Code draws it (ledger A3): raw mode, an input box with its own
 * editing and history, a status line, the permission menus. PLAIN, or a
 * console that is not UP-Term's: the A2 line mode at the window's own
 * prompt instead. With a PROMPT: that one turn, then the end. PING:
 * one request of one token, its HTTP status and times (the transport
 * check). DEBUG: a log in T:Claude.log (request heads with the key blanked,
 * the stream's events). URL: another endpoint, e.g. http://host:8080/... for
 * tools/claude_fixture.py (recorded answers, no key needed). ROOT: the
 * start directory (default: the current one).
 *
 * The key: ENV:ANTHROPIC_API_KEY, else the file ENVARC:Claude/key. It is
 * never shown, never logged, and cleared from memory at the end.
 *
 * This file is only the wiring of claude/repl.c (host-tested) to the
 * console, bsdsocket (net_amiga.c) and AmigaDOS (sys_amiga.c). */
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <dos/var.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "repl.h"
#include "net_amiga.h"
#include "sys_amiga.h"
#include "util.h"

static const char vers[] = "$VER: Claude 1.0 (4.10.2026) UP-Term";
const char stack_cookie[] = "$STACK: 32768";

#define TEMPLATE "PROMPT/F,MODEL/K,EFFORT/K,URL/K,ROOT/K,PING/S,DEBUG/S,PLAIN/S"
enum { A_PROMPT, A_MODEL, A_EFFORT, A_URL, A_ROOT, A_PING, A_DEBUG, A_PLAIN, A_COUNT };

#define MIN_STACK 16000

static char key[512];
static BPTR logf;

static long c_read(void *u, char *buf, long cap)
{
    long n;
    (void)u;
    if (!FGets(Input(), (STRPTR)buf, cap))
        return -1;
    n = (long)strlen(buf);
    while (n && (buf[n - 1] == '\n' || buf[n - 1] == '\r'))
        buf[--n] = 0;
    return n;
}

static void c_write(void *u, const char *s, long n)
{
    (void)u;
    Write(Output(), (APTR)s, n);
}

static int c_brk(void *u)
{
    (void)u;
    return (SetSignal(0, SIGBREAKF_CTRL_C) & SIGBREAKF_CTRL_C) != 0;
}

static int c_sleep(void *u, long ms)
{
    long ticks = ms / 20;
    while (ticks > 0) {
        long t = ticks > 5 ? 5 : ticks;
        if (c_brk(u))
            return 1;
        Delay(t);
        ticks -= t;
    }
    return c_brk(u);
}

static unsigned long c_ms(void *u)
{
    struct DateStamp d;
    (void)u;
    DateStamp(&d);
    return (unsigned long)d.ds_Days * 86400000UL + (unsigned long)d.ds_Minute * 60000UL +
           (unsigned long)d.ds_Tick * 20UL;
}

static void c_log(void *u, const char *s, long n)
{
    (void)u;
    if (logf)
        Write(logf, (APTR)s, n);
}

/* ---- the screen of its own: raw keys, the window's size ---- */

static vt_termios saved;
static int saved_termios;       /* the console was in termios mode already (a PTY:) */
static int raw_on;

static struct FileHandle *console(BPTR h)
{
    struct FileHandle *fh;
    if (!h || !IsInteractive(h))
        return 0;
    fh = (struct FileHandle *)BADDR(h);
    return fh->fh_Type ? fh : 0;
}

/* raw mode through UP-Term's line discipline (the termios path vsh and
 * tmux take); another console refuses the packet and the line mode stays */
static int c_raw(void *u, int on)
{
    struct FileHandle *fh = console(Input());
    (void)u;
    if (!fh)
        return -1;
    if (on) {
        vt_termios t;
        if (!DoPkt(fh->fh_Type, ACTION_VTCON_TCGETA, fh->fh_Arg1, (LONG)&saved, 0, 0, 0))
            return -1;
        saved_termios = IoErr() == 1;
        t = saved;
        ld_make_raw(&t);
        if (!DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)&t, LD_TCSANOW, 0, 0))
            return -1;
        raw_on = 1;
        return 0;
    }
    if (raw_on) {
        if (saved_termios)
            DoPkt(fh->fh_Type, ACTION_VTCON_TCSETA, fh->fh_Arg1, (LONG)&saved, LD_TCSANOW, 0, 0);
        else
            SetMode(Input(), 0);    /* ends the termios mode: the window's own line editing again */
        raw_on = 0;
    }
    return 0;
}

static long c_rd(void *u, char *buf, long cap, long ms)
{
    BPTR in = Input();
    LONG n;
    (void)u;
    if (!WaitForChar(in, ms > 0 ? ms * 1000 : 0))
        return 0;
    n = Read(in, buf, cap);
    return n > 0 ? n : -1;
}

static int c_size(void *u, int *cols, int *rows)
{
    struct FileHandle *fh = console(Output());
    vt_winsize ws;
    (void)u;
    if (!fh)
        fh = console(Input());
    if (!fh)
        return -1;
    ws.ws_col = ws.ws_row = 0;
    if (!DoPkt(fh->fh_Type, ACTION_VTCON_GWINSZ, fh->fh_Arg1, (LONG)&ws, 0, 0, 0) || !ws.ws_col || !ws.ws_row)
        return -1;
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
}

static void say(const char *s)
{
    Write(Output(), (APTR)s, (LONG)strlen(s));
}

/* the key, from ENV: or ENVARC:Claude/key; 0 when there is none */
static int load_key(void)
{
    BPTR f;
    LONG n;
    if (GetVar((STRPTR)"ANTHROPIC_API_KEY", (STRPTR)key, sizeof(key), 0) > 0)
        return cl_key_clean(key) == 0;
    f = Open((STRPTR)"ENVARC:Claude/key", MODE_OLDFILE);
    if (!f)
        return 0;
    n = Read(f, key, sizeof(key) - 1);
    Close(f);
    key[n > 0 ? n : 0] = 0;
    return cl_key_clean(key) == 0;
}

int main(void)
{
    LONG args[A_COUNT];
    struct RDArgs *rda;
    struct Task *me = FindTask(0);
    cl_io io;
    net_amiga na;
    cl_net net;
    sys_amiga sa;
    cl_sys sys;
    cl_repl *r;
    BPTR rootlock = 0, oldcd = 0;
    char root[256];
    const char *url;
    int rc = 0, have_key;
    if (!vers[0] || !stack_cookie[0])
        return 20;
    if ((ULONG)me->tc_SPUpper - (ULONG)me->tc_SPLower < MIN_STACK) {
        say("Claude needs a stack of 16000 bytes or more: type Stack 32768 first.\n");
        return 20;
    }
    memset(args, 0, sizeof(args));
    rda = ReadArgs((STRPTR)TEMPLATE, args, 0);
    if (!rda) {
        PrintFault(IoErr(), (STRPTR)"Claude");
        return 20;
    }
    url = args[A_URL] ? (const char *)args[A_URL] : CL_DEFAULT_URL;
    have_key = load_key();
    if (args[A_ROOT]) {
        rootlock = Lock((STRPTR)args[A_ROOT], SHARED_LOCK);
        if (!rootlock) {
            PrintFault(IoErr(), (STRPTR)args[A_ROOT]);
            FreeArgs(rda);
            return 20;
        }
        oldcd = CurrentDir(rootlock);
    }
    if (!NameFromLock(((struct Process *)me)->pr_CurrentDir, (STRPTR)root, sizeof(root)))
        cl_copy(root, "", sizeof(root));
    if (args[A_DEBUG])
        logf = Open((STRPTR)"T:Claude.log", MODE_NEWFILE);
    memset(&io, 0, sizeof(io));
    io.read_line = c_read;
    io.write = c_write;
    io.brk = c_brk;
    io.sleep = c_sleep;
    io.ms = c_ms;
    io.log = c_log;
    if (!args[A_PROMPT] && !args[A_PING] && !args[A_PLAIN] && console(Input()) && console(Output())) {
        io.read = c_rd;
        io.size = c_size;
        io.raw = c_raw;
    }
    net_amiga_init(&na, &net);
    sys_amiga_init(&sa, &sys);
    r = (cl_repl *)malloc(sizeof(cl_repl));
    if (!r || repl_init(r, &io, &net, &sys, url, have_key ? key : 0, root)) {
        if (!r)
            say("Out of memory.\n");
        rc = 20;
    } else {
        r->debug = args[A_DEBUG] != 0;
        if (args[A_MODEL])
            cl_copy(r->model, (const char *)args[A_MODEL], sizeof(r->model));
        if (args[A_EFFORT])
            cl_copy(r->effort, (const char *)args[A_EFFORT], sizeof(r->effort));
        cl_copy(r->session, "ENVARC:Claude/session.json", sizeof(r->session));
        if (args[A_PING])
            rc = repl_ping(r) ? 10 : 0;
        else if (args[A_PROMPT])
            repl_line(r, (const char *)args[A_PROMPT]);
        else {
            repl_screen(r);         /* the line mode stays when the console says no */
            repl_run(r);
        }
    }
    if (r) {
        repl_free(r);               /* the screen's footer cleared, raw mode off */
        free(r);
    }
    c_raw(0, 0);
    net_amiga_exit(&na);
    memset(key, 0, sizeof(key));
    if (logf)
        Close(logf);
    if (rootlock) {
        CurrentDir(oldcd);
        UnLock(rootlock);
    }
    FreeArgs(rda);
    return rc;
}
