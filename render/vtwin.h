/* vtwin: one terminal bound to one Intuition window -- the engine, the
 * renderer, the fonts, the frame clock, selection, copy/paste and key
 * translation. Shared by the XCON: handler (handler/vtcon_handler.c) and
 * the console.device units (device/, plan 2026-09-30-console-device.md
 * phase DX): what is the window's lives here, what is the owner's policy
 * (line discipline, line editing, break keys, DOS packets, who reads)
 * stays with the owner and is reached through vtwin_host.
 *
 * A vtwin lives inside its owner's per-process state (no statics: every
 * window's process runs the same code). Lifetime:
 *   vtwin_init      once: the frame clock
 *   (the owner fills the spec fields: pers, latin1, cp437, fg_rgb, bg_rgb,
 *    title, fontname/fontsize, altname/altsize)
 *   vtwin_open_font before the owner opens its window (its size may need it)
 *   vtwin_attach    the window is there: engine, renderer, first draw
 *   ... vtwin_write, vtwin_key, vtwin_mouse, vtwin_resize, vtwin_refresh,
 *       vtwin_tick on the frame clock's signal ...
 *   vtwin_detach    before the owner closes the window (it may attach again)
 *   vtwin_cleanup   once, at the end */
#ifndef VTWIN_H
#define VTWIN_H

#include <exec/types.h>
#include <devices/timer.h>
#include <intuition/intuition.h>
#include <graphics/text.h>
#include <devices/keymap.h>
#include "../engine/vtengine.h"
#include "amiga_render.h"

/* What the window needs from its owner. `user` is vtwin.user. */
typedef struct vtwin_host {
    /* the engine's reports (DSR, DA, window status...): the read stream */
    void (*reply)(void *user, const vt_u8 *b, long n);
    /* bytes that go straight into the read stream: raw event reports,
     * mouse reports, bracketed-paste markers */
    void (*input)(void *user, const vt_u8 *b, long n);
    /* a typed key, encoded (n bytes; key is a VT_KEY_* or 0 for a
     * character, mods VT_MOD_*): the owner's policy decides (breaks, line
     * discipline, line editor, raw) */
    void (*key)(void *user, const vt_u8 *b, int n, long key, int mods);
    /* a pasted key: as typed, but never a break */
    void (*pasted)(void *user, const vt_u8 *b, int n, long key);
    /* is the stream raw (keys go to the reader as they come)? */
    int (*raw)(void *user);
    /* the grid changed size (after vt_resize) */
    void (*resized)(void *user);
} vtwin_host;

typedef struct vtwin {
    /* spec, filled by the owner before vtwin_attach */
    enum vt_personality pers;
    int latin1, cp437;
    ULONG fg_rgb, bg_rgb;        /* VR_KEEP: the screen's pens */
    char title[80];              /* also where OSC titles land */
    char fontname[40];
    WORD fontsize;
    char altname[11][40];        /* FONT1..FONT9 (SGR 11-19), FRAKTUR (SGR 20) */
    WORD altsize[11];
    struct TextFont *given_font; /* draw with this font (the window's), not opened or closed here */
    int sb_lines;                /* scrollback lines: 0 = 500 (XCON:), -1 = none */
    struct KeyMap *keymap;       /* keys convert with this map; 0 = the system default */
    /* profile (config/upconf): the window's defaults the program's
     * sequences still override; 0 / VR_KEEP keep the historical look */
    int bold_bright;             /* xterm SGR 1 takes the bright 8-15 (1, default) */
    int bell;                    /* 0 none, 1 beep (default), 2 a screen flash */
    ULONG cursor_rgb;            /* VR_KEEP: the inverted cell (default) */
    int cursor_style;            /* DECSCUSR default, 0-6 */
    int cursor_blink;            /* ?12 default */
    int meta_alt;                /* Alt (not Left Amiga) is the ESC prefix */
    int copy_on_select;          /* a drag ends with the text on the clipboard */
    int wheel_scroll;            /* the wheel moves through the scrollback */
    ULONG pal16[16];             /* profile palette: 0x01RRGGBB, 0 = the xterm's */
    /* owner switches, 0 for XCON: (a console.device unit sets them) */
    int nodraw_resize;           /* CONFLAG_NODRAW_ON_NEWSIZE: a resize clears, nothing redrawn */
    int no_clipboard;            /* no RAmiga-C/V copy and paste (only SNIPMAP units have them) */
    int foreign_window;          /* not ours: never change its flags (ReportMouse) */
    /* live */
    struct Window *win;
    struct TextFont *font;
    int font_opened;
    struct TextFont *alt[11];
    vt_term *t;
    vr_render r;
    WORD want_cols;              /* DECCOLM asked for this width (0: none) */
    int layout_dirty;            /* a CSI t/u/x/y changed the text area */
    int render_pending;          /* the grid is ahead of the screen */
    struct MsgPort *frame_port;  /* the frame clock (timer.device) */
    struct timerequest *frame;
    int frame_open, frame_busy;
    int dragging, drag_moved;    /* mouse selection */
    int drag_ax, drag_ay;
    int drag_x, drag_y;          /* the cell the selection ends at now */
    char find_q[VT_FIND_QUERY_MAX]; /* the last find query, for "find next" */
    long find_next;              /* the row to continue from (VT_ROW_NONE: from the oldest) */
    const vtwin_host *host;
    void *user;
} vtwin;

void vtwin_init(vtwin *w, const vtwin_host *host, void *user);
void vtwin_cleanup(vtwin *w);
/* the frame clock's signal (0 without one) */
ULONG vtwin_sigmask(const vtwin *w);
/* call after a Wait() that may have been the frame clock */
void vtwin_tick(vtwin *w);

struct TextFont *vtwin_open_font(vtwin *w);
int vtwin_attach(vtwin *w, struct Window *win); /* 0: no memory (the owner closes its window) */
void vtwin_detach(vtwin *w);

/* output: the grid now, the screen at the next frame */
void vtwin_write(vtwin *w, const vt_u8 *b, long n);
/* draw what the grid has that the screen has not */
void vtwin_render(vtwin *w);
/* the window's size changed: new grid size, redraw, raw report */
void vtwin_resize(vtwin *w);
/* damage to repair (IDCMP_REFRESHWINDOW) */
void vtwin_refresh(vtwin *w);
/* a raw-key event: code and qualifier as Intuition gives them, prev the
 * previous two down keys (dead keys; 0 when unknown), the event's time */
void vtwin_key(vtwin *w, UWORD code, UWORD qual, ULONG prev, ULONG secs, ULONG micros);
/* a mouse event: move (1) or button (code SELECTDOWN/UP, MENUDOWN/UP) at
 * window coordinates mx, my */
void vtwin_mouse(vtwin *w, int move, UWORD code, UWORD qual, WORD mx, WORD my);
/* the wheel: up (1) / down (-1). The program's when it asked for the mouse,
 * otherwise the scrollback by a few lines (the spec's wheel_scroll) */
void vtwin_wheel(vtwin *w, int up, WORD mx, WORD my);
/* Find (Right Amiga F, or the console's menu): the scrollback and the grid,
 * oldest line first, case-insensitively, and scroll the view so the line the
 * match is on shows. q NULL or empty repeats the last query. 1 when the view
 * moved to a hit, 0 when there was none (the view is left alone). The view
 * returns to the live output when the match is already on screen. */
int vtwin_find(vtwin *w, const char *q);
/* an Amiga input event report for a window class, when the program asked
 * for that class (CSI n {); 1 when it was sent */
int vtwin_raw_report(vtwin *w, int cls);

#endif
