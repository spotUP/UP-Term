/* See win_refresh.h. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
#include <proto/exec.h>
#include "win_refresh.h"

unsigned long win_refresh_count;

int screen_alive(const struct Screen *scr)
{
    const struct Screen *s;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        if (s == scr)
            return 1;
    return 0;
}

int window_alive(const struct Screen *scr, const struct Window *w)
{
    const struct Window *x;
    for (x = scr->FirstWindow; x; x = x->NextWindow)
        if (x == w)
            return 1;
    return 0;
}

#define REFRESH_MAX 64

/* Only windows that cannot close under us: the caller's own (a window closes only through its owner task, and the owner is
 * the one running here), and Workbench's while Workbench is idle (wb_ready). Every other task's window (Directory Opus) is
 * skipped: the list is read under LockIBase but RefreshWindowFrame must run unlocked, and in that gap the owner may close the
 * window (use-after-free, the intermittent freeze at the theme pick). Such programs repaint themselves on the theme send. */
int win_refresh_frames(int wb_ready)
{
    struct Window *list[REFRESH_MAX];
    struct Screen *scr[REFRESH_MAX];
    int n = 0, i, done = 0;
    ULONG lock;
    struct Task *me, *wb;
    if (!IntuitionBase)
        return 0;
    me = FindTask(0);
    wb = wb_ready ? FindTask((STRPTR)"Workbench") : 0;  /* compared as a pointer, never dereferenced */
    lock = LockIBase(0);
    {
        struct Screen *s;
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen) {
            struct Window *w;
            for (w = s->FirstWindow; w && n < REFRESH_MAX; w = w->NextWindow) {
                struct Task *owner = w->UserPort ? (struct Task *)w->UserPort->mp_SigTask : 0;
                if (!owner || (owner != me && owner != wb))
                    continue;
                list[n] = w;
                scr[n] = s;
                n++;
            }
        }
    }
    UnlockIBase(lock);
    for (i = 0; i < n; i++) {
        int alive;
        lock = LockIBase(0);
        alive = screen_alive(scr[i]) && window_alive(scr[i], list[i]);
        UnlockIBase(lock);
        if (alive) {
            RefreshWindowFrame(list[i]);
            win_refresh_count++;
            done++;
        }
    }
    return done;
}
