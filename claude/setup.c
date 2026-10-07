/* setup -- the first-run wizard of C:Claude; see setup.h. */
#include <stdlib.h>
#include <string.h>
#include "repl_int.h"
#include "setup.h"
#include "cli.h"
#include "path.h"
#include "util.h"

static void say2(cl_repl *r, const char *a, const char *b)
{
    repl_say(r, a, b);
}

/* one check's result, as /doctor shows them */
static void chk(cl_repl *r, int ok, const char *what, const char *detail)
{
    char m[400];
    cl_copy(m, ok > 0 ? "  [OK] " : ok == 0 ? "  [FAIL] " : "  [INFO] ", sizeof(m));
    cl_cat(m, what, sizeof(m));
    cl_cat(m, ": ", sizeof(m));
    cl_cat(m, detail, sizeof(m));
    ui_line(&r->ui, m);
}

int setup_remote_text(const char *host, long port, char *out, long cap)
{
    char num[16];
    cl_copy(out, "; Claude Code on another computer: host and port. With no API key,\n"
                 "; plain Claude connects there (uptelnet). Delete this file to stop.\n", cap);
    cl_cat(out, host, cap);
    cl_cat(out, " ", cap);
    cl_ltoa(port, num);
    cl_cat(out, num, cap);
    cl_cat(out, "\n", cap);
    return (long)strlen(out) < cap - 1 ? 0 : -1;
}

static int write_home(cl_sys *sys, const char *home, const char *name, const char *text)
{
    char file[300];
    if (path_join(home, name, file, sizeof(file)))
        return -1;
    if (sys->mkdir)
        sys->mkdir(sys->u, home);
    return sys->write(sys->u, file, text, (long)strlen(text)) ? -1 : 0;
}

int setup_write_remote(cl_sys *sys, const char *home, const char *host, long port)
{
    char text[512];
    if (!host || !*host || port < 1 || port > 65535 || setup_remote_text(host, port, text, sizeof(text)))
        return -1;
    return write_home(sys, home, "remote", text);
}

int setup_mark_done(cl_sys *sys, const char *home)
{
    return write_home(sys, home, "setup-done", "C:Claude's setup wizard has run (/setup runs it again).\n");
}

static int exists(cl_repl *r, const char *name)
{
    char file[300];
    return path_join(r->home, name, file, sizeof(file)) == 0 && r->sys->kind(r->sys->u, file) != 0;
}

int setup_due(cl_repl *r)
{
    return r->url.tls && !(r->key && *r->key) && !exists(r, "remote") && !exists(r, "setup-done");
}

/* the host and port the remote file holds now, 0 when it names none */
static int current_remote(cl_repl *r, char *host, long cap, long *port)
{
    char file[300], *t = 0;
    long n = 0;
    int ok;
    if (path_join(r->home, "remote", file, sizeof(file)) || r->sys->read(r->sys->u, file, 511, &t, &n) || !t)
        return 0;
    ok = cli_remote_parse(t, host, cap, port);
    free(t);
    return ok;
}

/* The choices of the three menus; the screen shows them as a menu (ui_pick,
 * the picker /model uses), the line mode numbers them. One text for both. */
static const char *const mode_opt[] = {
    "Claude Code on another computer (your Claude subscription; this window connects to it)",
    "An Anthropic API key here (Claude talks to Anthropic directly; needs AmiSSL 5)",
    "Later (nothing is stored; /setup comes back to this)"
};
static const char *const keep_opt[] = {
    "Store these settings anyway",
    "Enter the address again",
    "Back to the first question"
};
static const char *const info_opt[] = {
    "Show the page about amimcp and amiagent",
    "Skip it"
};

/* a menu's question in the screen: the option's index, -1 Esc. In the line
 * mode (no screen: plain, -p, scripts) the options are numbered and the
 * answer is typed: -2, the step's own line handler reads it. */
