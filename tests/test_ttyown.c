/* handler/ttyown.h: the owner of a window's termios mode. */
#include "harness.h"
#include "../handler/ttyown.h"

static void *dead_task; /* the one task the fake exec reports gone */

static int fake_alive(void *t)
{
    return t != 0 && t != dead_task;
}

typedef struct probe {
    ttyown *s;
    int leaves;
    int active_seen; /* what ttyown_active answered inside first() */
} probe;

static void count_leave(void *u)
{
    ((probe *)u)->leaves++;
}

/* entering feeds typed-ahead keys to the line discipline, which asks
 * whether the mode is active (tty_first -> to_tty -> tty_active) */
static void first_asks(void *u)
{
    probe *p = (probe *)u;
    p->active_seen = ttyown_active(p->s, fake_alive, count_leave, p);
}

void suite_ttyown(void);
void suite_ttyown(void)
{
    int a, b;
    ttyown s = { 0, 0 };
    probe p;
    p.s = &s;
    p.leaves = 0;
    p.active_seen = -1;

    /* keys typed ahead while a program enters termios mode do not end the
     * mode half way in (the vsh prompt offset after screen's "Cannot find
     * master process" error: owner set only after the keys were fed) */
    dead_task = 0;
    CHECK(ttyown_enter(&s, &a, first_asks, &p) == 1);
    CHECK_INT(p.active_seen, 1);
    CHECK_INT(p.leaves, 0);
    CHECK(s.on);
    CHECK(s.owner == &a);

    /* a second tcsetattr: no second first(), the newest setter owns it */
    p.active_seen = -1;
    CHECK(ttyown_enter(&s, &b, first_asks, &p) == 0);
    CHECK_INT(p.active_seen, -1);
    CHECK(s.owner == &b);
    CHECK(ttyown_active(&s, fake_alive, count_leave, &p));

    /* the owner's end ends the mode, once, and the leave runs while it is on */
    dead_task = &b;
    CHECK(!ttyown_active(&s, fake_alive, count_leave, &p));
    CHECK_INT(p.leaves, 1);
    CHECK(!s.on);
    CHECK(s.owner == 0);
    CHECK(!ttyown_active(&s, fake_alive, count_leave, &p));
    CHECK_INT(p.leaves, 1);

    /* after that, the next program entering gets a fresh first() */
    dead_task = 0;
    p.active_seen = -1;
    CHECK(ttyown_enter(&s, &a, first_asks, &p) == 1);
    CHECK_INT(p.active_seen, 1);
    CHECK_INT(p.leaves, 1);
}
