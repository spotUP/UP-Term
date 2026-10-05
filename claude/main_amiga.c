/* C:Claude -- Claude Code for AmigaOS 3.x (68020+), ledgers A2-A4.
 *
 *   Claude [options] [prompt]                 Claude Code's flags (cli.h)
 *   Claude PROMPT/F,PRINT=P/S,MODEL/K,...     the same as AmigaDOS keywords
 *
 * With no PROMPT: a conversation on a screen of its own in the window, as
 * Claude Code draws it (ledger A3): raw mode, an input box with its own
 * editing and history, a status line, the permission menus. PLAIN, or a
 * console that is not UP-Term's: the A2 line mode at the window's own
 * prompt instead. With a PROMPT: the same, starting with it. PRINT (-p):
 * Claude Code's print mode (print.h): one answer to the prompt and what
 * is piped in (Type file | Claude -p "explain", or < file), in the
 * output format asked for, then the end. PING: one request of one token,
 * its HTTP status and times (the transport check). DEBUG: a log in
 * T:Claude.log (request heads with the key blanked, the stream's events).
 * URL: another endpoint, e.g. http://host:8080/... for
 * tools/claude_fixture.py (recorded answers, no key needed). ROOT: the
 * start directory (default: the current one). CONTINUE (-c), RESUME (-r):
 * an earlier conversation of this directory goes on.
 *
 * The key: ENV:ANTHROPIC_API_KEY, else the file ENVARC:Claude/key. It is
 * never shown, never logged, and cleared from memory at the end. Without
 * one the conversation starts at /login (print mode: an error result).
 *
 * This file is only the wiring of claude/repl.c, cli.c and print.c
 * (host-tested) to the console, bsdsocket (net_amiga.c) and AmigaDOS
 * (sys_amiga.c). */
#include <stdlib.h>
#include <string.h>
#include <exec/types.h>
#include <exec/tasks.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/var.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/vtcon_packets.h"
#include "../tty/ldisc.h"
#include "repl.h"
#include "cli.h"
#include "print.h"
#include "net_amiga.h"
#include "sys_amiga.h"
#include "util.h"

static const char vers[] = "$VER: Claude 1.0 (5.10.2026) UP-Term";
const char stack_cookie[] = "$STACK: 32768";

#define MIN_STACK 16000

static char key[512];
static BPTR logf;
static BPTR errf;               /* print mode's error stream */
static int errf_opened;

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

/* Ctrl+G (A4 1.12): the prompt's file in the user's editor -- ENV:EDITOR
 * (vsh's), else Ed -- in this window, raw mode off meanwhile */
static int c_edit(void *u, const char *path)
{
    char ed[200], cmd[512];
    LONG rc;
    (void)u;
    if (GetVar((STRPTR)"EDITOR", (STRPTR)ed, sizeof(ed), 0) <= 0)
        cl_copy(ed, "Ed", sizeof(ed));
    cl_copy(cmd, ed, sizeof(cmd));
    cl_cat(cmd, " \"", sizeof(cmd));
    cl_cat(cmd, path, sizeof(cmd));
    cl_cat(cmd, "\"", sizeof(cmd));
    rc = SystemTags((STRPTR)cmd, SYS_Input, Input(), SYS_Output, Output(), SYS_UserShell, TRUE, TAG_END);
    return rc == -1 ? -1 : 0;
}

static void say(const char *s)
{
    Write(Output(), (APTR)s, (LONG)strlen(s));
}

/* the key, from ENV: or ENVARC:Claude/key; 0 when there is none
 * (source: print mode's apiKeySource) */
static int load_key(char *source, long cap)
{
    BPTR f;
    LONG n;
    if (GetVar((STRPTR)"ANTHROPIC_API_KEY", (STRPTR)key, sizeof(key), 0) > 0) {
        cl_copy(source, "ANTHROPIC_API_KEY", cap);
        return cl_key_clean(key) == 0;
    }
    f = Open((STRPTR)"ENVARC:Claude/key", MODE_OLDFILE);
    if (!f)
        return 0;
    n = Read(f, key, sizeof(key) - 1);
    Close(f);
    key[n > 0 ? n : 0] = 0;
    cl_copy(source, "user", cap);
    return cl_key_clean(key) == 0;
}

/* ---- print mode: stdout, the error stream, what is piped in ---- */

static void p_out(void *u, const char *s, long n)
{
    (void)u;
    Write(Output(), (APTR)s, n);
}

static void p_err(void *u, const char *s, long n)
{
    (void)u;
    if (errf)
        Write(errf, (APTR)s, n);
}

static long p_in(void *u, char *buf, long cap)
{
    LONG n;
    (void)u;
    if (SetSignal(0, 0) & SIGBREAKF_CTRL_C)
        return -1;
    n = Read(Input(), buf, cap);
    return n < 0 ? -1 : n;
}

