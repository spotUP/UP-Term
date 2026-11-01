#include <stdio.h>
#include <string.h>
#include "harness.h"
#include "../handler/wbpark.h"

/* A close hides an eligible window once; the open that follows shows it. */
static void close_then_open(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE); /* opened, never parked */
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_HIDE);
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_NONE); /* a second close: already gone */
    CHECK_INT(wbpark_note(&p, 1, 0), WBPARK_SHOW); /* eligibility only gates the close */
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE);
}

/* IPrefs 3.1 only reaches CloseWorkBench when our window carries WFLG_WBENCHWINDOW; the handler opens it with
 * WA_WBenchWindow exactly when screennotify can park it and the screen is the Workbench. */
static void flag_decision(void)
{
    CHECK_INT(wbpark_flag_window(1, 1), 1);
    CHECK_INT(wbpark_flag_window(0, 1), 0); /* nobody parks it: stay a plain window */
    CHECK_INT(wbpark_flag_window(1, 0), 0); /* an own or foreign screen */
    {   /* wired: create_window asks it and adds the tag */
        static char buf[400000];
        FILE *f = fopen("handler/vtcon_handler.c", "rb");
        size_t n = f ? fread(buf, 1, sizeof buf - 1, f) : 0;
        char *q;
        if (f) fclose(f);
        buf[n] = 0;
        q = strstr(buf, "wbpark_flag_window(");
        CHECK_INT(q != 0, 1);
        CHECK_INT(q && strstr(q, "WA_WBenchWindow") && strstr(q, "WA_WBenchWindow") - q < 400, 1);
    }
}

/* A window that may not leave (tabs, own screen) stays put. */
static void not_eligible(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 0, 0), WBPARK_NONE);
    CHECK_INT(wbpark_note(&p, 1, 0), WBPARK_NONE);
}

/* A failed reopen stays parked and the next open retries. */
static void failed_reopen_retries(void)
{
    wbpark p;
    memset(&p, 0, sizeof p);
    CHECK_INT(wbpark_note(&p, 0, 1), WBPARK_HIDE);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_SHOW);
    wbpark_failed(&p);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_SHOW);
    CHECK_INT(wbpark_note(&p, 1, 1), WBPARK_NONE);
}

static char hbuf[600000];

static const char *handler_src(void)
{
    FILE *f = fopen("handler/vtcon_handler.c", "rb");
    size_t n = f ? fread(hbuf, 1, sizeof hbuf - 1, f) : 0;
    if (f) fclose(f);
    hbuf[n] = 0;
    return hbuf;
}

/* A tabbed console parks like a plain one; DISK_INFO has no window while parked. */
static void tabs_park(void)
{
    const char *src = handler_src();
    char *e, *m, *d;
    int i;
    CHECK_INT(wbpark_eligible(1, 1, 0, 0, 0, 1), 1);
    CHECK_INT(wbpark_eligible(1, 1, 1, 0, 0, 1), 0); /* a tab: its host parks it */
    CHECK_INT(wbpark_eligible(1, 1, 0, 1, 0, 1), 0);
    CHECK_INT(wbpark_eligible(1, 1, 0, 0, 1, 1), 0);
    CHECK_INT(wbpark_eligible(1, 1, 0, 0, 0, 0), 0);
    CHECK_INT(wbpark_disk_window(1, (void *)src) == 0, 1);
    CHECK_INT(wbpark_disk_window(0, (void *)src) == (void *)src, 1);
    /* wired: the eligibility has no tab count, wb_msgs parks and unparks the tabs, DISK_INFO asks the helper */
    e = strstr(src, "static int wb_eligible(con *c)");
    CHECK_INT(e != 0, 1);
    CHECK_INT(e && strstr(e, "wbpark_eligible(") && strstr(e, "wbpark_eligible(") - e < 120, 1);
    CHECK_INT(e && strstr(e, "ntabs") && strstr(e, "ntabs") - e > 400, 1);
    m = strstr(src, "static void wb_msgs(con *c)");
    CHECK_INT(m && strstr(m, "wb_tabs_park(c)") && strstr(m, "wb_tabs_park(c)") < strstr(m, "window_unbind(c)"), 1);
    CHECK_INT(m && strstr(m, "wb_tabs_unpark(c)") && strstr(m, "wb_tabs_unpark(c)") > strstr(m, "window_rebind(c, scr, win)"), 1);
    d = strstr(src, "case ACTION_DISK_INFO:");
    CHECK_INT(d && strstr(d, "wbpark_disk_window(wb_is_parked(c)") && strstr(d, "wbpark_disk_window(") - d < 900, 1);
    for (i = 0; i < 2; i++) { /* the tab side answers both messages */
        const char *k = i ? "case TM_UNPARK:" : "case TM_PARK:";
        char *t = strstr(src, "static void tab_msgs(con *c)");
        CHECK_INT(t && strstr(t, k) != 0, 1);
    }
}

