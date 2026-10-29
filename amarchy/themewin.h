/* The themed borderless window helper (roadmap R2.5a), extracted from bar.c: a window opened on a public screen at
 * x/y/w/h that is never activated, drawn in the pens of the current Omarchy GUI theme (menu_theme.h roles), all on
 * ONE shared user port so the commodity waits on one signal bit for every helper window. The bar is the first user;
 * the notice window (P4) adds IDCMP_MOUSEBUTTONS through the same helper. Amiga only. */
#ifndef AMARCHY_THEMEWIN_H
#define AMARCHY_THEMEWIN_H

#include "menu_theme.h"
#include "gthemes_core.h"

struct Screen;
struct Window;

/* graphics.library (the global GfxBase), counted: the first tw_libs_open opens it, the last tw_libs_close closes it.
 * tw_open needs it open. 1 on success. */
int tw_libs_open(void);
void tw_libs_close(void);

/* Opens a borderless, never-activated, simple-refresh window titled title on scr, with the right mouse button
 * trapped, listening for idcmp on the shared port. The pens are taken on the first window and kept until the last
 * closes. 0 on failure (nothing is left open). */
typedef void (*tw_redraw_fn)(struct Window *win);
/* redraw (may be 0) repaints the whole window; tw_tick calls it after a theme switch retook the pens. */
struct Window *tw_open(struct Screen *scr, int x, int y, int w, int h, const char *title, unsigned long idcmp,
                       tw_redraw_fn redraw);
void tw_close(struct Window *win);

/* The commodity tick (tick_hook): when the saved theme is no longer the one the pens were taken for (tw_theme_changed),
 * the pens are released and taken again, and every open helper window's redraw runs (R2.5b). */
void tw_tick(long now);
/* "theme=<name> recolours=N bg=RRGGBB text=RRGGBB bgpen=N": the theme the pens belong to, how often they were retaken,
 * the role colours asked for and the pen index in use (BAR PENS; theme=- while no helper window is open). */
void tw_describe(char *out, int cap);

/* The pen of a theme role (MT_BG, MT_TEXT, MT_ACCENT, MT_BORDER_ROLE, MT_OK, MT_FAIL): a role the screen had no pen
 * for answers a safe fallback. Valid while a helper window is open. */
long tw_pen(int role);

/* Drawing, in window coordinates. tw_text draws s with its baseline one pixel below the font's, fg on bg. */
void tw_text(struct Window *win, int x, const char *s, int fg, int bg);
void tw_fill(struct Window *win, int x0, int y0, int x1, int y1, int role);
void tw_frame(struct Window *win, int x0, int y0, int x1, int y1, int role);
/* Width in pixels of s in the window's font. */
int tw_text_px(struct Window *win, const char *s);

/* The shared port: its signal mask (0 when no helper window is open), and the next message as plain values. tw_next
 * answers 1 and fills the fields (replying to the message), 0 when the port is empty. */
unsigned long tw_sigmask(void);
int tw_next(struct Window **win, unsigned long *cls, unsigned *code, int *mx, int *my);

#endif
