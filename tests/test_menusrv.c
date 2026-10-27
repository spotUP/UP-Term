/* The warm menu server's open decision (amarchy/menusrv_core.c; plan 2026-10-27-amarchy-warm-menu-server.md
 * section 5). Sentinel counters prove which path ran: a live server gets the message and nothing is spawned. */
#include "../amarchy/menusrv_core.h"
#include "harness.h"

typedef struct {
    int found, alive, outstanding;
    long clock;
    int sent, spawned, started, took;
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
    o.st = st;
    r.kind = 0;
    r.node = 0;
    return ms_open(&o, &r);
}

static void test_live_server(void)
{
    fake f = { 1, 1, 0, 100, 0, 0, 0, 0 };
    ms_state st = { 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SEND);
    CHECK_INT(f.sent, 1);
    CHECK_INT(f.spawned, 0);
    CHECK_INT(f.started, 0);
    CHECK_INT(f.took, 0);
}

static void test_no_port(void)
{
    fake f = { 0, 0, 0, 100, 0, 0, 0, 0 };
    ms_state st = { 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 1);
    CHECK_INT(f.took, 0);
}

/* AMARCHY-MENU-STALE-PORT: a port whose owner task is gone is taken over, the menu still opens */
static void test_stale_port(void)
{
    fake f = { 1, 0, 0, 100, 0, 0, 0, 0 };
    ms_state st = { 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.took, 1);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 1);
}

/* a wedged server (alive, an earlier message unreplied): fall back, do not start a second server */
static void test_unreplied(void)
{
    fake f = { 1, 1, 1, 100, 0, 0, 0, 0 };
    ms_state st = { 0, 0 };
    CHECK_INT(open_once(&f, &st), MS_SPAWN_RESTART);
    CHECK_INT(f.sent, 0);
    CHECK_INT(f.spawned, 1);
    CHECK_INT(f.started, 0);
}

static void test_restart_throttle(void)
{
    fake f = { 0, 0, 0, 100, 0, 0, 0, 0 };
    ms_state st = { 0, 0 };
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

void suite_menusrv(void)
{
    test_live_server();
    test_no_port();
    test_stale_port();
    test_unreplied();
    test_restart_throttle();
    test_verdicts();
}
