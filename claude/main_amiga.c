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
 * one, and with ENVARC:Claude/remote naming a computer that runs Claude
 * Code (Install writes it: "host port", the NAS), plain Claude connects
 * there with C:uptelnet in this window instead (W49: one command, your
 * Claude subscription). Otherwise the conversation starts at /login
 * (print mode: an error result).
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
#include "../tty/bmsg.h"
#include "../tty/qmark.h"
#include "../net/upagent.h"
#include "repl.h"
#include "cli.h"
#include "launch.h"
#include "print.h"
#include "net_amiga.h"
#include "io_amiga.h"
#include "sys_amiga.h"
#include "util.h"
#include "path.h"

/* the up-claude add-on's version and the build day (upterm_version.h, written by the Makefile) */
#include "upterm_version.h"
static const char vers[] = "$VER: Claude " UPCLAUDE_VERSION " (" UPTERM_DATE ") UP-Term";
const char stack_cookie[] = "$STACK: 32768";

#define MIN_STACK 16000

#define NAS_KEY_FILE "ENVARC:Claude/nas-key"

static char key[512];
static BPTR logf;
static char logname[256] = "T:Claude.log";  /* --debug-file; opened at the first line (/debug too) */
static BPTR errf;               /* print mode's error stream */
static int errf_opened;

