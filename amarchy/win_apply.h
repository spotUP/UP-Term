/* The two OS calls the win: verbs (win.c) and AmTile (tile.c) share: a window's box as win_core wants it, and the
 * request that moves and sizes a window. Both follow win.c's lock discipline: call them with IBase locked, on a
 * window that was just found in a screen's window list. */
#ifndef AMARCHY_WIN_APPLY_H
#define AMARCHY_WIN_APPLY_H

#include "win_core.h"

struct Window;

/* IBase locked: w's box (with its Min / Max limits; 0 for none) and its screen. */
void win_box_of(const struct Window *w, win_box *b, win_screen *s);

/* IBase locked: queue the box to Intuition (ChangeWindowBox only queues; it never waits on the lock). */
void win_apply_box(struct Window *w, const win_box *b);

/* IBase locked: queue WindowToFront (win.c WV_FRONT / WV_CYCLE and AmTile's monocle and CYCLE share it). */
void win_raise(struct Window *w);

#endif
