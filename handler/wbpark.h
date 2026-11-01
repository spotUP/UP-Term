/* wbpark: the console's window leaves the Workbench screen while it closes
 * and comes back when it opens again. screennotify.library (Stefan Becker,
 * Aminet util/libs/ScreenNotify10.lha) sends each Workbench client a message
 * before CloseWorkBench (value FALSE) and after OpenWorkBench or a failed
 * close (value TRUE). Without it, IPrefs' reset of the Workbench screen (a
 * new ScreenMode, font or palette) fails while our window is open and
 * Intuition shows "Intuition is attempting to reset the Workbench screen"
 * with Retry/Cancel (RIG31, WB-RESET-SCREENNOTIFY).
 *
 * This is the decision only; the handler (vtcon_handler.c wb_msgs) closes
 * and reopens the window. No OS calls, so the host tests cover it
 * (tests/test_wbpark.c). The zeroed value is "not parked". Portable C89.
 */
#ifndef WBPARK_H
#define WBPARK_H

#define WBPARK_NONE 0 /* nothing to do */
#define WBPARK_HIDE 1 /* close the window now, before the reply */
#define WBPARK_SHOW 2 /* reply first, then reopen the window */

typedef struct {
    int parked; /* the window was closed for a Workbench close */
    int was_front; /* it was the frontmost window of its screen when it closed */
    int was_active; /* it was the active window when it closed */
    int tries; /* raise ticks left after the reopen */
    int covered; /* a tick saw the window covered: the drawer window has appeared */
} wbpark;

/* The host console of a window (with tabs or without) may close and reopen it: its own window on the
 * Workbench screen, not a tab (the host parks the tabs), not a foreign window, not a screen of its own.
 * Tabs make no difference: every tab lets go of the window first and takes the new one (TM_PARK,
 * TM_UNPARK in vtcon_handler.c). */
static int wbpark_eligible(int own_win, int has_term, int is_tab, int foreign, int own_screen, int on_wb_screen)
{
    return own_win && has_term && !is_tab && !foreign && !own_screen && on_wb_screen;
}

/* ACTION_DISK_INFO's id_VolumeNode is the console's Window pointer. While parked there is none: NULL.
 * Callers (DOS requesters, Info, programs reading ConUnit) take NULL as "no window" and fall back to the
 * Workbench screen; blocking until the reopen would stall the caller inside the very Workbench close that
 * waits for every client, and opening a window would make CloseWorkBench fail. */
static void *wbpark_disk_window(int parked, void *win)
{
    return parked ? 0 : win;
}

/* One Workbench message: `opened` is its value (0 = about to close, else
 * opened or the close failed); `eligible` is wbpark_eligible. */
static int wbpark_note(wbpark *p, int opened, int eligible)
{
    if (!opened) {
        if (p->parked || !eligible)
            return WBPARK_NONE;
        p->parked = 1;
        return WBPARK_HIDE;
    }
    if (!p->parked)
        return WBPARK_NONE;
    p->parked = 0;
    return WBPARK_SHOW;
}

/* IPrefs 3.1 (40.7) counts the windows on the Workbench screen that lack
 * WFLG_WBENCHWINDOW and shows the "close all windows" requester without ever
 * calling CloseWorkBench, so the screennotify patch never runs. A console
 * window on the Workbench screen therefore opens with WA_WBenchWindow: IPrefs
 * then calls CloseWorkBench, screennotify tells us, and we park. Only when
 * screennotify is there to park it (a flagged window nobody parks would make
 * CloseWorkBench fail silently) and only on the Workbench screen. */
static int wbpark_flag_window(int notify_present, int on_wb_screen)
{
    return notify_present && on_wb_screen;
}

/* The window about to close: remember where it stood, so the reopen puts it back. The reopened Workbench
 * (its drawer windows open after the console's) otherwise covers the new window and keeps the activation. */
static void wbpark_remember(wbpark *p, int front, int active)
{
    p->was_front = front != 0;
    p->was_active = active != 0;
}

#define WBPARK_TO_FRONT 1
#define WBPARK_ACTIVATE 2

/* The Workbench opens its drawer windows a while AFTER the console's window is back, over it and active. So the
 * restore is repeated once a second for WBPARK_RAISE_TRIES seconds (the handler's raise timer). */
