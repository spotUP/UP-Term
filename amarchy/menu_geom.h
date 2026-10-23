/* Amarchy drawn menu, the geometry (plan 2026-10-23-amarchy-drawn-menu.md, G1). No OS calls, so
 * tests/test_keymap.c covers it; amarchy-gui draws into the rectangles it gives.
 *
 * Every size scales from the font the way Omarchy's menu scales from its own (research
 * 2026-10-23_amarchy-vs-omarchy-geometry.md): a length of px in Omarchy's 12 px tall, 7.2 px wide
 * font becomes px * ysize / 12 vertically and px * cw / 7.2 horizontally, at least 1. */
#ifndef AMARCHY_MENU_GEOM_H
#define AMARCHY_MENU_GEOM_H

#include "menu_core.h"

#define MG_MAX_ROWS 8           /* rows shown at once, however tall the screen */

typedef struct mg_font {
    int ysize;                  /* the font's line height in pixels (tf_YSize) */
    int cw;                     /* its character width (tf_XSize; topaz 8) */
} mg_font;

/* The pixel width of a text in the window's font (TextLength on the Amiga). */
typedef int (*mg_text_fn)(const char *s, void *ctx);

/* The scaled element sizes, all pixels. */
typedef struct mg_metrics {
    int bx, by;                 /* border: left/right, top/bottom */
    int hpad, vpad;             /* inside the border */
    int head_h;                 /* the header row (the node's name) */
    int gap;                    /* between the header and the first row */
    int row_h, row_gap, pitch;  /* one row, the space below it, their sum */
    int row_pad;                /* text inset inside a row's box, each side */
    int icon_gap, chev_gap;     /* icon to label, label to the group chevron */
    int min_w;                  /* narrowest window */
    int margin_x, margin_y;     /* kept free on each side of the screen */
} mg_metrics;

void mg_metrics_for(mg_metrics *m, const mg_font *f);

/* The window's width for a menu opened at node: the widest row (icon column when icon_w > 0, label,
 * chevron on groups) or header over every level reachable from node (mn_levels), so the window
 * keeps its width while the user walks the levels; at least min_w, at most the screen minus its
 * margins. */
int mg_width(const keymap *k, const char *node, const mg_font *f, int icon_w, int chev_w,
             mg_text_fn text, void *ctx, int scr_w);

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
void mg_layout(mg_geom *g, const mg_font *f, int w, int n, int scr_w, int scr_h);

/* The first row shown so the selection stays in view, from the previous first row. */
int mg_scroll(const mg_geom *g, int sel, int top);
/* 1 when rows below the shown ones exist, so the peek row is drawn. */
int mg_more(const mg_geom *g, int top);
/* Window-relative top of the i-th shown row (0 is the row at top). */
int mg_row_y(const mg_geom *g, int i);
/* The entry under a window-relative point, the peek row included; -1 off the rows. */
int mg_hit(const mg_geom *g, int top, int mx, int my);

#endif
