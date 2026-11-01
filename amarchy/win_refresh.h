/* The window-list checks AmTile used on its own and the frame redraw the flat UI needs (FLAT-C9): one place, shared by
 * tile.c and desk_dos.c. IBase locked for the two alive checks. */
#ifndef AMARCHY_WIN_REFRESH_H
#define AMARCHY_WIN_REFRESH_H

struct Screen;
struct Window;

/* IBase locked: scr is still a screen of Intuition's. */
int screen_alive(const struct Screen *scr);

/* IBase locked: w is still a window of scr. */
int window_alive(const struct Screen *scr, const struct Window *w);

/* Redraw the frame of every window of every screen (RefreshWindowFrame), so the flat glyph hook and outline show or go at
 * once. IBase is held only to list the windows and to re-check each one; RefreshWindowFrame itself runs unlocked (it takes
 * layer locks). Returns the number of frames refreshed (win_refresh_count too: the reachability sentinel). */
int win_refresh_frames(void);
extern unsigned long win_refresh_count;

#endif