static int polls, pauses, answer_after;
static int got_after(void *u)
{
    (void)u;
    polls++;
    return answer_after >= 0 && pauses >= answer_after;
}
static void count_pause(int ticks)
{
    (void)ticks;
    pauses++;
}

/* The reopened window goes back in front and active when it was before the park (the reopened Workbench drawer
 * window otherwise covers it); the handler remembers it at park time and applies it after the rebind. */
typedef struct fakelayer {
    struct fakelayer *front;
} fakelayer;

static const void *fake_next(const void *l)
{
    return ((const fakelayer *)l)->front;
}

static void park_restore_order(void)
{
    wbpark p = {0, 0, 0, 0, 0};
    const char *src = handler_src();
    char *h, *s, *r;
    int i;
    wbpark_remember(&p, 1, 1);
    CHECK_INT(wbpark_restore(&p), WBPARK_TO_FRONT | WBPARK_ACTIVATE);
    /* front is depth (layers in front), never Screen->FirstWindow (open order) */
    CHECK_INT(wbpark_in_front(0), 1);
    CHECK_INT(wbpark_in_front(1), 0);
    CHECK_INT(strstr(src, "FirstWindow == ") == 0, 1);
    {
        /* behaviour: a fake layer chain. Windows were opened own first (screen window list: own is FIRST), yet a
         * later window lies in front by depth: the old FirstWindow rule said "front", depth says covered. */
        fakelayer bar = {0}, top = {0}, own = {0};
        own.front = &top; /* top is in front of own */
        top.front = &bar;
        CHECK_INT(wbpark_in_front(wbpark_layers_in_front(&own, &bar, fake_next)), 0);
        /* the reverse: own is not first in the window list (opened later) but nothing lies in front of it */
        top.front = 0;
        own.front = &bar; /* only the title bar layer in front: does not count */
        CHECK_INT(wbpark_in_front(wbpark_layers_in_front(&own, &bar, fake_next)), 1);
        own.front = 0;
        CHECK_INT(wbpark_in_front(wbpark_layers_in_front(&own, &bar, fake_next)), 1);
        CHECK_INT(wbpark_layers_in_front(0, &bar, fake_next), 0);
        /* the handler walks through the shared function */
        CHECK_INT(strstr(src, "wbpark_layers_in_front(win->WLayer") != 0, 1);
    }
    /* the drawer window opens later and covers it: each tick puts back what is missing; ticks run 10 s */
    CHECK_INT(wbpark_retry(&p, 1, 1, 0), 0);
    CHECK_INT(wbpark_retry(&p, 0, 0, 0), WBPARK_TO_FRONT | WBPARK_ACTIVATE);
    CHECK_INT(wbpark_raise_pending(&p), 1);
    CHECK_INT(wbpark_retry(&p, 1, 1, 0), 0); /* covered once, in front again: done early */
    CHECK_INT(wbpark_raise_pending(&p), 0);
    wbpark_remember(&p, 1, 1);
    wbpark_restore(&p);
    CHECK_INT(WBPARK_RAISE_TRIES, 10); /* the span is 10 one-second ticks: drawer windows open seconds late */
    for (i = 0; i < WBPARK_RAISE_TRIES - 1; i++)
        wbpark_retry(&p, 1, 1, 0); /* never covered: runs the whole span */
    CHECK_INT(wbpark_raise_pending(&p), 1);
    wbpark_retry(&p, 1, 1, 0);
    CHECK_INT(wbpark_raise_pending(&p), 0);
    CHECK_INT(wbpark_retry(&p, 0, 0, 0), 0);
    wbpark_remember(&p, 1, 1);
    wbpark_restore(&p);
    CHECK_INT(wbpark_retry(&p, 0, 0, 1), 0); /* the user activated another window: raise nothing */
    CHECK_INT(wbpark_raise_pending(&p), 0);
    /* who counts as "the user picked another window": not nothing active, not ours, not a drawer window */
    CHECK_INT(wbpark_user_other(1, 0, 0), 1);
    CHECK_INT(wbpark_user_other(1, 0, 1), 0); /* the reopened drawer window took it: keep retrying */
    CHECK_INT(wbpark_user_other(1, 1, 1), 0); /* ours (it carries WA_WBenchWindow too) */
    CHECK_INT(wbpark_user_other(0, 0, 0), 0);
    /* a drawer window holding the activation does not stop the retries; the window is raised again */
    wbpark_remember(&p, 1, 1);
    wbpark_restore(&p);
    CHECK_INT(wbpark_retry(&p, 0, 0, wbpark_user_other(1, 0, 1)), WBPARK_TO_FRONT | WBPARK_ACTIVATE);
    CHECK_INT(wbpark_raise_pending(&p), 1);
    CHECK_INT(strstr(src, "wbpark_user_other(a != 0") != 0, 1); /* the handler asks the shared rule */
    p.tries = 0;
    wbpark_remember(&p, 1, 0);
    CHECK_INT(wbpark_restore(&p), WBPARK_TO_FRONT);
    wbpark_remember(&p, 0, 0);
    CHECK_INT(wbpark_restore(&p), 0);
    h = strstr(src, "act == WBPARK_HIDE");
    s = strstr(src, "act == WBPARK_SHOW");
    r = s ? strstr(s, "wbpark_restore(&c->wb)") : 0;
    CHECK_INT(h && strstr(h, "wbpark_remember(&c->wb") && strstr(h, "wbpark_remember(&c->wb") < s, 1);
    CHECK_INT(r && strstr(r, "wb_raise(c, back)") && strstr(r, "ztimer_start(c)"), 1);
    CHECK_INT(strstr(src, "wbpark_retry(&c->wb") != 0, 1); /* the timer applies the retries */
}