/* the error stream: the shell's (pr_CES), else the console window */
static void open_err(void)
{
    struct Process *pr = (struct Process *)FindTask(0);
    errf = pr->pr_CES;
    if (!errf) {
        errf = Open((STRPTR)"*", MODE_NEWFILE);
        errf_opened = errf != 0;
    }
}

/* The command line, both forms (cli.c). `Claude ?` shows the template and
 * reads the arguments from the next line, as ReadArgs does. 0, or -1
 * with the reason shown. */
static int args(cl_cli *c)
{
    char line[1024];
    if (cli_parse_line(c, (const char *)GetArgStr()))
        goto bad;
    if (!c->ask_template)
        return 0;
    say(CLI_TEMPLATE ": ");
    if (!FGets(Input(), (STRPTR)line, sizeof(line)))
        line[0] = 0;
    cli_free(c);
    if (cli_parse_line(c, line) == 0 && !c->ask_template)
        return 0;
bad:
    say(c->err[0] ? c->err : "Claude: bad arguments (Claude ? shows the template)");
    say("\n");
    return -1;
}

int main(void)
{
    struct Task *me = FindTask(0);
    cl_cli cli;
    cl_io io;
    net_amiga na, wa;
    cl_net net, wnet;
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
    cli_init(&cli);
    if (args(&cli)) {
        cli_free(&cli);
        return 20;
    }
    if (cli.help || cli.version) {
        int i;
        if (cli.help)
            for (i = 0; cli_usage(i); i++)
                say(cli_usage(i));
        else {
            say(cli_version());
            say("\n");
        }
        cli_free(&cli);
        return 0;
    }
    url = cli.url[0] ? cli.url : CL_DEFAULT_URL;
    have_key = load_key(cli.key_source, sizeof(cli.key_source));
    if (cli.root[0]) {
        rootlock = Lock((STRPTR)cli.root, SHARED_LOCK);
        if (!rootlock) {
            PrintFault(IoErr(), (STRPTR)cli.root);
            cli_free(&cli);
            return 20;
        }
        oldcd = CurrentDir(rootlock);
    }
    if (!NameFromLock(((struct Process *)me)->pr_CurrentDir, (STRPTR)root, sizeof(root)))
        cl_copy(root, "", sizeof(root));
    if (cli.debug)
        logf = Open((STRPTR)"T:Claude.log", MODE_NEWFILE);
    if (cli.print)
        open_err();
    memset(&io, 0, sizeof(io));
    io.read_line = c_read;
    io.write = c_write;
    io.brk = c_brk;
    io.sleep = c_sleep;
    io.ms = c_ms;
    io.log = c_log;
    if (!cli.print && !cli.ping && !cli.plain && console(Input()) && console(Output())) {
        io.read = c_rd;
        io.size = c_size;
        io.raw = c_raw;
        io.edit = c_edit;
    }
    net_amiga_init(&na, &net);
    net_amiga_init(&wa, &wnet);     /* WebFetch's own connection (ledger A4 WP2) */
    sys_amiga_init(&sa, &sys);
    r = (cl_repl *)malloc(sizeof(cl_repl));
    if (!r || repl_init(r, &io, &net, &sys, url, have_key ? key : 0, root)) {
        if (!r)
            say("Out of memory.\n");
        rc = 20;
    } else {
        r->debug = cli.debug;
        r->tools.web = &wnet;
        /* A2's one saved conversation: /resume takes it over when there is no session yet */
        cl_copy(r->session, "ENVARC:Claude/session.json", sizeof(r->session));
        cl_copy(r->ui.histfile, "ENVARC:Claude/history", sizeof(r->ui.histfile));   /* A4 1.1 */
        if (cli_apply(&cli, r)) {
            say(cli.err);
            say("\n");
            rc = 20;
        } else if (cli.ping)
            rc = repl_ping(r) ? 10 : 0;
        else if (cli.print) {
            cl_pout po;
            po.u = 0;
            po.out = p_out;
            po.err = errf ? p_err : 0;
            po.in = IsInteractive(Input()) ? 0 : p_in;    /* piped in, or < file */
            rc = print_run(r, &cli, &po);
        } else {
            repl_screen(r);         /* the line mode stays when the console says no */
            if (cli_session(&cli, r))
                ui_line(&r->ui, cli.err);
            r->first = cli.prompt;  /* sent once the session is up (after /login without a key) */
            repl_run(r);
        }
    }
    if (r) {
        repl_free(r);               /* the screen's footer cleared, raw mode off */
        free(r);
    }
    c_raw(0, 0);
    wnet.close(wnet.u);
    net_amiga_exit(&na);
    memset(key, 0, sizeof(key));
    if (logf)
        Close(logf);
    if (errf_opened)
        Close(errf);
    if (rootlock) {
        CurrentDir(oldcd);
        UnLock(rootlock);
    }
    cli_free(&cli);
    return rc;
}
