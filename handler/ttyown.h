/* handler/ttyown.h: who holds a window's termios mode (ACTION_VTCON_TCSETA).
 *
 * The mode lasts while the program that set it runs; the first check that
 * finds its task gone ends the mode (tty_leave, which also gives the window
 * back its LF-also-returns). Invariant: whenever the mode is on, the owner
 * is the task that set it last -- also while tty_enter is still entering,
 * because entering feeds the keys typed ahead to the line discipline, and
 * that path asks whether the mode is active. An owner assigned only after
 * that feed made the check see no owner, end the mode in the middle of
 * entering it, and leave the window with the mode off but its LF still
 * not returning (the vsh prompt offset right after GNU screen's
 * "Cannot find master process" error, ledger 2026-10-15).
 *
 * Header-only: the handler and the host test (tests/test_ttyown.c) share it. */
#ifndef TTYOWN_H
#define TTYOWN_H

typedef struct ttyown {
    int on;      /* in termios mode */
    void *owner; /* the task that set it; its end ends the mode */
} ttyown;

/* Enter (or stay in) termios mode for owner. first(u) runs only when the
 * mode was off, after the owner is in place: what it does may ask
 * ttyown_active. Returns 1 when the mode was off. */
static int ttyown_enter(ttyown *s, void *owner, void (*first)(void *), void *u)
{
    int was = s->on;
    s->owner = owner;
    s->on = 1;
    if (!was && first)
        first(u);
    return !was;
}

/* In termios mode while the owner runs. When alive(owner) says it is gone,
 * the mode is over: ttyown_active returns 0 and leaves the state off; the
 * caller's leave(u) runs first, while the mode is still on, to hand what the
 * line discipline holds back as plain input. */
static int ttyown_active(ttyown *s, int (*alive)(void *), void (*leave)(void *), void *u)
{
    if (!s->on)
        return 0;
    if (!alive(s->owner)) {
        if (leave)
            leave(u);
        s->on = 0;
        s->owner = 0;
    }
    return s->on;
}

#endif