static int ask(cl_repl *r, const char *title, const char *const *opt, int n, const char *typed)
{
    int i;
    char m[200], num[16];
    if (r->ui.tui)
        return ui_pick(&r->ui, title, opt, n, 0);
    ui_line(&r->ui, title);
    for (i = 0; i < n; i++) {
        cl_ltoa(i + 1, num);
        cl_copy(m, "  ", sizeof(m));
        cl_cat(m, num, sizeof(m));
        cl_cat(m, "  ", sizeof(m));
        cl_cat(m, opt[i], sizeof(m));
        ui_line(&r->ui, m);
    }
    ui_line(&r->ui, typed);
    return -2;
}

static void mode_chosen(cl_repl *r, int c);

static void page_host(cl_repl *r);
static void page_key(cl_repl *r);
static void page_mode(cl_repl *r)
{
    int c;
    ui_line(&r->ui, "");
    r->wiz.step = SETUP_MODE;
    c = ask(r, "Setup, step 1 of 4: how do you want to use Claude on this Amiga?", mode_opt, 3,
            "Type 1, 2 or 3 and press Enter.");
    if (c == -1) {                      /* Esc at the first question: nothing to go back to */
        r->wiz.step = SETUP_OFF;
        ui_line(&r->ui, "Setup left. Nothing was stored; /setup comes back to this.");
    } else if (c >= 0)
        mode_chosen(r, c + 1);
}

void setup_begin(cl_repl *r)
{
    memset(&r->wiz, 0, sizeof(r->wiz));
    ui_line(&r->ui, "Welcome to Claude on the Amiga. This takes a minute; a line starting with / leaves the setup.");
    page_mode(r);
}

static void page_host(cl_repl *r)
{
    char host[200], m[300], num[16];
    long port;
    ui_line(&r->ui, "");
    ui_line(&r->ui, "Setup, step 2 of 4: the computer that runs Claude Code.");
    ui_line(&r->ui, "On that computer run UP-Term's uptelnetd (tools/uptelnetd.py), for example with");
    ui_line(&r->ui, "--command 'tmux new -A -s claude claude'. It serves that one command to this Amiga over");
    ui_line(&r->ui, "telnet on your own network (port 2323, not encrypted, never forward it to the internet).");
    if (current_remote(r, host, sizeof(host), &port)) {
        cl_copy(m, "Now: ", sizeof(m));
        cl_cat(m, host, sizeof(m));
        cl_cat(m, " ", sizeof(m));
        cl_ltoa(port, num);
        cl_cat(m, num, sizeof(m));
        cl_cat(m, " (an empty line keeps it)", sizeof(m));
        ui_line(&r->ui, m);
    }
    ui_line(&r->ui, "Type its address and port, like 192.168.0.198 2323 (the port is 2323 when left out).");
    r->wiz.step = SETUP_HOST;
}

static void page_key(cl_repl *r)
{
    char m[200];
    ui_line(&r->ui, "");
    ui_line(&r->ui, "Setup, step 2 of 4: your Anthropic API key.");
    if (r->sys->info) {
        chk(r, r->sys->info(r->sys->u, "bsdsocket", m, sizeof(m)), "TCP/IP stack", m);
        chk(r, r->sys->info(r->sys->u, "amissl", m, sizeof(m)), "AmiSSL 5", m);
    }
    ui_line(&r->ui, "Paste the API key (from console.anthropic.com) and press Enter. It is stored in");
    ui_line(&r->ui, "ENVARC:Claude/key and never shown. An empty line goes back.");
    r->await_key = 1;
    r->wiz.step = SETUP_KEY;
}

static void info_chosen(cl_repl *r, int show);
static void finish(cl_repl *r);

static void page_info(cl_repl *r)
{
    int c;
    ui_line(&r->ui, "");
    ui_line(&r->ui, "Setup, step 3 of 4 (optional): let Claude control this Amiga.");
    r->wiz.step = SETUP_INFO;
    c = ask(r, "Show the page about amimcp and amiagent?", info_opt, 2, "Type 1 or 2 (y shows it, n skips it).");
    if (c >= 0)
        info_chosen(r, c == 0);
    else if (c == -1)
        info_chosen(r, 0);              /* Esc: skipped */
}

