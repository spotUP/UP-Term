/* AMARCHY-WIN-REFRESH-UAF: win_refresh_frames() refreshes only windows that cannot close under the unlocked
 * RefreshWindowFrame. A fake Intuition (tests/win_refresh_host): the list is read under LockIBase; the fake UnlockIBase
 * simulates Directory Opus closing a window in the gap (unlinks it); the fake RefreshWindowFrame fails when handed a window
 * that is no longer listed (the use-after-free). Built and run by tests/test_win_refresh.py. */
#include <stdio.h>
#include <string.h>
#include "../amarchy/win_refresh.h"
#include <proto/intuition.h>
#include <proto/exec.h>

#define CALL(wb) win_refresh_frames(wb)

struct IntuitionBase *IntuitionBase;
static struct IntuitionBase g_ib;
static struct Screen g_scr;
static struct Task g_me, g_wb, g_dopus;
static struct MsgPort g_port_me, g_port_wb, g_port_dopus;
static struct Window w_me, w_wb, w_dopus, w_noport;
static struct Window *g_unlink_on_unlock;      /* closed by "another task" when the lock is dropped the second time: after the list was
                                                * read and the first window re-checked alive, before RefreshWindowFrame runs on it */
static int g_unlocks;
static int g_fails, g_uaf, g_refreshed[4];
static struct Window *g_all[4] = { &w_me, &w_wb, &w_dopus, &w_noport };

struct Task *FindTask(STRPTR name)
{
    return name ? &g_wb : &g_me;
}

ULONG LockIBase(ULONG d) { (void)d; return 1; }

static int listed(const struct Window *w)
{
    const struct Window *x;
    for (x = g_scr.FirstWindow; x; x = x->NextWindow)
        if (x == w)
            return 1;
    return 0;
}

void UnlockIBase(ULONG l)
{
    struct Window **pp;
    (void)l;
    g_unlocks++;
    if (!g_unlink_on_unlock || g_unlocks < 2)
        return;
    for (pp = &g_scr.FirstWindow; *pp; pp = &(*pp)->NextWindow)
        if (*pp == g_unlink_on_unlock) {
            *pp = g_unlink_on_unlock->NextWindow;
            break;
        }
    g_unlink_on_unlock = 0;
}

void RefreshWindowFrame(struct Window *w)
{
    int i;
    if (!listed(w))
        g_uaf++;                                /* a window closed behind our back */
    for (i = 0; i < 4; i++)
        if (g_all[i] == w)
            g_refreshed[i]++;
}

static void reset(void)
{
    memset(g_refreshed, 0, sizeof g_refreshed);
    g_uaf = 0;
    g_unlink_on_unlock = 0;
    g_unlocks = 0;
    win_refresh_count = 0;
    IntuitionBase = &g_ib;
    g_ib.FirstScreen = &g_scr;
    g_scr.NextScreen = 0;
    g_port_me.mp_SigTask = &g_me;
    g_port_wb.mp_SigTask = &g_wb;
    g_port_dopus.mp_SigTask = &g_dopus;
    w_me.UserPort = &g_port_me;
    w_wb.UserPort = &g_port_wb;
    w_dopus.UserPort = &g_port_dopus;
    w_noport.UserPort = 0;
    g_scr.FirstWindow = &w_dopus;
    w_dopus.NextWindow = &w_wb;
    w_wb.NextWindow = &w_noport;
    w_noport.NextWindow = &w_me;
    w_me.NextWindow = 0;
}

static void check(int ok, const char *what)
{
    if (!ok) {
        printf("FAIL: %s\n", what);
        g_fails++;
    }
}

int main(void)
{
    int done;
    /* a foreign-task window closes while the lock is dropped between a re-check and the refresh: it must never reach RefreshWindowFrame */
    reset();
    g_unlink_on_unlock = &w_dopus;
    done = CALL(1);
    check(g_uaf == 0, "no window handed to RefreshWindowFrame after another task closed it (use-after-free)");
    check(g_refreshed[2] == 0, "foreign-task (DOpus) window is not refreshed");
    check(g_refreshed[3] == 0, "window without a UserPort is skipped");
    /* the same race on the Workbench window while Workbench is not ready: skipped, so nothing to free */
    reset();
    g_unlink_on_unlock = &w_wb;
    CALL(0);
    check(g_uaf == 0, "Workbench window closing in the gap is never refreshed when Workbench is not ready");
    /* own windows are refreshed, counted by the sentinel */
    reset();
    done = CALL(0);
    check(g_refreshed[0] == 1, "own-task window refreshed");
    check(g_refreshed[1] == 0, "Workbench window not refreshed while Workbench is busy");
    check(done == 1 && win_refresh_count == 1, "returns 1 and win_refresh_count is 1 (own window only)");
    reset();
    done = CALL(1);
    check(g_refreshed[0] == 1 && g_refreshed[1] == 1, "own and Workbench windows refreshed when Workbench is ready");
    check(g_refreshed[2] == 0 && g_refreshed[3] == 0, "still no DOpus or portless window");
    check(done == 2 && win_refresh_count == 2, "returns 2 and win_refresh_count is 2");
    if (g_fails) {
        printf("test_win_refresh: %d FAILED\n", g_fails);
        return 1;
    }
    printf("test_win_refresh: PASS\n");
    return 0;
}
