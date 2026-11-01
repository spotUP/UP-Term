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