static void show_info(cl_repl *r)
{
    ui_line(&r->ui, "amimcp and amiagent (github.com/thomas-luebker/amimcp) let Claude Code on another");
    ui_line(&r->ui, "computer read and drive this Amiga: amiagent runs here and waits for connections, amimcp");
    ui_line(&r->ui, "is the MCP server on that computer. They share a token: set it on both sides, and anything");
    ui_line(&r->ui, "that connects without it is refused. Use them on your local network only, and never");
    ui_line(&r->ui, "forward the port to the internet: whoever holds the token controls this machine.");
}

static void finish(cl_repl *r)
{
    char m[300], num[16];
    int rc = setup_mark_done(r->sys, r->home);
    ui_line(&r->ui, "");
    ui_line(&r->ui, "Setup, step 4 of 4: done.");
    if (r->wiz.mode == 1) {
        cl_copy(m, r->wiz.host, sizeof(m));
        cl_cat(m, " ", sizeof(m));
        cl_ltoa(r->wiz.port, num);
        cl_cat(m, num, sizeof(m));
        say2(r, "  Claude Code runs on: ", m);
        ui_line(&r->ui, "  Type /exit, then Claude: it connects there with C:uptelnet in this window.");
    } else if (r->wiz.mode == 2) {
        ui_line(&r->ui, "  The API key is stored in ENVARC:Claude/key. Type your first prompt below.");
    } else
        ui_line(&r->ui, "  Nothing was stored. /login stores a key, /setup runs this again.");
    if (rc)
        say2(r, "  The setup could not be marked as done: ", r->sys->err(r->sys->u));
    r->wiz.step = SETUP_OFF;
}

static void mode_chosen(cl_repl *r, int c)
{
    r->wiz.mode = c;
    if (c == 1)
        page_host(r);
    else if (c == 2)
        page_key(r);
    else
        finish(r);
}

static void info_chosen(cl_repl *r, int show)
{
    if (show)
        show_info(r);
    finish(r);
}

/* the keep question after a failed connect: 0 store anyway, 1 the address
 * again, 2 back to the first question */
static void keep_chosen(cl_repl *r, int c)
{
    if (c == 0) {
        if (setup_write_remote(r->sys, r->home, r->wiz.host, r->wiz.port))
            say2(r, "  Could not store ENVARC:Claude/remote: ", r->sys->err(r->sys->u));
        else
            say2(r, "  Stored in ", "ENVARC:Claude/remote");
        page_info(r);
    } else if (c == 1)
        page_host(r);
    else
        page_mode(r);
}

static int yes(const char *s)
{
    return (s[0] == 'y' || s[0] == 'Y') && (!s[1] || !strcmp(s + 1, "es") || !strcmp(s + 1, "ES"));
}

/* the connect test and the checks of step 2, remote mode */
static void test_remote(cl_repl *r)
{
    char m[300], num[16];
    int rc;
    ui_line(&r->ui, "Checks:");
    if (r->sys->info) {
        chk(r, r->sys->info(r->sys->u, "bsdsocket", m, sizeof(m)), "TCP/IP stack", m);
    }
    chk(r, r->sys->kind(r->sys->u, "C:uptelnet") == 1 ? 1 : 0, "C:uptelnet",
        r->sys->kind(r->sys->u, "C:uptelnet") == 1 ? "there" : "not there: run the UP-Term Install");
    cl_copy(m, r->wiz.host, sizeof(m));
    cl_cat(m, " port ", sizeof(m));
    cl_ltoa(r->wiz.port, num);
    cl_cat(m, num, sizeof(m));
    rc = r->net->open(r->net->u, r->wiz.host, (int)r->wiz.port, 0);
    if (rc == 0) {
        r->net->close(r->net->u);
        chk(r, 1, "Connect to the computer", m);
        setup_write_remote(r->sys, r->home, r->wiz.host, r->wiz.port) == 0
            ? say2(r, "  Stored in ", "ENVARC:Claude/remote")
            : say2(r, "  Could not store ENVARC:Claude/remote: ", r->sys->err(r->sys->u));
        page_info(r);
    } else {
        cl_copy(m, rc == NET_BREAK ? "stopped" : r->net->err(r->net->u), sizeof(m));
        chk(r, 0, "Connect to the computer", m[0] ? m : "failed");
        r->wiz.step = SETUP_KEEP;
        rc = ask(r, "The computer did not answer. Keep these settings anyway?", keep_opt, 3,
                 "Type 1, 2 or 3 (y stores them, n asks for the address again).");
        if (rc >= 0)
            keep_chosen(r, rc);
        else if (rc == -1)
            keep_chosen(r, 2);          /* Esc: back */
    }
}

