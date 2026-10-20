/* layout: the split panes of one tab as a small binary tree -- the
 * arithmetic only, no OS calls and no allocation (a fixed array), so it is
 * host-tested (tests/test_layout.c). The window owner (vtwin / the host)
 * feeds it the window's pixel area and reads back one pixel rect per pane
 * and one per divider. One layout per tab; at most LAY_MAX_PANES panes.
 *
 * Pane sizes are whole cells; the divider is either one cell wide
 * (LAY_DIV_CELL) or one pixel line (LAY_DIV_PIXEL). With a pixel divider
 * the second child of a split absorbs the pixels the first one cannot use,
 * so the pane rects and the divider rects tile the area exactly. A split
 * keeps its position as a ratio in 1/65536ths of the room it divides, so a
 * resize keeps the proportions; the minimum pane size clamps the solved
 * sizes but never rewrites the ratio.
 *
 * Pane ids are 0..LAY_MAX_PANES-1 and stay with their pane; the first pane
 * is 0. Divider ids are opaque (valid until the next split or close).
 *
 * Portable C89, as the engine. */
#ifndef LAYOUT_H
#define LAYOUT_H

#define LAY_MAX_PANES 4
#define LAY_MAX_NODES (2 * LAY_MAX_PANES - 1)

#define LAY_DIV_CELL  0 /* a divider takes one cell (cell_w or cell_h) */
#define LAY_DIV_PIXEL 1 /* a divider takes one pixel line */

#define LAY_RIGHT 0 /* split: new pane to the right; neighbour: to the right */
#define LAY_DOWN  1
#define LAY_LEFT  2 /* neighbour only */
#define LAY_UP    3

typedef struct lay_rect {
    int x, y, w, h;
} lay_rect;

typedef struct lay_node {
    signed char used;
    signed char pane;      /* leaf: its pane id; split: -1 */
    signed char dir;       /* split: LAY_RIGHT (a left of b) or LAY_DOWN (a above b) */
    signed char a, b;      /* split: child node indexes */
    long ratio;            /* split: a's share of the room, 1/65536ths */
    lay_rect r;            /* the node's solved area */
    lay_rect div;          /* split: the solved divider */
} lay_node;

typedef struct lay {
    lay_node n[LAY_MAX_NODES];
    int root;
    int npanes;
    int cw, ch;            /* the cell, pixels */
    int div_mode;          /* LAY_DIV_CELL / LAY_DIV_PIXEL */
    int min_cols, min_rows;
    lay_rect area;
} lay;

/* One pane (id 0) filling the area. min_cols/min_rows: the smallest pane. */
void lay_init(lay *l, int cell_w, int cell_h, int div_mode,
              int min_cols, int min_rows,
              int x, int y, int w, int h);
/* Solve for a new window area (also what the other calls do for the
 * stored area). */
void lay_solve(lay *l, int x, int y, int w, int h);
/* A new cell (a new font): the splits keep their ratios, the stored area
 * is solved again, the panes snapping to the new cell. */
void lay_set_cell(lay *l, int cell_w, int cell_h);
/* The solved rect of a pane / divider; 0 when there is no such pane or divider. */
int lay_pane_rect(const lay *l, int pane, lay_rect *r);
int lay_divider_rect(const lay *l, int divider, lay_rect *r);
int lay_pane_count(const lay *l);
/* The divider ids, in index order: the n-th divider, or -1 past the last. */
int lay_divider_at(const lay *l, int nth);

/* Split a pane in half, the new pane to the right (LAY_RIGHT) or below
 * (LAY_DOWN). Returns the new pane's id, or -1 when refused: the maximum
 * panes, no such pane, or a pane would fall below the minimum size. */
int lay_split(lay *l, int pane, int dir);
/* Close a pane; its sibling takes the space. Returns the id of a pane now
 * in that space (the one to focus), or -1 for the last pane / no such pane. */
int lay_close(lay *l, int pane);
/* The pane next to this one in a direction (LAY_RIGHT/DOWN/LEFT/UP): the
 * nearest whose edge is beyond this pane's and overlaps its span, the one
 * closest to its centre on a tie. -1 when there is none. */
int lay_neighbour(const lay *l, int pane, int dir);
/* The divider under the pixel x,y, grabbed within slop pixels either
 * side; -1 for none. */
int lay_hit_divider(const lay *l, int x, int y, int slop);
/* Move a divider so it starts at pixel position px (along its axis),
 * snapped to cells and held to the minimum pane sizes. Returns 1 when the
 * layout changed. */
int lay_drag(lay *l, int divider, int px);

/* The tree as text, to keep a tab's split over a restart: a pane is "p",
 * a split "r" (side by side) or "d" (one above the other), its ratio and
 * its two children in brackets, as in "r32768(p,d40000(p,p))". The panes
 * are written in order, left to right and top to bottom; order[i] (when
 * order is not 0) gets the id of the i-th. Returns the text's length, or
 * -1 when it does not fit size bytes with its terminator. */
int lay_save(const lay *l, char *buf, int size, int *order);
/* Replace the tree with one lay_save wrote, the panes numbered 0..n-1 in
 * the order written, solved for the stored area; the cell, the divider
 * mode and the minimum stay. Returns the pane count, or -1 (l unchanged)
 * when the text is not such a tree: a bad letter or bracket, a ratio
 * outside 1..65535, more than LAY_MAX_PANES panes, text left over. */
int lay_load(lay *l, const char *s);
/* How to build the tree lay_save wrote by splitting, one pane at a time,
 * starting from one pane: step j splits pane from[j] in dir[j] (LAY_RIGHT /
 * LAY_DOWN), the new pane being pane j+1 (pane 0 is the one there at the
 * start). leaf[k] gets the written place (0..n-1, lay_save's order) of pane k.
 * Returns the pane count n (n-1 steps), or -1 when s is not such a tree.
 * The arrays are LAY_MAX_PANES long. */
int lay_steps(const char *s, int *from, int *dir, int *leaf);

/* A pane's scrollback in lines (NS4.3, owner 2026-10-20): lines (the
 * window's setting, 0 none) in a tab of one or two panes; past two panes
 * each pane holds at most LAY_SB_MANY. The one rule vtwin sizes by; a
 * macro, so up-console.device (vtwin without the layout) links it too. */
#define LAY_SB_MANY 200
#define lay_sb_lines(lines, panes) ((panes) > 2 && (lines) > LAY_SB_MANY ? LAY_SB_MANY : (lines))

#endif