static void c_log(void *u, const char *s, long n)
{
    (void)u;
    if (!logf)
        logf = Open((STRPTR)logname, MODE_NEWFILE);     /* DEBUG, --debug-file, /debug */
    if (logf)
        Write(logf, (APTR)s, n);
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

/* ENVARC:Claude/remote: 1 with the host and port it names, 0 when there is no file or it names none */
static int remote_named(char *host, long hcap, long *port)
{
    char text[512];
    LONG n;
    BPTR f = Open((STRPTR)"ENVARC:Claude/remote", MODE_OLDFILE);
    if (!f)
        return 0;
    n = Read(f, text, sizeof(text) - 1);
    Close(f);
    text[n > 0 ? n : 0] = 0;
    return cli_remote_parse(text, host, hcap, port);
}

/* ENVARC:Claude/last-mode: the choice the question highlights (LAUNCH_LOCAL when there is none) */
static int last_mode(void)
{
    char text[16];
    LONG n;
    BPTR f = Open((STRPTR)LAUNCH_LAST_FILE, MODE_OLDFILE);
    if (!f)
        return LAUNCH_LOCAL;
    n = Read(f, text, sizeof(text) - 1);
    Close(f);
    text[n > 0 ? n : 0] = 0;
    return launch_last_parse(text);
}

static void save_last_mode(int mode)
{
    const char *t = launch_last_text(mode);
    BPTR f = Open((STRPTR)LAUNCH_LAST_FILE, MODE_NEWFILE);
    if (f) {
        Write(f, (APTR)t, (LONG)strlen(t));
        Close(f);
    }
}

static int path_exists(const char *path)
{
    BPTR l = Lock((STRPTR)path, SHARED_LOCK);
    if (l)
        UnLock(l);
    return l != 0;
}

/* ENVARC:Claude/remote names Claude Code on another computer: uptelnet
 * there in this window, its return code; -1 when the file names none */
static int remote(void)
{
    char host[200], cmd[320], num[12];
    long port, i;
    LONG rc;
    if (!remote_named(host, sizeof(host), &port))
        return -1;
    /* the file names a computer: its uptelnet has to be in C: */
    {
        BPTR l = Lock((STRPTR)"C:uptelnet", SHARED_LOCK);
        if (!l) {
            say("Claude: ");
            say(bmsg_uptelnet_missing());
            say("\n");
            return 20;
        }
        UnLock(l);
    }
    i = sizeof(num) - 1;
    num[i] = 0;
    do
        num[--i] = (char)('0' + port % 10);
    while ((port /= 10) && i > 0);
    cl_copy(cmd, "uptelnet \"", sizeof(cmd));
    cl_cat(cmd, host, sizeof(cmd));
    cl_cat(cmd, "\" ", sizeof(cmd));
    cl_cat(cmd, num + i, sizeof(cmd));
    /* the key login (net/tn.h): the Amiga's key for this NAS; a missing file asks the NAS to pair */
    cl_cat(cmd, " KEY " NAS_KEY_FILE, sizeof(cmd));
    /* the session daemon (net/upagent.h): the wasabid that is there, else uptelnet says it has none */
    {
        const char *agent = upagent_pick(path_exists);
        if (agent) {
            cl_cat(cmd, " AGENT ", sizeof(cmd));
            cl_cat(cmd, agent, sizeof(cmd));
        }
    }
    rc = SystemTags((STRPTR)cmd, SYS_Input, Input(), SYS_Output, Output(), SYS_UserShell, TRUE, TAG_END);
    return rc == -1 ? 20 : (int)rc;
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

/* The command line, both forms (cli.c). A lone `?` never gets here: main
 * shows the template and ends first (qmark.h). 0, or -1 with the reason
 * shown. */
static int args(cl_cli *c)
{
    if (!cli_parse_line(c, (const char *)GetArgStr()))
        return 0;
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
    /* `Claude ?` shows the template and ends: no ReadArgs prompt waiting for the arguments */
    if (qmark_line((const char *)GetArgStr())) {
        say(CLI_TEMPLATE);
        say(CLI_TEMPLATE_MORE "\n");
        return 0;
    }
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
    if (cli.remote_addr) {
        /* the line "host port" of ENVARC:Claude/remote, for the Installer
         * and ClaudeCode (one parser: cli_remote_parse); WARN when none */
        char text[512], line[260];
        LONG n = 0;
        BPTR f = Open((STRPTR)"ENVARC:Claude/remote", MODE_OLDFILE);
        if (f) {
            n = Read(f, text, sizeof(text) - 1);
            Close(f);
        }
        text[n > 0 ? n : 0] = 0;
        cli_free(&cli);
        if (!cli_remote_line(text, line, sizeof(line)))
            return 5;
        say(line);
        say("\n");
        return 0;
    }
    url = cli.url[0] ? cli.url : CL_DEFAULT_URL;
    have_key = load_key(cli.key_source, sizeof(cli.key_source));
    /* the console first: the launch question needs it (CL1) */
    memset(&io, 0, sizeof(io));
    io_amiga_args();
    io_amiga_line(&io);
    io.log = c_log;
    if (!cli.print && !cli.ping && !cli.plain)
        io_amiga_screen(&io);
    {
        char h[200];
        long pt;
        int mode = cli_launch_mode(&cli, have_key, remote_named(h, sizeof(h), &pt));
        if (mode == LAUNCH_ASK) {
            mode = launch_ask(&io, last_mode());
            if (mode < 0) {             /* Esc, Ctrl+C: nothing started */
                io_amiga_end();
                cli_free(&cli);
                return 5;
            }
            save_last_mode(mode);
        }
        if (mode == LAUNCH_REMOTE) {
            int rr = remote();
            io_amiga_end();
            cli_free(&cli);
            if (rr < 0) {
                say("Claude: ENVARC:Claude/remote names no computer. Run `Claude SETUP` to store the address.\n");
                return 10;
            }
            return rr;
        }
    }
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
    if (cli.debug_file[0])
        cl_copy(logname, cli.debug_file, sizeof(logname));
    else {
        /* CLAUDE_CODE_DEBUG_LOGS_DIR (Claude Code: a file's path, despite the name) */
        char v[256];
        if (GetVar((STRPTR)"CLAUDE_CODE_DEBUG_LOGS_DIR", (STRPTR)v, sizeof(v), 0) > 0)
            cl_copy(logname, v, sizeof(logname));
    }
    if (cli.print)
        open_err();
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
        r->log_path = logname;      /* /debug names it */
        r->tools.web = &wnet;
        /* A2's one saved conversation: /resume takes it over when there is no session yet */
        /* in the growing files' directory (claude/datadir.h): UP-Term:var/Claude with the kit */
        if (path_join(r->data, "session.json", r->session, sizeof(r->session)))
            r->session[0] = 0;
        if (path_join(r->data, "history", r->ui.histfile, sizeof(r->ui.histfile)))   /* A4 1.1 */
            r->ui.histfile[0] = 0;
        if (cli_apply(&cli, r)) {
            say(cli.err);
            say("\n");
            rc = 20;
        } else if (cli.ping)
            rc = repl_ping(r) ? 10 : 0;
        else if (cli.sub) {
            cl_pout po;
            po.u = 0;
            po.out = p_out;
            po.err = 0;
            po.in = 0;
            rc = print_subcommand(r, &cli, &po);  /* Claude doctor, auth ..., purge */
        } else if (cli.init_only) {
            repl_setup(r, "init", 1);   /* --init-only: Setup and SessionStart hooks, then nothing */
            rc = 0;
        } else if (cli.print) {
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
            r->first = cli.setup ? "/setup" : cli.prompt;   /* Claude SETUP: the wizard; else sent once the session is up (after /login without a key) */
            repl_run(r);
        }
    }
    if (r) {
        repl_free(r);               /* the screen's footer cleared, raw mode off */
        free(r);
    }
    io_amiga_end();
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
