/* Amarchy quick menu and key viewer, the portable part (plan
 * 2026-10-22-amarchy-commodity.md, Phase D). No OS calls, so tests/test_keymap.c covers it.
 * amarchy/menu.c (the amarchy-menu helper) draws it with libupkgui; the rows come from the
 * one keymap (config/keymap.c): the menu reads [menu], the viewer reads [bind]. */
#ifndef AMARCHY_MENU_CORE_H
#define AMARCHY_MENU_CORE_H

#include "../config/keymap.h"

#define MN_MAX_ITEMS 64

typedef struct mn_item {
    int  row;                   /* index into keymap.menu, -1 for a group (a node with children) */
    char seg[KM_ID];            /* the id segment: the node path of a group is node + "." + seg */
    char label[KM_LABEL];       /* a leaf's own label; a group's segment with a capital */
} mn_item;

/* Filter match of one label: 2 when the label starts with the filter, 1 when it contains it,
 * 0 when not; case is ignored; an empty filter matches everything (2). */
int mn_match(const char *label, const char *filter);

/* The entries one level below `node` ("" is the root, "apps" the Apps node), in file order: a
 * [menu] id one segment deeper is a leaf, a longer one makes a group (once). The count; at
 * most max. A row whose id equals the node itself is never its own child. */
int mn_children(const keymap *k, const char *node, mn_item *out, int max);

/* The entries that match filter, prefix matches first, each group in its order. Writes the
 * indices into items to idx; the count. */
int mn_filter(const mn_item *items, int n, const char *filter, int *idx, int max);

/* The node one level up ("apps.tools" -> "apps", "apps" -> ""). 0 at the root: Esc there leaves
 * the menu; otherwise 1 and out holds the parent. */
int mn_parent(const char *node, char *out, int n);
/* The node an item opens (groups only): node + "." + seg. */
void mn_child_node(const char *node, const mn_item *it, char *out, int n);

/* Every level a menu opened at `node` can show: node itself, then each group below it, each passed
 * to fn as its entries (mn_children) and their count. The window sizes (mn_osd_size here, the drawn
 * menu's amarchy/menu_geom.c) take their maxima over this one walk. */
typedef void (*mn_level_fn)(const mn_item *items, int n, void *ctx);
void mn_levels(const keymap *k, const char *node, mn_level_fn fn, void *ctx);

/* The menu window's size in cells, so the OSD fits its content (plan 2026-10-23-amarchy-osd.md,
 * O5). The head lines and the side padding equal the library's UI_OSD_HEAD_LINES and UI_OSD_PAD
 * (amarchy/menu.c checks that at compile time; the commodity cannot include upkgui/ui.h). */
#define MN_OSD_HEAD_LINES 2     /* the filter line and the gap above the first entry */
#define MN_OSD_PAD        2     /* columns of padding on each side of the text */
#define MN_OSD_MIN_COLS   24    /* room for the filter line however short the labels are */
/* Rows and columns for `node` and every level reachable from it (the menu is walked with
 * mn_children, as amarchy-menu walks it): rows = head lines + the most entries any of those
 * levels has, cols = the widest "label  item" (a group's item is ">") + 2 * pad, at least
 * MN_OSD_MIN_COLS. Both come back through the pointers. */
void mn_osd_size(const keymap *k, const char *node, int *cols, int *rows);

/* One viewer line for a [bind] row: "Super+Shift+Space  Apps  (menu)". The spec column is padded
 * to MN_SPEC_COL. 1 when it fit. */
#define MN_SPEC_COL 22
int mn_spec(const km_bind *b, char *out, int n);
int mn_viewer_line(const km_bind *b, char *out, int n);
/* The whole viewer text, one line per [bind] row in table order, each ending in a newline.
 * The line count; the text is cut at a line when buf is full. */
int mn_viewer_text(const keymap *k, char *buf, long cap);

/* The TERM the helper must use: "vtcon" when the environment has none (TERM is unset system-wide
 * on an Amiga, and ncurses' newterm fails without it), else NULL (keep what is set). The helper
 * only ever runs in an XCON: window, which is the vtcon handler, so the default is that terminal's
 * own name; vsh sets it the same way. */
const char *mn_term_default(const char *term);

#endif
