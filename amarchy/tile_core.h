/* The pure part of AmTile: which windows tile, their order, and the master-stack boxes (amarchy/tile_core.c). No OS
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

/* Master-stack: n windows in[0..n) (front = master) to out[0..n). The work area is the screen below its title bar;
 * gap sits on the outer edges and between cells. One window fills the area; else the master takes ratio_pct of it on the
 * left and the rest are split evenly in the right column, the last cell taking the remainder pixels. Each box is held
 * to its window's Min / Max by win_fit. */
void tile_master_stack(const win_screen *s, int gap, int ratio_pct, int n, const tile_win *in, win_box *out);

/* The state file AmTile writes (ENV:Amarchy/tile, "on" or "off" and a newline): 1 when buf[0..len) reads "on". Amarchy
 * runs AmTile ON at its start by this. */
int tile_state_on(const char *buf, long len);

#endif
