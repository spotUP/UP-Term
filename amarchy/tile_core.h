/* The pure part of AmTile: which windows tile, their order, and the layout boxes (amarchy/tile_core.c). No OS
 * headers, no Amiga types: the commodity copies each window's flags and limits into a tile_win, calls these, and
 * hands the boxes to Intuition. The clamp is win_core's win_fit. Host-tested by tests/test_tile.c. */
#ifndef AMARCHY_TILE_CORE_H
#define AMARCHY_TILE_CORE_H

#include "win_core.h"

#define TILE_GAP_DEFAULT    4
#define TILE_RATIO_DEFAULT  55    /* master width in percent of the work area */
#define TILE_RATIO_MIN      20
#define TILE_RATIO_MAX      80

#define TW_SIZEGADGET  1          /* WFLG_SIZEGADGET */
#define TW_BACKDROP    2          /* WFLG_BACKDROP */
#define TW_BORDERLESS  4          /* WFLG_BORDERLESS */

typedef struct tile_win {
    long id;                 /* opaque id of the window (its pointer) */
    int flags;               /* TW_* */
    const char *title;       /* the window's title, may be NULL */
    win_box box;             /* current box with Min / Max limits; the layout ignores left/top/width/height */
} tile_win;

typedef enum { TILE_FLOAT = 0, TILE_TILE = 1 } tile_kind;

/* TILE_TILE when the window is a resizable, bordered, non-backdrop window that is not the Amarchy menu's and whose
 * limits leave room to resize. A system requester has no size gadget, so it floats by the first rule. */
tile_kind tile_classify(const tile_win *w);

/* order[0..n) holds the tiled windows' ids, front (master) first; set[0..m) the ids that are tiled now. Closed ids are
 * removed, the others keep their place, ids not in order go to the front. Returns the new count (order holds >= m). */
int tile_order_update(long *order, int n, const long *set, int m);

/* The tracked set belongs to one screen. *cur is the id of the screen it was built on (0 = none yet); next is the
 * screen to tile now. When they differ, *cur becomes next and the order is dropped (returns 0 = the new count), so no
 * box worked out for screen A is applied on screen B. Same screen: returns n unchanged. */
int tile_screen_switch(long *cur, long next, int n);

#define TILE_NMASTER_MAX    8
#define TILE_MAX            32                          /* windows considered on the screen; capacity of a tile_floats */

typedef enum { TL_DWINDLE = 0, TL_MASTER, TL_MIRROR, TL_COLUMNS, TL_ROWS, TL_GRID, TL_MONOCLE, TL_COUNT } tile_layout_kind;

const char *tile_layout_name(int kind);                 /* "dwindle", "master-stack", "mirrored", "columns", "rows", "grid", "monocle"; NULL out of range */
int tile_layout_parse(const char *word, int *kind);     /* case-insensitive exact word, 1 ok / 0 unknown (word may end at ' ' or NUL) */
int tile_layout_step(int kind, int dir);                /* next (dir>0) / previous (dir<0), wrapping over TL_COUNT */

/* n windows in[0..n) (front = newest) to out[0..n) in the given layout. The work area is the screen below its title
 * bar; gap sits on the outer edges and between cells; remainder pixels go to the last cell of a run. Each box is held
 * to its window's Min / Max by win_fit. Knobs: ratio_pct moves dwindle, master-stack and mirrored; nmaster (clamped to
 * 1..n) moves master-stack and mirrored only; columns, rows, grid and monocle ignore both (written exemption). An
 * unknown kind falls back to TL_DWINDLE. Rules and worked numbers: thoughts/shared/plans/2026-10-28-amtile-layout.md. */
void tile_layout(int kind, const win_screen *s, int gap, int ratio_pct, int nmaster, int n, const tile_win *in, win_box *out);

/* Moves id to order[0], the rest keep their order. 1 when id was found (changed or already first), 0 when not tiled. */
int tile_promote(long *order, int n, long id);

/* The index in order[0..n) of the window after (dir>0) or before (dir<0) id, wrapping. id not in order: 0. n < 1: 0. */
int tile_cycle(const long *order, int n, long id, int dir);

/* The state file AmTile writes (ENV:Amarchy/tile, "on" or "off" and a newline): 1 when buf[0..len) reads "on". Amarchy
 * runs AmTile ON at its start by this. */
int tile_state_on(const char *buf, long len);

/* Swaps the places of ids a and b in order[0..n). 1 when done, 0 when either is not in order (nothing changes). */
int tile_swap(long *order, int n, long a, long b);

/* The windows the owner floated by hand: kept out of the tiled set until they are cleared. */
typedef struct { long id[TILE_MAX]; int n; } tile_floats;
int  tile_float_has(const tile_floats *f, long id);
int  tile_float_set(tile_floats *f, long id);           /* 1 added, 0 already in or full */
int  tile_float_clear(tile_floats *f, long id);         /* 1 removed, 0 not in */
void tile_float_prune(tile_floats *f, const long *live, int m);   /* drops the ids not in live[0..m): closed windows */

/* ---- P3-C2 FLOAT, C3 SWAP, C4 opt-out list ------------------------------------------------------------------- */

typedef enum { TF_OFF = 0, TF_ON, TF_TOGGLE } tile_float_mode;

/* The argument of "FLOAT ON|OFF|TOGGLE [<hex id>]": cmd is the whole command, its first word is skipped. *mode is set;
 * *id is the hex window id (at most 8 digits, an optional "0x" or "$" first) or 0 when none was given. 1 ok, 0 bad. */
int tile_float_parse(const char *cmd, int *mode, long *id);

/* SWAP: boxes[0..n) are the current boxes of the windows in order[0..n) (same index). The neighbour of active in dir
 * (WA_LEFT, WA_RIGHT, WA_UP, WA_DOWN, by win_pick) takes its place in order and it takes the neighbour's. 1 when
 * swapped, 0 when active is not tiled or there is no neighbour (nothing changes). */
int tile_swap_dir(long *order, int n, const win_box *boxes, long active, win_arg dir);

/* The opt-out list (ENV:Amarchy/tile.conf): one AmigaDOS pattern per line, matched on the window title. */
#define TILE_CONF_LINES  64
#define TILE_CONF_LEN    64                              /* a longer line is cut here */
typedef struct { char pat[TILE_CONF_LINES][TILE_CONF_LEN]; int n; } tile_conf;

/* Splits buf[0..len) into c: lines end at LF or CR; trailing blanks go, empty lines and lines starting with ';' are
 * dropped, lines past the 64th are ignored. Returns the pattern count. */
int tile_conf_split(const char *buf, long len, tile_conf *c);

/* match(pattern, title) is case-insensitive AmigaDOS matching (AmTile passes dos.library's, the host test its own). */
typedef int (*tile_match_fn)(const char *pattern, const char *title);

/* 1 when title matches any pattern of c. NULL title never matches. */
int tile_optout(const tile_conf *c, const char *title, tile_match_fn match);

/* The ids of in[0..n) that tile now: tile_classify says TILE_TILE, the id is not in f (floated by hand) and the title
 * matches no opt-out pattern. Written to set[0..); returns the count. f, c and match may be NULL. */
int tile_set(const tile_win *in, int n, const tile_floats *f, const tile_conf *c, tile_match_fn match, long *set);

#endif
