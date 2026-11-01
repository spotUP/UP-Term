/* See win_refresh.h. */
#include <exec/types.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <proto/intuition.h>
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

int win_refresh_frames(void)
{
    struct Window *list[REFRESH_MAX];
    struct Screen *scr[REFRESH_MAX];
    int n = 0, i, done = 0;
    ULONG lock;
    if (!IntuitionBase)
        return 0;
    lock = LockIBase(0);
    {
        struct Screen *s;
        for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen) {
            struct Window *w;
            for (w = s->FirstWindow; w && n < REFRESH_MAX; w = w->NextWindow) {
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
