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

/* One Workbench message: `opened` is its value (0 = about to close, else
 * opened or the close failed); `eligible` says the console has a window it
 * may close and reopen on its own (a plain Workbench window: no tabs, no own
 * screen, not a foreign window). */
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

/* The reopen failed: stay parked, so the next "opened" message tries again. */
static void wbpark_failed(wbpark *p)
{
    p->parked = 1;
}

#endif
