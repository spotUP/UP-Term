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
#include "vtinput.h"

/* With a key to the owner: the physical Alt key was down (VT_MOD_ALT is
 * Meta, which is Left Amiga unless meta_alt). KingCON's Alt+Tab. */
#define VTWIN_MOD_ALTKEY 8

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
    /* the title changed (OSC 0/2): a tab's label (may be 0) */
    void (*titled)(void *user);
    /* Ctrl + click on an OSC 8 hyperlink: open uri (may be 0: no links) */
    void (*open_link)(void *user, const char *uri);
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
    struct RastPort *own_rp;     /* draw through this RastPort, not the window's: a tab sharing
                                  * its window with other processes (own pens and font) */
    WORD inset_top;              /* pixels above the text kept free (the tab bar) */
    /* profile (config/upconf): the window's defaults the program's
     * sequences still override; 0 / VR_KEEP keep the historical look */
    int bold_bright;             /* xterm SGR 1 takes the bright 8-15 (1, default) */
    int bell;                    /* 0 none, 1 beep (default), 2 a screen flash */
    ULONG cursor_rgb;            /* VR_KEEP: the inverted cell (default) */
    ULONG sel_fg_rgb, sel_bg_rgb;/* VR_KEEP: the selection swaps fg and bg (default) */
    int cursor_style;            /* DECSCUSR default, 0-6 */
    int cursor_blink;            /* ?12 default */
    int meta_alt;                /* Alt (not Left Amiga) is the ESC prefix */
    int copy_on_select;          /* a drag ends with the text on the clipboard */
    int wheel_scroll;            /* the wheel moves through the scrollback */
    int reflow;                  /* a resize re-wraps lines and scrollback (1, default) */
    int clip_access;             /* VT_CLIP_*: what OSC 52 may do (program-clipboard; write) */
    ULONG pal16[16];             /* profile palette: 0x01RRGGBB, 0 = the xterm's */
    int aspect_off;              /* font-aspect = off: the font as asked, on any screen */
    int aspect_known, square, square_fits; /* vtwin_set_screen: the screen's pixels */
    char fallback[64];           /* outline font for glyphs the bitmap font lacks
                                  * (font-fallback; FONTS:<name>.otag), "" none */
    /* owner switches, 0 for XCON: (a console.device unit sets them) */
    int nodraw_resize;           /* CONFLAG_NODRAW_ON_NEWSIZE: a resize clears, nothing redrawn */
    int no_clipboard;            /* no RAmiga-C/V copy and paste (only SNIPMAP units have them) */
    int foreign_window;          /* not ours: never change its flags (ReportMouse) */
    int title_on_screen;         /* the title goes to the screen's title bar (FULLSCREEN) */
    /* live */
    struct Window *win;
    struct TextFont *font;
    int font_opened;
    struct TextFont *alt[11];
    struct vo_font *outline;     /* fallback, open while attached (0: none or not found) */
    vt_term *t;
    vr_render r;
    WORD want_cols;              /* DECCOLM asked for this width (0: none) */
    int layout_dirty;            /* a CSI t/u/x/y changed the text area */
    int render_pending;          /* the grid is ahead of the screen */
    struct MsgPort *frame_port;  /* the frame clock (timer.device) */
    struct timerequest *frame;
    int frame_open, frame_busy;
    ULONG frame_us;              /* the frame clock's next interval (pace.h); 0: the shortest */
    ULONG frame_wait;            /* the interval of the request in flight (us) */
    ULONG prof_render, prof_frames; /* EClock ticks drawing, and render passes (the handler's PROF=1) */
    ULONG prof_part[3];          /* PROF=1: of the drawing, damaged rows / scrolls / cursor and mask */
    long sync_held;              /* microseconds a ?2026 update has been held back */
    long note_us;                /* OSC 9 / 777: microseconds the notice stays in the title */
    char note_saved[80];         /* the title it stands in for */
    int dragging, drag_moved;    /* mouse selection */
    int drag_ax, drag_ay;
    int drag_x, drag_y;          /* the cell the selection ends at now */
    vti_mouse mouse;             /* buttons and moves the program was told about (vtinput) */
    int mouse_mods;              /* VT_MOD_* of the last mouse event (motion polled on the clock) */
    ULONG click_secs, click_micros; /* the last left press, for DoubleClick() */
    int sel_whole;               /* the selection is a double-clicked word or triple-clicked line */
    int pointer_on;              /* ReportMouse is on: a drag, or the program wants moves */
    char find_q[VT_FIND_QUERY_MAX]; /* the last find query, for "find next" */
    long find_next;              /* the row to continue from (VT_ROW_NONE: from the oldest) */
    const vtwin_host *host;
    void *user;
} vtwin;

void vtwin_init(vtwin *w, const vtwin_host *host, void *user);
/* the profile fields to the historical look (vtwin_init does it; an owner
 * that parses a new spec calls it again before applying a profile) */
