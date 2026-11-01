/* Amarchy drawn menu, the geometry (plan 2026-10-23-amarchy-drawn-menu.md, G1). No OS calls, so
 * tests/test_keymap.c covers it; amarchy-gui draws into the rectangles it gives.
 *
 * Every size comes from the font, not the screen: a row is ysize * 15 / 4 high (topaz 8: 30, TopazPro 16:
 * 60, so the menu is as big on 640x256 as on 640x512) and Omarchy's px (its rows are 50 px) become
 * px * row / 50 down and mg_cols() of that across. A length looks the same across and down on a screen
 * whose pixels are not square (640x256 hires: twice as tall as wide); plan
 * 2026-10-23-amarchy-menu-aspect.md. */
#ifndef AMARCHY_MENU_GEOM_H
#define AMARCHY_MENU_GEOM_H

#include "menu_core.h"

#define MG_MAX_ROWS MN_OSD_MAX_ENTRIES /* rows shown at once, however tall the screen: the text menu's cap */

typedef struct mg_font {
    int ysize;                  /* the font's line height in pixels (tf_YSize) */
    int cw;                     /* its character width (tf_XSize; topaz 8) */
} mg_font;

typedef struct mg_screen {
    int w, h;                   /* the screen the menu opens on, pixels */
    int rx, ry;                 /* DisplayInfo Resolution ticks of one pixel; 0, 0 = square */
    int minimal;                /* 1 = the minimal menu (menu-style = minimal): no border, no icon or chevron gap */
    int anchored;               /* 1 = the menu hangs from (ax, ay): its right edge at ax, its bottom edge at ay, so it
                                 * opens upward from a bottom bar (the Amarchy bar's agents panel); 0 = centred */
    int ax, ay;
} mg_screen;

/* The columns that look as long as `lines` lines (lines when the aspect is unknown). */
int mg_cols(const mg_screen *s, int lines);

/* The pixel width of a text in the window's font (TextLength on the Amiga). */
typedef int (*mg_text_fn)(const char *s, void *ctx);

/* The scaled element sizes, all pixels. */
typedef struct mg_metrics {
    int bx, by;                 /* border: left/right, top/bottom */
    int hpad, vpad;             /* inside the border */
    int head_h;                 /* the header row (the node's name) */
    int gap;                    /* between the header and the first row */
    int unit;                   /* Omarchy's 50 px row in this font: the scale of every size here */
    int row_h, row_gap, pitch;  /* one row, the space below it, their sum */
    int row_pad;                /* text inset inside a row's box, each side */
    int icon_gap, chev_gap;     /* icon to label, label to the group chevron */
    int min_w;                  /* narrowest window */
    int margin_x, margin_y;     /* kept free on each side of the screen */
} mg_metrics;

void mg_metrics_for(mg_metrics *m, const mg_font *f, const mg_screen *s);

/* The window's width for a menu opened at node: the widest row (icon column when icon_w > 0, label,
 * chevron on groups) or header over every level reachable from node (mn_levels), so the window
 * keeps its width while the user walks the levels; at least min_w, at most the screen minus its
 * margins. */
int mg_width(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, const mg_screen *s);
/* mg_width over the rows shown with (term) or without an UP-Term window under the menu */
int mg_width_t(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, const mg_screen *s, int cond);

/* The header a level shows: "Amarchy" at the root, else the node's last segment with a capital. */
void mg_header(const char *node, char *out, int n);

/* One level laid out, window-relative except x, y (the window's place on the screen). */
typedef struct mg_geom {
    mg_metrics m;
    int x, y, w, h;             /* the window, centred on the screen */
    int head_x, head_y, head_w; /* the header row (height m.head_h) */
    int list_x, list_y, list_w, list_h;   /* the rows, the peek included */
    int n;                      /* entries of the level */
    int visible;                /* rows drawn whole */
    int peek;                   /* height of the cut-off row below them when n > visible, else 0 */
} mg_geom;

/* Height and place of a level of n entries at width w (mg_width): up to MG_MAX_ROWS rows or what
 * fits in 70 % of the screen; when the level has more, room for a peek of the next row (the
 * scroll cue). */
void mg_layout(mg_geom *g, const mg_font *f, int w, int n, const mg_screen *s);

/* The first row shown so the selection stays in view, from the previous first row. */
int mg_scroll(const mg_geom *g, int sel, int top);
/* 1 when rows below the shown ones exist, so the peek row is drawn. */
int mg_more(const mg_geom *g, int top);
/* Window-relative top of the i-th shown row (0 is the row at top). */
int mg_row_y(const mg_geom *g, int i);
/* The entry under a window-relative point, the peek row included; -1 off the rows. */
int mg_hit(const mg_geom *g, int top, int mx, int my);

/* 1 when a blit of the h display lines from top may start with the beam on line beam: `blit` lines of beam time
 * for the box (0: not measured, planned as half a frame), a frame is `frame` lines. The beam must not show a box
 * line while the copy runs, or that frame shows part new, part old (the white lines of 2026-10-25). A planar
 * copy is one blit per plane, the whole box each time, so while it runs every line of the box has some planes
 * new and some old, and the beam crossing it shows pens neither picture has (white): the copy must be done
 * before the beam reaches the box's top, and the beam must be off the box when it starts, however fast it is.
 * A copy no start can keep clear starts at once. */
int mg_blit_safe(int beam, int top, int h, int frame, int blit);

/* The blit rate the next start is planned with, from the last estimate and the rate a copy just took (both
 * beam lines per 65536 pixels). The same copy takes longer while the display fetches its planes than in the
 * vertical blank, so the last rate alone often promised a copy faster than it was and the beam crossed it
 * (the A1200: 13 of 30 rows late, white lines in the moving bar): a slower copy raises the estimate at once,
 * a faster one lowers it by a sixteenth of the difference. */
long mg_blit_rate(long est, long took);

/* The i-th strip of a copy of the window lines y0..y1: a box taller than a row is copied a row at a time, each
 * strip started where the beam cannot cross it (mg_blit_safe), as the whole box has no such start (the full
 * window at the menu's open took 308 beam lines of 313, and every open showed a torn line: the grab of
 * 2026-10-25 19:37). 1 with the strip in *s0, *s1; 0 past the last. */
int mg_blit_strip(int y0, int y1, int row_h, int i, int *s0, int *s1);

#endif