/* A tab that never answers TM_PARK ends the wait after the bound, not never; one that answers ends it at once. */
static void park_wait_bounded(void)
{
    const char *src = handler_src();
    char *s, *m;
    int i;
    polls = pauses = 0;
    answer_after = -1; /* never */
    CHECK_INT(wbpark_wait(got_after, count_pause, 0), 0);
    CHECK_INT(pauses, WBPARK_WAIT_TICKS / WBPARK_POLL_TICKS);
    polls = pauses = 0;
    answer_after = 3;
    CHECK_INT(wbpark_wait(got_after, count_pause, 0), 1);
    CHECK_INT(pauses, 3);
    polls = pauses = 0;
    answer_after = 0;
    CHECK_INT(wbpark_wait(got_after, count_pause, 0), 1);
    CHECK_INT(pauses, 0);
    /* wired: tab_sync waits through wbpark_wait (no Wait on the reply port without a limit), a failed park
     * leaves the window open, puts the tabs back and answers the message */
    s = strstr(src, "static int tab_sync(con *c, con *to");
    CHECK_INT(s && strstr(s, "wbpark_wait(sync_got") && strstr(s, "wbpark_wait(sync_got") - s < 500, 1);
    CHECK_INT(s && strstr(s, "Wait(1UL << c->tab_reply") == 0 || (strstr(s, "Wait(1UL << c->tab_reply") - s > 700), 1);
    {   /* TM_UNPARK binds the tab to the screen of the host's window, never its own lock_screen */
        char *u = strstr(src, "case TM_UNPARK:");
        char *e2 = u ? strstr(u, "ReplyMsg(&t->msg)") : 0;
        CHECK_INT(u && e2 && strstr(u, "t->win->WScreen") && strstr(u, "t->win->WScreen") < e2, 1);
        CHECK_INT(u && e2 && (strstr(u, "lock_screen(c)") == 0 || strstr(u, "lock_screen(c)") > e2), 1);
    }
    {   /* the park and the unpark count panes (tabslot_all), not tab slots: one slot split in two has ntabs 1 */
        for (i = 0; i < 2; i++) {
            const char *name = i ? "static void wb_tabs_unpark(con *c)\n{" : "static int wb_tabs_park(con *c)\n{";
            char *b = strstr(src, name);
            char *end = b ? strstr(b, "\n}\n") : 0;
            char *g = b ? strstr(b, "tabslot_all(") : 0;
            char *bad = b ? strstr(b, "ntabs < 2") : 0;
            CHECK_INT(b && end && g && g < end, 1);
            CHECK_INT(bad == 0 || (end && bad > end), 1);
        }
    }
    m = strstr(src, "static void wb_msgs(con *c)");
    CHECK_INT(m && strstr(m, "if (wb_tabs_park(c))") && strstr(m, "wb_tabs_abort(c)") && strstr(m, "c->wb.parked = 0"), 1);
    CHECK_INT(m && strstr(m, "wb_tabs_abort(c)") < strstr(m, "ReplyMsg(&m->snm_Message)"), 1);
}

void suite_wbpark(void)
{
    close_then_open();
    flag_decision();
    not_eligible();
    failed_reopen_retries();
    tabs_park();
    park_restore_order();
    park_wait_bounded();
}