void vtwin_profile_defaults(vtwin *w);
/* The spec's settings (colours, palette, cursor, bold-bright, selection
 * colours) applied to an attached window and drawn: vtwin_attach does it,
 * and an owner whose menus changed one calls it (every field is set, so a
 * setting turned back off takes effect too). */
void vtwin_apply_settings(vtwin *w);
/* Another font for an attached window, live: name ("topaz" or
 * "topaz.font") and size; an empty name is the system's default font. The
 * grid takes the window's new columns and rows, the text and scrollback
 * stay. 0 when the font cannot be opened or is proportional (nothing
 * changes). */
int vtwin_set_font(vtwin *w, const char *name, WORD size);
/* A new scrollback size for an attached window, live (0: none); the view
 * returns to the live output. 0 when there was no memory (unchanged). */
int vtwin_set_scrollback(vtwin *w, int lines);
/* A tab: shown (redrawn whole, its title on the window) or not (draws
 * nothing, its engine goes on). */
void vtwin_show(vtwin *w, int on);
/* The tab bar's height changed: the text area moves, the grid follows. */
void vtwin_set_inset(vtwin *w, WORD top);
void vtwin_cleanup(vtwin *w);
/* the title (w->title) to the window's title bar, or the screen's */
void vtwin_show_title(vtwin *w);
/* the frame clock's signal (0 without one) */
ULONG vtwin_sigmask(const vtwin *w);
/* call after a Wait() that may have been the frame clock */
void vtwin_tick(vtwin *w);

struct TextFont *vtwin_open_font(vtwin *w);
/* The screen the window is on, before vtwin_open_font: square pixels or
 * tall ones decide between the two fonts of a pair (render/fontpair). */
void vtwin_set_screen(vtwin *w, struct Screen *scr);
/* After a move to another screen: the pair's other font when this screen
 * wants it (1 when the font changed). */
int vtwin_fit_aspect(vtwin *w);
int vtwin_attach(vtwin *w, struct Window *win); /* 0: no memory (the owner closes its window) */
void vtwin_detach(vtwin *w);
/* The window goes but the terminal stays (moving to another screen):
 * unbind before closing the old window, rebind to the new one -- the
 * text, scrollback, modes and fonts go along; the grid takes the new
 * window's size. 0 from rebind when there is nothing to bind. */
void vtwin_unbind(vtwin *w);
int vtwin_rebind(vtwin *w, struct Window *win);

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
/* a mouse event: move (1) or button (code SELECTDOWN/UP, MIDDLEDOWN/UP,
 * MENUDOWN/UP) at window coordinates mx, my, at the event's time (a run of
 * clicks selects a word, then a line) */
void vtwin_mouse(vtwin *w, int move, UWORD code, UWORD qual, WORD mx, WORD my, ULONG secs, ULONG micros);
/* the wheel: up (1) / down (0), qual the event's qualifier (Ctrl and Meta
 * go into the report). The program's when it asked for the mouse,
 * otherwise the scrollback by a few lines (the spec's wheel_scroll) */
void vtwin_wheel(vtwin *w, int up, UWORD qual, WORD mx, WORD my);
/* The window became active (in 1) or stopped being: the program's focus
 * report when it asked for one (?1004) */
void vtwin_focus(vtwin *w, int in);
/* Find (Right Amiga F, or the console's menu): the scrollback and the grid,
 * oldest line first, case-insensitively, and scroll the view so the line the
 * match is on shows. q NULL or empty repeats the last query. 1 when the view
 * moved to a hit, 0 when there was none (the view is left alone). The view
 * returns to the live output when the match is already on screen. */
int vtwin_find(vtwin *w, const char *q);
/* Edit > Select all: the scrollback and the screen selected. */
void vtwin_select_all(vtwin *w);
/* Edit > Clear scrollback: the lines above the screen gone. */
void vtwin_clear_scrollback(vtwin *w);
/* Edit > Reset terminal: RIS, then the window's own settings again. */
void vtwin_reset(vtwin *w);
/* View > Bigger / Smaller font: the next designed size of the font (dir 1 /
 * -1); 0 when there is none (nothing changes). */
int vtwin_font_step(vtwin *w, int dir);
/* View > 80 x 24 ...: the window sized to cols x rows cells (the grid
 * follows on the resize); 0 when the screen is too small. */
int vtwin_set_size(vtwin *w, int cols, int rows);
/* copy the selection to the clipboard / type the clipboard in: Right Amiga
 * C and V, also for an owner's menu (no-ops without a clipboard) */
void vtwin_copy(vtwin *w);
void vtwin_paste(vtwin *w);
/* an Amiga input event report for a window class, when the program asked
 * for that class (CSI n {); 1 when it was sent */
int vtwin_raw_report(vtwin *w, int cls);

#endif
