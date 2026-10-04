/* vtinput: what the window's input means, without Intuition. A wheel notch,
 * a button, a pointer move, a click run, a console key or a pasted character
 * is decided here from the terminal's modes; render/vtwin.c reads the events
 * and does what these say (reports to the program, selection, scrollback,
 * paste). Portable C89 like the engine, tested on the host (suite input). */
#ifndef VTINPUT_H
#define VTINPUT_H

#include "../engine/vtengine.h"

/* The text area in window pixels: its top left, the cell size and the
 * cells that fit (vr_render's ox, oy, cw, ch, cols, rows). */
typedef struct vti_geom {
    int ox, oy, cw, ch, cols, rows;
} vti_geom;

/* The cell under window pixel px, py: 1 inside the text area, 0 outside
 * (x, y untouched). */
int vti_cell_at(const vti_geom *g, int px, int py, int *x, int *y);

/* Lines a wheel notch moves (the view, or cursor keys with ?1007). */
#define VTI_WHEEL_LINES 3

/* A wheel notch (up 1, down 0) at window pixel px, py. A program that asked
 * for the mouse gets a report (buttons 64/65, the cell under the pointer,
 * clamped to the grid); on the alternate screen with ?1007 and no mouse mode
 * it gets VTI_WHEEL_LINES cursor keys (xterm's alternateScroll). Returns
 * the bytes in out (at most 32), or 0: then the window's scrollback moves
 * by *lines (up positive). view: how far back the window shows now -- a
 * scrolled-back view keeps the wheel for itself. */
int vti_wheel(const vti_geom *g, const vt_term *t, int view, int up, int mods,
              int px, int py, vt_u8 *out, int *lines);

/* The mouse between events: which buttons went to the program, the cell
 * the last report named, the run of clicks. */
typedef struct vti_mouse {
    int held;            /* bit b: button b's press was reported */
    int last_x, last_y;  /* the cell of the last report (-1: none yet) */
    int clicks;          /* 1, 2, 3: the click run the last local press was in */
    int click_x, click_y;
} vti_mouse;

/* No button held, no report made, no clicks: a window's start. */
void vti_mouse_reset(vti_mouse *m);

/* What a button event is for (vti_button). */
enum vti_action {
    VTI_NONE = 0,
    VTI_REPORT,      /* out holds the program's report */
    VTI_SELECT,      /* start a selection at the cell: m->clicks says char / word / line */
    VTI_SELECT_END,  /* the selecting button went up */
    VTI_PASTE        /* the middle button with nobody else asking: paste */
};

/* A button (0 left, 1 middle, 2 right) went down (down 1) or up at cell
 * x, y (in: the pointer is on the grid; x, y are only read when it is).
 * A program in a mouse mode gets it unless Shift is held (xterm: Shift
 * gives the mouse back to selection) or a local selection is on
 * (selecting); a release of a button whose press it got always goes to it.
 * dclick: Intuition's DoubleClick() says this press followed the last one
 * in time. *n gets the report's length (out: at most 32 bytes). */
int vti_button(vti_mouse *m, const vt_term *t, int btn, int down, int x, int y, int in,
               int mods, int selecting, int dclick, vt_u8 *out, int *n);

/* The pointer moved to cell x, y: the program's motion report (?1002 while
 * a reported button is down, ?1003 always), once per cell. Returns the
 * report's length in out, 0 for none. */
int vti_motion(vti_mouse *m, const vt_term *t, int x, int y, int mods, vt_u8 *out);

/* 1 while the program wants pointer moves (the window must report them):
 * ?1003, or ?1002 with a reported button down. */
int vti_wants_motion(const vti_mouse *m, const vt_term *t);

/* The word around cell x of row y (vt_row numbering, below 0 the
 * scrollback): *x0 .. *x1 inclusive. A word is a run of letters, digits and
 * the path and URL characters -#%&+,./=?@\_~: ; a run of blanks is one too,
 * any other character stands alone. 0 when the row does not exist. */
int vti_word(const vt_term *t, int x, int y, int *x0, int *x1);

/* The logical line row y belongs to (rows a wrap joined): *y0 .. *y1. */
void vti_line(const vt_term *t, int y, int *y0, int *y1);

/* Shift+PgUp / PgDn: 1 when they move the window's scrollback (the main
 * screen, no mouse mode), 0 when they belong to the program (terminfo's
 * kPRV / kNXT: the alternate screen has no scrollback of its own, and a
 * program that took the mouse runs full screen). */
int vti_page_keys_scroll(const vt_term *t);

/* A pasted character: 0 when it must be dropped. Inside bracketed paste
 * (?2004) only text, Tab and line breaks go through: ESC (which could end
 * the brackets early with ESC [ 201 ~ and type commands) and the other
 * C0 / C1 controls do not. */
int vti_paste_keeps(const vt_term *t, unsigned long ch);

#endif