#define WBPARK_RAISE_TRIES 10

/* Front means depth, not open order: no window layer lies in front of ours. Screen->FirstWindow is the order the
 * windows were opened in and says nothing about which one is on top. The handler counts the layers in front of
 * the window (Layer->front chain, the screen's title bar layer left out). */
static int wbpark_in_front(int layers_in_front)
{
    return layers_in_front == 0;
}

/* The chain walk: layers lying in front of `own`, following `next` (Layer->front) from `own`, `bar` (the screen's
 * title bar layer, may be NULL) left out. `own` NULL: 0. The handler passes the real Layer chain, the host test a
 * fake one, so the walk is tested as it runs. */
static int wbpark_layers_in_front(const void *own, const void *bar, const void *(*next)(const void *))
{
    const void *l;
    int n = 0;
    if (!own)
        return 0;
    for (l = next(own); l; l = next(l))
        if (!bar || l != bar)
            n++;
    return n;
}

/* The user picked another window: one is active (`any_active`), it is not ours (`is_own`) and it is not a
 * Workbench drawer window (`wbench_window`, WFLG_WBENCHWINDOW). A drawer window taking the activation is the
 * reopened Workbench, not the user, so the retries go on. The handler passes IntuitionBase->ActiveWindow's
 * state, the host test plain values. */
static int wbpark_user_other(int any_active, int is_own, int wbench_window)
{
    return any_active && !is_own && !wbench_window;
}

/* After the reopen: what to do with the new window (WBPARK_TO_FRONT, WBPARK_ACTIVATE bits), and the retries start. */
static int wbpark_restore(wbpark *p)
{
    p->tries = WBPARK_RAISE_TRIES;
    p->covered = 0;
    return (p->was_front ? WBPARK_TO_FRONT : 0) | (p->was_active ? WBPARK_ACTIVATE : 0);
}

/* One tick of the raise timer: `front` / `active` say where the window stands now; `user_other` says another
 * window that is not a Workbench drawer window holds the activation (the user picked it). Returns the bits still
 * to do. The rule: retry for WBPARK_RAISE_TRIES ticks; stop early once the window was seen covered (the drawer
 * window appeared) and stands in front again; stop at once, raising nothing, when the user activated another
 * window. The last tick forgets what was remembered. */
static int wbpark_retry(wbpark *p, int front, int active, int user_other)
{
    int r;
    if (p->tries <= 0)
        return 0;
    if (p->was_active && user_other) {
        p->tries = 0;
        p->was_front = p->was_active = 0;
        return 0;
    }
    if (p->was_front && !front)
        p->covered = 1;
    r = (p->was_front && !front ? WBPARK_TO_FRONT : 0) | (p->was_active && !active ? WBPARK_ACTIVATE : 0);
    if (--p->tries == 0 || (p->covered && !r))
        p->tries = 0, p->was_front = p->was_active = 0;
    return r;
}

static int wbpark_raise_pending(const wbpark *p)
{
    return p->tries > 0;
}

/* The reopen failed: stay parked, so the next "opened" message tries again. */
static void wbpark_failed(wbpark *p)
{
    p->parked = 1;
}

/* A tab that does not answer TM_PARK must not stall the Workbench close inside IPrefs for ever (tab_sync used to
 * wait without a limit). 5 seconds: a healthy tab answers in a few ticks, one busy with a long write well within
 * a second; IPrefs shows its own Retry/Cancel requester when CloseWorkBench fails, so a late answer costs nothing
 * and an endless wait costs the whole Workbench. In 1/50 s ticks, polled every WBPARK_POLL_TICKS. */
#define WBPARK_WAIT_TICKS 250
#define WBPARK_POLL_TICKS 2

/* Wait for a reply, polling: 1 as soon as `got(u)` says so, 0 after WBPARK_WAIT_TICKS without one. `pause(n)`
 * sleeps n ticks. */
static int wbpark_wait(int (*got)(void *), void (*pause)(int), void *u)
{
    int waited = 0;
    for (;;) {
        if (got(u))
            return 1;
        if (waited >= WBPARK_WAIT_TICKS)
            return 0;
        pause(WBPARK_POLL_TICKS);
        waited += WBPARK_POLL_TICKS;
    }
}

#endif