int setup_line(cl_repl *r, const char *line)
{
    char s[300];
    long n;
    cl_copy(s, line, sizeof(s));
    for (n = (long)strlen(s); n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r'); n--)
        s[n - 1] = 0;
    for (n = 0; s[n] == ' ' || s[n] == '\t'; n++)
        ;
    if (n)
        memmove(s, s + n, strlen(s + n) + 1);
    if (s[0] == '/') {
        r->wiz.step = SETUP_OFF;
        r->await_key = 0;
        return 0;
    }
    switch (r->wiz.step) {
    case SETUP_MODE:
        if (!strcmp(s, "1") || !strcmp(s, "2") || !strcmp(s, "3"))
            mode_chosen(r, s[0] - '0');
        else
            ui_line(&r->ui, "Type 1, 2 or 3.");
        break;
    case SETUP_HOST: {
        char host[200];
        long port;
        if (!s[0]) {
            if (!current_remote(r, host, sizeof(host), &port)) {
                page_mode(r);
                break;
            }
        } else if (!cli_remote_parse(s, host, sizeof(host), &port)) {
            ui_line(&r->ui, "That is not an address and port. Type something like 192.168.0.198 2323.");
            break;
        }
        cl_copy(r->wiz.host, host, sizeof(r->wiz.host));
        r->wiz.port = port;
        test_remote(r);
        break;
    }
    case SETUP_KEEP:
        if (yes(s) || !strcmp(s, "1"))
            keep_chosen(r, 0);
        else if (!strcmp(s, "n") || !strcmp(s, "N") || !strcmp(s, "no") || !strcmp(s, "2"))
            keep_chosen(r, 1);
        else if (!strcmp(s, "3"))
            keep_chosen(r, 2);
        else
            ui_line(&r->ui, "Type 1, 2 or 3.");
        break;
    case SETUP_KEY:
        r->await_key = 0;
        if (!s[0]) {
            page_mode(r);
            break;
        }
        memset(r->keybuf, 0, sizeof(r->keybuf));
        slash_run(r, "/login", s);      /* the key's one path: checked, stored, in use */
        memset(s, 0, sizeof(s));
        if (!r->keybuf[0]) {
            page_key(r);                /* not usable: asked again */
            break;
        }
        ui_line(&r->ui, "Testing the key with one request of one token:");
        if (repl_ping(r) == 0)
            chk(r, 1, "Anthropic API", "the key was accepted");
        else
            chk(r, 0, "Anthropic API", "the test request failed (see above); /login stores another key");
        page_info(r);
        break;
    case SETUP_INFO:
        if (yes(s) || !strcmp(s, "1"))
            info_chosen(r, 1);
        else if (!strcmp(s, "n") || !strcmp(s, "N") || !strcmp(s, "no") || !strcmp(s, "2") || !s[0])
            info_chosen(r, 0);
        else
            ui_line(&r->ui, "Type 1 or 2.");
        break;
    default:
        return 0;
    }
    return 1;
}
