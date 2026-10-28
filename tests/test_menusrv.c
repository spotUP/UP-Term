/* The warm menu server's open decision (amarchy/menusrv_core.c; plan 2026-10-27-amarchy-warm-menu-server.md
 * section 5). Sentinel counters prove which path ran: a live server gets the message and nothing is spawned. */
#include "../amarchy/menusrv_core.h"
#include "harness.h"
#include "../amarchy/serve_sig.h"
#include <signal.h>

typedef struct {
    int found, alive, outstanding;
    long clock;
    int sent, spawned, started, took;
    long disk;                  /* the program's stamp on disk */
    int quits, quit_ok;
} fake;

static void f_find(void *c, int *found, int *alive, int *out)
{
    fake *f = c;
    *found = f->found;
    *alive = f->alive;
    *out = f->outstanding;
}
static void f_take(void *c) { ((fake *)c)->took++; }
static void f_send(void *c, const ms_req *r) { (void)r; ((fake *)c)->sent++; }
static void f_spawn(void *c, const ms_req *r) { (void)r; ((fake *)c)->spawned++; }
static void f_start(void *c) { ((fake *)c)->started++; }
static long f_now(void *c) { return ((fake *)c)->clock; }
static long f_stamp(void *c) { return ((fake *)c)->disk; }
static int f_quit(void *c) { ((fake *)c)->quits++; return ((fake *)c)->quit_ok; }

static int open_once(fake *f, ms_state *st)
{
    ms_ops o;
    ms_req r;
    o.ctx = f;
    o.find = f_find;
    o.take_over = f_take;
    o.send = f_send;
    o.spawn = f_spawn;
    o.start_server = f_start;
    o.now = f_now;
    o.stamp = f_stamp;
    o.quit = f_quit;
    o.st = st;
    r.kind = 0;
    r.node = 0;
    return ms_open(&o, &r);
}

static void test_live_server(void)
{
    fake f = { 1, 1, 0, 100, 0, 0, 0, 0, 0, 0, 0 };
    ms_state st = { 0, 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SEND);
    CHECK_INT(f.sent, 1);
    CHECK_INT(f.spawned, 0);
    CHECK_INT(f.started, 0);
    CHECK_INT(f.took, 0);
}

static void test_no_port(void)
{
    fake f = { 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0 };
    ms_state st = { 0, 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 1);
    CHECK_INT(f.took, 0);
}

/* AMARCHY-MENU-STALE-PORT: a port whose owner task is gone is taken over, the menu still opens */
static void test_stale_port(void)
{
    fake f = { 1, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0 };
    ms_state st = { 0, 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.took, 1);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 1);
}

/* a wedged server (alive, an earlier message unreplied): fall back, do not start a second server */
static void test_unreplied(void)
{
    fake f = { 1, 1, 1, 100, 0, 0, 0, 0, 0, 0, 0 };
    ms_state st = { 0, 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 0);
}

/* WARM-SERVER-NEW-BINARY: a hotkey press after the server program on disk changed does not reach the old
 * server: it opens a menu of its own (new code), the old server is told to end and a new one starts */
static void test_new_binary_replaces_server(void)
{
    fake f = { 1, 1, 0, 100, 0, 0, 0, 0, 5000, 0, 1 };
    ms_state st = { 90, 1, 5000 };
    CHECK_INT(open_once(&f, &st), MS_SEND);        /* same file: the warm path */
    CHECK_INT(f.sent, 1);
    CHECK_INT(f.quits, 0);
    f.disk = 6000;                                  /* upkg replaced the program */
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.sent, 1);                           /* not sent to the old server */
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.quits, 1);
    CHECK_INT(f.started, 1);
    CHECK_INT(st.bin_stamp == 6000, 1);
    f.quit_ok = 0;                                  /* a busy server will not end: left, tried again */
    f.disk = 7000;
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.started, 1);
    CHECK_INT(st.bin_stamp == 6000, 1);
    CHECK_INT(ms_binary_changed(0, 5), 0);          /* unknown stamps never restart */
    CHECK_INT(ms_binary_changed(5, 0), 0);
    CHECK_INT(ms_binary_changed(5, 5), 0);
    CHECK_INT(ms_binary_changed(5, 6), 1);
}

static void test_restart_throttle(void)
{
    fake f = { 0, 0, 0, 100, 0, 0, 0, 0, 0, 0, 0 };
    ms_state st = { 0, 0, 0 };
    open_once(&f, &st);
    f.clock = 105;
    open_once(&f, &st);
    CHECK_INT(f.spawned, 2);
    CHECK_INT(f.started, 1);        /* inside 10 s: no second start */
    f.clock = 100 + MS_RESTART_SECS;
    open_once(&f, &st);
    CHECK_INT(f.started, 2);
}

static void test_verdicts(void)
{
    CHECK_INT(mo_verdict(0, 0), MO_NONE);
    CHECK_INT(mo_verdict(1, 1), MO_SIGNAL);
    CHECK_INT(mo_verdict(1, 0), MO_TAKE_OVER);
    CHECK_INT(ms_route(1, 1, 0), MS_SEND);
    CHECK_INT(ms_route(1, 1, 1), MS_SPAWN_RESTART);
    CHECK_INT(ms_route(1, 0, 0), MS_SPAWN_RESTART);
    CHECK_INT(ms_route(0, 0, 0), MS_SPAWN_RESTART);
}

/* each menu style opens through its own server: the text menu's port and program, the drawn menu's */
static void test_server_names(void)
{
    const char *port, *task;
    ms_server_names(1, &port, &task);
    CHECK_STR(port, "AMARCHY.MENUSRV");
    CHECK_STR(task, "amarchy-menu");
    ms_server_names(0, &port, &task);
    CHECK_STR(port, "AMARCHY.GUISRV");
    CHECK_STR(task, "amarchy-gui");
}

/* A menu process (warm server and cold menu) ignores SIGHUP: an XCON console closing sent it "Hangup" and ended
 * it. The test raises it for real: unhandled, the default action ends this process before the CHECK below. */
static void test_menu_ignores_hangup(void)
{
    static volatile int survived;
    signal(SIGHUP, SIG_DFL);    /* a background-started test run inherits SIGHUP ignored: start from the default */
    menu_ignore_hangup();
    raise(SIGHUP);
    survived = 1;
    CHECK(survived == 1);
    signal(SIGHUP, SIG_DFL);
}

void suite_menusrv(void)
{
    test_live_server();
    test_no_port();
    test_stale_port();
    test_unreplied();
    test_new_binary_replaces_server();
    test_restart_throttle();
    test_verdicts();
    test_server_names();
    test_menu_ignores_hangup();
}
