/* WAIT_CHAR packets by task (handler/waitset.c): what pty-handler and the
 * XCON: handler keep for ixemul 80.x, whose select has a packet out per
 * process. */
#include "harness.h"
#include "../handler/waitset.h"

static int pk[6], task[3];

static ws_time at(unsigned long s, unsigned long us)
{
    ws_time t;
    t.s = s;
    t.us = us;
    return t;
}

/* the symptom with ixemul 80.1: two processes selecting one terminal each
 * ended the other's WAIT_CHAR, so neither could wait */
static void two_tasks_wait_side_by_side(void)
{
    waitset s = {{{0}}, 0};
    CHECK(ws_drop_task(&s, &task[0]) == 0);
    CHECK(ws_add(&s, &pk[0], &task[0], at(10, 0), 1000000) == 0);
    CHECK(ws_drop_task(&s, &task[1]) == 0);   /* the other task's: none */
    CHECK(ws_add(&s, &pk[1], &task[1], at(10, 500), 1000000) == 0);
    CHECK_INT(s.n, 2);
    /* input: both are answered, the oldest first */
    CHECK(ws_take(&s) == &pk[0]);
    CHECK(ws_take(&s) == &pk[1]);
    CHECK(ws_take(&s) == 0);
}

/* ixemul's select sends one per call, and its close sends one with no
 * timeout to end the one still out: the same task's older packet goes */
static void a_newer_wait_ends_the_same_tasks_older_one(void)
{
    waitset s = {{{0}}, 0};
    ws_add(&s, &pk[0], &task[0], at(10, 0), 1000000);
    ws_add(&s, &pk[1], &task[1], at(10, 0), 1000000);
    CHECK(ws_drop_task(&s, &task[0]) == &pk[0]);
    CHECK_INT(s.n, 1);
    CHECK(s.w[0].pk == &pk[1]);
}

static void each_waiter_has_its_own_deadline(void)
{
    waitset s = {{{0}}, 0};
    ws_add(&s, &pk[0], &task[0], at(10, 900000), 10000000);  /* until 20.9 */
    ws_add(&s, &pk[1], &task[1], at(11, 0), 200000);         /* until 11.2 */
    CHECK_INT(ws_next(&s, at(11, 100000)), 100000);
    CHECK(ws_expired(&s, at(11, 100000)) == 0);
    CHECK(ws_expired(&s, at(11, 200000)) == &pk[1]);
    CHECK(ws_expired(&s, at(11, 200000)) == 0);
    CHECK_INT(ws_next(&s, at(11, 200000)), 9700000);
    CHECK_INT(ws_next(&s, at(30, 0)), 0);                    /* passed */
    CHECK(ws_expired(&s, at(30, 0)) == &pk[0]);
    CHECK_INT(ws_next(&s, at(30, 0)), -1);                   /* no one waits */
}

static void a_full_set_lets_the_oldest_go(void)
{
    waitset s = {{{0}}, 0};
    int i;
    static int many[WS_MAX + 1], who[WS_MAX + 1];
    for (i = 0; i < WS_MAX; i++)
        CHECK(ws_add(&s, &many[i], &who[i], at(1, 0), 1000) == 0);
    CHECK(ws_add(&s, &many[WS_MAX], &who[WS_MAX], at(1, 0), 1000) == &many[0]);
    CHECK_INT(s.n, WS_MAX);
}

static void a_long_timeout_does_not_overflow(void)
{
    waitset s = {{{0}}, 0};
    ws_add(&s, &pk[0], &task[0], at(0, 0), 0x7fffffffUL);   /* ~35 minutes */
    CHECK(ws_next(&s, at(0, 0)) > 0);
}

void suite_waitset(void)
{
    two_tasks_wait_side_by_side();
    a_newer_wait_ends_the_same_tasks_older_one();
    each_waiter_has_its_own_deadline();
    a_full_set_lets_the_oldest_go();
    a_long_timeout_does_not_overflow();
}
